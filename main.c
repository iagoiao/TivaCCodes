
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "inc/tm4c123gh6pm.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#define TARGET_IS_BLIZZARD_RB1//usar antes de rom.h
#include "driverlib/rom.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/ssi.h"
#include "driverlib/sysctl.h"
#include "driverlib/fpu.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "utils/uartstdio.h"
#include "driverlib/uart.h"
#include "inc/hw_ints.h"
#include "inc/hw_gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/eeprom.h"
#include "driverlib/hibernate.h"


#define DS18B20_PORT_BASE        GPIO_PORTA_BASE
#define DS18B20_PERIPH           SYSCTL_PERIPH_GPIOA
#define DS18B20_PIN              GPIO_PIN_3
#define DS18B20_1                GPIOPinWrite(DS18B20_PORT_BASE,DS18B20_PIN, DS18B20_PIN)
#define DS18B20_0                GPIOPinWrite(DS18B20_PORT_BASE,DS18B20_PIN, 0)
#define DS18B20_Val              GPIOPinRead (DS18B20_PORT_BASE, DS18B20_PIN)
#define DS18B20_Input            GPIOPinTypeGPIOInput(DS18B20_PORT_BASE, DS18B20_PIN)
#define DS18B20_Output           GPIOPinTypeGPIOOutput(DS18B20_PORT_BASE, DS18B20_PIN)
#define MaxNOfSensors 10 //Número máximo de sensores(nem todos precisam estar sendo usados necessariamente)
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define TRUE 1
#define FALSE 0
static unsigned char dscrc_table[] = {
		0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
		157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
		35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
		190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
		70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
		219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
		101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
		248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
		140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
		17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
		175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
		50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
		202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
		87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
		233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
		116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

// method declarations
void Init_Hw(void);
int  OWFirst();
int  OWNext();
int  OWVerify();
void OWTargetSetup(unsigned char family_code);
void OWFamilySkipSetup();
int  OWReset();
void OWWriteByte(unsigned char byte_value);
void OWWriteBit(unsigned char bit_value);
int  OWSearch();
unsigned char docrc8(unsigned char value);
int DS18B20_ResetAndConvertAll(void);
int DS18B20_OUTPUTdata (float data,int sensor);
signed long DS18B20_ResetAndRead(uint32_t sensor);
void DS18B20_WaitConvertion(void);

// global search state
//unsigned char ALLROMS[8][MaxNOfSensors];
uint32_t ui32Status;
unsigned char ROM_NO[8];
int LastDiscrepancy;
int LastFamilyDiscrepancy;
int LastDeviceFlag;
unsigned char crc8;
int NOfSensors=0;//Numero de sensores encontrados pelo search;
uint32_t pui32NVData[64];//array para salvar no modulo de hibernação-[0]-On/off,[1]-read/convert[2]-NOfsensores
//void UARTIntHandler(void)//Interrupt generated by UART
//{
//	uint32_t RecChar;
//	uint32_t ui32Status;
//	//	HibernateDataGet(pui32NVData, 64);
//	//	NOfSensors=pui32NVData[0];
//	ui32Status = UARTIntStatus(UART0_BASE, true); //get interrupt status
//	UARTIntClear(UART0_BASE, ui32Status); //clear the asserted interrupts
//	RecChar=UARTCharGetNonBlocking(UART0_BASE);
//	if(RecChar=='t')
//	{
//		float fdat=0;
//		DS18B20_ResetAndConvertAll();
//		DS18B20_WaitConvertion();
//		int k;
//		for(k=0; k<NOfSensors; k++)//Read/print all temperatures and send them trough UART
//		{
//			fdat = (float)DS18B20_ResetAndRead(k)/16;//
//			printf("Temperatura para sensor %d : %f C \n",k,fdat);
//			DS18B20_OUTPUTdata(fdat,k);
//		}
//
//	}
//	else
//	{
//		UART_Send_Str("   Pressione t para ler os sensores    ");
//	}
//}


void main (void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_HIBERNATE);
	if(HibernateIsActive())
	{
		Init_Hw();

		// Read the status to determine cause of wake.
		int temp=HibernateRTCGet();
		ui32Status = HibernateIntStatus(false);

		HibernateDataGet(pui32NVData, 64);
		NOfSensors=pui32NVData[2];
		Delay1us(15);
		if(ui32Status & HIBERNATE_INT_PIN_WAKE)//Se acordou devido ao SW2
		{
			IntMasterDisable();
			Delay1ms(500);
			IntMasterEnable();
			if(pui32NVData[0]==0)//Se acordou devido ao sw2 e estava dormindo anteriormente..
			{
				pui32NVData[0]=1;//indica que agora estará fazendo leituras(on)
				pui32NVData[1]=1;//indica que os sensores já vão possuir temperaturas armazendas
				DS18B20_ResetAndConvertAll();//Faz todas as leituras
				HibernateDataSet(pui32NVData, 64);
				UARTCharPutNonBlocking(UART0_BASE,'W');
				HibernateRTCMatchSet(0,HibernateRTCGet()+1);
				HibernateRequest();

			}
			else if(pui32NVData[0]==1)//Se acordou devido ao sw2 e ja estava lendo dados, passa a dormir até sw2 ser pressionado novamente
			{
				pui32NVData[0]=0;//indica que agora não estará fazendo leituras(off)
				pui32NVData[1]=0;//indica que os sensores não vão possuir temperaturas armazendas
				UARTCharPut(UART0_BASE,'S');
				HibernateDataSet(pui32NVData, 64);
				HibernateRequest();
			}

		}
		else if(ui32Status & HIBERNATE_INT_RTC_MATCH_0)//Se acordou devido ao RTC
		{
			if(pui32NVData[0]==0)//Acordou indevidamente, volta a dormir
			{
				UARTCharPutNonBlocking(UART0_BASE,'I');
				HibernateDataSet(pui32NVData, 64);
				HibernateRequest();
			}
			else if(pui32NVData[0]==1)//Acordou devidamente
			{
				if(pui32NVData[1]==1)//Se já possuir temperaturas no sensores, acordou para enviarvia uart  e requisitar nova leitura
				{
					int k;
					for(k=0; k<NOfSensors; k++)//Read/print all temperatures and send them trough UART
					{
						float fdat=0;
						fdat = (float)DS18B20_ResetAndRead(k)/16;//
						DS18B20_OUTPUTdata(fdat,k);
					}
					UARTCharPutNonBlocking(UART0_BASE,'T');
					DS18B20_ResetAndConvertAll();
					pui32NVData[0]=1;//apenas indica que continuará em on(linha provavelmente desnecessária)
					pui32NVData[1]=1;//indica que os sensores terão temperaturas lidas
					HibernateDataSet(pui32NVData, 64);
					HibernateRTCMatchSet(0,HibernateRTCGet()+1);
					HibernateRequest();
				}
				else if(pui32NVData[1]==0)//Se acordou e os sensores não possuem temperatura lida recentemente,ordena leitura e volta a dormir
				{
					pui32NVData[0]=1;
					pui32NVData[1]=1;
					DS18B20_ResetAndConvertAll();
					UARTCharPutNonBlocking(UART0_BASE,'C');
					HibernateDataSet(pui32NVData, 64);
					HibernateRTCMatchSet(0,HibernateRTCGet()+1);

					HibernateRequest();
				}
			}
		}

	}
	else//Primeira vez que liga, inicia normalmente.
	{
//		FPULazyStackingEnable();
//		SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL |SYSCTL_OSC_MAIN |SYSCTL_XTAL_16MHZ);
//
//		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
//		SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
//		GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE,GPIO_PIN_1);
//		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
//		SysCtlPeripheralEnable(DS18B20_PERIPH);
//		SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);
//
//		GPIOPinConfigure(GPIO_PA0_U0RX);
//		GPIOPinConfigure(GPIO_PA1_U0TX);
//		GPIOPinTypeUART(GPIO_PORTA_BASE,GPIO_PIN_0|GPIO_PIN_1);
//		UARTConfigSetExpClk(UART0_BASE,SysCtlClockGet(),115200,
//				(UART_CONFIG_WLEN_8|UART_CONFIG_STOP_ONE|UART_CONFIG_PAR_NONE));
//		IntMasterEnable();
//
//		HibernateEnableExpClk(SysCtlClockGet());
//		HibernateRTCTrimSet(0x7FFF);
//		HibernateRTCSet(0);
//		HibernateGPIORetentionEnable();
//		HibernateWakeSet(HIBERNATE_WAKE_PIN|HIBERNATE_WAKE_RTC);
//
//		HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;//Estas 5 linhas Destravam ,Configuram PF0 e PF1 como inputs e habilitam os pull-ups..
//		HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
//		HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;
//		GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_DIR_MODE_IN);//Config the pins as inputs
//		GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);//Activate Pull-up resistors on both pins
//
//		EEPROMInit();
		//GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_3, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD_WPU);//Activate Pull-up resistors on pin

		//	//GPIOPinWrite(GPIO_PORTF_BASE,GPIO_PIN_1,GPIO_PIN_1);
		//
		Init_Hw();
		HibernateRTCSet(0);


		pui32NVData[0]=0;
		pui32NVData[1]=0;
		DS18B20_SearchAndSaveAll();
		pui32NVData[2]=NOfSensors;
		HibernateDataSet(pui32NVData, 64);
		HibernateRTCEnable();
		HibernateRequest();
		//	pui32NVData[0]=NOfSensors;
		//	HibernateDataSet(pui32NVData,64);
		//	//	DS18B20_ResetAndConvertAll();
		//	DS18B20_WaitConvertion();
	}
}


