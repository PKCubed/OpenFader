#include "py32f0xx_hal.h"
#include "py32f0xx_bsp_printf.h"
#include "ws2812.h"
#include "SEGGER_RTT.h"
#include <math.h>
#include <stdbool.h>

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

void APP_ErrorHandler(void);

ADC_HandleTypeDef AdcHandle;
DMA_HandleTypeDef HdmaCh1;
TIM_HandleTypeDef htim3;

// Global variable where the DMA will automatically drop the latest ADC reading
volatile uint32_t latest_adc_val = 0;

// Basic system clock configuration function
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
    APP_ErrorHandler();
  }

  // SysClk Config
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}



// Override the weak _write function so printf outputs to SEGGER RTT
int _write(int file, char *ptr, int len) {
  (void)file;
  SEGGER_RTT_Write(0, ptr, len);
  return len;
}

// Initialize ADC to read analog voltage from PA4 (Channel 4) via DMA
static void APP_ADC_Init(void)
{
  // Reset the ADC hardware state before configuration
  __HAL_RCC_ADC_FORCE_RESET();
  __HAL_RCC_ADC_RELEASE_RESET();
  
  // Enable clocks for ADC, GPIOA, SYSCFG (for DMA mapping), and DMA
  __HAL_RCC_ADC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_DMA_CLK_ENABLE();

  // Configure PA4 specifically as an analog input pin
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_4;                   // Select pin PA4
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;            // Put it into analog mode
  GPIO_InitStruct.Pull = GPIO_NOPULL;                 // No internal pull-ups or pull-downs
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // --- DMA CONFIGURATION ---
  // Map the ADC DMA request to DMA Channel 1
  HAL_SYSCFG_DMA_Req(DMA_CHANNEL_MAP_ADC);

  // Configure DMA Channel 1 to move data from Peripheral to Memory
  HdmaCh1.Instance                 = DMA1_Channel1;
  HdmaCh1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  HdmaCh1.Init.PeriphInc           = DMA_PINC_DISABLE;         // Always read from the exact same ADC register
  HdmaCh1.Init.MemInc              = DMA_MINC_DISABLE;         // Always write to the exact same variable (latest_adc_val)
  HdmaCh1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;  // ADC data register is 16-bit
  HdmaCh1.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;      // Our variable is 32-bit
  HdmaCh1.Init.Mode                = DMA_CIRCULAR;             // Keep looping endlessly (Continuous mode)
  HdmaCh1.Init.Priority            = DMA_PRIORITY_HIGH;        // High priority for ADC data

  // Initialize the DMA channel
  HAL_DMA_Init(&HdmaCh1);
  
  // Link this DMA handle to our ADC handle
  __HAL_LINKDMA(&AdcHandle, DMA_Handle, HdmaCh1);

  // --- ADC CONFIGURATION ---
  AdcHandle.Instance = ADC1;
  
  // Run an internal calibration sequence on the ADC for accurate readings
  if (HAL_ADCEx_Calibration_Start(&AdcHandle) != HAL_OK)
  {
    APP_ErrorHandler();
  }
  
  // Configure the general ADC operation
  AdcHandle.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV1;
  AdcHandle.Init.Resolution            = ADC_RESOLUTION_12B;
  AdcHandle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  AdcHandle.Init.ScanConvMode          = ADC_SCAN_DIRECTION_FORWARD;
  AdcHandle.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  AdcHandle.Init.LowPowerAutoWait      = ENABLE;                   // Enable auto-wait for stability during continuous conversions
  AdcHandle.Init.ContinuousConvMode    = ENABLE;                   // **NEW**: Automatically start next reading!
  AdcHandle.Init.DiscontinuousConvMode = DISABLE;
  AdcHandle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  AdcHandle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
  AdcHandle.Init.DMAContinuousRequests = ENABLE;                   // **NEW**: Tell ADC to constantly push data to DMA!
  AdcHandle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
  AdcHandle.Init.SamplingTimeCommon    = ADC_SAMPLETIME_239CYCLES_5;
  
  // Apply the ADC configuration
  if (HAL_ADC_Init(&AdcHandle) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  // Link our configured ADC specifically to Channel 4 (which maps to PA4)
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Rank         = ADC_RANK_CHANNEL_NUMBER;
  sConfig.Channel      = ADC_CHANNEL_4;
  
  // Apply channel configuration
  if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  // START the ADC and link it to our target variable via DMA (buffer size 1)
  if (HAL_ADC_Start_DMA(&AdcHandle, (uint32_t*)&latest_adc_val, 1) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

// Initialize TIM3 to generate a PWM signal on PA6 and PA7 for the motor driver
static void APP_TIM3_PWM_Init(void)
{
  // Enable the clock for TIM3 peripheral
  __HAL_RCC_TIM3_CLK_ENABLE();
  
  // Enable the clock for GPIOA port
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // Configure PA6 (TIM3_CH1) and PA7 (TIM3_CH2) pins for Alternate Function Push-Pull mode
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM3;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Configure the TIM3 time base (frequency)
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 10 - 1;                      // 2.4MHz timer clock (24MHz / 10)
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100 - 1;                        // 24kHz PWM frequency (2.4MHz / 100)
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  // Configure the specific PWM channels
  TIM_OC_InitTypeDef sConfigOC = {0};
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;                                // Initial duty cycle: 0%
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    APP_ErrorHandler();
  }
  
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    APP_ErrorHandler();
  }
  
  // Start the PWM generation
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

// Helper function to easily control the motor speed and direction
// speed ranges from -100 (full speed down) to +100 (full speed up)
void APP_Motor_SetSpeed(int speed)
{
  // Clamp speed to safe limits
  if (speed > 100) speed = 100;
  if (speed < -100) speed = -100;
  
  if (speed > 0)
  {
    // Move UP (Increase ADC): PA7 gets PWM, PA6 is grounded
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, speed);
  }
  else if (speed < 0)
  {
    // Move DOWN (Decrease ADC): PA6 gets PWM, PA7 is grounded (use absolute value of speed)
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, -speed);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
  }
  else
  {
    // Stop: Both grounded
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
  }
}


