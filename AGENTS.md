# AGENTS.md

## Purpose

This file defines how AI coding agents must behave when working in this repository.

The project architecture, goals, technology choices and long-term vision are documented in `Ideia.md`.

Agents must read `Ideia.md` before making architectural decisions.

This file is not the engine design document.

---

# 1. Read Before Changing

Before editing code, the agent must:

1. Inspect the relevant existing files.
2. Understand the current subsystem boundaries.
3. Check existing naming and coding conventions.
4. Check whether the requested functionality already exists.
5. Read relevant documentation before introducing a new pattern.
6. Read `Ideia.md` when the task affects architecture, language choice, rendering, memory, runtime structure, tooling or public APIs.
7. Verify the exact target files and directories before editing, and confirm afterward that no unrelated files were changed.

Do not make architectural assumptions without first checking the repository.

---

# 2. Follow the Existing Project Direction

Do not redesign the project unless the user explicitly requests it.

The agent must preserve the architecture defined by the repository and `Ideia.md`.

In particular, do not silently replace or change:

- C++ as the primary runtime language
- C ABI as the interoperability boundary
- Rust as the preferred language for offline tooling
- Lua as optional gameplay scripting
- Slang as the preferred shader source language
- Vulkan as the initial graphics backend
- the modular runtime philosophy
- the lightweight-engine philosophy
- the separation between runtime, editor and offline tools

If an alternative is technically better, explain it first instead of silently implementing it.

---

# 3. Do Not Invent Requirements

Implement the task requested by the user.

Do not add unrelated systems because they may be useful later.

Do not implement speculative features.

Do not create APIs for hypothetical future use unless they are necessary for the current task.

Prefer:

```text
requested feature
+
minimum supporting architecture
```

Avoid:

```text
requested feature
+
large generic framework
+
future abstractions
+
unrequested systems
```

---

# 4. Prefer Small, Reviewable Changes

Make the smallest coherent change that correctly solves the task.

Avoid rewriting unrelated files.

Avoid mass formatting unrelated code.

Avoid renaming unrelated symbols.

Avoid moving files unless the change requires it.

Do not replace working systems purely because another implementation is preferred.

Large refactors require a clear technical reason.

Before considering a task complete, inspect the repository status and the diff for the exact target files. If files outside the requested scope were modified, stop and investigate instead of leaving unrelated changes in the worktree.

---

# 5. Preserve User Work

Never intentionally discard user-written code unless explicitly requested.

Before replacing an implementation:

- understand why it exists;
- preserve behavior that is still required;
- check references and call sites;
- preserve public contracts unless a breaking change is requested.

Do not overwrite unrelated changes.

Do not revert code simply because it differs from your preferred style.

---

# 6. Correctness First

Priority order:

```text
1. Correctness
2. Safety
3. Clear ownership
4. Simplicity
5. Performance
6. Memory efficiency
7. Maintainability
8. Binary size
9. Startup time
10. Convenience
```

Performance is important, but never knowingly introduce:

- memory corruption;
- invalid lifetimes;
- data races;
- undefined behavior;
- broken synchronization;
- invalid GPU resource usage;

just to improve performance.

---

# 7. Keep the Runtime Lightweight

When modifying runtime code, consider:

- RAM usage
- CPU overhead
- allocations
- startup cost
- binary size
- dependencies
- synchronization
- cache locality
- GPU overhead

Do not introduce permanent runtime cost for an optional feature when the feature can be compiled out.

Prefer compile-time exclusion over runtime disabling where practical.

---

# 8. No Hidden Expensive Work

Avoid APIs that hide expensive operations.

Do not silently perform major operations such as:

- disk access;
- blocking waits;
- GPU synchronization;
- large memory allocation;
- resource compilation;
- large data copies;
- thread creation;
- shader compilation;

inside apparently cheap functions.

Expensive behavior should be explicit or clearly documented.

---

# 9. Memory Rules

Runtime memory ownership must be explicit.

When adding or modifying allocations, determine:

- who allocates;
- who owns;
- who frees;
- lifetime;
- whether memory can move;
- whether references remain valid;
- whether allocation occurs in a hot path.

Avoid unnecessary heap allocations.

Avoid allocating every frame unless required.

Prefer existing project allocators and memory systems.

Do not introduce a new allocator abstraction when the existing one is sufficient.

For C++ runtime code:

