#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Custom 32-bit float: 1 sign bit, 5 exponent bits, 26 mantissa bits
typedef uint32_t NewFloat;

// Largest finite value: exponent 30 (11110) with all 26 mantissa bits set.
// Exponent 31 is treated as overflow, so results saturate to this instead.
#define NFLOAT_MAX_MAGNITUDE ((30u << 26) | 0x3FFFFFFu)

int is_valid_bit_string(char *s);
NewFloat bits_string_to_nfloat(char *s);
void nfloat_debug(NewFloat f);
NewFloat float_to_nfloat(float f);
void nfloat_print(NewFloat f);
NewFloat nfloat_double(NewFloat f);
NewFloat nfloat_add(NewFloat a, NewFloat b);


int main(void) {
    int part_num;       // which mode the line asks for (p1 to p4)
    char s1[40];        // a bit string, or the word "double" / "add"
    float f1, f2;       // standard floats read from input
    NewFloat n1, n2;    // the same values in our format

    // Each line starts with p1..p4. Keep going until input ends.
    while (scanf(" p%d", &part_num) == 1) {
        if (part_num == 1) {
            // p1: bit string in, raw fields out.
            // %39s reads at most 39 chars, so s1 can't overflow.
            if (scanf("%39s", s1) != 1 || !is_valid_bit_string(s1)) {
                fprintf(stderr, "p1: expected a 32-character string of 0s and 1s\n");
                return 1;
            }

            n1 = bits_string_to_nfloat(s1);
            nfloat_debug(n1);
        } else if (part_num == 2) {
            // p2: normal float in, raw fields out
            if (scanf("%f", &f1) != 1) {
                fprintf(stderr, "p2: expected a number\n");
                return 1;
            }

            n1 = float_to_nfloat(f1);
            nfloat_debug(n1);
        } else if (part_num == 3) {
            // p3: bit string in, decimal value out
            if (scanf("%39s", s1) != 1 || !is_valid_bit_string(s1)) {
                fprintf(stderr, "p3: expected a 32-character string of 0s and 1s\n");
                return 1;
            }

            n1 = bits_string_to_nfloat(s1);
            nfloat_print(n1);
        } else if (part_num == 4) {
            // p4: arithmetic, either "double x" or "add x y"
            if (scanf("%39s", s1) != 1) {
                fprintf(stderr, "p4: expected 'double' or 'add'\n");
                return 1;
            }

            if (strcmp(s1, "double") == 0) {
                if (scanf("%f", &f1) != 1) {
                    fprintf(stderr, "p4 double: expected a number\n");
                    return 1;
                }

                // Convert, then multiply by 2 using only bit operations
                n1 = float_to_nfloat(f1);
                n1 = nfloat_double(n1);
            } else if (strcmp(s1, "add") == 0) {
                if (scanf("%f %f", &f1, &f2) != 2) {
                    fprintf(stderr, "p4 add: expected two numbers\n");
                    return 1;
                }

                // Convert both, then add using only bit operations
                n1 = float_to_nfloat(f1);
                n2 = float_to_nfloat(f2);
                n1 = nfloat_add(n1, n2);
            } else {
                fprintf(stderr, "p4: unknown operation '%s', expected 'double' or 'add'\n", s1);
                return 1;
            }

            nfloat_print(n1);
        } else {
            fprintf(stderr, "unknown part p%d, expected p1 to p4\n", part_num);
            return 1;
        }
    }

    return 0;
}


// Returns 1 if s is exactly 32 characters, all '0' or '1'.
int is_valid_bit_string(char *s) {
    // One character per bit, so the length must be exactly 32
    if (strlen(s) != 32) {
        return 0;
    }

    for (int i = 0; i < 32; i++) {
        if (s[i] != '0' && s[i] != '1') {
            return 0;
        }
    }

    return 1;
}


// Packs a string like "0101..." into a 32-bit value.
// The first character becomes bit 31, the last becomes bit 0.
NewFloat bits_string_to_nfloat(char *s) {
    NewFloat result = 0;

    for (int i = 0; i < 32; i++) {
        // Shift everything left to make room at bit 0
        result = result << 1;

        // Then set bit 0 if this character is a '1'
        if (s[i] == '1') {
            result = result | 1;
        }
    }

    return result;
}


// Prints the three fields separately: "sign exponent mantissa".
void nfloat_debug(NewFloat f) {
    // Sign: bit 31
    printf("%u ", (unsigned int)((f >> 31) & 1));

    // Exponent: bits 30 down to 26.
    // Shift each bit down to position 0 and mask it off.
    for (int i = 30; i >= 26; i--) {
        printf("%u", (unsigned int)((f >> i) & 1));
    }

    printf(" ");

    // Mantissa: bits 25 down to 0
    for (int i = 25; i >= 0; i--) {
        printf("%u", (unsigned int)((f >> i) & 1));
    }

    printf("\n");
}


