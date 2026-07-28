# Active-Mantissa Policy Contract

## Status and Scope

This document records the approved scientific definition of the
active-mantissa demotion policy for the AxPIKE transprecision experiment. It
defines the complete policy and experiment contract approved during the
policy-design sprints. No policy behavior should be inferred beyond this
contract.

The complete policy is not yet integrated into the experiment workflow. The
validated implementation provides a fixed per-run configuration object with
exact-baseline defaults and implements the complete protected-bit range for all
five FP32/FP64 carrier-to-candidate transitions. Every candidate mask is derived
independently from the original architectural result. When a candidate accepts,
the architectural register stores the selected masked value, so subsequent
instructions consume it.

The deterministic experiment-matrix generator, lazy metadata recovery, and
contextual special-value behavior are implemented and validated with focused
tests. Reduced application validation, counter-invariant checks, and
deterministic replay still precede scientific evaluation. Therefore, the
existence of the complete classification core, command-line configuration,
metadata recovery, and experiment matrix must not be interpreted as readiness
to run the complete experiment.

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

Spike nevertheless produces the result in architectural carrier \(W\), which
is FP32 for a single-precision instruction and FP64 for a double-precision
instruction. The masking policy inspects the explicit mantissa bits of \(W\).
The logical type \(t_{\mathrm{op}}\) is used to interpret operand promotions
and result promotions or demotions; it does not redefine the bit layout of the
architectural result.

Consider an architectural value \(x_W\) and a smaller candidate \(B\). Let

\[
    p_W = \operatorname{mantissaBits}(W), \qquad
    p_B = \operatorname{mantissaBits}(B)
\]

and define

\[
    d_{W\rightarrow B}=p_W-p_B.
\]

An active mantissa bit is a bit equal to one in the explicit architectural
mantissa field. The policy operates directly on this field and does not branch
on whether the encoded finite value is normal or subnormal.

## Protected and Ignorable Regions

For a transition \(W\rightarrow B\), the \(d_{W\rightarrow B}\) excess bits are
partitioned into:

- \(n\) most-significant protected bits; and
- \(k=d_{W\rightarrow B}-n\) least-significant ignorable bits.

The policy explores a consecutive ignorable window that always starts at the
least-significant end of the source mantissa. Bit position defines the policy:
an active bit in the protected region prevents the masked value from being
exactly representable in the candidate, whereas bits in the ignorable window
are cleared before candidate representability is evaluated. No population
count or numerical-error weighting is used.

## Masked Value and Candidate Acceptance

Let

\[
    x'_{B,k}=M_{W,k}(x_W)
\]

be the value obtained by clearing the \(k\) least-significant bits of the
explicit mantissa of \(x_W\), while preserving the remaining architectural
carrier fields.

Let \(C_{W\rightarrow B}\) denote conversion from \(W\) to \(B\), and let
\(C_{B\rightarrow W}\) denote conversion back to \(W\). Candidate \(B\) accepts
\(x_W\) under configuration \(k\) exactly when

