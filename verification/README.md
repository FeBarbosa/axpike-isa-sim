# FP16 FlexFloat vs SoftFloat Verification

This directory contains the standalone validation work used to compare FlexFloat
against SoftFloat for FP16 behavior before moving the same idea back into AxPIKE.

## Goal

Validate FlexFloat configured as FP16 against Berkeley SoftFloat using direct
bit-exact comparisons on raw `uint16_t` FP16 encodings.

The current focus is outside AxPIKE:

- SoftFloat is the golden reference.
- FlexFloat is the implementation under test.
- The comparison is done with a reusable file-driven harness and chunked
  exhaustive runs.

## What Exists Here

### `src/`

Source files for the verification work:

- `fp16_softfloat_flexfloat_compare.cpp`
- `fp16_pair_input_generator.cpp`
- `lowprecision_fp16_fp32_hook_compare.cpp`
- `lowprecision_fp16_fp64_hook_compare.cpp`
- `lowprecision_flexfloat_fp64_hook_compare.cpp`

### `inputs/`

Test-vector files for the file-driven harness:

- `fp16_sanity_pairs.hex`
- `fp16_sample_pairs.hex`
- `lowprecision_fp32_values.hex`
- `lowprecision_fp64_values.hex`

The FP16 pair files contain two FP16 values per non-comment line:

```text
3c00 4000
7e00 3c00
```

The hook-only files contain one raw floating-point value per non-comment line:
`lowprecision_fp32_values.hex` uses 8 hex digits and `lowprecision_fp64_values.hex` uses 16 hex
digits.

### `reports/`

Summary reports kept under version control:

- `fp16_exhaustive_softfloat_flexfloat.md`

### `out/`

Generated binaries and run artifacts:

- `fp16_softfloat_flexfloat_compare`
- `fp16_exhaustive/`, the completed exhaustive FP16 sweep artifacts

Generated `.csv` files and `verification/out/` binaries/logs are intentionally
ignored by the repository. Keep reproducible source files, input vectors, and
summary Markdown reports under version control; regenerate large or transient
outputs locally.

### `scripts/`

Runner scripts for larger validation stages:

- `run_fp16_exhaustive_compare.py`

## Hook-Only FP16 Lowprecision Test

The hook-only test is `src/lowprecision_fp16_fp32_hook_compare.cpp`.

It exercises the same FP16 conversion path used by
`adele/adf/LowPrecisionFP16.cc`, but without running AxPIKE itself:

1. Read a raw FP32 bit pattern from an input file.
2. Convert it with SoftFloat using `FP32 -> FP16 -> FP32`.
3. Convert it with `typeSimulationSoftFloatFP16(value)` from the ADF helper.
4. Compare the two FP32 bit patterns.

The sample input file is `inputs/lowprecision_fp32_values.hex`.

Current observed result:

- finite values match the SoftFloat round-trip
- the helper now matches the SoftFloat round-trip on the sample set with no
  mismatches

The FP64 hook-only test is `src/lowprecision_fp16_fp64_hook_compare.cpp`.

It validates the FP64 overload used by the FP64 LowPrecision FP16 hook:

1. Read a raw FP64 bit pattern from `inputs/lowprecision_fp64_values.hex`.
2. Convert it with SoftFloat using `FP64 -> FP16 -> FP64`.
3. Convert it with `typeSimulationSoftFloatFP16(uint64_t)`.
4. Compare the resulting FP64 bit patterns and SoftFloat exception flags.

Current observed result on the directed FP64 input file:

```text
total=46 mismatches=0
```

The FlexFloat-backed FP64 hook-only test is
`src/lowprecision_flexfloat_fp64_hook_compare.cpp`.

It validates the FP64 helper used by BF16, E5M2, and E4M3:

1. Read a raw FP64 bit pattern from `inputs/lowprecision_fp64_values.hex`.
2. Convert it with a direct local `flexfloat<E, M>` model.
3. Convert it with `typeSimulationFF64(E, M, value)`.
4. Compare the resulting FP64 bit patterns.

Current observed result on the directed FP64 input file:

```text
format=bf16 total=46 mismatches=0
format=e5m2 total=46 mismatches=0
format=e4m3 total=46 mismatches=0
```

### `verification_plan.md`

The current prompt/plan used to guide the verification work.

### `../LOW_PRECISION_CONVERSION_TRACE_PROMPT.md`

The prompt used to define the AxPIKE low-precision conversion trace facility.
It records the trace requirements, environment variables, expected log columns,
and suggested LeNet probe commands.

## Current Build Setup

The repo already has:

- SoftFloat sources in `softfloat/`
- FlexFloat installed locally under `~/.local/include` and `~/.local/lib`
- AxPIKE linked against FlexFloat in the local build

For the standalone verification, the important part is the direct library usage:

- SoftFloat is compiled from the repository’s `softfloat/` sources.
- FlexFloat is used through the installed C++ wrapper `flexfloat.hpp`.

## File-Driven Harness

The main reusable harness is `fp16_softfloat_flexfloat_compare.cpp`.

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

Useful FP16 bit-pattern meanings for reading the examples:

