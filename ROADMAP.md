# Roadmap da Game Engine

Este documento transforma a visão de `Ideia.md` em uma sequência de trabalho. O objetivo é construir uma engine pública, moderna, eficiente e capaz de evoluir por muitos anos.

O roadmap não define prazos. Cada fase é um marco técnico. Uma fase pode durar o tempo necessário, ser dividida em protótipos ou voltar para revisão quando os testes mostrarem que uma decisão estava errada.

## Estado atual

O repositório está na fase de planejamento. Ainda não existe implementação da engine, do runtime, do editor ou das ferramentas.

## Regras de evolução

* validar decisões importantes com protótipos e medições;
* não congelar abstrações antes de conhecer suas necessidades reais;
* manter C++ como linguagem principal do runtime;
* manter a C ABI pequena e estável;
* manter Rust fora do runtime, limitado às ferramentas offline;
* manter Lua opcional;
* manter o editor e os importadores fora do jogo final;
* fazer cada módulo opcional possuir uma fronteira de build clara;
* não adicionar um recurso avançado sem definir seu custo, fallback e forma de teste;
* avançar apenas quando o marco atual estiver reproduzível e depurável.

## Fase 0 — Fundação pública do projeto

Objetivo: permitir que outra pessoa clone o repositório, entenda a proposta e consiga construir o projeto.

### Tarefas

- [ ] Definir a licença do projeto.
- [ ] Criar `README.md` com a visão, o estado atual e o modo de compilação.
- [ ] Criar `CONTRIBUTING.md`.
- [ ] Definir o `CODE_OF_CONDUCT.md` e as regras de contribuição.
- [ ] Definir a política de versões e compatibilidade.
- [ ] Escolher a versão do padrão C++.
- [ ] Definir compiladores e plataformas suportadas inicialmente.
- [ ] Criar `CMakeLists.txt` e `CMakePresets.json`.
- [ ] Separar configurações Debug, Release, Sanitizers e Profile.
- [ ] Configurar warnings elevados, formatador e análise estática.
- [ ] Criar CI para compilar e testar em Windows e Linux.
- [ ] Definir a política de dependências, licenças e código de terceiros.
- [ ] Definir a estrutura inicial de diretórios.

### Critério de conclusão

Um novo colaborador consegue clonar o projeto, configurar o compilador, construir um executável vazio e executar os testes sem depender de arquivos locais não documentados.

## Fase 1 — Core e plataforma mínima

Objetivo: criar um runtime pequeno, previsível e corretamente encerrado.

### Tarefas

- [ ] Definir tipos básicos, convenções de erro e resultados.
- [ ] Criar logging, assertions e diagnóstico de desenvolvimento.
- [ ] Definir ownership, lifetime e política de alocação.
- [ ] Criar allocators necessários somente depois de definir seus usos.
- [ ] Criar handles opacos e IDs com geração quando necessário.
- [ ] Criar tipos matemáticos mínimos para a primeira cena.
- [ ] Criar a camada de plataforma.
- [ ] Criar janela e ciclo de eventos para Windows.
- [ ] Adicionar suporte inicial a Linux sem espalhar condicionais pelo core.
- [ ] Criar input de teclado e mouse.
- [ ] Criar timing e loop principal.
- [ ] Definir o caminho de shutdown e destruição de recursos.
- [ ] Adicionar testes de memória, handles, timing e eventos.

### Critério de conclusão

O runtime abre uma janela, processa eventos, recebe input, executa o loop e fecha sem vazamentos ou recursos pendentes em Debug e Sanitizers.

## Fase 2 — Vulkan e RHI mínima

Objetivo: inicializar a GPU e estabelecer uma abstração pequena baseada em necessidades reais.

### Tarefas

