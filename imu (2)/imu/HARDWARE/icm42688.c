#include "fonts.h"

static float accSensitivity = 0.244f;       //加速度的最小分辨率 mg/LSB
static float gyroSensitivity = 32.8f;       //陀螺仪的最小分辨率
uint8_t temp = 0;
float ypr[3];
//ICM42688使用的ms级延时函�?
#define ICM42688DelayMs(_nms) Delay_ms(_nms)

/****************************************************************
函数原型 static uint8_t icm42688_read_reg(uint8_t reg)
�?   �? 读取ICM42688指定寄存器的�?
�?   �? reg 指定寄存器地址
�?�?�? 读取到的寄存器地址
*****************************************************************/
static uint8_t icm42688_read_reg(uint8_t reg){
    uint8_t regval = 0;
    IIC_Read_Bytes(ICM42688_ADDRESS,reg,1,&regval);
    return regval;
}

/****************************************************************
函数原型 static uint8_t icm42688_read_regs(uint8_t reg,uint8_t *buf,uint16_t len)
�?   �? 读取ICM42688指定寄存器的length个�?
�?   �? reg 起始寄存器地址
          *buf 数据指针
          len 寄存器个�?
�?�?�? �?
*****************************************************************/
static uint8_t icm42688_read_regs(uint8_t reg,uint8_t *buf,uint16_t len){
    IIC_Read_Bytes(ICM42688_ADDRESS, reg, len, buf);
    return 0;
}

/****************************************************************
函数原型 static uint8_t icm42688_write_reg(uint8_t reg,uint8_t value)
�?   �? 向ICM42688指定寄存器写数据
�?   �? reg 指定寄存器地址
          value 写入数据
�?�?�? 0
*****************************************************************/
static uint8_t icm42688_write_reg(uint8_t reg,uint8_t value){
    IIC_Write_Bytes(ICM42688_ADDRESS,reg,1,&value);
    return 0;
}

float bsp_Icm42688GetAres(uint8_t Ascale)
{
    switch(Ascale)
    {
    // Possible accelerometer scales (and their register bit settings) are:
    // 2 Gs (11), 4 Gs (10), 8 Gs (01), and 16 Gs  (00).
    case AFS_2G:
        accSensitivity = 2000 / 32768.0f;
        break;
    case AFS_4G:
        accSensitivity = 4000 / 32768.0f;
        break;
    case AFS_8G:
        accSensitivity = 8000 / 32768.0f;
        break;
    case AFS_16G:
        accSensitivity = 16000 / 32768.0f;
        break;
    }

    return accSensitivity;
}

float bsp_Icm42688GetGres(uint8_t Gscale)
{
    switch(Gscale)
    {
    case GFS_15_125DPS:
        gyroSensitivity = 15.125f / 32768.0f;
        break;
    case GFS_31_25DPS:
        gyroSensitivity = 31.25f / 32768.0f;
        break;
    case GFS_62_5DPS:
        gyroSensitivity = 62.5f / 32768.0f;
        break;
    case GFS_125DPS:
        gyroSensitivity = 125.0f / 32768.0f;
        break;
    case GFS_250DPS:
        gyroSensitivity = 250.0f / 32768.0f;
        break;
    case GFS_500DPS:
        gyroSensitivity = 500.0f / 32768.0f;
        break;
    case GFS_1000DPS:
        gyroSensitivity = 1000.0f / 32768.0f;
        break;
    case GFS_2000DPS:
        gyroSensitivity = 2000.0f / 32768.0f;
        break;
    }
    return gyroSensitivity;
}

/****************************************************************
函数原型 int8_t bsp_Icm42688RegCfg(void)
�?   �? ICM42688寄存器配�?
�?   �? �?
�?�?�? 正确设备 0
          错误设备 -1
*****************************************************************/
int8_t bsp_Icm42688RegCfg(void){
    uint8_t reg_val = 0;
    /* 读取 who am i 寄存�?*/
    reg_val = icm42688_read_reg(ICM42688_WHO_AM_I);
    //printf("reg_val:%d\n",reg_val);
    icm42688_write_reg(ICM42688_REG_BANK_SEL, 0); //设置bank 0区域寄存�?
    icm42688_write_reg(ICM42688_DEVICE_CONFIG, 0x01); //软复位传感器
    ICM42688DelayMs(100);
    if(reg_val == ICM42688_ID){
        bsp_Icm42688GetAres(AFS_4G);
        icm42688_write_reg(ICM42688_REG_BANK_SEL, 0x00);
        //reg_val = icm42688_read_reg(ICM42688_ACCEL_CONFIG0);//page74
        reg_val = (AFS_4G << 5);   //量程 ±2g
        reg_val |= (AODR_100Hz);     //输出速率 100HZ
        icm42688_write_reg(ICM42688_ACCEL_CONFIG0, reg_val);

        bsp_Icm42688GetGres(GFS_1000DPS);
        icm42688_write_reg(ICM42688_REG_BANK_SEL, 0x00);
        //reg_val = icm42688_read_reg(ICM42688_GYRO_CONFIG0);//page73
        reg_val = (GFS_1000DPS << 5);   //量程 ±1000dps
        reg_val |= (GODR_100Hz);     //输出速率 100HZ
        icm42688_write_reg(ICM42688_GYRO_CONFIG0, reg_val);

        icm42688_write_reg(ICM42688_REG_BANK_SEL, 0x00);
        reg_val = icm42688_read_reg(ICM42688_PWR_MGMT0); //读取PWR—MGMT0当前寄存器的�?page72)
        reg_val &= ~(1 << 5);//使能温度测量
        reg_val |= ((3) << 2);//设置GYRO_MODE  0:关闭 1:待机 2:预留 3:低噪�?
        reg_val |= (3);//设置ACCEL_MODE 0:关闭 1:关闭 2:低功�?3:低噪�?
        icm42688_write_reg(ICM42688_PWR_MGMT0, reg_val);
        ICM42688DelayMs(1); //操作完PWR—MGMT0寄存器后 200us内不能有任何读写寄存器的操作

        return 0;
    }
    return -1;
}

