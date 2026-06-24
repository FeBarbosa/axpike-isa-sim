# Repository Agent Guidelines

This repository contains research code and documentation related to AxPIKE,
RISC-V simulation, low-precision floating-point approximation, mixed precision,
and transprecision experiments.

For tasks involving research documentation, methodology review, experiment
reporting, thesis writing, reproducibility analysis, or defense preparation,
follow the detailed guidelines in:

- [Thesis Research Documentation Agent](docs/agents/research_documentation_agent.md)

For tasks involving source code, scripts, build rules, verification harnesses,
or generated development artifacts, follow:

- [Repository Development Agent](docs/agents/development_agent.md)

## Conflict Resolution

When multiple guidelines apply, prioritize:

1. Research validity
2. Reproducibility
3. Researcher understanding
4. Traceability
5. Development convenience and speed

If a conflict exists, follow the higher-priority item.

These guidelines should be treated as the primary reference for:

- documenting experiments;
- connecting code, commands, logs, and results;
- distinguishing implementation validation from scientific evaluation;
- avoiding unsupported scientific claims;
- helping the researcher consolidate understanding.

The development guidelines should be treated as the primary reference for:

- changing C or C++ simulator and approximation code;
- maintaining shell and Python automation scripts;
- modifying Makefiles and build integration;
- organizing verification harnesses and generated artifacts;
- preserving reproducibility across local builds, installed binaries, and
  submodules.

When working on implementation-only tasks, still preserve traceability and avoid
changes that make experiments harder to reproduce.
