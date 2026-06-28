# Experimental Infrastructure Definition

## Objective

This phase defines version 1.0 of the transprecision infrastructure for AxPIKE. The implementation will reuse the existing ADF wrappers that intercept reads from and writes to the floating-point register file.

The infrastructure will classify scalar FP32 and FP64 instructions for execution using the smallest supported floating-point type compatible with their operands. It will preserve the architectural FP32 or FP64 register storage used by Spike while storing values that have been quantized according to the classified execution type.

This phase is an implementation and instrumentation effort. It does not evaluate the later research policy based on active mantissa bits and does not predict RTL cost, performance, area, latency, or energy consumption.

## Scope

Version 1.0 supports the following types in ascending selection precedence:

1. `E5M2`
2. `FP16`
3. `FP32`
4. `FP64`

Other reduced types may be integrated after version 1.0. This initial set was selected to simplify and accelerate development while preserving a strict precedence in which each type has greater range or precision than the previous type.

The implementation covers the scalar FP32 and FP64 ADF groups that read from or write to the floating-point register file. Instruction-specific handling for loads, stores, conversions, unary operations, comparisons, and arithmetic operations will be determined while mapping these groups to the implementation.

## Register Type State

Each floating-point register must maintain a persistent type tag in addition to its architectural FP32 or FP64 value. The tag records the type assigned to the stored value and allows a promotion caused by one instruction to affect subsequent instructions.

The tag does not change the physical register width modeled by Spike. Values classified or quantized as `E5M2` or `FP16` continue to be stored using the corresponding architectural FP32 or FP64 encoding.

The location and representation of the tags in the AxPIKE implementation will be determined through direct code investigation.

## Exact Representability

A value is exactly representable in a candidate type when the following round trip preserves its original architectural bit pattern:

```text
architectural FP32/FP64 value
    -> candidate reduced type
    -> architectural FP32/FP64 value
```

Candidate types are tested in selection-precedence order. If the bit patterns differ, the next type is evaluated. NaN, infinity, signed zero, and subnormal values follow the special-value policy defined below rather than relying only on the finite-normal comparison rule.

## Instruction Classification

Dynamic type inference and conversion hooks are applied at both `regbank_read` and `regbank_write`.

For instructions with two or more FP operands, the intended execution type is the largest operand tag. Operands tagged with smaller types are conceptually promoted to this common type before execution. Each instruction is counted exactly once using this intended execution type; individual wrapper accesses must not increment the instruction execution counter independently.

The arithmetic operation itself continues to be executed by Spike using its original architectural FP32 or FP64 implementation. Version 1.0 does not execute a bit-accurate `E5M2` or `FP16` arithmetic operation. Reduced-type behavior is inferred by quantizing and classifying the architectural result through the ADF write hook.

The mechanism used to collect operand tags, retain the current intended execution type, and finalize one classification per instruction will be determined through direct code investigation.

## Result Processing

The architectural FP32 or FP64 result produced by Spike is tested against the intended execution type.

The range-event and special-value rules take precedence over the generic inexact-result promotion rule. An overflow, underflow, NaN, infinity, or signed-zero case must first be handled according to the policy in the next section; only remaining finite results follow the exact or inexact processing below.

If the result is exactly representable in the intended execution type, it is stored using the corresponding architectural FP32 or FP64 encoding. The destination register receives the smallest supported tag that represents the result exactly. If this tag is smaller than the intended execution type, the transition is recorded as a demotion.

If conversion to the intended execution type is inexact:

1. The result rounded to the originally classified execution type is stored in the destination register using FP32 or FP64 encoding.
2. The smallest following type that can represent the unrounded architectural result exactly is selected as the destination tag.
3. The transition is recorded as a promotion.

This rule intentionally preserves the rounded result of the current operation while causing subsequent operations that consume the destination register to use the promoted tag.

Demotion therefore occurs when an exactly represented result receives a smaller tag than the intended execution type, according to the exact-representability and precedence rules.

## Range Events And Special Values

