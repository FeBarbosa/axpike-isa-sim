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

This phase is an implementation and instrumentation effort. It does not evaluate the later research demotion/promotion policy based on active mantissa bits and does not predict RTL cost, performance, area, latency, or energy consumption.

## Scope

Version 1.0 supports the following types in ascending selection precedence:

1. `E5M2`
2. `FP16`
3. `FP32`
4. `FP64`

Other reduced types may be integrated after version 1.0. This initial set was selected to simplify and accelerate development while preserving a strict precedence in which each type has greater range and precision than the previous type.

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

A value is exactly representable in a candidate type when the following round trip preserves its original architectural bit pattern:

```text
    **architectural FP32/FP64 value**
    -> conversion to the candidate reduced type (FlexFloat)
    -> conversion back to the **architectural FP32/FP64 value**
```

Candidate types are tested in selection-precedence order. If the bit patterns differ, the next type is evaluated. NaN, infinity, signed zero, and subnormal values follow the special-value policy defined below rather than relying only on the finite-normal comparison rule.

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
a bit-accurate `E5M2` or `FP16` arithmetic operation. Reduced-type behavior is
inferred by classifying the architectural result through the transprecision
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
value, the transition is recorded by `result_narrow_from_to`.

The current implementation does not yet store a rounded reduced-precision value
when the architectural result is inexact for the intended execution type. It
preserves the Spike architectural result and records the destination tag selected
by the exact-representability classifier. This means version 1.0 currently
supports analysis of dynamic tags, instruction classification, operand
promotion, and result narrowing opportunities, but not a full numerical
simulation of reduced-precision arithmetic.

Promotion of result values caused by inexactness, exponent range overflow, or
mantissa precision loss is not yet counted separately. These counters are
required before the next experiment, where promotion/demotion policies will be
based on tolerated least-significant mantissa bits.

## Range Events And Special Values

Overflow or underflow relative to the intended execution type would occur when
the architectural result is outside the dynamic range representable by that
classified type. These events are distinct from overflow or underflow of the
original FP32 or FP64 Spike operation.

For example, if Spike produces a finite FP32 result and conversion to `E5M2`
would produce positive infinity, the event should be recorded as an `E5M2` range
overflow. The current implementation does not yet expose dedicated overflow or
underflow counters for reduced transprecision types.

The infrastructure must distinguish:

- a result below the smallest normal value of the classified type;
- an exactly representable subnormal result;
- an inexact subnormal result;
- a result rounded to signed zero;
- an IEEE 754 underflow flag, when provided by the underlying implementation.

If version 1.0 cannot reproduce the complete IEEE 754 underflow semantics for a
reduced format, it must report the inferred event explicitly as a reduced-format
range event rather than claiming bit-accurate reproduction of the IEEE 754 flag.
These event counters remain to be implemented before the next experiment.

Special values use contextual tag assignment because their bit patterns do not
always identify the precision context that produced or introduced them.

For values produced by FP operations:

- finite non-zero results receive the smallest supported tag that represents the
  value exactly;
- `+0` and `-0` receive the smallest supported tag, currently `E5M2`;
- infinities and NaNs receive the intended execution type of the operation.

For architectural writes external to the FP ALU, including FP loads:

- finite non-zero values receive the smallest supported tag that represents the
  value exactly;
- `+0` and `-0` receive the smallest supported tag, currently `E5M2`;
- infinities and NaNs receive the architectural type of the write.

The signed-zero rule is intentionally generalized across operation results and
external architectural writes: both `+0` and `-0` are exactly representable in
all supported formats, so version 1.0 tags them with the smallest available
type. Infinities and NaNs keep contextual precision information instead:
operation results preserve the type in which the event was inferred, while
external writes preserve the architectural source type.

Subnormal handling initially follows the simplest behavior compatible with the existing AxPIKE, Spike, and FlexFloat conversion paths. The observed policy, including whether subnormals are preserved or flushed to zero, must be established by implementation-level validation.

Events attributed to `E5M2` or `FP16` are inferred from conversion of the architectural result. They must not be described as flags produced by a bit-accurate reduced-precision execution unit.

