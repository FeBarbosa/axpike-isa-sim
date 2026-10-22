# OPRECOMP FFT RV64 Architectural Baseline

This report records the first OPRECOMP FFT execution through proxy kernel and
AxPIKE on 2026-10-20. Transprecision was explicitly disabled. This is an
incremental portability and implementation-validation milestone for
[issue #7](https://github.com/FeBarbosa/axpike-isa-sim/issues/7), not an enabled
transprecision experiment or application-level scientific result.

## Validation Question

Can the pinned, unmodified OPRECOMP FFT numerical sources be built as a static
RV64 executable, run through the current proxy kernel and AxPIKE with the
smallest selected input, pass the original reconstruction-error criterion, and
leave every transprecision event counter at zero in disabled mode?

## Adaptation Boundary

The OPRECOMP FFT sources are referenced directly from the independent checkout
at commit:

```text
1afb13560b6c2cc01ce6f933c1f61a926b799163
```

The AxPIKE-side overlay consists of:

- `verification/oprecomp/fft/Makefile`, which verifies the external source
  revision and tracked-source cleanliness before cross-compilation; and
- `verification/oprecomp/fft/oprecomp_pk.c`, which replaces only the OPRECOMP
  host measurement implementation for this target build.

The adapter preserves the `oprecomp_start`, `oprecomp_iterate`, and
`oprecomp_stop` interface and uses proxy-kernel-supported `gettimeofday`. It
does not provide AMESTER networking or power measurement. The FFT
initialization, plans, transforms, normalization, reconstruction metric, and
acceptance threshold remain in the pinned OPRECOMP sources.

## Artifact Identity

| Artifact | Identity |
| --- | --- |
| OPRECOMP source | `1afb13560b6c2cc01ce6f933c1f61a926b799163` |
| AxPIKE parent at execution | `38ca30d0994b802746c3ad8406b19b8983d409de` |
| AxPIKE executable SHA-256 | `901ad692462f43a78551122385b983b6c79ef25bbe9260d59913ab0785650e2b` |
| Proxy-kernel source | `5d417b01a139fc67b5649e02c20bef40bbf40c70` |
| Proxy-kernel executable SHA-256 | `2d743727adcd38921d381c15b976ef2ada06f2ee8ccfdd06b2c2892e03d2b735` |
| RV64 FFT executable SHA-256 | `415cb82a87645c48b2164f03b222eb23b8a16624e553ddf3082b9b3e418d3f79` |
| Compiler | `riscv64-unknown-elf-gcc (g6afcc4f6d) 16.1.0` |

The AxPIKE source tree contained the uncommitted overlay during the final
validation run; this report was added immediately afterward. Neither artifact
affects the already-built AxPIKE executable, whose hash is the authoritative
simulator identity for this pre-commit milestone.

The target executable is a statically linked 64-bit little-endian RISC-V ELF,
uses the double-float ABI, and declares `rv64imafdc` plus the associated
`zicsr`, `zifencei`, and compressed-instruction subsets. This matches the
proxy-kernel executable's relevant ISA and ABI attributes.

## Reproducible Commands

From the AxPIKE repository root:

```bash
make -C verification/oprecomp/fft build

cd verification/out/oprecomp-fft-rv64
../../../build/axpike \
  --transprecision-mode=disabled \
  ../../../../axpike-pk/build/pk \
  ./run_fftw \
  1000000
```

The executed command used absolute paths equivalent to the relative command
shown above.

## Observed Numerical Result

The process exited with status zero and printed:

```text
bbl loader
# array_length: 1000000
# time_total: 0.509896
# iterations: 1
# time_avg: 0.509896
max diff =2.07473e-15
Results are correct
```

The maximum reconstruction error was finite and below the original `1.0e-7`
threshold. The native-host pilot produced `1.81799e-15`. Both pass the same
threshold; exact equality across host and RV64 builds was not required.

The reported target time derives from proxy-kernel time services. It is not
native wall-clock time and is not interpreted as a performance response.

## Simulator Evidence

The instruction counter reported 543,877,007 executed instructions across the
application, proxy kernel, and other unscoped execution. Representative
user-mode FP64 counts included:

| Instruction | Count |
| --- | ---: |
| `fadd_d` | 50,413,944 |
| `fmadd_d` | 41,738,892 |
| `fmsub_d` | 14,702,390 |
| `fmul_d` | 39,092,325 |
| `fnmsub_d` | 10,351,790 |
| `fsub_d` | 45,416,856 |
| `fld` | 55,617,505 |
| `fsd` | 27,224,596 |

This confirms that the selected workload substantially exercises scalar FP64
instructions. It does not establish coverage of every instruction or
floating-point value class relevant to the transprecision model.

The global transprecision CSV identified `policy_mode` as `disabled`. Excluding
its six policy-metadata rows, all 68 event rows in the global CSV and all 68
rows in the section CSV had value zero. This satisfies the disabled-mode
zero-counter invariant for the emitted schema.

## Generated Artifact Hashes

The generated files remain under the ignored `verification/out` directory and
are not committed.

| Artifact | SHA-256 |
| --- | --- |
| Instruction counters | `6ce34e5e3088796e8e22db559ab28002ebfecee475526533ad9d426c931ce7b5` |
| Energy counters | `56599c9ca0fac0b95a8c1bfc817c66a6c521d70f01b349e110c809bb81d83625` |
| Global transprecision counters | `a751d5e03288d5ea23af1ee9de9ca3739bcfc3e1c1531c478c6f036d9ff0b6ea` |
| Section transprecision counters | `96f5ab3287f023076607743873a0c1b3749f18aeb37fe78a130f3782e87bcf23` |

## Deterministic Replay

The same AxPIKE, proxy-kernel, and RV64 FFT executables were replayed on
2026-10-21 with the same disabled-mode argument and array length in the
independent `verification/out/oprecomp-fft-rv64-replay-1` directory.

The replay exited successfully and reproduced the original observable result:

```text
# array_length: 1000000
# time_total: 0.509896
# iterations: 1
# time_avg: 0.509896
max diff =2.07473e-15
Results are correct
```

The instruction, energy, global transprecision, and section transprecision
CSVs were byte-identical to the first execution and therefore retained the
hashes listed above. The replay again contained 68 zero-valued global event
rows and 68 zero-valued section rows. This establishes deterministic replay
for the recorded baseline artifacts and numerical output.

## Differential Sanity Check Against Official Spike

On 2026-10-21, the same RV64 FFT executable was run with the same newly built
official proxy-kernel executable on both a recent official Spike and AxPIKE.
The simulator ISA and privilege arguments were also held constant. Only AxPIKE
received its additional `--transprecision-mode=disabled` argument.

| Artifact | Identity |
| --- | --- |
| Official Spike source | `6a13e0c53852ddd657f8f60d62b72c7623d194d7` |
| Official Spike SoftFloat submodule | `32eb3295a3c814c1551f94ebc29eed1dec3cb3b1` |
| Official Spike executable SHA-256 | `6267d55e51fc5e68963af2a094f0e11f91d8662fcb8a506acb1c3cbbb98f32c1` |
| Official proxy-kernel source | `9c61d29846d8521d9487a57739330f9682d5b542` |
| Official proxy-kernel executable SHA-256 | `2546180fafa790fd1b8c9a35b3afa4c68810076c678cda8a966524cdca704199` |
| AxPIKE executable SHA-256 | `901ad692462f43a78551122385b983b6c79ef25bbe9260d59913ab0785650e2b` |
| RV64 FFT executable SHA-256 | `415cb82a87645c48b2164f03b222eb23b8a16624e553ddf3082b9b3e418d3f79` |

The executions were equivalent to:

```bash
<official-spike> \
  --isa=rv64imafdc_zicntr_zihpm \
  --priv=msu \
  <official-pk> \
  verification/out/oprecomp-fft-rv64/run_fftw \
  1000000

build/axpike \
  --isa=rv64imafdc_zicntr_zihpm \
  --priv=msu \
  --transprecision-mode=disabled \
  <official-pk> \
  verification/out/oprecomp-fft-rv64/run_fftw \
  1000000
```

Both processes exited with status zero and produced the same observable
result:

```text
# array_length: 1000000
# time_total: 0.509919
# iterations: 1
# time_avg: 0.509919
max diff =2.07473e-15
Results are correct
```

The AxPIKE run also retained 68 zero-valued global transprecision event rows
and 68 zero-valued section rows. The instruction-counter CSV changed because
the official proxy kernel differs from the previously used AxPIKE proxy
kernel; that difference is outside the numerical comparison.

For this executable, input, ISA configuration, and shared proxy kernel, AxPIKE
therefore agrees with a recent official Spike on the benchmark's observable
numerical result. This is a differential implementation sanity check. It does
not prove general AxPIKE correctness, establish bit-for-bit equality of the
complete architectural state, or validate AxPIKE against the historical Spike
revision from which it was derived. The different native-host result
(`1.81799e-15`) remains acceptable under the benchmark threshold and is not, by
itself, evidence of an AxPIKE defect.

## Evidence Classification and Remaining Gate

This milestone supports RV64 build portability, integrated execution through
proxy kernel and AxPIKE, the original basic round-trip check for the selected
input, and the disabled-mode transprecision zero-counter invariant.

It does not establish enabled transprecision behavior, an independent FFT
oracle, correct scientific-fixture initialization, performance, energy, RTL
behavior, or application-level numerical quality.

All application and support instructions currently belong to section 0. Before
running `n=0`, `n=8`, `n=21`, or `n=50`, the FFT kernel must receive an explicit
section boundary and the reconstruction metric must be kept outside that
measured section. The enabled-mode workflow must also ensure that
transprecision does not corrupt the metric calculation itself or otherwise
document and validate an independent QoR calculation path.
