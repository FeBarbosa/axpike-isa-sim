# Uniform-n Transprecision Experiment-Matrix Validation

This report records structural and reproducibility validation of the SSCAD
uniform saturated experiment-matrix generator on 2026-08-02. It does not
execute AxPIKE numerical workloads, LeNet, or the scientific matrix.

## Validation Question

Does the generator produce exactly one configuration for every integer `n`
from 0 through 50, saturate each type at its approved maximum, emit accepted
AxPIKE arguments, and reproduce a deterministic policy-versioned manifest?

## Controlled Definition

The canonical vector order and limits are:

| Type | Maximum protected width |
| --- | ---: |
| FP64 | 50 |
| FP32 | 21 |
| FP16 | 8 |
| E5M2 | 0 |

For each integer `n` from 0 through 50, the vector is
`min(n,50), min(n,21), min(n,8), 0`. This produces 51 distinct dynamic
configurations. The matrix records one historical original FP32/FP64 reference
whose provenance remains pending, but the generator does not execute or
reconstruct that control. Fixed FP16 and E5M2 controls are outside scope.

## Reproducible Commands

```bash
python3 -m unittest \
  verification.tests.test_transprecision_experiment_matrix \
  verification.tests.test_reduced_lenet_validation

python3 verification/scripts/generate_transprecision_experiment_matrix.py \
  --output /tmp/transprecision-uniform-matrix-a.json

python3 verification/scripts/generate_transprecision_experiment_matrix.py \
  --output /tmp/transprecision-uniform-matrix-b.json

cmp \
  /tmp/transprecision-uniform-matrix-a.json \
  /tmp/transprecision-uniform-matrix-b.json

sha256sum /tmp/transprecision-uniform-matrix-a.json

python3 -c 'import json, subprocess; from pathlib import Path; m = json.loads(Path("/tmp/transprecision-uniform-matrix-a.json").read_text()); [subprocess.run(["build/axpike", c["cli_argument"], "--help"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) for c in m["configurations"]]; print("accepted", len(m["configurations"]), "uniform vectors")'
```

## Observed Evidence

- All 13 focused Python tests passed: 6 matrix tests and 7 reduced-validation
  automation tests.
- The manifest contains `n=0` through `n=50` exactly once and in ascending
  order.
- Independent expected-vector checks passed for all 51 points.
- Directed checks passed at `n=0`, `n=8`, `n=9`, `n=21`, `n=22`, and `n=50`,
  covering endpoints and both sides of the FP16 and FP32 saturation boundaries.
- The current `build/axpike` parser accepted every generated CLI vector before
  processing `--help`.
- Two generated manifests were byte-identical.
- Manifest SHA-256:
  `8541835acacaa260593db919cff3853e58c7ccd1337cc62238287fda702c5529`.
- The manifest uses schema version 3 and records policy
  `effective-type-quantization-v4`, version 4.
- It records 51 planned new executions, one provenance-pending historical
  reference control, and 52 planned evaluation points.
- Invalid vector length, FP64 width 51, and E5M2 width 1 remain rejected.

## Artifact Identity

- AxPIKE parent commit:
  `205a069647efca3d9e46e8ce2b1a399d3fda0bce`.
- The parent worktree and paper submodule were dirty because this is a
  pre-commit implementation and methodology milestone.
- AxPIKE executable SHA-256:
  `5e7a88a44cc98a8287ef862c965eeed34037a03c6a38f388e24e9ecefe7c2272`.
- Matrix-generator SHA-256:
  `e81cd3f897425f916ab580456725d2125acc0c68685d76fda335da41a722f674`.
- ADF submodule commit:
  `e9b5742a078e6f5af0c44b16bc0986d005f1bd89`.
- Recorded paper submodule commit:
  `4277f1a0114f0dfc6991c6d1c3208ed4a2c7dd06`, with uncommitted methodology
  changes present.

The executable hash identifies only the parser used by the integration audit.
The generator hash identifies the source that produced the two byte-identical
manifests. These dirty-worktree identities must not be reused as final campaign
provenance.

## Evidence Classification and Limitations

This evidence supports implementation validation and reproducibility of matrix
construction only. It does not validate numerical behavior, application
accuracy, runtime, the historical reference, or scientific conclusions. The
two endpoints have separate reduced application evidence. The 49 interior
configurations still need the 62-image reduced gate before any complete
10,000-image campaign.
