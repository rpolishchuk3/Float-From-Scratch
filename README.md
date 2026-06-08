# Float From Scratch - Custom 32-bit Floating-Point in C

A hand-built floating-point numeric type implemented entirely in C using bitwise operations. No `float` or `double` arithmetic - every operation, from conversion to addition, works directly on the binary representation.

---

## Motivation

IEEE 754 is the universal standard for floating-point arithmetic, but it's a set of engineering tradeoffs, not laws of nature. The 8-bit exponent in a 32-bit `float` gives you a range of roughly ±3.4 × 10³⁸ - far more than most applications ever need - while leaving only 23 bits for the mantissa. I wanted to explore what happens when you flip that tradeoff: shrink the exponent, expand the mantissa, and see what you gain in precision and what you give up in range.

Beyond the design question, I wanted to actually build it - not just read about it. That means writing the conversion logic, implementing addition with proper exponent alignment and normalization, and handling edge cases like subnormals and overflow entirely through bit manipulation. No shortcuts, no casting to `double` to do the math.

Custom float formats like this come up in real contexts: ML accelerators use low-bit formats (bfloat16, FP8) to maximize throughput. Embedded systems work with fixed-width types where range is predictable and precision matters more. Graphics hardware historically used reduced-exponent formats for color math. The tradeoffs here are the same ones engineers make in those domains.

---

## Real-World Relevance

This project explores how floating-point numbers are represented and manipulated at a low level, which is critical in systems where precision, performance, and determinism matter.

### Why This Matters

- **Embedded & Low-Level Systems**: Many embedded systems (e.g., IoT devices, microcontrollers) lack full IEEE-754 support or require optimized numeric formats. Implementing a custom float type demonstrates how numerical computation can be adapted to constrained environments.
- **Computer Architecture & Compilers**: This project directly relates to how CPUs handle arithmetic operations at the hardware level and how compilers translate high-level math into machine instructions.
- **Graphics & Game Development**: Efficient floating-point operations are essential in rendering pipelines, physics engines, and real-time simulations where performance is critical.

### Key Takeaway

By building a floating-point system from scratch, this project bridges the gap between high-level programming and hardware-level computation, providing a deeper understanding of how numerical data is stored, processed, and optimized in real-world systems.

---

## Overview

The custom type - `NewFloat` - is a `uint32_t` under the hood, with its 32 bits split as **1 sign / 5 exponent / 26 mantissa** instead of IEEE 754's 1/8/23. Three exponent bits move to the mantissa, trading a dynamic range of 10⁷⁶ down to roughly 10⁹ in exchange for finer fractional resolution.

The full implementation covers:
- Parsing raw bit strings into the format
- Converting standard IEEE 754 `float` values across, including subnormal handling
- Printing the stored value as a decimal
- Doubling and adding two `NewFloat` values using a proper FPU-style pipeline

---

## Format Specification

```
Bit 31     Bits 30–26     Bits 25–0
  │            │               │
Sign (1)   Exponent (5)   Mantissa (26)
```

| Field | Width | Encoding |
|-------|-------|----------|
| Sign | 1 bit | `0` = positive, `1` = negative |
| Exponent | 5 bits | Biased by 15, actual range −14 to +16 |
| Mantissa | 26 bits | Fractional bits, implicit leading `1` when exp ≠ 0 |

Side-by-side with IEEE 754 `float`:

| | IEEE 754 `float` | `NewFloat` |
|--|--|--|
| Exponent bits | 8 | 5 |
| Mantissa bits | 23 | **26** |
| Exponent bias | 127 | 15 |
| Exp range | −126 to +127 | −14 to +16 |
| Subnormal threshold | biased exp = `0x00` | biased exp = `0b00000` |
| Overflow encoding | ±infinity (`0xFF`) | saturates to max value |

When the biased exponent is `00000`, the value is subnormal: the leading digit is `0` and the actual exponent is fixed at −14. Rather than encoding infinity, the format saturates - overflow clamps to the largest representable value.

---

## Features

- **Bit-string parser** - reads a 32-char `'0'`/`'1'` string and packs it into a `NewFloat` via shift-and-OR
- **Debug printer** - prints the raw `sign | exponent | mantissa` bit fields, space-separated
- **IEEE 754 converter** - type-puns a `float`, extracts its fields, rebiases the exponent, and repacks into the `NewFloat` layout. Handles subnormals, underflow, and overflow
- **Decimal printer** - reconstructs the true value bit-by-bit using powers of 2, printed to 7 decimal places
- **Doubling** - pure exponent increment with correct subnormal-to-normal boundary promotion
- **Addition** - full pipeline: align exponents, add or subtract mantissas, normalize, handle subnormals and saturation

---

## Implementation Highlights

### Accessing IEEE 754 bits without undefined behavior

