# Stage 1 Sanity Report

Scope:
- FlexFloat FP16 vs SoftFloat FP16
- Operation exercised: addition only
- Inputs driven through the standalone directed reference binaries

Test cases:

| Case | SoftFloat result bits | FlexFloat result bits | Outcome |
| --- | --- | --- | --- |
| `1 + 2` | `0x4200` | `0x4200` | match |
| `1.5 + 2.25` | `0x4380` | `0x4380` | match |
| `65504 + 1` | `0x7bff` | `0x7bff` | match |
| `0 + -0` | `0x0000` | `0x0000` | match |
| `5.9604645e-08 + 5.9604645e-08` | `0x0002` | `0x0002` | match |
| `inf + 1` | `0x7c00` | `0x7c00` | match |
| `nan + 1` | `0x7e00` | `0x7e00` | match |
| `-nan + 1` | `0x7e00` | `0xfe00` | NaN sign difference |

Observations:
- Normal values matched.
- Saturation to the largest finite FP16 value matched.
- Signed zero behaved as expected in the tested case.
- Subnormal addition matched.
- Infinity handling matched.
- NaN propagation matched for the quiet NaN case.
- A negative-NaN input produced a sign-bit difference:
  - SoftFloat normalized the input to `0x7e00`
  - FlexFloat preserved the negative sign bit as `0xfe00`

Conclusion:
- Stage 1 is feasible and the current standalone setup is suitable for deeper validation.
- The only observed discrepancy in the sanity set is a NaN sign-classification difference, which should be tracked in later mismatch classification.