- [ ] Criar instância Vulkan.
- [ ] Habilitar validation layers em builds de desenvolvimento.
- [ ] Criar surface, selecionar GPU e consultar capabilities.
- [ ] Criar device, queues e command pools.
- [ ] Criar swapchain e sincronização básica.
- [ ] Definir a RHI mínima para device, buffers, imagens, samplers e pipelines.
- [ ] Criar command lists, fences, semaphores e frames in flight.
- [ ] Definir destruição adiada de recursos GPU.
- [ ] Criar upload de buffers e texturas por staging.
- [ ] Adicionar debug names e integração com ferramentas de captura.
- [ ] Renderizar um triângulo.
- [ ] Validar o caminho de resize e perda de surface quando aplicável.

### Critério de conclusão

Uma aplicação C++ abre uma janela, inicializa Vulkan, renderiza um triângulo, reage a resize e encerra sem erros das validation layers.

## Fase 3 — Pipeline de shaders

Objetivo: tornar shaders reproduzíveis, multiplataforma e independentes do compilador no runtime final.

### Tarefas

- [ ] Fixar a versão da toolchain Slang usada pelo projeto.
- [ ] Definir convenções de módulos, entry points, stages e profiles.
- [ ] Compilar shaders Slang para SPIR-V no pipeline offline.
- [ ] Definir layout de recursos entre C++ e GPU.
- [ ] Gerar ou validar reflection de shaders.
- [ ] Criar identificação determinística de shader, target e variante.
- [ ] Criar shader cache e pipeline cache.
- [ ] Separar artefatos Debug e Release.
- [ ] Produzir diagnósticos úteis para erros de compilação.
- [ ] Definir como capabilities Vulkan selecionam variantes.
- [ ] Manter hot reload restrito ao desenvolvimento/editor.
- [ ] Garantir que o jogo exportado não dependa do compilador Slang.

### Critério de conclusão

Um shader Slang é compilado offline, validado, carregado pelo runtime e usado para renderizar sem exigir o compilador no executável final.

## Fase 4 — Primeira cena 3D

Objetivo: sair do triângulo e renderizar uma cena pequena com qualidade visual correta.

### Tarefas

- [ ] Criar câmera e transformações.
- [ ] Criar vertex/index buffers.
- [ ] Carregar uma mesh preparada.
- [ ] Carregar textura e sampler.
- [ ] Criar material básico.
- [ ] Criar depth buffer e depth testing.
- [ ] Criar iluminação básica.
- [ ] Implementar PBR inicial.
- [ ] Implementar HDR e tone mapping.
- [ ] Criar uma cena de referência pequena.
- [ ] Medir CPU, GPU, RAM, VRAM, draw calls e startup.
- [ ] Adicionar uma captura de referência para detectar regressões visuais.

### Critério de conclusão

Uma cena de referência carrega, exibe mesh texturizada, câmera, material e iluminação básica em Vulkan com resultados visuais e métricas reproduzíveis.

## Fase 5 — Formato de assets e ferramentas offline

Objetivo: fazer o runtime consumir dados preparados, compactos e validados.

### Tarefas

- [ ] Definir formatos binários versionados para meshes, texturas, materiais e cenas.
- [ ] Definir validação de tamanhos, offsets, contagens, versões e referências.
- [ ] Criar workspace Rust para ferramentas offline.
- [ ] Criar importador inicial de glTF.
- [ ] Criar compilador de texturas.
- [ ] Criar compilador de meshes e dados de vértices.
- [ ] Criar compilador de materiais e dependências.
- [ ] Criar packager para o formato final do jogo.
- [ ] Criar cache incremental de importação.
- [ ] Definir política de compressão e alinhamento.
- [ ] Criar mensagens de erro com arquivo e recurso de origem.
- [ ] Testar assets malformados como entrada não confiável.
- [ ] Adicionar hot reload apenas para o fluxo de desenvolvimento.

### Critério de conclusão

Um asset original é importado offline, convertido para o formato da engine, validado, empacotado e carregado pelo runtime sem interpretar o formato pesado original.

## Fase 6 — Cena, entidades e dados

