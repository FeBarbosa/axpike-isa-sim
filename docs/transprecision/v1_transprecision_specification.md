# Experimental Infrastructure Definition

## Objective

This phase defines version 1.0 of the transprecision infrastructure for AxPIKE.
The implementation uses Spike instruction macros to observe floating-point
register tags and uses the existing ADF/FlexFloat conversion helpers to classify
architectural FP32 and FP64 values.

The infrastructure classifies scalar FP32 and FP64 instructions for execution
using the smallest supported floating-point type compatible with their operands.
It preserves the architectural FP32 or FP64 register representation used by
Spike while associating each floating-point register with a tag that records the
classified type of its stored value.

The current extension is a functional-simulator model layered over Spike's
architectural arithmetic. Spike produces an FP32 or FP64 result, after which the
extension quantizes it to the effective type, optionally reduces it to a lower
candidate, and persists the selected carrier representation with its tag. This
is not equivalent to executing every intermediate step in a native E5M2 or
FP16 arithmetic unit.

Version 1.0 remains the historical exact-representability baseline. The current
policy version is `effective-type-quantization-v4`: it uses four type-based
protected-bit parameters and distinguishes operation-result
\(W\rightarrow T\) quantization from later \(T\rightarrow J\) reduction. The
SSCAD experiment-matrix redesign now contains 51 uniform saturated dynamic
configurations. The full-protection and no-protection endpoints have passed
reduced application validation, counter-invariant checks, and deterministic
replay. Reduced validation of the remaining 49 interior configurations is still
required before the complete scientific evaluation.

This phase is an implementation and instrumentation effort. The complete
active-mantissa demotion policy is defined separately in
[`active_mantissa_policy_contract.md`](active_mantissa_policy_contract.md), and
it does not predict RTL cost, performance, area, latency, or energy
consumption.

## Scope

Version 1.0 supports the following types in ascending selection precedence:

1. `E5M2`
2. `FP16`
3. `FP32`
4. `FP64`

Other candidate formats may be integrated after version 1.0. This initial set
was selected to simplify and accelerate development while preserving a strict
precedence in which each format has greater range and precision than the
previous format.

The implementation covers scalar FP32 and FP64 instruction paths that read from
or write to the floating-point register file, including loads, stores,
conversions, unary operations, comparisons, and arithmetic operations used by the
current LeNet execution study. Vector and quad-precision FP instructions are out
of scope for version 1.0.

## Register Type State

Each floating-point register must maintain a persistent type tag in addition to its architectural FP32 or FP64 value. The tag records the type assigned to the stored value and allows a promotion caused by one instruction to affect subsequent instructions.

The tag does not change the physical register width modeled by Spike. Values classified as `E5M2` or `FP16` continue to be stored using the corresponding architectural FP32 or FP64 encoding.

The tags are stored in the simulator processor state as an `FPR_TAGS` sidecar
array. They are reset with the architectural register state and are updated by
the transprecision-aware floating-point write macros.

## Exact Representability

A value is exactly representable in a candidate format when the following round
trip preserves its original architectural bit pattern:

```text
    **architectural FP32/FP64 value**
    -> conversion to the candidate format (FlexFloat)
    -> conversion back to the **architectural FP32/FP64 value**
```

Candidate formats are tested in selection-precedence order. If the bit patterns
differ, the next format is evaluated. NaN, infinity, signed zero, and subnormal
values follow the special-value policy defined below rather than relying only on
the finite-normal comparison rule.

## Instruction Classification

Dynamic type inference is implemented through instruction macros that read the
source register tags and write the destination tag. The implementation does not
currently add independent ADF wrapper callbacks for every register read/write;
instead, Spike instruction handlers are mapped to transprecision-aware read
observation and write-classification macros.

For instructions with two or more FP operands, the intended execution type is the largest operand tag. Operands tagged with smaller types are conceptually promoted to this common type before execution. Each instruction is counted exactly once using this intended execution type; individual wrapper accesses must not increment the instruction execution counter independently.

For FP-to-FP conversion instructions, such as `fcvt_d_s` and `fcvt_s_d`,
`effective_type_by_instruction` records the effective type of the FP source
operand, not the architectural destination type. For example, an `E5M2`-tagged
FP32 value converted by `fcvt_d_s` is counted as `fcvt_d_s/E5M2` even though the
architectural result is FP64. The destination tag is still selected from the
converted result using the normal transprecision result-classification policy.

