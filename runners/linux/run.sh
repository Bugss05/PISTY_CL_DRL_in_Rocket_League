#!/bin/bash

cfg() { grep -E "^#define $1 " "$(dirname "$0")/../../src/config.h" | sed 's/.*"\(.*\)".*/\1/'; }

PROJECT_ROOT=$(cfg CONFIG_PROJECT_ROOT)
LIBTORCH_PATH=$(cfg CONFIG_LIBTORCH_PATH)
PYTHON_HOME=$(cfg CONFIG_PYTHON_HOME)
PYTHON_PACKAGES=$(cfg CONFIG_PYTHON_PACKAGES)
ROCM_LIB=$(cfg CONFIG_ROCM_LIB)
BIN_PATH="$PROJECT_ROOT/build"

cd "$PROJECT_ROOT/build"
make -j$(nproc) || { echo "Erro ao compilar! A cancelar arranque."; exit 1; }
cd "$PROJECT_ROOT"

# Para compilar de raiz com cmake (so precisas quando adicionas ficheiros .cpp novos):
#   cd "$PROJECT_ROOT/build"
#   cmake .. -DCMAKE_PREFIX_PATH="$LIBTORCH_PATH" -DCMAKE_BUILD_TYPE=Release
#   make -j$(nproc)

# --- CONFIGURAÇÕES ---
LOG_FILE="crash_report.log"
TEMP_LIMIT=90
COOL_DOWN=600

# --- VARIÁVEIS DE AMBIENTE ---
export HSA_OVERRIDE_GFX_VERSION=12.0.1
# AMD_SERIALIZE_KERNEL=3  # DEBUG ONLY — serializa kernels GPU, mata throughput
export ROCR_VISIBLE_DEVICES=0
export HIP_FORCE_DEV_KERNARG=1
export MALLOC_ARENA_MAX=4
export PYTHONHOME="$PYTHON_HOME"
export PYTHONPATH="$PYTHON_PACKAGES:$BIN_PATH/python_scripts"
export LD_LIBRARY_PATH="$ROCM_LIB:$LD_LIBRARY_PATH"

while true; do
    echo "[$(date)] A iniciar GigaLearnBot..." | tee -a "$LOG_FILE"

    cd "$BIN_PATH"
    ./GigaLearnBot

    EXIT_CODE=$?

    if [ $EXIT_CODE -ne 0 ]; then
        echo "[$(date)] CRASH DETETADO! Código: $EXIT_CODE" | tee -a "$LOG_FILE"

        GPU_TEMP=$(rocm-smi --showtemp | grep -m 1 'Temperature' | awk '{print $2}' | cut -d'.' -f1)

        echo "Temperatura da GPU: ${GPU_TEMP}°C" | tee -a "$LOG_FILE"

        if [ "$GPU_TEMP" -gt "$TEMP_LIMIT" ]; then
            echo "ALERTA: GPU a ${GPU_TEMP}°C! A arrefecer por 10 minutos..." | tee -a "$LOG_FILE"
            sleep $COOL_DOWN
        else
            echo "Temperatura segura (${GPU_TEMP}°C). A reiniciar em 5 segundos..." | tee -a "$LOG_FILE"
            sleep 5
        fi
    else
        echo "[$(date)] Bot fechado manualmente ou finalizado com sucesso." | tee -a "$LOG_FILE"
        break
    fi
done
