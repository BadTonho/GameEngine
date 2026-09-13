# GAME ENGINE — PLANO INICIAL

## Objetivo

Criar uma game engine moderna, extremamente leve, rápida e modular.

A prioridade principal da engine será:

* baixo uso de RAM;
* baixo uso de CPU em idle;
* inicialização extremamente rápida;
* executáveis pequenos;
* arquitetura moderna;
* suporte a hardware moderno;
* possibilidade de rodar em PCs fracos;
* editor leve;
* sistemas completamente opcionais;
* nenhum custo para recursos que não forem utilizados;
* controle explícito de memória;
* alta performance;
* fácil utilização para desenvolvedores de jogos.

A filosofia principal será:

> Zero custo quando um recurso não é utilizado.

Se um jogo não usa física, Lua, networking, partículas ou outro sistema, esse código idealmente não deve nem existir no executável final.

---

# LINGUAGENS

## Zig

Zig será a principal linguagem da engine.

Responsável por:

* Core
* Memory Management
* Platform Layer
* Window
* Input
* Filesystem
* Threading
* Job System
* Renderer
* Render Graph
* Scene System
* ECS
* Asset Runtime
* Animation
* Audio Runtime
* Physics Integration
* Networking Runtime futuramente
* Game Runtime
* Plugins internos
* Performance-critical code

Regra:

> Tudo que roda dentro do jogo deve preferencialmente ser escrito em Zig.

Objetivo:

* nenhuma garbage collection;
* nenhuma runtime pesada;
* controle explícito de memória;
* poucas alocações;
* executáveis pequenos;
* acesso direto às APIs do sistema;
* desempenho previsível.

---

# C

C será utilizado principalmente como ABI pública da engine.

Não será necessariamente usado para implementar grandes subsistemas.

Objetivo:

Criar uma interface estável entre a engine e outras linguagens.

Exemplo:

```c
typedef unsigned long long EngineEntity;

EngineEntity engine_entity_create(void);

void engine_entity_destroy(
    EngineEntity entity
);

void engine_transform_set_position(
    EngineEntity entity,
    float x,
    float y,
    float z
);
```

Internamente essas funções podem ser implementadas em Zig.

A C ABI permitirá integração futura com:

* C
* C++
* Zig
* Rust
* C#
* Lua
* Python
* outras linguagens

Arquitetura:

```text
              ENGINE
                 │
                 │
              C ABI
                 │
       ┌─────────┼─────────┐
       │         │         │
      Zig        C        Rust
       │         │         │
     Plugins   Plugins   Plugins
```

---

# RUST

Rust será usado principalmente para ferramentas externas.

Essas ferramentas não precisam ser incluídas no jogo final.

Possíveis ferramentas:

```text
Tools/
├── AssetCompiler/
├── TextureCompiler/
├── ShaderCompiler/
├── ModelImporter/
├── AnimationCompiler/
├── AssetDatabase/
├── Packager/
├── BuildServer/
└── ProjectTools/
```

Exemplo de pipeline:

```text
player.glb
player.png
player_normal.png
walk.fbx

        ↓

Rust Asset Pipeline

        ↓

Importação
Otimização
Compressão
Conversão
LOD
Metadata
Processamento

        ↓

player.mesh
player.texture
player_normal.texture
walk.animation
```

O runtime não deve precisar interpretar formatos pesados durante o jogo.

O objetivo é carregar arquivos já preparados para a engine.

Isso deve melhorar:

* startup;
* loading;
* RAM;
* CPU;
* tamanho do runtime;
* simplicidade da engine.

---

# LUA

Lua será opcional.

Será utilizada principalmente para gameplay e scripting.

Exemplo:

```lua
function start()
    player = Engine.findEntity("Player")
end

function update(dt)
    if Input.keyDown("W") then
        player:move(0, 0, 5 * dt)
    end
end
```

Lua nunca será obrigatória.

Exemplo:

```text
Projeto A

Zig
+
Engine
```

Resultado:

```text
Lua runtime:
0 bytes
```

Outro projeto:

```text
Projeto B

Zig
+
Engine
+
Lua
```

Somente nesse caso Lua entra no executável.

---

# ARQUITETURA GERAL

