#include "../include/spikehat.h"
#include "protocol.h"
#include <pthread.h>

int spikehat_motor_pwm(spikehat_t *hat, int port, float power) {
    if (power < -1.0f) power = -1.0f;
    if (power >  1.0f) power =  1.0f;
    return proto_sendf(hat->fd, "port %d; pwm; set %f", port, (double)power);
}

int spikehat_motor_start(spikehat_t *hat, int port, int speed) {
    if (speed < -100) speed = -100;
    if (speed >  100) speed =  100;
    /* speed は -100〜100 のまま PID セットポイントに渡す (Python buildhat と同仕様) */
    return proto_sendf(hat->fd,
        "port %d; select 0; selrate 10; "
        "pid %d 0 0 s1 1 0 0.003 0.01 0 100 0.01; set %d",
        port, port, speed);
}

int spikehat_motor_stop(spikehat_t *hat, int port) {
    return proto_sendf(hat->fd, "port %d; off", port);
}

int spikehat_motor_coast(spikehat_t *hat, int port) {
    return proto_sendf(hat->fd, "port %d; coast", port);
}

int spikehat_motor_run_for_seconds(spikehat_t *hat, int port, float seconds, int speed) {
    if (speed < -100) speed = -100;
    if (speed >  100) speed =  100;
    /* speed は -100〜100 のまま使用 (Python buildhat と同仕様) */
    return proto_sendf(hat->fd,
        "port %d; select 0; selrate 10; "
        "pid %d 0 0 s1 1 0 0.003 0.01 0 100 0.01; "
        "set pulse %d 0 %f 0",
        port, port, speed, (double)seconds);
}

int spikehat_motor_get_speed(spikehat_t *hat, int port, int *speed) {
    if (!hat || port < 0 || port >= SPIKEHAT_MAX_PORTS) return -1;
    pthread_mutex_lock(&hat->lock);
    int valid = hat->ports[port].valid && hat->ports[port].nvalues >= 1;
    if (valid) *speed = (int)hat->ports[port].values[0];
    pthread_mutex_unlock(&hat->lock);
    return valid ? 0 : -1;
}

int spikehat_motor_get_position(spikehat_t *hat, int port, int *degrees) {
    if (!hat || port < 0 || port >= SPIKEHAT_MAX_PORTS) return -1;
    pthread_mutex_lock(&hat->lock);
    int valid = hat->ports[port].valid && hat->ports[port].nvalues >= 2;
    if (valid) *degrees = (int)hat->ports[port].values[1];
    pthread_mutex_unlock(&hat->lock);
    return valid ? 0 : -1;
}
