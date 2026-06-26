# Problem definition

Domain: Experimental Infrastructure Definition

This phase designs the initial version of the transprecision infrastructure for AxPIKE, based on the existing ADF wrappers that intercept reads from and writes to the floating-point register file.

Reduced-precision floating-point types are selected only when a value can be represented exactly, without rounding or quantization error. Otherwise, the value remains represented in its original floating-point type.

In this initial implementation, type selection is performed dynamically at read and write time, without maintaining any persistent metadata about the selected type. The infrastructure depicts the conversion flow between floating-point types for transprecision, but preserving the original storage representation and operation execution used by the simulator (FP64/FP32).

Although values may be treated as lower-precision types, the infrastructure does not physically change the storage representation; instead, it determines whether a value can be losslessly represented by a target floating-point - FP16 E5M2, F32 or FP64.

The primary goal of this implementation is to provide a functional foundation for validating dynamic type conversion in a transprecision environment. Once validated, this infrastructure will be used to explore and evaluate different type promotion and demotion policies.

For this first version of transprecision, type set and selection precedence is: `E5M2`, `FP16`, `FP32`, and `FP64`. Other reduced types will be integrated later; they are excluded from v1.0 to simplify and accelerate the first implementation.

# Key Terms

- **Promotion**: Classifying or converting a value to a higher-precision floating-point type when the current candidate type cannot represent it exactly, or when an operation requires operands to use a common type. For this version of transprecision, the architectural storage remains FP32 or FP64. There are two cases when promotion is necessary:
  - *Range-driven promotion*: the candidate type lacks sufficient exponent range.
  - *Precision-driven promotion*: its significand lacks sufficient precision, despite sufficient exponent range.   
- **Demotion**: Classifying a value as belonging to a lower-precision floating-point type only when conversion to that type and back to the architectural FP32 or FP64 type preserves the value exactly.

# Potential Limitations

The proposed transprecision simulation methodology, based on dynamic floating-point type selection during floating-point register reads and writes, may provide limited insight into how later processor design stages (e.g., RTL-level implementations) should be developed and its limitations.

Because the approach operates at the simulator level and abstracts away hardware implementation details for type selection and conversion, it may hide or simplify challenges that would arise in a real hardware implementation. These challenges include implementation complexity, performance impact, area overhead, additional latency, and energy consumption associated with supporting multiple floating-point representations and dynamic type transitions.

As a result, the current infrastructure should be viewed primarily as a mechanism for validating the functional behavior of dynamic type conversion policies rather than as a predictor of hardware cost or implementation feasibility.

# TODO

- [ ] Specify that dynamic type inference and conversion are applied on both `regbank_write` and `regbank_read`.
- [ ] Define the `regbank_read` multi-operand rule: when an instruction reads two or more FP operands, the operation must be marked as using the largest inferred type among those operands.
- [ ] State that v1.0 covers all ADF groups that read from or write to floating-point registers.
- [ ] Specify IEEE 754 special-value behavior: NaN, infinities, signed zero, subnormals, overflow, and underflow are propagated through the corresponding architectural FP32 or FP64 representation.
- [ ] Define validation criteria that separate implementation validation from later scientific evaluation of mantissa-bit-based promotion and demotion policies.
- [ ] Map the specification to implementation files, including the ADF model files, `LowPrecisionSimulation/typeConvertion.c`, `LowPrecisionSimulation/typeConvertion.h`, generated wrappers, and the read/write interception path in `axpike_storage.cc`.
