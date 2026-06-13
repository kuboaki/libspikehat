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
    return proto_sendf(hat->fd,
        "port %d; select 0; selrate 10; "
        "pid %d 0 0 s1 1 0 0.003 0.01 0 100 0.01; "
        "set pulse %d 0 %f 0",
        port, port, speed, (double)seconds);
}

int spikehat_motor_run_to_position(spikehat_t *hat, int port, int position_deg, int speed) {
    if (speed == 0)   return -1;
    if (speed < -100) speed = -100;
    if (speed >  100) speed =  100;
    if (speed < 0) speed = -speed;

    /* 現在位置を取得 */
    int cur_pos = 0;
    spikehat_motor_get_position(hat, port, &cur_pos);

    /* Python buildhat と同仕様: speed×0.05 で 0〜5 の速度単位に変換 */
    double sv     = speed * 0.05;
    double pos    = cur_pos / 360.0;        /* 現在位置 (回転数) */
    double newpos = position_deg / 360.0;   /* 目標位置 (回転数) */
    double diff   = newpos - pos;
    double dur    = (diff < 0 ? -diff : diff) / sv;
    /* 最低でも0.5秒を確保 (モーターが物理的に追いつけるよう余裕を持たせる) */
    if (dur < 0.5) dur = 0.5;

    return proto_sendf(hat->fd,
        "port %d; select 0; selrate 10; "
        "pid %d 0 1 s4 0.0027777778 0 5 0 .1 3 0.01; "
        "set ramp %f %f %f 0",
        port, port, pos, newpos, dur);
}

int spikehat_motor_run_for_degrees(spikehat_t *hat, int port, int degrees, int speed) {
    if (speed == 0) return -1;

    /* 負のspeedは方向反転として扱う (Python buildhat と同仕様) */
    int mul = (speed < 0) ? -1 : 1;

    /* 現在位置を取得 */
    int cur_pos = 0;
    spikehat_motor_get_position(hat, port, &cur_pos);

    return spikehat_motor_run_to_position(hat, port, cur_pos + degrees * mul, speed);
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
