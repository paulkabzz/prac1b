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

static double timestamp_us(void)
{


}

/* Hand check: r^2 <= x < (r+1)^2, written out in full. */
static int hand_check(uint32_t x, uint32_t r)
{
    return ((uint64_t) r*r <= x) && (x < (uint64_t)(r +1)*(r+1));}

static double time_n_calls(long reps)
{
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    volatile uint32_t n =987654321 ;
    for (int i = 0; i < reps; i++) {
        golden_isqrt(n);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double diff = (double) (end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec)*1e-9; 

    return diff;

}

int main(void)
{
    for (uint8_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
        uint32_t r = golden_isqrt(inputs[i]);
        int ok = hand_check(inputs[i], r);
        printf("%-12u %-12u %-8s\n", inputs[i], r, ok ? "PASS" : "FAIL");
    }

    double time = time_n_calls(1e9);

    printf("Time Elapsed: %lf\n", time);

    return 0;

}