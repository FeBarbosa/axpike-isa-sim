require_either_extension('D', EXT_ZDINX);
require_fp;
bool less = f64_lt_quiet(FRS1_D, FRS2_D) ||
            (f64_eq(FRS1_D, FRS2_D) && (FRS1_D.v & F64_SIGN));
if (isNaNF64UI(FRS1_D.v) && isNaNF64UI(FRS2_D.v))
  WRITE_FRD_D_OPERATION_RESULT(f64(defaultNaNF64UI),
      FRS1_FRS2_EFFECTIVE_TYPE);
else
  WRITE_FRD_D_OPERATION_RESULT(
      (less || isNaNF64UI(FRS2_D.v) ? FRS1_D : FRS2_D),
      FRS1_FRS2_EFFECTIVE_TYPE);
set_fp_exceptions;