## Statistics

At the end of an application execution, the current infrastructure reports:

- the number and proportion of instructions classified as `E5M2`, `FP16`, `FP32`, and `FP64`;
- operand promotions caused by a smaller operand tag being promoted to the
  instruction's intended execution type;
- result narrowing from the intended execution type to a smaller destination tag;
- generated result classes: finite, zero, infinity, and NaN;
- destination write-tag totals;
- operand observations that contain `UNCLASSIFIED`.

The transprecision CSV currently uses the columns:

```text
"Category","Instruction","From","To","Type","Class","Value"
```

The implemented categories are:

- `transprecision_effective_type_observations`: total number of effective-type
  observations;
- `last_transprecision_effective_type`: last observed effective type, useful as
  a debug/status row rather than as a numeric metric;
- `operand_unclassified_total`: number of observed operands whose tag was
  `UNCLASSIFIED`;
- `effective_type_total`: total observations by intended execution type;
- `write_tag_total`: destination tag totals, including architectural writes such
  as FP loads and integer-to-FP moves/conversions;
- `operation_result_class_total`: finite, zero, infinity, and NaN result
  classes for operation-result write macros;
- `promotion_from_to`: operand promotions from a smaller operand tag to the
  instruction's intended execution type;
- `result_narrow_from_to`: cases where the result tag is smaller than the
  intended execution type;
- `effective_type_by_instruction`: intended execution type distribution per
  instruction.

The following counters are not implemented yet and must be added before the
mantissa-bit promotion/demotion experiment:

- inexact result counts by intended execution type and destination type;
- reduced-format overflow counts by intended execution type;
- reduced-format underflow counts by intended execution type;
- result promotions caused by exponent range or mantissa precision loss;
- exact demotions versus approximate/tolerated demotions;
- values accepted by mantissa-bit tolerance;
- values rejected because the exponent is outside the candidate type range;
- values rejected because mantissa loss exceeds the configured tolerance.

Instruction execution counters and result-value counters are separate. A write classification must not increment an instruction execution counter.

## Key Terms

- **Operand promotion**: Counting an operand whose tag is smaller than the
  instruction's intended execution type because the instruction requires its FP
  operands to use a common type.
- **Result promotion**: Classifying a result with a larger tag because the
  intended execution type cannot represent it according to the active
  classification policy. This is not yet implemented as a dedicated counter.
- **Promotion**: General term for classifying or converting a value to a higher-precision floating-point type because the current type cannot represent it exactly or because an instruction requires its operands to use a common type.
  - *Range-driven promotion*: the current type lacks sufficient exponent range.
  - *Precision-driven promotion*: the significand lacks sufficient precision despite sufficient exponent range.
- **Result narrowing**: Current implemented counter for cases where the result
  tag is smaller than the intended execution type.
- **Demotion**: Classifying or converting a value to a lower-precision floating-point type when the active classification policy permits the transition. In version 1.0 this means exact representability; in the next experiment it will include the configured mantissa-bit tolerance.
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
opportunities for operand promotion/result narrowing.

## Implementation Mapping

- Persistent per-FPR tags are stored in `state_t::FPR_TAGS`.
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

- Dedicated inexact, overflow, underflow, and result-promotion counters are not
  implemented yet.
- The mantissa-bit tolerance policy is not implemented yet; version 1.0 uses
  exact representability for finite non-zero values.
- Reduced arithmetic is not bit-accurate; Spike still computes the architectural
  FP32 or FP64 result.
- The stored architectural result is not currently replaced by a rounded
  reduced-precision value.
- `write_tag_total` includes writes that are not instruction effective-type
  observations, such as FP loads and integer-to-FP writes. It must not be
  interpreted as an instruction execution counter.
- Zfinx/Zdinx paths bypass the FPR tag write path when they write integer
  registers instead of the floating-point register file. Version 1.0 is focused
  on the traditional floating-point register file path.
- Subnormal behavior still needs focused validation before making claims about
  reduced-format underflow semantics.
