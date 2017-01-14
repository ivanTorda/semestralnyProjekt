/* Includes ------------------------------------------------------------------*/
#include "main.h"

#define MAX_STRLEN 12 // this is the maximum string length of our string in characters

char received_string[MAX_STRLEN+1]; // this will hold the recieved string
RCC_ClocksTypeDef RCC_Clocks;
int main(void) {

	RCC_GetClocksFreq(&RCC_Clocks);
	SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);

	UART_init();
	USART_puts("ready\r");


	while (1) {


	}

}

/*
 * USART prerusenie, ak pride na UART nejaky znak, prerusi sa cely kod a vykona sa prave tato funkcia.
 * jedina jej starost je, aby ulozila prijaty znak do globalnej premennej receivedChar.
 */
void USART1_IRQHandler(void) {
	if (USART_GetITStatus(USART1, USART_IT_RXNE)) {
			static uint8_t cnt = 0; // this counter is used to determine the string length
			char t = USART1->DR; // the character from the USART1 data register is saved in t
			if ((t != '\n') && (cnt < MAX_STRLEN)) {
				received_string[cnt] = t;
				cnt++;
			} else {
				cnt = 0;
				//USART_puts(USART1, USART_Handle(received_string));
				handleUSARTCommands();
				for (uint8_t i = 0; i <= MAX_STRLEN + 1; i++) {
					received_string[i] = '\0';
				}
			}
		}
}

/*
 * funkcia ktora dokaze poslat aj String cez UART
 * cize nie len pismenka ale aj cele slovo..
 */

void USART_puts(volatile char *s) {

	while (*s) {
		// wait until data register is empty
		while (!(USART1->SR & 0x00000040))
			;
		USART_SendData(USART1, *s);
		*s++;
	}
}

void handleUSARTCommands(){
	if (strcmp(received_string, "at\r") == 0) {
		STM_EVAL_LEDOn(LED2);
		memset(received_string, '0', sizeof(received_string));
		USART_puts("OK\r");
		STM_EVAL_LEDOff(LED2);
		return;
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

