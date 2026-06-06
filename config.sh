#!/bin/bash
# ============================================================
#  config.sh — Caminhos locais da máquina
#  Edita APENAS este ficheiro quando mudares o projeto de sítio
# ============================================================

# Raiz do projeto (pasta onde está este ficheiro)
PROJECT_ROOT="/home/bugss/Desktop/Robotica"

# Libtorch dentro do GigaLearnCPP (usado no cmake)
LIBTORCH_PATH="$PROJECT_ROOT/GigaLearnCPP/libtorch"

# Python — home e pacotes instalados
PYTHON_HOME="/usr"
PYTHON_PACKAGES="/usr/local/lib/python3.12/dist-packages"

# ROCm (AMD GPU) — tipicamente /opt/rocm
ROCM_LIB="/opt/rocm/lib"

# Meshes de colisão do RocketSim
# DEVE ser igual ao argumento de RocketSim::Init() no ExampleMain.cpp
COLLISION_MESHES="$PROJECT_ROOT/collision_meshes"
