# GAME ENGINE — PLANO INICIAL

## Objetivo

Criar uma game engine moderna, extremamente leve, rápida, modular e visualmente avançada.

A meta não é ser leve por possuir poucos recursos. A meta é entregar **qualidade visual de nível AAA com o menor custo possível de RAM, CPU, GPU, armazenamento e tempo de inicialização**.

A engine deve conseguir escalar desde hardware modesto até máquinas high-end sem obrigar todos os projetos a carregar sistemas caros.

A prioridade principal da engine será:

* baixo uso de RAM;
* baixo uso de CPU em idle;
* inicialização extremamente rápida;
* executáveis pequenos;
* arquitetura moderna;
* renderer moderno e escalável;
* alta qualidade visual;
* suporte a hardware moderno;
* possibilidade de rodar em PCs fracos;
* editor leve e responsivo;
* sistemas completamente opcionais;
* custo proporcional aos recursos realmente utilizados;
* controle explícito de memória;
* alta performance;
* fácil utilização para desenvolvedores de jogos;
* ferramentas pesadas fora do runtime sempre que possível.

As duas filosofias centrais serão:

> You only pay for what you use.

> Maximum visual quality per unit of hardware.

Em português:

> Você só paga pelo que usa.

> Máxima qualidade visual pelo menor custo possível de hardware.

Se um jogo não usa física, Lua, networking, ray tracing, volumetria, GI dinâmica, partículas ou outro sistema opcional, esse código e seus dados idealmente não devem existir no executável final. Quando a remoção completa não for tecnicamente possível, o custo residual deve ser pequeno, explícito e mensurável.

A qualidade gráfica deve ser **escalável**, não obrigatoriamente pesada.

## Escopo e critério de sucesso

Este documento descreve uma visão de longo prazo. Ele não é um compromisso de prazo de lançamento nem exige que todos os sistemas sejam implementados ao mesmo tempo.

O foco inicial de validação será uma engine para jogos 3D em desktop, começando por Windows e Linux, com renderização em tempo real e caminhos gráficos escaláveis. Outras plataformas, gêneros e recursos podem ser adicionados conforme a arquitetura e os testes justificarem.

Nesta visão, “melhor possível” significa maximizar a combinação de:

* qualidade visual;
* previsibilidade de performance;
* eficiência de RAM, VRAM, CPU e GPU;
* estabilidade e correção;
* capacidade de evolução;
* simplicidade para quem utiliza a engine.

Esses objetivos podem entrar em conflito. Cada decisão importante deverá explicitar o trade-off e ser validada por protótipos, testes e medições. A visão pode permanecer ambiciosa mesmo quando a implementação de uma parte for revisada.

# LINGUAGENS

## C++

C++ será a principal linguagem da engine e do runtime.

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
* RHI
* Render Graph
* Scene System
* ECS
* Asset Runtime
* Animation Runtime
* Audio Runtime
* Physics Integration
* Networking Runtime futuramente
* Game Runtime
* Plugins internos
* Performance-critical code

Regra:

> Tudo que roda dentro do jogo deve preferencialmente ser escrito em C++.

Objetivos:

* nenhum garbage collector obrigatório no core;
* nenhuma runtime pesada obrigatória;
* ownership e lifetime explícitos;
* poucas alocações;
* executáveis pequenos;
* acesso direto às APIs do sistema;
* desempenho previsível;
* fácil integração com C e APIs nativas;
* uso de design orientado a dados quando isso melhorar o sistema.

A versão do padrão C++, os compiladores suportados e a configuração de toolchain deverão ser **fixados/pinados** no repositório ou na configuração de build. Atualizações devem ser deliberadas e testadas, nunca automáticas.

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

Internamente essas funções podem ser implementadas em C++.

A C ABI poderá servir como uma fronteira de integração futura com:

* C
* C++
* Zig (binding futuro, não runtime oficial)
* Rust
* C#
* Lua
* Python
* outras linguagens

A C ABI, sozinha, não cria bindings automáticos. Cada linguagem que utilizar a engine precisará de um binding, gerador ou adaptador próprio. A ABI pública deverá permanecer pequena, estável e baseada em tipos simples.

## Regras da ABI pública

