#include "protocol.h"
#include "serial.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

int proto_sendf(int fd, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n < 0 || n >= (int)(sizeof(buf) - 2)) return -1;
    buf[n++] = '\r';
    buf[n]   = '\0';
    return serial_write(fd, buf, n);
}

/*
 * 出力フォーマット (buildhat firmware):
 *   P<N>M<mode>: <v1> <v2> ...   シンプルモード
 *   P<N>C<idx>:  <v1> <v2> ...   コンビモード
 *   P<N>: <message>               接続/切断などのステータス
 */
int proto_parse(const char *line,
                int *port, int *mode_type, int *mode_idx,
                float *values, int *nvalues) {
    if (!line || line[0] != 'P') return 0;
    if (!isdigit((unsigned char)line[1])) return 0;
    if (line[2] != 'M' && line[2] != 'C') return 0;
    if (!isdigit((unsigned char)line[3]) || line[4] != ':') return 0;

    *port      = line[1] - '0';
    *mode_type = (unsigned char)line[2];
    *mode_idx  = line[3] - '0';

    const char *p = line + 5;
    int n = 0;
    while (n < 8) {
        float v;
        int consumed = 0;
        if (sscanf(p, " %f%n", &v, &consumed) != 1) break;
        values[n++] = v;
        p += consumed;
    }
    *nvalues = n;
    return (n > 0) ? 1 : 0;
}
