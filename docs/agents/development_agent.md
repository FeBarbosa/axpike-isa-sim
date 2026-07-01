# Repository Development Agent

## Purpose

Guide code, build, automation, and verification work in this repository so that
engineering changes remain reproducible, reviewable, and useful for research.

This document complements the research documentation guidelines. Development
work should not only make the code run; it should preserve the ability to
explain, reproduce, and validate experimental results.

## Scope

These guidelines apply to:

- C and C++ source code;
- AxPIKE and Spike-derived simulator code;
- ADF approximation code in submodules;
- shell automation scripts;
- Python automation scripts;
- Makefiles and build-system changes;
- verification harnesses;
- generated inputs;
- logs, reports, and result-processing utilities.

## General Development Principles

- Treat development as researcher-led pair programming. The researcher makes the
  primary decisions; the agent should usually navigate by decomposing work,
  identifying risks, proposing alternatives, asking clarifying questions, and
  reviewing implementation choices.
- Prefer existing repository patterns over new abstractions.
- Keep changes scoped to the behavior being modified.
- Prefer small, reviewable implementation steps and incremental validation over
  unnecessarily large code changes.
- Separate exploratory code from reusable verification or production code.
- Do not hide experimental assumptions inside scripts or build rules.
- Treat build, installation, and test commands as part of the research record.
- Preserve traceability between source changes, commands, inputs, outputs, and
  conclusions.

Before large implementations, provide a short implementation plan when
appropriate. If work is expected to affect several source files, build rules,
documents, or verification artifacts, summarize the implementation goal,
affected components, expected files, validation strategy, and possible risks
before editing. Do not make a significant architectural decision without first
explaining the alternatives, reasoning, and expected consequences.

Prefer researcher-driven execution of builds, scripts, simulations, validation
harnesses, and shell commands. Normally provide the commands, explain their
purpose and expected evidence, and ask the researcher to execute them and return
the relevant output when further analysis is needed. This preserves researcher
ownership, develops familiarity with build and validation procedures, encourages
deliberate interpretation of logs and results, and avoids consuming conversational
context with unnecessary command output.

The agent may execute commands when:

- the researcher explicitly requests execution;
- the task primarily consists of repository exploration;
- automation substantially improves researcher understanding;
- the expected output is small and directly relevant.

When an exception applies, execute only the commands needed for the current step
and summarize the relevant evidence instead of presenting unfiltered output.

## Researcher Understanding

Development artifacts should support researcher understanding, not only
automation.

When creating or modifying code, scripts, build rules, or verification harnesses,
help the researcher explain:

- why the artifact exists;
- what research or validation question it supports;
- why the chosen design works and why it was selected;
- what assumptions it encodes;
- how it can be validated;
- how its validation supports, and limits, research conclusions;
- how failures or mismatches should be interpreted.

Avoid producing automation that hides important experimental reasoning. A script
or build rule is not complete if the researcher cannot explain what it does, why
it is needed, and how its output affects the research workflow. Treat development
sessions involving research infrastructure, approximation methods, simulator
behavior, or validation harnesses as learning sessions: connect each important
implementation choice to the research question, its assumptions, its validation,
and its effect on experimental interpretation. When practical, ask the researcher
to explain these connections before proceeding beyond a meaningful milestone.

## Code Changes

When modifying simulator or approximation code, identify:

- the affected component;
- the affected floating-point format;
- the affected instruction, hook, or conversion path;
- whether the change affects FP32, FP64, FP16, BF16, E5M2, E4M3, scalar, vector,
  or library-level behavior;
- whether the change is an implementation fix, an experimental feature, or a
  validation aid.

Prefer small, explicit changes over broad rewrites. Avoid unrelated refactors
unless they are necessary to make the requested change safe and understandable.
After each meaningful milestone, pause to summarize what changed, why it matters,
and how to validate it. Confirm that the researcher can explain the design and
its assumptions before continuing to the next substantial step.