\[
    \operatorname{bits}_W
    \left(
        C_{B\rightarrow W}\left(C_{W\rightarrow B}(x'_{B,k})\right)
    \right)
    =
    \operatorname{bits}_W(x'_{B,k}).
\]

The conversion round trip is the authoritative acceptance check. It verifies
that the masked value is exactly representable in the candidate according to
the conversion model used by the experiment. Consequently, candidate range,
normal/subnormal encoding effects, and the candidate conversion semantics are
not approximated by a separate mantissa-only range test.

Each candidate is derived independently from the original \(x_W\). A masked
value produced for a rejected candidate is discarded before the next candidate
is tested. If candidate \(B\) is accepted, the propagated value is
\(x'_{B,k}\). The original ignored bits do not participate in a subsequent
rounding decision. The architectural FP32 or FP64 register representation
stores \(x'_{B,k}\), and simulator-side precision metadata records \(B\). Later
instructions therefore consume the masked value.

If no smaller candidate accepts, the architectural carrier \(W\) is selected
without masking.

## Exact Baseline

For \(k=0\), no source mantissa bit is cleared:

\[
    x'_{B,0}=x_W.
\]

The candidate-acceptance equation therefore becomes the existing exact
representability round trip. Equivalently, \(k=0\) corresponds to
\(n=d_{W\rightarrow B}\). This configuration is the exact transprecision
baseline and introduces no policy-induced value change.

For \(k>0\), or equivalently \(n<d_{W\rightarrow B}\), progressively larger
least-significant regions may be cleared before exact candidate
representability is tested.

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
into signed zero, the zero is propagated and a distinct `masked_to_zero` event
is recorded.

Infinity and NaN are handled outside the finite-value masking rule:

- an externally introduced infinity or NaN preserves its architectural bit
  pattern and receives E5M2 metadata;
- an operation-generated infinity or NaN preserves Spike's architectural bit
  pattern and receives \(t_{\mathrm{op}}\) metadata;
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

Spike computes an architectural result \(y_W\) from the persistent masked
operands. Candidate classification uses \(W\), always derives candidate masks
from the original \(y_W\), and stores the first accepted \(y'_{B,k}\) together
with tag \(B\). Relative to \(t_{\mathrm{op}}\):

- \(B<t_{\mathrm{op}}\) is a result demotion;
- \(B=t_{\mathrm{op}}\) retains the operation type;
- \(B>t_{\mathrm{op}}\) is a result promotion.

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

The experiment compares three mappings from an experiment-level control to the
per-transition protected width.

### Proportional Protection

This is the principal combined-policy parameterization. For protection ratio
\(\rho\), with \(0\leq\rho\leq1\),

\[
    n_{W\rightarrow B}
    =
    \left\lceil
        \rho d_{W\rightarrow B}
    \right\rceil,
    \qquad
    k_{W\rightarrow B}
    =
    d_{W\rightarrow B}-n_{W\rightarrow B}.
\]

This mapping applies the same relative protection to excess regions of
different widths. The full-application experiment uses
\(\rho\in\{1,0.75,0.50,0.25,0\}\).

### Per-Transition Sensitivity

One \(n_{W\rightarrow B}\) is varied at a time. All other transitions remain at
their exact configurations, \(n=d\). Every integer protected width from zero
through \(d_{W\rightarrow B}\) is evaluated for each of:

- FP32 to FP16: \(n=0,\ldots,13\);
- FP32 to E5M2: \(n=0,\ldots,21\);
- FP64 to FP32: \(n=0,\ldots,29\);
- FP64 to FP16: \(n=0,\ldots,42\);
- FP64 to E5M2: \(n=0,\ldots,50\).

The resulting curves isolate application and format-assignment sensitivity to
each transition without a full factorial exploration.

### Global Absolute Protection

A single absolute \(n\) is applied to all transitions:

\[
    n_{W\rightarrow B}
    =
    \min(n,d_{W\rightarrow B}).
\]

This mapping intentionally exposes the effect of applying the same protected
bit count to excess regions of different widths. Every global integer
\(n=0,\ldots,50\) is evaluated.

Configurations are identified by their complete vector of effective protected
widths. Equivalent vectors across proportional, per-transition, and global
parameterizations are executed once. In particular, proportional protection
of 100%, global \(n=50\), and every fully protected per-transition endpoint
share the exact baseline. Proportional protection of 0% and global \(n=0\)
share the combined no-protection endpoint.

## Execution Configuration

Each AxPIKE execution receives the complete protected-width vector through:

```text
--transprecision-protected-bits=fp32-e5m2:n,fp32-fp16:n,fp64-e5m2:n,fp64-fp16:n,fp64-fp32:n
```

The five named transitions are mandatory when the option is present; their
textual order is irrelevant. Duplicate or unknown names, missing transitions,
non-decimal values, and widths outside the transition bounds are rejected.
When the option is absent, the exact-baseline vector `21,13,50,42,29` is used.
The configuration remains fixed throughout the execution.

The simulator accepts only the complete canonical vector. Proportional,
per-transition, and global parameterizations are expanded and deduplicated by
the external experiment generator. Every transprecision CSV records the five
effective protected widths using `policy_protected_bits` rows so that the
result identifies its own policy configuration.

The deterministic generator is:

```text
verification/scripts/generate_transprecision_experiment_matrix.py
```

It produces a JSON manifest with:

- one entry for each unique dynamic configuration;
- a stable identifier derived from the complete protected-width vector;
- the complete `--transprecision-protected-bits` argument;
- every proportional, per-transition, or global parameterization that maps to
  the configuration; and
- separate metadata for the three fixed controls.

Generate a manifest with:

```text
python3 verification/scripts/generate_transprecision_experiment_matrix.py \
  --output verification/out/transprecision-experiment-matrix.json
```

The output directory is for generated artifacts and is not the durable
experiment record. Final experiments must preserve the generator version,
manifest, commands, inputs, and summarized results in the traceability
structure selected for the submission.

## Experimental Objective

The experiment characterizes curves relating policy aggressiveness to:

- application accuracy;
- the distribution of assigned formats;
- exact and masked-value demotions; and
- format transitions required by the dynamic execution.

The experiment does not define an application-independent acceptable-accuracy
threshold. Conclusions must describe the observed trade-off for the evaluated
workload and configurations.

## Instrumentation Contract

The scientific transition metrics are:

- operand promotions, indexed by operand tag and operation type;
- result promotions, indexed by operation type and selected result tag;
- exact result demotions, for which the selected value equals the original
  architectural result bit-for-bit; and
- masked result demotions, for which the propagated selected value differs from
  the original architectural result.

Only transitions between different supported formats are emitted. Equal-format
pairs and pairs involving `UNCLASSIFIED` are not promotion or demotion events.
The previous aggregate result-narrowing metric is not retained because it would
duplicate the sum of exact and masked result demotions.

Candidate rejection counts are not scientific outputs. The search may reject
one or more candidate formats before selecting a result, but only the selected
format and its transition relative to the operation type are recorded.
Dedicated rejection counts by range, precision, or round-trip failure are
therefore outside the experiment.

External architectural writes have no operation type and are not described as
result promotions or demotions. Their selected tags contribute to the write-tag
distribution. When an external write propagates a changed masked value, the
event is recorded by architectural carrier and selected format.

Value-class totals, `masked_to_zero`, and uncovered-metadata events are
validation diagnostics rather than principal scientific transition metrics.
The `fp64_load_nan_boxed_fp32_effective_total` diagnostic counts FP64 loads
whose all-ones upper word was subsequently confirmed by a typed FP32 FPR read.
It counts at most once per loaded value and remains independent of the FP64
carrier class recorded at the load.

All unique configurations are first executed on a deterministic reduced MNIST
case for structural validation and then on the complete 10,000-image MNIST
test set for the reported accuracy curves. Deduplicating the approved vectors
produces 201 dynamic configurations. Including the original FP32/FP64 run and
the fixed FP16 and E5M2 controls produces 204 unique full-application runs. The
experiment generator must reproduce this count rather than rely on a manually
maintained run list.

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
- deterministic generation and deduplication of the experiment matrix;
- zero or individually explained `UNCLASSIFIED`, NaN, and infinity events;
- counter invariants and deterministic replay of a reduced LeNet run;
- traceability from commits, commands, inputs, and configurations to CSVs,
  tables, and figures.
