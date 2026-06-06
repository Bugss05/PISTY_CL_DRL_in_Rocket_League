# Configuração Local — `src/config.h`

Este é o **único ficheiro que precisas de editar** quando clonares o projeto numa máquina nova.  
O `ExampleMain.cpp` e os scripts `run.sh` / `run_render.sh` lêem os caminhos daqui.

---

## Variáveis

| Define | O que é | Exemplo |
|---|---|---|
| `CONFIG_PROJECT_ROOT` | Pasta raiz do projeto (onde está o `run.sh`) | `/home/utilizador/Robotica` |
| `CONFIG_COLLISION_MESHES` | Pasta com as meshes de colisão do RocketSim | `/home/utilizador/Robotica/collision_meshes` |
| `CONFIG_LIBTORCH_PATH` | Pasta do libtorch (usada no cmake) | `/home/utilizador/Robotica/GigaLearnCPP/libtorch` |
| `CONFIG_PYTHON_HOME` | Raiz da instalação Python | `/usr` |
| `CONFIG_PYTHON_PACKAGES` | Pasta dos pacotes Python instalados | `/usr/local/lib/python3.12/dist-packages` |
| `CONFIG_ROCM_LIB` | Bibliotecas do ROCm (AMD GPU) | `/opt/rocm/lib` |

---

## Como configurar numa máquina nova

1. Clona o repositório
2. Abre `src/config.h`
3. Substitui os caminhos pelos da tua máquina
4. Compila com cmake (ver abaixo)

```cpp
#define CONFIG_PROJECT_ROOT     "/caminho/para/o/projeto"
#define CONFIG_COLLISION_MESHES "/caminho/para/o/projeto/collision_meshes"
#define CONFIG_LIBTORCH_PATH    "/caminho/para/o/projeto/GigaLearnCPP/libtorch"
#define CONFIG_PYTHON_HOME      "/usr"
#define CONFIG_PYTHON_PACKAGES  "/usr/local/lib/python3.12/dist-packages"
#define CONFIG_ROCM_LIB         "/opt/rocm/lib"
```

---

## Primeira compilação (cmake)

Só precisas de correr o cmake quando:
- É a primeira vez que compilas
- Adicionaste ficheiros `.cpp` novos

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH="<CONFIG_LIBTORCH_PATH>" -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Nas compilações seguintes basta correr `run.sh` — ele faz `make` automaticamente.

---

## Onde encontrar cada caminho

### `CONFIG_COLLISION_MESHES`
Pasta com os ficheiros `.bin` das meshes. Tens de a descarregar separadamente do RocketSim.  
Estrutura esperada:
```
collision_meshes/
├── soccar/
│   ├── CornerA.bin
│   ├── ...
```

### `CONFIG_LIBTORCH_PATH`
Descarrega o libtorch em [pytorch.org](https://pytorch.org/get-started/locally/) (versão ROCm se tiveres AMD).  
Coloca na pasta `GigaLearnCPP/libtorch/`.

### `CONFIG_PYTHON_PACKAGES`
Para descobrir o caminho correto na tua máquina:
```bash
python3 -c "import site; print(site.getsitepackages()[0])"
```

### `CONFIG_ROCM_LIB`
Se o ROCm estiver instalado em sítio diferente:
```bash
find /opt /usr -name "libamdhip64.so" 2>/dev/null
```
Usa a pasta onde esse ficheiro está.

---

## Notas para AMD GPU (ROCm)

Se tiveres uma GPU AMD, verifica também a variável de ambiente no `run.sh`:

```bash
export HSA_OVERRIDE_GFX_VERSION=12.0.1   # para RX 9070 XT (gfx1201)
```

Consulta a tua arquitetura com:
```bash
rocminfo | grep gfx
```
