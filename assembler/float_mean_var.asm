# Computes mean and variance of a 128-element floating-point array.
# Results are left in float registers after HLT.
#
# Input: pattern [1.0, 2.0, 3.0, 4.0] repeated 32 times (128 values).
#   Expected f3 = 128.0  (N as float, confirms constant load)
#   Expected f4 = 2.5    (mean)
#   Expected f5 = 1.25   (variance)
#
# Register map:
#   x4  = array base address (200)
#   x5  = N = 128
#   x6  = loop counter i
#   x7  = working word address (base + i)
#   x8  = constants base address (100)
#   x9  = comparison / branch condition scratch
#   f1  = running sum (pass 1) / variance accumulator (pass 2)
#   f2  = current array element arr[i]
#   f3  = 128.0 (N as float, for division)
#   f4  = mean (result)
#   f5  = (arr[i] - mean) scratch, then variance (result)
#
# Memory layout (word addresses):
#   Code      :   0 -  28
#   Constants : 100        (128.0 as float)
#   Data      : 200 - 327  (128 float words = 32 cache lines)
#
# Cache sizing:
#   Code and constants occupy ~9 cache lines; data occupies exactly 32.
#   Together these exceed the 32-line cache, so data lines continuously
#   evict instruction lines and vice versa across both passes.
#   Recommended configuration: 4-way associative, 8 sets.

# -- Initialization --
    ADDI x4 x0 200       # x4 = base of float array
    ADDI x5 x0 128       # x5 = N = 128
    ADDI x8 x0 100       # x8 = address of float constant 128.0
    LWF f3 x8            # f3 = 128.0

# -- Pass 1: compute sum, then mean --
    FADD f1 f0 f0        # f1 = 0.0  (f0 is hardwired 0.0)
    ADDI x6 x0 0         # i = 0

pass1
    SGE x9 x6 x5         # x9 = (i >= N) ? 1 : 0
    BNEQ x9 x0 pass1done # exit when all elements summed
    ADD x7 x4 x6         # x7 = &arr[i]
    LWF f2 x7            # f2 = arr[i]
    FADD f1 f1 f2        # sum += arr[i]
    ADDI x6 x6 1         # i++
    J x0 pass1

pass1done
    FDIV f4 f1 f3        # f4 = mean = sum / 128.0  ->  2.5

# -- Pass 2: compute variance --
    FADD f1 f0 f0        # f1 = 0.0  (reuse f1 as variance accumulator)
    ADDI x6 x0 0         # i = 0

pass2
    SGE x9 x6 x5         # x9 = (i >= N) ? 1 : 0
    BNEQ x9 x0 pass2done # exit when all elements processed
    ADD x7 x4 x6         # x7 = &arr[i]
    LWF f2 x7            # f2 = arr[i]
    FSUB f5 f2 f4        # f5 = arr[i] - mean
    FMUL f5 f5 f5        # f5 = (arr[i] - mean)^2
    FADD f1 f1 f5        # var_sum += (arr[i] - mean)^2
    ADDI x6 x6 1         # i++
    J x0 pass2

pass2done
    FDIV f5 f1 f3        # f5 = variance = var_sum / 128.0  ->  1.25

    HLT

# -- Constants --
.loc 100
    .dataFloat 128.0

# -- Data section --
# 128 floats: pattern [1.0, 2.0, 3.0, 4.0] repeated 32 times.
.loc 200
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0
    .dataFloat 1.0
    .dataFloat 2.0
    .dataFloat 3.0
    .dataFloat 4.0