* utilizar handles opacos em vez de expor estruturas internas;
* definir claramente quem aloca, quem possui e quem libera cada recurso;
* versionar a ABI e validar compatibilidade;
* representar falhas com códigos de erro e contratos documentados;
* evitar containers, strings, classes, templates, exceções e layouts internos de C++ na fronteira pública;
* definir convenções para callbacks, threads e shutdown.

Arquitetura:

```text
              ENGINE
                 │
                 │
              C ABI
                 │
       ┌─────────┼─────────┐
       │         │         │
      C++        C        Rust
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
├── ShaderTooling/
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

Rust Asset / Tool Pipeline

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

C++
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

C++
+
Engine
+
Lua
```

Somente nesse caso Lua entra no executável.

---

# SLANG

Slang será a linguagem padrão para shaders da engine.

Responsável por:

* vertex shaders;
* fragment/pixel shaders;
* compute shaders;
* ray tracing shaders futuramente;
* bibliotecas compartilhadas de código GPU;
* especialização e geração de variantes de shader.

Objetivo principal:

```text
                 Slang
                   │
        ┌──────────┼──────────┐
        │          │          │
        ↓          ↓          ↓
      SPIR-V      DXIL       Metal
        │          │          │
        ↓          ↓          ↓
     Vulkan       D3D12      Metal
```

A engine deve evitar manter versões completamente separadas do mesmo shader para Vulkan, Direct3D 12 e Metal sempre que Slang puder fornecer uma base compartilhada.

## Regra de compilação de shaders

Shaders devem ser compilados **offline** sempre que possível.

Pipeline preferido:

```text
Shader .slang
    ↓
Shader Compiler / Toolchain
    ↓
SPIR-V / DXIL / saída Metal
    ↓
Shader Cache / Pipeline Cache
    ↓
Runtime
```

O compilador Slang e ferramentas de compilação de shader **não devem ser dependências obrigatórias do jogo final**.

Em builds de release, o runtime deve consumir shaders já compilados e preparados para a plataforma alvo.

Hot reload e compilação em desenvolvimento podem existir no editor, mas devem permanecer fora do runtime final sempre que possível.

# ARQUITETURA GERAL

```text
                              GAME ENGINE
                                  │
                                  │
                               C++ CORE
                                  │
          ┌───────────────────────┼───────────────────────┐
          │                       │                       │
        CORE                   RENDERER                 SCENE
          │                       │                       │
      Memory                     RHI                     ECS
      Filesystem              Render Graph             Transform
      Jobs                    GPU Culling              Hierarchy
      Threading               GPU Driven              Components
      Logging                 PBR                     Assets
      Platform                Lighting                Animation
      Input                   Shadows                 Physics
      Timing                  Post FX                 Audio
          │                       │                       │
          └───────────────────────┼───────────────────────┘
                                  │
                                C ABI
                                  │
                       ┌──────────┼──────────┐
                       │          │          │
                      C++         C        Rust
                    Plugins    Plugins    Plugins


                         GPU SHADER LAYER
                                  │
                                Slang
                                  │
                  ┌───────────────┼───────────────┐
                  │               │               │
                SPIR-V           DXIL          Metal target
                  │               │               │
                Vulkan           D3D12           Metal


                        TOOLCHAIN / EDITOR
                                  │
                            C++ + Rust
                                  │
             ┌────────────────────┼────────────────────┐
             │                    │                    │
           Assets               Shaders              Build
             │                    │                    │
         Compiler           Slang Compiler          Packager
         Importer           Cache/Variants          Exporter


                             GAMEPLAY
                                  │
                           C++ or Lua
                                  │
                          Lua OPTIONAL
```

Separação fundamental:

```text
Editor / Toolchain ≠ Runtime final
```

