#include "fonts.h"

#define N_IR_BUFFER_LENGTH 500U
#define MAX30102_WINDOW_STEP 100U
#define MAX30102_LED_CURRENT_DEFAULT 0x24U
#define MAX30102_LED_CURRENT_MIN 0x08U
#define MAX30102_LED_CURRENT_MAX 0x50U
#define MAX30102_LED_CURRENT_STEP 0x02U
#define MAX30102_AGC_INTERVAL_SAMPLES 25U
#define MAX30102_IR_TARGET_LOW 40000UL
#define MAX30102_IR_TARGET_HIGH 120000UL

uint32_t aun_red_buffer[N_IR_BUFFER_LENGTH];
uint32_t aun_ir_buffer[N_IR_BUFFER_LENGTH];

static uint16_t spo2_fill = 0;
static uint8_t spo2_warmup_done = 0;
static uint8_t spo2_calc_req = 0;
static uint8_t spo2_started = 0;

static int32_t g_last_hr = -999;
static int32_t g_last_spo2 = -999;
static int8_t g_last_hr_valid = 0;
static int8_t g_last_spo2_valid = 0;
static uint32_t g_calc_count = 0U;
static uint8_t g_last_pending = 0U;
static uint8_t g_part_id = 0U;
static uint8_t g_rev_id = 0U;
static uint8_t g_led_current = MAX30102_LED_CURRENT_DEFAULT;
static uint32_t g_agc_ir_accum = 0U;
static uint16_t g_agc_sample_count = 0U;

static void max30102_apply_led_current(uint8_t led_current)
{
	if (led_current < MAX30102_LED_CURRENT_MIN)
	{
		led_current = MAX30102_LED_CURRENT_MIN;
	}
	else if (led_current > MAX30102_LED_CURRENT_MAX)
	{
		led_current = MAX30102_LED_CURRENT_MAX;
	}

	g_led_current = led_current;
	max30102_Bus_Write(REG_LED1_PA, g_led_current);
	max30102_Bus_Write(REG_LED2_PA, g_led_current);
}

static void max30102_reset_runtime_state(void)
{
	uint16_t i;

	spo2_fill = 0;
	spo2_warmup_done = 0;
	spo2_calc_req = 0;
	spo2_started = 0;
	g_calc_count = 0U;
	g_last_pending = 0U;
	g_led_current = MAX30102_LED_CURRENT_DEFAULT;
	g_agc_ir_accum = 0U;
	g_agc_sample_count = 0U;

	g_last_hr = -999;
	g_last_spo2 = -999;
	g_last_hr_valid = 0;
	g_last_spo2_valid = 0;

	for (i = 0; i < N_IR_BUFFER_LENGTH; i++)
	{
		aun_red_buffer[i] = 0;
		aun_ir_buffer[i] = 0;
	}
}

static void max30102_clear_interrupt_status(void)
{
	(void)max30102_Bus_Read(REG_INTR_STATUS_1);
	(void)max30102_Bus_Read(REG_INTR_STATUS_2);
}

static uint8_t max30102_fifo_pending(void)
{
	uint8_t wr = max30102_Bus_Read(REG_FIFO_WR_PTR) & 0x1F;
	uint8_t rd = max30102_Bus_Read(REG_FIFO_RD_PTR) & 0x1F;

	g_last_pending = (uint8_t)((wr - rd) & 0x1F);
	return g_last_pending;
}

static uint32_t max30102_compose_sample(const uint8_t *data)
{
	return ((uint32_t)(data[0] & 0x03U) << 16) |
		   ((uint32_t)data[1] << 8) |
		   (uint32_t)data[2];
}