void Delay1ms(unsigned long y)
{
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/(3000/y));
}
void Delay1us(unsigned long y)
{
	ROM_SysCtlDelay(ROM_SysCtlClockGet()/(3000000/y));
}
void Init_Hw(void)
{
	FPULazyStackingEnable();
			SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL |SYSCTL_OSC_MAIN |SYSCTL_XTAL_16MHZ);

			SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
			SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
			GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE,GPIO_PIN_1);
			SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
			SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);
			SysCtlPeripheralEnable(DS18B20_PERIPH);


			GPIOPinConfigure(GPIO_PA0_U0RX);
			GPIOPinConfigure(GPIO_PA1_U0TX);
			GPIOPinTypeUART(GPIO_PORTA_BASE,GPIO_PIN_0|GPIO_PIN_1);
			UARTConfigSetExpClk(UART0_BASE,SysCtlClockGet(),115200,
					(UART_CONFIG_WLEN_8|UART_CONFIG_STOP_ONE|UART_CONFIG_PAR_NONE));
			IntMasterEnable();

			HibernateEnableExpClk(SysCtlClockGet());
			HibernateRTCTrimSet(0x7FFF);
			HibernateGPIORetentionEnable();
			HibernateWakeSet(HIBERNATE_WAKE_PIN|HIBERNATE_WAKE_RTC);


			HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;//Estas 5 linhas Destravam ,Configuram PF0 e PF1 como inputs e habilitam os pull-ups..
			HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
			HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;
			GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_DIR_MODE_IN);//Config the pins as inputs
			GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);//Activate Pull-up resistors on both pins

			EEPROMInit();
}
void DS18B20_SelectSensor(int sensor)
{
	EEPROMRead(ROM_NO,sensor*8,8);//le a rom do sensor selecionado que está salva na eeprom
}
void DS18B20_SearchAndSaveAll(void)
{
	int rslt;
	rslt = OWFirst();
	while (rslt)
	{
		NOfSensors++;
		EEPROMProgram(ROM_NO,(NOfSensors-1)*8,8);//Salva na eeprom a ROM recem encontrada
		rslt = OWNext();
	}
}
/*******************************************************************************
 * Nome da função: DS18B20 WriteByte
 * Função: Escreva um byte para 18B20
 * Entrada: com
 * Saída: None
 *******************************************************************************/
