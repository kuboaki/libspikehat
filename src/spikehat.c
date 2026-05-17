#include "../include/spikehat.h"
#include "serial.h"
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

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
    return hat;
}

void spikehat_close(spikehat_t *hat) {
    if (!hat) return;
    /* 全使用ポートを停止 */
    for (int i = 0; i < SPIKEHAT_MAX_PORTS; i++) {
        if (hat->ports[i].device != SPIKEHAT_DEVICE_NONE)
            proto_sendf(hat->fd, "port %d; coast; off", i);
    }
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
        /* シンプルモード6: HSV -> P<N>M6: <hue> <sat> <val> */
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