The arithmetic operation itself continues to be executed by Spike using its
original architectural FP32 or FP64 implementation. Operation-result write
macros then quantize the architectural result to the carrier-limited effective
type before attempting any lower candidate.

The intended execution type is recorded once per instruction by the effective
type macros and is stored in the processor state as
`last_transprecision_effective_type` for focused internal validation. The same
observation path increments only `effective_type_by_instruction`; aggregate
type totals are derived by CSV consumers.

## Result Processing

Let \(W\) be the architectural destination carrier and \(T\) the operation
effective type limited so that \(T\leq W\). The architectural result is first
quantized to \(T\), retaining its representation in carrier \(W\). Lower
candidates \(J<T\) are then attempted independently from that same
\(T\)-quantized value. The first accepted candidate is stored; otherwise \(T\)
is retained.

The range-event and special-value rules take precedence over future generic
inexact-result promotion rules. An overflow, underflow, NaN, infinity, or
signed-zero case must first be handled according to the policy in the next
section; only remaining finite results follow the active finite-value
classification policy.

For finite non-zero results, the destination tag cannot exceed \(T\). Every
lower tag is recorded as a result-tag reduction, and reductions whose candidate
mask changes the \(T\)-quantized value form a changed subset. A tag above \(T\)
is an invalid state counted by a zero-expected invariant, not a supported
result-promotion policy.

Full type protection prevents additional lossy reduction below \(T\), but the
operation-result \(W\rightarrow T\) quantization can still change the architectural
result. Therefore this endpoint must not be described as identical to the
historical exact-representability baseline.

## Range Events And Special Values

Overflow or underflow relative to the carrier-limited effective type \(T\)
occurs when the architectural result is outside the dynamic range represented
by \(T\). These events are distinct from overflow or underflow of the original
FP32 or FP64 Spike operation.

If a finite architectural result exceeds the largest finite value of \(T\), the
model records overflow and propagates signed infinity. If a finite nonzero
architectural result is below the smallest normal value of \(T\), the model
records underflow and propagates the quantized \(T\) subnormal or signed zero.
E5M2 supports subnormals. Neither inferred event modifies architectural
`fflags`.

The infrastructure must distinguish:

- a result below the smallest normal value of the classified type;
- an exactly representable subnormal result;
- an inexact subnormal result;
- a result rounded to signed zero;
- an IEEE 754 underflow flag, when provided by the underlying implementation.

The counters describe simulator-inferred effective-type range events. They do
not claim bit-accurate reproduction of the exception flags of a physical
reduced-precision unit.

Special values use contextual tag assignment because their bit patterns do not
always identify the precision context that produced or introduced them.
The following rules are shared by the exact baseline and the dynamic
active-mantissa policy. The complete experiment contract is recorded in
[`active_mantissa_policy_contract.md`](active_mantissa_policy_contract.md).

For values produced by FP operations:

- finite non-zero results are quantized to the operation effective type and may
  then receive a lower accepted tag;
- `+0` and `-0` receive the smallest supported tag, currently `E5M2`;
- infinities and NaNs receive the carrier-limited effective type
  \(T=\min(t_{\mathrm{op}},W)\).

For architectural writes external to the FP ALU, including FP loads:

- finite non-zero values are classified using the active protected-bit policy
  with the architectural carrier as the source type; the accepted masked value
  and selected tag are stored together;
- `+0` and `-0` receive the smallest supported tag, currently `E5M2`;
- infinities and NaNs preserve their architectural bit pattern and receive
  `E5M2`.

Under the historical version-1 exact policy, the active protected widths are at
their exact endpoint and finite external values therefore receive the smallest
tag that represents the unchanged value exactly. The current configurable
policy generalizes that path and can propagate a masked external value.

The signed-zero rule is intentionally generalized across operation results and
external architectural writes: both `+0` and `-0` are exactly representable in
all supported formats, so version 1.0 tags them with the smallest available
type. Infinities and NaNs keep contextual precision information instead:
operation results preserve the type in which the event was inferred, while
external writes receive `E5M2` because no operation type is available. Their
sign and payload do not participate in format selection.

