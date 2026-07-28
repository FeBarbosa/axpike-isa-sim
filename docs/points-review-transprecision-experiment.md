# Points to be reviewed or changed in AxPike v1 implementation of Transprecision

- Tag used for functional simulation proposes and not architectural feature. It is not an microarchitectural feature too, i.e., if someone use this functional model
to design an RTL processor, this will not include tags in FPR.

- review the .csv metrics, because some of them are not necessary or do not make sense:
    - same type promotion/demotion (e.g., E5M2 to E5M2)
    - Resolution: emit only unequal supported-format pairs. Record operand
      promotions, result promotions, and exact versus masked result demotions.
      Candidate rejections are internal search decisions and are not reported.
      Masked external writes are diagnostic events, not result demotions.

- Terminology resolved: use "candidate format" for the format currently being
  evaluated. Avoid "reduced type", because, for example, FP32 is reduced only
  relative to FP64 and the term does not identify the format's role in the
  classification policy.

- Candidate-format subnormal, overflow, and underflow cases must be covered by
  focused validation tests.

- Need an classification of the instructions strategies to classify the results between the reduced
types

- The policy of using the architectural type of the instruction when writing an external NaN,
+/-Inf value to a FPR register is not the best solution, because it forces every operation with
that tagged value to be classified with the NaN tag, and considering an E5M2 operatand used in
arithmetic operation between an external NaN classified as FP32 will be classified as FP32 operation
and it would be more interesting for an actual hardware to perform this operation as E5M2, because
the result would change (if I am not wrong, it will produce an NaN value).
    - Resolution: an external infinity or NaN preserves its architectural bits
      and receives E5M2 metadata. An operation-generated infinity or NaN
      preserves Spike's result bits and receives the operation type. External
      and operation-generated value classes are counted separately.

- What repens when FP32 is not sufficient for an architectural FP32? This should be promoted to FP64.
    - The solution for this problem should be convert the LeNet code to FP64 considering the experiment scope?
