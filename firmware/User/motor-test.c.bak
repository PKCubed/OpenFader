#include "py32f0xx_hal.h"
#include "py32f0xx_bsp_printf.h"
#include "ws2812.h"
#include "SEGGER_RTT.h"

void APP_ErrorHandler(void);
TIM_HandleTypeDef htim3;


// Basic system clock configuration function if we need to ensure 24MHz HSI.
// The default startup code already sets SystemCoreClock to HSI (24MHz).
static void APP_SystemClockConfig(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // HSI Config 24MHz
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
  RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
  RCC_OscInitStruct.LSIState = RCC_LSI_OFF;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    while (1);
  }

  // SysClk Config
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    while (1);
  }
}

// Initialize TIM3 to generate a PWM signal on PA6 and PA7
static void APP_TIM3_PWM_Init(void)
{
  // Enable the clock for TIM3 peripheral
  __HAL_RCC_TIM3_CLK_ENABLE();
  
  // Enable the clock for GPIOA port so we can use its pins
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // Configure PA6 (TIM3_CH1) and PA7 (TIM3_CH2) pins for Alternate Function Push-Pull mode
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;      // Select pins 6 and 7
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;             // Alternate function, push-pull output
  GPIO_InitStruct.Pull = GPIO_NOPULL;                 // No pull-up or pull-down resistors
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;       // High speed switching
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM3;          // Assign these pins to TIM3 (Alternate Function 1)
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);             // Apply configuration to GPIOA

  // Configure the TIM3 time base (frequency)
  htim3.Instance = TIM3;                              // Select TIM3 hardware block
  htim3.Init.Prescaler = 240 - 1;                     // Divide 24MHz system clock by 240 -> 100kHz timer clock
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;        // Timer counts up from 0 to Period
  htim3.Init.Period = 100 - 1;                        // 100 counts at 100kHz = 1kHz PWM frequency
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;  // No extra clock division
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE; // Allow seamless period updates
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)             // Initialize the timer for PWM
  {
    APP_ErrorHandler();                               // Halt on error
  }

  // Configure the specific PWM channels
  TIM_OC_InitTypeDef sConfigOC = {0};
  sConfigOC.OCMode = TIM_OCMODE_PWM1;                 // PWM Mode 1: Output is active while counter < compare value
  sConfigOC.Pulse = 0;                                // Initial duty cycle: 0% (0 / 100), effectively GND
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;         // Active state is logic HIGH (3.3V)
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;          // Disable fast mode
  
  // Apply configuration to Channel 1 (PA6)
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    APP_ErrorHandler();                               // Halt on error
  }
  
  // Apply configuration to Channel 2 (PA7)
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    APP_ErrorHandler();                               // Halt on error
  }
  
  // Start the PWM generation on both channels
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

// Override the weak _write function so printf outputs to SEGGER RTT
int _write(int file, char *ptr, int len)
{
  (void)file;
  SEGGER_RTT_Write(0, ptr, len);
  return len;
}

int main(void)
{
  // Initialize the Hardware Abstraction Layer (HAL)
  HAL_Init();
  
  // Configure the system clocks (running at 24MHz HSI)
  APP_SystemClockConfig();
  
  // Initialize our custom TIM3 PWM configuration
  APP_TIM3_PWM_Init();
  
  // Initialize the USART peripheral so we can use printf for debugging
  // BSP_USART_Config(); // Commented out to use RTT instead of UART
  
  // Initialize SEGGER RTT (Optional but good practice)
  SEGGER_RTT_Init();
  
  // Print a startup message to the RTT terminal
  printf("PY32F003 PWM Swap (via RTT)\r\nSystem Clock: %ld\r\n", SystemCoreClock);

  // State variable to keep track of which pin is active
  uint8_t swap_state = 0;
  
  // Infinite loop - main program logic
  while (1)
  {
    if (swap_state == 0)
    {
      printf("Up\r\n");
      // State 0: PA6 is GND (0%), PA7 is active PWM (50%)
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);  // Set PA6 duty cycle to 0 (GND)
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 80); // Set PA7 duty cycle to 50 (50% of 100)
      swap_state = 1;                                   // Next time, go to State 1
    }
    else
    {
      printf("Down\r\n");
      // State 1: PA6 is active PWM (50%), PA7 is GND (0%)
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 80); // Set PA6 duty cycle to 50 (50% of 100)
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);  // Set PA7 duty cycle to 0 (GND)
      swap_state = 0;                                   // Next time, go back to State 0
    }
    
    // Wait for 2 seconds (2000 milliseconds) before swapping again
    HAL_Delay(2000); 
  }
}

void APP_ErrorHandler(void)
{
  while (1);
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  while (1);
}
#endif /* USE_FULL_ASSERT */
