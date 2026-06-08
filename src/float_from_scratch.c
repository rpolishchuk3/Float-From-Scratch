#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

typedef uint32_t NewFloat;

NewFloat bits_string_to_nfloat(char *s);
void nfloat_debug(NewFloat f);
NewFloat float_to_nfloat(float f);
void nfloat_print(NewFloat f);
NewFloat nfloat_double(NewFloat f);
NewFloat nfloat_add(NewFloat a, NewFloat b);

int main(void) {
    int part_num;
    char s1[40];
    float f1, f2;
    NewFloat n1, n2;
    
    while (1) {
        int res = scanf(" p%d", &part_num);
        
        if (res != 1) {
            
            break;
        } else if (part_num == 1) {
            scanf("%s", s1);
            n1 = bits_string_to_nfloat(s1);
            nfloat_debug(n1);
        } else if (part_num == 2) {
            scanf("%f", &f1);
            n1 = float_to_nfloat(f1);
            nfloat_debug(n1);
        } else if (part_num == 3) {
            scanf("%s", s1);
            n1 = bits_string_to_nfloat(s1);
            nfloat_print(n1);
        } else {
            scanf("%s ", s1);
            
            if (s1[0] == 'd') {
                scanf("%f", &f1);
                n1 = float_to_nfloat(f1);
                n1 = nfloat_double(n1);
            } else {
                scanf("%f %f", &f1, &f2);
                n1 = float_to_nfloat(f1);
                n2 = float_to_nfloat(f2);
                n1 = nfloat_add(n1, n2);
            }
            
            nfloat_print(n1);
        }
    }
}

NewFloat bits_string_to_nfloat(char *s) {
    NewFloat result = 0;
    
    for (int i = 0; i < 32; i++) {
        result = result << 1;
        
        if (s[i] == '1') {
            result = result | 1;
        }
    }
    
    return result;
}

void nfloat_debug(NewFloat f) {
    printf("%u ", (f >> 31) & 1);
    
    for (int i = 30; i >= 26; i--) {
        printf("%u", (f >> i) & 1);
    }
    
    printf(" ");
    
    for (int i = 25; i >= 0; i--) {
        printf("%u", (f >> i) & 1);
    }
    
    printf("\n");
}

NewFloat float_to_nfloat(float f) {
    uint32_t *float_bits = (uint32_t *)&f;
    uint32_t bits = *float_bits;
    
    uint32_t sign = (bits >> 31) & 1;
    uint32_t exponent = (bits >> 23) & 0xFF;
    uint32_t mantissa = bits & 0x7FFFFF;
    
    if (exponent == 0) {
        
        return sign << 31;
    }
    
    if (exponent == 0xFF) {
        
        return sign << 31;
    }
    
    int actual_exp = (int)exponent - 127;
    int target_exp = actual_exp + 15;
    
    if (target_exp <= 0) {
        int shift = 1 - target_exp;
        uint32_t new_mantissa;
        
        if (shift >= 27) {
            new_mantissa = 0;
        } else {
            uint32_t full_mantissa = (1 << 23) | mantissa;
            new_mantissa = (full_mantissa << 3) >> shift;
        }
        
        return (sign << 31) | new_mantissa;
    } else if (target_exp >= 31) {
        
        return (sign << 31) | (30 << 26) | 0x3FFFFFF;
    }
    
    uint32_t new_mantissa = mantissa << 3;
    
    return (sign << 31) | (target_exp << 26) | new_mantissa;
}

void nfloat_print(NewFloat f) {
    uint32_t sign = (f >> 31) & 1;
    uint32_t exponent = (f >> 26) & 0x1F;
    uint32_t mantissa = f & 0x3FFFFFF;
    
    double value = 0.0;
    
    if (exponent == 0) {
        value = 0.0;
        
        for (int i = 0; i < 26; i++) {
            if (mantissa & (1 << (25 - i))) {
                value += 1.0 / (1 << (i + 1));
            }
        }
        
        value = value / (1 << 14);
    } else {
        int actual_exp = (int)exponent - 15;
        value = 1.0;
        
        for (int i = 0; i < 26; i++) {
            if (mantissa & (1 << (25 - i))) {
                value += 1.0 / (1 << (i + 1));
            }
        }
        
        if (actual_exp >= 0) {
            value = value * (1 << actual_exp);
        } else {
            value = value / (1 << (-actual_exp));
        }
    }
    
    if (sign) {
        value = -value;
    }
    
    printf("%.7f\n", value);
}