## C And C++ Guidelines

- Follow the local style already used in the touched files.
- Prefer explicit bit-level handling for floating-point representation work.
- Avoid ad hoc casts when a helper already exists for type conversion.
- Be careful when moving between host floating-point values and raw IEEE-754
  bit patterns.
- Make NaN, infinity, signed zero, subnormal, overflow, and underflow behavior
  explicit when they matter to the experiment.
- Avoid comments that merely restate code. Add short comments only when they
  clarify numerical assumptions, simulator behavior, or non-obvious conversion
  logic.

When changing low-precision simulation code, verify whether the documentation
and validation harnesses must also change.

## Python Script Guidelines

Python scripts should be suitable for repeated experimental use.

- Use clear argument parsing for scripts intended to be reused.
- Prefer deterministic behavior unless randomness is an explicit part of the
  experiment.
- Record enough information in logs to reproduce the run.
- Separate input generation, execution, comparison, and reporting when that
  improves clarity.
- Avoid hard-coded absolute paths unless the script is explicitly local-only.
- Use names that describe the scientific or engineering purpose, not temporary
  development stages.

For verification scripts, make mismatch policies explicit. For example, clearly
state whether NaN payload, NaN sign, signed zero, or normalization differences
are considered mismatches.

## Shell Script Guidelines

Shell scripts should be minimal and predictable.

- Use shell scripts for orchestration, not complex parsing.
- Prefer Python for non-trivial data processing or report generation.
- Quote variables that may contain paths.
- Fail early when a command failure invalidates the rest of the script.
- Avoid silently ignoring build, simulator, or comparison failures.
- Document required environment variables when they are needed.

## Build-System Guidelines

Build changes must preserve reproducibility.

When modifying Makefiles or build configuration, identify:

- what target is affected;
- which generated files are produced;
- whether the rule is part of the default build;
- whether the rule affects local builds, installed binaries, or both;
- whether `bear -- make` still produces useful compilation metadata.

Generated headers or generated instruction sources should be integrated into the
default build only when they are required for a normal build to succeed.

After build-system changes, prefer verifying:

- the specific target that changed;
- the full relevant build when feasible;
- whether `make install` is required for the executable on `PATH` to reflect the
  change.

## Automation And Verification Artifacts

Verification artifacts should make the validation question obvious.

Each maintained verification artifact should have:

- a clear name;
- a defined input format;
- a defined output format;
- a comparison policy;
- a known scope;
- a documented limitation.

Avoid keeping redundant smoke tests, obsolete binaries, unused logs, or scripts
whose purpose is already covered by a clearer harness.

## Result And Log Handling

Do not commit bulky generated results unless they are intentionally preserved as
evidence.

Prefer committing:

- concise reports;
- small representative inputs;
- scripts that regenerate results;
- mismatch summaries;
- documentation explaining how to reproduce large results.

Avoid committing:

- binaries;
- temporary logs;
- full exhaustive logs unless explicitly needed;
- generated files that can be recreated cheaply and reliably.

If a large result is important, document where it was produced, how it was
produced, and what summary was extracted from it.

## Submodule Guidelines

Treat submodules as separate code ownership contexts.

When changing a submodule:

- inspect the submodule status separately;
- commit submodule changes inside the submodule first;
- update the parent repository pointer only after the submodule commit exists;
- document whether the parent repository depends on the new submodule commit.

Do not assume that a parent repository commit captures submodule file changes.

## Testing Expectations

Testing should match the risk of the change.

Define how each incremental step will be checked before extending the change.
Report what was validated, what remains unvalidated, and what those boundaries
mean for experimental interpretation.

For low-risk documentation-only changes, review the rendered or plain Markdown
content.

For code or build changes, prefer at least one focused test that exercises the
changed behavior.

For low-precision arithmetic changes, consider whether validation is needed at:

- direct helper-function level;
- instruction hook level;
- simulator execution level;
- application level.