Objetivo: criar a base de dados da cena sem comprometer o design com uma decisão não medida.

### Tarefas

- [ ] Definir entidade como handle seguro contra reutilização inválida.
- [ ] Implementar transform e hierarquia.
- [ ] Definir armazenamento de componentes.
- [ ] Prototipar alternativas de ECS quando necessário.
- [ ] Medir iteração, criação, destruição e queries.
- [ ] Definir sistema de scene serialization.
- [ ] Criar componentes de câmera, mesh, material e luz.
- [ ] Definir dependências entre sistemas.
- [ ] Criar testes de validade, geração, hierarquia e serialização.
- [ ] Comparar RAM e CPU em cenas com diferentes quantidades de entidades.

### Critério de conclusão

Uma cena pode ser criada, salva, carregada e atualizada com dados organizados, handles válidos e benchmarks que justifiquem o modelo escolhido.

## Fase 7 — Renderer escalável

Objetivo: construir a arquitetura de renderização que sustenta qualidade alta sem impor todos os custos a todos os projetos.

### Tarefas

- [ ] Definir passes e dependências reais antes de congelar o render graph.
- [ ] Implementar o caminho de iluminação escolhido com base em benchmarks.
- [ ] Implementar IBL e sombras.
- [ ] Implementar instancing.
- [ ] Implementar frustum culling.
- [ ] Avaliar GPU culling e indirect drawing.
- [ ] Avaliar Forward+, Clustered ou Deferred para diferentes classes de cena.
- [ ] Criar níveis de qualidade e fallbacks.
- [ ] Adicionar GPU timestamps e relatório de passes.
- [ ] Controlar uso de VRAM e lifetime de recursos temporários.
- [ ] Verificar que recursos avançados não aumentam o custo do caminho básico quando ausentes.

### Critério de conclusão

O renderer suporta uma cena maior, possui caminhos gráficos escaláveis e apresenta custos por passe medidos em hardware de referência.

## Fase 8 — Editor separado

Objetivo: oferecer uma ferramenta pública útil sem contaminar o runtime exportado.

### Tarefas

- [ ] Definir formato de projeto.
- [ ] Criar inicialização separada para editor e jogo.
- [ ] Reutilizar o renderer do runtime na viewport.
- [ ] Criar viewport.
- [ ] Criar hierarchy.
- [ ] Criar inspector.
- [ ] Criar asset browser.
- [ ] Criar console e diagnóstico.
- [ ] Criar profiler básico.
- [ ] Criar operações de salvar, carregar e desfazer quando necessário.
- [ ] Separar módulos Editor e Runtime no build.
- [ ] Medir RAM e startup do editor com projeto vazio.

### Critério de conclusão

Um usuário consegue criar um projeto, abrir uma cena, adicionar objetos, ajustar propriedades, salvar e visualizar o resultado usando o renderer da engine.

## Fase 9 — Módulos opcionais de runtime

Objetivo: adicionar recursos de produção sem tornar todos os jogos dependentes deles.

Cada módulo deve possuir API, build target, testes, documentação, custo medido e caminho de remoção.

### Ordem sugerida

- [ ] Animação.
- [ ] Áudio.
- [ ] Integração de física.
- [ ] Lua opcional para scripting.
- [ ] Navigation.
- [ ] Networking.
- [ ] Vídeo e outros módulos específicos quando houver necessidade real.

### Critério de conclusão

Um projeto consegue selecionar os módulos necessários no build e no empacotamento, sem carregar editor, importadores ou módulos opcionais não utilizados.

## Fase 10 — Qualidade, segurança e performance contínuas

Objetivo: impedir que crescimento da engine destrua seus princípios originais.

### Tarefas

