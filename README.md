# Bot Videos

To see our bot in action, please check out our companion repository with videos of the bot playing **[here](https://github.com/miguel-c05/pisty-videos)**.

# Installation & Setup Instructions

This guide walks you through setting up the project from scratch: cloning the repository, installing all required tools and dependencies, and getting everything ready to build and run. Follow the steps in order.

These instructions work on **both Linux and Windows**, but the setup is **easier and more reliable on Linux**. If you have the choice, we recommend using Linux.

## Table of Contents

- [System Requirements](#system-requirements)
- [1. Clone the Repository](#1-clone-the-repository)
- [2. Install Core Tools (CMake & Python)](#2-install-core-tools-cmake--python)
- [3. C++ Compiler (Visual Studio Build Tools)](#3-c-compiler-visual-studio-build-tools)
- [4. Optional GPU Support (CUDA / ROCm)](#4-optional-gpu-support-cuda--rocm)
- [5. Python Environment & Dependencies](#5-python-environment--dependencies)
- [6. Download LibTorch](#6-download-libtorch)
- [7. Configure Local Paths (`src/config.h`)](#7-configure-local-paths-srcconfigh)
- [8. Build the Project](#8-build-the-project)
- [9. Run the Bot](#9-run-the-bot)
- [10. Model the Robot (`src/novamain.cpp`)](#10-model-the-robot-srcnovamaincpp)
- [11. Start Training & Using Our Model](#11-start-training--using-our-model)

## System Requirements

Before starting, ensure your system meets the following requirements:

- Windows 10/11 (64-bit) or Linux
- Administrator rights (to install software)
- Internet connection (to download tools and dependencies)
- At least 8 GB RAM (16+ GB recommended for training)
- 10 GB free disk space (for the repository, libtorch, and checkpoints)

## 1. Clone the Repository

Clone the repository with `--depth 1` so you only download the latest snapshot instead of the entire git history. This keeps the download small and fast.

```bash
git clone --depth 1 https://github.com/Bugss05/Robotica.git
cd Robotica
```

## 2. Install Core Tools (CMake & Python)

Make sure you have **CMake** and **Python 3.11** installed.

### Linux

Install both with your package manager. They will be added to your `PATH` automatically, so no extra configuration is needed.

```bash
sudo apt update
sudo apt install -y cmake python3.11 python3-pip
```

Verify the installation:

```bash
cmake --version
python3.11 --version
```

### Windows

Install **CMake** and **Python 3.11**.

> **Important:** Make sure both **CMake** and **Python** are added to your **system environment variables (`PATH`)** during installation. On Linux this is handled automatically, so no extra work is required.

> **Note:** If you have multiple Python versions, ensure `python` points to 3.11. You can also use `py -3.11`, but we assume `python` works.

Verify the installation (open a **new** terminal so the updated `PATH` takes effect):

```bash
cmake --version
python --version
```

## 3. C++ Compiler (Visual Studio Build Tools)

GigaLearnCPP requires a modern C++ compiler.

### Linux

A recent version of GCC or Clang is enough. Install the build essentials with your package manager:

```bash
sudo apt update
sudo apt install -y build-essential
```

### Windows

The easiest way is to install the **Build Tools for Visual Studio 2026**.

1. Download the installer from: [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2026)
2. Run the installer and select **Desktop development with C++**.
3. This will install the MSVC compiler, CMake (optional, but we'll install CMake separately), and the Windows SDK.
4. Complete the installation and restart your computer.

> **Alternative:** If you already have Visual Studio 2022 Community/Pro with C++ tools, you can skip this step.

## 4. Optional GPU Support (CUDA / ROCm)

This step is **optional but important if you plan to train**. It is **less important if you only want to run** the model, which works fine on CPU.

- **NVIDIA GPUs:** install **CUDA**.
- **AMD GPUs:** install **ROCm**.

Your GPU must have a **Compute Capability of 3.5 or higher**. Without a supported GPU, training will be significantly slower or fall back to CPU.

## 5. Python Environment & Dependencies

The heavier **Weights & Biases** metrics require some extra Python packages. To keep them isolated from your system Python, create and activate a virtual environment, then install the dependencies from `requirements.txt`.

### Linux

```bash
python3.11 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Windows

```bash
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

> **Note:** Keep the virtual environment activated whenever you run the costlier Weights & Biases metrics. To deactivate it later, just run `deactivate`.

## 6. Download LibTorch

Go to the official LibTorch download page and choose the build that best matches your graphics card:

- **LibTorch download page:** https://pytorch.org/get-started/locally/ (select **LibTorch** as the package)

Pick the version according to your hardware:

- **NVIDIA GPU:** choose the **CUDA** build that matches the CUDA version you installed in step 4.
- **AMD GPU:** choose the **ROCm** build. ⚠️ This is **only available on Linux**, there is no ROCm LibTorch build for Windows.
- **No GPU / CPU only:** a **CPU** build is available and will work, but it is **much slower and not recommended**, especially for training.

After downloading, extract the archive **inside the `GigaLearnCPP` folder** so you end up with `GigaLearnCPP/libtorch/`. You will point the build at this folder in the next step.

## 7. Configure Local Paths (`src/config.h`)

The project uses absolute paths that differ from machine to machine. This file is **not included in the repository, you must create it yourself** at `src/config.h`. It is the **only file you need to create/edit** when setting up the project on a new machine. The main program and the runner scripts in `runners/` read all their paths from here.

### Paths to set

Create a new file `src/config.h` and add the defines below, replacing each value with the correct path for your machine:

| Define | What it is | Example |
|---|---|---|
| `CONFIG_PROJECT_ROOT` | Root folder of the project (the cloned repo root) | `/home/user/Robotica` |
| `CONFIG_COLLISION_MESHES` | Folder with the RocketSim collision meshes | `/home/user/Robotica/collision_meshes` |
| `CONFIG_LIBTORCH_PATH` | LibTorch folder (used by CMake) | `/home/user/Robotica/GigaLearnCPP/libtorch` |
| `CONFIG_PYTHON_HOME` | Root of your Python installation | `/usr` |
| `CONFIG_PYTHON_PACKAGES` | Folder with the installed Python packages | `/usr/local/lib/python3.12/dist-packages` |
| `CONFIG_ROCM_LIB` | ROCm libraries (AMD GPU only) | `/opt/rocm/lib` |
| `CONFIG_VS_GENERATOR` | **(Windows only)** Visual Studio generator name for CMake's `-G` flag | `Visual Studio 17 2022` |

Example contents:

```cpp
#define CONFIG_PROJECT_ROOT     "/path/to/project"
#define CONFIG_COLLISION_MESHES "/path/to/project/collision_meshes"
#define CONFIG_LIBTORCH_PATH    "/path/to/project/GigaLearnCPP/libtorch"
#define CONFIG_PYTHON_HOME      "/usr"
#define CONFIG_PYTHON_PACKAGES  "/usr/local/lib/python3.12/dist-packages"
#define CONFIG_ROCM_LIB         "/opt/rocm/lib"

// Windows only — CMake generator name for your installed Visual Studio
#define CONFIG_VS_GENERATOR     "Visual Studio 17 2022"
```

> **Note:** `Torch_DIR` is derived automatically from `CONFIG_LIBTORCH_PATH`, so you do **not** need to define it separately.

### How to find each path

- **`CONFIG_COLLISION_MESHES`** - the folder with the RocketSim `.bin` mesh files. You must download these separately from RocketSim. Expected structure:

  ```
  collision_meshes/
  ├── soccar/
  │   ├── CornerA.bin
  │   ├── ...
  ```

- **`CONFIG_LIBTORCH_PATH`** - the `GigaLearnCPP/libtorch/` folder from step 6.
- **`CONFIG_PYTHON_PACKAGES`** - find it on your machine with:

  ```bash
  python3 -c "import site; print(site.getsitepackages()[0])"
  ```

- **`CONFIG_ROCM_LIB`** - if ROCm is installed in a non-default location, locate it with:

  ```bash
  find /opt /usr -name "libamdhip64.so" 2>/dev/null
  ```

  Use the folder where that file lives.

- **`CONFIG_VS_GENERATOR`** (Windows only) - the CMake generator name for your installed Visual Studio:

  | Installed version | Value to use |
  |---|---|
  | Visual Studio 2022 | `Visual Studio 17 2022` |
  | Visual Studio 2019 | `Visual Studio 16 2019` |
  | Visual Studio 2017 | `Visual Studio 15 2017` |

  You can also list every generator available on your system with `cmake --help`.

> **AMD GPU (ROCm) note:** Also check the environment variable in your run script (`runners/linux/run.sh`). Set it to match your GPU architecture:
>
> ```bash
> export HSA_OVERRIDE_GFX_VERSION=12.0.1   # e.g. RX 9070 XT (gfx1201)
> ```
>
> Find your architecture with: `rocminfo | grep gfx`

## 8. Build the Project

There are two ways to build. **Try Option A first**, if it works without errors, you are done and can skip Option B.

### Option A - Automated scripts (recommended)

We provide helper scripts that speed up the whole process. They live in the **`runners/`** folder, split into a `linux/` and a `windows/` subfolder. Use the `linux/` (`.sh`) scripts on **Linux** and the `windows/` (`.bat`) scripts on **Windows**:

| Linux (`runners/linux/`) | Windows (`runners/windows/`) | What it does |
|---|---|---|
| `compile.sh` | `compile.bat` | Compiles the project. **Run this first.** You only need this script to build. |
| `run.sh` | `run.bat` | Runs the bot in training mode. Requires the compile script to have been run at least once. |
| `run_render.sh` | `run_render.bat` | Runs the bot with rendering. Requires the compile script to have been run at least once. |

So the flow is: run the compile script once, then run **either** the run script **or** the run_render script.

**If the scripts run without errors, your build is done, skip Option B.**

### Option B - Manual build (fallback)

Use this only if the automated scripts fail. This is **normally only needed on Windows**.

**Step 1: Create and enter the build folder**

```
mkdir build
cd build
```

**Step 2: Configure CMake**

Run the following command on one line. Use the same generator name you set in `CONFIG_VS_GENERATOR`, and point `Torch_DIR` at your `CONFIG_LIBTORCH_PATH` (the `share\cmake\Torch` folder inside it).

```
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTorch_DIR="<CONFIG_LIBTORCH_PATH>\share\cmake\Torch"
```

Explanation:

- `-G "Visual Studio 17 2022"` – generates the solution for your Visual Studio (the value of `CONFIG_VS_GENERATOR`).
- `-A x64` – 64-bit platform.
- `-DCMAKE_BUILD_TYPE=RelWithDebInfo` – builds the project in RelWithDebInfo.
- `-DTorch_DIR=...` – points CMake to the `TorchConfig.cmake` file inside libtorch.

> **Note:** The automated scripts derive `Torch_DIR` automatically from `CONFIG_LIBTORCH_PATH`, so you only need to pass it by hand in this manual fallback.

> **Common error:** If CMake cannot find Python, set `-DPython_EXECUTABLE=...` to your Python executable (the one inside your virtual environment). Example:
> `-DPython_EXECUTABLE="C:\Users\YourUsername\RLBot\GigaLearnCPP-Leak\venv\Scripts\python.exe"`

After running, you should see `-- Configuring done` and `-- Generating done`.

> **VERY IMPORTANT:** If you ever **add a new file** to GigaLearn (such as a state setter), you **must reconfigure CMake again**. If you are just **editing** an existing file, you only have to rebuild it.

**Step 3: Build the executable**

```
cmake --build . --config Release --target GigaLearnBot
```

This compiles all sources and produces `GigaLearnBot.exe` inside `build\RelWithDebInfo\`. The build can take **3–10 minutes** depending on your CPU.

> **Note:** Warnings about `C4251` are benign and can be ignored.

## 9. Run the Bot

### Training mode (no changes)

The easiest way is to use the scripts: `runners/linux/run.sh` (Linux) or `runners/windows/run.bat` (Windows).

For more control, run the executable directly. Enter the build output folder, which depending on your CMake version is `build`, `Release`, or `RelWithDebInfo`, and run the executable:

```
cd build        # or: cd Release / cd RelWithDebInfo
GigaLearnBot.exe
```

### Render mode (to watch the bot)

Rendering requires an extra step. You must open **RocketSimVis** first:

1. Start RocketSimVis:

   ```
   cd RocketSimVis
   run.bat
   ```

2. When the RocketSimVis page opens, run the render script (`runners/windows/run_render.bat` or `runners/linux/run_render.sh`), or do it manually by running the executable with the `--render` flag:

   ```
   GigaLearnBot.exe --render
   ```

## 10. Model the Robot (`src/novamain.cpp`)

To change the robot's behavior, **`src/novamain.cpp` is one of the only files you need to touch, nothing else.**

From the whole repository, the only things you need to know about are these three folders:

```cpp
RLGymCPP/Rewards/
RLGymCPP/TerminalConditions/
RLGymCPP/StateSetters/
```

- All **rewards** live in `Rewards/` (together with their wrappers).
- All **terminal conditions** live in `TerminalConditions/`.
- All **state setters** live in `StateSetters/`.

`novamain.cpp` is organized in a few parts:

### Rewards weight vector

At the top there is a vector of reward weights. Each line is one reward with its arguments, followed by its weight. Each reward has a comment explaining what it does, for anyone who wants to use it.

```cpp
{ new ZeroSumReward(new GoalReward(-1, 1.2f, 2.5f), 0.0f, 1.0f),        40.0f }, // GoalReward(concedeScale, speedScale, heightScale): golo escalado pela velocidade e ALTURA de entrada
```

### Terminal conditions vector

Next there is a vector of terminal conditions, defining which conditions you want the simulation to use.

```cpp
new GoalScoreCondition(),
new TimeoutCondition(40.f),
```

### State setter

Then there is a state setter that defines which states are used, with their arguments and a weight per state. The environment **normalizes these weights into percentages**, so the weights can be greater than 1. The naming follows the same convention as the rewards.

### Main (robot values)

Finally, the `main` holds the robot's values. The most important one is the **device**:

- **`GPU_CUDA`** - for GPU training, on **both AMD and NVIDIA** (super important to set this if you have a GPU).
- **`CPU`** - for any plain CPU.
- **`AUTO`** - if you are not sure about your installation.

```cpp
LearnerDeviceType::GPU_CUDA;
```

The remaining parameters are parts of the network, updates and general architecture.

## 11. Start Training & Using Our Model

Once `novamain.cpp` is set up, **start training** by building and running the bot (see sections 8 and 9).

### Using our pretrained model

We will later share a folder with our latest model, either on **Moodle** or via an **email link**. To use it:

1. Paste the folder inside your **`build`** (or **`Release`**) folder.
2. If you have already produced checkpoints, **delete that existing checkpoints folder** and paste ours in its place so training starts from our model.
