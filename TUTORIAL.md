# Tutorial ROXBOX — Baixar, construir e rodar no PC e no Android

Este tutorial explica, passo a passo, como baixar o projeto ROXBOX (clone do
Roblox Studio + Player), compilar e executar **no PC (Windows)** e **no
Android**.

---

## 1. Baixar o projeto no PC

O projeto está em um repositório Git. Para clonar, você precisa do
**Git** instalado (https://git-scm.com).

### 1.1 Abra o terminal
- **Windows:** pressione `Win + R`, type `cmd` e dê Enter (ou use o PowerShell).
- **Linux/macOS:** abra o Terminal.

### 1.2 Clone o repositório
No terminal, vá para a pasta onde quer guardar o projeto e digite:

```bash
git clone https://github.com/paulodosjogos143-dotcom/Studio.git
cd Studio
```

Isso cria uma pasta `Studio` com todo o código do motor.

> Se não tiver o Git, também pode baixar o arquivo ZIP do repositório
> (botão **Code → Download ZIP** no GitHub), extrair e entrar na pasta.

### 1.3 Estrutura do projeto
Após o download, você terá:

```
Studio/
  engine/        # motor: mundo voxel, mesh, render, scripting
  studio/        # aplicativo do Studio (editor)
  player/        # aplicativo do Player (jogo)
  tests/         # testes (smoke_test)
  examples/      # scripts de exemplo
  packaging/     # scripts de build + instalador de 1 clique
  CMakeLists.txt # configuração de build unificada
  Makefile       # build alternativo (usado para o smoke_test)
```

---

## 2. Construir e rodar no PC (Windows)

### 2.1 Instalar as ferramentas (requisitos)
Precisa de:
1. **CMake ≥ 3.16** — https://cmake.org/download (marcar "Add CMake to system PATH").
2. **Compilador C++17** — instale o **Visual Studio Build Tools 2022**:
   https://visualstudio.microsoft.com/visual-cpp-build-tools/
   (ao instalar, marque a caixa **"Desktop development with C++"**).

### 2.2 Opção A — Instalador de 1 clique (mais fácil)
Dentro da pasta `Studio`:
- Localize o arquivo **`packaging\install_1click.bat`**.
- **Duplo-clique** nele.

Ele faz tudo automaticamente:
- Verifica se o CMake está instalado.
- Compila o **ROXBOX Studio** e o **ROXBOX Player**.
- Copia os executáveis para a pasta **`dist\`**.
- Se o Android SDK estiver configurado, também monta o `.apk`.

### 2.3 Opção B — Script de build manual
Abra o terminal dentro da pasta `Studio` e execute:

```cmd
packaging\build_pc.bat
```

### 2.4 Opção C — Compilar manualmente com CMake
```cmd
cmake -S . -B build_pc -DCMAKE_BUILD_TYPE=Release
cmake --build build_pc --config Release -j
```

### 2.5 Encontrar e rodar os executáveis
Após o build, os programas estarão em:

```
Studio\dist\ROXBOX_Studio.exe   (editor)
Studio\dist\ROXBOX_Player.exe   (jogo)
```

**Rodar:**
- **Studio (editor):** duplo-clique em `ROXBOX_Studio.exe`.
- **Player (jogo):** duplo-clique em `ROXBOX_Player.exe`.

**Opções de linha de comando do Player:**
```cmd
ROXBOX_Player.exe --auto          # qualidade automática (escalona)
ROXBOX_Player.exe --tier 0        # qualidade mínima (alvo 15 GFLOPS)
ROXBOX_Player.exe --mobile        # controles touch (se usar com emulador/touch)
ROXBOX_Player.exe mapa.roxmap     # carregar um mundo salvo
```

---

## 3. Construir e rodar no PC (Linux/macOS)

### 3.1 Instalar as ferramentas
```bash
# Debian/Ubuntu
sudo apt update
sudo apt install cmake g++ git libgl1-mesa-dev libx11-dev

# Arch
sudo pacman -S cmake gcc git mesa-dev xorg-server-devel

# macOS (com Homebrew)
brew install cmake
```

### 3.2 Construir
```bash
cd Studio
bash packaging/build_pc.sh
```
Ou o instalador de 1 clique:
```bash
bash packaging/install_1click.sh
```

### 3.3 Rodar
```bash
./dist/ROXBOX_Studio
./dist/ROXBOX_Player --auto
```

---

## 4. Construir e instalar no Android (.apk)

Para o Android, o motor usa **OpenGL ES 2.0** — ideal para dispositivos
ultra-baixos (alvo de 60 FPS em ~15 GFLOPS).

### 4.1 Instalar o Android SDK + NDK
1. Baixe o **Android Studio**: https://developer.android.com/studio
2. Abra o Android Studio → `Configure` → `SDK Manager`.
3. Em **SDK Platforms**, marque uma versão Android (API 21+).
4. Em **SDK Tools**, marque:
   - **NDK**
   - **CMake**
   - **Android SDK Build-Tools**
5. Anote o caminho do SDK (ex.: `C:\Users\SeuUsuario\AppData\Local\Android\Sdk`).

### 4.2 Definir a variável ANDROID_HOME
**Windows:** abra `Edit the system environment variables` → `Environment
Variables` → em **System variables**, clique `New`:
- Nome: `ANDROID_HOME`
- Valor: caminho do SDK (ex.: `C:\Users\SeuUsuario\AppData\Local\Android\Sdk`)

Reabra o terminal após isso.

**Linux/macOS:** adicione no `~/.bashrc` (ou `~/.zshrc`):
```bash
export ANDROID_HOME=$HOME/Android/Sdk
```

### 4.3 Construir o APK
Dentro da pasta `Studio`:

```bash
# Linux/macOS/Windows com bash
bash packaging/build_android.sh
```

O script compila o motor com OpenGL ES 2.0 para os ABIs
(`armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`) e monta o APK em:

```
Studio\dist\ROXBOX_Player.apk
```

> O script tenta assinar o APK. Para uma assinatura oficial, coloque um
> keystore em `dist/release.keystore` (senha `roxbox`). Sem isso, o APK
> fica assinado com chave temporária — perfeito para testes.

### 4.4 Instalar no celular/emulador
Com o celular em **ferramentas de desenvolvedor** ativadas (ou um emulador
Android em execução), use o **ADB** (incluso no SDK):

```bash
adb install -r dist/ROXBOX_Player.apk
```

Pronto — o **ROXBOX Player** aparece na tela inicial do aparelho. Toque para
rodar o jogo.

> Para usar os **controles touch**, inicie com o modo mobile (o jogador já
> detecta toque automaticamente quando há input touch).

---

## 5. Verificar que tudo funciona (teste rápido)

Sem precisar de GPU, você pode rodar o teste automático para confirmar que o
motor está intacto:

```bash
# qualquer plataforma
make            # compila smoke_test + versões headless
./smoke_test    # deve imprimir: ALL TESTS PASSED
```

O `smoke_test` valida: geração do mundo, Greedy Meshing, salvar/carregar
mapas, a API Roblox completa, o scheduler `task.wait`, os eventos
`RenderStepped` e o escalonamento automático de qualidade.

---

## 6. Dica de resolução de problemas

| Problema | Solução |
|----------|---------|
| `cmake not found` | Instale o CMake e adicione à PATH. |
| Erro de compilador C++ | Instale o "Desktop development with C++" (Visual Studio) ou `g++`. |
| `ANDROID_HOME` não encontrado | Defina a variável e **reabra o terminal**. |
| APK não installa | Ative "Fontes desconhecidas" no celular ou use `adb install -r`. |
| Lag no PC potente | Use `--auto` para escalar a qualidade ao máximo automaticamente. |
| Frame baixo em hardware fraco | Use `--tier 0` (qualidade mínima, alvo 15 GFLOPS). |

---

## 7. Resumo rápido

```bash
# 1. Baixar
git clone https://github.com/paulodosjogos143-dotcom/Studio.git
cd Studio

# 2. PC — Windows (1 clique)
packaging\install_1click.bat
# → dist\ROXBOX_Studio.exe  e  dist\ROXBOX_Player.exe

# 2b. PC — Linux/macOS
bash packaging/install_1click.sh
# → dist/ROXBOX_Studio  e  dist/ROXBOX_Player

# 3. Android
# (com ANDROID_HOME definido)
bash packaging/build_android.sh
# → dist/ROXBOX_Player.apk
adb install -r dist/ROXBOX_Player.apk
```

Em poucos passos você tem o **Editor** e o **Jogo** rodando — no PC em
qualidade máxima e no Android com OpenGL ES 2.0 otimizado para 60 FPS em
hardware ultra-fraco.
