#include "../include/spikehat.h"
#include "serial.h"
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

/* --- 安全停止: グローバルHATレジストリ --- */
#define SPIKEHAT_MAX_INSTANCES 4
static spikehat_t          *g_hats[SPIKEHAT_MAX_INSTANCES];
static pthread_mutex_t      g_hats_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int g_stopping = 0;

static void stop_all_motors(void) {
    if (g_stopping) return;
    g_stopping = 1;
    for (int i = 0; i < SPIKEHAT_MAX_INSTANCES; i++) {
        spikehat_t *hat = g_hats[i];
        if (!hat) continue;
        for (int p = 0; p < SPIKEHAT_MAX_PORTS; p++)
            proto_sendf(hat->fd, "port %d; coast", p);
    }
    g_stopping = 0;
}

static void atexit_handler(void) {
    stop_all_motors();
}

/* 受信スレッド: 全ポートのデータ行をキャッシュに書き込む */
static void *reader_thread(void *arg) {
    spikehat_t *hat = (spikehat_t *)arg;
    char line[512];

    while (hat->running) {
        int n = serial_readline(hat->fd, line, sizeof(line), 200);
        if (n <= 0) continue;

        int port, mode_type, mode_idx, nvalues;
        float values[8];
        if (proto_parse(line, &port, &mode_type, &mode_idx, values, &nvalues)) {
            if (port >= 0 && port < SPIKEHAT_MAX_PORTS) {
                pthread_mutex_lock(&hat->lock);
                memcpy(hat->ports[port].values, values, (size_t)nvalues * sizeof(float));
                hat->ports[port].nvalues = nvalues;
                hat->ports[port].valid   = 1;
                pthread_mutex_unlock(&hat->lock);
            }
        }
    }
    return NULL;
}

spikehat_t *spikehat_open(const char *device) {
    int fd = serial_open(device);
    if (fd < 0) {
        fprintf(stderr, "spikehat: cannot open %s\n", device);
        return NULL;
    }

    /* ファームウェア確認 */
    write(fd, "version\r", 8);
    usleep(300000);
    char line[512];
    int found = 0;
    for (int i = 0; i < 15 && !found; i++) {
        serial_readline(fd, line, sizeof(line), 400);
        if (strncmp(line, "Firmware version:", 17) == 0) found = 1;
    }
    if (!found) {
        fprintf(stderr, "spikehat: Build HAT firmware not found. "
                "Run a buildhat Python script first to load firmware.\n");
        serial_close(fd);
        return NULL;
    }

    /* 全ポートをデセレクト・エコー無効 */
    write(fd, "port 0; select; port 1; select; "
              "port 2; select; port 3; select; echo 0\r", 71);

    /* "Done initialising ports" を待つ (最大5秒) */
    for (int i = 0; i < 25; i++) {
        serial_readline(fd, line, sizeof(line), 200);
        if (strncmp(line, "Done initialising ports", 23) == 0) break;
    }

    spikehat_t *hat = calloc(1, sizeof(spikehat_t));
    if (!hat) { serial_close(fd); return NULL; }
    hat->fd      = fd;
    hat->running = 1;
    pthread_mutex_init(&hat->lock, NULL);
    pthread_create(&hat->reader, NULL, reader_thread, hat);

    /* レジストリ登録 + 初回のみ atexit を設定 */
    pthread_mutex_lock(&g_hats_lock);
    static int handlers_registered = 0;
    if (!handlers_registered) {
        atexit(atexit_handler);
        handlers_registered = 1;
    }
    for (int i = 0; i < SPIKEHAT_MAX_INSTANCES; i++) {
        if (!g_hats[i]) { g_hats[i] = hat; break; }
    }
    pthread_mutex_unlock(&g_hats_lock);

    return hat;
}

void spikehat_close(spikehat_t *hat) {
    if (!hat) return;

    /* レジストリから除去（二重停止を防ぐ） */
    pthread_mutex_lock(&g_hats_lock);
    for (int i = 0; i < SPIKEHAT_MAX_INSTANCES; i++) {
        if (g_hats[i] == hat) { g_hats[i] = NULL; break; }
    }
    pthread_mutex_unlock(&g_hats_lock);

    /* 全使用ポートを停止し、HATが処理するまで待つ */
    for (int i = 0; i < SPIKEHAT_MAX_PORTS; i++) {
        if (hat->ports[i].device != SPIKEHAT_DEVICE_NONE)
            proto_sendf(hat->fd, "port %d; coast", i);
    }
    usleep(200000);  /* 200ms: HATがoffコマンドを処理する時間 */
    hat->running = 0;
    pthread_join(hat->reader, NULL);
    serial_close(hat->fd);
    pthread_mutex_destroy(&hat->lock);
    free(hat);
}

int spikehat_port_config(spikehat_t *hat, int port, spikehat_device_t type) {
    if (!hat || port < 0 || port >= SPIKEHAT_MAX_PORTS) return -1;
    hat->ports[port].device  = type;
    hat->ports[port].valid   = 0;
    hat->ports[port].nvalues = 0;

    switch (type) {
    case SPIKEHAT_DEVICE_MOTOR_M:
    case SPIKEHAT_DEVICE_MOTOR_L:
        /* コンビモード0: speed(M1)+position(M2)+apos(M3) -> P<N>C0: <spd> <pos> <apos> */
        proto_sendf(hat->fd,
            "port %d; port_plimit 0.7; pwmparams 0.65 0.01; "
            "combi 0 1 0 2 0 3 0; select 0; selrate 10", port);
        break;
    case SPIKEHAT_DEVICE_DISTANCE:
        /* シンプルモード0: 距離(mm) -> P<N>M0: <mm> */
        proto_sendf(hat->fd,
            "port %d; port_plimit 1; set -1; select 0; selrate 10", port);
        break;
    case SPIKEHAT_DEVICE_COLOR:
        /* シンプルモード6: HSV -> P<N>M6: <hue> <sat> <val> (初期値) */
        hat->ports[port].select_mode = 6;
        proto_sendf(hat->fd,
            "port %d; port_plimit 1; set -1; select 6; selrate 10", port);
        break;
    case SPIKEHAT_DEVICE_FORCE:
        /* コンビモード0: force(M0)+pressed(M1)+peak(M3) -> P<N>C0: <f> <p> <peak> */
        proto_sendf(hat->fd,
            "port %d; combi 0 0 0 1 0 3 0; select 0; selrate 10", port);
        break;
    default:
        return -1;
    }
    return 0;
}

void spikehat_sleep(spikehat_t *hat, float seconds) {
    (void)hat;  /* 実機版では hat は使わない */
    usleep((useconds_t)(seconds * 1e6f));
}
