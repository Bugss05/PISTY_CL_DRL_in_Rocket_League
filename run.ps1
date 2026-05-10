# PowerShell version for Windows
# Run this with: powershell -ExecutionPolicy Bypass -File run.ps1

# --- CONFIGURAÇÕES ---
$LOG_FILE = "crash_report.log"
$TEMP_LIMIT = 90
$COOL_DOWN = 600  # 10 minutos em segundos
$PROJECT_ROOT = "C:\Users\diogo\OneDrive\Área de Trabalho\LIACD\3_ANO\IRI\Robotica"
$BIN_PATH = "$PROJECT_ROOT\build"
$BUILD_DIR = "$PROJECT_ROOT\build"

# --- VARIÁVEIS DE AMBIENTE (Essenciais para a tua GPU) ---
$env:HSA_OVERRIDE_GFX_VERSION = "12.0.1"
$env:AMD_SERIALIZE_KERNEL = "3"
$env:PYTHONHOME = "C:\Python312"  # Ajusta conforme tua instalação Python
$env:PYTHONPATH = "$BIN_PATH\python_scripts"

# Build
Write-Host "[$(Get-Date)] A compilar o projeto..." -ForegroundColor Cyan
Set-Location $BUILD_DIR
cmake --build . --config Release --target GigaLearnBot
$BUILD_EXIT = $LASTEXITCODE

if ($BUILD_EXIT -ne 0) {
    Write-Host "[$(Get-Date)] ERRO NA COMPILAÇÃO! Código: $BUILD_EXIT" -ForegroundColor Red
    Add-Content -Path $LOG_FILE -Value "[$(Get-Date)] ERRO NA COMPILAÇÃO! Código: $BUILD_EXIT"
    exit 1
}

Write-Host "[$(Get-Date)] Compilação bem-sucedida!" -ForegroundColor Green

# Loop de execução
$continueLoop = $true
while ($continueLoop) {
    Write-Host "[$(Get-Date)] A iniciar GigaLearnBot..." -ForegroundColor Cyan
    Add-Content -Path $LOG_FILE -Value "[$(Get-Date)] A iniciar GigaLearnBot..."
    
    & "$BIN_PATH\GigaLearnBot.exe"
    $EXIT_CODE = $LASTEXITCODE
    
    if ($EXIT_CODE -ne 0) {
        Write-Host "[$(Get-Date)] CRASH DETETADO! Código: $EXIT_CODE" -ForegroundColor Red
        Add-Content -Path $LOG_FILE -Value "[$(Get-Date)] CRASH DETETADO! Código: $EXIT_CODE"
        
        # Tenta obter temperatura da GPU (se rocm-smi estiver disponível)
        try {
            $rocmSmi = rocm-smi --showtemp 2>$null
            if ($rocmSmi) {
                $GPU_TEMP = $rocmSmi | Select-String 'Temperature' | Select-Object -First 1 | 
                            ForEach-Object { [int]($_ -split '\s+')[1] }
                
                Write-Host "Temperatura da GPU: ${GPU_TEMP}°C" -ForegroundColor Yellow
                Add-Content -Path $LOG_FILE -Value "Temperatura da GPU: ${GPU_TEMP}°C"
                
                if ($GPU_TEMP -gt $TEMP_LIMIT) {
                    Write-Host "ALERTA: GPU a ${GPU_TEMP}°C! A arrefecer por 10 minutos..." -ForegroundColor Red
                    Add-Content -Path $LOG_FILE -Value "ALERTA: GPU a ${GPU_TEMP}°C! A arrefecer por 10 minutos..."
                    Start-Sleep -Seconds $COOL_DOWN
                }
                else {
                    Write-Host "Temperatura segura (${GPU_TEMP}°C). A reiniciar em 5 segundos..." -ForegroundColor Green
                    Add-Content -Path $LOG_FILE -Value "Temperatura segura (${GPU_TEMP}°C). A reiniciar em 5 segundos..."
                    Start-Sleep -Seconds 5
                }
            }
            else {
                Write-Host "rocm-smi não disponível. A reiniciar em 5 segundos..." -ForegroundColor Yellow
                Add-Content -Path $LOG_FILE -Value "rocm-smi não disponível. A reiniciar em 5 segundos..."
                Start-Sleep -Seconds 5
            }
        }
        catch {
            Write-Host "Não foi possível verificar temperatura da GPU. A reiniciar em 5 segundos..." -ForegroundColor Yellow
            Add-Content -Path $LOG_FILE -Value "Não foi possível verificar temperatura da GPU. A reiniciar em 5 segundos..."
            Start-Sleep -Seconds 5
        }
    }
    else {
        Write-Host "[$(Get-Date)] Bot fechado manualmente ou finalizado com sucesso." -ForegroundColor Green
        Add-Content -Path $LOG_FILE -Value "[$(Get-Date)] Bot fechado manualmente ou finalizado com sucesso."
        $continueLoop = $false
    }
}

Write-Host "Script finalizado." -ForegroundColor Cyan
