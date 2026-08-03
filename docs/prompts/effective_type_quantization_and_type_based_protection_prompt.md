# Effective-Type Quantization and Type-Based Protection Refactor Prompt

Use this prompt to start a new agent session focused on changing the numerical
semantics of the AxPIKE transprecision model. The work makes the effective type
represent the simulated operation precision and replaces transition-specific
protected-bit parameters with one parameter for each source type.

```text
# Initial Instructions

Read `AGENTS.md` and follow, in particular:

- `docs/agents/development_agent.md`;
- `docs/agents/architecture_functional_verification_agent.md`; and
- `docs/agents/research_documentation_agent.md`.

Also read:

- `docs/transprecision/active_mantissa_policy_contract.md`;
- `docs/transprecision/v1_transprecision_specification.md`;
- `verification/README.md`;
- `docs/prompts/sscad2026_submission_strategy_prompt.md`; and
- the methodology in
  `paper-sscad2026/Template_SBC/template-latex/sbc-template.tex`.

The current implementation is consolidated in local commits. Inspect the live
repository state and do not assume that the current methodology remains
correct: it describes the previous numerical model and must be reviewed after
the semantic analysis.

Do not modify files initially. First inspect the current implementation and
present:

1. the current result-classification data flow;
2. the semantic differences introduced by this request;
3. the affected code, tests, scripts, counters, and documents;
4. the principal research-validity risks; and
5. the smallest concrete implementation step.

Pause for researcher review after this initial analysis.

Do not start the complete scientific evaluation.

---

# Objective

Refactor the transprecision model so that:

1. the effective type represents the simulated precision of the operation;
2. after Spike executes an instruction in architectural FP32 or FP64, its
   result is quantized to the effective type before result classification;
3. the protected-bit parameter is associated with the effective source type,
   rather than with each source-to-candidate transition;
4. the configuration contains one parameter for each supported architectural
   or simulated type: FP64, FP32, FP16, and E5M2; and
5. result classification considers only the effective type itself or
   lower-precision candidate types.

This is a semantic change to the numerical model, not merely a command-line
configuration refactor.

---

# Intended Result Data Flow

For an operation with architectural carrier W and effective type T:

1. Spike executes the instruction normally in W, where W is FP32 or FP64.
2. The architectural result `y_W` is quantized to T.
3. The quantized value is represented again within carrier W.
4. The precision-reduction policy uses T as its source type.
5. Lower-precision candidates are evaluated from the smallest to the largest.
6. If no lower candidate is accepted, the quantized result remains in T.
7. The selected value and its precision tag are written together.
8. Subsequent instructions consume the persistent selected value and tag.

Conceptual flow:

stored operands
→ architectural FP32/FP64 operation
→ result quantization to the effective type
→ type-based precision-reduction policy
→ selected value and tag write-back

Quantization to the effective type should use the corresponding format
conversion, preferably through the same FlexFloat-based models already used by
the simulator.

Do not describe post-operation quantization as bit-accurate native E5M2 or FP16
arithmetic. Explicitly preserve the limitations concerning double rounding,
FMA, subnormal behavior, NaN payloads, and architectural exception flags.

---

# Supported Types and Mantissa Widths

Use the following explicit mantissa widths:

- FP64: 52 bits;
- FP32: 23 bits;
- FP16: 10 bits; and
- E5M2: 2 bits.

For effective source type T and lower-precision candidate J:

- `a` is the mantissa width of T;
- `b` is the mantissa width of J;
- `d_T_to_J = a - b` is the candidate-relative excess region;
- `n_T` is the protected-bit parameter associated with source type T;
- `effective_n_T_to_J = min(n_T, d_T_to_J)`; and
- `k_T_to_J = d_T_to_J - effective_n_T_to_J`.

Equivalently:

`k_T_to_J = max(0, a - b - n_T)`.

The `n_T` parameter states how many bits in the candidate-relative excess
region are protected for every reduction attempt originating in T. It does not
identify the same physical mantissa-bit positions for every candidate.

Example for `n_FP32 = 10`:

- FP32 to FP16:
  `d = 23 - 10 = 13`, so `k = 13 - 10 = 3`;
- FP32 to E5M2:
  `d = 23 - 2 = 21`, so `k = 21 - 10 = 11`.

Expected parameter ranges:

- `n_FP64`: 0 through 50;
- `n_FP32`: 0 through 21;
- `n_FP16`: 0 through 8; and
- `n_E5M2`: exactly 0 while no lower-precision candidate exists.

E5M2 must appear in configuration, manifests, and CSV output to complete the
type-indexed policy vector. Its parameter must currently be validated as zero
and must not be given an artificial numerical effect.

The full-protection endpoint is:

- `n_FP64 = 50`;
- `n_FP32 = 21`;
- `n_FP16 = 8`; and
- `n_E5M2 = 0`.

At this endpoint, no additional lossy reduction below the effective type is
allowed. This does not imply identity with the previous exact baseline because
the W-to-T quantization may already change the
architectural result.

---

# Candidate Selection

After quantization to T:

- T = FP64: try E5M2, FP16, and FP32; otherwise retain FP64;
- T = FP32: try E5M2 and FP16; otherwise retain FP32;
- T = FP16: try E5M2; otherwise retain FP16;
- T = E5M2: retain E5M2.

Derive every candidate independently from the same result already quantized to
T. Never apply candidate masks cumulatively after a rejected attempt.

The final finite result must never receive a type more precise than T under the
new semantics.

---

# Mantissa Mapping Within Architectural Carriers

Masking must operate on the least-significant mantissa bits of the effective
source type, not necessarily on the absolute least-significant fraction bits of
the architectural carrier.

Examples:

- FP16 represented in FP32 has 13 lower FP32 fraction bits outside the FP16
  mantissa;
- FP16-to-E5M2 masking must operate above those 13 carrier-only bits;
- FP32 represented in FP64 has 29 lower FP64 fraction bits outside the FP32
  mantissa; and
- a type-based mask helper should explicitly account for carrier width, source
  type width, candidate width, and `k`.

Directed tests must detect masking at an incorrect carrier offset.

---

# Architectural Decisions Required Before Implementation

Resolve and document the following points before editing behavior:

1. **Effective-type ceiling**

   Define how the effective type is constrained by the architectural precision
   of the instruction. An FP32 instruction must not accidentally become an
   effective FP64 operation because of incompatible metadata.

2. **Special values**

   Define the handling of signed zero, infinity, and NaN. Prefer preserving the
   architectural bits of infinity and NaN and using the effective type as
   context, rather than silently changing NaN sign or payload through an
   auxiliary conversion.

3. **W-to-T quantization loss**

   Treat quantization from the architectural result to T as a separate source
   of numerical change from any later reduction from T to J.

4. **Result promotions**

   Under the new model, result promotion above T should not be a normal event.
   After W-to-T quantization, the result either remains in T or is reduced.
   Audit and redefine or retire the existing result-promotion metric.

5. **Counter semantics**

   Propose counters that distinguish:

   - architectural results changed by quantization to T;
   - results unchanged by quantization to T;
   - results retained in T;
   - exact reductions relative to the T-quantized value;
   - reductions that apply additional masking;
   - nonzero values changed to signed zero; and
   - impossible promotions or invalid states, which should remain zero.

   Do not reuse an existing counter with a new meaning without changing its
   name, documentation, invariants, and downstream extraction.

6. **External writes**

   Loads and other external writes have no operation effective type. Define
   whether their source policy type remains the architectural carrier W and
   keep this path distinct from operation-result quantization.

## Researcher-approved resolution of the counter and range semantics

The following decisions supersede the exploratory counter suggestions above:

- retain only `effective_type_by_instruction` for effective-type
  observations; do not emit `transprecision_effective_type_observations`,
  `effective_type_total`, or `last_transprecision_effective_type` in the CSV;
- retain `last_transprecision_effective_type` only as internal simulator state
  used by focused validation;
- count result quantization only for supported transitions where W is wider
  than T; do not emit identity transitions where W equals T;
- for each W-to-T transition, record total quantizations, changed values,
  overflows, underflows, and changes to signed zero;
- derive unchanged quantizations as total minus changed instead of storing a
  separate counter;
- interpret a finite architectural result outside T's maximum finite range as
  model overflow, propagate signed infinity, and do not change architectural
  floating-point flags;
- interpret a finite nonzero architectural result below T's minimum normal
  magnitude as model underflow, propagate the T-quantized subnormal or signed
  zero, and do not change architectural floating-point flags;
- E5M2 supports subnormals, so underflow does not imply a change to zero;
- for a result tag reduced from T to a lower J, record total reductions and the
  changed subset; derive exact reductions as total minus changed;
- retain the scalar count of tag reductions that change a nonzero value to
  signed zero; and
- retain the existing value-class, external-write, metadata-recovery, and
  invalid-state counter groups.

---

# Configuration Interface

Replace the transition-indexed vector with a type-indexed configuration.

A possible interface is:

`--transprecision-type-protected-bits=fp64:N,fp32:N,fp16:N,e5m2:0`

Before changing the interface, evaluate:

- compatibility with the previous option;
- whether old transition names should be rejected explicitly;
- whether keeping the same option name could make old and new results
  ambiguous;
- whether a policy-version field is required; and
- how the new type-indexed vector will be recorded in every transprecision CSV.

Every generated artifact must identify the policy version and all four
effective parameter values.

---

# Incremental Work Plan

## Milestone 1 — Read-Only Semantic Audit

- Map where the effective type is calculated and recorded.
- Map all FP32 and FP64 operation-result write paths.
- Confirm that finite-result classification currently does not quantize to the
  effective type first.
- Identify instruction-specific and special-value paths.
- Propose the exact new counter contract.
- Identify which current validation claims become obsolete.
- Present the smallest implementation step and pause for review.

## Milestone 2 — Effective-Type Result Quantization

- Implement an isolated helper that quantizes an architectural result to E5M2,
  FP16, FP32, or FP64 and returns its representation in the original carrier.
- Do not apply the new `n_T` reduction policy yet.
- Validate each relevant carrier/effective-type combination.
- Demonstrate that the returned finite result is representable in T.
- Validate sign, zero, range boundaries, subnormals, infinity, and NaN.
- Confirm that helper conversions do not alter guest-visible `fflags`.

Pause and review this semantic milestone before continuing.

## Milestone 3 — Type-Based Protected-Bit Policy

- Replace the transition-specific parameters with the four type-specific
  parameters.
- Implement `effective_n` and `k` for every lower candidate.
- Implement candidate-relative masking at the correct carrier offset.
- Derive every candidate from the original T-quantized result.
- Select the smallest accepted candidate.
- Retain T if no lower candidate is accepted.
- Guarantee that a finite result cannot be assigned above T.

## Milestone 4 — Instruction and Metadata Integration

- Integrate quantization and classification with operation-result write macros.
- Audit FP32, FP64, FMA, conversions, comparisons, moves, loads, and stores.
- Keep external-write semantics distinct from operation-result semantics.
- Ensure that value and tag persist together.
- Revalidate lazy metadata recovery and contextual NaN-boxed FP32 handling.

## Milestone 5 — Counters and CSV Contract

- Update policy configuration and CSV export.
- Separate W-to-T quantization changes from later T-to-J reductions.
- Remove, replace, or redefine result-promotion counters.
- Record the policy version and four type parameters.
- Update summary extraction and invariant checks.
- Reject old artifacts that do not identify the new policy semantics.

## Milestone 6 — Experiment-Matrix Redesign

- Reformulate the generator for FP64, FP32, FP16, and E5M2 parameters.
- Recalculate and test the number of raw and deduplicated configurations.
- Do not preserve the previous count of 201 dynamic configurations as an
  assumption.
- Redefine proportional, per-type, and global parameterizations.
- Retain aliases when multiple parameterizations produce the same type vector.
- Do not execute the complete matrix in this milestone.

### Implemented milestone-6 resolution

The following multi-parameter matrix is retained as implementation history. It
was superseded for the SSCAD experiment by the researcher-approved uniform
saturated sweep documented in
`docs/experiments/sscad2026_uniform_n_experiment_protocol.md`.

- Use canonical vector order FP64, FP32, FP16, E5M2 with maximum protected
  widths 50, 21, 8, and 0.
- Generate proportional points for ratios 1.00, 0.75, 0.50, 0.25, and 0.00,
  rounding each type width upward.
- Generate a per-type sweep over every valid integer width while all other
  types remain fully protected. Retain the single E5M2 point at zero as an
  explicit coverage alias.
- Generate a global-absolute sweep from 0 through 50 with independent
  saturation at each type maximum.
- Deduplicate complete vectors while retaining every originating
  parameterization as an alias.
- Expect 139 raw points, 104 unique dynamic configurations, and 107 planned
  full-application executions after adding the three fixed controls.
- Emit manifest schema 2, policy `effective-type-quantization-v4`, numeric
  policy version 4, and only the type-based AxPIKE command-line option.

---

# Required Directed Validation

Add independent, directed checks for:

- FP32/FP64 result quantization to every valid effective type;
- identity when T equals the architectural carrier;
- representability of the quantized finite result in T;
- `d`, `effective_n`, and `k` calculation;
- saturation when `n_T` exceeds a candidate-specific excess width;
- correct carrier offsets for simulated source types;
- FP16-to-E5M2 reduction;
- independent candidate attempts;
- no additional lossy reduction when candidate-specific `k = 0`;
- retention in T when no lower candidate is accepted;
- impossibility of a result type above T;
- persistent selected values and tags;
- signed zero and masked-to-zero behavior;
- infinity, NaN sign/payload, NaN boxing, and subnormal cases;
- architectural rounding-mode and `fflags` boundaries; and
- all revised counter invariants.

Use an independent oracle where practical. Do not validate a helper only by
calling the same implementation through a different wrapper.

After directed validation, execute only the deterministic reduced LeNet
validation. Compare full protection and no protection, repeat the runs, and
verify:

- outcome partitioning;
- policy identity;
- effective-type and result-transition partitions;
- zero unexplained unclassified/fallback paths;
- impossible result promotions equal to zero;
- contextual NaN-boxing invariants; and
- deterministic CSV and summary hashes.

Do not run the complete 10,000-image scientific evaluation before review of
this milestone.

---

# Documentation Requirements

Update:

- `docs/transprecision/active_mantissa_policy_contract.md`;
- `docs/transprecision/v1_transprecision_specification.md`;
- `verification/README.md`;
- the reduced-validation report; and
- the paper methodology.

The methodology must distinguish:

- architectural carrier W;
- operation effective type T;
- post-operation quantization to T;
- later reduction from T to lower candidates;
- loss introduced at each stage; and
- limitations relative to native E5M2 or FP16 arithmetic.

Do not claim equivalence with the previous exact policy at the full-protection
endpoint. Under the new semantics, full protection prevents additional
reduction below T, but W-to-T quantization can still change the
architectural result.

Distinguish implementation validation, simulator-model validation, and
scientific application evaluation. Do not present functional-simulator
evidence as RTL validation.

---

# Post-Implementation Policy Review

After the complete implementation and validation described by this prompt,
evaluate whether the resulting model needs an explicit result-promotion policy.
Base this review on the observed quantization, range, special-value, invalid
state, and application-level evidence. Do not introduce result promotion during
the implementation milestones without a separate researcher-approved semantic
contract.

## Researcher-approved post-implementation resolution

- Do not introduce a result-promotion policy at this stage.
- Keep any result tag above carrier-limited effective type T as an invalid
  internal state.
- Require `invalid_result_promotion_total` to remain zero in valid executions.
- Handle effective-type range failures using the approved overflow and
  underflow propagation rules rather than promotion.
- Reconsider result promotion only if later application evidence motivates a
  separate researcher-approved semantic contract.

---

# Milestone Reporting

At every milestone, report:

- files changed;
- semantic decision implemented;
- commands and tests executed;
- observed evidence;
- failed or unsupported cases;
- counter invariants;
- remaining limitations; and
- whether the evidence supports implementation validation or simulator-model
  validation.

Prefer small, reviewable changes. Do not create commits, run the complete
scientific matrix, or change the research scope without explicit researcher
approval.
```