// Converts an IEEE 754 float (1/8/23, bias 127) into our 1/5/26 format (bias 15).
NewFloat float_to_nfloat(float f) {
    // Copy the float's raw bits into an integer.
    // memcpy instead of a pointer cast, because the cast breaks strict aliasing (UB).
    uint32_t bits;
    memcpy(&bits, &f, sizeof bits);

    // Split the IEEE fields
    uint32_t sign = (bits >> 31) & 1;           // bit 31
    uint32_t exponent = (bits >> 23) & 0xFF;    // bits 30-23
    uint32_t mantissa = bits & 0x7FFFFF;        // bits 22-0

    // IEEE zero or subnormal: far below our smallest value, so it becomes zero.
    // Keep the sign, so -0 stays -0.
    if (exponent == 0) {
        return sign << 31;
    }

    // IEEE exponent all ones means infinity or NaN
    if (exponent == 0xFF) {
        // Non-zero mantissa means NaN. We have no NaN encoding, so use zero.
        if (mantissa != 0) {
            return 0;
        }

        // Infinity saturates to the largest value
        return (sign << 31) | NFLOAT_MAX_MAGNITUDE;
    }

    // Rebias: remove IEEE's bias of 127, then add our bias of 15
    int actual_exp = (int)exponent - 127;
    int target_exp = actual_exp + 15;

    // Too small to be normal in our format, so store it as a subnormal
    if (target_exp <= 0) {
        // Subnormals use a fixed exponent of -14, so shift the mantissa
        // right by however far below that the real exponent is
        int shift = 1 - target_exp;
        uint32_t new_mantissa;

        if (shift >= 27) {
            // Every bit would be shifted out, so it underflows to zero
            new_mantissa = 0;
        } else {
            // Put back the hidden leading 1, widen 23 bits to 26, then shift
            uint32_t full_mantissa = (1u << 23) | mantissa;
            new_mantissa = (full_mantissa << 3) >> shift;
        }

        // Exponent field stays 0, which marks a subnormal
        return (sign << 31) | new_mantissa;
    }

    // Too big for our 5-bit exponent, so saturate
    if (target_exp >= 31) {
        return (sign << 31) | NFLOAT_MAX_MAGNITUDE;
    }

    // Normal case: same fraction, padded with 3 zero bits at the bottom
    uint32_t new_mantissa = mantissa << 3;

    return (sign << 31) | ((uint32_t)target_exp << 26) | new_mantissa;
}


// Prints the value in decimal with 7 digits after the point.
// Uses only integer math, rounding the last digit half-to-even.
void nfloat_print(NewFloat f) {
    uint32_t sign = (f >> 31) & 1;          // bit 31
    uint32_t exponent = (f >> 26) & 0x1F;   // bits 30-26
    uint32_t mantissa = f & 0x3FFFFFF;      // bits 25-0

    // Treat the number as one big integer (significand) with a binary point
    // somewhere inside it. fraction_bits says how many low bits are after the point.
    uint64_t significand;
    int fraction_bits;

    if (exponent == 0) {
        // Subnormal: 0.mantissa * 2^-14, no hidden 1.
        // 26 mantissa bits plus 14 more for the 2^-14.
        significand = mantissa;
        fraction_bits = 26 + 14;
    } else {
        // Normal: 1.mantissa * 2^(exponent - 15).
        // A bigger exponent moves the binary point to the right.
        significand = (1ULL << 26) | mantissa;
        fraction_bits = 26 - ((int)exponent - 15);
    }

    // Split at the binary point
    uint64_t fraction_mask = (1ULL << fraction_bits) - 1;
    uint64_t integer_part = significand >> fraction_bits;

    // Multiply the fraction by 10^7. The bits that move above the
    // binary point are the 7 decimal digits we print.
    uint64_t scaled_fraction = (significand & fraction_mask) * 10000000ULL;
    uint64_t decimal_digits = scaled_fraction >> fraction_bits;

    // What's left below the point decides the rounding of the 7th digit
    uint64_t remainder = scaled_fraction & fraction_mask;
    uint64_t half = 1ULL << (fraction_bits - 1);

    if (remainder > half) {
        // More than half: round up
        decimal_digits++;
    } else if (remainder == half && (decimal_digits & 1)) {
        // Exactly half and the last digit is odd: round to the even digit
        decimal_digits++;
    }

    // Rounding can carry into the integer part, e.g. 0.99999995 -> 1.0000000
    if (decimal_digits == 10000000ULL) {
        integer_part++;
        decimal_digits = 0;
    }

    if (sign) {
        printf("-");
    }

    // %07 keeps leading zeros, so 0.05 prints as 0.0500000
    printf("%llu.%07llu\n", (unsigned long long)integer_part, (unsigned long long)decimal_digits);
}


