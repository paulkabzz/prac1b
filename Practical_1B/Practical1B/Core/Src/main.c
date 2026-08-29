/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * EEE3096S 2026 - Practical 1B
  * Tasks 2 and 3: fast integer square root, TIM16 timing, optimisation flags
  *
  * Student 1 : <name>  <student number>
  * Student 2 : <name>  <student number>
  * Date      : <date>
  *
  * Board pins used
  *   PC13 : scope pulse. Driven LOW for the timed section, HIGH otherwise.
  *          Broken out on Header P1.
  *   PB1  : pass or fail indicator. User LED 1. ON means all ten golden
  *          values matched.
  *
  * Search for TODO. Every TODO is a piece of work you have to complete.
  * Do not delete the USER CODE markers. STM32CubeIDE overwrites everything
  * outside them whenever you regenerate from the .ioc file.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdint.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define PULSE_PIN    13u          /* PC13 */
#define LED_PIN      1u           /* PB1  */

#define TEST_INPUT   987654321u   /* the input named in the Task 2 question */
#define LONG_RUN_N   20000u       /* calls in the wrap-around run           */
/* USER CODE END PD */

/* USER CODE BEGIN PV */

/* The ten inputs from Task 1. Do not change these. */
static const uint32_t golden_inputs[10] = {
    0u, 1u, 15u, 16u, 4095u, 65535u,
    123456789u, 987654321u, 4294836225u, 4294967295u
};

/*
 * TODO 1
 * Fill this array with the ten outputs produced by YOUR Task 1 golden
 * measure. Copy them from your own PC run, not from a friend and not from
 * the practical sheet. The firmware self-test below compares against these.
 */
static const uint32_t golden_outputs[10] = {
    0u, 1u, 3u, 4u, 63u, 255u,
    11111u, 31426u, 65535u, 65535u
};

/*
 * Results. Keep these volatile so the optimiser leaves them alone at -O1
 * and above. Read them in the STM32CubeIDE Live Expressions view.
 */
volatile uint8_t  pass_all = 0u;   /* 1 means all ten matched      */
volatile uint32_t single_call_span = 0u;   /* timer counts, one call       */
volatile uint32_t long_run_span = 0u;   /* timer counts, LONG_RUN_N     */
volatile float    mean_us_per_call = 0.0f; /* long run divided by N        */

/* Sink for the return value. Stops the optimiser deleting the call. */
static volatile uint32_t sink = 0u;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static void     gpio_init(void);
static void     timing_timer_init(void);
static uint32_t isqrt(uint32_t x);
static uint32_t time_one_call(uint32_t x);
static uint32_t time_n_calls(uint32_t x, uint32_t n);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* ---------------------------------------------------------------------------
 * Hardware initialisation
 * ------------------------------------------------------------------------ */
static void gpio_init(void)
{
    /*
     * TODO 2
     * Enable the peripheral clock for GPIOC and GPIOB.
     * RM0091 Section 6.4.6: RCC_AHBENR, bit 19 = IOPCEN, bit 18 = IOPBEN.
     */
    RCC->AHBENR |= (RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN);

    /*
     * TODO 3  –  DONE
     * PC13 =>general purpose output (MODER13 = 01).
     * PB1  => general purpose output (MODER1  = 01).
     * Two bits per pin: clear both, then set the 01 pattern.
     */
    GPIOC->MODER &= ~(3UL << (PULSE_PIN * 2));   /* clear PC13 */
    GPIOC->MODER |=  (1UL << (PULSE_PIN * 2));   /* set 01     */

    GPIOB->MODER &= ~(3UL << (LED_PIN * 2));     /* clear PB1  */
    GPIOB->MODER |=  (1UL << (LED_PIN * 2));     /* set 01     */

    /*
     * TODO 4  –  DONE
     * Idle states: PC13 HIGH (scope pulse is active-low), PB1 LOW (LED off).
     * BSRR lower 16 bits set the pin, BRR clears the pin.
     */
    GPIOC->BSRR = (1UL << PULSE_PIN);   /* PC13 HIGH */
    GPIOB->BRR  = (1UL << LED_PIN);     /* PB1  LOW  */
}

