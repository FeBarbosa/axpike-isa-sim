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
- `generate_transprecision_experiment_matrix.py`
- `prepare_reduced_mnist_fixture.py`
- `run_reduced_lenet_validation.py`
- `run_uniform_n_reduced_matrix.py`
- `summarize_reduced_lenet_validation.py`
- `plot_reduced_lenet_validation.py`

## Transprecision Experiment Matrix

The complete pre-run methodology and claim boundaries are defined in
[`../docs/experiments/sscad2026_uniform_n_experiment_protocol.md`](../docs/experiments/sscad2026_uniform_n_experiment_protocol.md).

`scripts/generate_transprecision_experiment_matrix.py` expands the approved
uniform saturated parameterization. One integer `n` ranges from 0 through 50.
For each point, the generated vector is `min(n,50), min(n,21), min(n,8), 0` for
FP64, FP32, FP16, and E5M2, respectively.

The JSON manifest contains the uniform `n`, a stable type-named vector
identifier, policy name and version, the canonical type order, and the complete
AxPIKE CLI argument. E5M2 remains explicit with its only valid width, zero.
The matrix has 51 distinct dynamic configurations and requires no alias
deduplication.

The full-protection endpoint at `n=50` remains a dynamic configuration, not an
original-execution baseline. The original FP32/FP64 result is recorded as a
provenance-pending historical reference outside the generated vector matrix.
Fixed-format FP16 and E5M2 ADF controls are outside the scoped SSCAD experiment.

Generate the manifest with:

```bash
python3 verification/scripts/generate_transprecision_experiment_matrix.py \
  --output verification/out/transprecision-experiment-matrix.json
```

The generator validates 51 dynamic configurations, 51 planned new executions,
one historical reference control, and 52 planned evaluation points. Its
manifest schema is version 3.

Run its focused tests with:

```bash
python3 -m unittest \
  verification/tests/test_transprecision_experiment_matrix.py
```

These tests validate matrix construction, policy identity, parameter bounds,
saturation breakpoints, CLI generation, and reproducibility only. They do not
execute AxPIKE or establish application-level numerical correctness.

## Focused Transprecision Validation

The simulator-side focused suite can be rebuilt and executed from an existing
configured build directory with:

```bash
cd build
make -j2 \
  transprecision-tags-utst \
  transprecision-operands-utst \
  transprecision-classification-utst \
  transprecision-write-macros-utst \
  transprecision-fp-execution-utst
./transprecision-tags-utst
./transprecision-operands-utst
./transprecision-classification-utst
./transprecision-write-macros-utst
./transprecision-fp-execution-utst
```

The suite covers tag state, effective-type selection, protected-bit
classification, persistent selected writes, separate W-to-T quantization and
later result-tag-reduction counters, overflow and underflow boundaries,
subnormal propagation without mandatory zeroing, contextual special values, external/operation
value-class counters, metadata recovery, and representative instruction
execution. Special-value checks require preservation of infinity and NaN
architectural bits, including NaN sign and payload, without recording a masked
reduction or invalid result promotion. Directed FP64-load NaN-boxing
checks also cover structural candidate detection, no confirmation on typed FP64
reads, one-time confirmation on typed FP32 reads, repeated reads, and overwrite
invalidation.

Passing this suite is implementation validation only. It does not replace the
reduced application run, counter-invariant checks, deterministic replay, or the
full scientific evaluation.

The deterministic reduced-LeNet diagnosis and effective-confirmation result are
recorded in
`reports/lenet_reduced_external_nan_diagnosis.md`.

The current policy-version-4 evidence is recorded in
`reports/lenet_reduced_deterministic_validation.md`. It documents only reduced
implementation and simulator-model validation, not the complete scientific
matrix.

Prepare and run this validation with:

```bash
python3 verification/scripts/prepare_reduced_mnist_fixture.py \
  --source-directory lenet-riscv-cpp-inference/data \
  --output-directory verification/out/mnist-prefix62-v1

python3 verification/scripts/run_reduced_lenet_validation.py \
  --axpike build/axpike \
  --application lenet-riscv-cpp-inference/build/bin/app \
  --data-directory verification/out/mnist-prefix62-v1 \
  --output-directory verification/out/lenet-reduced-validation-v8

python3 verification/scripts/summarize_reduced_lenet_validation.py \
  --manifest \
    verification/out/lenet-reduced-validation-v8/run-manifest.json \
  --output-directory \
    verification/out/lenet-reduced-validation-v8-summary-v4

python3 verification/scripts/plot_reduced_lenet_validation.py \
  --summary \
    verification/out/lenet-reduced-validation-v8-summary-v4/summary.json \
  --output-directory \
    verification/out/lenet-reduced-validation-v8-figures-v4 \
  --pdf
```