// Multiplies by 2 using only bit operations.
NewFloat nfloat_double(NewFloat f) {
    uint32_t sign = (f >> 31) & 1;          // bit 31
    uint32_t exponent = (f >> 26) & 0x1F;   // bits 30-26
    uint32_t mantissa = f & 0x3FFFFFF;      // bits 25-0

    // Subnormal: the exponent is fixed, so double the mantissa instead
    if (exponent == 0) {
        // If the top mantissa bit is set, shifting left pushes it into the
        // hidden-1 position, so the number becomes normal with exponent 1
        if (mantissa & (1u << 25)) {
            return (sign << 31) | (1u << 26) | ((mantissa << 1) & 0x3FFFFFF);
        }

        // Otherwise it stays subnormal, just twice as big
        return (sign << 31) | (mantissa << 1);
    }

    // Normal: doubling is just exponent + 1, mantissa unchanged
    uint32_t new_exp = exponent + 1;

    // Exponent would overflow, so saturate
    if (new_exp >= 31) {
        return (sign << 31) | NFLOAT_MAX_MAGNITUDE;
    }

    return (sign << 31) | (new_exp << 26) | mantissa;
}


// Adds two NewFloats using only integer and bit operations.
NewFloat nfloat_add(NewFloat a, NewFloat b) {
    // Step 1: unpack both numbers into sign, exponent and mantissa

    uint32_t sign_first = (a >> 31) & 1;
    uint32_t exp_first = (a >> 26) & 0x1F;
    uint32_t mantis_first = a & 0x3FFFFFF;

    uint32_t sign_second = (b >> 31) & 1;
    uint32_t exp_second = (b >> 26) & 0x1F;
    uint32_t mantis_second = b & 0x3FFFFFF;

    // 64-bit so the addition below can't overflow
    uint64_t full_mantis_first;
    uint64_t full_mantis_second;

    // Normal numbers get their hidden 1 back at bit 26. Subnormals have none.
    if (exp_first == 0) {
        full_mantis_first = mantis_first;
    } else {
        full_mantis_first = (1ULL << 26) | mantis_first;
    }

    if (exp_second == 0) {
        full_mantis_second = mantis_second;
    } else {
        full_mantis_second = (1ULL << 26) | mantis_second;
    }

    // Real (unbiased) exponents. Subnormals always use -14.
    int actual_exp_first;
    int actual_exp_second;

    if (exp_first == 0) {
        actual_exp_first = -14;
    } else {
        actual_exp_first = (int)exp_first - 15;
    }

    if (exp_second == 0) {
        actual_exp_second = -14;
    } else {
        actual_exp_second = (int)exp_second - 15;
    }

    // Step 2: align. Shift the smaller number right until both exponents match.

    if (actual_exp_first > actual_exp_second) {
        // Second number is smaller
        int diff = actual_exp_first - actual_exp_second;

        // Shifting a 64-bit value by 64 or more is undefined behavior
        if (diff < 64) {
            full_mantis_second = full_mantis_second >> diff;
        } else {
            full_mantis_second = 0;
        }

        actual_exp_second = actual_exp_first;
    } else if (actual_exp_second > actual_exp_first) {
        // First number is smaller
        int diff = actual_exp_second - actual_exp_first;

        if (diff < 64) {
            full_mantis_first = full_mantis_first >> diff;
        } else {
            full_mantis_first = 0;
        }

        actual_exp_first = actual_exp_second;
    }

    // Step 3: add or subtract the aligned mantissas

    uint64_t result_mantis;
    uint32_t result_sign;
    int result_exp = actual_exp_first;    // both exponents are equal now

    if (sign_first == sign_second) {
        // Same signs: the magnitudes add
        result_mantis = full_mantis_first + full_mantis_second;
        result_sign = sign_first;
    } else {
        // Different signs: subtract the smaller magnitude from the larger,
        // and the result takes the larger one's sign
        if (full_mantis_first >= full_mantis_second) {
            result_mantis = full_mantis_first - full_mantis_second;
            result_sign = sign_first;
        } else {
            result_mantis = full_mantis_second - full_mantis_first;
            result_sign = sign_second;
        }
    }

    // Exact cancellation, e.g. 2.5 + (-2.5).
    // Only -0 + -0 gives -0. Everything else gives +0, like IEEE.
    if (result_mantis == 0) {
        if (sign_first == 1 && sign_second == 1) {
            return 1u << 31;
        }

        return 0;
    }

    // Step 4: normalize so the leading 1 sits at bit 26

    // A carry went past bit 26: shift right and raise the exponent
    while (result_mantis >= (1ULL << 27)) {
        result_mantis = result_mantis >> 1;
        result_exp++;
    }

    // Leading 1 is too low: shift left and lower the exponent,
    // but stop at -14, the smallest exponent we have
    while (result_mantis < (1ULL << 26) && result_exp > -14) {
        result_mantis = result_mantis << 1;
        result_exp--;
    }

    // Step 5: repack into sign, biased exponent and mantissa

    // Still no 1 at bit 26 means the result is subnormal (exponent field 0)
    int biased_exp;

    if (result_mantis < (1ULL << 26)) {
        biased_exp = 0;
    } else {
        biased_exp = result_exp + 15;
    }

    // Too big for the exponent field, so saturate
    if (biased_exp >= 31) {
        return (result_sign << 31) | NFLOAT_MAX_MAGNITUDE;
    }

    // & 0x3FFFFFF drops the hidden 1 at bit 26 before packing
    return (result_sign << 31) | ((uint32_t)biased_exp << 26) | (uint32_t)(result_mantis & 0x3FFFFFF);
}
