# libspikehat

C/Python ライブラリ — Raspberry Pi Build HAT 経由で LEGO SPIKE Prime デバイスを透過的に操作する。

## 概要

`libspikehat` は Raspberry Pi Build HAT のシリアルプロトコルを直接実装した共有ライブラリです。
C アプリケーションからはリンクして、Python アプリケーションからは `ctypes` 経由で、
**同じライブラリを通じて同じデバイスを操作**できます。

```
┌───────────────┐       ┌─────────────────────┐
│   C アプリ     │       │   Python アプリ       │
└──────┬────────┘       └──────────┬───────────┘
       │                           │ ctypes
       └────────────┬──────────────┘
              ┌─────▼──────────────────────┐
              │   libspikehat.so            │
              │  (Build HAT プロトコル実装)  │
              └─────┬──────────────────────┘
                    │ UART /dev/serial0 (115200 baud)
              ┌─────▼──────────────────────┐
              │        Build HAT            │
              └────────────────────────────┘
```

## 対応デバイス

| デバイス | 定数 | 機能 |
|---------|------|------|
| SPIKE Prime Mアンギュラーモーター | `SPIKEHAT_DEVICE_MOTOR_M` | PWM・速度制御・位置取得 |
| SPIKE Prime Lアンギュラーモーター | `SPIKEHAT_DEVICE_MOTOR_L` | 同上 |
| SPIKE Prime カラーセンサー | `SPIKEHAT_DEVICE_COLOR` | HSV 値取得 |
| SPIKE Prime 距離センサー | `SPIKEHAT_DEVICE_DISTANCE` | 距離(mm)取得 |
| SPIKE Prime フォースセンサー | `SPIKEHAT_DEVICE_FORCE` | 力(N)・押下状態取得 |

## 必要環境

