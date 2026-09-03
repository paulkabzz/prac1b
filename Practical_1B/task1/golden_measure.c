/*
 * Task 1: The Golden Measure on the PC
 *
 * Integer square root of a 32-bit unsigned input x: the largest integer
 * whose square does not exceed x.  Golden version uses double precision
 * arithmetic and the standard library square root.
 *
 * Prediction (written before running):
 *   On a ~3 GHz x86-64 CPU, sqrt + floor + conversion is roughly
 *   20..40 instructions.  Estimate: 40 instructions * 0.33 ns/instr
 *   ~= 13 ns per call.  One call alone cannot be timed reliably because
 *   clock_gettime resolution is ~10..50 ns and scheduling noise is larger
 *   than the call itself, so we loop and divide.
 *
 * Build:   gcc -O2 -o golden_measure golden_measure.c -lm
 * Run:     ./golden_measure
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>

static const uint32_t inputs[10] = {
    0, 1, 15, 16, 4095, 65535,
    123456789, 987654321, 4294836225u, 4294967295u
};

static uint32_t golden_isqrt(uint32_t x)
{
    return (uint32_t)floor(sqrt((double)x));
}

/* Hand check: r^2 <= x < (r+1)^2, written out in full. */
static int hand_check(uint32_t x, uint32_t r)
{
    return ((uint64_t) r*r <= x) && (x < (uint64_t)(r +1)*(r+1));
}

static double time_n_calls(long reps)
{
    struct timespec start, end;
    volatile uint32_t x = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (long i = 0; i < reps; i++) {
        x = golden_isqrt(987654321u);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    (void)x;

    double elapsed_s = (double)(end.tv_sec - start.tv_sec)
                     + (double)(end.tv_nsec - start.tv_nsec) * 1e-9;

    return (elapsed_s / (double)reps) * 1e9;
}

int main(void)
{

    for (int i = 0; i < (int)(sizeof(inputs) / sizeof(inputs[0])); i++) {
       uint32_t r = golden_isqrt(inputs[i]);
        int ok = hand_check(inputs[i], r);
        printf("%-12u %-12u %-8s\n", inputs[i], r, ok ? "PASS" : "FAIL");
    }

    /* ---- Full hand check for 987654321 ---- */
    printf("Handcheck for %u: %s", 987654321u, hand_check(987654321u, golden_isqrt(987654321u)) ? "PASS": "FAIL" );

    /* ---- Timing: two runs with different repetition counts ---- */
    long reps1 = 1e6;
    long reps2 = 10e6   ;

    double ns1 = time_n_calls(reps1);
    double ns2 = time_n_calls(reps2);
    double mean   = (ns1 + ns2) / 2.0;
    double spread = fabs(ns1 - ns2);

    printf("\n---- Timing results ----\n");
    printf("Run 1: %ld reps => %.2f ns/call\n", reps1, ns1);
    printf("Run 2: %ld reps => %.2f ns/call\n", reps2, ns2);
    printf("Mean:  %.2f ns/call\n", mean);
    printf("Spread: %.2f ns\n", spread);

    return 0;
}