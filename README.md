# NFloat From Scratch - A Custom 32-bit Floating-Point Implementation in C

A from-scratch implementation of a custom floating-point numeric type in C, built entirely using bitwise operations. NewFloat trades IEEE 754's 8-bit exponent for a 5-bit exponent and expands the mantissa to 26 bits, squeezing out extra precision at the cost of range - and implementing all arithmetic directly on the binary representation without ever touching `float` or `double` math.

---

## Overview

IEEE 754 single-precision floats allocate 23 bits to the mantissa. NewFloat reallocates 3 of the 8 exponent bits to the mantissa instead, producing a 26-bit fraction field. The result is a floating-point format with finer fractional precision over a narrower exponent range, represented in the same 32-bit footprint as a standard `float`.

All operations - conversion, inspection, doubling, and addition - are implemented purely through bit masking, shifting, and integer arithmetic. No floating-point arithmetic is used anywhere in the core logic.

---

## Format Specification

NewFloat is stored in a `uint32_t` with the following layout:

```
┌ Sign (1 bit)
│   ┌ Exponent (5 bits, biased by 15)
│   │         Mantissa (26 bits)
│ ┌─┴─┐ ┌───────────┴────────────┐
0 00000 00000000000000000000000000
```

| Field | Bits | Details |
|-------|------|---------|
| Sign | 31 | `0` = positive, `1` = negative |
| Exponent | 30–26 | Biased by 15; actual range −14 to +16 |
| Mantissa | 25–0 | Fractional bits; leading `1` implicit when exponent ≠ 0 |

Compared to IEEE 754 `float`:

| Property | IEEE 754 `float` | NewFloat |
|----------|-----------------|----------|
| Total bits | 32 | 32 |
| Exponent bits | 8 | 5 |
| Mantissa bits | 23 | 26 |
| Exponent bias | 127 | 15 |
| Actual exp range | −126 to +127 | −14 to +16 |
| Subnormal exponent | biased `00000000` | biased `00000` |

When the biased exponent is `00000`, the value is subnormal: the leading digit is `0` rather than `1`, and the actual exponent is fixed at −14. The biased exponent `11111` (31) is reserved as overflow/saturation - the format saturates to the maximum representable value rather than encoding infinity.

---

## Features

- **Bit-string parser** - converts a 32-character `'0'`/`'1'` string directly into a `NewFloat` via serial shift-and-OR
- **Debug printer** - dumps the internal binary layout as `sign | exponent | mantissa`, space-separated, for direct inspection of the bit fields
- **IEEE 754 → NewFloat converter** - type-puns a `float` via pointer cast, extracts its sign/exponent/mantissa, rebiases the exponent to 15, and repacks into the 32-bit NewFloat layout; handles subnormals, saturation, and underflow
- **Decimal printer** - reconstructs the true value from the bit fields using integer powers of 2 and prints to 7 decimal places via `double` accumulation
- **Doubling** (`nfloat_double`) - implemented as a pure exponent increment; handles the subnormal-to-normal boundary and saturates at overflow
- **Addition** (`nfloat_add`) - full floating-point addition pipeline: exponent alignment via right-shift, signed mantissa addition/subtraction, normalization loop, subnormal demotion, and overflow saturation

---

## Implementation Details

### Type Punning for Conversion

`float_to_nfloat` accesses the raw IEEE 754 bits of a `float` without any cast that would invoke undefined behavior, by going through a `uint32_t *` pointer - a standard pattern for reading the internal representation of floating-point values in C:

```c
uint32_t *float_bits = (uint32_t *)&f;
uint32_t bits = *float_bits;
```

The sign, 8-bit exponent, and 23-bit mantissa are then extracted with masks and shifted into NewFloat's 5/26-bit layout.

### Exponent Rebias

IEEE 754 uses a bias of 127; NewFloat uses 15. Conversion subtracts 127, then adds 15:

```c
int actual_exp = (int)exponent - 127;
int target_exp = actual_exp + 15;
```

Values that underflow the 5-bit range are demoted to subnormals by right-shifting the full mantissa (with implicit leading 1 restored) until the exponent reaches the floor. Values that overflow saturate to `(sign << 31) | (30 << 26) | 0x3FFFFFF`.

### Mantissa Alignment in Addition

Before adding two NewFloat values, their mantissas must share a common exponent. The smaller operand's mantissa is right-shifted by the exponent difference, with a 64-bit intermediate (`uint64_t`) used throughout to prevent overflow during accumulation:

```c
int diff = actual_exp_first - actual_exp_second;
full_mantis_second = full_mantis_second >> diff;
```

After addition or subtraction, the result is renormalized: shifted right if the carry bit overflows bit 26, or shifted left if the result is subnormal, with the exponent adjusted in step. The final value is then repacked into the 32-bit NewFloat layout.

### Subnormal Boundary in Doubling

`nfloat_double` detects the subnormal-to-normal transition: when a subnormal mantissa's leading `1` shifts into bit 26, the exponent is set to `1` (biased) and the mantissa is shifted to match, rather than blindly incrementing the zero exponent:

```c
if (mantissa & (1 << 25)) {
    return (sign << 31) | (1 << 26) | ((mantissa << 1) & 0x3FFFFFF);
}
```

---

## Example Usage

The program reads from `stdin`, dispatching on a mode prefix (`p1`–`p4`):

| Mode | Input format | Operation |
|------|-------------|-----------|
| `p1` | 32-char bit string | Parse and debug-print internal layout |
| `p2` | `float` literal | Convert IEEE 754 → NewFloat, debug-print |
| `p3` | 32-char bit string | Parse and print decimal value |
| `p4 double` | `float` literal | Double the value, print decimal |
| `p4 add` | two `float` literals | Add two values, print decimal |

**Input:**
```
p1 00010011111111111111000000000000
p2 3.9
p3 01010000111110110000100101111110
p4 double 3.14
p4 add 3.4 -2.9
```

**Output:**
```
0 00100 11111111111111000000000000
0 10000 11110011001100110011010000
39.8449087
6.2800002
0.5000000
```

---

## Building & Running

### Prerequisites

- GCC with C99 support (`-std=c99`)

### Compile

```bash
gcc -Wall -Werror -std=c99 -o nfloat nfloat.c
```

Or with the included Makefile:

```bash
make
```

### Run

```bash
./nfloat < input.txt
```

Or interactively:

```bash
./nfloat
p1 00010011111111111111000000000000
p4 add 3.14 3.14
```

---

## Project Structure

```
.
├── src/
│   └── nfloat.c          # Full implementation - parser, converter, printer, arithmetic
├── Testcases/            # Input/output pairs covering all four operation modes
├── Makefile
└── README.md
```

---

## Future Improvements

- **Rounding modes** - currently truncates on mantissa shifts; add round-to-nearest-even (banker's rounding) to reduce accumulated error across multi-step calculations
- **Multiplication and division** - extend the arithmetic suite; multiplication is a natural next step since it only requires mantissa multiply and exponent add, without the alignment complexity of addition
- **Infinity and NaN encoding** - use the reserved biased exponent `11111` for ±infinity and quiet NaN instead of saturating, bringing the format closer to IEEE 754 semantics
- **Extended precision variant** - parametrize the exponent/mantissa split at compile time to explore the precision-range tradeoff across different bit allocations
- **Hardware simulation** - model the addition pipeline as discrete stages (align, add, normalize, round) to mirror how an FPU would implement this in logic gates
