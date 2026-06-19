#!/bin/bash
# Rebuild cmake de raiz — usa quando adicionas ficheiros .cpp novos
cfg() { grep -E "^#define $1 " "$(dirname "$0")/../../src/config.h" | sed 's/.*"\(.*\)".*/\1/'; }

PROJECT_ROOT=$(cfg CONFIG_PROJECT_ROOT)
LIBTORCH_PATH=$(cfg CONFIG_LIBTORCH_PATH)

rm -rf "$PROJECT_ROOT/build"
mkdir "$PROJECT_ROOT/build"
cd "$PROJECT_ROOT/build"
cmake .. -DCMAKE_PREFIX_PATH="$LIBTORCH_PATH" -DCMAKE_BUILD_TYPE=Release
