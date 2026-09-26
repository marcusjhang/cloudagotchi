#!/bin/sh
# Everything that can run on the host during development. No ESP-IDF, <1 s.
#   firmware/main/pet.c   game rules over simulated weeks   (tools/sim)
#   firmware/main/hw_math.h  RTC/PMIC pure math             (tools/tests)
set -e
cd "$(dirname "$0")/.."

echo "== game rules (tools/sim) =="
tools/sim/run.sh
echo
echo "== hardware math (tools/tests) =="
cc -std=c11 -O2 -Wall -Wextra -Ifirmware/main tools/tests/test_hw_math.c -o /tmp/tamagotchi-hwmath
/tmp/tamagotchi-hwmath
echo
echo "== pet E2E (tools/e2e) =="
cc -std=c11 -O2 -Wall -Wextra -Ifirmware/main firmware/main/pet.c tools/e2e/e2e.c -o /tmp/tamagotchi-e2e
/tmp/tamagotchi-e2e