void OWWriteByte(unsigned char dat)
{
	unsigned long j;
	Delay1ms(1);//verificar
	for(j=8;j>0;j--)
	{
		DS18B20_Output;
		DS18B20_0;   //antes de cada envio o bus é puxado para 0 por 1us
		Delay1us (3);
		if (dat&0x01)           //Em seguida, escrever um bit, na posição mais baixa
			DS18B20_1;
		Delay1us (65);           //Durante peo menos 60us
		DS18B20_Input; //Em seguida, solta o bus para o tempo de recuperação, pelo menos 1us, pode então escrever o segundo valor
		Delay1us (2);
		dat>>=1;
	}
}
/*******************************************************************************
 * Nome da função: DS18B20 Leia Byte
 * Função: Lê um byte
 * Entrada: com
 * Saída: None
 *******************************************************************************/

unsigned char OWReadByte()
{
	unsigned char dat=0;
	unsigned long j;

	for(j=8;j>0;j--)
	{
		DS18B20_Output;
		dat >>= 1;
		DS18B20_0;//Bus para 0 por pelo menos 1us
		Delay1us (2);
		DS18B20_Input; //Solta o Bus em seguida
		Delay1us (12);//Espera até o momento de ler os dados
		if (DS18B20_Val)
		{
			dat |= 0x80;
		}
		Delay1us (45);

		DS18B20_Input;

	}
	return dat;
}
/******************************************88
 *OWReadBit
 *
 *
 **/