Do not claim application-level conclusions from helper-level validation alone.

## Documentation Coupling

Update documentation when a development change affects:

- numerical behavior;
- approximation coverage;
- supported formats;
- build commands;
- required dependencies;
- verification procedure;
- interpretation of existing results.

Documentation should state whether a change affects implementation validation,
numerical characterization, application-level evaluation, or reproducibility.

## Repository Fork Boundaries

This project is developed in personal forks of the AxPIKE repositories.

For the parent repository, all issues, branches, commits, pull requests, and
pushes must target the fork:

- `https://github.com/FeBarbosa/axpike-isa-sim`

Do not create issues, branches, commits, pull requests, or pushes in the original
upstream repository:

- `https://github.com/VArchC/axpike-isa-sim`

For the `adele/adf` submodule, all issues, branches, commits, pull requests, and
pushes must target the fork:

- `https://github.com/FeBarbosa/axpike-adf`

Do not create issues, branches, commits, pull requests, or pushes in the original
upstream repository:

- `https://github.com/VArchC/axpike-adf`

Before pushing, opening a pull request, or creating an issue, verify the active
remote with `git remote -v`.

When working inside a submodule, verify the submodule remote separately. Do not
assume that the parent repository remote applies to the submodule.

If an upstream repository is configured as a remote, it should be used only for
reading, comparison, or synchronization, not as a target for project work unless
the researcher explicitly requests it.

## Branch Guidelines

Use `master` as the default base branch unless the repository clearly defines a
different base branch.

Before creating commits, verify that the current branch is appropriate for the
change.

Check:

- the current branch name;
- the apparent purpose of the branch;
- whether the branch already contains local commits not present in `master`;
- whether the new change belongs to the same conceptual scope;
- whether documentation, verification, build, and submodule changes should be
  committed together or split across branches.

Prefer creating or switching to a dedicated branch when the new work has a
different research question, implementation goal, or verification scope from the
current branch.

Avoid adding unrelated work to a branch only because it is currently checked out.

When working with submodules, also verify the submodule branch and whether the
parent repository branch is intended to depend on that submodule state.

If the relationship between the current branch and `master` is unclear, inspect
the commit history before committing.

## Commit Guidelines

Prefer small commits with one clear purpose.

A commit should usually group changes that share the same reason, such as:

- one implementation change;
- one verification harness update;
- one documentation update;
- one build-system change;
- one submodule pointer update.

Avoid mixing unrelated code, documentation, generated results, and cleanup in the
same commit unless they are required to make one change understandable or
reproducible.

Commit size should be judged by conceptual scope, not by line count. A commit is
too large when it becomes difficult to explain why all included changes belong
together.

Use commit messages that describe the behavior, validation, or reproducibility
impact. Prefer messages such as:

- `Use FlexFloat for FP16 low-precision simulation`
- `Document FP16 NaN normalization mismatch`
- `Add file-driven FP16 verification harness`

Avoid messages that only describe file edits, such as:

- `Update files`
- `Fix stuff`
- `Change README`

For research-related commits, the message should make clear whether the commit
affects implementation, verification, documentation, build behavior, or
experimental interpretation.

## Commit Preparation

Before committing development changes, check:

- `git status --short` in the parent repository;
- `git status --short` in modified submodules;
- the current branch and its intended scope;
- whether the new commit belongs on the current branch;
- whether generated files should be tracked or ignored;
- whether documentation reflects behavior changes;
- whether tests or focused verification were run;
- whether installed tools reflect the latest build when that matters.

## Push And PR Preparation

Before pushing, opening a pull request, or creating an issue, check:

- the active repository remote;
- whether the target remote is the expected fork;
- whether the same check was done inside modified submodules;
- whether the branch name and commit scope match the fork-based workflow.

Do not push, open pull requests, or create issues against upstream repositories
unless the researcher explicitly requests that target.
