#include "rocc.h"
#include <stdio.h>

// GCD RoCC instruction macros
#define GCD_START 0
#define GCD_READ  1

// Helper functions for GCD RoCC instructions
static inline void gcd_start(unsigned long a, unsigned long b)
{
    ROCC_INSTRUCTION_SS(0, a, b, GCD_START);
}

static inline unsigned long gcd_read()
{
    unsigned long result;
    ROCC_INSTRUCTION_D(0, result, GCD_READ);
    return result;
}

// Reference software implementation for verification
unsigned long gcd_ref(unsigned long a, unsigned long b) {
    while (b != 0) {
        if (a > b) {
            a = a - b;
        } else {
            b = b - a;
        }
    }
    return a;
}

int main(void)
{
    // Test case 1: Basic GCD
    unsigned long a1 = 48, b1 = 18;
    unsigned long expected1 = gcd_ref(a1, b1);
    
    printf("Testing GCD RoCC: gcd(%lu, %lu)\n", a1, b1);
    printf("Expected result: %lu\n", expected1);
    
    // Start GCD computation
    gcd_start(a1, b1);
    
    // Read result
    unsigned long result1 = gcd_read();
    printf("Hardware result: %lu\n", result1);
    
    if (result1 != expected1) {
        printf("FAIL: Test 1 failed - expected %lu, got %lu\n", expected1, result1);
        return 1;
    }
    printf("PASS: Test 1 passed\n\n");
    
    // Test case 2: Coprime numbers
    unsigned long a2 = 17, b2 = 13;
    unsigned long expected2 = gcd_ref(a2, b2);
    
    printf("Testing GCD RoCC: gcd(%lu, %lu)\n", a2, b2);
    printf("Expected result: %lu\n", expected2);
    
    gcd_start(a2, b2);
    unsigned long result2 = gcd_read();
    printf("Hardware result: %lu\n", result2);
    
    if (result2 != expected2) {
        printf("FAIL: Test 2 failed - expected %lu, got %lu\n", expected2, result2);
        return 2;
    }
    printf("PASS: Test 2 passed\n\n");
    
    // Test case 3: Same numbers
    unsigned long a3 = 42, b3 = 42;
    unsigned long expected3 = gcd_ref(a3, b3);
    
    printf("Testing GCD RoCC: gcd(%lu, %lu)\n", a3, b3);
    printf("Expected result: %lu\n", expected3);
    
    gcd_start(a3, b3);
    unsigned long result3 = gcd_read();
    printf("Hardware result: %lu\n", result3);
    
    if (result3 != expected3) {
        printf("FAIL: Test 3 failed - expected %lu, got %lu\n", expected3, result3);
        return 3;
    }
    printf("PASS: Test 3 passed\n\n");
    
    printf("All GCD RoCC tests passed!\n");
    return 0;
}