int OWReadBit()
{
	Delay1us(40);
	DS18B20_Output;
	DS18B20_0;//Bus para 0 por pelo menos 1us
	Delay1us (2);
	DS18B20_Input; //Solta o Bus em seguida
	Delay1us (13);//Espera até o momento de ler os dados
	if(DS18B20_Val)
		return TRUE;
	else
		return FALSE;

}
// Send 1 bit of data to teh 1-Wire bus
//
void OWWriteBit(unsigned char bit)
{
	Delay1us(40);
	DS18B20_Output;//bus para low por ~1us
	DS18B20_0;
	Delay1us(3);
	if(bit==1)//para escrever 1 solta o bus
		DS18B20_Input;
	Delay1us(60);
	DS18B20_Input;
}
/*******************************************************************************
 * Nome da função: DS18B20 Leia Temp
 * Função: Leia temperatura
 * Entrada: com
 * Saída: None
 *******************************************************************************/
signed long DS18B20_ResetAndRead(uint32_t sensor)
{

	unsigned long temp=0;
	unsigned char tmh,tml;
	DS18B20_SelectSensor(sensor);
	OWReset();
	OWWriteByte(0x55);//comando seleionar Rom
	int i;

	for( i=0;i <=7;i++)
		OWWriteByte(ROM_NO[i]);

	OWWriteByte(0xbe);  //Envia comando para ler temperatura
	tml=OWReadByte();  //um total de 16 bits ，le o byte mais   baixo
	tmh=OWReadByte();  //le o byte mais alto
	temp=tmh;
	temp<<=8;
	temp|=tml;
	return temp;
}

int DS18B20_ResetAndConvertAll(void)
{

	unsigned char check =1;
	check=OWReset();
	if(check)
	{
		printf("Device is not there");
		return 1;
	}
	OWWriteByte(0xcc);  //Comando pular ROM
	OWWriteByte(0x44);         //Converter temperatura
	return 0;

}

void DS18B20_WaitConvertion(void)
{
	Delay1ms(750);
}

//************************************implementação de search**************************************************


//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : no device present
//
int OWFirst()
{
	// reset the search state
	LastDiscrepancy = 0;
	LastDeviceFlag = FALSE;
	LastFamilyDiscrepancy = 0;

	return OWSearch();
}

//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
int OWNext()
{
	// leave the search state alone
	return OWSearch();
}