static void max30102_agc_update(uint32_t ir_sum, uint8_t sample_count)
{
	uint32_t avg_ir;

	if (sample_count == 0U)
	{
		return;
	}

	g_agc_ir_accum += ir_sum;
	g_agc_sample_count = (uint16_t)(g_agc_sample_count + sample_count);
	if (g_agc_sample_count < MAX30102_AGC_INTERVAL_SAMPLES)
	{
		return;
	}

	avg_ir = g_agc_ir_accum / (uint32_t)g_agc_sample_count;
	if ((avg_ir < MAX30102_IR_TARGET_LOW) &&
		(g_led_current < MAX30102_LED_CURRENT_MAX))
	{
		max30102_apply_led_current((uint8_t)(g_led_current + MAX30102_LED_CURRENT_STEP));
	}
	else if ((avg_ir > MAX30102_IR_TARGET_HIGH) &&
			 (g_led_current > MAX30102_LED_CURRENT_MIN))
	{
		max30102_apply_led_current((uint8_t)(g_led_current - MAX30102_LED_CURRENT_STEP));
	}

	g_agc_ir_accum = 0U;
	g_agc_sample_count = 0U;
}
uint8_t max30102_Bus_Write(uint8_t Register_Address, uint8_t Word_Data)
{

	/* 采用串行EEPROM随即读取指令序列，连续读取若干字�?*/

	/* �?步：发起I2C总线启动信号 */
	IIC2_START();

	/* �?步：发起控制字节，高7bit是地址，bit0是读写控制位�?表示写，1表示�?*/
	IIC2_Send_Byte(max30102_WR_address | I2C_WR);	/* 此处是写指令 */

	/* �?步：发送ACK */
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}

	/* �?步：发送字节地址 */
	IIC2_Send_Byte(Register_Address);
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}
	
	/* �?步：开始写入数�?*/
	IIC2_Send_Byte(Word_Data);

	/* �?步：发送ACK */
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}

	/* 发送I2C总线停止信号 */
	IIC2_STOP();
	return 1;	/* 执行成功 */

cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设�?*/
	/* 发送I2C总线停止信号 */
	IIC2_STOP();
	return 0;
}
uint8_t max30102_Bus_Read(uint8_t Register_Address)
{
	uint8_t  data;


	/* �?步：发起I2C总线启动信号 */
	IIC2_START();

	/* �?步：发起控制字节，高7bit是地址，bit0是读写控制位�?表示写，1表示�?*/
	IIC2_Send_Byte(max30102_WR_address | I2C_WR);	/* 此处是写指令 */

	/* �?步：发送ACK */
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}

	/* �?步：发送字节地址�?*/
	IIC2_Send_Byte((uint8_t)Register_Address);
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}
	

	/* �?步：重新启动I2C总线。下面开始读取数�?*/
	IIC2_START();

	/* �?步：发起控制字节，高7bit是地址，bit0是读写控制位�?表示写，1表示�?*/
	IIC2_Send_Byte(max30102_WR_address | I2C_RD);	/* 此处是读指令 */

	/* �?步：发送ACK */
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}

	/* �?步：读取数据 */
	{
		data = IIC2_Read_Byte(0);	/* �?个字�?*/
	}
	/* 发送I2C总线停止信号 */
	IIC2_STOP();
	return data;	/* 执行成功 返回data�?*/

cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设�?*/
	/* 发送I2C总线停止信号 */
	IIC2_STOP();
	return 0;
}


