#include "stm32l1xx_nucleo.h"




/*
 * Inicializacia UARTU, konkretne USART1, vyuzivaju sa
 * piny PA9 a PA10, rychlost 9600 BaudRate/s
 * Plus dole je k tomu inicializacia prerusenia (IRQ) pre UART.
 * To znamena ze ked nieco pride na uart, nejaky znak
 * vykona sa prerusenie a prijaty znak sa ulozi do premennej
 *
 */
void UART_init(void){

	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_40MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART1, ENABLE);

}

/*
 * Inicializacia LEDky.. nic extra
 * blikne ak zaregistroval povel cez UART, riesi sa to v hlavnej funkcii main()
 */
void LED_init(void) {
	STM_EVAL_LEDInit(LED2);
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_EXTI);
}


/*
 * Incializacia internych preruseni TIM3 a TIM4 (Timerov)
 * takze Pouzivame 2 interne prerusenia,
 * Prvy Timer TIM4 zbehne kazdych 0.5 sekundy (vid TIM.TIM_Period = 500; //ms)
 * Tento vyuzivame aby kazdu tuto periodu riesila zapis do EEPROM, a vobec cely
 * meraci proces.
 * Este tu kontrolujem, ci napatie nekleslo pod nebezpecnu hodnotu.. 10.2V
 *  Podrobnejsie sa mu budem venovat v main.c -> TIM4_IRQHandler
 *
 * Druhy Timer TIM3 zbehne kazdych 50ms (vid TIM.TIM_Period = 50;)
 * Tu sa riesi samotne vzorkovanie prudu a napatie
 * napatie sa sice vzorkuje kazdych 50ms ale s prudom je to trosku zlozitejsie..
 * ale nic hrozne..
 * slubujem.
 */
void InitializeTimer(void) {
	TIM_TimeBaseInitTypeDef TIM;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	// Time base configuration
	TIM_TimeBaseStructInit(&TIM);
	TIM.TIM_Prescaler = (SystemCoreClock / 1000) - 1;
	TIM.TIM_Period = 500; // 500ms interval
	TIM.TIM_ClockDivision = 0;
	TIM.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM4, &TIM);
	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
	NVIC_EnableIRQ(TIM4_IRQn); // Enable TIM4 IRQ
	TIM_Cmd(TIM4, ENABLE); // Enable counter on TIM4

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	// Time base configuration
	TIM_TimeBaseStructInit(&TIM);
	TIM.TIM_Prescaler = (SystemCoreClock / 1000) - 1;
	TIM.TIM_Period = 50; // 50ms interval
	TIM.TIM_ClockDivision = 0;
	TIM.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM);
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
	NVIC_EnableIRQ(TIM3_IRQn); // Enable TIM4 IRQ
	TIM_Cmd(TIM3, ENABLE); // Enable counter on TIM4
}