/****************************************************************
函数原型 int8_t bsp_Icm42688Init(void)
�?   �? ICM42688初始�?
�?   �? �?
�?�?�? 正确设备 0
          错误设备 -1
*****************************************************************/
int8_t bsp_Icm42688Init(void){
    return (bsp_Icm42688RegCfg());
}

/****************************************************************
函数原型 int8_t bsp_IcmGetTemperature(int16_t* pTemp)
�?   �? 读取ICM42688内部传感器温�?
�?   �? 温度数据指针
�?�?�? 0
*****************************************************************/
int8_t bsp_IcmGetTemperature(int16_t* pTemp){
    uint8_t buffer[2] = {0};

    icm42688_read_regs(ICM42688_TEMP_DATA1, buffer, 2);

    *pTemp = (int16_t)(((int16_t)((buffer[0] << 8) | buffer[1])) / 132.48 + 25);
    return 0;
}

    uint8_t buffer[12] = {0};
/*******************************************************************************
�?   称： bsp_IcmGetRawData(icm42688RealData_t* accData, icm42688RealData_t* GyroData)
�?   能： 读取Icm42688加速度陀螺仪数据
�?   数： 三轴加速度，三轴角速度
�?�?值： 0
*******************************************************************************/
int8_t bsp_IcmGetRawData(icm42688RealData_t* accData, icm42688RealData_t* GyroData)
{

	icm42688RawData_t accRaw;
	icm42688RawData_t gyroRaw;

    icm42688_read_regs(ICM42688_ACCEL_DATA_X1, buffer, 12);

    accRaw.x  = ((uint16_t)buffer[0] << 8)  | buffer[1];
    accRaw.y  = ((uint16_t)buffer[2] << 8)  | buffer[3];
    accRaw.z  = ((uint16_t)buffer[4] << 8)  | buffer[5];
    gyroRaw.x = ((uint16_t)buffer[6] << 8)  | buffer[7];
    gyroRaw.y = ((uint16_t)buffer[8] << 8)  | buffer[9];
    gyroRaw.z = ((uint16_t)buffer[10] << 8) | buffer[11];


    accData->x = (float)(accRaw.x * accSensitivity);
    accData->y = (float)(accRaw.y * accSensitivity);
    accData->z = (float)(accRaw.z * accSensitivity);

    GyroData->x = (float)(gyroRaw.x * gyroSensitivity);
    GyroData->y = (float)(gyroRaw.y * gyroSensitivity);
    GyroData->z = (float)(gyroRaw.z * gyroSensitivity);

    return 0;
}

void icm42688Task(void)
{
//    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,GPIO_PIN_SET);
    IMU_getYawPitchRoll(ypr);
//    printf("yaw:%.2f pitch:%.2f roll:%.2f\r\n", ypr[0], ypr[1], ypr[2]);
//    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,GPIO_PIN_RESET);
}


/* Send task: outputs pose data and bio data. */
void printTask(void)
{
    static uint32_t seq = 0;

    int32_t heart_rate = -999;
    int32_t spo2 = -999;
    int8_t hr_valid = 0;
    int8_t spo2_valid = 0;
    uint16_t ppg_fill = 0U;
    uint32_t ppg_calc_count = 0U;
    uint8_t ppg_pending = 0U;

#if ENABLE_PPG_SENSOR
    max30102_get_latest_result(&heart_rate, &hr_valid, &spo2, &spo2_valid);
    max30102_get_debug_state(&ppg_fill,
                             &ppg_calc_count,
                             &ppg_pending,
                             NULL,
                             NULL,
                             NULL);
#endif

    printf("%u,%lu,%.2f,%.2f,%.2f,%ld,%d,%ld,%d,%u,%lu,%u\r\n",
           (unsigned int)SENSOR_ID,
           (unsigned long)seq++,
           ypr[0],
           ypr[1],
           ypr[2],
           (long)heart_rate,
           (int)hr_valid,
           (long)spo2,
           (int)spo2_valid,
           (unsigned int)ppg_fill,
           (unsigned long)ppg_calc_count,
           (unsigned int)ppg_pending);
}