//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
int OWSearch()
{
	int id_bit_number;
	int last_zero, rom_byte_number, search_result;
	int id_bit, cmp_id_bit;
	unsigned char rom_byte_mask, search_direction;

	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = 0;
	crc8 = 0;

	// if the last call was not the last one
	if (!LastDeviceFlag)
	{
		// 1-Wire reset
		if (OWReset())
		{
			// reset the search
			LastDiscrepancy = 0;
			LastDeviceFlag = FALSE;
			LastFamilyDiscrepancy = 0;
			return FALSE;
		}

		// issue the search command
		OWWriteByte(0xf0);
		// loop to do the search
		do
		{

			// read a bit and its complement
			id_bit = OWReadBit();
			cmp_id_bit = OWReadBit();

			// check for no devices on 1-wire
			if ((id_bit == 1) && (cmp_id_bit == 1))
			{
				printf(" No device found at bit %d  \n",id_bit_number);
				break;
			}
			else
			{
				// all devices coupled have 0 or 1
				if (id_bit != cmp_id_bit)
				{
					//					printf("Bit encontrado! %d posicao %d \n",id_bit,id_bit_number);
					search_direction = id_bit;  // bit write value for search
				}
				else
				{
					// if this discrepancy if before the Last Discrepancy
					// on a previous next then pick the same as last time
					if (id_bit_number < LastDiscrepancy)
						search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
					else
						// if equal to last pick 1, if not then pick 0
						search_direction = (id_bit_number == LastDiscrepancy);

					// if 0 was picked then record its position in LastZero
					if (search_direction == 0)
					{
						last_zero = id_bit_number;

						// check for Last discrepancy in family
						if (last_zero < 9)
							LastFamilyDiscrepancy = last_zero;
					}
				}

				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1)
					ROM_NO[rom_byte_number] |= rom_byte_mask;
				else
					ROM_NO[rom_byte_number] &= ~rom_byte_mask;

				// serial number search direction write bit
				OWWriteBit(search_direction);

				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;

				// if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
				if (rom_byte_mask == 0)
				{
					docrc8(ROM_NO[rom_byte_number]);  // accumulate the CRC
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		}
		while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

		// if the search was successful then
		if (!((id_bit_number < 65) || (crc8 != 0)))
		{
			// search successful so set LastDiscrepancy,LastDeviceFlag,search_result
			LastDiscrepancy = last_zero;

			// check for last device
			if (LastDiscrepancy == 0)
				LastDeviceFlag = TRUE;

			search_result = TRUE;
		}
	}

	// if no device found then reset counters so next 'search' will be like a first
	if (!search_result || !ROM_NO[0])
	{
		printf("As no other device was found, quiting search \n");
		LastDiscrepancy = 0;
		LastDeviceFlag = FALSE;
		LastFamilyDiscrepancy = 0;
		search_result = FALSE;
	}

	return search_result;
}


//--------------------------------------------------------------------------
// Verify the device with the ROM number in ROM_NO buffer is present.
// Return TRUE  : device verified present
//        FALSE : device not present
//
int OWVerify()
{
	unsigned char rom_backup[8];
	int i,rslt,ld_backup,ldf_backup,lfd_backup;

	// keep a backup copy of the current state
	for (i = 0; i < 8; i++)
		rom_backup[i] = ROM_NO[i];
	ld_backup = LastDiscrepancy;
	ldf_backup = LastDeviceFlag;
	lfd_backup = LastFamilyDiscrepancy;

	// set search to find the same device
	LastDiscrepancy = 64;
	LastDeviceFlag = FALSE;

	if (OWSearch())
	{
		// check if same device found
		rslt = TRUE;
		for (i = 0; i < 8; i++)
		{
			if (rom_backup[i] != ROM_NO[i])
			{
				rslt = FALSE;
				break;
			}
		}
	}
	else
		rslt = FALSE;

	// restore the search state
	for (i = 0; i < 8; i++)
		ROM_NO[i] = rom_backup[i];
	LastDiscrepancy = ld_backup;
	LastDeviceFlag = ldf_backup;
	LastFamilyDiscrepancy = lfd_backup;

	// return the result of the verify
	return rslt;
}

//--------------------------------------------------------------------------
// Setup the search to find the device type 'family_code' on the next call
// to OWNext() if it is present.
//
void OWTargetSetup(unsigned char family_code)
{
	int i;

	// set the search state to find SearchFamily type devices
	ROM_NO[0] = family_code;
	for (i = 1; i < 8; i++)
		ROM_NO[i] = 0;
	LastDiscrepancy = 64;
	LastFamilyDiscrepancy = 0;
	LastDeviceFlag = FALSE;
}

//--------------------------------------------------------------------------
// Setup the search to skip the current device type on the next call
// to OWNext().
//
void OWFamilySkipSetup()
{
	// set the Last discrepancy to last family discrepancy
	LastDiscrepancy = LastFamilyDiscrepancy;
	LastFamilyDiscrepancy = 0;

	// check for end of list
	if (LastDiscrepancy == 0)
		LastDeviceFlag = TRUE;
}

//--------------------------------------------------------------------------
// 1-Wire Functions to be implemented for a particular platform
//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current
// global 'crc8' value.
// Returns current global crc8 value
//
unsigned char docrc8(unsigned char value)
{
	crc8 = dscrc_table[crc8 ^ value];
	return crc8;
}
//--------------------------------------------------------------------------
// Reset the 1-Wire bus and return the presence of any device
// Return TRUE  : device present
//        FALSE : no device present
//
int OWReset()
{

	int error=0;
	DS18B20_Output;
	DS18B20_1;
	Delay1us ( 15);
	DS18B20_0;

	Delay1us( 480 );  // delay for 480us
	DS18B20_Input;
	Delay1us( 70 );  // delay for 70us

	if(DS18B20_Val)
	{
		error = TRUE;      // Presense pulse not detected
	}
	else
	{
		error = FALSE;     // ds18b20 is there
	}


	Delay1us(410);
	return ( error );        // return the sampled presence pulse result}
}
int DS18B20_OUTPUTdata(float data, int sensor)//Temperatura de saída uart
{
	unsigned int temp=0;
	unsigned int d1,d2,d3;

	temp=data*10;

	d3=temp%10;
	d2=((temp-d3)/10)%10;
	d1=((temp-d3-(d2*10))/100);
	UART_Send_Str("Sensor ");
	UARTCharPutNonBlocking(UART0_BASE,sensor+48);
	UARTCharPutNonBlocking(UART0_BASE,':');
	UARTCharPutNonBlocking(UART0_BASE,d1+48);
	UARTCharPutNonBlocking(UART0_BASE,d2+48);
	UARTCharPutNonBlocking(UART0_BASE,'.');
	UARTCharPutNonBlocking(UART0_BASE,d3+48);
	UARTCharPutNonBlocking(UART0_BASE,'C');
	UARTCharPutNonBlocking(UART0_BASE,' ');
	return 0;
}
void UART_Send_Str(unsigned char *s)//Caractere de saída
{
	int i=0;
	while(s[i]!=0)
	{
		UARTCharPut(UART0_BASE,s[i]);
		i++;
	}
}
void UART_init(unsigned temp)//Configurando interface serial
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	GPIOPinConfigure(GPIO_PA0_U0RX);
	GPIOPinConfigure(GPIO_PA1_U0TX);
	GPIOPinTypeUART(GPIO_PORTA_BASE,GPIO_PIN_0|GPIO_PIN_1);
	UARTConfigSetExpClk(UART0_BASE,SysCtlClockGet(),115200,
			(UART_CONFIG_WLEN_8|UART_CONFIG_STOP_ONE|UART_CONFIG_PAR_NONE));
	UARTCharPut(UART0_BASE,temp);
}
//--------------------------------------------------------------------------
// Send 8 bits of data to the 1-Wire bus
//

//--------------------------------------------------------------------------


//--------------------------------------------------------------------------


/*

{
		UART_Send_Str("t0.txt=");//Caractere de saída
		UART_init(34);//saída citou codigo ASCII 34
		DS18B20_OUTPUTdata(fdat);//Temp de saída
		UART_init(34);//citação de saída
		UART_Send_END();//Finaliza
		UART_Send_Str("j0.val=");
		DS18B20_OUTPUTdata(fdat);
		UART_Send_END();
		UART_Send_Str("t2.txt=");//Caractere de saída
		UART_init(34);//saída citou codigo ASCII 34
		UART_Send_Str("湖北民族学院科技学院-吴刚");//Temp de saída
		UART_init(34);//Citações de saída
		UART_Send_END();//Envia END
}



void UART_Send_END(void)   //Caracter END
{
	UART_init(0xFF);
	UART_init(0xFF);
	UART_init(0xFF);
}




 */