The runner intentionally executes only the full-protection and combined
no-protection endpoints over the deterministic 62-image prefix. The
experimental application mode includes softmax followed by argmax, emits only
aggregate outcome counts, and suppresses unrelated
floating-point progress and reporting work. This is the
smallest contiguous prefix of the standard MNIST test ordering that contains at
least one example of every class from 0 through 9. The fixture manifest uses
schema 2 and records the per-class counts, represented classes, last source
index, and minimal-prefix invariant. This improves structural class coverage
without treating the reduced fixture as a statistically representative sample.
The runner emits manifest schema 3 with run-record schema 4 and requires policy
`effective-type-quantization-v4`; older
artifacts are rejected. The summarizer checks outcome partitioning, policy
identity, effective-type totals derived from per-instruction rows,
lower-effective-type quantization bounds, overflow/underflow invariants,
changed result-tag reductions as a subset of all tag reductions, zero invalid
result promotions, zero unclassified types/operands, zero fallback events, and
`fp64_load_nan_boxed_fp32_effective_total <=
external_write_class_total[NAN]`. It also requires all eight stable network
regions and checks that every global transprecision counter equals the sum of
the regional counters. The summary directory includes `regions.csv` for one
aggregate row per run and region, plus
`effective-types-by-region-instruction.csv` for the detailed effective-type
table.
Passing these checks supports reduced
implementation and simulator-model validation only; it does not replace the
10,000-image scientific matrix.

Stable section identifiers need not be visited contiguously. Instruction and
energy CSVs retain columns from section 0 through the greatest selected
identifier and report zero for an unvisited identifier. The regional
transprecision CSV contains the regions actually selected. Consequently, the
diagnostic `direct-logits` path may omit softmax region 6 while still selecting
argmax region 7; the principal `softmax` campaign must continue to instantiate
all eight expected regions.

Run the focused sparse-section counter test with:

```bash
make -C build axpike_stats_sections-utst
build/axpike_stats_sections-utst
```

Run the focused automation tests with:

```bash
python3 -m unittest \
  verification/tests/test_transprecision_experiment_matrix.py \
  verification/tests/test_reduced_lenet_validation.py \
  verification/tests/test_uniform_n_reduced_matrix.py
```

After the two endpoint checks pass, generate and execute the resumable
51-configuration reduced matrix with:

```bash
python3 verification/scripts/generate_transprecision_experiment_matrix.py \
  --output verification/out/transprecision-uniform-matrix-v1.json

python3 verification/scripts/run_uniform_n_reduced_matrix.py \
  --matrix verification/out/transprecision-uniform-matrix-v1.json \
  --axpike build/axpike \
  --application lenet-riscv-cpp-inference/build/bin/app \
  --data-directory verification/out/mnist-prefix62-v1 \
  --output-directory \
    verification/out/lenet-uniform-n-reduced-matrix-v1
```

The runner validates the immutable campaign identity before resuming. Each
configuration uses a numbered attempt directory and is skipped only when its
completion marker, run-record hash, every recorded artifact hash, matrix
identity, policy vector, and invariant gate remain valid. Failed and
interrupted attempts are preserved. The optional `--max-new-runs` argument can
bound a session without invalidating later resumption. A complete campaign
emits `run-manifest.json`, `summary/`, and `matrix-gate.json`; the full
10,000-image evaluation must not start unless the matrix gate reports all 51
configurations as passed.

The campaign identity uses schema 2 and evaluates AxPIKE's dirty-worktree flag
over the executable provenance scope. The independently versioned
`paper-sscad2026` article tree is explicitly excluded because it cannot affect
the simulator or LeNet binaries. This exclusion does not hide its state: the
manifest records the parent repository's tracked object and status entry, plus
the article worktree's observed commit, dirty flag, and status entries. That
separate snapshot remains part of the immutable campaign record. A change to
simulator code, automation, build inputs, ADF, or application sources still
makes the corresponding executable provenance dirty.

## Hook-Only FP16 Lowprecision Test

The hook-only test is `src/lowprecision_fp16_fp32_hook_compare.cpp`.

It exercises the same FP16 conversion path used by
`adele/adf/LowPrecisionFP16.cc`, but without running AxPIKE itself:

1. Read a raw FP32 bit pattern from an input file.
2. Convert it with a direct local `flexfloat<5, 10>` model.
3. Convert it with `typeSimulationFF(5, 10, value)` from the ADF helper.
4. Compare the two FP32 bit patterns.

The sample input file is `inputs/lowprecision_fp32_values.hex`.

Current observed result:

- finite values match the direct FlexFloat model
- the helper matches the direct FlexFloat model on the sample set with no
  mismatches

The FP64 hook-only test is `src/lowprecision_fp16_fp64_hook_compare.cpp`.

It validates the FP64 overload used by the FP64 LowPrecision FP16 hook:

1. Read a raw FP64 bit pattern from `inputs/lowprecision_fp64_values.hex`.
2. Convert it with a direct local `flexfloat<5, 10>` model.
3. Convert it with `typeSimulationFF64(5, 10, value)` from the ADF helper.
4. Compare the resulting FP64 bit patterns.

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
