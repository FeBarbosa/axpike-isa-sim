# Exhaustive FP16 SoftFloat vs FlexFloat Report

This report records the completed exhaustive standalone comparison between
SoftFloat FP16 and FlexFloat FP16 for the basic arithmetic operations.

## Scope

- Reference: Berkeley SoftFloat
- Implementation under test: FlexFloat configured as FP16
- Input encoding: raw `uint16_t` FP16 bit patterns
- Operations: `add`, `sub`, `mul`, `div`
- `a_bits`: `0x0000..0xffff`
- `b_bits`: `0x0000..0xffff`
- Chunk size: `0x0100` values of `a_bits`
- Chunks per operation: `256`
- Total chunks: `1024`
- Operand pairs per chunk: `16,777,216`
- Operand pairs per operation: `4,294,967,296`
- Total operation cases checked: `17,179,869,184`

## Command

```bash
python3 verification/scripts/run_fp16_exhaustive_compare.py \
  --ops add,sub,mul,div \
  --a-block-size 0100 \
  --b-start 0000 \
  --b-end ffff \
  --out-dir verification/out/fp16_exhaustive \
  --delete-inputs \
  --max-workers 4
```

The run was resumed after interruption. During resume, the runner was updated to
treat empty `.done` files as invalid and to regenerate `.hex` inputs that do not
have a valid `.done` marker.

## Artifacts

- Execution log: `verification/out/fp16_exhaustive/execution.log`
- Progress JSON: `verification/out/fp16_exhaustive/progress.json`
- Per-operation summaries:
  - `verification/out/fp16_exhaustive/summaries/add.txt`
  - `verification/out/fp16_exhaustive/summaries/sub.txt`
  - `verification/out/fp16_exhaustive/summaries/mul.txt`
  - `verification/out/fp16_exhaustive/summaries/div.txt`
- Mismatch logs:
  - `verification/out/fp16_exhaustive/logs/add_a7c00_a7cff.log`
  - `verification/out/fp16_exhaustive/logs/add_afc00_afcff.log`
  - `verification/out/fp16_exhaustive/logs/sub_a7c00_a7cff.log`
  - `verification/out/fp16_exhaustive/logs/sub_afc00_afcff.log`
  - `verification/out/fp16_exhaustive/logs/mul_a0000_a00ff.log`
  - `verification/out/fp16_exhaustive/logs/mul_a7c00_a7cff.log`
  - `verification/out/fp16_exhaustive/logs/mul_a8000_a80ff.log`
  - `verification/out/fp16_exhaustive/logs/mul_afc00_afcff.log`
  - `verification/out/fp16_exhaustive/logs/div_a0000_a00ff.log`
  - `verification/out/fp16_exhaustive/logs/div_a7c00_a7cff.log`
  - `verification/out/fp16_exhaustive/logs/div_a8000_a80ff.log`
  - `verification/out/fp16_exhaustive/logs/div_afc00_afcff.log`

No generated `.hex` input files were left after the completed run because
`--delete-inputs` was used.

## Final Status

The final execution log ended with:

```text
complete generated=1024 compared=1024 mismatches=12
```

The `mismatches=12` value counts chunks whose comparison returned a nonzero
status. The individual mismatch count from all mismatch logs is 20 cases.

| Operation | Mismatch chunks | Mismatch cases |
| --- | ---: | ---: |
| `add` | 2 | 2 |
| `sub` | 2 | 2 |
| `mul` | 4 | 8 |
| `div` | 4 | 8 |
| Total | 12 | 20 |

## Mismatch Pattern

Every mismatch has the same result pattern:

```text
soft=0x7e00
flex=0xfe00
flags=0x10
```

Meaning:

- SoftFloat returns `0x7e00`, a positive quiet NaN.
- FlexFloat returns `0xfe00`, a negative quiet NaN.
- SoftFloat raises `flags=0x10`, the invalid-operation flag.
- Both results are quiet NaNs; the bit-level difference is the NaN sign.

The mismatches are all invalid-operation NaN-result cases:

```text
add a=0x7c00 b=0xfc00 soft=0x7e00 flex=0xfe00 flags=0x10
add a=0xfc00 b=0x7c00 soft=0x7e00 flex=0xfe00 flags=0x10

sub a=0x7c00 b=0x7c00 soft=0x7e00 flex=0xfe00 flags=0x10
sub a=0xfc00 b=0xfc00 soft=0x7e00 flex=0xfe00 flags=0x10

mul a=0x0000 b=0x7c00 soft=0x7e00 flex=0xfe00 flags=0x10
mul a=0x0000 b=0xfc00 soft=0x7e00 flex=0xfe00 flags=0x10
mul a=0x7c00 b=0x0000 soft=0x7e00 flex=0xfe00 flags=0x10
mul a=0x7c00 b=0x8000 soft=0x7e00 flex=0xfe00 flags=0x10
mul a=0x8000 b=0x7c00 soft=0x7e00 flex=0xfe00 flags=0x10
mul a=0x8000 b=0xfc00 soft=0x7e00 flex=0xfe00 flags=0x10
mul a=0xfc00 b=0x0000 soft=0x7e00 flex=0xfe00 flags=0x10
mul a=0xfc00 b=0x8000 soft=0x7e00 flex=0xfe00 flags=0x10

div a=0x0000 b=0x0000 soft=0x7e00 flex=0xfe00 flags=0x10
div a=0x0000 b=0x8000 soft=0x7e00 flex=0xfe00 flags=0x10
div a=0x7c00 b=0x7c00 soft=0x7e00 flex=0xfe00 flags=0x10
div a=0x7c00 b=0xfc00 soft=0x7e00 flex=0xfe00 flags=0x10
div a=0x8000 b=0x0000 soft=0x7e00 flex=0xfe00 flags=0x10
div a=0x8000 b=0x8000 soft=0x7e00 flex=0xfe00 flags=0x10
div a=0xfc00 b=0x7c00 soft=0x7e00 flex=0xfe00 flags=0x10
div a=0xfc00 b=0xfc00 soft=0x7e00 flex=0xfe00 flags=0x10
```

## Interpretation

The exhaustive sweep did not find finite-result mismatches between SoftFloat
FP16 and FlexFloat FP16 for `add`, `sub`, `mul`, or `div`.

The only observed divergence is the sign bit chosen for the canonical quiet NaN
returned by invalid operations. SoftFloat canonicalizes these cases to
`0x7e00`, while FlexFloat returns `0xfe00`.

For numerical validation, this means FlexFloat matches SoftFloat across the
complete FP16 input space except for NaN sign/canonicalization in invalid
operations. For strict bit-for-bit validation, those cases must either be
reported explicitly, as done here, or normalized by a comparison policy that
treats quiet NaNs with different signs as equivalent.
