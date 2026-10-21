# OPRECOMP FFT Native Pilot

This report records the native-host FFT pilot executed on 2026-10-20 for
[issue #7](https://github.com/FeBarbosa/axpike-isa-sim/issues/7). It establishes
that the upstream FFT source can be compiled and can pass its original basic
reconstruction check in the current host environment. It does not exercise
RV64, proxy kernel, or AxPIKE.

## Validation Question

Can the simplest upstream OPRECOMP FFT variant compile without source changes,
execute natively with the smallest array length listed in `fft/test.sh`, and
pass the benchmark's original reconstruction-error check?

## Source and Build Identity

The OPRECOMP micro-benchmark repository was an independently checked-out,
clean source tree at detached commit:

```text
1afb13560b6c2cc01ce6f933c1f61a926b799163
```

The relevant source directory was `mb/fft`. No tracked OPRECOMP source was
changed. The build produced only the untracked objects `fft_stick.o`,
`run_fftw.o`, and `oprecomp.o`, plus the untracked `run_fftw` executable.

The host build used:

```text
gcc (GCC) 16.2.1 20260810
Linux 7.2.6-arch2-1 x86_64
```

The upstream Makefile supplied `-O2` and linked `libm` and `librt`. The resulting
binary was a dynamically linked, non-stripped, 64-bit x86-64 PIE executable.
Its SHA-256 was:

```text
067242622002a963f57cb0401480beb8c964459366c282ead521a4b65ed632b5
```

This executable hash identifies the exact native binary used for the pilot. It
does not make the binary portable across hosts or preserve its dynamic-library
environment.

## Reproducible Commands

From the independently checked-out OPRECOMP repository:

```bash
cd mb/fft
make build
./run_fftw 1000000
```

The direct executable invocation was used instead of `test.sh` because the
script runs array lengths 1,000,000, 5,000,000, and 10,000,000. Its
`measure.py` wrapper also omits the human-readable reconstruction check from
its final output. The direct invocation preserves the selected smallest input
and exposes the original metric and pass/fail message.

## Metric and Observed Result

The program performs a forward transform, normalizes the transformed values,
performs a backward transform, and computes the maximum absolute difference
over the real and imaginary components of the original and reconstructed
arrays. The upstream acceptance threshold is `1.0e-7`.

For an array length of 1,000,000, the observed output was:

```text
# array_length: 1000000
# time_total: 0.051915
# iterations: 1
# time_avg: 0.051915
max diff =1.81799e-15
Results are correct
```

The observed reconstruction error was finite and below the upstream threshold,
so the native basic-validation criterion passed. The single timing observation
is recorded for traceability only and is not a performance result.

## Evidence Classification and Limitations

This milestone supports only native-host implementation validation: the
selected source compiled without modification, executed to completion, and
passed its own reconstruction check for the selected input.

It does not establish:

- correctness against an independent FFT oracle;
- correctness for inputs other than the exercised deterministic initialization;
- RV64 or proxy-kernel build compatibility;
- AxPIKE functional-simulation behavior;
- approximate or transprecision numerical quality;
- reproducible performance, energy, or hardware characteristics; or
- application-level scientific conclusions.

The original timing region includes forward and backward plan creation, both
transforms, and normalization. It excludes array initialization and the final
error calculation. This boundary must be reviewed before the later AxPIKE
experiment so that the measured kernel is explicit and consistent across
configurations.

The upstream initialization contains this condition:

```c
if (i >= (n/4-1) && (3*n/4-1))
```

Its second operand is not an upper-bound comparison. Therefore, the exercised
input may differ from the apparent intent of selecting a bounded middle region.
The pilot deliberately preserved and evaluated the upstream source as-is. This
observation must be resolved or explicitly accepted before treating the input
as a scientific fixture.
