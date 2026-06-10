#!/usr/bin/env python3
"""モーターの位置をリセット(Python版)"""
import sys
sys.path.insert(0, '../python')
from spikehat import SpikeHat, DEVICE_MOTOR_M

PORT_MOTOR  = 0   # ポートA: Mアンギュラーモーター
ALIGN_SPEED = 10  # 初期位置合わせの速度
RUN_SPEED   = 5   # motor_start テストの速度

def return_to_origin(hat, port):
    """現在位置から原点(0度)へ戻す（初期位置合わせ）"""
    cur_pos = hat.motor_get_position(port)
    if cur_pos == 0:
        return

    print(f"初期位置合わせ: 現在位置 {cur_pos} 度 -> 0 度")
    hat.motor_run_for_degrees(port, -cur_pos, ALIGN_SPEED)

    dur = (abs(cur_pos) / 360.0) / (ALIGN_SPEED * 0.05)
    if dur < 0.5:
        dur = 0.5
    hat.sleep(dur + 0.5)

def print_status(hat, port):
    try:
        print(f"速度: {hat.motor_get_speed(port)}")
        print(f"位置: {hat.motor_get_position(port)} 度")
    except RuntimeError as e:
        print(f"フィードバックなし: {e}")

with SpikeHat() as hat:
    hat.port_config(PORT_MOTOR, DEVICE_MOTOR_M)
    hat.sleep(1.0)
    print("=== モーター位置をリセット ===")
    return_to_origin(hat, PORT_MOTOR)
    print("\n完了")
