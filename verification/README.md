# FP16 FlexFloat vs SoftFloat Verification

This directory contains the standalone validation work used to compare FlexFloat
against SoftFloat for FP16 behavior before moving the same idea back into AxPIKE.

## Goal

Validate FlexFloat configured as FP16 against Berkeley SoftFloat using direct
bit-exact comparisons on raw `uint16_t` FP16 encodings.

The current focus is outside AxPIKE:

- SoftFloat is the golden reference.
- FlexFloat is the implementation under test.
- The comparison is done in small directed tests and in a file-driven harness.

## What Exists Here

### `src/`

Source files for the verification work:

- `fp16_softfloat_ref.cpp`
- `fp16_flexfloat_ref.cpp`
- `fp16_file_compare.cpp`
- `fp16_input_generator.cpp`
- `fp16_lowprecision_hook_test.cpp`
- `fp32_add_axpike.c`

### `inputs/`

Test-vector files for the file-driven harness:

- `stage1_sanity_fp16.hex`
- `hook_only_fp32.hex`

Each non-comment line contains two FP16 values encoded as raw hex:

```text
3c00 4000
7e00 3c00
```

### `out/`

Generated binaries and run artifacts:

- `fp16_softfloat_ref`
- `fp16_flexfloat_ref`
- `fp16_file_compare`
- `fp32_add.riscv`
- `stage1_report.md`
- `stage2_report.md`
- `stage3_report.md`
- `file_driver_report.md`
- `stage3_full/`, the completed exhaustive FP16 sweep artifacts

### `scripts/`

Runner scripts for larger validation stages:

- `run_stage3.py`

## Hook-Only FP16 Lowprecision Test

The hook-only test is `src/fp16_lowprecision_hook_test.cpp`.

It exercises the same FP16 conversion path used by
`adele/adf/LowPrecisionFP16.cc`, but without running AxPIKE itself:

1. Read a raw FP32 bit pattern from an input file.
2. Convert it with SoftFloat using `FP32 -> FP16 -> FP32`.
3. Convert it with `typeSimulationSoftFloatFP16(value)` from the ADF helper.
4. Compare the two FP32 bit patterns.

The sample input file is `inputs/hook_only_fp32.hex`.

Current observed result:

- finite values match the SoftFloat round-trip
- the helper now matches the SoftFloat round-trip on the sample set with no
  mismatches

### `verification_plan.md`

The current prompt/plan used to guide the verification work.

## Current Build Setup

The repo already has:

- SoftFloat sources in `softfloat/`
- FlexFloat installed locally under `~/.local/include` and `~/.local/lib`
- AxPIKE linked against FlexFloat in the local build

For the standalone verification, the important part is the direct library usage:

- SoftFloat is compiled from the repository’s `softfloat/` sources.
- FlexFloat is used through the installed C++ wrapper `flexfloat.hpp`.

## Directed Reference Programs

Two small programs were created first:

- `fp16_softfloat_ref.cpp`
- `fp16_flexfloat_ref.cpp`

They were used to compare a few hand-picked FP16 additions such as:

- `1 + 2`
- `1.5 + 2.25`
- `65504 + 1`
- `0 + -0`
- `subnormal + subnormal`
- `inf + 1`
- `nan + 1`

These runs showed matching results for the normal cases and one NaN-sign
classification difference on a negative-NaN input.

## File-Driven Harness

The main reusable harness is `fp16_file_compare.cpp`.

What it does:

1. Reads raw `uint16_t` FP16 operand pairs from a text file.
2. Selects one operation at a time: `add`, `sub`, `mul`, or `div`.
3. Executes the operation in SoftFloat.
4. Executes the same operation in FlexFloat.
5. Compares the resulting FP16 encodings bit-for-bit.
6. Prints mismatches and a summary.

Example input file:

```text
3c00 4000
7bff 3c00
7e00 3c00
```

## Stage 1 Result

