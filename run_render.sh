#!/bin/bash
cd "build"
make -j${NUMBER_OF_PROCESSORS:-$(nproc)}
cd ..

# compilar tudo ter em modo Release
#cd /c/Robotica/build
#cmake .. -DCMAKE_PREFIX_PATH="/c/Robotica/GigaLearnCPP/libtorch" -DCMAKE_BUILD_TYPE=Release
#make -j${NUMBER_OF_PROCESSORS:-$(nproc)}
# --- CONFIGURAÇÕES ---
LOG_FILE="crash_report.log"
TEMP_LIMIT=90
COOL_DOWN=600 # 10 minutos em segundos
PROJECT_ROOT="/c/Robotica"
BIN_PATH="$PROJECT_ROOT/build"

# --- VARIÁVEIS DE AMBIENTE (Essenciais para a tua GPU) ---
# export HSA_OVERRIDE_GFX_VERSION=12.0.1  # ROCm/Linux only
# export AMD_SERIALIZE_KERNEL=3            # ROCm/Linux only
export PYTHONHOME="/c/Python314"
export PYTHONPATH="/c/Python314/Lib/site-packages:$BIN_PATH/python_scripts"
# export LD_LIBRARY_PATH="/opt/rocm/lib:$LD_LIBRARY_PATH"  # Linux only

while true; do
    echo "[$(date)] A iniciar GigaLearnBot..." | tee -a "$LOG_FILE"

    cd "$BIN_PATH"
    ./GigaLearnBot.exe --render

    # Captura o erro (Exit Code)
    EXIT_CODE=$?

    if [ $EXIT_CODE -ne 0 ]; then
        echo "[$(date)] CRASH DETETADO! Código: $EXIT_CODE" | tee -a "$LOG_FILE"

        # Obtém a temperatura atual da GPU (Sensor Edge)
        # rocm-smi não disponível no Windows; substituir por ferramenta AMD equivalente
        # GPU_TEMP=$(rocm-smi --showtemp | grep -m 1 'Temperature' | awk '{print $2}' | cut -d'.' -f1)

        # echo "Temperatura da GPU: ${GPU_TEMP}°C" | tee -a "$LOG_FILE"

        # if [ "$GPU_TEMP" -gt "$TEMP_LIMIT" ]; then
        #     echo "ALERTA: GPU a ${GPU_TEMP}°C! A arrefecer por 10 minutos..." | tee -a "$LOG_FILE"
        #     sleep $COOL_DOWN
        # else
            echo "Temperatura GPU indisponível no Windows. A reiniciar em 5 segundos..." | tee -a "$LOG_FILE"
            sleep 5
        # fi
    else
        echo "[$(date)] Bot fechado manualmente ou finalizado com sucesso." | tee -a "$LOG_FILE"
        break # Sai do loop se tu fechares o programa normalmente
    fi
done
