# Stage 2 Sampled Sweep Report

Input file:
- `verification/inputs/stage2_sample_fp16.hex`

Harness:
- `verification/out/fp16_file_compare`

Scope:
- FP16 `add`
- FP16 `sub`
- FP16 `mul`
- FP16 `div`

Sample size:
- 48 operand pairs per operation

Results:
- `add`: `total=48`, `mismatches=0`
- `sub`: `total=48`, `mismatches=1`
- `mul`: `total=48`, `mismatches=0`
- `div`: `total=48`, `mismatches=4`

Observed mismatch pattern:
- The mismatches are all NaN-sign differences on invalid operations.
- SoftFloat returned `0x7e00` in the sampled failures.
- FlexFloat returned `0xfe00` in the sampled failures.
- SoftFloat flags were `0x10` (`invalid`) in each printed mismatch.

Examples:
- `sub`: `0x7c00 - 0x7c00`
- `div`: `0x0000 / 0x0000`
- `div`: `0x0000 / 0x8000`
- `div`: `0x8000 / 0x8000`
- `div`: `0x7c00 / 0x7c00`

Interpretation:
- The sampled harness is stable and reproduces the same NaN sign behavior
  observed in the directed sanity stage.
- Addition and multiplication matched on this sampled set.
- Subtraction and division exercised invalid-operation cases that expose the
  NaN sign convention difference.
