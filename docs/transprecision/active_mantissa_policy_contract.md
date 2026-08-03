# Active-Mantissa Policy Contract

## Status and Scope

This document records the approved scientific definition of the
active-mantissa demotion policy for the AxPIKE transprecision experiment. It
defines the complete policy and experiment contract approved during the
policy-design sprints. No policy behavior should be inferred beyond this
contract.

The scoped execution and analysis protocol is maintained separately in
[`../experiments/sscad2026_uniform_n_experiment_protocol.md`](../experiments/sscad2026_uniform_n_experiment_protocol.md).

The operation-result path now implements effective-type quantization followed
by optional type-based reduction. Spike first computes in architectural carrier
\(W\). The result is then quantized to the operation effective type \(T\), and
all lower candidates are derived independently from that \(T\)-quantized value.
The accepted value and its tag are stored together, so subsequent instructions
consume the selected representation.

The configuration, metadata, counter, and CSV contracts are implemented and
validated with focused tests. The full-protection and no-protection endpoints
have also passed reduced application validation, counter-invariant checks, and
deterministic replay. The SSCAD experiment matrix now contains 51 dynamic
configurations under one uniform saturated parameterization. The remaining 49
interior configurations must pass the same reduced-validation gate before the
complete scientific evaluation.

## Formats and Ordering

The supported formats, from smallest to largest, are:

| Format | Exponent bits | Explicit mantissa bits | Exponent bias |
| --- | ---: | ---: | ---: |
| E5M2 | 5 | 2 | 15 |
| FP16 | 5 | 10 | 15 |
| FP32 | 8 | 23 | 127 |
| FP64 | 11 | 52 | 1023 |

E5M2 and FP16 use IEEE-like encodings: an all-zero exponent encodes zero or a
subnormal, and an all-one exponent encodes infinity or NaN. FP32 and FP64 use
their standard IEEE-754 encodings. The effective behavior of the FlexFloat
configurations must be validated against these assumptions.

Candidate formats are evaluated from smallest to largest. For an architectural
FP32 carrier, the order is E5M2, FP16, and FP32. For an architectural FP64
carrier, the order is E5M2, FP16, FP32, and FP64. The architectural carrier is
the mandatory fallback.

## Operation Type and Architectural Carrier

The logical operation type and the representation inspected by the policy are
distinct.

For an instruction with operand metadata \(t_1,\ldots,t_m\), the logical
operation type is

\[
    t_{\mathrm{op}}=t_{\mathrm{eff}}=\max(t_1,\ldots,t_m).
\]

The policy contract uses \(t_{\mathrm{op}}\); the current instrumentation and
paper also use the name effective type, \(t_{\mathrm{eff}}\), for the same
quantity.

Spike nevertheless produces the initial result in architectural carrier \(W\),
which is FP32 for a single-precision instruction and FP64 for a double-precision
instruction. The effective type is limited to the destination carrier:
\(T=\min(t_{\mathrm{op}},W)\). The architectural result \(y_W\) is quantized to
\(T\), producing \(y_T\) represented in the original carrier.

For a smaller candidate \(J<T\), let

\[
    a = \operatorname{mantissaBits}(T), \qquad
    b = \operatorname{mantissaBits}(J)
\]

and define

\[
    d_{T\rightarrow J}=a-b,\qquad
    \operatorname{effectiveN}_{T\rightarrow J}=\min(n_T,d_{T\rightarrow J}),
    \qquad
    k_{T\rightarrow J}=d_{T\rightarrow J}
      -\operatorname{effectiveN}_{T\rightarrow J}.
\]

An active mantissa bit is a bit equal to one in the explicit architectural
mantissa field. The policy operates directly on this field and does not branch
on whether the encoded finite value is normal or subnormal.

## Protected and Ignorable Regions

For a transition \(T\rightarrow J\), the \(d_{T\rightarrow J}\) excess bits are
partitioned into:

- \(\operatorname{effectiveN}_{T\rightarrow J}\) most-significant protected
  bits; and
- \(k_{T\rightarrow J}\) least-significant ignorable bits.

The configured protection is attached to source type \(T\), not independently
to every transition. The canonical vector is
`FP64:0..50, FP32:0..21, FP16:0..8, E5M2:0`. In a wider carrier, the ignorable
window begins at the carrier offset corresponding to the least-significant bit
of the simulated \(T\) mantissa. Bit position defines the policy:
an active bit in the protected region prevents the masked value from being
exactly representable in the candidate, whereas bits in the ignorable window
are cleared before candidate representability is evaluated. No population
count or numerical-error weighting is used.

