#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdarg.h>

/* printf形式でコマンドを送信 (\r を自動付加) */
int proto_sendf(int fd, const char *fmt, ...);

/*
 * 受信行を解析する
 * 戻り値: 1=データ行 (port/mode_type/mode_idx/values/nvalues が設定される)
 *          0=データ行でない (接続/切断メッセージなど)
 * mode_type: 'M'=シンプルモード, 'C'=コンビモード
 */
int proto_parse(const char *line,
                int *port, int *mode_type, int *mode_idx,
                float *values, int *nvalues);

#endif