int main(void)
{
  // Initialize the Hardware Abstraction Layer (HAL)
  HAL_Init();
  
  // Configure the system clocks (running at 24MHz HSI)
  APP_SystemClockConfig();
  
  // Initialize SEGGER RTT for seamless console output over SWD
  SEGGER_RTT_Init();
  
  // Print a startup message to the RTT terminal
  printf("OpenFader v%i.%i\r\nSystem Clock: %ld\r\n", VERSION_MAJOR, VERSION_MINOR, SystemCoreClock);

  // Initialize the custom ADC configuration on PA4
  APP_ADC_Init();
  
  // Initialize the motor PWM on PA6 and PA7
  APP_TIM3_PWM_Init();
  
  /*
  // Good PID Controller Settings
  float kp = 0.015f;
  float ki = 0.005f;
  float kd = 2.5f;
  */

  float kp = 0.06f;
  float ki = 0.005f;
  float kd = 0.5f;
  
  bool enable_deadzones = false; // Set to false to completely disable all deadzones for testing
  int deadzone = 10;       // Stop motor when error falls below this
  int deadzone_exit = 25;  // Don't wake up until error exceeds this (Hysteresis)
  int d_deadzone = 25;     // Ignore D-term changes smaller than this to stop jitter
  
  int min_pwm = 55;        // Minimum PWM required to overcome physical friction
  float max_i_term = 30.0f; // Maximum PWM power the I-term is allowed to add
  
  // Anti-Windup Settings
  int i_active_zone = 200; // Only integrate when error is smaller than this (prevents windup on long travels)
  
  float previous_error = 0.0f;
  float integral = 0.0f;
  bool motor_enabled = true;
  bool is_resting = false; // Hysteresis state tracker
  
  uint32_t last_pid_time = HAL_GetTick();
  uint32_t last_print_time = HAL_GetTick();
  
  // Variables to hold state for printing
  int current_setpoint = 2048;
  float current_error = 0.0f;
  int current_motor_speed = 0;

  // ADC Filtering settings
  float filtered_adc = 0.0f;
  float alpha = 0.2f; // Smoothing factor: lower = smoother but more lag, 1.0 = no smoothing

  while (1)
  {
    uint32_t current_time = HAL_GetTick();

    // --- 1. Fast PID Loop (Runs every 1ms = 1000Hz) ---
    if (current_time - last_pid_time >= 1)
    {
      last_pid_time = current_time;

      // Generate a sine wave setpoint based on system time
      float time_sec = current_time / 1000.0f;
      
      // Uncomment to test sine wave: 
      current_setpoint = 2048 + (int)(1900.0f * sin(time_sec));


      // Apply Exponential Moving Average (EMA) Filter to ADC readings
      if (filtered_adc == 0.0f) filtered_adc = (float)latest_adc_val; // Initialize on first run
      filtered_adc = (alpha * (float)latest_adc_val) + ((1.0f - alpha) * filtered_adc);

      if (motor_enabled)
      {
        // 1. Calculate Error (Target - Current Position)
        current_error = (float)current_setpoint - filtered_adc;
        
        // Hysteresis check to prevent chattering on the boundary
        if (enable_deadzones)
        {
          if (!is_resting && current_error >= -deadzone && current_error <= deadzone)
          {
            is_resting = true; // Enter resting state
          }
          else if (is_resting && (current_error > deadzone_exit || current_error < -deadzone_exit))
          {
            is_resting = false; // Wake up from resting state
          }
        }
        else
        {
          is_resting = false; // Never rest if deadzones are disabled
        }
        
        if (is_resting)
        {
          // We are close enough to the target! Stop the motor and clear integral buildup
          current_error = 0.0f;
          integral = 0.0f; 
          current_motor_speed = 0;
          previous_error = 0.0f;
          
          APP_Motor_SetSpeed(0);
        }
        else
        {
          // 2. Calculate PID terms
          
          // Integration Window: Only integrate when we are relatively close to the target.
          // If we are far away, the P-term has plenty of power, and integrating would just 
          // build up a massive windup debt while traveling or if physically held.
          if (current_error > -i_active_zone && current_error < i_active_zone)
          {
            integral += current_error;
          }
          else
          {
            integral = 0.0f; 
          }
          
          // Anti-windup for the integral term
          // Dynamically clamp the integral so it can't contribute more than max_i_term
          float max_integral = ki > 0.0f ? (max_i_term / ki) : 0.0f;
          if (integral > max_integral) integral = max_integral;
          if (integral < -max_integral) integral = -max_integral;
          
          float derivative = current_error - previous_error;
          
          // Disable D term when we are within N counts from the setpoint
          // to prevent the D-term from reacting to high-frequency noise near the target
          if (enable_deadzones && current_error > -d_deadzone && current_error < d_deadzone)
          {
            derivative = 0.0f;
          }
          
          // Calculate P, I, D components separately
          float p_term = kp * current_error;
          float i_term = ki * integral;
          float d_term = kd * derivative;
        
          // Apply stiction compensation (min_pwm) exclusively to the P-term.
          // This gives the D-term and I-term full linear authority to smoothly brake
          // the motor all the way down to 0 (and even into negative for active braking)
          // without triggering a violent +/- 60% jump in PWM.
          if (p_term > 0)
          {
            p_term += min_pwm;
          }
          else if (p_term < 0)
          {
            p_term -= min_pwm;
          }
        
          // 3. Compute the final motor speed (-100 to 100)
          float output = p_term + i_term + d_term;
          current_motor_speed = (int)output;
        
          // 4. Drive the Motor
          APP_Motor_SetSpeed(current_motor_speed);
          
          previous_error = current_error;
        }
      }
      else
      {
        // Motor is disabled - Let the user control it manually
        APP_Motor_SetSpeed(0);
      }
    }

    // --- 2. Slow Print Loop (Runs every 100ms = 10Hz) ---
    if (current_time - last_print_time >= 100)
    {
      last_print_time = current_time;
      
      if (motor_enabled)
      {
        printf("SP: %d | ADC: %d | ERR: %d | OUT: %d\r\n", current_setpoint, (int)filtered_adc, (int)current_error, current_motor_speed);
      }
      else
      {
        printf("Motor Disabled | ADC: %d\r\n", (int)filtered_adc);
      }
    }
  }
}

void APP_ErrorHandler(void) {
  while (1);
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
  while (1);
}
#endif /* USE_FULL_ASSERT */
