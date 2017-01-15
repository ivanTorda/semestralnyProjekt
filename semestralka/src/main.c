/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "init.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "stm32l1xx_flash.h"

#define NUMBER_OF_SAMPLES 10
#define MAX_STRLEN 12 // this is the maximum string length of our string in characters

char received_string[MAX_STRLEN+1]; // this will hold the recieved string
uint8_t eeprom[256];
FLASH_Status FLASHStatus;
///////////////////////////////////////////////////////////
//vsetky tieto premenne su nejake stavove premenne, FLAGY,
//actualCurrent je najnovsia prefiltrovana vzorka prudu
/////////////////////////////////////////////////////////
uint16_t currentSamples[NUMBER_OF_SAMPLES];
uint8_t sampleCounter = 0;

float actualCurrent = 0.00;

float current = 0.00;
uint16_t voltage = 0.00;

uint32_t eepromCurrentPosition = 0;
uint8_t currentMeasureTime = 0; //counter for make a sample
uint8_t voltageMeasureTime = 0;
uint8_t startMeasureFlag = 0; //start/stop flag for measuring
uint8_t clearEEPROMFlag = 0; //start/stop flag for measuring

uint8_t measureStatus = 0; //0 - not measuring; 1 - measuring; 2 - stopped measure, EEPROM overflow; 3 - stopped measure low battery;
uint8_t startRead = 0;


//Ak je clanok plne nabity, tak ma napatie 4.2 Volta
//Ak je clanok vybity, tak kriticke napatie na clanku, 3.4Volta
//a ked sa nezastavi vybijaci proces pod tuto hranicu tak dochadza k poskodeniu clanku :P
uint8_t lowVoltageCell = 3.4;

//pocet clankov v baterii, ja som pouzival 3 clanky, cize dokopy pri nabitej baterii je 3*4.2V = 12.6V
//pri vybitej baterii je to zase 3*3.4V = 10.2V
uint8_t numberOfCell = 3;

// to je premenna pre kriticke napatie baterky 3*3.4V = 10.2V
uint8_t voltageCutOff;

//counter
uint8_t i = 0;

uint16_t receivedChar;
RCC_ClocksTypeDef RCC_Clocks;


int main(void) {

	RCC_GetClocksFreq(&RCC_Clocks);
	SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);

	adc_init();
	NVICInit();
	UART_init();
	InitializeTimer();
	outputPortInit();
	LED_init();
	clearEEPROM();
	USART_puts("ready\r");

	voltageCutOff = lowVoltageCell * numberOfCell;

	memset(currentSamples, 0, NUMBER_OF_SAMPLES);


	while (1) {


	}
}

void TIM4_IRQHandler(void) {
	if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {

		if (startMeasureFlag == 1) {

			//curent time counter reset//29
			if(currentMeasureTime >= 6){
				currentMeasureTime=0;
			}

			//get 5second delay for undervoltage protection
			if (voltageMeasureTime >= 10) {

				if (getVoltage() <= 10.2) {
					stopMeasure();
					measureStatus = 3;
					voltageMeasureTime = 0;
					currentMeasureTime = 0;
					GPIO_SetBits(GPIOA, GPIO_Pin_7);
					TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
					return;
				}
				voltageMeasureTime = 0;
			}

			//measure on current every 30seconds
			if (currentMeasureTime == 0) {
				GPIO_ResetBits(GPIOA, GPIO_Pin_7);
				//reset counter if EEPROM is full
				if (eepromCurrentPosition >= 255) {
					stopMeasure();
					measureStatus = 2;
					GPIO_SetBits(GPIOA, GPIO_Pin_7);
					TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
					return;
				}else{
					writeEEPROMByte(eepromCurrentPosition,(uint8_t) actualCurrent); //write current into EEPROM
					eeprom[eepromCurrentPosition]=actualCurrent;
					eepromCurrentPosition++;
					measureStatus = 1;
					GPIO_SetBits(GPIOA, GPIO_Pin_7);
				}
			}

			currentMeasureTime++;
			voltageMeasureTime++;
		}
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
	}
}

