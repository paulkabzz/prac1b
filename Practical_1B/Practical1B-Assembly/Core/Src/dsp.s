/*
 * dsp.s
 * EEE3096S 2026 - Practical 1B, Task 4
 * Cycle-counted ADC to DAC loop with a 45 degree phase delay
 *
 * Student 1 : <name>  <student number>
 * Student 2 : <name>  <student number>
 *
 * =========================================================================
 * CYCLE BUDGET
 * =========================================================================
 * System clock: 8 MHz HSI  =>  1 cycle = 125 ns
 *
 * 45 degrees at 1 kHz:
 *   T = 1/1000 = 1 ms = 1000 us
 *   45/360 * 1000 us = 125 us
 *   125 us / 0.125 us = 1000 cycles required total delay
 *
 * Per-instruction cycle costs on Cortex-M0 (from ARM DDI0432C):
 *   LDR  Rt, [Rn]       : 2 cycles
 *   STR  Rt, [Rn]       : 2 cycles
 *   MOVS Rt, #imm       : 1 cycle
 *   SUBS Rt, Rt, #imm   : 1 cycle
 *   BNE  label           : 3 cycles (taken), 1 cycle (not taken)
 *   B    label           : 3 cycles (unconditional)
 *   NOP                  : 1 cycle
 *
 * LOOP BODY (outside delay):
 *   LDR  R2, [R0]        2 cycles   (read ADC_DR)
 *   STR  R2, [R1]        2 cycles   (write DAC_DHR12R1)
 *   MOVS R3, #N          1 cycle    (load delay counter)
 *   --- delay loop ---
 *   B    loop             3 cycles   (branch back to top)
 *                       --------
 *   Overhead:             8 cycles
 *
 * DELAY LOOP (per iteration):
 *   SUBS R3, R3, #1      1 cycle
 *   BNE  delay_loop       3 cycles (taken)
 *                       --------
 *   Per iteration:        4 cycles
 *   Last iteration:       1 + 1 = 2 cycles (BNE not taken)
 *
 * Total for N iterations: 4*(N-1) + 2 = 4*N - 2
 *
 * Target: 1000 cycles total
 *   1000 = 8 + 4*N - 2
 *   1000 = 6 + 4*N
 *   4*N  = 994
 *   N    = 248.5  =>  N = 248
 *
 *   With N = 248: overhead=8, delay=4*248-2=990, total = 8+990 = 998
 *   Off by 2 cycles (998 vs 1000).  Add 2 NOPs before the delay loop.
 *
 *   Final: 8 + 2 + 990 = 1000 cycles = 125 us.  Exact.
 *
 * =========================================================================
 */

    .syntax unified
    .thumb
    .cpu    cortex-m0
    .fpu    softvfp

    .global DSP_Loop
    .type   DSP_Loop, %function

@ ---------------------------------------------------------------------------
@ Peripheral addresses
@ ---------------------------------------------------------------------------
    .equ ADC_DR,      0x40012440    @ ADC data register (RM0091 §13.12.5)
    .equ DAC_DHR12R1, 0x40007408    @ DAC ch1 12-bit right-aligned (RM0091 §14.5.3)

    .section .text.DSP_Loop, "ax", %progbits

@ ===========================================================================
@ ENTRY POINT
@ ===========================================================================
DSP_Loop:
    @ Setup base-address registers outside the timed loop (not counted)
    LDR R0, =ADC_DR              @ R0 = &ADC_DR
    LDR R1, =DAC_DHR12R1         @ R1 = &DAC_DHR12R1

loop:
    @ --- SAMPLE AND OUTPUT ------------------------------------------------
    LDR  R2, [R0]                @ 2 cy  | Read latest ADC conversion
    STR  R2, [R1]                @ 2 cy  | Write it straight to DAC

    @ --- DELAY SETUP ------------------------------------------------------
    @ Need 1000 total cycles per loop iteration for 125 us at 8 MHz.
    @ Overhead (LDR+STR+MOVS+B) = 8 cycles.
    @ Add 2 NOPs for padding = 2 cycles.
    @ Delay loop with N=248: 4*248 - 2 = 990 cycles.
    @ Total: 8 + 2 + 990 = 1000 cycles = 125.0 us.

    MOVS R3, #248                @ 1 cy  | Load delay counter N = 248
    NOP                          @ 1 cy  | Padding NOP #1
    NOP                          @ 1 cy  | Padding NOP #2

delay_loop:
    @ --- INNER DELAY LOOP -------------------------------------------------
    SUBS R3, R3, #1              @ 1 cy  | Decrement counter (sets flags)
    BNE  delay_loop              @ 3 cy taken, 1 cy last | Loop if != 0

    @ --- REPEAT -----------------------------------------------------------
    B    loop                    @ 3 cy  | Back to sample the next value

    @ ----------------------------------------------------------------------
    @ CYCLE BUDGET SUMMARY
    @   LDR  R2,[R0]       2
    @   STR  R2,[R1]       2
    @   MOVS R3,#248       1
    @   NOP                1
    @   NOP                1
    @   delay (248 iters)  990   [4*247 + 2 = 990]
    @   B loop             3
    @                    -----
    @   TOTAL           1000 cycles = 125.0 us = 45 degrees at 1 kHz
    @ ----------------------------------------------------------------------

    .size DSP_Loop, .-DSP_Loop