E5M2 and FP16 subnormals are supported by the simulated conversion path.
When a finite nonzero architectural result is below the minimum normal
magnitude of T, the model records underflow and propagates the T-quantized
subnormal or signed zero. Underflow therefore does not imply a change to zero.
Focused boundary tests establish this behavior for the implemented path.

Events attributed to `E5M2` or `FP16` are inferred from conversion of the architectural result. They must not be described as flags produced by a bit-accurate reduced-precision execution unit.

## Statistics

At the end of an application execution, the current infrastructure reports:

- the number and proportion of instructions classified as `E5M2`, `FP16`, `FP32`, and `FP64`;
- operand promotions caused by a smaller operand tag being promoted to the
  instruction's intended execution type;
- result quantization events from architectural carrier W to a lower effective
  type T, including changed, overflow, underflow, and changed-to-zero subsets;
- total result-tag reductions from T to a smaller destination tag and the
  value-changing subset;
- invalid result promotions, which must remain zero;
- generated result classes: finite, zero, infinity, and NaN;
- destination write-tag totals;
- lazy metadata recoveries and carrier fallbacks; and
- any operand observations that remain `UNCLASSIFIED` after recovery.

The transprecision CSV currently uses the columns:

```text
"Category","Instruction","From","To","Type","Class","Value"
```

The implemented categories are:

- `policy_version`: `effective-type-quantization-v4` with numeric version `4`;
- `policy_protected_bits`: the four protected widths indexed by source type
  `FP64`, `FP32`, `FP16`, and `E5M2`;
- `operand_unclassified_total`: number of operands still observed as
  `UNCLASSIFIED` after lazy recovery; covered scalar FP32/FP64 paths should
  report zero;
- `write_tag_total`: destination tag totals, including architectural writes such
  as FP loads and integer-to-FP moves/conversions;
- `operation_result_class_total`: finite, zero, infinity, and NaN result
  classes for operation-result write macros;
- `operand_promotion_from_to`: operand promotions from a smaller operand tag to the
  instruction's intended execution type;
- `result_quantization_total_from_to`: operation-result quantizations indexed
  by architectural carrier and lower effective type; identity pairs are not
  recorded;
- `result_quantization_changed_from_to`: the subset whose bits change during
  W-to-T quantization;
- `result_quantization_overflow_from_to`: finite architectural results outside
  the finite range of the effective type and propagated as signed infinity;
- `result_quantization_underflow_from_to`: finite nonzero architectural
  results below the smallest normal of the effective type, propagated as an
  effective-type subnormal or signed zero;
- `result_quantization_to_zero_from_to`: the underflow subset propagated as
  signed zero;
- `result_tag_reduction_total_from_to`: all lower result-tag selections;
- `result_tag_reduction_changed_from_to`: the subset whose candidate mask
  changes the effective-type-quantized value;
- `invalid_result_promotion_total`: any result tag above T; valid runs
  report zero;
- `external_write_class_total`: finite, zero, infinity, and NaN classes
  observed on external architectural writes;
- `external_write_masked_from_to`: external writes whose selected masked value
  differs from the incoming architectural value, indexed by carrier and
  selected format;
- `result_tag_reduction_to_zero_total`: nonzero T-quantized results changed to
  signed zero during later reduction;
- `external_write_masked_to_zero_total`: external writes changed to signed zero
  by the external-write policy;
- `lazy_reclassification_total`: FPR reads that found missing metadata and
  recovered a tag using exact classification without changing the value;
- `unclassified_fallback_total`: lazy recoveries for which exact
  classification did not return a supported tag and the architectural carrier
  was assigned conservatively;
- `fp64_load_nan_boxed_fp32_effective_total`: FP64 load results with an
  all-ones upper 32-bit word that were subsequently consumed by a typed FP32
  FPR read, counted once per loaded value without changing its bits or tag;
- `effective_type_by_instruction`: intended execution type distribution per
  instruction.

The active-mantissa experiment intentionally does not report candidate
rejection counts. Range, precision, and round-trip rejection are internal
reasons for continuing the candidate search; the scientific output is the
selected format and its promotion or demotion relative to the operation type.
Candidate rejection remains an internal search detail. Operation-result
overflow, underflow, and change-to-zero are reported explicitly for each
supported lower-type W-to-T transition.

Instruction execution counters and result-value counters are separate. A write classification must not increment an instruction execution counter.

## Key Terms

