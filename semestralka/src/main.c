/* Includes ------------------------------------------------------------------*/
#include "main.h"

RCC_ClocksTypeDef RCC_Clocks;
int main(void) {

	RCC_GetClocksFreq(&RCC_Clocks);
	SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);


	while (1) {


	}

}



#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t* file, uint32_t line)
{
	while (1)
	{
	}
}
#endif