To read the raw binary of a `float`, I type-pun through a `uint32_t *` pointer - the standard C approach for inspecting floating-point internals:

```c
uint32_t *float_bits = (uint32_t *)&f;
uint32_t bits = *float_bits;

uint32_t sign     = (bits >> 31) & 1;
uint32_t exponent = (bits >> 23) & 0xFF;
uint32_t mantissa =  bits        & 0x7FFFFF;
```

### Exponent rebias

IEEE 754 biases by 127. `NewFloat` biases by 15. The conversion is a subtract-and-add:

```c
int actual_exp = (int)exponent - 127;
int target_exp  = actual_exp + 15;
```

If `target_exp` underflows (≤ 0), the implicit leading `1` is shifted into the mantissa to produce a subnormal. If it overflows (≥ 31), the result saturates.

### Addition: align → combine → normalize

The trickiest operation. Both operands are expanded to include their implicit leading bit, then the smaller one is right-shifted to match the larger exponent:

```c
int diff = actual_exp_first - actual_exp_second;
full_mantis_second >>= diff;
```

A `uint64_t` intermediate prevents overflow during the addition. After combining, the result is normalized - right-shifted if a carry overflows bit 26, left-shifted if the result is subnormal - then repacked into 32 bits.

### Subnormal promotion in doubling

When a subnormal's leading `1` bit shifts into the normalized position (bit 26), the exponent transitions from 0 to 1 rather than staying subnormal:

```c
if (mantissa & (1 << 25)) {
    return (sign << 31) | (1 << 26) | ((mantissa << 1) & 0x3FFFFFF);
}
```

---

## Example Usage

The program reads from `stdin` with a mode prefix on each line:

| Prefix | Input | Operation |
|--------|-------|-----------|
| `p1` | 32-char bit string | Parse → debug print |
| `p2` | `float` literal | Convert IEEE 754 → debug print |
| `p3` | 32-char bit string | Parse → decimal print |
| `p4 double` | `float` literal | Double → decimal print |
| `p4 add` | two `float` literals | Add → decimal print |

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

**Conversion walkthrough - `3.9` as IEEE 754 then as `NewFloat`:**

```
IEEE 754:  0 10000000 11110011001100110011010   (exp bias 127 → actual 1)
NewFloat:  0 10000     11110011001100110011010000 (exp bias 15 → actual 1)
                       └─ 3 extra mantissa bits ─┘
```

The exponent encodes the same value, the mantissa gains 3 bits of fractional precision.

---

## Build & Run

**Requirements:** GCC, C99

```bash
# Compile
gcc -Wall -Werror -std=c99 -o nfloat nfloat.c

# Or via Makefile
make

# Run with a file
./nfloat < input.txt

# Run interactively
./nfloat
p2 3.14
p4 add 1.5 -0.25
```

---

## Project Structure

```
.
├── src/
│   └── nfloat.c       # Parser, converter, printer, arithmetic
├── Testcases/         # Input/output pairs for all four modes
├── Makefile
└── README.md
```

---

## Limitations

- **Truncation only** - mantissa shifts truncate rather than round, so error accumulates across chained operations
- **No infinity or NaN** - overflow saturates to the max value. There is no signaling mechanism for invalid results
- **Narrow range** - actual exponents of −14 to +16 cover values from ~0.00006 to ~65536. Anything outside that saturates or underflows to zero
- **No multiplication or division** - the arithmetic suite covers doubling and addition only

---

## Future Improvements

- **Round-to-nearest-even** - replace truncation with proper banker's rounding to minimize cumulative error
- **Multiplication and division** - multiplication is the natural next operation: add exponents, multiply mantissas, normalize
- **Infinity and NaN encoding** - repurpose biased exponent `11111` for ±∞ and quiet NaN, matching IEEE 754 semantics
- **Parametric format** - make the exponent/mantissa split a compile-time constant to explore the precision-range tradeoff across different configurations (e.g. 3/28, 6/25)
- **FPU pipeline model** - decompose addition into explicit stages (align, add, normalize, round) to mirror how hardware floating-point units are designed

---

## Key Takeaways

- **Floating-point is a set of tradeoffs, not a fixed standard.** Shifting bits between the exponent and mantissa directly changes what a format is good at - this is exactly the design space that ML hardware teams work in with bfloat16 and FP8.
- **Subnormals are genuinely subtle.** The boundary between subnormal and normal behavior requires special-casing in every operation: conversion, doubling, addition, and printing all need to handle it separately.
- **Type punning gives you direct access to the machine representation.** Reading a `float`'s bits through a `uint32_t *` is the standard low-level technique and understanding why it works (and why alternatives like direct cast are UB) is fundamental to systems programming in C.
- **Addition is the hard operation.** Alignment, signed subtraction, normalization, subnormal demotion, and saturation all have to be handled in sequence - it's a complete mini-pipeline, which is why FPUs dedicate significant silicon to it.
