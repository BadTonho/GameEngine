# Editor mínimo

Fase 8A adiciona `gameengine_editor` como um executável separado do runtime. A sessão do
editor lê um manifesto `.geproject` v1, resolve uma cena `.gescene` relativa ao projeto e
associa a `Scene` carregada ao renderer por uma ponte interna. A C ABI e o RHI público não
ganham componentes de editor.

## Projeto

Um projeto novo é criado com:

```text
gameengine_editor --new-project <diretorio>
```

Isso gera `gameengine.geproject` e `scenes/main.gescene` com o cubo, a câmera e a luz
procedurais. Para abrir e testar a sessão:

```text
gameengine_editor --project <diretorio>/gameengine.geproject --smoke-test
```

O parser JSON é pequeno e estrito: a versão deve ser `1`, a cena deve ser um caminho relativo
sem componentes `.` ou `..`, e apenas mesh/material procedurais são aceitos nesta etapa. A
serialização é estável e o salvamento usa arquivos temporários antes da substituição final.
Uma falha de leitura mantém a cena carregada anterior intacta.

## Viewport e edição

Com Vulkan disponível, o editor reutiliza o renderer e desenha uma UI 2D interna sobre o pass
da cena. Os vértices da UI ficam em um buffer host-visible persistente, dividido por frame em
voo; a fonte bitmap 8x8 e os textos são gerados deterministicamente em memória. A UI mostra a
hierarquia ordenada pelo índice da entidade, o transform selecionado, o estado de salvamento e
diagnósticos básicos.

Os atalhos iniciais são:

| Entrada | Ação |
| --- | --- |
| clique esquerdo na hierarquia | seleciona a entidade |
| setas | move em X/Z |
| PageUp/PageDown | move em Y |
| `R` | restaura o transform inicial |
| `Ctrl+S` | salva a cena |
| `Escape` | fecha o editor |

Sem Vulkan, o mesmo target valida o projeto e informa `unavailable` em smoke test, permitindo
testar o formato e o ciclo da sessão em builds de ferramentas sem backend gráfico.

Asset browser, file dialog, edição de mesh/textura/material, câmera livre, undo/redo, console e
profiler continuam fora da Fase 8A.

## Ferramentas da Fase 8B

O `AssetCatalog` é um módulo somente leitura do editor. Um refresh explícito percorre a raiz do
projeto, ignora links simbólicos e considera apenas `.gemesh`, `.getex`, `.gemat` e `.gescene`.
Os caminhos são armazenados relativos, normalizados e ordenados lexicograficamente. Cada arquivo
é validado pelo leitor zero-copy de `engine/assets`; o catálogo retém apenas metadados e erros,
nunca uma cópia proprietária dos bytes do asset. Nenhum asset real é necessário: o projeto novo
continua contendo somente a cena procedural.

O painel `Console` usa um ring buffer de capacidade fixa, com os níveis `debug`, `info`, `warning`
e `error`. As mensagens exibidas no editor também continuam sendo escritas em `stderr`, mas o
console não executa comandos nem faz polling de arquivos.

O painel `Profiler` reutiliza `FrameTimingReport` do renderer e mostra passes, tempos CPU/GPU,
draws, dispatches, visibilidade, perfil efetivo e o startup do editor. RAM atual e pico são
consultados somente em amostras explícitas fora da gravação do frame; quando a plataforma não
oferece a métrica, o painel informa `unavailable`. O runtime exportado não inclui esses módulos.
