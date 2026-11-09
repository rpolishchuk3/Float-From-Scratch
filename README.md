# Float From Scratch - Custom 32-bit Floating-Point in C

A hand-built floating-point numeric type implemented entirely in C using bitwise operations. Input is parsed by `scanf` into an ordinary `float`. After that, conversion, arithmetic and decimal printing all work on the bits with integer operations - no `float` or `double` arithmetic.

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
| Exponent | 5 bits | Biased by 15, actual range −14 to +15 (`11111` is never produced) |
| Mantissa | 26 bits | Fractional bits, implicit leading `1` when exp ≠ 0 |

Side-by-side with IEEE 754 `float`:

| | IEEE 754 `float` | `NewFloat` |
|--|--|--|
| Exponent bits | 8 | 5 |
| Mantissa bits | 23 | **26** |
| Exponent bias | 127 | 15 |
| Exp range | −126 to +127 | −14 to +15 |
| Subnormal threshold | biased exp = `0x00` | biased exp = `0b00000` |
| Overflow encoding | ±infinity (`0xFF`) | saturates to max value |

When the biased exponent is `00000`, the value is subnormal: the leading digit is `0` and the actual exponent is fixed at −14. Rather than encoding infinity, the format saturates - overflow clamps to the largest representable value, `0 11110 111…1` (65535.9995). An IEEE infinity converts to that value with its sign; NaN converts to `+0`.

---

## Features

- **Bit-string parser** - reads a 32-char `'0'`/`'1'` string and packs it into a `NewFloat` via shift-and-OR
- **Debug printer** - prints the raw `sign | exponent | mantissa` bit fields, space-separated
- **IEEE 754 converter** - copies a `float`'s bits into a `uint32_t` with `memcpy`, extracts its fields, rebiases the exponent, and repacks into the `NewFloat` layout. Handles subnormals, underflow, overflow and infinity
- **Decimal printer** - prints the exact stored value to 7 decimal places using only 64-bit integer arithmetic, rounding half to even
- **Doubling** - pure exponent increment with correct subnormal-to-normal boundary promotion
- **Addition** - full pipeline: align exponents, add or subtract mantissas, normalize, handle subnormals and saturation

---

## Implementation Highlights

### Reading a float's bits without undefined behavior

Reading a `float` through a `uint32_t *` breaks C's strict aliasing rule. That is undefined behavior: the optimizer may assume a `float` and a `uint32_t` never share memory. Copying the bytes with `memcpy` is the defined way, and compilers turn it into a single register move:

```c
uint32_t bits;
memcpy(&bits, &f, sizeof bits);

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

A `uint64_t` intermediate prevents overflow during the addition. After combining, the result is normalized: shifted right if the sum carries past bit 26, shifted left until the leading 1 reaches bit 26 or the exponent reaches −14. If the leading 1 still hasn't reached bit 26, the result is subnormal and is stored with biased exponent `00000`. Otherwise it is repacked with its rebiased exponent.

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

The exponent encodes the same value. The 3 extra mantissa bits are `000`: `scanf` already rounded the input to a 23-bit `float`, so conversion can't recover precision that was never there. The extra bits are used by arithmetic results - when addition shifts the smaller operand right to align exponents, its low bits land in them instead of being dropped.

---

## Build & Run

**Requirements:** a C99 compiler (clang on macOS, or GCC), make

Run everything from the repository root.

| Command | What it does |
|---------|--------------|
| `make` | Builds `nfloat` from `src/nfloat.c` |
| `make test` | Runs every `Testcases/*-input.txt` and compares against the matching `-output.txt` |
| `make clean` | Removes the binary and test output |

Building by hand:

```bash
cc -std=c99 -Wall -Wextra -o nfloat src/nfloat.c
```

Running:

```bash
./nfloat < Testcases/1-4-input.txt
```

Or run `./nfloat` and type lines such as `p2 3.14` or `p4 add 1.5 -0.25`, then press Ctrl-D to finish.

Malformed input (a bit string that isn't exactly 32 `0`/`1` characters, an unknown `pN` prefix, an unknown `p4` operation, or a missing number) prints an error to stderr and exits with status 1.

---

## Project Structure

```
.
├── src/
│   └── nfloat.c       # Parser, converter, printer, arithmetic
├── Testcases/         # Input/output pairs: 1-1 to 1-4 for p1-p4, 1-5 for edge cases
├── Makefile
└── README.md
```

---

## Limitations

- **Truncation only** - mantissa shifts truncate rather than round, so error accumulates across chained operations
- **No infinity or NaN** - overflow and infinity saturate to the max value, and NaN becomes `+0`. There is no signaling mechanism for invalid results
- **Narrow range** - normal values run from 2⁻¹⁴ (~0.000061) to just under 2¹⁶ (65535.9995). Subnormals extend down to 2⁻⁴⁰ (~9.1 × 10⁻¹³) with fewer significant bits. Anything smaller becomes zero, anything larger saturates
- **Input precision is capped by `float`** - inputs are parsed into a 23-bit-mantissa `float` first, so converted values never use the 3 extra mantissa bits
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
- **Reading a float's bits is a strict aliasing trap.** Casting `&f` to `uint32_t *` compiles and usually works, but it is undefined behavior and can break under optimization. `memcpy` (or a union, in C99 and later) is the defined way to reach the representation.
- **Addition is the hard operation.** Alignment, signed subtraction, normalization, subnormal demotion, and saturation all have to be handled in sequence - it's a complete mini-pipeline, which is why FPUs dedicate significant silicon to it.
