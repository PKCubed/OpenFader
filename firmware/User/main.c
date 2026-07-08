#include "py32f0xx_hal.h"
#include "py32f0xx_bsp_printf.h"
#include "ws2812.h"
#include "SEGGER_RTT.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

void APP_ErrorHandler(void);

ADC_HandleTypeDef AdcHandle;
DMA_HandleTypeDef HdmaCh1;

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
  
  while (1)
  {
    // That's it! We don't need to trigger the ADC or wait for it.
    // The hardware is doing it all automatically in the background.
    // We just print whatever the latest value is whenever we feel like it.
    
    printf("PA4 ADC Value: %lu\r\n", latest_adc_val);
    
    // Sleep for 100 milliseconds (just to prevent spamming the console too fast)
    HAL_Delay(100); 
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
