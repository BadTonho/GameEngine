# Contributing to GameEngine

Thank you for your interest in contributing. GameEngine is a long-term public project, so correctness, clear ownership and maintainable boundaries are more important than adding features quickly.

## Before starting

1. Read [Idea.md](Idea.md) for the project direction.
2. Read [ROADMAP.md](ROADMAP.md) to find the current phase.
3. Search existing issues and code before starting a new implementation.
4. For architectural changes, explain the trade-off before implementing them.
5. Keep changes focused and avoid unrelated refactors.

## Development setup

The project uses CMake and C++20. See the build instructions in [README.md](README.md).

The supported validation commands are:

```sh
cmake --preset <preset>
cmake --build --preset <preset>
ctest --preset <preset>
```

The project does not use an external test framework in its current phase. Add tests through CTest unless a later decision explicitly changes this policy.

## Code guidelines

- use C++20 without compiler extensions;
- keep ownership and lifetime explicit;
- use RAII for scope-bound resources;
- do not use owning raw pointers when ownership can be represented explicitly;
- avoid unnecessary allocations in hot paths;
- keep modules cohesive and dependencies directed;
- separate runtime, editor and offline tooling code;
- keep backend-specific graphics code behind the rendering boundary;
- do not expose C++ classes, templates, STL types or exceptions through the C ABI;
- add comments for constraints, invariants and design reasons, not for obvious syntax.

Run the formatter and relevant static analysis before submitting changes. Do not claim a check passed unless it was actually run.

## Documentation

Public documentation is written in English. Keep architecture, roadmap, API, contribution and user documentation in their appropriate locations. Update documentation when behavior, build steps, public interfaces or ownership changes.

## Pull requests

A pull request should:

- describe the problem and the chosen approach;
- identify important ownership, lifetime and performance consequences;
- include tests for meaningful behavior changes when practical;
- report checks that were run and any checks that could not be run;
- avoid unrelated files and generated artifacts;
- keep the final diff free of credentials, tokens, private keys, personal data, local paths and other sensitive information.

## Public repository safety

This repository is public. Before committing, inspect the complete diff and new files for secrets, credentials, private configuration, personal information and machine-specific paths. If sensitive content is found, remove it from the proposed change and report pre-existing exposure to the maintainers without reproducing the value.