- **Operand promotion**: Counting an operand whose tag is smaller than the
  instruction's intended execution type because the instruction requires its FP
  operands to use a common type.
- **Invalid result promotion**: Observing a result tag above the effective
  operation type. This is not a normal policy action and valid runs report
  zero. A finite result outside the effective type's range instead propagates
  signed infinity as an internal model-overflow event.
- **Promotion**: General term for classifying or converting a value to a higher-precision floating-point type because the current type cannot represent it exactly or because an instruction requires its operands to use a common type.
  - *Range-driven promotion*: the current type lacks sufficient exponent range.
  - *Precision-driven promotion*: the significand lacks sufficient precision despite sufficient exponent range.
- **Result-tag reduction**: Selecting a result tag below the effective
  operation type. Total selections and the value-changing subset are counted
  separately.
- **Demotion**: Classifying or converting a value to a lower-precision
  floating-point type when the active classification policy permits the
  transition. The type-based policy clears the configured least-significant
  mantissa window and propagates the accepted candidate to later instructions.
- **Intended execution type**: The common type selected from the operand tags for classifying an instruction and interpreting its result.
- **Architectural type**: The FP32 or FP64 type determined by the original Spike instruction and register access.
  Architectural type also remains the physical representation stored by Spike in
  version 1.0.

## Validation Boundary

The focused suites validate the functional behavior and instrumentation of tag
propagation, instruction classification, effective-type quantization,
protected-bit masking, special-value tag policy, and current statistics
collection. The reduced full-protection and no-protection runs additionally
validate integrated value, tag, and counter propagation on the exercised LeNet
prefix. This evidence is not the complete scientific evaluation of the
type-based policy.

Because reduced operations are not executed by a bit-accurate reduced arithmetic
unit, conclusions about reduced hardware behavior must remain within this
limitation. The current directed results support claims about dynamic value
classification, tag propagation, instruction-type distribution, result
quantization, operand promotion, and later result reduction.

## Implementation Mapping

- Persistent per-FPR tags are stored in `state_t::FPR_TAGS`.
- Transient FP64-load NaN-boxing context is stored separately from precision
  tags and is cleared by any subsequent FPR write.
- Tags and transprecision counters are reset in `state_t::reset`.
- Effective-type selection, observation, architectural writes, and operation
  result writes are implemented in `riscv/decode_macros.h`.
- Exact representability is implemented in
  `riscv/transprecision_classification.cc` using the ADF/FlexFloat conversion
  helpers.
- The ADF `LowPrecisionSimulation/typeConvertion.c` helper was extended to
  support FP32-style conversion through exponent/mantissa pair `8,23`; this is
  required when classifying FP64 architectural values as representable in FP32.
- End-of-run CSV output is implemented in `adele/axpike_stats.cc`.
- Focused unit tests cover tag state, operand effective type, exact
  classification, write macros, instruction-level FP execution behavior, special
  values, and FP-to-FP conversion classification.

## Known Limitations Before The Next Experiment

- The finite-value active-mantissa classification core supports all five
  carrier/candidate transitions and command-line configuration. The
  experiment-matrix generator, lazy metadata recovery, and contextual
  special-value behavior are also implemented and covered by focused tests.
  Reduced application validation, counter-invariant checks, and deterministic
  replay passed for the full-protection and no-protection endpoints. The
  remaining 49 interior configurations in the 51-point uniform sweep have not
  yet passed this gate.
- Reduced arithmetic is not bit-accurate; Spike still computes the architectural
  FP32 or FP64 result.
- The version-1 exact baseline preserves the architectural result. The active
  mantissa policy instead propagates the accepted masked carrier value; it does
  not emulate candidate-format arithmetic.
- `write_tag_total` includes writes that are not instruction effective-type
  observations, such as FP loads and integer-to-FP writes. It must not be
  interpreted as an instruction execution counter.
- Zfinx/Zdinx paths bypass the FPR tag write path when they write integer
  registers instead of the floating-point register file. Version 1.0 is focused
  on the traditional floating-point register file path.
- Effective-type subnormal propagation is validated at helper and integration
  level, but this does not establish physical reduced-precision-unit behavior.
- The post-implementation review does not introduce a result-promotion policy.
  A tag above the carrier-limited effective type remains invalid and must keep
  `invalid_result_promotion_total` at zero. Any future promotion proposal
  requires a separate researcher-approved semantic contract.