- [ ] Criar cenas e workloads de benchmark versionados.
- [ ] Medir startup, RAM, VRAM, CPU, GPU, draw calls, loading e tamanho do executável.
- [ ] Criar baseline e detecção de regressão.
- [ ] Executar Sanitizers e análise estática no CI.
- [ ] Testar shutdown e destruição repetidamente.
- [ ] Fuzzar parsers e formatos de assets.
- [ ] Testar handles, ownership e referências após destruição.
- [ ] Integrar validation layers e debug markers.
- [ ] Documentar limitações conhecidas por plataforma e GPU.
- [ ] Criar perfis Low, Medium e High com fallbacks explícitos.
- [ ] Medir antes e depois de otimizações importantes.

### Critério de conclusão

Cada mudança relevante possui evidência de correção, custo e impacto. Regressões de performance ou memória são detectadas antes de chegar a uma release.

## Fase 11 — Plataformas e backends adicionais

Objetivo: expandir a engine sem enfraquecer o backend Vulkan inicial.

### Tarefas

- [ ] Estabilizar o contrato da RHI a partir do uso real em Vulkan.
- [ ] Definir matriz de capabilities por backend.
- [ ] Implementar D3D12 depois que o renderer comum estiver estável.
- [ ] Validar shaders Slang e layouts em DXIL.
- [ ] Implementar suporte Metal quando o caminho de toolchain e capabilities estiver maduro.
- [ ] Validar o Metal target e manter exceções específicas isoladas.
- [ ] Adicionar cada plataforma ao CI e aos benchmarks correspondentes.

### Critério de conclusão

Um mesmo projeto pode usar mais de um backend sem que cena, assets, gameplay ou API pública conheçam detalhes da API gráfica.

## Fase 12 — Recursos gráficos avançados

Objetivo: adicionar qualidade visual avançada somente quando a base estiver estável e o custo puder ser controlado.

### Ordem de avaliação

- [ ] TAA e reconstrução temporal.
- [ ] Upscaling.
- [ ] SSAO.
- [ ] SSR.
- [ ] Contact shadows.
- [ ] Volumetric fog.
- [ ] GPU particles.
- [ ] Real-time GI.
- [ ] Ray tracing.
- [ ] Virtualized geometry.

Cada recurso deve incluir:

- [ ] custo CPU/GPU/RAM/VRAM;
- [ ] impacto no startup e no tamanho do build;
- [ ] fallback para hardware mais fraco;
- [ ] testes visuais e de estabilidade;
- [ ] possibilidade de remoção do build quando não utilizado.

## Fase 13 — Ecossistema público e releases

Objetivo: transformar a engine em um projeto confiável para usuários e contribuidores externos.

### Tarefas

- [ ] Publicar documentação de instalação, arquitetura e uso.
- [ ] Publicar exemplos pequenos e completos.
- [ ] Publicar templates de projetos.
- [ ] Documentar C ABI e SDK C++.
- [ ] Criar política de compatibilidade e depreciação.
- [ ] Criar changelog e notas de release.
- [ ] Distribuir binários e ferramentas de forma reproduzível.
- [ ] Definir processo de revisão de pull requests.
- [ ] Criar issues para iniciantes e áreas de contribuição.
- [ ] Documentar como criar plugins.
- [ ] Criar testes de compatibilidade da API pública.
- [ ] Publicar benchmarks com metodologia, não apenas números.

### Critério de conclusão

Uma pessoa externa consegue instalar a engine, seguir um exemplo, criar um projeto, entender a arquitetura, contribuir com código e atualizar para uma nova versão com regras documentadas.

## Primeiro marco concreto

O primeiro objetivo de implementação é concluir a Fase 0 e iniciar a Fase 1:

```text
clone
  ↓
CMake configure
  ↓
C++ compile
  ↓
empty runtime
  ↓
window + events + input
  ↓
tests + sanitizers
```

Depois disso, o próximo marco visual é:

```text
window
  ↓
Vulkan
  ↓
Slang offline
  ↓
triangle
  ↓
mesh + texture + camera
  ↓
PBR scene
```

Esse caminho cria uma base real para avaliar as decisões seguintes sem abandonar a visão completa da engine.
