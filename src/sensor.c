#include "../include/spikehat.h"
#include <pthread.h>
#include <string.h>

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

int spikehat_distance_read(spikehat_t *hat, int port, int *mm) {
    float v[8]; int n;
    if (get_cached(hat, port, v, &n) != 0 || n < 1) return -1;
    *mm = (int)v[0];
    return 0;
}

int spikehat_color_read_hsv(spikehat_t *hat, int port, int *hue, int *sat, int *val) {
    float v[8]; int n;
    if (get_cached(hat, port, v, &n) != 0 || n < 3) return -1;
    *hue = (int)v[0];
    *sat = (int)v[1];
    *val = (int)v[2];
    return 0;
}

int spikehat_force_read(spikehat_t *hat, int port, int *force, int *pressed) {
    float v[8]; int n;
    if (get_cached(hat, port, v, &n) != 0 || n < 2) return -1;
    *force   = (int)v[0];
    *pressed = (int)v[1];
    return 0;
}