O jogo exportado deve conter somente os módulos e dados necessários para sua execução.

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
│   ├── input/
│   ├── filesystem/
│   │
│   ├── renderer/
│   │   ├── rhi/
│   │   ├── vulkan/
│   │   ├── d3d12/
│   │   ├── metal/
│   │   ├── render_graph/
│   │   ├── gpu_driven/
│   │   ├── visibility/
│   │   ├── shaders/
│   │   ├── materials/
│   │   ├── lighting/
│   │   ├── shadows/
│   │   ├── pbr/
│   │   ├── postfx/
│   │   ├── upscaling/
│   │   ├── volumetrics/
│   │   ├── raytracing/
│   │   └── gpu/
│   │
│   ├── scene/
│   │   ├── entity/
│   │   ├── components/
│   │   ├── transform/
│   │   └── hierarchy/
│   │
│   ├── ecs/
│   ├── assets/
│   ├── animation/
│   ├── audio/
│   ├── physics/
│   ├── scripting/
│   │   └── lua/
│   ├── networking/
│   └── runtime/
│
├── api/
│   ├── c/
│   └── cpp/
│
├── shaders/
│   ├── common/
│   ├── materials/
│   ├── lighting/
│   ├── shadows/
│   ├── postfx/
│   ├── compute/
│   └── raytracing/
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
│   ├── shader_cache/
│   ├── texture_compiler/
│   ├── model_importer/
│   ├── animation_compiler/
│   ├── asset_database/
│   ├── packager/
│   └── build_tools/
│
├── plugins/
├── examples/
├── benchmarks/
├── tests/
├── third_party/
├── docs/
├── CMakeLists.txt
└── README.md
```

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

O renderer é um dos sistemas centrais da engine.

A meta é alcançar **qualidade visual moderna/AAA sem transformar essa qualidade em custo obrigatório para todo projeto**.

O renderer deve ser completamente desacoplado da API gráfica.

Não espalhar Vulkan, Direct3D 12 ou Metal pelo código da engine.

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
Render Graph
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
 ┌────┼─────┐
 │    │     │
 ↓    ↓     ↓
VK   DX12  Metal
```

A qualidade visual deve ser escalável por níveis e módulos.

Exemplo conceitual:

```text
BASE
├── PBR
├── HDR
├── Image Based Lighting
├── Shadow Maps
├── Forward+ / Clustered Lighting
├── GPU Culling
└── Instancing

ADVANCED
├── TAA
├── Upscaling
├── SSAO
├── SSR
├── Contact Shadows
├── Volumetric Fog
└── Improved Shadowing

ULTRA / OPTIONAL
├── Real-time Global Illumination
├── Ray Traced Reflections
├── Ray Traced Shadows
├── Virtualized Geometry
├── High-end Volumetrics
└── Advanced Reconstruction/Upscaling
```

Os sistemas avançados não devem contaminar o custo do renderer básico quando estiverem desativados.

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

A RHI deverá representar conceitos realmente compartilhados entre os backends. Durante os primeiros protótipos, ela deve ser pequena e evoluir a partir das necessidades observadas no backend Vulkan. Não é necessário congelar uma abstração completa antes de existir um renderer funcional.

Detalhes específicos de cada API, capacidades opcionais e limitações de hardware devem permanecer isolados no backend ou ser expostos por capacidades explícitas da RHI.

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

Shaders:

```text
Slang
  │
  ├── SPIR-V → Vulkan
  ├── DXIL   → Direct3D 12
  └── Metal target → Metal
```

O backend gráfico e o formato final do shader devem poder mudar sem obrigar os sistemas de cena, materiais e gameplay a conhecer detalhes específicos da API.

# TECNOLOGIAS MODERNAS DE RENDERIZAÇÃO

A engine poderá evoluir para suportar:

* Vulkan;
* Direct3D 12;
* Metal;
* Slang para shaders;
* GPU Driven Rendering;
* Bindless Resources;
* Indirect Drawing;
* Multi-Draw Indirect;
* Compute Shaders;
* Async Compute;
* Render Graph;
* GPU Frustum Culling;
* GPU Occlusion Culling;
* Hi-Z / hierarchical depth quando útil;
* PBR;
* Image Based Lighting;
* Forward+;
* Clustered Lighting;
* HDR;
* Tone Mapping;
* Temporal Anti-Aliasing;
* temporal reconstruction;
* upscaling;
* Instancing;
* GPU particles;
* Screen Space Reflections;
* Ambient Occlusion;
* Volumetric Fog;
* Contact Shadows;
* real-time GI futuramente;
* ray tracing opcional;
* virtualized geometry futuramente;
* texture streaming;
* mesh streaming;
* asynchronous asset streaming.