```text
                       GAME ENGINE
                           │
                           │
                        ZIG CORE
                           │
       ┌───────────────────┼───────────────────┐
       │                   │                   │
     CORE               RENDERER             SCENE
       │                   │                   │
       │                   │                   │
   Memory                Vulkan               ECS
   Filesystem            D3D12                Transform
   Jobs                  Metal                Hierarchy
   Threading             Render Graph         Components
   Logging               GPU Culling          Assets
   Platform              Compute              Animation
   Input                 PBR                  Physics
   Timing                Bindless             Audio
       │                   │                   │
       └───────────────────┼───────────────────┘
                           │
                         C ABI
                           │
                ┌──────────┼──────────┐
                │          │          │
               Zig         C        Rust
             Plugins    Plugins    Plugins


                    TOOLCHAIN / EDITOR
                           │
                     Zig + Rust
                           │
          ┌────────────────┼────────────────┐
          │                │                │
       Assets           Shaders          Build
          │                │                │
      Compiler          Compiler         Packager
      Importer          Cache            Exporter


                         GAMEPLAY
                            │
                           Lua
                        OPTIONAL
```

---

# ESTRUTURA DO PROJETO

```text
Engine/
│
├── engine/
│   │
│   ├── core/
│   │   ├── memory/
│   │   ├── allocator/
│   │   ├── logging/
│   │   ├── threading/
│   │   ├── jobs/
│   │   ├── timing/
│   │   └── containers/
│   │
│   ├── platform/
│   │   ├── windows/
│   │   ├── linux/
│   │   └── macos/
│   │
│   ├── window/
│   │
│   ├── input/
│   │
│   ├── filesystem/
│   │
│   ├── renderer/
│   │   ├── rhi/
│   │   ├── vulkan/
│   │   ├── d3d12/
│   │   ├── metal/
│   │   ├── render_graph/
│   │   ├── shaders/
│   │   ├── materials/
│   │   ├── lighting/
│   │   ├── pbr/
│   │   └── gpu/
│   │
│   ├── scene/
│   │   ├── entity/
│   │   ├── components/
│   │   ├── transform/
│   │   └── hierarchy/
│   │
│   ├── ecs/
│   │
│   ├── assets/
│   │
│   ├── animation/
│   │
│   ├── audio/
│   │
│   ├── physics/
│   │
│   ├── scripting/
│   │   └── lua/
│   │
│   ├── networking/
│   │
│   └── runtime/
│
├── api/
│   └── c/
│
├── editor/
│   ├── core/
│   ├── viewport/
│   ├── hierarchy/
│   ├── inspector/
│   ├── asset_browser/
│   ├── console/
│   ├── profiler/
│   └── project_manager/
│
├── tools/
│   ├── asset_compiler/
│   ├── shader_compiler/
│   ├── texture_compiler/
│   ├── model_importer/
│   ├── animation_compiler/
│   ├── asset_database/
│   ├── packager/
│   └── build_tools/
│
├── plugins/
│
├── examples/
│
├── benchmarks/
│
├── tests/
│
├── third_party/
│
├── docs/
│
├── build.zig
│
└── README.md
```

---

# MEMORY SYSTEM

A engine deve evitar malloc/free constante durante gameplay.

Objetivo:

```text
Alocações por frame:

0 ou próximo de 0
```

Estrutura inicial:

```text
Game Memory
│
├── Permanent Arena
│
├── Engine Arena
│
├── Scene Arena
│
├── Asset Arena
│
├── Frame Arena A
│
└── Frame Arena B
```

Frame Arena:

```text
Frame começa

████████████████████░░░░░░

alocações sequenciais

██████████████████████████

Frame termina

reset()

░░░░░░░░░░░░░░░░░░░░░░░░
```

Sem milhares de malloc/free durante cada frame.

---

# DATA-ORIENTED DESIGN

Evitar arquiteturas extremamente orientadas a objetos.

Evitar:

```text
GameObject
├── Transform
├── Physics
├── Renderer
├── Audio
├── Script
└── ...
```

Preferir dados organizados:

```text
Transforms

[T][T][T][T][T][T][T][T]

Velocities

[V][V][V][V][V][V]

Renderables

[R][R][R][R][R]

Lights

[L][L][L]

Physics Bodies

[P][P][P][P][P]
```

Objetivos:

* melhor CPU cache;
* melhor SIMD;
* processamento em batch;
* melhor multithreading;
* upload eficiente para GPU;
* menor overhead por entidade.

---

# ENTITY SYSTEM

Entidades devem preferencialmente ser IDs.

Exemplo:

```text
Entity

64 bits
```

Possível organização:

```text
Entity ID

┌────────────────┬────────────────┐
│ Generation     │ Index          │
└────────────────┴────────────────┘
```

Isso permite detectar handles inválidos e entidades destruídas.

---

# RENDERER

O renderer será completamente desacoplado da API gráfica.

Não espalhar Vulkan pelo código da engine.

Evitar:

```text
Scene
    ↓
vkCmdDraw()
```

Preferir:

