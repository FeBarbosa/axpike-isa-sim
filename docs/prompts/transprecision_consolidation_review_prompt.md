# Transprecision Consolidation Review Prompt

Use this prompt to start a new chat focused on consolidating, reviewing,
documenting, and studying the current transprecision implementation in AxPIKE
before starting the next experimental policy.

```text
Read AGENTS.md, docs/agents/development_agent.md, and
docs/agents/research_documentation_agent.md.

I want to consolidate the current transprecision implementation in AxPIKE before
starting the promotion/demotion policy based on the least significant bits of
the mantissa.

An important part of this work is improving my technical and scientific
understanding of the implementation. The process should work as a guided study:
reconstruct the decisions that were made, explain how transprecision was
implemented in the code, how it was validated, which metrics were incorporated
into AxPIKE, and how each of these parts supports or limits the experiment. Do
not only summarize the final state; help me become able to explain the
implementation and its validation.

Perform this reconstruction iteratively, in small steps. At each step, explain a
limited block of the implementation or validation, connect that block to the
scientific objective of transprecision, and propose a short questionnaire to
check my understanding before moving forward. Use my answers to adjust the level
of detail, correct conceptual gaps, and decide the next study block.

Objectives:
1. review the current implementation state;
2. map the modified files and their responsibilities;
3. list the design decisions already made;
4. list the validations that were executed and what each one demonstrates;
5. identify known limitations;
6. organize the next steps until the paper submission on August 7, 2026.

Do not implement anything initially. First evaluate the repository and produce a
documentation and validation plan.

Known state:
- transprecision tags were added to the FP register state;
- loads and architectural writes classify the written value;
- FP operations classify operands and use the highest-precision effective type;
- results are reclassified on write-back;
- special values have a defined policy: NaN/Inf values produced by operations
  use the effective type, external NaN/Inf values use the architectural type,
  and +/-0 uses the smallest available type;
- transprecision counters were added to the CSV output;
- LeNet was executed and generated AxPIKE_transprecision_*.csv;
- fcvt_d_s/fcvt_s_d were adjusted to count using the effective type of the
  source operand.
```

After consolidating the current implementation, review the code with a focus on
maintainability, readability, and explainability. This review should evaluate
whether the implementation needs to be reorganized to separate more clearly:

- the simulation of low-precision types through AxPIKE/ADF approximations;
- the initial transprecision infrastructure based on tags, classification, and
  metrics;
- the points shared by both approaches, especially conversions, value
  classification, and instrumentation.

The objective of this stage is not to refactor immediately, but to identify
risks related to conceptual coupling, ambiguous names, mixed responsibilities,
and design decisions that should be documented before evolving toward the
mantissa-bit-based policy.