static void timing_timer_init(void)
{
    /*
     * TODO 5  –  DONE
     * Enable TIM16 peripheral clock.
     * GPIO ports are on AHB (RCC_AHBENR).
     * TIM16 is on APB2 (RCC_APB2ENR, bit 17 = TIM16EN).
     * RM0091 Section 6.4.7.
     */
    RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;

    /*
     * TODO 6  –  DONE
     * Clock path: HSI 8 MHz → AHB prescaler /1 → APB2 prescaler /1 → TIM16.
     *   timer clock = 8 MHz
     *   PSC = 0  =>  counter clock = 8 MHz / (0+1) = 8 MHz  →  125 ns/tick
     */
    TIM16->PSC = 0u;

    /*
     * TODO 7  –  DONE
     * ARR = 0xFFFF for a full 16-bit free-running counter.
     * Generate an update event (EGR.UG) to force PSC into the shadow register,
     * then clear the update flag, then enable the counter (CR1.CEN).
     */
    TIM16->ARR = 0xFFFFu;
    TIM16->EGR = TIM_EGR_UG;       /* force prescaler load      */
    TIM16->SR  = 0u;                /* clear the update flag     */
    TIM16->CR1 |= TIM_CR1_CEN;     /* start counting            */
}

/* ---------------------------------------------------------------------------
 * Task 2 core algorithm
 * ------------------------------------------------------------------------ */

/*
 * Helper. Returns non-zero when mid * mid is at or below x.
 *
 * Keep this as a separate function. Task 3 asks you to find one
 * optimisation transformation in the disassembly, and the treatment of
 * this helper at -O2 is the easiest one to spot and name.
 *
 * Note: mid * mid overflows 32 bits for large mid. Promote before you
 * multiply.
 */
static inline uint32_t square_le(uint32_t mid, uint32_t x)
{
    /* TODO 8  –  DONE
     * Promote to 64-bit before multiplying to avoid overflow for mid > 65535.
     * Returns 1 if mid*mid <= x, 0 otherwise.
     */
    return ((uint64_t)mid * mid <= x) ? 1u : 0u;
}

/*
 * Integer square root. Returns the largest r with r * r <= x, for every
 * x from 0 to 4294967295.
 */
static uint32_t isqrt(uint32_t x)
{
    /*
     * Binary search over the answer range [0, 65535].
     * The maximum integer square root of a 32-bit number is 65535
     * (since 65535^2 = 4294836225 <= 4294967295).
     *
     * At each step, test whether (lo + hi) / 2 satisfies mid^2 <= x.
     * If yes, move lo up; if no, move hi down.
     */
    uint32_t lo = 0u;
    uint32_t hi = 65535u;

    while (lo <= hi)
    {
        uint32_t mid = lo + (hi - lo) / 2u;
        if (square_le(mid, x))
        {
            lo = mid + 1u;
        }
        else
        {
            hi = mid - 1u;
        }
    }
    /* lo is one past the answer; hi is the largest r with r*r <= x */
    return hi;
}

/* ---------------------------------------------------------------------------
 * Timing harness
 * ------------------------------------------------------------------------ */

/*
 * Times one call.
 * Drives PC13 LOW for the timed window so the scope pulse and the counter
 * span cover the same code.
 * Returns the elapsed counter span.
 */
static uint32_t time_one_call(uint32_t x)
{
    uint16_t a = 0u;
    uint16_t b = 0u;

    GPIOC->BRR = (1UL << PULSE_PIN);   /* PC13 low: pulse starts */

    /* TODO 10  –  DONE.  TIM16->CNT holds the running count (RM0091 §17.5.5). */
    a = (uint16_t)TIM16->CNT;

    sink = isqrt(x);                   /* the code under test */

    /* TODO 11  –  DONE */
    b = (uint16_t)TIM16->CNT;

    GPIOC->BSRR = (1UL << PULSE_PIN);  /* PC13 high: pulse ends */

    /*
     * TODO 12  –  DONE
     * Wrap-safe elapsed span.  Because a and b are uint16_t and we
     * subtract in uint16_t arithmetic, the C unsigned wraparound gives
     * the correct modular distance even when b < a:
     *
     *   Example:  a = 65500, b = 20
     *     (uint16_t)(20 − 65500) = (uint16_t)(−65480)
     *                            = 65536 − 65480 = 56.  Correct.
     */
    return (uint16_t)(b - a);
}