```text
Scene
    ↓
Renderer API
    ↓
RHI
    ↓
Backend
```

Estrutura:

```text
Renderer API
      │
      ↓
Render Graph
      │
      ↓
RHI
      │
 ┌────┼────┐
 │    │    │
 ↓    ↓    ↓
VK   DX12 Metal
```

---

# RHI

RHI significa:

```text
Render Hardware Interface
```

Ela será responsável por abstrair APIs gráficas.

Possíveis objetos:

```text
GPUDevice
GPUBuffer
GPUTexture
GPUShader
GPUPipeline
GPUCommandList
GPUFence
GPUSemaphore
GPUSwapchain
```

A engine utiliza esses objetos.

O backend converte para:

```text
Vulkan
Direct3D 12
Metal
```

---

# BACKENDS GRÁFICOS

Prioridade:

```text
1. Vulkan
2. Direct3D 12
3. Metal
```

Inicialmente:

```text
Vulkan
```

Depois:

```text
Windows
├── Vulkan
└── Direct3D 12

Linux
└── Vulkan

macOS
└── Metal
```

---

# TECNOLOGIAS MODERNAS DE RENDERIZAÇÃO

A engine poderá evoluir para suportar:

* Vulkan;
* Direct3D 12;
* Metal;
* GPU Driven Rendering;
* Bindless Resources;
* Indirect Drawing;
* Compute Shaders;
* Async Compute;
* Render Graph;
* GPU Frustum Culling;
* GPU Occlusion Culling;
* PBR;
* Forward+;
* Clustered Lighting;
* HDR;
* Temporal Anti-Aliasing;
* Upscaling;
* Instancing;
* GPU particles;
* texture streaming;
* mesh streaming;
* asynchronous asset streaming.

Essas tecnologias devem ser adicionadas progressivamente.

Tecnologia moderna não significa colocar tudo dentro da engine.

Cada recurso deve justificar seu custo.

---

# PLATFORM LAYER

A engine deverá ter uma camada própria para sistema operacional.

```text
Platform
│
├── Windows
│   └── Win32
│
├── Linux
│   ├── Wayland
│   └── X11 opcional
│
└── macOS
    └── Cocoa
```

API da engine:

```text
window_create()
window_destroy()

window_poll_events()

mouse_position()
mouse_button()

keyboard_key()

filesystem_open()

thread_create()

timer_get()
```

O restante da engine não deve depender diretamente do sistema operacional.

---

# ASSET SYSTEM

Arquivos originais:

```text
PNG
JPG
TGA
GLTF
FBX
WAV
OGG
etc.
```

Não devem necessariamente ser utilizados diretamente durante gameplay.

Pipeline:

```text
Arquivo original

        ↓

Asset Importer

        ↓

Asset Compiler

        ↓

Asset otimizado

        ↓

Runtime
```

Exemplo:

```text
dragon.glb
     ↓
dragon.mesh
```

```text
dragon.png
     ↓
dragon.texture
```

O runtime deve fazer o mínimo possível.

---

# EDITOR

O editor deve ser separado completamente do runtime.

```text
Editor
   │
   ↓
Engine API
   │
   ↓
Engine Runtime
```

O jogo exportado não deve carregar:

* editor;
* inspector;
* asset browser;
* editor UI;
* project manager;
* importadores;
* shader compiler;
* debug tools que não forem necessários.

---

# META DE EDITOR

Projeto vazio:

```text
RAM:

ideal:
< 150 MB
```

Startup:

```text
ideal:
< 1 segundo
```

Esses números são metas iniciais e podem mudar conforme testes reais.

---

# META DE RUNTIME

Projeto vazio:

```text
RAM:

ideal:
< 20 MB
```

Startup:

```text
ideal:
< 100 ms
```

Executable base:

```text
ideal:
< 10 MB
```

Esses valores devem ser tratados como targets de engenharia e não promessas.

---

# BUILD MODULAR

O usuário poderá escolher módulos.

Exemplo:

```text
[✓] Renderer 3D
[✓] Audio
[✓] Physics
[✓] Animation

[ ] Renderer 2D
[ ] Lua
[ ] Networking
[ ] Video
[ ] Navigation
[ ] Ray Tracing
```

Build final:

```text
Game.exe

Core
Renderer3D
Audio
Physics
Animation
```

Não incluído:

```text
Lua
Networking
2D
Video
Navigation
Ray Tracing
```

Não apenas desativado.

Não compilado.

---

# PRINCÍPIO

```text
Don't Pay For What You Don't Use
```

Ou:

```text
Você só paga pelo que usa.
```

Essa será uma das regras centrais da arquitetura.

---

