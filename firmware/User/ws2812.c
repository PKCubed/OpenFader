#include "ws2812.h"
#include "py32f0xx_hal.h"
#include <string.h>

TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_up;

#define WS2812_HIGH WS2812_PIN
#define WS2812_LOW (WS2812_PIN << 16)

uint32_t ws2812_dma_buffer[72];
volatile uint8_t ws2812_dma_ready = 1;

void WS2812_DmaCplt(DMA_HandleTypeDef *hdma) {
    // Stop timer and DMA
    HAL_TIM_Base_Stop(&htim1);
    __HAL_TIM_DISABLE_DMA(&htim1, TIM_DMA_UPDATE);
    
    // Ensure pin is low after transfer
    WS2812_PORT->BRR = WS2812_PIN;
    
    ws2812_dma_ready = 1;
}

void DMA1_Channel1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_tim1_up);
}

void WS2812_Init(void) {
    // 1. Enable Clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_DMA_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    // 2. GPIO Init (PA5 as Output)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = WS2812_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(WS2812_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WS2812_PORT, WS2812_PIN, GPIO_PIN_RESET);

    // 3. DMA Channel Init
    hdma_tim1_up.Instance = DMA1_Channel1;
    hdma_tim1_up.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_tim1_up.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tim1_up.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim1_up.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_tim1_up.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_tim1_up.Init.Mode = DMA_NORMAL;
    hdma_tim1_up.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    HAL_DMA_Init(&hdma_tim1_up);

    // Register callback manually
    hdma_tim1_up.XferCpltCallback = WS2812_DmaCplt;

    // 4. Map TIM1_UP to DMA1 Channel 1 via SYSCFG
    // LL_SYSCFG_DMA_MAP_TIM1_UP is 0x10 for DMA1_CH1 (bits 4:0 in CFGR3)
    SYSCFG->CFGR3 = (SYSCFG->CFGR3 & ~0x1FUL) | 0x10UL;

    // 5. Link DMA to TIM1
    __HAL_LINKDMA(&htim1, hdma[TIM_DMA_ID_UPDATE], hdma_tim1_up);

    // 6. DMA Interrupt
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    // 7. TIM1 Init
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 9; // 24MHz / 10 = 2.4MHz (416.6ns per DMA transfer)
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim1);
}

void WS2812_SendPixel(uint8_t r, uint8_t g, uint8_t b) {
    // Wait until previous transfer finishes
    while(!ws2812_dma_ready);
    ws2812_dma_ready = 0;

    // WS2812 expects GRB order
    uint32_t grb = (g << 16) | (r << 8) | b;
    uint32_t idx = 0;
    
    // Each bit is represented by 3 DMA transfers to BSRR
    for (int i = 23; i >= 0; i--) {
        if (grb & (1 << i)) {
            // Logic 1: High, High, Low
            ws2812_dma_buffer[idx++] = WS2812_HIGH;
            ws2812_dma_buffer[idx++] = WS2812_HIGH;
            ws2812_dma_buffer[idx++] = WS2812_LOW;
        } else {
            // Logic 0: High, Low, Low
            ws2812_dma_buffer[idx++] = WS2812_HIGH;
            ws2812_dma_buffer[idx++] = WS2812_LOW;
            ws2812_dma_buffer[idx++] = WS2812_LOW;
        }
    }

    // Start DMA transfer
    HAL_DMA_Start_IT(&hdma_tim1_up, (uint32_t)ws2812_dma_buffer, (uint32_t)&WS2812_PORT->BSRR, 72);
    
    // Enable TIM Update DMA Request
    __HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);
    
    // Start TIM1
    HAL_TIM_Base_Start(&htim1);
}

void WS2812_Show(void) {
    // A delay of >50us causes the WS2812 to latch the data.
    // HAL_Delay(1) is 1ms, which is plenty.
    HAL_Delay(1);
}