- use RAII for resources with scope-bound lifetime;
- do not use owning raw pointers when ownership can be represented explicitly;
- distinguish C++ object lifetime from the allocator that owns its storage;
- evaluate standard-library containers and custom containers by measured cost;
- document and validate policies for exceptions, RTTI, coroutines and allocators instead of adopting or banning them by preference.

---

# 10. Hot Path Rules

Treat the following as potentially performance-critical:

- frame update;
- render submission;
- culling;
- entity/component iteration;
- animation;
- physics synchronization;
- job scheduling;
- asset streaming;
- GPU upload preparation.

Inside hot paths, avoid unnecessary:

- allocations;
- locks;
- string operations;
- filesystem access;
- logging;
- pointer chasing;
- copies;
- virtual/indirect calls;
- synchronization.

Do not claim an optimization is faster without measurement when measurement is practical.

---

# 11. Rendering Rules

Keep graphics API code behind the rendering backend / RHI boundary.

Do not spread Vulkan, Direct3D 12 or Metal calls across unrelated engine systems.

Shared renderer code should depend on engine abstractions, not backend-specific implementation details.

Backend-specific exceptions must remain isolated.

---

# 12. Shader Rules

Use Slang as the default shader source language unless there is a clear backend-specific reason not to.

Prefer offline shader compilation.

Do not make the Slang compiler a required dependency of exported games unless runtime shader compilation is explicitly enabled.

Avoid uncontrolled shader permutation growth.

Before adding a shader variant, consider whether the behavior can be expressed with:

- specialization;
- material data;
- runtime branching;
- feature flags;
- offline permutation pruning.

Do not duplicate shader implementations for Vulkan, D3D12 and Metal when shared Slang code can reasonably support them.

---

# 13. Runtime / Editor Separation

Do not introduce editor dependencies into runtime modules.

Editor-only features belong in editor code.

Examples:

- hierarchy UI;
- inspector;
- asset browser;
- editor viewport controls;
- project manager;
- development panels;
- import interfaces.

Exported games must not carry editor systems unless explicitly required.

---

# 14. Runtime / Tooling Separation

Heavy asset processing should normally happen offline.

Prefer:

```text
source asset
↓
offline tool
↓
optimized engine asset
↓
runtime
```

Do not move importers, converters, compressors or heavy compilers into the runtime without a strong reason.

Rust tooling must not become a runtime dependency by accident.

---

# 15. Dependency Rules

Do not add a dependency automatically because it makes implementation easier.

Before adding a dependency, evaluate:

- why it is needed;
- binary-size impact;
- memory impact;
- runtime overhead;
- transitive dependencies;
- build-time impact;
- platform support;
- licensing;
- maintenance status;
- replacement difficulty.

Prefer existing dependencies when they already solve the problem.

Do not add a large library to use a tiny feature without considering a smaller solution.

---

# 16. Public API Rules

Treat public APIs as expensive commitments.

Before changing public API or C ABI:

1. Check existing users.
2. Avoid unnecessary breaking changes.
3. Keep ownership clear.
4. Keep ABI types simple.
5. Avoid leaking internal C++ implementation details.
6. Avoid unstable memory layouts.
7. Document behavior and lifetime.

Do not expose internal containers or implementation-specific pointers through the public ABI unless explicitly required.

A source-level C++ SDK may provide an ergonomic interface for C++ users, but binary plugins and cross-language integrations should use the versioned C ABI unless a stronger compatibility contract is explicitly defined. Do not expose C++ classes, templates, STL types, exceptions or compiler-specific layouts as an ABI commitment.

---

# 17. Error Handling

Do not silently ignore errors.

Distinguish between:

- invalid user/data input;
- recoverable runtime failure;
- programmer error;
- broken internal invariant;
- fatal engine failure.

Assertions are acceptable for impossible internal states.

Assertions must not replace proper handling of expected runtime failures.

---

# 18. Logging

Do not add noisy logs to hot paths.

Use appropriate log levels.

Do not perform expensive formatting for disabled logs when avoidable.

Debug logging must be removable or inexpensive in release builds.

---

# 19. Threading

Do not create background threads casually.

When adding threading, define:

- ownership;
- startup;
- shutdown;
- synchronization;
- data access;
- failure behavior.

Prefer the engine job system or existing worker pool when appropriate.

Avoid global locks.

Do not use lock-free programming simply because it sounds faster.

---

# 20. Platform Code

Keep platform-specific code isolated.

Do not spread operating-system conditionals throughout shared modules.

Prefer:

```text
shared code
↓
platform abstraction
↓
Windows / Linux / macOS implementation
```

