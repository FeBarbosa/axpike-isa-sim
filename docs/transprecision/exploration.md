# Problem definition

Domain: Experimental Infrastructure Definition

This phase designs the initial version of the transprecision infrastructure for AxPIKE, based on the existing ADF wrappers that intercept reads from and writes to the floating-point register file.

Reduced-precision floating-point types are selected only when a value can be represented exactly, without rounding or quantization error. Otherwise, the value remains represented in its original floating-point type.

In this initial implementation, type selection is performed dynamically at read and write time, without maintaining any persistent metadata about the selected representation. As a result, the infrastructure validates the conversion flow between floating-point types while preserving the original storage representation used by the simulator.

Although values may be treated as lower-precision types, the underlying storage format remains FP32 or FP64. The infrastructure does not physically change the storage representation; instead, it determines whether a value can be losslessly represented by a target floating-point format such as BF16 or E5M2.

The primary goal of this implementation is to provide a functional foundation for validating dynamic type conversion in a transprecision environment. Once validated, this infrastructure will be used to explore and evaluate different type promotion and demotion policies.

# Key Terms

- **Promotion**: Interpreting a value as belonging to a higher-precision floating-point type, without changing its underlying storage representation.
- **Demotion**: Interpreting a value as belonging to a lower-precision floating-point type, without changing its underlying storage representation, and only when the value can be represented exactly in that type.

In this initial implementation, demotions are performed only when the target type can represent the value exactly. Otherwise, the original representation is preserved.

# Potential Limitations

The proposed transprecision simulation methodology, based on dynamic floating-point type selection during floating-point register reads and writes, may provide limited insight into how later processor design stages (e.g., RTL-level implementations) should be developed.

Because the approach operates at the simulator level and abstracts away hardware implementation details, it may hide or simplify challenges that would arise in a real hardware implementation. These challenges include implementation complexity, performance impact, area overhead, additional latency, and energy consumption associated with supporting multiple floating-point representations and dynamic type transitions.

As a result, the current infrastructure should be viewed primarily as a mechanism for validating the functional behavior of dynamic type conversion policies rather than as a predictor of hardware cost or implementation feasibility.

# TODO

- [ ] Explicitly fix the v1.0 scope: no persistent per-register tags, no hardware-cost model, and no claim of reproducing the complete Carvalho/Linhares TFPU.
- [ ] Define the exact-conversion semantics: a value may be demoted only when the round trip `FP32/FP64 -> reduced type -> FP32/FP64` preserves the original value exactly.
- [ ] Document the initial transprecision type set and selection precedence: `E5M2`, `FP16`, `FP32`, and `FP64`. Other reduced types may be integrated later; they are excluded from v1.0 to simplify and accelerate the first implementation.
- [ ] Specify that dynamic type inference and conversion are applied on both `regbank_write` and `regbank_read`.
- [ ] Define the `regbank_read` multi-operand rule: when an instruction reads two or more FP operands, the operation must be marked as using the largest inferred type among those operands.
- [ ] State that v1.0 covers all ADF groups that read from or write to floating-point registers.
- [ ] Specify IEEE 754 special-value behavior: NaN, infinities, signed zero, subnormals, overflow, and underflow are propagated through the corresponding architectural FP32 or FP64 representation.
- [ ] Define validation criteria that separate implementation validation from later scientific evaluation of mantissa-bit-based promotion and demotion policies.
- [ ] Map the specification to implementation files, including the ADF model files, `LowPrecisionSimulation/typeConvertion.c`, `LowPrecisionSimulation/typeConvertion.h`, generated wrappers, and the read/write interception path in `axpike_storage.cc`.