## Masked Value and Candidate Acceptance

Let

\[
    y'_{J,k}=M_{T,k}(y_T)
\]

be the value obtained by clearing the candidate-relative \(k\) bits in the
active \(T\) mantissa region while preserving the remaining carrier fields.

Let \(C_{W\rightarrow J}\) denote conversion from the carrier representation
to \(J\), and let \(C_{J\rightarrow W}\) denote conversion back to \(W\).
Candidate \(J\) accepts exactly when

\[
    \operatorname{bits}_W
    \left(
        C_{J\rightarrow W}\left(C_{W\rightarrow J}(y'_{J,k})\right)
    \right)
    =
    \operatorname{bits}_W(y'_{J,k}).
\]

The conversion round trip is the authoritative acceptance check. It verifies
that the masked value is exactly representable in the candidate according to
the conversion model used by the experiment. Consequently, candidate range,
normal/subnormal encoding effects, and the candidate conversion semantics are
not approximated by a separate mantissa-only range test.

Each candidate is derived independently from the original \(y_T\). A masked
value produced for a rejected candidate is discarded before the next candidate
is tested. If candidate \(J\) is accepted, the propagated value is
\(y'_{J,k}\). The original ignored bits do not participate in a subsequent
rounding decision. The architectural FP32 or FP64 register representation
stores \(y'_{J,k}\), and simulator-side precision metadata records \(J\). Later
instructions therefore consume the masked value.

If no smaller candidate accepts, \(T\) and \(y_T\) are retained.

## Exact Baseline

For every lower candidate with \(k=0\), no additional \(T\)-mantissa bit is
cleared:

\[
    y'_{J,0}=y_T.
\]

The candidate-acceptance equation therefore becomes an exact-representability
test relative to the already quantized \(y_T\). Full type protection prevents
additional lossy reduction below \(T\); it does not make the complete path
identical to architectural FP32/FP64 execution because \(W\rightarrow T\)
quantization can still change the result.

For \(k_{T\rightarrow J}>0\), progressively larger least-significant regions
may be cleared before exact candidate representability is tested.

## Rounding and Exception State

All evaluated architectural floating-point operations use RISC-V
round-to-nearest, ties-to-even (RNE). Instructions with a static rounding field
use RNE directly; instructions using dynamic rounding execute with the `frm`
field configured as RNE.

Mantissa masking is bit clearing, not IEEE-754 rounding, and it cannot create a
rounding carry. Any RNE carry produced by Spike is already present in the
architectural result before classification. An accepted masked value is exactly
representable in its candidate, so the candidate round trip must not change it.

The guest-visible `fflags` remain those produced by Spike's architectural
operation. Internal masking and candidate round trips do not modify them and
do not use `inexact` or any other architectural exception flag to select a
format. Policy events are recorded using simulator-side counters. Consequently,
the experiment does not claim that `fflags` reproduce a physical E5M2 or FP16
unit.

## Zero, Infinity, and NaN

Existing positive and negative zero values are evaluated in candidate order and
select E5M2 while preserving their sign. If masking changes a nonzero value
into signed zero, the zero is propagated and attributed either to W-to-T
quantization or to the later result-tag-reduction stage.

Infinity and NaN are handled outside the finite-value masking rule:

- an externally introduced infinity or NaN preserves its architectural bit
  pattern and receives E5M2 metadata;
- an operation-generated infinity or NaN preserves Spike's architectural bit
  pattern and receives carrier-limited effective-type metadata
  \(T=\min(t_{\mathrm{op}},W)\);
- NaN sign and payload do not participate in format selection, and the
  experiment does not claim candidate-format NaN-payload equivalence.

External and operation-generated infinity and NaN observations are counted
separately.

## Metadata Recovery and Operand Selection

Every FPR write must assign precision metadata. `UNCLASSIFIED` is restricted to
initialization or evidence of an uncovered path. If an instruction reads an
`UNCLASSIFIED` FPR, the simulator performs exact \(k=0\) classification without
changing the value, updates the metadata, and records a
`lazy_reclassification` event. If classification still cannot assign a
supported tag, the architectural carrier is used as a conservative fallback
and an `unclassified_fallback` event is recorded. No operand reaches
\(t_{\mathrm{op}}\) selection while still unclassified.

Recovery occurs on the typed FP32 or FP64 FPR read so the instruction's
architectural carrier is explicit. It always uses the exact policy, regardless
of the dynamic protected-width vector active for new writes. Recovery changes
only simulator-side metadata: it neither masks the stored value nor records a
new architectural write.

Operands with tags smaller than \(t_{\mathrm{op}}\) are counted as conceptual
promotions. Equal-format operands are not promotions. Because the format order
is precision-inclusive, promotion does not numerically change a stored
operand. FPR reads do not remask or reclassify values that already have valid
metadata.

## Result Selection and Persistence

Spike computes an architectural result \(y_W\) from the persistent selected
operands. Operation writeback quantizes it to \(y_T\), derives all lower
candidates from \(y_T\), and stores the first accepted \(y'_{J,k}\) with tag
\(J\). A valid result satisfies \(J\leq T\). \(J<T\) is a result reduction;
\(J=T\) retains the effective type. \(J>T\) is an invalid state, not a normal
promotion policy.

The post-implementation review concluded that the current experiment will not
introduce an explicit result-promotion policy. A finite result outside the
dynamic range of \(T\) follows the defined overflow or underflow behavior; it
does not select a tag above \(T\). `invalid_result_promotion_total` therefore
remains a zero-expected invariant. Promotion may be reconsidered only if later
application evidence motivates a separate researcher-approved semantic
contract.

The stored masked value is consumed by later instructions. FP32 writes preserve
Spike's architectural NaN-boxing mechanism. The metadata is simulator-only and
is not stored in memory.

Loads, integer-to-FP conversions, and raw integer-to-FPR moves classify their
architectural results and store the accepted masked values and metadata.
FP32/FP64 conversion instructions use the source operand tag as
\(t_{\mathrm{op}}\), execute Spike's architectural conversion, and classify
the result in the destination carrier. Stores neither reclassify the FPR nor
change its tag; they write the persistent architectural value to memory.

An FP64 load whose upper 32 bits are all ones is also a structurally valid
NaN-boxed FP32 candidate. This property does not change the FP64 carrier
classification, stored bits, or precision tag. The simulator records transient
per-FPR load context and confirms it only when a typed FP32 read consumes that
FPR. Confirmation is counted once per loaded value; subsequent FP32 reads do
not recount it, FP64 reads do not confirm or clear it, and any FPR overwrite
clears the context. This context is not stored in memory and does not claim to
recover metadata from a preceding store.

## Experimental Parameterizations

The canonical vector order is `FP64, FP32, FP16, E5M2`, with maximum protected
widths `50, 21, 8, 0`.

The SSCAD experiment uses one integer parameter, `n`, uniformly for every
supported source type. It evaluates every integer from 0 through 50. Because
the types have different maximum protected widths, the effective value for a
type is `min(n, maximum_for_type)`. This gives the vector
`min(n,50), min(n,21), min(n,8), 0` in canonical order.

The 51 values of `n` produce 51 distinct dynamic configurations. There is no
proportional sweep, per-type sweep, factorial combination, or alias
deduplication in the scoped SSCAD experiment. The saturation points divide the
curve into three regimes: all variable-width types change for `n` from 0
through 8; FP64 and FP32 change for `n` from 9 through 21; and only FP64 changes
for `n` from 22 through 50. E5M2 has zero configurable protected bits and is
therefore unchanged throughout the sweep.

The endpoint at `n=0` is the combined no-protection dynamic policy. The
endpoint at `n=50` is the full-protection vector `50,21,8,0`. Full protection
can still perform \(W\rightarrow T\) quantization, so it is not the original
architectural FP32/FP64 control.

One original FP32/FP64 result is retained as an external reference control. It
will be recovered from repository history rather than generated by the dynamic
matrix. Its commit, executable, application and input hashes, command, and
output provenance must be established before it is compared with the 51
dynamic points. Fixed-format FP16 and E5M2 ADF runs are outside the scoped
SSCAD matrix.

The generated manifest uses schema version 3, identifies policy
`effective-type-quantization-v4` with numeric version 4, and includes all four
effective protected widths and the originating uniform `n` in every dynamic
configuration. It records 51 planned new executions and the historical control
as a provenance-pending external reference, for 52 planned evaluation points.

## Execution Configuration

Each AxPIKE execution receives the complete protected-width vector through:

```text
--transprecision-type-protected-bits=fp64:n,fp32:n,fp16:n,e5m2:0
```

The four named types are mandatory when the option is present; their textual
order is irrelevant. Duplicate or unknown names, missing types, non-decimal
values, and widths outside the type bounds are rejected.
When the option is absent, the full-protection vector `50,21,8,0` is used.
The configuration remains fixed throughout the execution.

Every transprecision CSV records policy
`effective-type-quantization-v4`, numeric version `4`, and the four type
parameters using `policy_protected_bits` rows. Consumers must reject older
policy versions rather than reinterpret them.

## Experimental Objective

The experiment characterizes curves relating policy aggressiveness to:

- application accuracy;
- the distribution of assigned formats;
- \(W\rightarrow T\) quantizations for lower effective types, including
  changed results, overflow, and underflow;
- total and value-changing \(T\rightarrow J\) result-tag reductions; and
- format transitions required by the dynamic execution.

The experiment does not define an application-independent acceptable-accuracy
threshold. Conclusions must describe the observed trade-off for the evaluated
workload and configurations.

## Instrumentation Contract

The instrumentation metrics are:

- operand promotions, indexed by operand tag and operation type;
- `result_quantization_total_from_to`, partitioned by \(W\) and lower \(T\);
- `result_quantization_changed_from_to`, the subset whose carrier bits change;
- explicit overflow events propagated as signed infinity without changing
  architectural exception flags;
- explicit underflow events propagated as an effective-type subnormal or
  signed zero, also without changing architectural exception flags;
- `result_tag_reduction_total_from_to`, for every \(J<T\) selection; and
- `result_tag_reduction_changed_from_to`, the subset whose candidate mask
  changes the propagated value.

Identity \(W=T\) result paths are not quantization events and are not exported.
Effective-type totals are derived exclusively from
`effective_type_by_instruction`.

`invalid_result_promotion_total` replaces the former result-promotion matrix.
It counts any impossible \(J>T\) state and must remain zero. It is a validation
invariant, not a supported policy outcome.

Candidate rejection counts are not scientific outputs. The search may reject
one or more candidate formats before selecting a result, but only the selected
format and its transition relative to the operation type are recorded.
Dedicated rejection counts by range, precision, or round-trip failure are
therefore outside the experiment.

External architectural writes have no operation type and are not described as
result promotions or demotions. Their selected tags contribute to the write-tag
distribution. When an external write propagates a changed masked value, the
event is recorded by architectural carrier and selected format.

Value-class totals, `result_tag_reduction_to_zero_total`,
`external_write_masked_to_zero_total`, and uncovered-metadata events are
validation diagnostics rather than principal scientific transition metrics.
The `fp64_load_nan_boxed_fp32_effective_total` diagnostic counts FP64 loads
whose all-ones upper word was subsequently confirmed by a typed FP32 FPR read.
It counts at most once per loaded value and remains independent of the FP64
carrier class recorded at the load.

All approved unique configurations must first execute on a deterministic
reduced MNIST case for structural validation. Full scientific evaluation
remains blocked until those reduced validations pass under policy version 4.

## Instruction and Claim Scope

The experiment covers scalar FP32 and FP64 instructions that use the
traditional FPR and execute in the LeNet workload: loads, stores, unary,
binary, and ternary operations, FMA, comparisons, integer/FP conversions,
FP32/FP64 conversions, and integer/FPR moves.

Vector instructions, FP128, Zfinx/Zdinx, architectural FP16 instructions,
BF16, and E4M3 are outside the experiment. Hardware area, energy, latency, and
performance claims are also outside scope.

Application accuracy is measured primarily by direct argmax over LeNet logits.
Softmax is a separate experiment and does not contribute to the principal
policy curves.

## Implementation and Validation Requirements

Before dynamic-policy results are used, focused validation must demonstrate:

- correct mask construction for every carrier/candidate pair;
- exact equivalence between \(k=0\) and the version-1 round-trip baseline;
- independent candidate attempts derived from the same architectural input;
- persistent storage and subsequent consumption of the accepted masked value;
- correct operation, result-transition, special-value, and metadata-recovery
  counters;
- preservation of architectural RNE and `fflags` behavior;
- deterministic generation of all 51 uniform saturated configurations;
- zero or individually explained `UNCLASSIFIED`, NaN, and infinity events;
- counter invariants and deterministic replay of a reduced LeNet run;
- traceability from commits, commands, inputs, and configurations to CSVs,
  tables, and figures.
