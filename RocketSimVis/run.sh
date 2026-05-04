#!/bin/bash

# No Linux, usamos $(dirname "$0") para obter a pasta do script
# Isto substitui o %0\.. do Windows
SCRIPT_DIR=$(dirname "$0")

# Rodar o python (no Ubuntu usa-se python3)
python3 "$SCRIPT_DIR/src/main.py"

# O comando 'pause' do Windows traduz-se para o 'read' no Linux
read -p "Pressione [Enter] para continuar..."