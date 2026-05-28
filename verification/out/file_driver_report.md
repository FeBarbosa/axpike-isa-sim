# File-Driven FlexFloat vs SoftFloat Sanity Run

Input file:
- `verification/inputs/stage1_sanity_fp16.hex`

Harness:
- `verification/src/fp16_file_compare.cpp`

Build:
- `g++ -std=c++17 -O0 -I. -I/home/felipe/.local/include verification/src/fp16_file_compare.cpp build/libsoftfloat.a /home/felipe/.local/lib/libflexfloat.a -o verification/out/fp16_file_compare`

Results:
- `add`: `total=8`, `mismatches=0`
- `sub`: `total=8`, `mismatches=0`
- `mul`: `total=8`, `mismatches=0`
- `div`: `total=8`, `mismatches=1`

Observed division mismatch:
- operands: `0x0000 / 0x8000`
- SoftFloat result: `0x7e00`
- FlexFloat result: `0xfe00`
- classification: NaN sign difference
- SoftFloat flags: `0x10` (`invalid`)

Conclusion:
- The file-driven harness works.
- The sanity vectors match for add, sub, and mul.
- Division shows the same NaN-sign edge case already seen in the directed stage-one checks.
