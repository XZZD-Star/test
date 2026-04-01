#ifndef __MYIIC_H
#define __MYIIC_H

#include "fonts.h"

//SDA输入输出配置
#define SDA_PORT GPIOA
#define SDA_PIN GPIO_PIN_9
#define SDA2_PORT GPIOB
#define SDA2_PIN GPIO_PIN_8

// 配置为输入模式的宏
#define SDA_IN do { \
    GPIO_InitTypeDef GPIO_InitStruct = {0}; \
    GPIO_InitStruct.Pin = SDA_PIN; \
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; \
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; \
    HAL_GPIO_Init(SDA_PORT, &GPIO_InitStruct); \
} while(0)
#define SDA_OUT do { \
    GPIO_InitTypeDef GPIO_InitStruct = {0}; \
    GPIO_InitStruct.Pin = SDA_PIN; \
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; \
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; \
    HAL_GPIO_Init(SDA_PORT, &GPIO_InitStruct); \
} while(0)

#define SDA2_IN do { \
    GPIO_InitTypeDef GPIO_InitStruct = {0}; \
    GPIO_InitStruct.Pin = SDA2_PIN; \
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; \
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; \
    HAL_GPIO_Init(SDA2_PORT, &GPIO_InitStruct); \
} while(0)
#define SDA2_OUT do { \
    GPIO_InitTypeDef GPIO_InitStruct = {0}; \
    GPIO_InitStruct.Pin = SDA2_PIN; \
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; \
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; \
    HAL_GPIO_Init(SDA2_PORT, &GPIO_InitStruct); \
} while(0)

#define READ_SDA HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_9)                    //读取SDA

#define READ_SDA2 HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_8)                    //读取SDA

//高低电平设置
#define SCL_HIGH HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8, GPIO_PIN_SET)
#define SCL_LOW HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8, GPIO_PIN_RESET)
#define SDA_HIGH HAL_GPIO_WritePin(GPIOA,GPIO_PIN_9, GPIO_PIN_SET)
#define SDA_LOW HAL_GPIO_WritePin(GPIOA,GPIO_PIN_9, GPIO_PIN_RESET)

#define SCL2_HIGH HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7, GPIO_PIN_SET)
#define SCL2_LOW HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7, GPIO_PIN_RESET)
#define SDA2_HIGH HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8, GPIO_PIN_SET)
#define SDA2_LOW HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8, GPIO_PIN_RESET)

//IIC所有操作函数
void IIC_START(void);
void IIC_STOP(void);
uint8_t IIC_Wait_Ack(void);
void IIC_Ack(void);
void IIC_NAck(void);
void IIC_Send_Byte(uint8_t txd);
uint8_t IIC_Read_Byte(uint8_t Ack);
uint8_t IIC_Read_OneByte(unsigned char IIC_Addr, unsigned char addr);
uint8_t IIC_Read_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data);
uint8_t IIC_Write_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data);
uint8_t IIC_Creat_Byte(uint8_t dev,uint8_t reg,uint8_t *data);
uint8_t IIC_Write_Byte(uint8_t dev,uint8_t reg,uint8_t data);
uint8_t IIC_Write_Bits(uint8_t dev,uint8_t reg,uint8_t bitstart,uint8_t length,uint8_t data);
uint8_t IIC_Write_Bit(uint8_t dev,uint8_t reg,uint8_t bitnum,uint8_t data);

void IIC2_START(void);
void IIC2_STOP(void);
uint8_t IIC2_Wait_Ack(void);
void IIC2_Ack(void);
void IIC2_NAck(void);
void IIC2_Send_Byte(uint8_t txd);
uint8_t IIC2_Read_Byte(uint8_t Ack);
uint8_t IIC2_Read_OneByte(unsigned char IIC_Addr, unsigned char addr);
uint8_t IIC2_Read_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data);
uint8_t IIC2_Write_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data);
uint8_t IIC2_Creat_Byte(uint8_t dev,uint8_t reg,uint8_t *data);
uint8_t IIC2_Write_Byte(uint8_t dev,uint8_t reg,uint8_t data);
uint8_t IIC2_Write_Bits(uint8_t dev,uint8_t reg,uint8_t bitstart,uint8_t length,uint8_t data);
uint8_t IIC2_Write_Bit(uint8_t dev,uint8_t reg,uint8_t bitnum,uint8_t data);

#endif
