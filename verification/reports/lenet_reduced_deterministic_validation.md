# Reduced Deterministic LeNet Validation

This report records the reduced LeNet validation milestone used to close the
methodology implementation and exercise the future result-extraction pipeline.
It is implementation and simulator-model validation only. It is not the
complete 10,000-image scientific evaluation and must not be used to claim a
workload-wide accuracy/precision trade-off.

## Validation Questions

The 59-image prefix and two dynamic-policy endpoints test whether:

1. the exact and no-protection protected-bit vectors reach the application;
2. application outcomes partition as `correct + errors = processed`;
3. effective-type counters partition all effective-type observations;
4. no effective types or operands remain unclassified;
5. no unclassified fallback is used;
6. effective NaN-boxed FP32 confirmations never exceed external FP64-carrier
   NaN observations;
7. an independent replay produces identical scientific CSV contents; and
8. the same extracted summary can drive dependency-free example figures.

## Reproducible Fixture and Commands

The fixture generator rewrites the test IDX headers to 59 and copies exactly
the first 59 image and label records. The training files are linked unchanged
because the application expects the complete four-file MNIST directory.

```bash
python3 verification/scripts/prepare_reduced_mnist_fixture.py \
  --source-directory lenet-riscv-cpp-inference/data \
  --output-directory verification/out/mnist-prefix59-v1

python3 verification/scripts/run_reduced_lenet_validation.py \
  --axpike build/axpike \
  --application lenet-riscv-cpp-inference/build/bin/app \
  --data-directory verification/out/mnist-prefix59-v1 \
  --output-directory verification/out/lenet-reduced-validation-v3

python3 verification/scripts/summarize_reduced_lenet_validation.py \
  --manifest \
    verification/out/lenet-reduced-validation-v3/run-manifest.json \
  --output-directory \
    verification/out/lenet-reduced-validation-v3-summary

python3 verification/scripts/plot_reduced_lenet_validation.py \
  --summary \
    verification/out/lenet-reduced-validation-v3-summary/summary.json \
  --output-directory \
    verification/out/lenet-reduced-validation-v3-figures \
  --pdf
```

The runner executes only:

- exact protected widths: `21,13,50,42,29`; and
- the combined no-protection endpoint: `0,0,0,0,0`.

Both use `direct-logits` and exactly 59 images. The runner refuses an existing
output directory, records the AxPIKE and application hashes, records the size
and hash of every MNIST input, and hashes every generated CSV.

## Fixture Identity

| Artifact | Size | SHA-256 |
| --- | ---: | --- |
| Reduced test images | 46,272 bytes | `0d32b2692ced569f8472d058adf79fa34239c9dc818c2d51a4cf68b10d4e5a73` |
| Reduced test labels | 67 bytes | `96ec825c18a1bed7930110db7bb426589b8a0276fc53776a10e356e74a21c256` |
| AxPIKE executable | -- | `f1e3418a4c5fd584f3ea3bec286a9a6124e20f88a024ce2e872268336c627baa` |
| LeNet application | -- | `c2dfadc3ee6afed5eeb920b6d86459167d81eaaec2d7dc92cf467d5dd2a0483f` |

The generated reduced image and label files are byte-identical to the
previously diagnosed temporary fixture.

The run manifest records these source revisions:

| Component | Commit | Dirty |
| --- | --- | --- |
| AxPIKE parent | `cd85e125c8ac5598d2591a1cb2a9619613eeef0d` | yes |
| ADF | `e9b5742a078e6f5af0c44b16bc0986d005f1bd89` | no |
| LeNet application | `16741f1a33f92e36400a98f186a9069a45105ab8` | yes |

The dirty states are expected for this pre-commit validation milestone. The
executable hashes above are therefore the authoritative identities of what was
run. The complete scientific evaluation should be launched only after the
approved changes are committed, with clean revisions recorded in its manifest.

## Results

| Metric | Exact | No protection |
| --- | ---: | ---: |
| Processed | 59 | 59 |
| Correct | 59 | 11 |
| Errors | 0 | 48 |
| Effective observations | 12,895,371 | 12,864,342 |
| E5M2 effective type | 377,337 | 12,862,613 |
| FP16 effective type | 8,128 | 1,066 |
| FP32 effective type | 12,371,138 | 663 |
| FP64 effective type | 138,768 | 0 |
| Unclassified effective type | 0 | 0 |
| Unclassified operands | 0 | 0 |
| Unclassified fallback | 0 | 0 |
| External carrier NaN | 123 | 123 |
| Effective NaN-boxed FP32 | 58 | 58 |
| Operation-produced NaN | 0 | 0 |

All declared invariants passed. In particular:

- exact: `377337 + 8128 + 12371138 + 138768 = 12895371`;
- no protection:
  `12862613 + 1066 + 663 + 0 = 12864342`;
- both runs: `58 <= 123`; and
- both runs: unclassified effective types, unclassified operands, and fallback
  events are zero.

The equality of the 123/58 NaN-accounting values at both endpoints is expected:
the counter describes contextual FP64-load carrier use and does not depend on
the protected-bit widths.

## Deterministic Replay

The two endpoints were independently replayed. For each endpoint, the
instruction, energy, and transprecision CSV files were byte-identical:

| Endpoint | CSV | SHA-256 |
| --- | --- | --- |
| Exact | Instruction | `c3e358d2ff09657058744e66648dfd190beae6c4212dbfc0b82c9500772bf729` |
| Exact | Energy | `56599c9ca0fac0b95a8c1bfc817c66a6c521d70f01b349e110c809bb81d83625` |
| Exact | Transprecision | `03c94d3b5355421cd6b7273a17ca338df40d68b4bf040401c017febdfd2a5272` |
| No protection | Instruction | `bf0fa9e5a3247dbdb0a86401a6c45231dea58104e383979749f124bf7cf10360` |
| No protection | Energy | `56599c9ca0fac0b95a8c1bfc817c66a6c521d70f01b349e110c809bb81d83625` |
| No protection | Transprecision | `f2dfcf50efd7b2577ef13108e8dbe185395eee34d10ca7b162488e92c0c64502` |

The two extracted `summary.csv` files were also byte-identical, with SHA-256
`2841863307922fe7b33a2fc5424327fb2c7d585f32e93e65e12bf3a3a3612e93`.

The application logs differ only in the incremental identifier embedded by
AxPIKE in generated CSV filenames (`45` versus `46`, for example). This
non-semantic filename allocation is excluded from the determinism claim; the
CSV contents and extracted metrics are identical.

## Figure Artifact and Interpretation Boundary

The canonical example-figure source is the dependency-free SVG generated from
`summary.json`:

```text
reduced_validation_overview.svg
SHA-256: 59e86bb937060465cf08a225afaa4ba1365b19482b77d0fcf4535c20fa126e1b
```

It contains three panels: reduced accuracy, effective-type distribution, and
exact-policy NaN accounting. The SVG is deterministic. The PDF is a rendering
artifact for LaTeX; Inkscape embeds a creation date, so byte identity of PDFs
generated at different times is not part of the reproducibility claim.

The `11/59` no-protection outcome demonstrates that the reduced pipeline is
sensitive to a strongly approximate endpoint. It is not a scientific accuracy
estimate for the complete MNIST test set. Accuracy curves and comparative
claims remain pending the approved 10,000-image matrix.

## Evidence Classification

This milestone supports:

- implementation validation of the scripts and counter invariants;
- simulator-model validation of integrated value/tag/counter propagation on
  the exercised LeNet prefix; and
- reproducibility of the reduced extraction and SVG-generation path.

It does not support RTL, synthesis, timing, area, power, or full
application-level scientific claims.