Overflow or underflow relative to the intended execution type occurs when the architectural result is outside the dynamic range representable by that classified type. These events are distinct from overflow or underflow of the original FP32 or FP64 Spike operation.

For example, if Spike produces a finite FP32 result but conversion to `E5M2` produces positive infinity, the destination register stores positive infinity using its FP32 encoding. The event is recorded as an `E5M2` range overflow.

The infrastructure must distinguish:

- a result below the smallest normal value of the classified type;
- an exactly representable subnormal result;
- an inexact subnormal result;
- a result rounded to signed zero;
- an IEEE 754 underflow flag, when provided by the underlying implementation.

If version 1.0 cannot reproduce the complete IEEE 754 underflow semantics for a reduced format, it must report the inferred event explicitly as a reduced-format range event rather than claiming bit-accurate reproduction of the IEEE 754 flag.

NaN, infinity, and signed zero generated by an instruction classified as a reduced type are propagated through the corresponding architectural FP32 or FP64 encoding and retain the classified type tag. Values that are already special when introduced through their original architectural representation retain their architectural type unless an explicit reduced-type rule is validated for them.

Subnormal handling initially follows the simplest behavior compatible with the existing AxPIKE, Spike, and FlexFloat conversion paths. The observed policy, including whether subnormals are preserved or flushed to zero, must be established by implementation-level validation.

Events attributed to `E5M2` or `FP16` are inferred from conversion of the architectural result. They must not be described as flags produced by a bit-accurate reduced-precision execution unit.

## Statistics

At the end of an application execution, the infrastructure must report:

- the number and proportion of instructions classified as `E5M2`, `FP16`, `FP32`, and `FP64`;
- the number of promotions and demotions, including source and destination types;
- the number of inexact results for each intended execution type;
- the number of inferred overflow and underflow events for each intended execution type;
- the number of generated infinities and NaNs for each intended execution type;
- the result classifications observed at `regbank_write`.

Instruction execution counters and result-value counters are separate. A write classification must not increment an instruction execution counter.

## Key Terms

- **Promotion**: Classifying or converting a value to a higher-precision floating-point type because the current type cannot represent it exactly or because an instruction requires its operands to use a common type.
  - *Range-driven promotion*: the current type lacks sufficient exponent range.
  - *Precision-driven promotion*: the significand lacks sufficient precision despite sufficient exponent range.
- **Demotion**: Classifying or converting a value to a lower-precision floating-point type when the exact-representability rule permits the transition.
- **Intended execution type**: The common type selected from the operand tags for classifying an instruction and interpreting its result.
- **Architectural type**: The FP32 or FP64 type determined by the original Spike instruction and register access.

## Validation Boundary

Version 1.0 validates the functional behavior and instrumentation of tag propagation, instruction classification, conversion, result quantization, event detection, and statistics collection. It is not a scientific evaluation of the later mantissa-bit-based promotion and demotion policy.

Because reduced operations are not executed by a bit-accurate reduced arithmetic unit, differences may arise from double rounding, reduced-format exception generation, NaN handling, and subnormal behavior. Conclusions about reduced hardware behavior must remain within this limitation.

## Implementation Investigation TODO

- [ ] Determine where and how persistent per-FPR type tags will be stored and reset.
- [ ] Determine how ADF wrappers will collect operand tags and retain the intended execution type until `regbank_write`.
- [ ] Map scalar FP32 and FP64 instruction groups to their read, execution-classification, and write behavior.
- [ ] Verify the existing FlexFloat conversion behavior for exact round trips, rounding, overflow, infinity, NaN, signed zero, and subnormals.
- [ ] Define focused validation cases for tag propagation, exact classification, multi-operand promotion, inexact-result promotion, range events, one-per-instruction counting, and special-value propagation.
- [ ] Map the implementation to the ADF model files, `LowPrecisionSimulation/typeConvertion.c`, `LowPrecisionSimulation/typeConvertion.h`, generated wrappers, the counter infrastructure, and the read/write interception path in `axpike_storage.cc`.
