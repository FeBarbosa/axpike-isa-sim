You are a senior software verification engineer specialized in IEEE 754 floating-point validation.

Your task is to design and implement a validation framework that compares FlexFloat against Berkeley SoftFloat for FP16 arithmetic, without AxPIKE in the loop.

Goal:
- Validate FlexFloat configured as FP16 against SoftFloat FP16.
- Use this as the pre-AxPIKE reference verification step.

Current setup assumptions:
- SoftFloat is available in the repository under `softfloat/`.
- FlexFloat is installed locally and available through `#include <flexfloat.hpp>`.
- The installed FlexFloat build uses `FLEXFLOAT_ON_DOUBLE`.
- The installed FlexFloat build does not expose exception flags, so result-bit comparison is the primary pass/fail criterion.

Validation scope:
- FP16 addition
- FP16 subtraction
- FP16 multiplication
- FP16 division

For each operation:
- Exhaustively iterate over all `uint16_t` operand encodings.
- Interpret each operand as raw FP16 bits.
- Run the operation in SoftFloat.
- Run the same operation in FlexFloat.
- Compare the resulting FP16 encodings bit-for-bit.

Comparison policy:
- Primary comparison:

    softfloat_result_bits == flexfloat_result_bits

- Do not use numeric tolerance.
- Do not silently ignore NaN mismatches.
- Classify mismatches by category:
  - NaN payload difference
  - NaN sign difference
  - quiet/signaling NaN conversion difference
  - zero-sign difference
  - subnormal handling difference
  - overflow difference
  - underflow difference
  - rounding difference
  - invalid-operation difference
  - division-by-zero difference
  - generic bit mismatch

Special values to cover:
- positive zero
- negative zero
- normal numbers
- subnormal numbers
- infinities
- quiet NaNs
- signaling NaNs, if handled by both libraries

Flags:
- Record SoftFloat exception flags for every case.
- FlexFloat flags are optional and should only be compared if the local FlexFloat build is rebuilt with flag support.
- If FlexFloat flags are unavailable, document that limitation explicitly.

Test harness requirements:
- Standalone C or C++ driver.
- Deterministic exhaustive loops over `uint16_t` operand pairs.
- One operation at a time.
- Progress reporting.
- Mismatch logging.
- Checkpointing or resumability for long runs.
- Configurable output directory.
- Optional early stop after `N` mismatches.
- Optional partitioning of operand ranges for parallel execution.
- Batch output to reduce I/O overhead.

Suggested project structure:
- `softfloat/` for the golden reference implementation.
- `verification/src/fp16_softfloat_flexfloat_compare.cpp` for the reusable file-driven
  comparison harness.
- `verification/src/fp16_pair_input_generator.cpp` for deterministic input
  generation and chunking.
- `verification/scripts/run_fp16_exhaustive_compare.py` for resumable exhaustive orchestration.
- `verification/out/` or a user-configurable output directory for logs and
  summaries.

Verification passes:
1. Sanity cases
   - Test a few hand-picked values such as `1 + 2`, `1.5 + 2.25`, and `65504 + 1`.
2. Sampled sweep
   - Run a smaller, stratified or random subset to validate the harness.
3. Exhaustive sweep
   - Run `add`, then `sub`, then `mul`, then `div`.

Mismatch report:
- operation
- operand A bits in hex
- operand B bits in hex
- operand A classification
- operand B classification
- FlexFloat result bits in hex
- SoftFloat result bits in hex
- FlexFloat result classification
- SoftFloat result classification
- SoftFloat exception flags
- FlexFloat exception flags, if available
- rounding mode
- mismatch category

Summary report:
- total tested cases per operation
- total mismatches per operation
- mismatch rate
- mismatches grouped by category
- example mismatches per category
- counts of zeros, subnormals, normals, infinities, and NaNs
- SoftFloat flags observed
- FlexFloat flag availability
- known limitations

Implementation strategy:
- Use raw `uint16_t` loops.
- Avoid unnecessary conversions.
- Compare encodings rather than values.
- Batch logs.
- Support resumability.
- Keep the harness simple enough to run outside AxPIKE.

Expected output:
Provide a practical, implementation-oriented verification plan suitable for direct FlexFloat-vs-SoftFloat validation. Prioritize correctness, reproducibility, and debuggability.