The Stage 1 sanity file is `inputs/stage1_sanity_fp16.hex`.

Results from the file-driven harness:

- `add`: 8 cases, 0 mismatches
- `sub`: 8 cases, 0 mismatches
- `mul`: 8 cases, 0 mismatches
- `div`: 8 cases, 1 mismatch

The only observed mismatch was a NaN-sign case:

- operands: `0x0000 / 0x8000`
- SoftFloat result: `0x7e00`
- FlexFloat result: `0xfe00`
- SoftFloat flags: `invalid`

This is the first mismatch category to carry forward into later comparison
logic.

## Input Generator

The stage-3 input generator is `fp16_input_generator.cpp`.

What it does:

1. Writes the same text format consumed by the comparison harness.
2. Emits raw `uint16_t` FP16 operand pairs in hexadecimal.
3. Supports operand range partitioning with `--a-start`, `--a-end`,
   `--b-start`, and `--b-end`.
4. Supports a `--limit` option for bounded sample generation.

This makes it practical to split the exhaustive sweep into smaller chunks before
running the full `65,536 x 65,536` space.

## Stage 3 Runner

The chunked exhaustive orchestration script is `scripts/run_stage3.py`.

It follows the current chunk schedule:

- `a_bits = 0x0000..0x00ff`, then `0x0100..0x01ff`, and so on up to
  `0xff00..0xffff`
- `b_bits = 0x0000..0xffff` for every chunk

For each chunk and each operation, the script:

1. Generates the chunk file with `fp16_input_generator`.
2. Runs `fp16_file_compare` on that file.
3. Records per-chunk logs and summaries.
4. Marks completed chunk files so reruns can skip them.

This is the structure used for the full exhaustive verification run.

The full-run command pattern is:

```bash
python3 verification/scripts/run_stage3.py \
  --ops add,sub,mul,div \
  --a-block-size 0100 \
  --b-start 0000 \
  --b-end ffff \
  --out-dir verification/out/stage3_full \
  --delete-inputs \
  --max-workers 4
```

The `--delete-inputs` option removes generated `.hex` chunk files after each
comparison. Completion is tracked by `.done` markers, so the run can be resumed
without keeping the large generated input files.

Resume behavior:

- a valid `.done` marker means the chunk has already been compared
- empty or unparsable `.done` markers are treated as incomplete
- a `.hex` file without a valid `.done` marker is regenerated before comparison

This was needed because an interrupted run left partial `.hex` files and empty
`.done` markers. The runner now repairs those cases during resume.

## Full Exhaustive Stage 3 Result

The exhaustive FlexFloat-vs-SoftFloat FP16 verification is complete for all
`uint16_t` operand pairs of `add`, `sub`, `mul`, and `div`.

Final runner summary:

```text
complete generated=1024 compared=1024 mismatches=12
```

The `mismatches=12` value counts chunks with at least one mismatch. The
underlying mismatch logs contain 20 individual cases. All of them are
invalid-operation NaN results where SoftFloat returns `0x7e00` and FlexFloat
returns `0xfe00`; no finite-result mismatch was observed.

Detailed scope, artifact paths, mismatch logs, and interpretation are recorded
in `out/stage3_report.md`.

## How The Pieces Fit Together

- `verification_plan.md` defines the intended validation strategy.
- `src/fp16_softfloat_ref.cpp` and `src/fp16_flexfloat_ref.cpp` provide small
  directed comparisons.
- `src/fp16_file_compare.cpp` provides the reusable file-driven harness.
- `inputs/stage1_sanity_fp16.hex` provides a reproducible input set.
- `out/` stores the binaries and reports produced so far.

## Next Step

The standalone FlexFloat-vs-SoftFloat FP16 comparison is now complete for the
basic arithmetic operations. The next step is to reuse the same input and
classification strategy against the AxPIKE lowprecision FP16 hook, so the
standalone behavior can be compared with the simulator-integrated behavior.
