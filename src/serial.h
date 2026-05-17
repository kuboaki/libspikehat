#ifndef SERIAL_H
#define SERIAL_H

int  serial_open(const char *device);
void serial_close(int fd);
int  serial_write(int fd, const char *buf, int len);
int  serial_readline(int fd, char *buf, int maxlen, int timeout_ms);

#endif