/*
 * Times n calls back to back so the counter crosses its overflow more
 * than once. Same arithmetic as the single call version.
 */
static uint32_t time_n_calls(uint32_t x, uint32_t n)
{
    uint16_t a = 0u;
    uint16_t b = 0u;

    GPIOC->BRR = (1UL << PULSE_PIN);

    /* TODO 13  –  DONE */
    a = (uint16_t)TIM16->CNT;

    for (uint32_t i = 0u; i < n; i++)
    {
        sink = isqrt(x);
    }

    /* TODO 14  –  DONE */
    b = (uint16_t)TIM16->CNT;

    GPIOC->BSRR = (1UL << PULSE_PIN);

    /*
     * TODO 15  –  DONE
     * Same wrap-safe subtraction.  At 8 MHz (125 ns/tick), the 16-bit
     * counter wraps every 65536 × 125 ns = 8.192 ms.  With PSC = 0 this
     * means we can measure up to ~8 ms unambiguously.  LONG_RUN_N must
     * be chosen so the total time stays inside that window (or we accept
     * that the 16-bit modular subtraction still gives the correct
     * *modular* count as long as the true elapsed ≤ 65535 ticks).
     */
    return (uint16_t)(b - a);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  */
int main(void)
{
  /* MCU Configuration -------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* USER CODE BEGIN 2 */
  gpio_init();
  timing_timer_init();

  /* Self-test against the ten golden values from Task 1 */
  pass_all = 1u;
  for (int i = 0; i < 10; i++)
  {
      if (isqrt(golden_inputs[i]) != golden_outputs[i])
      {
          pass_all = 0u;
          break;
      }
  }

  /*
   * TODO 16  –  DONE
   * Drive PB1 from pass_all. LED on for a pass, off for a fail.
   */
  if (pass_all)
  {
      GPIOB->BSRR = (1UL << LED_PIN);   /* PB1 HIGH = LED on  */
  }
  else
  {
      GPIOB->BRR  = (1UL << LED_PIN);   /* PB1 LOW  = LED off */
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  while (1)
  {
    /* USER CODE BEGIN 3 */

    /* Task 2 and Task 3: single call measurement */
    single_call_span = time_one_call(TEST_INPUT);

    /*
     * TODO 17  –  DONE
     * Task 2 wrap case: run the long measurement, then work out the mean
     * time per call.
     *
     * Each tick = 125 ns = 0.125 µs.
     * mean_us = (long_run_span * 0.125) / LONG_RUN_N
     *
     * Uncomment the two lines below when ready. Keep them commented while
     * placing scope cursors on the single-call pulse.
     */
    /* long_run_span    = time_n_calls(TEST_INPUT, LONG_RUN_N); */
    /* mean_us_per_call = (float)long_run_span * 0.125f / (float)LONG_RUN_N; */

    /* Gap between measurements so the scope has a clean single pulse */
    for (volatile int d = 0; d < 100000; d++)
    {
    }
    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */

/*
 * ---------------------------------------------------------------------------
 * TASK 3 CHECKLIST. No code changes needed below this line.
 * ---------------------------------------------------------------------------
 *
 * Build this same file four times, once per optimisation level.
 *
 *   Project > Properties > C/C++ Build > Settings > MCU GCC Compiler
 *     > Optimization > Optimization Level
 *
 *   None            -O0
 *   Optimize        -O1
 *   Optimize more   -O2
 *   Optimize size   -Os
 *
 * At every level record:
 *   1. Text size in bytes. Build Analyzer tab, or run
 *        arm-none-eabi-size Debug/Practical1B.elf
 *   2. single_call_span from Live Expressions.
 *   3. The PC13 pulse width from the scope, with cursors.
 *
 * At -O2 also dump the disassembly of your isqrt function:
 *        arm-none-eabi-objdump -d Debug/Practical1B.elf > disasm_O2.txt
 *   or open Debug/Practical1B.list, which the build already produces.
 * Find one transformation the compiler applied, name it, and point at the
 * source lines above it acts on.
 * ---------------------------------------------------------------------------
 */