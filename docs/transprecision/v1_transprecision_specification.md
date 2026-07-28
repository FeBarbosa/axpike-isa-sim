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

The current implementation is a classification, tag-propagation, and
instrumentation layer. It does not yet replace Spike's architectural FP32 or
FP64 arithmetic result with a bit-accurate reduced-precision result. Reduced
types are used to classify operands, select an intended execution type,
reclassify write-back values, and count promotion/demotion opportunities.

Version 1.0 remains the exact-representability baseline described by this
document. A subsequent implementation now supports the protected mantissa-bit
parameters for all five FP32/FP64 carrier-to-candidate transitions and
propagates accepted masked values through architectural write-back. This does
not change the definition of the version-1 baseline and is not yet the complete
active-mantissa experiment workflow.

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
original architectural FP32 or FP64 implementation. Version 1.0 does not execute
a bit-accurate `E5M2` or `FP16` arithmetic operation. Candidate-format behavior
is inferred by classifying the architectural result through the transprecision
write macros.

The intended execution type is recorded once per instruction by the effective
type macros and is stored in the processor state as
`last_transprecision_effective_type`. The same observation path increments the
per-instruction and per-type counters.

## Result Processing

The architectural FP32 or FP64 result produced by Spike is classified against
the supported transprecision types and the intended execution type.

The range-event and special-value rules take precedence over future generic
inexact-result promotion rules. An overflow, underflow, NaN, infinity, or
signed-zero case must first be handled according to the policy in the next
section; only remaining finite results follow the active finite-value
classification policy.

For finite non-zero results, the destination register receives the smallest
supported tag that represents the architectural result exactly. If this tag is
smaller than the intended execution type for the instruction that produced the
value, the transition is recorded as an exact result demotion.

The current implementation does not yet store a rounded reduced-precision value
when the architectural result is inexact for the intended execution type. It
preserves the Spike architectural result and records the destination tag selected
by the exact-representability classifier. This means version 1.0 currently
supports analysis of dynamic tags, instruction classification, operand
promotion, and exact result-demotion opportunities, but not a full numerical
simulation of reduced-precision arithmetic.

Result promotions are counted by operation type and selected result tag.
Candidate-specific causes such as exponent range or mantissa precision are not
reported separately because they are internal to candidate selection.

## Range Events And Special Values

Overflow or underflow relative to the intended execution type would occur when
the architectural result is outside the dynamic range representable by that
classified type. These events are distinct from overflow or underflow of the
original FP32 or FP64 Spike operation.

For example, if Spike produces a finite FP32 result and conversion to `E5M2`
would produce positive infinity, the event should be recorded as an `E5M2` range
overflow during focused validation. The full-application experiment does not
expose dedicated overflow or underflow counters for candidate formats.

The infrastructure must distinguish:

- a result below the smallest normal value of the classified type;
- an exactly representable subnormal result;
- an inexact subnormal result;
- a result rounded to signed zero;
- an IEEE 754 underflow flag, when provided by the underlying implementation.

If version 1.0 cannot reproduce the complete IEEE 754 underflow semantics for a
candidate format, it must report the inferred event explicitly as a
candidate-format range event rather than claiming bit-accurate reproduction of
the IEEE 754 flag. These event counters remain to be implemented before the next
experiment.

Special values use contextual tag assignment because their bit patterns do not
always identify the precision context that produced or introduced them.
The following rules are shared by the exact baseline and the dynamic
active-mantissa policy. The complete experiment contract is recorded in
[`active_mantissa_policy_contract.md`](active_mantissa_policy_contract.md).

For values produced by FP operations:

- finite non-zero results receive the smallest supported tag that represents the
  value exactly;
- `+0` and `-0` receive the smallest supported tag, currently `E5M2`;
- infinities and NaNs receive the intended execution type of the operation.

For architectural writes external to the FP ALU, including FP loads:

- finite non-zero values receive the smallest supported tag that represents the
  value exactly;
- `+0` and `-0` receive the smallest supported tag, currently `E5M2`;
- infinities and NaNs preserve their architectural bit pattern and receive
  `E5M2`.

The signed-zero rule is intentionally generalized across operation results and
external architectural writes: both `+0` and `-0` are exactly representable in
all supported formats, so version 1.0 tags them with the smallest available
type. Infinities and NaNs keep contextual precision information instead:
operation results preserve the type in which the event was inferred, while
external writes receive `E5M2` because no operation type is available. Their
sign and payload do not participate in format selection.

Subnormal handling initially follows the simplest behavior compatible with the existing AxPIKE, Spike, and FlexFloat conversion paths. The observed policy, including whether subnormals are preserved or flushed to zero, must be established by implementation-level validation.