- `0x0000`: positive zero, `+0`
- `0x8000`: negative zero, `-0`
- `0x3c00`: positive one, `+1.0`
- `0x4000`: positive two, `+2.0`
- `0x7bff`: largest positive finite FP16 value, `+65504`
- `0x7c00`: positive infinity, `+inf`
- `0xfc00`: negative infinity, `-inf`
- `0x7e00`: positive quiet NaN, used by SoftFloat as the canonical invalid-operation NaN here
- `0xfe00`: negative quiet NaN, the FlexFloat result observed in these invalid-operation cases

## Sanity Pair Result

The sanity pair file is `inputs/fp16_sanity_pairs.hex`.

Results from the file-driven harness:

- `add`: 8 cases, 0 mismatches
- `sub`: 8 cases, 0 mismatches
- `mul`: 8 cases, 0 mismatches
- `div`: 8 cases, 1 mismatch

The only observed mismatch was a NaN-sign case:

- operands: `0x0000 / 0x8000`, meaning `+0 / -0`
- operation meaning: zero divided by zero is an invalid operation and produces a quiet NaN
- SoftFloat result: `0x7e00`, positive quiet NaN
- FlexFloat result: `0xfe00`, negative quiet NaN
- SoftFloat flags: `invalid`

This mismatch category is carried forward into the broader comparison logic.

## Representative Sample Result

The representative sample file is `inputs/fp16_sample_pairs.hex`.

Results from the file-driven harness:

- `add`: 48 cases, 0 mismatches
- `sub`: 48 cases, 1 mismatch
- `mul`: 48 cases, 0 mismatches
- `div`: 48 cases, 4 mismatches

All sampled mismatches are the same invalid-operation NaN-sign difference seen
in the sanity pairs: the operands produce a quiet NaN, SoftFloat returns the
positive quiet NaN `0x7e00`, FlexFloat returns the negative quiet NaN `0xfe00`,
and SoftFloat raises `invalid`.

## FlexFloat NaN Normalization

FlexFloat has upstream history around NaN bit-pattern assumptions. Issue #7
reported that tests should not assume host-specific NaN signs or payloads, and
commit `ef82d2e8268ec338552d1f5b526d9fa509acd853` introduced the
`NAN_NORMALIZATION` build flag.

With `NAN_NORMALIZATION`, FlexFloat canonicalizes NaN values to a positive quiet
NaN with the quiet bit set and payload bits cleared. Without that build option,
NaN sign and payload can depend on the host backend and operation path.

The local verification was run against the installed FlexFloat build available
in `~/.local`. In this setup, invalid-operation FP16 cases produced:

- SoftFloat: `0x7e00`, positive quiet NaN
- FlexFloat: `0xfe00`, negative quiet NaN

Therefore, the observed mismatch is not a finite arithmetic mismatch. It is a
NaN canonicalization/sign-policy difference. If FlexFloat is rebuilt with
`NAN_NORMALIZATION`, the NaN-sign mismatch category should be rechecked because
the expected FlexFloat NaN bit pattern may change.

## Input Generator

The exhaustive input generator is `fp16_pair_input_generator.cpp`.

What it does:

1. Writes the same text format consumed by the comparison harness.
2. Emits raw `uint16_t` FP16 operand pairs in hexadecimal.
3. Supports operand range partitioning with `--a-start`, `--a-end`,
   `--b-start`, and `--b-end`.
4. Supports a `--limit` option for bounded sample generation.

This makes it practical to split the exhaustive sweep into smaller chunks before
running the full `65,536 x 65,536` space.

## Exhaustive Runner

The chunked exhaustive orchestration script is `scripts/run_fp16_exhaustive_compare.py`.

It follows the current chunk schedule:

- `a_bits = 0x0000..0x00ff`, then `0x0100..0x01ff`, and so on up to
  `0xff00..0xffff`
- `b_bits = 0x0000..0xffff` for every chunk

For each chunk and each operation, the script:

1. Generates the chunk file with `fp16_pair_input_generator`.
2. Runs `fp16_softfloat_flexfloat_compare` on that file.
3. Records per-chunk logs and summaries.
4. Marks completed chunk files so reruns can skip them.

This is the structure used for the full exhaustive verification run.

The full-run command pattern is:

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

The `--delete-inputs` option removes generated `.hex` chunk files after each
comparison. Completion is tracked by `.done` markers, so the run can be resumed
without keeping the large generated input files.

Resume behavior:

- a valid `.done` marker means the chunk has already been compared
- empty or unparsable `.done` markers are treated as incomplete
- a `.hex` file without a valid `.done` marker is regenerated before comparison

This was needed because an interrupted run left partial `.hex` files and empty
`.done` markers. The runner now repairs those cases during resume.

## Full Exhaustive Result

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
in `reports/fp16_exhaustive_softfloat_flexfloat.md`.

## How The Pieces Fit Together

- `verification_plan.md` defines the intended validation strategy.
- `src/fp16_softfloat_flexfloat_compare.cpp` provides the reusable file-driven harness.
- `inputs/fp16_sanity_pairs.hex` provides a reproducible input set.
- `reports/` stores committed result summaries.
- `out/` stores ignored generated binaries and run artifacts.

## Next Step

The standalone FlexFloat-vs-SoftFloat FP16 comparison is now complete for the
basic arithmetic operations, and the hook-only FP32/FP64 tests validate the
conversion helpers used by the AxPIKE lowprecision hooks. The remaining step is
to run equivalent tests through the full AxPIKE simulator path, with real
approximation activation and instruction execution, so the standalone and
hook-only behavior can be compared with the simulator-integrated behavior.
