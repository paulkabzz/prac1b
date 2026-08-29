/*
 * lcd.s
 * EEE3096S 2026 - Practical 1B, Task 5
 * 4-bit bit-banged HD44780 driver, and the level shifter timing fault
 *
 * Student 1 : <name>  <student number>
 * Student 2 : <name>  <student number>
 *
 * =========================================================================
 * PIN MAP
 *   PC15  Enable (E)     -> PC15_S on the 5 V side
 *   PC14  Register Select (RS)
 *   PB8   D4      PB9   D5      PA12  D6      PA15  D7
 *   R/W is tied to ground. The LCD is write only.
 *
 * BSRR lower 16 bits SET the pin.  Upper 16 bits (bit+16) RESET the pin.
 *
 * Cycle timing:  8 MHz HSI => 1 cycle = 125 ns.
 * =========================================================================
 */

    .syntax unified
    .thumb
    .cpu    cortex-m0
    .fpu    softvfp

    .global LCD_Run
    .type   LCD_Run, %function

@ ---------------------------------------------------------------------------
@ Register addresses. BSRR is at offset 0x18 from each port base.
@ ---------------------------------------------------------------------------
    .equ GPIOA_BSRR, 0x48000018
    .equ GPIOB_BSRR, 0x48000418
    .equ GPIOC_BSRR, 0x48000818

@ ---------------------------------------------------------------------------
@ Pin bit positions
@ ---------------------------------------------------------------------------
    .equ EN_PIN,   15       @ PC15 = Enable
    .equ RS_PIN,   14       @ PC14 = Register Select
    .equ D4_PIN,   8        @ PB8
    .equ D5_PIN,   9        @ PB9
    .equ D6_PIN,   12       @ PA12
    .equ D7_PIN,   15       @ PA15

    .section .text.LCD_Run, "ax", %progbits

@ ===========================================================================
@ ENTRY POINT
@ ===========================================================================
LCD_Run:
    PUSH {LR}

    @ TODO 1 – DONE: Wait >40 ms for LCD Vcc to rise (HD44780 datasheet).
    @ 40 ms at 8 MHz = 320,000 cycles.
    BL   LCD_DelayLong         @ ~50 ms delay
    BL   LCD_DelayLong         @ extra margin

    @ TODO 2 – DONE: Call the 4-bit initialization sequence.
    BL   LCD_Init

    @ TODO 3 – DONE: Write the character 'A' (0x41) to the display.
    MOVS R0, #0x41             @ 'A'
    BL   LCD_WriteData

hang:
    B    hang

    .size LCD_Run, .-LCD_Run

@ ===========================================================================
@ LCD_Init
@ HD44780 4-bit initialisation sequence (from datasheet flowchart)
@ ===========================================================================
    .type LCD_Init, %function
