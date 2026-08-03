# Reduced LeNet External-NaN Diagnosis

> Historical note: this report predates the effective-type quantization v3
> statistics contract and uses the former transition-based protection policy.
> It remains available for traceability, but its counters and policy settings
> must not be interpreted as evidence for the current type-based contract. The
> current reduced fixture uses the 62-image minimal prefix covering all ten
> classes; the 59-image evidence below is intentionally preserved unchanged.

This report records a focused implementation diagnosis of the 123 external
writes classified as NaN during the deterministic, exact-policy LeNet run. It
does not constitute scientific evaluation of the transprecision model.

## Validation Question

Determine whether the 123 events are semantic FP64 NaNs or NaN-boxed FP32
register contents restored by FP64-width load instructions.

## Scope

- LeNet mode: `direct-logits`
- Input: deterministic 59-image MNIST prefix
- Policy: exact protected-bit thresholds
- Trace: only architectural writes already classified as external NaN
- Trace limit: 123 events globally
- General instruction tracing: disabled

## Command

```bash
AXPIKE_TRACE_TRANSPRECISION_EXTERNAL_NAN=1 \
AXPIKE_TRACE_TRANSPRECISION_EXTERNAL_NAN_LIMIT=123 \
AXPIKE_TRACE_TRANSPRECISION_EXTERNAL_NAN_FILE=external-nan.tsv \
build/axpike \
  --transprecision-protected-bits=fp32-e5m2:21,fp32-fp16:13,fp64-e5m2:50,fp64-fp16:42,fp64-fp32:29 \
  pk lenet-riscv-cpp-inference/app direct-logits 59
```

The run directory contained a `data` symlink to the prepared 59-image fixture.

## Observed Events

| Property | Count |
| --- | ---: |
| Traced external-NaN writes | 123 |
| 64-bit carriers | 123 |
| Upper 32 bits equal to `0xffffffff` | 123 |
| Valid NaN-boxed FP32 pattern | 123 |
| Lower FP32 classified as finite | 118 |
| Lower FP32 classified as zero | 5 |
| Lower FP32 classified as infinity or NaN | 0 |

The 118 finite values were all:

```text
64-bit register bits: 0xffffffff437f0000
lower FP32 bits:      0x437f0000
lower FP32 value:     255.0
```

The remaining five values were:

```text
64-bit register bits: 0xffffffff00000000
lower FP32 bits:      0x00000000
lower FP32 value:     +0.0
```

## Instruction and PC Attribution

| Instruction | PC | Events | Context |
| --- | --- | ---: | --- |
| `c.fldsp fs0,8(sp)` | `0x20f3a` | 59 | `convolution2DRelu_1` epilogue |
| `c.fldsp fs0,8(sp)` | `0x22654` | 59 | `lenet_inference` epilogue |
| `fld fs0..fs4,offset(sp)` | `0x10fa6`–`0x10fb6` | 5 | `main` epilogue |

The corresponding prologues save these callee-saved floating-point registers
with `fsd`; the reported loads restore the same 64-bit register images. Thus
these are not accidental eight-byte reads of standalone four-byte objects.
They are ABI register spills whose payload is a NaN-boxed FP32 value.

## Counter Invariants

- Trace rows: `123`
- `external_write_class_total[NAN]`: `123`
- `operation_result_class_total[NAN]`: `0`
- Instruction attribution: `118 + 5 = 123`
- Lower FP32 attribution: `118 finite + 5 zero = 123`
- The instruction, energy, and transprecision CSV files were byte-identical to
  the prior untraced exact run.
- LeNet outcome remained `processed=59`, `correct=59`, `errors=0`.

The transprecision CSV SHA-256 was:

```text
8a6332419a5437699f514abcd19425a5623e6b3691535e295bf9d130dfb7fdf1
```

## Effective FP32-Read Confirmation

A follow-up implementation added
`fp64_load_nan_boxed_fp32_effective_total`. It marks structurally valid
NaN-boxed FP32 candidates only at `fld`, `c.fld`, and `c.fldsp`, then confirms
and counts a loaded value once when a typed FP32 FPR read consumes it. The
context is cleared on overwrite and does not modify the FPR bits or precision
tag.

Two deterministic exact-policy reruns produced:

```text
processed: 59
correct: 59
errors: 0
external_write_class_total[NAN]: 123
operation_result_class_total[NAN]: 0
fp64_load_nan_boxed_fp32_effective_total: 58
```

The instruction and energy CSV files remained byte-identical to the pre-counter
exact run. The transprecision CSV differed only by the new counter row, and the
two post-change transprecision CSV files were byte-identical with SHA-256:

```text
03c94d3b5355421cd6b7273a17ca338df40d68b4bf040401c017febdfd2a5272
```

The difference between 123 carrier observations and 58 effective
confirmations is intentional. Loads that are never consumed as FP32, or whose
destination FPR is overwritten first, remain unconfirmed and do not contribute
to the effective counter.

## Conclusion

All 123 external events satisfy the structural NaN-boxing pattern and were
attributed to FP64-width spill/restore instructions. None has a NaN in its
lower FP32 payload, and none is an operation-produced NaN. Of these events, 58
loaded values were subsequently confirmed by a typed FP32 read.

Consequently, `external_write_class_total[NAN]` currently describes the FP64
carrier interpretation at the architectural write boundary, not the semantic
class of a NaN-boxed narrower payload. Scientific plots must not interpret
these 123 events as numerically generated NaNs without first defining and
applying a NaN-box-aware counter policy.
