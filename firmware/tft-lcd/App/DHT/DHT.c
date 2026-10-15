#include "DHT.h"    
#include "main.h"

void DHT_Init_InPut(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.Pin=DHT11_Pin;
	GPIO_InitStructure.Mode=GPIO_MODE_INPUT;
	HAL_GPIO_Init(GPIOB,&GPIO_InitStructure);
}

void DHT_Init_OutPut(void)
{	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.Pin=DHT11_Pin;
	GPIO_InitStructure.Mode=GPIO_MODE_OUTPUT_PP;

	GPIO_InitStructure.Speed=GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB,&GPIO_InitStructure);
}

void DHT_Start(void)
{
	DHT_Init_OutPut();
	
	HAL_GPIO_WritePin(GPIOB,DHT11_Pin,GPIO_PIN_RESET); // 拉低总线
	delay_us(19000);
	HAL_GPIO_WritePin(GPIOB,DHT11_Pin,GPIO_PIN_SET); // 拉高总线
	delay_us(20);
	DHT_Init_InPut();
}

uint16_t DHT_Scan(void)
{
	return HAL_GPIO_ReadPin(GPIOB,DHT11_Pin); // 返回读取数据
}

uint16_t DHT_ReadBit(void)
{
	while(HAL_GPIO_ReadPin(GPIOB,DHT11_Pin) == RESET);
	delay_us(40);
	if(HAL_GPIO_ReadPin(GPIOB,DHT11_Pin) == SET)
	{
		while(HAL_GPIO_ReadPin(GPIOB,DHT11_Pin) == SET);
		return 1;
	}
	else
	{
		return 0;
	}
}

uint16_t DHT_ReadByte(void)
{
	uint16_t i, data = 0;
	for(i = 0; i < 8; i++)
	{
		data <<= 1; // 向左进一位
		data |= DHT_ReadBit();
	}
	return data;
}

uint16_t DHT_ReadData(uint8_t buffer[5])
{
	uint16_t i =0;
	
	DHT_Start();
	if(DHT_Scan() == RESET)
	{
		while(DHT_Scan() == RESET);
		while(DHT_Scan() == SET);
		for(i = 0; i < 5; i++)
		{
			buffer[i] = DHT_ReadByte();
		}
		// DHT11输出的40位数据
		while(DHT_Scan() == RESET);
		DHT_Init_OutPut();
		HAL_GPIO_WritePin(GPIOB,DHT11_Pin,GPIO_PIN_SET);
		
		uint8_t check = buffer[0] + buffer[1] + buffer[2] + buffer[3];
		if(check != buffer[4])
		{
			return 1; // 数据出错
		}
	}
	return  0;
}

