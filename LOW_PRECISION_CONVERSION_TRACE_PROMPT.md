# Prompt: Low-Precision Conversion Trace Instrumentation In AxPIKE

I need focused instrumentation to debug low-precision conversion behavior in AxPIKE during a LeNet softmax/expf run.

## Context

- The client app is a RISC-V LeNet inference binary using AxPIKE approximations.
- The suspicious path is `softmax -> expf()`.
- In the old AxPIKE version without FP64 instruction coverage, `expf()` internally uses FP64 instructions, especially `fcvt.d.s` and `fcvt.s.d`.
- I want to compare E5M2 and FP16 conversion behavior around FP32<->FP64 boundaries.
- The goal is to explain why E5M2 had much better accuracy than FP16 when FP64 arithmetic was not covered.

Please implement a temporary, low-noise trace facility in AxPIKE that logs low-precision register conversions only for FP16 and E5M2.

## Files Likely Involved

- `adele/adf/LowPrecisionFP16.cc`
- `adele/adf/LowPrecisionE5M2.cc`
- `adele/adf/LowPrecisionSimulation/typeConvertion.c`
- Related ADF files if needed:
  - `adele/adf/LowPrecisionFP16.adf`
  - `adele/adf/LowPrecisionE5M2.adf`

## Current Conversion Hooks

- `ReadLowPrecisionFP16()`
- `ReadLowPrecisionFP16FP64()`
- `ReadLowPrecisionE5M2()`
- `ReadLowPrecisionE5M2FP64()`

## Required Trace Data

Please add optional logging inside these hooks showing, for each conversion:

- approximation type: `FP16` or `E5M2`
- width: `FP32` or `FP64`
- whether the hook is being used for `regbank_read` or `regbank_write`, if that context is available
- current PC, if accessible from `processor_t`
- instruction mnemonic or opcode, if accessible without a large refactor
- original raw bits
- converted raw bits
- original decimal value
- converted decimal value
- whether original/converted is finite, inf, or nan
- absolute error and relative error when finite

## Runtime Control

The instrumentation must be controlled by environment variables or a similarly simple runtime switch, for example:

- `AXPIKE_TRACE_LP_CONVERSIONS=1`
- `AXPIKE_TRACE_LP_LIMIT=1000`
- `AXPIKE_TRACE_LP_FILE=/tmp/axpike_lp_conversions.log`
- `AXPIKE_TRACE_LP_FORMAT=fp16,e5m2` or `AXPIKE_TRACE_LP_FORMAT=all`
- `AXPIKE_TRACE_LP_WIDTH=fp32,fp64` or `AXPIKE_TRACE_LP_WIDTH=all`

## Constraints

- Default behavior must be unchanged when tracing is disabled.
- Keep the implementation temporary and minimally invasive.
- Avoid huge logs by enforcing a hard log limit.
- Prefer one CSV or TSV line per conversion.
- Flush periodically or use stderr/file safely enough that a short run preserves logs.
- Do not change the numerical conversion behavior.
- Do not refactor unrelated AxPIKE code.
- Do not remove existing FP64 coverage support.
- Keep the trace code easy to remove later, preferably isolated in a small helper file instead of spreading formatting logic through each hook.

## Please Document

1. How to rebuild AxPIKE after the change.
2. Example commands to run:
   - FP16 trace
   - E5M2 trace
3. How to interpret the resulting log.

## Suggested Test Commands From The LeNet Repository

After rebuilding AxPIKE, run these from:

```bash
/home/felipe/Research/repos/lenet-riscv-cpp-inference
```

FP16 expf probe:

```bash
AXPIKE_TRACE_LP_CONVERSIONS=1 AXPIKE_TRACE_LP_LIMIT=2000 AXPIKE_TRACE_LP_FILE=/tmp/fp16_lp.log AXPIKE_TRACE_LP_FORMAT=fp16 AXPIKE_TRACE_LP_WIDTH=all axpike pk app expf-probe expf-probe 10 fp16
```

E5M2 expf probe:

```bash
AXPIKE_TRACE_LP_CONVERSIONS=1 AXPIKE_TRACE_LP_LIMIT=2000 AXPIKE_TRACE_LP_FILE=/tmp/e5m2_lp.log AXPIKE_TRACE_LP_FORMAT=e5m2 AXPIKE_TRACE_LP_WIDTH=all axpike pk app expf-probe expf-probe 10 e5m2
```

Softmax-only FP16:

```bash
AXPIKE_TRACE_LP_CONVERSIONS=1 AXPIKE_TRACE_LP_LIMIT=5000 AXPIKE_TRACE_LP_FILE=/tmp/fp16_softmax_lp.log AXPIKE_TRACE_LP_FORMAT=fp16 AXPIKE_TRACE_LP_WIDTH=all axpike pk app test softmax softmax trace-softmax 1 fp16
```

Softmax-only E5M2:

```bash
AXPIKE_TRACE_LP_CONVERSIONS=1 AXPIKE_TRACE_LP_LIMIT=5000 AXPIKE_TRACE_LP_FILE=/tmp/e5m2_softmax_lp.log AXPIKE_TRACE_LP_FORMAT=e5m2 AXPIKE_TRACE_LP_WIDTH=all axpike pk app test softmax softmax trace-softmax 1 e5m2
```

## Expected Final Answer

The final answer should include:

- Summary of files changed.
- Exact log columns.
- Rebuild command used.
- Any limitations, especially if PC or instruction mnemonic cannot be accessed from these hooks.