void TIM3_IRQHandler(void) {
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
		//get 10 samples, save it into array
		if (sampleCounter < 10) {
			currentSamples[sampleCounter] = Read_AD_Value(ADC_Channel_0);
			sampleCounter++;
		}

		// if we got 10 samples then make an average
		else {
			i = 0;
			while (i < NUMBER_OF_SAMPLES) {
				current += currentSamples[i];
				i++;
			}
			current = current / NUMBER_OF_SAMPLES;
			actualCurrent = fabs(current - 3095); //actualCurrent = abs(current - 3095) * 0.0165;
			sampleCounter = 0;
			current = 0.00;
		}
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
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

/*Funkcia, ktora po zavolani spusti proces merania, nastavia sa flagy ktore sa riesia v TIM4_IRQHandler
* Zapne ziarovku, zatazovy obvod..
* (zapne pin PA6, ktory napa rele a ten zase ziarovku)
*
*/
void startMeasure() {
	measureStatus = 0;
	voltageMeasureTime = 0;
	startMeasureFlag = 1;
	GPIO_ResetBits(GPIOA, GPIO_Pin_6);
}

/*
 * Funkcia, ktora po zavolani zastavi celkovo proces merania, nastavi sa startMeasureFlag a aj measureStatus na nulu.
 * A natvrdo sa odpoji ziarovkovy, zatazovy obvod..
 * (vypne pin PA6, ktory napa rele a ten zase ziarovku)
 * Viem ze pre vypnutie tam nema byt Set ale Reset, lenze to rele ma prevratenu logiku.. don't care
 */
void stopMeasure() {
	startMeasureFlag = 0;
	measureStatus = 0;
	GPIO_SetBits(GPIOA, GPIO_Pin_6);

}

void handleUSARTCommands(){
	char str[10];
	if (strcmp(received_string, "at\r") == 0) {
		STM_EVAL_LEDOn(LED2);
		memset(received_string, '0', sizeof(received_string));
		USART_puts("OK\r");
		STM_EVAL_LEDOff(LED2);
		return;
	}
	if (strcmp(received_string, "amp\r") == 0) {
		STM_EVAL_LEDOn(LED2);
		memset(received_string, '0', sizeof(received_string));
		sprintf(str, "%.3f\r", actualCurrent);
		USART_puts(str);
		STM_EVAL_LEDOff(LED2);
		return;
		//Ak pride 'v'(Voltage), tak mi hod naspat aktualne napatie baterky, a blikni ledkou..
	}
	if (strcmp(received_string, "volt\r") == 0) {
		STM_EVAL_LEDOn(LED2);
		memset(received_string, '0', sizeof(received_string));
		sprintf(str, "%.3f\r", getVoltage());
		USART_puts(str);
		STM_EVAL_LEDOff(LED2);
		return;
		//Ak pride 'm' (Measure status), tak mi hod naspat stav, ci sa meria/nemeria/kalibruje.. proste aby som vedel
		//ze ci nieco robi :P
	}
}


/*
 * funkcia, ktora mi pri zavolani vypluje aktualne napatie vo Voltoch
 * je to prenasobene bulharskou konstantou 0.0065 kt. som ziskal empirickym meranim
 */
float getVoltage(void) {
	return Read_AD_Value(ADC_Channel_1) * 0.0065;
}

void clearEEPROM() {
	for (uint16_t j = 0; j < 256; j++) {
		FLASHStatus = writeEEPROMByte(j, (uint8_t) 0);
		eeprom[j]=0;
	}
	eepromCurrentPosition = 0;
}

FLASH_Status writeEEPROMByte(uint32_t address, uint8_t data) {
    FLASH_Status status = FLASH_COMPLETE;
    address = address + 0x08080000;
    DATA_EEPROM_Unlock();  //Unprotect the EEPROM to allow writing
    status = DATA_EEPROM_ProgramByte(address, data);
    DATA_EEPROM_Lock();  // Reprotect the EEPROM
    return status;
}

uint8_t readEEPROMByte(uint32_t address) {
    uint8_t tmp = 0;
    address = address + 0x08080000;
    tmp = *(__IO uint32_t*)address;

    return tmp;
}


#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t* file, uint32_t line)
{
	while (1)
	{
	}
}
#endif