Essas tecnologias devem ser adicionadas progressivamente.

Tecnologia moderna não significa colocar tudo dentro da engine.

Cada recurso deve justificar seu custo em:

* CPU;
* GPU;
* VRAM;
* RAM;
* tamanho do executável;
* complexidade;
* tempo de build;
* manutenção.

## Princípio visual

A pergunta principal não deve ser:

> Como colocar o maior número de efeitos?

A pergunta deve ser:

> Qual técnica entrega a maior melhoria visual pelo menor custo possível?

A engine deve buscar alta **qualidade visual por unidade de hardware**.

## Escalabilidade gráfica

Um mesmo projeto deve poder utilizar caminhos diferentes dependendo do hardware.

Exemplo:

```text
PC FRACO
├── Baked Lighting
├── Probes
├── PBR
├── Shadow Maps
└── efeitos leves

PC MÉDIO
├── PBR
├── Forward+ / Clustered
├── SSAO
├── SSR
├── TAA
└── Volumetrics moderada

PC HIGH-END
├── Real-time GI
├── Ray Tracing opcional
├── Advanced Reflections
├── High-end Volumetrics
├── Virtualized Geometry
└── Advanced Upscaling/Reconstruction
```

A engine deve permanecer a mesma. O custo muda conforme os recursos utilizados.

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
SLANG
etc.
```

Não devem necessariamente ser utilizados diretamente durante gameplay.

Pipeline:

```text
Arquivo original
      ↓
Asset Importer / Shader Compiler
      ↓
Asset Compiler
      ↓
Otimização / Compressão / Conversão
      ↓
Asset otimizado / Shader compilado
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

```text
lighting.slang
    ↓
SPIR-V / DXIL / Metal target
    ↓
shader cache
```

O runtime deve fazer o mínimo possível.

Importação, conversão, compressão, compilação e geração de variantes devem ocorrer offline sempre que isso melhorar o runtime.

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
* compilador Slang;
* shader compiler;
* asset compiler;
* debug tools que não forem necessários.

O editor deve continuar visualmente moderno e agradável, mas beleza da interface não pode justificar desperdício sistemático de memória ou CPU.

A viewport do editor deve utilizar o mesmo renderer da engine para que a prévia represente corretamente o resultado final.

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

Esses números são metas iniciais e podem mudar conforme testes reais. Toda medição deverá informar plataforma, hardware, configuração de build, cena, recursos carregados e critério utilizado. Uma meta só é útil quando puder ser reproduzida.

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

Esses valores devem ser tratados como targets de engenharia e não promessas. O custo de um projeto vazio deve ser separado do custo dos módulos e assets que o projeto escolher carregar.

---

# BUILD MODULAR

O sistema de build e empacotamento permitirá escolher módulos.

Exemplo:

```text
[✓] Renderer 3D
[✓] PBR
[✓] Audio
[✓] Physics
[✓] Animation
[✓] TAA

[ ] Renderer 2D
[ ] Lua
[ ] Networking
[ ] Video
[ ] Navigation
[ ] Volumetrics
[ ] Real-time GI
[ ] Ray Tracing
[ ] Virtualized Geometry
```

Build final:

```text
Game.exe

Core
Renderer3D
PBR
Audio
Physics
Animation
TAA
```

Não incluído:

```text
Lua
Networking
2D
Video
Navigation
Volumetrics
Real-time GI
Ray Tracing
Virtualized Geometry
Slang compiler
Editor
Asset compiler
```

O objetivo não é apenas desativar recursos em tempo de execução. Módulos opcionais devem ser removidos do build, link ou pacote final sempre que tecnicamente possível. Ainda assim, componentes compartilhados podem possuir algum custo residual; esse custo deve ser conhecido e medido.

# PRINCÍPIOS

```text
Don't Pay For What You Don't Use
```

Ou:

```text
Você só paga pelo que usa.
```

Segundo princípio:

```text
Maximum Visual Quality Per Unit of Hardware
```

Ou:

```text
Máxima qualidade visual pelo menor custo possível.
```

Essas serão regras centrais da arquitetura.

A engine não deve escolher entre ser bonita e ser leve.

Ela deve buscar as duas coisas através de arquitetura, escalabilidade e modularidade.

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