Only use direct platform APIs outside the platform layer when there is a documented reason.

---

# 21. Naming and Style

Follow the existing repository style.

Do not impose a new style without being asked.

Use clear names.

Avoid unnecessary abbreviations.

Prefer consistency with surrounding code over personal preference.

Comments should explain why, constraints or invariants.

Do not write comments that merely restate the code.

---

# 22. Documentation

Update documentation when a change alters:

- architecture;
- public API;
- file format;
- build process;
- subsystem ownership;
- supported platform behavior;
- shader pipeline;
- asset pipeline;
- configuration.

Do not copy the whole architecture into `AGENTS.md`.

Architecture belongs in `Ideia.md` or the appropriate design document.

Documentation must remain organized and easy to navigate:

- place each document in the appropriate repository location;
- keep architecture, roadmap, API, contribution and user documentation separated by purpose;
- update an existing document instead of creating a duplicate when the subject already has an owner;
- use consistent headings, terminology and links;
- keep links and referenced paths valid;
- do not scatter temporary notes or generated documentation through source directories.

---

# 23. Testing

For meaningful code changes, run the relevant available checks.

Depending on the subsystem, this may include:

- build;
- unit tests;
- integration tests;
- renderer tests;
- asset tests;
- shader compilation;
- example application;
- benchmark;
- debug validation.

Do not claim tests passed unless they were actually run.

If a relevant test cannot be run, say so.

---

# 24. Benchmarks

Do not invent benchmark numbers.

Do not claim performance improvements based only on intuition.

When changing performance-sensitive systems, compare before and after when practical.

Relevant measurements may include:

- RAM usage;
- VRAM usage;
- allocations per frame;
- CPU frame time;
- GPU frame time;
- startup time;
- asset loading time;
- shader/pipeline creation time;
- executable size;
- build time.

Performance regressions require investigation or an explicit tradeoff.

---

# 25. Do Not Over-Optimize Early

The engine is performance-focused, but not every line needs manual optimization.

Prefer a clear implementation first when performance is not yet relevant.

Optimize measured bottlenecks and architectural costs.

Do not make code significantly harder to maintain for insignificant theoretical gains.

---

# 26. Do Not Add Abstractions Without Need

Before adding an abstraction, ask:

1. What problem does it solve now?
2. Are there actually multiple implementations?
3. Does it reduce or increase complexity?
4. Does it hide important costs?
5. Does it make ownership less clear?
6. Can the current problem be solved more directly?

Avoid architecture astronautics.

## Modularity and monoliths

Avoid unnecessary monolithic files and modules. When a file or subsystem accumulates multiple independent responsibilities, separate it into cohesive units with clear ownership, lifecycle and dependencies whenever that improves comprehension, testing or build boundaries.

Do not split code into arbitrary tiny files merely to increase the file count. Prefer modules organized around stable responsibilities, keep dependencies directed and minimize circular coupling. A larger unit is acceptable when its responsibilities are genuinely cohesive and the separation would add more complexity than value.

---

# 27. Do Not Change Languages Casually

Do not change C++ as the primary runtime language or introduce additional implementation languages without a concrete reason.

Current intended roles are defined in `Ideia.md`.

In particular, do not introduce into core/runtime code merely out of preference:

- Zig;
- C#;
- Java;
- Python;
- another scripting runtime;

If another language materially improves a subsystem, explain the tradeoff before making the change.

---

# 28. Repository Hygiene

Do not commit or generate unnecessary:

- binaries;
- build artifacts;
- cache files;
- temporary files;
- IDE-specific files;
- generated output;

unless the repository intentionally tracks them.

Respect `.gitignore`.

Do not add secrets, tokens, credentials or machine-specific paths.

---

# 29. Generated Code

If code is generated:

- clearly separate generated and handwritten files;
- do not manually edit generated files unless instructed;
- document the generator;
- ensure regeneration is deterministic when practical.

Do not add generated noise to unrelated diffs.

---

# 30. File Formats and Serialization

When changing runtime file formats:

- version the format when appropriate;
- validate sizes and offsets;
- handle malformed data;
- consider alignment;
- consider endianness where relevant;
- avoid unsafe unchecked parsing;
- document compatibility implications.

Do not silently break existing assets without a migration plan when compatibility matters.

---

# 31. Security

Treat external files as untrusted input.

Validate:

- lengths;
- counts;
- offsets;
- indexes;
- integer arithmetic;
- versions;
- enum values;
- compressed sizes;
- resource references.