- Raspberry Pi 4
- [Raspberry Pi Build HAT](https://www.raspberrypi.com/products/build-hat/)
- **Raspberry Pi OS Bookworm (64bit)** — 下記の注意を参照
- `sudo apt install python3-build-hat` (ファームウェアロード用)
- `gcc`, `cmake >= 3.15`

> **警告: OS バージョンについて (2026年5月時点)**
>
> 2026年5月現在、Raspberry Pi OS の最新版は **Trixie** ですが、
> `python3-build-hat` パッケージおよび Build HAT ファームウェアは
> **Bookworm でのみ動作確認されています**。
> Trixie では `python3-build-hat` が提供されておらず、本ライブラリも動作しません。
> Raspberry Pi 4 + Build HAT の組み合わせでは **Bookworm (Debian 12) を使用してください**。

## ビルド

```bash
git clone https://github.com/kuboaki/libspikehat.git
cd libspikehat
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

生成物:
- `build/libspikehat.so` — 共有ライブラリ
- `build/test_motor` — C サンプル(モーター)
- `build/test_sensor` — C サンプル(センサー)

## 初回起動の注意

Build HAT のファームウェアは `python3-build-hat` が管理しています。
本ライブラリを初めて使う前に、以下の手順を実行してください。

まず `python3-build-hat` をインストールします。

```bash
sudo apt install python3-build-hat
```

次に、以下を一度だけ実行してファームウェアを Build HAT にロードします。

```bash
python3 -c "from buildhat import Motor"
```

以降の起動時はこの手順は不要です。

## C 言語での使い方

```c
#include "spikehat.h"

int main(void) {
    spikehat_t *hat = spikehat_open("/dev/serial0");

    // ポート設定 (A=0, B=1, C=2, D=3)
    spikehat_port_config(hat, 0, SPIKEHAT_DEVICE_MOTOR_M);
    spikehat_port_config(hat, 3, SPIKEHAT_DEVICE_DISTANCE);
    sleep(1);

    // モーター: 速度5で2秒回転、逆方向に速度-3で2秒
    spikehat_motor_run_for_seconds(hat, 0, 2.0f, 5);
    // reverse
    spikehat_motor_run_for_seconds(hat, 0, 2.0f, -3);

    // 距離センサー読み取り
    int mm;
    if (spikehat_distance_read(hat, 3, &mm) == 0)
        printf("距離: %d mm\n", mm);

    spikehat_close(hat);
    return 0;
}
```

コンパイル:
```bash
gcc myapp.c -I/path/to/libspikehat/include \
    -L/path/to/libspikehat/build -lspikehat -lpthread \
    -Wl,-rpath,/path/to/libspikehat/build -o myapp
```

## Python での使い方

```python
import sys
sys.path.insert(0, '/path/to/libspikehat/python')
from spikehat import SpikeHat, DEVICE_MOTOR_M, DEVICE_DISTANCE

with SpikeHat() as hat:
    hat.port_config(0, DEVICE_MOTOR_M)
    hat.port_config(3, DEVICE_DISTANCE)

    # モーター: 速度5で2秒回転、逆方向に速度-3で2秒
    hat.motor_run_for_seconds(0, 2.0, 5)
    # reverse
    hat.motor_run_for_seconds(0, 2.0, -3)
    print(f"距離: {hat.distance_read(3)} mm")
```

## API リファレンス

### 初期化

| 関数 | 説明 |
|------|------|
| `spikehat_open(device)` | Build HAT をオープン。`device` は通常 `"/dev/serial0"` |
| `spikehat_close(hat)` | 全デバイスを停止してクローズ |
| `spikehat_port_config(hat, port, type)` | ポートにデバイス種別を割り当てて初期化 |

### モーター制御

| 関数 | 説明 |
|------|------|
| `spikehat_motor_pwm(hat, port, power)` | 直接 PWM 制御 (-1.0 〜 +1.0) |
| `spikehat_motor_start(hat, port, speed)` | PID 速度制御で回転開始 (-100 〜 +100) |
| `spikehat_motor_run_for_seconds(hat, port, sec, speed)` | 指定秒数回転 |
| `spikehat_motor_run_for_degrees(hat, port, deg, speed)` | 指定角度回転（speed 負値で逆転） |
| `spikehat_motor_stop(hat, port)` | 停止 (ブレーキ) |
| `spikehat_motor_coast(hat, port)` | 惰性停止 |
| `spikehat_motor_get_speed(hat, port, *speed)` | 現在速度を取得 |
| `spikehat_motor_get_position(hat, port, *degrees)` | 現在位置(度)を取得 |

**注意:** `spikehat_motor_run_for_seconds`の`seconds`は、加減速を含まない定速区間の
長さです。内部の加減速ランプを含めると、指定秒数を過ぎてから実際にモーターが
完全に停止するまで、速度に応じてさらに2〜5秒程度かかることがあります。

### センサー

| 関数 | 説明 |
|------|------|
| `spikehat_distance_read(hat, port, *mm)` | 距離をミリメートルで取得 |
| `spikehat_color_read_hsv(hat, port, *h, *s, *v)` | 色を HSV で取得 |
| `spikehat_color_read_rgb(hat, port, *r, *g, *b)` | 色を RGB で取得（各 0〜255） |
| `spikehat_force_read(hat, port, *force, *pressed)` | 力(N)と押下状態を取得 |

## プロジェクト構成

```
libspikehat/
├── include/spikehat.h      公開 API ヘッダ
├── src/
│   ├── serial.c/h          UART 通信
│   ├── protocol.c/h        Build HAT テキストプロトコル
│   ├── spikehat.c          初期化・受信スレッド・ポート管理
│   ├── motor.c             モーター制御実装
│   └── sensor.c            センサー読み取り実装
├── python/spikehat.py      Python ctypes バインディング
├── examples/
│   ├── test_motor.c/.py    モーターサンプル
│   └── test_sensor.c/.py   センサーサンプル
└── CMakeLists.txt
```

## プロトコル仕様

Build HAT との通信は `/dev/serial0` (115200 baud, 8N1) のテキストプロトコルです。
仕様の詳細は [raspberrypi/buildhat](https://github.com/raspberrypi/buildhat) リポジトリの
`docs/protocol.md` を参照してください。

## ライセンス

MIT License