# PERFORMANCE BUDGET

Toda alteração importante deverá poder ser medida.

Benchmarks automáticos:

```text
Startup Time

RAM Idle

Executable Size

RAM / 1.000 Entities

RAM / 10.000 Entities

RAM / 100.000 Entities

CPU / 1.000 Entities

CPU / 10.000 Entities

CPU / 100.000 Entities

Frame Time

GPU Time

Asset Loading Time

Scene Loading Time

Draw Calls

Build Time
```

---

# PERFORMANCE REGRESSION

O sistema de CI deverá futuramente detectar regressões.

Exemplo:

```text
PERFORMANCE REGRESSION

Runtime Memory

before:
31.4 MB

after:
38.7 MB

difference:
+23.2%

STATUS:
FAILED
```

Outro:

```text
Startup

before:
214 ms

after:
221 ms

difference:
+3.2%

STATUS:
WARNING
```

---

# DEPENDÊNCIAS

Regra:

> Nenhuma dependência é sagrada.

Toda biblioteca deverá ser avaliada por:

```text
RAM
CPU
Binary Size
Startup
Dependencies
Build Time
Maintainability
Platform Support
```

Uma biblioteca não entra apenas porque facilita desenvolvimento.

---

# PRIMEIRA VERSÃO

Não começar fazendo tudo.

## v0.0.1

Somente Zig.

```text
Core
Memory
Platform
Window
Input
Renderer
```

Objetivo:

```text
Abrir janela

Inicializar GPU

Limpar tela

Renderizar triângulo

Receber input

Fechar corretamente
```

---

# v0.0.2

Adicionar:

```text
Entities
Scene
Transform
Camera
Mesh
Texture
Basic Asset Loading
```

Objetivo:

```text
Abrir engine

Carregar modelo

Criar câmera

Renderizar modelo
```

---

# v0.0.3

Adicionar:

```text
ECS
Materials
Shaders
Lighting
Asset System
C ABI
```

---

# v0.0.4

Adicionar ferramentas Rust:

```text
Asset Compiler
Shader Compiler
Texture Compiler
Model Importer
```

---

# v0.0.5

Adicionar:

```text
Editor básico

Viewport
Hierarchy
Inspector
Asset Browser
Console
```

---

# v0.1

Adicionar:

```text
Lua opcional
Physics
Audio
Animation
Project System
Build / Export
```

Objetivo:

```text
Abrir editor

Criar projeto

Importar modelo

Arrastar para cena

Criar luz

Criar câmera

Adicionar script

Pressionar Play

Exportar jogo
```

Quando isso funcionar corretamente, a engine já pode ser considerada funcional.

---

# FILOSOFIA FINAL

A engine deve seguir os seguintes princípios:

```text
LIGHTWEIGHT

FAST

MODULAR

DATA ORIENTED

NO GC IN CORE

LOW MEMORY

LOW STARTUP TIME

LOW BINARY SIZE

ZERO-COST OPTIONAL FEATURES

MODERN GRAPHICS

MULTITHREADED

GPU DRIVEN

EXPLICIT MEMORY

CROSS PLATFORM

SIMPLE FOR DEVELOPERS
```

---

# STACK OFICIAL

```text
Runtime:
Zig

Public ABI:
C

Offline Tools:
Rust

Gameplay Scripting:
Lua

Graphics:
Vulkan
Direct3D 12 futuramente
Metal futuramente

Platforms:
Windows
Linux
macOS futuramente
```

---

# REGRA MAIS IMPORTANTE

Antes de adicionar qualquer recurso perguntar:

```text
1. Isso precisa existir?

2. Quanto de RAM isso adiciona?

3. Quanto de CPU isso utiliza?

4. Quanto aumenta o executável?

5. Quanto aumenta o startup?

6. Pode ser opcional?

7. Pode ser removido completamente do build?

8. Pode ser feito de forma mais simples?
```

Se a resposta não justificar o custo, o recurso deve ser reconsiderado.

---

# VISÃO

Criar uma game engine moderna que tenha recursos suficientes para desenvolver jogos atuais, mas sem seguir a tendência de engines cada vez maiores e mais pesadas.

O objetivo será permitir algo como:

```text
Engine abre imediatamente.

Projeto abre imediatamente.

Pouca RAM em idle.

Runtime pequeno.

Jogo começa rapidamente.

Somente sistemas necessários são compilados.

Hardware fraco consegue utilizar o editor.

Hardware poderoso consegue aproveitar tecnologias modernas.
```

A engine não deve ser leve porque possui poucos recursos.

Ela deve ser leve porque sua arquitetura foi projetada para eficiência.