# ROADMAP DE VALIDAÇÃO E EVOLUÇÃO

O roadmap detalhado de tarefas, dependências e critérios de conclusão está em [`ROADMAP.md`](ROADMAP.md). Esta seção mantém apenas a visão resumida das etapas.

As versões abaixo são marcos técnicos, não prazos de lançamento. A engine pode levar o tempo necessário para atingir qualidade, e cada marco poderá ser dividido em vários protótipos internos.

Uma etapa só deve ser considerada concluída quando seu comportamento, seus custos e seus caminhos de erro forem compreendidos por meio de testes e medições. Decisões que ainda não foram validadas não devem ser tratadas como contratos permanentes da arquitetura.

## v0.0.1

Somente C++ no runtime + Slang para o primeiro shader.

```text
Core
Memory
Platform
Window
Input
Renderer
RHI inicial
Vulkan
Shader pipeline mínimo
```

Objetivo:

```text
Abrir janela
Inicializar GPU
Compilar/preparar shader de desenvolvimento
Carregar shader compilado
Limpar tela
Renderizar triângulo
Receber input
Fechar corretamente
```

A primeira versão já deve estabelecer a separação entre código CPU/runtime e código GPU/shader.

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
Material básico
PBR inicial
```

Objetivo:

```text
Abrir engine
Carregar modelo
Criar câmera
Aplicar material
Iluminar cena básica
Renderizar modelo
```

A prioridade visual nesta fase é obter uma base PBR correta antes de efeitos complexos.

# v0.0.3

O ECS entra nesta fase como uma decisão a ser validada pelo uso real e por benchmarks. O layout de dados, o modelo de queries e a estratégia de armazenamento podem ser revisados antes de serem considerados estáveis.

Adicionar:

```text
ECS
Materials
Slang shader library
Shader variants/cache
Lighting
IBL
Shadows
HDR
Tone Mapping
Asset System
C ABI
```

Objetivo:

Criar a primeira base visual realmente sólida da engine sem introduzir recursos AAA de alto custo antes da infraestrutura necessária.

# v0.0.4

Adicionar ferramentas Rust:

```text
Asset Compiler
Shader Tooling
Texture Compiler
Model Importer
Animation Compiler inicial
Packager inicial
```

O pipeline deve gerar dados prontos para consumo eficiente pelo runtime.

# v0.0.5

Adicionar:

```text
Editor básico

Viewport
Hierarchy
Inspector
Asset Browser
Console
Profiler
Material inspection
Shader hot reload em desenvolvimento
```

O compilador e ferramentas de shader continuam componentes de desenvolvimento/editor, não do jogo final.

# v0.1

Este é o objetivo de uma primeira engine utilizável, não uma data. Os recursos podem ser lançados e estabilizados em marcos independentes.

Adicionar:

```text
Lua opcional
Physics
Audio
Animation
Project System
Build / Export
Forward+ ou Clustered Lighting
TAA
SSAO
primeiros efeitos de pós-processamento
```

Objetivo:

```text
Abrir editor
Criar projeto
Importar modelo
Arrastar para cena
Criar luz
Criar câmera
Criar material PBR
Adicionar script
Pressionar Play
Exportar jogo
```

Quando isso funcionar corretamente, a engine já pode ser considerada funcional.

Recursos como GI dinâmica, ray tracing, virtualized geometry e volumetria avançada ficam para versões posteriores, depois de profiling e estabilidade da base.

# DECISÕES ABERTAS E EXPERIMENTOS

Para preservar a qualidade da arquitetura, algumas decisões devem ser escolhidas com base em protótipos e medições, e não apenas por preferência:

* modelo de ECS e layout das queries;
* estratégia de handles, ownership e lifetime de recursos;
* limites e capacidades da RHI;
* caminho principal de iluminação, como Forward+, Clustered ou Deferred;
* combinação de allocators para dados permanentes, temporários e de streaming;
* formato de assets, cache e política de streaming;
* integração e custo real do Lua opcional;
* uso de compute, async compute, GPU culling e outras técnicas avançadas.

Manter essas decisões abertas durante a fase de pesquisa não enfraquece a visão. Evita congelar abstrações antes de conhecer suas exigências reais.

# FILOSOFIA FINAL

A engine deve seguir os seguintes princípios:

```text
LIGHTWEIGHT