void max30102_FIFO_ReadWords(uint8_t Register_Address,uint16_t Word_Data[][2],uint8_t count)
{
	uint8_t i=0;
	uint8_t no = count;
	uint8_t data1, data2;
	/* �?步：发起I2C总线启动信号 */
	IIC2_START();

	/* �?步：发起控制字节，高7bit是地址，bit0是读写控制位�?表示写，1表示�?*/
	IIC2_Send_Byte(max30102_WR_address | I2C_WR);	/* 此处是写指令 */

	/* �?步：发送ACK */
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}

	/* �?步：发送字节地址�?*/
	IIC2_Send_Byte((uint8_t)Register_Address);
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}
	

	/* �?步：重新启动I2C总线。下面开始读取数�?*/
	IIC2_START();

	/* �?步：发起控制字节，高7bit是地址，bit0是读写控制位�?表示写，1表示�?*/
	IIC2_Send_Byte(max30102_WR_address | I2C_RD);	/* 此处是读指令 */

	/* �?步：发送ACK */
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}

	/* �?步：读取数据 */
	while (no)
	{
		data1 = IIC2_Read_Byte(0);	
		IIC2_Ack();
		data2 = IIC2_Read_Byte(0);
		IIC2_Ack();
		Word_Data[i][0] = (((uint16_t)data1 << 8) | data2);  //

		
		data1 = IIC2_Read_Byte(0);	
		IIC2_Ack();
		data2 = IIC2_Read_Byte(0);
		if(1==no)
			IIC2_NAck();	/* 最�?个字节读完后，CPU产生NACK信号(驱动SDA = 1) */
		else
			IIC2_Ack();
		Word_Data[i][1] = (((uint16_t)data1 << 8) | data2); 

		no--;	
		i++;
	}
	/* 发送I2C总线停止信号 */
	IIC2_STOP();

cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设�?*/
	/* 发送I2C总线停止信号 */
	IIC2_STOP();
}

void max30102_FIFO_ReadBytes(uint8_t Register_Address,uint8_t* Data)
{	
/* �?步：发起I2C总线启动信号 */
	IIC2_START();

	/* �?步：发起控制字节，高7bit是地址，bit0是读写控制位�?表示写，1表示�?*/
	IIC2_Send_Byte(max30102_WR_address | I2C_WR);	/* 此处是写指令 */

	/* �?步：发送ACK */
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}

	/* �?步：发送字节地址�?*/
	IIC2_Send_Byte((uint8_t)Register_Address);
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}
	

	/* �?步：重新启动I2C总线。下面开始读取数�?*/
	IIC2_START();

	/* �?步：发起控制字节，高7bit是地址，bit0是读写控制位�?表示写，1表示�?*/
	IIC2_Send_Byte(max30102_WR_address | I2C_RD);	/* 此处是读指令 */

	/* �?步：发送ACK */
	if (IIC2_Wait_Ack() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应�?*/
	}

	/* �?步：读取数据 */
	Data[0] = IIC2_Read_Byte(1);	
	Data[1] = IIC2_Read_Byte(1);	
	Data[2] = IIC2_Read_Byte(1);	
	Data[3] = IIC2_Read_Byte(1);
	Data[4] = IIC2_Read_Byte(1);	
	Data[5] = IIC2_Read_Byte(0);
	/* 最�?个字节读完后，CPU产生NACK信号(驱动SDA = 1) */
	/* 发送I2C总线停止信号 */
	IIC2_STOP();
	return;

cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设�?*/
	/* 发送I2C总线停止信号 */
	IIC2_STOP();

//	u8 i;
//	u8 fifo_wr_ptr;
//	u8 firo_rd_ptr;
//	u8 number_tp_read;
//	//Get the FIFO_WR_PTR
//	fifo_wr_ptr = max30102_Bus_Read(REG_FIFO_WR_PTR);
//	//Get the FIFO_RD_PTR
//	firo_rd_ptr = max30102_Bus_Read(REG_FIFO_RD_PTR);
//	
//	number_tp_read = fifo_wr_ptr - firo_rd_ptr;
//	
//	//for(i=0;i<number_tp_read;i++){
//	if(number_tp_read>0){
//		IIC_ReadBytes(max30102_WR_address,REG_FIFO_DATA,Data,6);
//	}
	
	//max30102_Bus_Write(REG_FIFO_RD_PTR,fifo_wr_ptr);
}

