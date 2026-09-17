#!/bin/sh
# Build and run the game-rules simulator on the host. No ESP-IDF needed.
set -e
cd "$(dirname "$0")/../.."
cc -std=c11 -O2 -Wall -Wextra -Ifirmware/main firmware/main/pet.c tools/sim/sim.c -o /tmp/tamagotchi-sim
/tmp/tamagotchi-sim
