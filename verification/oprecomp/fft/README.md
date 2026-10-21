# OPRECOMP FFT RV64 Pilot

This directory provides an AxPIKE-side build overlay for the OPRECOMP FFT
micro-benchmark. It references the independently checked-out OPRECOMP sources
without copying or modifying them.

## Scope

The first milestone builds a static RV64 executable and runs the original FFT
round-trip check through proxy kernel with AxPIKE transprecision disabled. It
is implementation validation only. It does not yet validate an enabled
transprecision configuration or support scientific quality/performance claims.

The pinned OPRECOMP revision is:

```text
1afb13560b6c2cc01ce6f933c1f61a926b799163
```

The Makefile rejects a different revision or tracked OPRECOMP source changes.
Untracked build artifacts in the external checkout do not affect the check.

## Build

The default location assumes that `axpike-isa-sim` and
`oprecomp-micro-benchmarks` are sibling directories. Override `OPRECOMP_DIR`
when the checkout is elsewhere.

```bash
make -C verification/oprecomp/fft build
```

The output is `verification/out/oprecomp-fft-rv64/run_fftw`. The build uses the
existing bare-metal toolchain, `rv64gc`, the double-float ABI, and static
linking for compatibility with proxy kernel.

## Architectural Baseline

Run the smallest input listed by the original `mb/fft/test.sh`:

```bash
build/axpike \
  --transprecision-mode=disabled \
  ../axpike-pk/build/pk \
  verification/out/oprecomp-fft-rv64/run_fftw \
  1000000
```

The basic gate requires successful process completion, a finite `max diff` no
greater than the upstream `1.0e-7` threshold, and `Results are correct`.
Transprecision counters must remain zero in disabled mode.

## Target Compatibility Adapter

The original `mb/common/oprecomp.c` depends on host socket APIs that are not
provided by the repository's bare-metal RISC-V toolchain. `oprecomp_pk.c`
implements the same start/iterate/stop API using `gettimeofday`, which proxy
kernel supports. With `AMESTER_DURATION` unset, both implementations execute
one iteration. The adapter intentionally does not support AMESTER power
measurement or networking.

The reported target time derives from proxy-kernel time services and is kept
only for traceability. It is not comparable to native wall-clock performance
and is not a scientific response variable.

The FFT input initialization, numerical implementation, reconstruction metric,
and acceptance threshold remain in the pinned OPRECOMP sources. The known
initialization-condition concern is documented in the native-pilot report and
is not changed by this overlay.

The first disabled-mode RV64 result is recorded in
`verification/reports/oprecomp_fft_rv64_architectural_baseline.md`.