void max30102_init(void)
{
 	__HAL_RCC_GPIOB_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.Pin  = MAX30102_INT_PIN;
	GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.Pull = GPIO_PULLUP;
 	HAL_GPIO_Init(MAX30102_INT_PORT, &GPIO_InitStructure);

	max30102_reset_runtime_state();
	
	max30102_reset();
	HAL_Delay(20);
	g_part_id = max30102_Bus_Read(REG_PART_ID);
	g_rev_id = max30102_Bus_Read(REG_REV_ID);
	
//	max30102_Bus_Write(REG_MODE_CONFIG, 0x0b);  //mode configuration : temp_en[3]      MODE[2:0]=010 HR only enabled    011 SP02 enabled
//	max30102_Bus_Write(REG_INTR_STATUS_2, 0xF0); //open all of interrupt
//	max30102_Bus_Write(REG_INTR_STATUS_1, 0x00); //all interrupt clear
//	max30102_Bus_Write(REG_INTR_ENABLE_2, 0x02); //DIE_TEMP_RDY_EN
//	max30102_Bus_Write(REG_TEMP_CONFIG, 0x01); //SET   TEMP_EN

//	max30102_Bus_Write(REG_SPO2_CONFIG, 0x47); //SPO2_SR[4:2]=001  100 per second    LED_PW[1:0]=11  16BITS

//	max30102_Bus_Write(REG_LED1_PA, 0x47); 
//	max30102_Bus_Write(REG_LED2_PA, 0x47); 
	
	
	
	max30102_Bus_Write(REG_INTR_ENABLE_1,0xc0);	// INTR setting
	max30102_Bus_Write(REG_INTR_ENABLE_2,0x00);
	max30102_Bus_Write(REG_FIFO_WR_PTR,0x00);  	//FIFO_WR_PTR[4:0]
	max30102_Bus_Write(REG_OVF_COUNTER,0x00);  	//OVF_COUNTER[4:0]
	max30102_Bus_Write(REG_FIFO_RD_PTR,0x00);  	//FIFO_RD_PTR[4:0]
	max30102_Bus_Write(REG_FIFO_CONFIG,0x0f);  	//sample avg = 1, fifo rollover=false, fifo almost full = 17
	max30102_Bus_Write(REG_MODE_CONFIG,0x03);  	//0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
	max30102_Bus_Write(REG_SPO2_CONFIG,0x27);  	// SPO2_ADC range = 4096nA, SPO2 sample rate (100 Hz), LED pulseWidth (400uS)  
	max30102_apply_led_current(g_led_current);
	max30102_Bus_Write(REG_PILOT_PA,0x7f);   	// Choose value for ~ 25mA for Pilot LED


	
//	// Interrupt Enable 1 Register. Set PPG_RDY_EN (data available in FIFO)
//	max30102_Bus_Write(0x2, 1<<6);

//	// FIFO configuration register
//	// SMP_AVE: 16 samples averaged per FIFO sample
//	// FIFO_ROLLOVER_EN=1
//	//max30102_Bus_Write(0x8,  1<<4);
//	max30102_Bus_Write(0x8, (0<<5) | 1<<4);

//	// Mode Configuration Register
//	// SPO2 mode
//	max30102_Bus_Write(0x9, 3);

//	// SPO2 Configuration Register
//	max30102_Bus_Write(0xa,
//			(3<<5)  // SPO2_ADC_RGE 2 = full scale 8192 nA (LSB size 31.25pA); 3 = 16384nA
//			| (1<<2) // sample rate: 0 = 50sps; 1 = 100sps; 2 = 200sps
//			| (3<<0) // LED_PW 3 = 411μs, ADC resolution 18 bits
//	);

//	// LED1 (red) power (0 = 0mA; 255 = 50mA)
//	max30102_Bus_Write(0xc, 0xb0);

//	// LED (IR) power
//	max30102_Bus_Write(0xd, 0xa0);

	max30102_clear_interrupt_status();
											
}

void max30102_reset(void)
{
	max30102_Bus_Write(REG_MODE_CONFIG,0x40);
	HAL_Delay(10);
}

