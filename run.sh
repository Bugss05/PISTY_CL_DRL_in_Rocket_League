#!/bin/bash
cd "build"
make -j$(nproc) || { echo "Erro ao compilar! A cancelar arranque."; exit 1; }
cd ..

# compilar tudo ter em modo Release
#cd /home/bugss/Desktop/Robotica/build
#cmake .. -DCMAKE_PREFIX_PATH="/home/bugss/Desktop/Robotica/GigaLearnCPP/libtorch" -DCMAKE_BUILD_TYPE=Release
#make -j$(nproc)
# --- CONFIGURAÇÕES ---
LOG_FILE="crash_report.log"
TEMP_LIMIT=90
COOL_DOWN=600 # 10 minutos em segundos
PROJECT_ROOT="/home/bugss/Desktop/Robotica"
BIN_PATH="$PROJECT_ROOT/build"

# --- VARIÁVEIS DE AMBIENTE (Essenciais para a tua GPU) ---
export HSA_OVERRIDE_GFX_VERSION=12.0.1
export AMD_SERIALIZE_KERNEL=3
export PYTHONHOME="/usr"
export PYTHONPATH="/usr/local/lib/python3.12/dist-packages:$BIN_PATH/python_scripts"
export LD_LIBRARY_PATH="/opt/rocm/lib:$LD_LIBRARY_PATH"

while true; do
    echo "[$(date)] A iniciar GigaLearnBot..." | tee -a "$LOG_FILE"
    
    cd "$BIN_PATH"
    ./GigaLearnBot
    
    # Captura o erro (Exit Code)
    EXIT_CODE=$?
    
    if [ $EXIT_CODE -ne 0 ]; then
        echo "[$(date)] CRASH DETETADO! Código: $EXIT_CODE" | tee -a "$LOG_FILE"
        
        # Obtém a temperatura atual da GPU (Sensor Edge)
        # Usamos o rocm-smi para extrair apenas o número
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
        break # Sai do loop se tu fechares o programa normalmente
    fi
done