VISUALLY ADVANCED

FAST

MODULAR

DATA ORIENTED

NO GC IN CORE

LOW MEMORY

LOW STARTUP TIME

LOW BINARY SIZE

COST PROPORTIONAL TO USED FEATURES

MODERN GRAPHICS

SCALABLE GRAPHICS

MULTITHREADED

GPU DRIVEN

EXPLICIT MEMORY

OFFLINE ASSET PROCESSING

OFFLINE SHADER COMPILATION

CROSS PLATFORM

SIMPLE FOR DEVELOPERS
```

A qualidade visual não deve depender de tornar todo projeto pesado.

Recursos caros devem possuir caminhos alternativos e/ou serem opcionais.

# STACK OFICIAL

```text
Runtime / Core / Renderer:
C++

Public ABI / Plugin Boundary:
C ABI

C++ SDK / Public API:
C++

Offline Tools:
Rust

Gameplay Native:
C++

Gameplay Scripting:
Lua opcional

GPU Shaders:
Slang

Primary Graphics API:
Vulkan

Future Graphics APIs:
Direct3D 12
Metal

Build System:
CMake + presets de toolchain

Platforms:
Windows
Linux
macOS futuramente
```

## Responsabilidade de cada linguagem

```text
C++
└── tudo que precisa ser extremamente leve, previsível e próximo do runtime

C ABI
└── fronteira estável para plugins e interoperabilidade

Rust
└── ferramentas offline, compiladores, importadores e pipeline pesado

Lua
└── scripting de gameplay opcional

Slang
└── código executado na GPU e geração multiplataforma de shaders
```

Nenhuma linguagem deve ser adicionada ao projeto apenas por preferência. Ela precisa resolver um problema concreto.

# REGRA MAIS IMPORTANTE

Antes de adicionar qualquer recurso perguntar:

```text
1. Isso precisa existir?

2. Quanto de RAM isso adiciona?

3. Quanto de CPU isso utiliza?

4. Quanto de GPU isso utiliza?

5. Quanto de VRAM isso utiliza?

6. Quanto aumenta o executável?

7. Quanto aumenta o startup?

8. Pode ser opcional?

9. Pode ser removido completamente do build?

10. Pode ser feito offline?

11. Existe um caminho mais barato para hardware fraco?

12. Quanto melhora a qualidade visual ou a experiência?

13. Pode ser feito de forma mais simples?
```

Se a resposta não justificar o custo, o recurso deve ser reconsiderado.

Para recursos gráficos, avaliar sempre a relação:

```text
melhoria visual
───────────────
custo total
```

A engine deve buscar maximizar essa relação.

# VISÃO

Criar uma game engine moderna capaz de produzir jogos visualmente impressionantes, inclusive com qualidade comparável a engines AAA, sem seguir a tendência de transformar todo projeto e todo editor em software pesado.

O objetivo será permitir algo como:

```text
Engine abre rapidamente.

Projeto abre rapidamente.

Pouca RAM em idle.

Runtime pequeno.

Jogo começa rapidamente.

Somente sistemas necessários são compilados ou empacotados.

Shaders são preparados offline.

Hardware fraco consegue utilizar o editor e caminhos gráficos mais leves.

Hardware médio consegue atingir ótima qualidade visual com técnicas eficientes.

Hardware poderoso consegue ativar tecnologias gráficas avançadas.
```

Visão de escalabilidade:

```text
LOW-END
   ↓
boa qualidade visual
baixo custo

MID-RANGE
   ↓
qualidade visual excelente
técnicas modernas eficientes

HIGH-END
   ↓
qualidade AAA
GI / RT / volumetria / reconstrução avançada opcionais
```

A engine não deve ser leve porque possui poucos recursos.

Ela deve ser leve porque sua arquitetura foi projetada para eficiência.

A engine não deve ser bonita porque desperdiça hardware.

Ela deve ser bonita porque o renderer utiliza técnicas modernas de forma inteligente.

Objetivo final:

> A beleza de uma engine AAA com uma arquitetura construída desde o início para ser leve, escalável e modular.