void max30102_start(void)
{
	max30102_reset_runtime_state();
	max30102_Bus_Write(REG_FIFO_WR_PTR,0x00);
	max30102_Bus_Write(REG_OVF_COUNTER,0x00);
	max30102_Bus_Write(REG_FIFO_RD_PTR,0x00);
	max30102_apply_led_current(g_led_current);
	max30102_clear_interrupt_status();
	spo2_started = 1;
}






void maxim_max30102_write_reg(uint8_t uch_addr, uint8_t uch_data)
{
//  char ach_i2c_data[2];
//  ach_i2c_data[0]=uch_addr;
//  ach_i2c_data[1]=uch_data;
//	
//  IIC_WriteBytes(I2C_WRITE_ADDR, ach_i2c_data, 2);
	IIC2_Write_Byte(I2C_WRITE_ADDR, uch_addr, uch_data);
}

void maxim_max30102_read_reg(uint8_t uch_addr, uint8_t *puch_data)
{
//  char ch_i2c_data;
//  ch_i2c_data=uch_addr;
//  IIC_WriteBytes(I2C_WRITE_ADDR, &ch_i2c_data, 1);
//	
//  i2c.read(I2C_READ_ADDR, &ch_i2c_data, 1);
//  
//   *puch_data=(uint8_t) ch_i2c_data;
	*puch_data = IIC2_Read_OneByte(I2C_WRITE_ADDR,uch_addr);
}

void maxim_max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led)
{
	uint32_t un_temp;
	char ach_i2c_data[6];
	*pun_red_led=0;
	*pun_ir_led=0;
  
  IIC2_Read_Bytes(I2C_WRITE_ADDR,REG_FIFO_DATA,6,(uint8_t *)ach_i2c_data);
  
  un_temp=(unsigned char) ach_i2c_data[0];
  un_temp<<=16;
  *pun_red_led+=un_temp;
  un_temp=(unsigned char) ach_i2c_data[1];
  un_temp<<=8;
  *pun_red_led+=un_temp;
  un_temp=(unsigned char) ach_i2c_data[2];
  *pun_red_led+=un_temp;
  
  un_temp=(unsigned char) ach_i2c_data[3];
  un_temp<<=16;
  *pun_ir_led+=un_temp;
  un_temp=(unsigned char) ach_i2c_data[4];
  un_temp<<=8;
  *pun_ir_led+=un_temp;
  un_temp=(unsigned char) ach_i2c_data[5];
  *pun_ir_led+=un_temp;
  *pun_red_led&=0x03FFFF;  //Mask MSB [23:18]
  *pun_ir_led&=0x03FFFF;  //Mask MSB [23:18]
}

void max30102_InitAndCollectBaseData(void)
{
    max30102_init();
    max30102_start();
}