Do not trust imported assets simply because they were produced by a common tool.

---

# 32. When Something Is Unclear

Do not guess about existing repository behavior when the answer can be determined from the code.

Search and inspect first.

For small ambiguities, choose the solution most consistent with existing code and document the assumption.

Ask the user only when the ambiguity materially changes the requested result and cannot reasonably be resolved from the repository.

---

# 33. When Fixing Bugs

When fixing a bug:

1. Identify the root cause.
2. Avoid merely hiding the symptom.
3. Keep the fix scoped.
4. Add or update a test when practical.
5. Check related lifetime, ownership or synchronization issues.
6. Avoid unrelated refactors in the same change.

---

# 34. When Refactoring

A refactor should preserve behavior unless behavior changes are explicitly part of the task.

Before a large refactor:

- understand current dependencies;
- preserve externally visible behavior;
- keep the project buildable when practical;
- avoid changing multiple unrelated systems simultaneously.

Do not refactor just to make the code look more familiar.

---

# 35. When Adding a Feature

Before implementing a significant feature:

1. Find the correct subsystem.
2. Check whether it should be runtime, editor, plugin or offline tooling.
3. Determine ownership.
4. Determine lifecycle.
5. Determine memory cost.
6. Determine threading implications.
7. Determine whether it can be optional.
8. Determine whether it affects public API.
9. Determine how it will be tested.
10. Implement the smallest coherent version.

---

# 36. When Working on Rendering Features

For rendering features, evaluate both quality and cost.

Consider:

- image quality;
- GPU cost;
- CPU cost;
- VRAM;
- RAM;
- bandwidth;
- shader complexity;
- scalability;
- low-end fallback;
- high-end path.

The goal is not maximum visual quality at any cost.

The project goal is high visual quality with high efficiency.

Do not permanently penalize low-end configurations for high-end rendering features.

---

# 37. Agent Must Not

The agent must not:

- silently redesign the engine;
- ignore `Ideia.md`;
- add unrelated features;
- introduce large dependencies without justification;
- add a new runtime language by preference;
- make Lua mandatory;
- make Slang a mandatory exported-game runtime dependency;
- mix editor and runtime code unnecessarily;
- move heavy offline processing into runtime without reason;
- add permanent runtime cost for unused optional features when avoidable;
- invent benchmark results;
- claim tests were run when they were not;
- hide major performance costs;
- discard user code unnecessarily;
- mass-reformat unrelated files;
- rewrite working systems without a reason;
- add speculative abstractions;
- sacrifice correctness for benchmark numbers;
- leave known resource leaks;
- leave known invalid lifetime bugs;
- leave known race conditions;
- create circular module dependencies without strong justification.

---

# 38. Agent Should

The agent should:

- inspect before editing;
- preserve existing design;
- keep changes focused;
- prefer simple implementations;
- minimize runtime overhead;
- make ownership explicit;
- keep optional systems removable;
- reuse existing infrastructure;
- isolate third-party APIs;
- keep platform-specific code isolated;
- test relevant changes;
- benchmark performance-sensitive changes when practical;
- update documentation when behavior changes;
- explain meaningful tradeoffs;
- report limitations accurately.

---

# 39. Completion Checklist

Before considering a task complete, check as applicable:

```text
[ ] requested behavior implemented
[ ] project compiles
[ ] relevant tests pass
[ ] no known resource leak introduced
[ ] ownership/lifetimes are valid
[ ] shutdown path remains valid
[ ] error paths considered
[ ] no unnecessary dependency added
[ ] runtime/editor boundary preserved
[ ] runtime/tooling boundary preserved
[ ] no unnecessary per-frame allocations added
[ ] public API compatibility considered
[ ] shader pipeline rules respected
[ ] performance-sensitive changes measured when practical
[ ] documentation updated when needed
[ ] target files and directories verified before and after editing
[ ] documentation is stored in the appropriate location and remains organized
[ ] no unnecessary monolithic file or module was introduced
[ ] no unrelated files changed
```

---

# 40. Final Rule

`Ideia.md` defines what this engine is intended to become.

`AGENTS.md` defines how an AI agent must work while helping build it.

When there is a conflict:

1. Follow the user's explicit current instruction.
2. Preserve correctness and safety.
3. Follow `Ideia.md` for project architecture.
4. Follow `AGENTS.md` for agent behavior.
5. Follow existing repository conventions when no higher-priority rule applies.
