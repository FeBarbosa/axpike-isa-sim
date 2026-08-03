# Reduced Deterministic LeNet Validation

This report records the policy-version-4 reduced LeNet validation executed on
2026-07-28. It is implementation and simulator-model validation only. It is not
the complete 10,000-image scientific evaluation and does not support
workload-wide accuracy/precision claims.

## Validation Scope

The deterministic 62-image prefix was executed at two type-based endpoints:

- full protection: `fp64:50,fp32:21,fp16:8,e5m2:0`; and
- no protection: `fp64:0,fp32:0,fp16:0,e5m2:0`.

The validation checks outcome partitioning, policy identity, effective-type
totals derived from per-instruction observations, lower-type W-to-T
quantization bounds, overflow and underflow subsets, result-tag reduction
bounds, zero invalid result promotions, zero unexplained
unclassified/fallback paths, contextual NaN-boxing bounds, and deterministic
replay. Full protection prevents additional lossy reduction below T; it is not
called an exact baseline because W-to-T quantization may change a result.

The fixture is the smallest contiguous prefix of the standard MNIST test
ordering that covers all ten classes. The final included record has zero-based
source index 61 and is the first class-8 example. The fixture is intentionally
not balanced and is not used to estimate workload accuracy. Its class counts
are:

| Class | Images |
| --- | ---: |
| 0 | 6 |
| 1 | 10 |
| 2 | 5 |
| 3 | 5 |
| 4 | 10 |
| 5 | 7 |
| 6 | 5 |
| 7 | 7 |
| 8 | 1 |
| 9 | 6 |

## Reproducible Commands

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

The replay used the same commands with
`lenet-reduced-validation-v8-replay` and
`lenet-reduced-validation-v8-replay-summary-v4`.

## Artifact Identity

| Artifact | SHA-256 |
| --- | --- |
| Fixture manifest | `4cfcb1250295b50967457d47040eaf85b7d78d8fe70b67806a052fe3da57e9d3` |
| Reduced test images | `43cdb93a795bf28db6975a9f41b7c80d64004bd0c615bdea13f7d18b9914d1ee` |
| Reduced test labels | `0f69e727764b4ce3b3a8dd0454297ce9bed474e9c4a2e05dd0b93576ed62a22d` |
| AxPIKE executable | `5e7a88a44cc98a8287ef862c965eeed34037a03c6a38f388e24e9ecefe7c2272` |
| LeNet application | `c2dfadc3ee6afed5eeb920b6d86459167d81eaaec2d7dc92cf467d5dd2a0483f` |

The manifest records AxPIKE parent commit
`205a069647efca3d9e46e8ce2b1a399d3fda0bce` with a dirty worktree, ADF commit
`e9b5742a078e6f5af0c44b16bc0986d005f1bd89`, and application commit
`019a644bebc3cb931b5c2da00bcce4b15c54f765`. The executable hash is therefore
the authoritative simulator identity for this pre-commit milestone.

## Results

| Metric | Full protection | No protection |
| --- | ---: | ---: |
| Processed | 62 | 62 |
| Correct | 62 | 52 |
| Errors | 0 | 10 |
| Accuracy | 100.00% | 83.87% |
| Effective observations | 13,551,058 | 13,523,232 |
| E5M2 effective type | 397,564 | 13,521,991 |
| FP16 effective type | 15,916 | 559 |
| FP32 effective type | 12,991,754 | 682 |
| FP64 effective type | 145,824 | 0 |
| Lower-type W-to-T quantizations | 169,951 | 12,649,333 |
| Quantizations that changed bits | 8,191 | 6,195,161 |
| Quantizations to zero | 0 | 0 |
| Quantization overflows | 0 | 0 |
| Quantization underflows | 5 | 51 |
| Result-tag reductions | 425,634 | 1,240 |
| Result-tag reductions that changed bits | 0 | 384 |
| Exact result-tag reductions, derived | 425,634 | 856 |
| Invalid result promotions | 0 | 0 |
| Unclassified effective types | 0 | 0 |
| Unclassified operands | 0 | 0 |
| Unclassified fallback | 0 | 0 |
| External carrier NaN | 129 | 129 |
| Effective NaN-boxed FP32 | 61 | 61 |

Every declared invariant passed. In particular:

- quantization totals do not exceed operation-result totals;
- only W-to-T transitions where T is smaller than W are emitted;
- changed quantizations do not exceed total quantizations;
- overflows do not exceed changed quantizations;
- changes to zero do not exceed either changed quantizations or underflows;
- overflow plus underflow does not exceed total quantizations;
- changed result-tag reductions do not exceed total result-tag reductions;
- invalid result promotions are zero;
- effective-type totals are derived solely from per-instruction observations;
- unclassified effective types, unclassified operands, and fallbacks are zero;
  and
- contextual boxed-FP32 confirmations satisfy `61 <= 129`.

The full-protection run contains 8,191 lower-type W-to-T quantization changes. This is
direct evidence that full protection is not equivalent to the former exact
policy, even though its changed T-to-J result-tag reduction count is zero.

The application run also exercised underflow without any quantization to zero:
5 events under full protection and 51 under no protection. This is consistent
with propagation of representable subnormal values and demonstrates why
underflow and change-to-zero must remain separate counters.

## Deterministic Replay

The independent replay produced byte-identical CSV contents:

| Endpoint | CSV | SHA-256 |
| --- | --- | --- |
| Full protection | Instruction | `a734e6b07bc0bdbeab57713ea4d368697fba64eac2910ad45a54597b9018d9aa` |
| Full protection | Energy | `56599c9ca0fac0b95a8c1bfc817c66a6c521d70f01b349e110c809bb81d83625` |
| Full protection | Transprecision | `1e7c594f779699fd7ce1bd7a8b58a3ab35ec8a00f5787b48a5b0d18f8b5b7b3b` |
| No protection | Instruction | `86c46c9f7e8e9a7706d2797db415cf222426ecf313d41eb6808b3162ae6e3632` |
| No protection | Energy | `56599c9ca0fac0b95a8c1bfc817c66a6c521d70f01b349e110c809bb81d83625` |
| No protection | Transprecision | `2d8b35c07b77388d7a5780dc84584029e7a5c060b9644b67c524a6d49795106a` |

The two extracted `summary.csv` files were also byte-identical, with SHA-256
`236e8e0875a425e290e6a8c5120d93c59fc2dad01391a072203078a2418a783a`.
The generated dependency-free SVG has SHA-256
`c99dd2bfc1bd394ed15daf4a2f08f8d10e0e12aa43aba64a4d9970e4999c8efc`.
The corresponding PDF has SHA-256
`d5def6c00d16dbc0e69d2cf7a5633150cac3b85c9e81249c66ccf47e961932a2`.

## Evidence Classification

This milestone supports:

- implementation validation of policy-version-4 counters, CSVs, scripts, and
  rejection of obsolete artifacts;
- simulator-model validation of integrated value/tag/counter propagation on
  the exercised LeNet prefix; and
- reproducibility of the reduced runner, extractor, and SVG path.

It does not support RTL, synthesis, timing, area, power, native low-precision
arithmetic equivalence, or complete application-level scientific claims.
