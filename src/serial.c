#include "serial.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>

int serial_open(const char *device) {
    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0) return -1;

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    tcgetattr(fd, &tty);
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &tty);
    return fd;
}

void serial_close(int fd) {
    close(fd);
}

int serial_write(int fd, const char *buf, int len) {
    return (int)write(fd, buf, len);
}

int serial_readline(int fd, char *buf, int maxlen, int timeout_ms) {
    struct timeval tv;
    fd_set fds;
    int i = 0;
    while (i < maxlen - 1) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) break;
        char c;
        if (read(fd, &c, 1) != 1) break;
        if (c == '\n') { buf[i] = '\0'; return i; }
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}