void max30102SampleTask(void)
{
    uint8_t temp[6];
    uint8_t pending;
    uint8_t force_read;
    uint8_t drain_count;
    uint8_t burst_count;
    uint8_t i;
    uint16_t remaining_slots;
    uint32_t ir_sample;
    uint32_t ir_sum = 0U;

    if (!spo2_started || spo2_calc_req)
    {
        return;
    }

    force_read = (MAX30102_INT == GPIO_PIN_RESET) ? 1U : 0U;
    pending = max30102_fifo_pending();
    if ((pending == 0U) && (force_read == 0U))
    {
        return;
    }

    remaining_slots = (uint16_t)(N_IR_BUFFER_LENGTH - spo2_fill);
    if (remaining_slots == 0U)
    {
        spo2_calc_req = 1;
        return;
    }

    drain_count = 0U;

    if (force_read != 0U)
    {
        max30102_FIFO_ReadBytes(REG_FIFO_DATA, temp);
        aun_red_buffer[spo2_fill] = max30102_compose_sample(temp);
        ir_sample = max30102_compose_sample(&temp[3]);
        aun_ir_buffer[spo2_fill] = ir_sample;
        ir_sum += ir_sample;
        spo2_fill++;
        drain_count = 1U;
        remaining_slots--;
    }

    if (remaining_slots > 0U)
    {
        pending = max30102_fifo_pending();
        burst_count = pending;
        if (remaining_slots < (uint16_t)burst_count)
        {
            burst_count = (uint8_t)remaining_slots;
        }

        for (i = 0U; i < burst_count; i++)
        {
            max30102_FIFO_ReadBytes(REG_FIFO_DATA, temp);
            aun_red_buffer[spo2_fill] = max30102_compose_sample(temp);
            ir_sample = max30102_compose_sample(&temp[3]);
            aun_ir_buffer[spo2_fill] = ir_sample;
            ir_sum += ir_sample;
            spo2_fill++;
        }

        drain_count = (uint8_t)(drain_count + burst_count);
    }

    if (drain_count == 0U)
    {
        max30102_clear_interrupt_status();
        return;
    }

    max30102_agc_update(ir_sum, drain_count);
    pending = max30102_fifo_pending();

    if (pending == 0U)
    {
        max30102_clear_interrupt_status();
    }

    if (spo2_fill >= N_IR_BUFFER_LENGTH)
    {
        spo2_calc_req = 1;
    }
}
void max30102CalcTask(void)
{
    uint16_t i;

    if (!spo2_calc_req)
    {
        return;
    }

    maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer,
                                           N_IR_BUFFER_LENGTH,
                                           aun_red_buffer,
                                           &g_last_spo2,
                                           &g_last_spo2_valid,
                                           &g_last_hr,
                                           &g_last_hr_valid);

    spo2_warmup_done = 1;
    g_calc_count++;

    for (i = MAX30102_WINDOW_STEP; i < N_IR_BUFFER_LENGTH; i++)
    {
        aun_red_buffer[i - MAX30102_WINDOW_STEP] = aun_red_buffer[i];
        aun_ir_buffer[i - MAX30102_WINDOW_STEP] = aun_ir_buffer[i];
    }

    spo2_fill = N_IR_BUFFER_LENGTH - MAX30102_WINDOW_STEP;
    spo2_calc_req = 0;
}

void Max30102Task(void)
{
    max30102SampleTask();
    max30102CalcTask();
}

void max30102_get_latest_result(int32_t *pn_heart_rate,
                                int8_t *pch_hr_valid,
                                int32_t *pn_spo2,
                                int8_t *pch_spo2_valid)
{
    if (pn_heart_rate != NULL)
    {
        *pn_heart_rate = g_last_hr;
    }

    if (pch_hr_valid != NULL)
    {
        *pch_hr_valid = g_last_hr_valid;
    }

    if (pn_spo2 != NULL)
    {
        *pn_spo2 = g_last_spo2;
    }

    if (pch_spo2_valid != NULL)
    {
        *pch_spo2_valid = g_last_spo2_valid;
    }
}

void max30102_get_debug_state(uint16_t *p_fill,
                               uint32_t *p_calc_count,
                               uint8_t *p_pending,
                               uint8_t *p_part_id,
                               uint8_t *p_rev_id,
                               uint8_t *p_int_level)
{
    if (p_fill != NULL)
    {
        *p_fill = spo2_fill;
    }

    if (p_calc_count != NULL)
    {
        *p_calc_count = g_calc_count;
    }

    if (p_pending != NULL)
    {
        *p_pending = g_last_pending;
    }

    if (p_part_id != NULL)
    {
        *p_part_id = g_part_id;
    }

    if (p_rev_id != NULL)
    {
        *p_rev_id = g_rev_id;
    }

    if (p_int_level != NULL)
    {
        *p_int_level = (MAX30102_INT == GPIO_PIN_RESET) ? 0U : 1U;
    }
}

uint8_t max30102_is_ready(void)
{
    return spo2_warmup_done;
}

