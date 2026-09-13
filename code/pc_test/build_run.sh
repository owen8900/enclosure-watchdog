#!/bin/bash
# ---------------------------------------------------------------
# 在 WSL 里编译并运行核心算法的 PC 端验证
# 用法：bash build_run.sh
# ---------------------------------------------------------------
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="/tmp/cabinet_test_$$"

echo "== 编译 =="
gcc -O2 -Wall -Wextra -std=c11 -I"$DIR/../App" \
    "$DIR/../App/filter.c" \
    "$DIR/../App/alarm_fsm.c" \
    "$DIR/../App/diagnose.c" \
    "$DIR/../App/heater.c" \
    "$DIR/main_pc_test.c" \
    -o "$OUT" -lm

echo "== 运行 =="
"$OUT"
RC=$?

rm -f "$OUT"
exit $RC