Events attributed to `E5M2` or `FP16` are inferred from conversion of the architectural result. They must not be described as flags produced by a bit-accurate reduced-precision execution unit.

## Statistics

At the end of an application execution, the current infrastructure reports:

- the number and proportion of instructions classified as `E5M2`, `FP16`, `FP32`, and `FP64`;
- operand promotions caused by a smaller operand tag being promoted to the
  instruction's intended execution type;
- exact and masked result demotions from the intended execution type to a
  smaller destination tag;
- result promotions to a larger destination tag;
- generated result classes: finite, zero, infinity, and NaN;
- destination write-tag totals;
- lazy metadata recoveries and carrier fallbacks; and
- any operand observations that remain `UNCLASSIFIED` after recovery.

The transprecision CSV currently uses the columns:

```text
"Category","Instruction","From","To","Type","Class","Value"
```

The implemented categories are:

- `policy_protected_bits`: the five effective protected widths, indexed by
  architectural carrier and candidate format;
- `transprecision_effective_type_observations`: total number of effective-type
  observations;
- `last_transprecision_effective_type`: last observed effective type, useful as
  a debug/status row rather than as a numeric metric;
- `operand_unclassified_total`: number of operands still observed as
  `UNCLASSIFIED` after lazy recovery; covered scalar FP32/FP64 paths should
  report zero;
- `effective_type_total`: total observations by intended execution type;
- `write_tag_total`: destination tag totals, including architectural writes such
  as FP loads and integer-to-FP moves/conversions;
- `operation_result_class_total`: finite, zero, infinity, and NaN result
  classes for operation-result write macros;
- `operand_promotion_from_to`: operand promotions from a smaller operand tag to the
  instruction's intended execution type;
- `result_promotion_from_to`: cases where the result tag is larger than the
  intended execution type;
- `result_demotion_exact_from_to`: cases where the result tag is smaller and
  the selected value is bit-for-bit equal to the architectural result;
- `result_demotion_masked_from_to`: cases where the result tag is smaller and
  the propagated value differs from the architectural result;
- `external_write_class_total`: finite, zero, infinity, and NaN classes
  observed on external architectural writes;
- `external_write_masked_from_to`: external writes whose selected masked value
  differs from the incoming architectural value, indexed by carrier and
  selected format;
- `masked_to_zero_total`: nonzero values changed to signed zero by masking;
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
Candidate-format range and subnormal behavior remain subject to focused
implementation validation rather than dedicated full-application counters.

Instruction execution counters and result-value counters are separate. A write classification must not increment an instruction execution counter.

## Key Terms

- **Operand promotion**: Counting an operand whose tag is smaller than the
  instruction's intended execution type because the instruction requires its FP
  operands to use a common type.
- **Result promotion**: Classifying a result with a larger tag because the
  intended execution type cannot represent it according to the active
  classification policy.
- **Promotion**: General term for classifying or converting a value to a higher-precision floating-point type because the current type cannot represent it exactly or because an instruction requires its operands to use a common type.
  - *Range-driven promotion*: the current type lacks sufficient exponent range.
  - *Precision-driven promotion*: the significand lacks sufficient precision despite sufficient exponent range.
- **Result narrowing**: Current implemented counter for cases where the result
  tag is smaller than the intended execution type.
- **Demotion**: Classifying or converting a value to a lower-precision
  floating-point type when the active classification policy permits the
  transition. In version 1.0 this means exact representability. In the next
  experiment, it means exact representability after clearing the configured
  least-significant mantissa window, with the masked value propagated to later
  instructions.
- **Intended execution type**: The common type selected from the operand tags for classifying an instruction and interpreting its result.
- **Architectural type**: The FP32 or FP64 type determined by the original Spike instruction and register access.
  Architectural type also remains the physical representation stored by Spike in
  version 1.0.

## Validation Boundary

Version 1.0 validates the functional behavior and instrumentation of tag
propagation, instruction classification, conversion-based exact
representability, special-value tag policy, and current statistics collection.
It is not a scientific evaluation of the later mantissa-bit-based promotion and
demotion policy.

Because reduced operations are not executed by a bit-accurate reduced arithmetic
unit, conclusions about reduced hardware behavior must remain within this
limitation. The current results support claims about dynamic value
classification, tag propagation, instruction-type distribution, and observed
opportunities for operand promotion and result promotion/demotion.

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
  Reduced application validation and counter-invariant checks are still
  required before scientific evaluation.
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
- Subnormal behavior still needs focused validation before making claims about
  candidate-format underflow semantics.