NewFloat nfloat_double(NewFloat f) {
    uint32_t sign = (f >> 31) & 1;
    uint32_t exponent = (f >> 26) & 0x1F;
    uint32_t mantissa = f & 0x3FFFFFF;
    
    if (exponent == 0) {
        if (mantissa & (1 << 25)) {
            
            return (sign << 31) | (1 << 26) | ((mantissa << 1) & 0x3FFFFFF);
        } else {
            
            return (sign << 31) | (mantissa << 1);
        }
    }
    
    uint32_t new_exp = exponent + 1;
    
    if (new_exp >= 31) {
        
        return (sign << 31) | (30 << 26) | 0x3FFFFFF;
    }
    
    return (sign << 31) | (new_exp << 26) | mantissa;
}

NewFloat nfloat_add(NewFloat a, NewFloat b) {
    uint32_t sign_first = (a >> 31) & 1;
    uint32_t exp_first = (a >> 26) & 0x1F;
    uint32_t mantis_first = a & 0x3FFFFFF;
    
    uint32_t sign_second = (b >> 31) & 1;
    uint32_t exp_second = (b >> 26) & 0x1F;
    uint32_t mantis_second = b & 0x3FFFFFF;
    
    uint64_t full_mantis_first;
    uint64_t full_mantis_second;
    
    if (exp_first == 0) {
        full_mantis_first = mantis_first;
    } else {
        full_mantis_first = (1 << 26) | mantis_first;
    }
    
    if (exp_second == 0) {
        full_mantis_second = mantis_second;
    } else {
        full_mantis_second = (1 << 26) | mantis_second;
    }
    
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
    
    if (actual_exp_first > actual_exp_second) {
        int diff = actual_exp_first - actual_exp_second;
        
        if (diff < 64) {
            full_mantis_second = full_mantis_second >> diff;
        } else {
            full_mantis_second = 0;
        }
        
        actual_exp_second = actual_exp_first;
    } else if (actual_exp_second > actual_exp_first) {
        int diff = actual_exp_second - actual_exp_first;
        
        if (diff < 64) {
            full_mantis_first = full_mantis_first >> diff;
        } else {
            full_mantis_first = 0;
        }
        
        actual_exp_first = actual_exp_second;
    }
    
    uint64_t result_mantis;
    uint32_t result_sign;
    int result_exp = actual_exp_first;
    
    if (sign_first == sign_second) {
        result_mantis = full_mantis_first + full_mantis_second;
        result_sign = sign_first;
    } else {
        if (full_mantis_first >= full_mantis_second) {
            result_mantis = full_mantis_first - full_mantis_second;
            result_sign = sign_first;
        } else {
            result_mantis = full_mantis_second - full_mantis_first;
            result_sign = sign_second;
        }
    }
    
    if (result_mantis == 0) {
        
        return 0;
    }
    
    while (result_mantis >= (1ULL << 27)) {
        result_mantis = result_mantis >> 1;
        result_exp++;
    }
    
    while (result_mantis < (1ULL << 26) && result_exp > -14) {
        result_mantis = result_mantis << 1;
        result_exp--;
    }
    
    int biased_exp = result_exp + 15;
    
    if (biased_exp <= 0) {
        int shift = 1 - biased_exp;
        result_mantis = result_mantis >> shift;
        biased_exp = 0;
    } else if (biased_exp >= 31) {
        
        return (result_sign << 31) | (30 << 26) | 0x3FFFFFF;
    }
    
    if (biased_exp > 0) {
        result_mantis = result_mantis & 0x3FFFFFF;
    }
    
    return (result_sign << 31) | (biased_exp << 26) | (result_mantis & 0x3FFFFFF);
}