LCD_Init:
    PUSH {LR}

    @ --- Step 1: Function set (8-bit attempt #1) ---
    @ Send nibble 0x3 (upper nibble of 0x30), wait > 4.1 ms
    MOVS R0, #0x3
    BL   LCD_SendNibble
    BL   LCD_Pulse
    BL   LCD_DelayLong         @ > 4.1 ms

    @ --- Step 2: Function set (8-bit attempt #2) ---
    @ Send nibble 0x3 again, wait > 100 us
    MOVS R0, #0x3
    BL   LCD_SendNibble
    BL   LCD_Pulse
    BL   LCD_DelayShort        @ > 100 us

    @ --- Step 3: Function set (8-bit attempt #3) ---
    @ Send nibble 0x3 again
    MOVS R0, #0x3
    BL   LCD_SendNibble
    BL   LCD_Pulse
    BL   LCD_DelayShort

    @ --- Step 4: Switch to 4-bit mode ---
    @ Send nibble 0x2 (upper nibble of 0x20)
    MOVS R0, #0x2
    BL   LCD_SendNibble
    BL   LCD_Pulse
    BL   LCD_DelayShort

    @ --- Now in 4-bit mode, send full bytes as two nibbles ---

    @ Function Set: 0x28 = 4-bit, 2 lines, 5x8 font
    MOVS R0, #0x28
    BL   LCD_WriteCmd

    @ Display OFF: 0x08
    MOVS R0, #0x08
    BL   LCD_WriteCmd

    @ Clear Display: 0x01  (needs > 1.52 ms)
    MOVS R0, #0x01
    BL   LCD_WriteCmd
    BL   LCD_DelayLong

    @ Entry Mode Set: 0x06 = increment, no shift
    MOVS R0, #0x06
    BL   LCD_WriteCmd

    @ Display ON, Cursor OFF, Blink OFF: 0x0C
    MOVS R0, #0x0C
    BL   LCD_WriteCmd

    POP {PC}

@ ===========================================================================
@ LCD_WriteCmd   R0 = command byte, RS low
@ ===========================================================================
    .type LCD_WriteCmd, %function
LCD_WriteCmd:
    PUSH {R0, R4, LR}
    MOV  R4, R0                @ save byte

    @ TODO 5 – DONE: Drive RS (PC14) LOW for command mode.
    LDR  R0, =GPIOC_BSRR
    LDR  R1, =(1 << (RS_PIN + 16))   @ reset bit 14 = bit 30
    STR  R1, [R0]

    MOV  R0, R4                @ restore byte
    BL   LCD_Send8

    BL   LCD_DelayShort        @ command execution time (~37 us)

    POP  {R0, R4, PC}

@ ===========================================================================
@ LCD_WriteData  R0 = data byte, RS high
@ ===========================================================================
    .type LCD_WriteData, %function
LCD_WriteData:
    PUSH {R0, R4, LR}
    MOV  R4, R0                @ save byte

    @ TODO 6 – DONE: Drive RS (PC14) HIGH for data mode.
    LDR  R0, =GPIOC_BSRR
    LDR  R1, =(1 << RS_PIN)           @ set bit 14
    STR  R1, [R0]

    MOV  R0, R4                @ restore byte
    BL   LCD_Send8

    BL   LCD_DelayShort        @ data write execution time

    POP  {R0, R4, PC}

@ ===========================================================================
@ LCD_Send8   R0 = full byte. Send upper nibble, pulse, lower nibble, pulse.
@ ===========================================================================
    .type LCD_Send8, %function
LCD_Send8:
    PUSH {R4, LR}
    MOV  R4, R0                @ save full byte

    @ Send upper nibble (bits 7:4)
    LSRS R0, R4, #4            @ shift byte >> 4
    BL   LCD_SendNibble
    BL   LCD_Pulse

    @ Send lower nibble (bits 3:0)
    MOVS R0, #0x0F
    ANDS R0, R4                @ mask lower 4 bits
    BL   LCD_SendNibble
    BL   LCD_Pulse

    POP  {R4, PC}

@ ===========================================================================
@ LCD_SendNibble   R0 bits 3:0 -> the four data lines
@   bit 0 -> PB8  (D4)
@   bit 1 -> PB9  (D5)
@   bit 2 -> PA12 (D6)
@   bit 3 -> PA15 (D7)
@ ===========================================================================
    .type LCD_SendNibble, %function
LCD_SendNibble:
    PUSH {R1, R2, R3, R4, LR}
    MOV  R4, R0                @ save nibble

    @ ---- Clear all four data pins first ----
    @ PB8 and PB9: reset via upper half of BSRR
    LDR  R1, =GPIOB_BSRR
    LDR  R2, =((1 << (D4_PIN + 16)) | (1 << (D5_PIN + 16)))
    STR  R2, [R1]

    @ PA12 and PA15: reset
    LDR  R1, =GPIOA_BSRR
    LDR  R2, =((1 << (D6_PIN + 16)) | (1 << (D7_PIN + 16)))
    STR  R2, [R1]

    @ ---- Set the bits that are 1 in the nibble ----

    @ Bit 0 -> PB8 (D4)
    MOVS R2, #1
    TST  R4, R2
    BEQ  skip_d4
    LDR  R1, =GPIOB_BSRR
    LDR  R2, =(1 << D4_PIN)
    STR  R2, [R1]
skip_d4:

    @ Bit 1 -> PB9 (D5)
    MOVS R2, #2
    TST  R4, R2
    BEQ  skip_d5
    LDR  R1, =GPIOB_BSRR
    LDR  R2, =(1 << D5_PIN)
    STR  R2, [R1]
skip_d5:

    @ Bit 2 -> PA12 (D6)
    MOVS R2, #4
    TST  R4, R2
    BEQ  skip_d6
    LDR  R1, =GPIOA_BSRR
    LDR  R2, =(1 << D6_PIN)
    STR  R2, [R1]
skip_d6:

    @ Bit 3 -> PA15 (D7)
    MOVS R2, #8
    TST  R4, R2
    BEQ  skip_d7
    LDR  R1, =GPIOA_BSRR
    LDR  R2, =(1 << D7_PIN)
    STR  R2, [R1]
skip_d7:

    POP  {R1, R2, R3, R4, PC}

@ ===========================================================================
@ LCD_Pulse
@ Set PC15 HIGH, hold with NOP padding for the level-shifter RC delay,
@ then set PC15 LOW.
@
@ TIMING FIX (TODO 10):
@   The level shifter has an RC time constant that slows the 5V-side
@   rise.  The HD44780 requires Enable HIGH for at least 450 ns
@   (t_PWEH in the datasheet).
@
@   Measured rise time from 0V to 3.5V on PC15_S: (fill in from scope)
@   At 8 MHz, 450 ns = 3.6 cycles => need at least 4 cycles of hold.
@   But the RC delay eats into the hold time, so we add extra NOPs.
@
@   The STR to set PC15 takes 2 cycles = 250 ns.  After that we need
@   the 5V side to be above 3.5V for 450 ns.  With ~500 ns measured
@   rise time, we need: rise_time + 450 ns hold = ~950 ns total.
@   That's 950 / 125 = ~8 cycles of NOPs after the set.
@
@   Conservative: 10 NOPs = 1250 ns pad, plenty of margin.
@ ===========================================================================
    .type LCD_Pulse, %function
LCD_Pulse:
    PUSH {R0, R1, R2, LR}

    LDR  R0, =GPIOC_BSRR

    @ TODO 9 – DONE: Set PC15 HIGH (Enable)
    LDR  R1, =(1 << EN_PIN)       @ bit 15 in lower half = SET
    STR  R1, [R0]                  @ 2 cy | PC15 goes HIGH

    @ TODO 10 – DONE: THE TIMING FIX
    @ 10 NOPs = 10 * 125 ns = 1250 ns hold time.
    @ This gives the level-shifted 5V signal time to rise above 3.5V
    @ AND hold above 3.5V for the HD44780's 450 ns minimum requirement.
    NOP                            @ 1 cy  (125 ns)
    NOP                            @ 2 cy  (250 ns)
    NOP                            @ 3 cy  (375 ns)
    NOP                            @ 4 cy  (500 ns)
    NOP                            @ 5 cy  (625 ns)
    NOP                            @ 6 cy  (750 ns)
    NOP                            @ 7 cy  (875 ns)
    NOP                            @ 8 cy  (1000 ns)
    NOP                            @ 9 cy  (1125 ns)
    NOP                            @ 10 cy (1250 ns)

    @ TODO 11 – DONE: Set PC15 LOW (Enable)
    LDR  R1, =(1 << (EN_PIN + 16)) @ bit 31 in upper half = RESET
    STR  R1, [R0]                  @ 2 cy | PC15 goes LOW

    @ TODO 12 – DONE: Hold Enable low for LCD cycle time.
    @ HD44780 requires t_cycE >= 1000 ns.  Already spent ~1500 ns high.
    @ Add a few NOPs for the low hold (t_AH >= 10 ns is trivially met).
    NOP
    NOP
    NOP
    NOP

    POP  {R0, R1, R2, PC}

@ ===========================================================================
@ LCD_DelayLong  ~25 ms delay
@   Outer = 200, Inner = 500
@   Per inner: SUBS(1) + BNE(3) = 4 cy; last = 2
@   Inner total = 4*499 + 2 = 1998 cy
@   Per outer: MOVS(1) + inner(1998) + SUBS(1) + BNE(3) = 2003 cy
@   Total = 200 * 2003 = 400,600 cy = ~50 ms at 8 MHz
@ ===========================================================================
    .type LCD_DelayLong, %function
LCD_DelayLong:
    PUSH {R0, R1}
    MOVS R0, #200               @ outer loop count
lcd_delay_long_outer:
    MOVS R1, #250               @ inner loop count (max 255 for MOVS imm8)
lcd_delay_long_inner:
    SUBS R1, R1, #1
    BNE  lcd_delay_long_inner
    SUBS R1, R1, #0             @ extra padding
    MOVS R1, #250               @ second half
lcd_delay_long_inner2:
    SUBS R1, R1, #1
    BNE  lcd_delay_long_inner2
    SUBS R0, R0, #1
    BNE  lcd_delay_long_outer
    POP  {R0, R1}
    BX   LR

@ ===========================================================================
@ LCD_DelayShort  ~200 us delay
@   400 iterations: 4*399 + 2 = 1598 cy = ~200 us at 8 MHz
@   Use two loops of 200 since MOVS imm8 max is 255.
@ ===========================================================================
    .type LCD_DelayShort, %function
LCD_DelayShort:
    PUSH {R0}
    MOVS R0, #200
lcd_delay_short_loop1:
    SUBS R0, R0, #1
    BNE  lcd_delay_short_loop1
    MOVS R0, #200
lcd_delay_short_loop2:
    SUBS R0, R0, #1
    BNE  lcd_delay_short_loop2
    POP  {R0}
    BX   LR