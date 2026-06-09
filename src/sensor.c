#include "../include/spikehat.h"
#include "protocol.h"
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>

static int get_cached(spikehat_t *hat, int port, float *values, int *nvalues) {
    if (!hat || port < 0 || port >= SPIKEHAT_MAX_PORTS) return -1;
    pthread_mutex_lock(&hat->lock);
    int valid = hat->ports[port].valid;
    if (valid) {
        *nvalues = hat->ports[port].nvalues;
        memcpy(values, hat->ports[port].values, (size_t)(*nvalues) * sizeof(float));
    }
    pthread_mutex_unlock(&hat->lock);
    return valid ? 0 : -1;
}

/* カラーセンサーのモードを切り替える。
 * 切り替えが必要な場合はキャッシュを無効化し、新データを待つ (最大1秒)。 */
static int color_switch_mode(spikehat_t *hat, int port, int new_mode) {
    pthread_mutex_lock(&hat->lock);
    int same = (hat->ports[port].select_mode == new_mode);
    pthread_mutex_unlock(&hat->lock);
    if (same) return 0;

    /* モード切り替え: キャッシュ無効化 → select コマンド送信 */
    pthread_mutex_lock(&hat->lock);
    hat->ports[port].valid       = 0;
    hat->ports[port].select_mode = new_mode;
    pthread_mutex_unlock(&hat->lock);

    /* set -1 でランプ電源を維持してからモード切り替え */
    proto_sendf(hat->fd, "port %d; port_plimit 1; set -1; select; select %d; selrate 10", port, new_mode);

    /* 新しいデータが届くまで待つ (最大1秒) */
    for (int i = 0; i < 20; i++) {
        usleep(50000); /* 50ms */
        pthread_mutex_lock(&hat->lock);
        int valid = hat->ports[port].valid;
        pthread_mutex_unlock(&hat->lock);
        if (valid) return 0;
    }
    return -1; /* タイムアウト */
}

int spikehat_distance_read(spikehat_t *hat, int port, int *mm) {
    float v[8]; int n;
    if (get_cached(hat, port, v, &n) != 0 || n < 1) return -1;
    *mm = (int)v[0];
    return 0;
}

int spikehat_color_read_hsv(spikehat_t *hat, int port, int *hue, int *sat, int *val) {
    if (color_switch_mode(hat, port, 6) != 0) return -1;
    float v[8]; int n;
    if (get_cached(hat, port, v, &n) != 0 || n < 3) return -1;
    *hue = (int)v[0];
    *sat = (int)v[1];
    *val = (int)v[2];
    return 0;
}

int spikehat_color_read_rgb(spikehat_t *hat, int port, int *r, int *g, int *b) {
    if (color_switch_mode(hat, port, 5) != 0) return -1;
    float v[8]; int n;
    if (get_cached(hat, port, v, &n) != 0 || n < 3) return -1;
    /* モード5のRGB値は 0〜1024 → 0〜255 にスケール */
    *r = (int)((v[0] / 1024.0f) * 255.0f);
    *g = (int)((v[1] / 1024.0f) * 255.0f);
    *b = (int)((v[2] / 1024.0f) * 255.0f);
    return 0;
}

int spikehat_force_read(spikehat_t *hat, int port, int *force, int *pressed) {
    float v[8]; int n;
    if (get_cached(hat, port, v, &n) != 0 || n < 2) return -1;
    *force   = (int)roundf(v[0] / 10.0f);
    *pressed = (int)v[1];
    return 0;
}

int spikehat_force_is_pressed(spikehat_t *hat, int port, int *pressed) {
    int force = 0;
    if (spikehat_force_read(hat, port, &force, pressed) != 0) return -1;
    return 0;
}

int spikehat_force_get_force(spikehat_t *hat, int port, int *force) {
    int pressed = 0;
    if (spikehat_force_read(hat, port, force, &pressed) != 0) return -1;
    return 0;
}
