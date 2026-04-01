#include "fonts.h"


/****************************************************************
函数原型 void IIC_START(void)
功    能  产生起始信号
*****************************************************************/
void IIC_START(void){
    SDA_OUT;
    SCL_HIGH;
    SDA_HIGH;
    Delay_us(4);
    SDA_LOW;
    Delay_us(4);
    SCL_LOW;
}

/****************************************************************
函数原型 void IIC_STOP(void)
功    能  产生结束信号
*****************************************************************/
void IIC_STOP(void){
    SCL_LOW;
    SDA_LOW;
    Delay_us(4);
    SCL_HIGH;
    SDA_HIGH;
    Delay_us(4);
}

/****************************************************************
函数原型 void IIC_Wait_Ack(void)
功    能  等待应答信号到来
返回值  1 接收应答失败
        0 接收应答成功
*****************************************************************/
uint8_t IIC_Wait_Ack(void){
    uint8_t ucErrTime=0;     //IIC等待应答超时时间
    SDA_IN;                  //SDA输入模式
    SCL_HIGH; Delay_us(1);
    SDA_HIGH; Delay_us(1);   //主机释放SDA，如果从机拉低SDA说明应答，反之非应答
    while(READ_SDA){
        ucErrTime++;
        if(ucErrTime>100)
        {
            IIC_STOP();
            return 1;
        }
        Delay_us(1);
    }
    SCL_LOW;                //主机拉低SCL，接收应答结束
    return 0;
}

/****************************************************************
函数原型 void IIC__Ack(void)
功    能  产生Ack应答
*****************************************************************/
void IIC_Ack(void){
    SCL_LOW;
    SDA_OUT;
    SDA_LOW;                //主机SCL低电平时，可变化SDA，发送应答
    Delay_us(2);            //等待SDA信号稳定
    SCL_HIGH;               //SCL释放时从机读取SDA
    Delay_us(2);            //确保从机有时间读取SDA
    SCL_LOW;               //发送Ack结束
}

/****************************************************************
函数原型 void IIC__NAck(void)
功    能  产生NAck应答
*****************************************************************/
void IIC_NAck(void){
    SCL_LOW;
    SDA_OUT;
    SDA_HIGH;
    Delay_us(2);
    SCL_HIGH;
    Delay_us(2);
    SCL_LOW;
}

/****************************************************************
函数原型 void IIC_Send_Byte(uint8_t txd)
功    能  发送一个字节
参    数  txd 字节
*****************************************************************/
void IIC_Send_Byte(uint8_t txd){
    uint8_t t;
    SCL_LOW;
    SDA_OUT;
    for(t=0;t<8;t++){      //高位先行
        if((0X80&txd)>>7) SDA_HIGH;
        else SDA_LOW;
        txd<<=1;
        Delay_us(2);
        SCL_HIGH;
        Delay_us(2);
        SCL_LOW;
        Delay_us(2);
    }
}

/****************************************************************
函数原型 uint8_t IIC_Read_Byte(unsigned char Ack)
功    能  接收一个字节
参    数  Ack 1 应答
              0 非应答
*****************************************************************/
uint8_t IIC_Read_Byte(uint8_t Ack){
    uint8_t t,received=0;
    SDA_IN;
//    SCL_HIGH;
//    Delay_us(2);
    for(t=0;t<8;t++){
        SCL_LOW;            //低电平时从机准备SDA的值
        Delay_us(2);
        SCL_HIGH;
        received<<=1;       //左移一位，为最低位空出位置
        
        if(READ_SDA)
            received++;    //最低位置1
        Delay_us(2);
    }
    if(Ack) 
        IIC_Ack();
    else 
        IIC_NAck();
    return received;
}

/****************************************************************
函数原型 uint8_t IIC_Read_OneByte(unsigned char IIC_Addr,unsigned char addr)
功    能  读取指定设备 指定寄存器的某个值
参    数  IIC_addr 指定设备地址
          addr 指定寄存器地址
*****************************************************************/
uint8_t IIC_Read_OneByte(unsigned char IIC_Addr, unsigned char addr){
    unsigned char res;
    IIC_START();
    IIC_Send_Byte(IIC_Addr);
    res++;
    IIC_Wait_Ack();
    IIC_Send_Byte(addr);
    res++;
    IIC_Wait_Ack();
    
    IIC_START();                //重复启动，用于读写模式切换
    IIC_Send_Byte(IIC_Addr+1);  
    res++;
    IIC_Wait_Ack();
    res = IIC_Read_Byte(1);     //读完一个字节就结束
    IIC_STOP();
    return res;                 //返回读取到的值
}

/****************************************************************
函数原型 uint8_t IIC_Read_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data)
功    能  读取指定设备 指定寄存器的length个值
参    数  dev 指定设备地址
          reg 指定寄存器地址
          length 要读的字节数
          *data 读出的数据存放的指针
返回值    读出来的字节数量
*****************************************************************/
uint8_t IIC_Read_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data){
    uint8_t count = 0;
    uint8_t temp;
    IIC_START();
    IIC_Send_Byte(dev);
    IIC_Wait_Ack();
    IIC_Send_Byte(reg);
    IIC_Wait_Ack();
    IIC_START();
	IIC_Send_Byte(dev+1);  //进入接收模式	
	IIC_Wait_Ack();
    for(count=0;count<length;count++){
        if(count!=(length-1))
            temp = IIC_Read_Byte(1);
        else
            temp = IIC_Read_Byte(0);         //最后一次不应答
        
        data[count] = temp;
    }
    IIC_STOP();
    return count;
}

/****************************************************************
函数原型 uint8_t IIC_Write_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data)
功    能  写入指定设备 指定寄存器的length个值
参    数  dev 指定设备地址
          reg 指定寄存器地址
          length 要写的字节数
          *data 写入的数据存放的指针
返回值    写入的字节数量
*****************************************************************/
uint8_t IIC_Write_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data){
    uint8_t count = 0;
    IIC_START();
    IIC_Send_Byte(dev);
    IIC_Wait_Ack();
    IIC_Send_Byte(reg);
    IIC_Wait_Ack();
    for(count=0;count<length;count++){
        IIC_Send_Byte(data[count]);
        IIC_Wait_Ack();
    }
    IIC_STOP();
    return 1;
}

/****************************************************************
函数原型 uint8_t IIC_Creat_Byte(uint8_t dev,uint8_t reg,uint8_t *data)
功    能  读取指定设备 指定寄存器的一个值
参    数  dev 指定设备地址
          reg 指定寄存器地址
          *data 要读取的字节
返回值    1
*****************************************************************/
uint8_t IIC_Creat_Byte(uint8_t dev,uint8_t reg,uint8_t *data){
    *data = IIC_Read_OneByte(dev,reg);
    return 1;
}

/****************************************************************
函数原型 uint8_t IIC_Write_Byte(uint8_t dev,uint8_t reg,uint8_t data)
功    能  写入指定设备 指定寄存器的一个值
参    数  dev 指定设备地址
          reg 指定寄存器地址
          data 要写入的字节
返回值    1
*****************************************************************/
uint8_t IIC_Write_Byte(uint8_t dev,uint8_t reg,uint8_t data){
    return IIC_Write_Bytes(dev,reg,1,&data);
}

/****************************************************************
函数原型 uint8_t IIC_Write_Bits(uint8_t dev,uint8_t reg,uint8_t bitstart,uint8_t length,uint8_t data)
功    能  修改指定设备 指定寄存器的一个字节的几个位
参    数  dev 指定设备地址
          reg 指定寄存器地址
          bitstart 修改的起始位
          length 修改的位长度
          data 存放要修改的字节的值
返回值    修改成功 1
          失败 0
*****************************************************************/
uint8_t IIC_Write_Bits(uint8_t dev,uint8_t reg,uint8_t bitstart,uint8_t length,uint8_t data){
    uint8_t b;
    if(IIC_Creat_Byte(dev,reg,&b)!=0){
        uint8_t mask = 0xFF << (bitstart+1) | 0xFF >> ((7-bitstart)+length);    //只保留要修改的位为1.其它为0
        data <<= (8 - length);
        data >>= (7 - bitstart);
        b &= mask;
        b |= data;
        return IIC_Write_Byte(dev,reg,b);
    }
    else
        return 0;
}

/****************************************************************
函数原型 uint8_t IIC_Write_Bit(uint8_t dev,uint8_t reg,uint8_t bitnum,uint8_t data)
功    能  修改指定设备 指定寄存器的一个字节的1个位
参    数  dev 指定设备地址
          reg 指定寄存器地址
          bitnum 修改的位地址
          data 存放要修改的字节的值
返回值    修改成功 1
          失败 0
*****************************************************************/
uint8_t IIC_Write_Bit(uint8_t dev,uint8_t reg,uint8_t bitnum,uint8_t data){
    data &= 0x01;                       //确保只传入一位
    return IIC_Write_Bits(dev,reg,bitnum,1,data);
}










/****************************************************************
函数原型 void IIC_START(void)
功    能  产生起始信号
*****************************************************************/
void IIC2_START(void){
    SDA2_OUT;
    SCL2_HIGH;
    SDA2_HIGH;
    Delay_us(4);
    SDA2_LOW;
    Delay_us(4);
    SCL2_LOW;
}

/****************************************************************
函数原型 void IIC_STOP(void)
功    能  产生结束信号
*****************************************************************/
void IIC2_STOP(void){
    SCL2_LOW;
    SDA2_LOW;
    Delay_us(4);
    SCL2_HIGH;
    SDA2_HIGH;
    Delay_us(4);
}

/****************************************************************
函数原型 void IIC_Wait_Ack(void)
功    能  等待应答信号到来
返回值  1 接收应答失败
        0 接收应答成功
*****************************************************************/
uint8_t IIC2_Wait_Ack(void){
    uint8_t ucErrTime=0;     //IIC等待应答超时时间
    SDA2_IN;                  //SDA输入模式
    SCL2_HIGH; Delay_us(1);
    SDA2_HIGH; Delay_us(1);   //主机释放SDA，如果从机拉低SDA说明应答，反之非应答
    while(READ_SDA2){
        ucErrTime++;
        if(ucErrTime>100)
        {
            IIC2_STOP();
            return 1;
        }
        Delay_us(1);
    }
    SCL2_LOW;                //主机拉低SCL，接收应答结束
    return 0;
}

/****************************************************************
函数原型 void IIC__Ack(void)
功    能  产生Ack应答
*****************************************************************/
void IIC2_Ack(void){
    SCL2_LOW;
    SDA2_OUT;
    SDA2_LOW;                //主机SCL低电平时，可变化SDA，发送应答
    Delay_us(2);            //等待SDA信号稳定
    SCL2_HIGH;               //SCL释放时从机读取SDA
    Delay_us(2);            //确保从机有时间读取SDA
    SCL2_LOW;               //发送Ack结束
}

/****************************************************************
函数原型 void IIC__NAck(void)
功    能  产生NAck应答
*****************************************************************/
void IIC2_NAck(void){
    SCL2_LOW;
    SDA2_OUT;
    SDA2_HIGH;
    Delay_us(2);
    SCL2_HIGH;
    Delay_us(2);
    SCL2_LOW;
}

/****************************************************************
函数原型 void IIC_Send_Byte(uint8_t txd)
功    能  发送一个字节
参    数  txd 字节
*****************************************************************/
void IIC2_Send_Byte(uint8_t txd){
    uint8_t t;
    SCL2_LOW;
    SDA2_OUT;
    for(t=0;t<8;t++){      //高位先行
        if((0X80&txd)>>7) SDA2_HIGH;
        else SDA2_LOW;
        txd<<=1;
        Delay_us(2);
        SCL2_HIGH;
        Delay_us(2);
        SCL2_LOW;
        Delay_us(2);
    }
}

/****************************************************************
函数原型 uint8_t IIC_Read_Byte(unsigned char Ack)
功    能  接收一个字节
参    数  Ack 1 应答
              0 非应答
*****************************************************************/
uint8_t IIC2_Read_Byte(uint8_t Ack){
    uint8_t t,received=0;
    SDA2_IN;
//    SCL_HIGH;
//    Delay_us(2);
    for(t=0;t<8;t++){
        SCL2_LOW;            //低电平时从机准备SDA的值
        Delay_us(2);
        SCL2_HIGH;
        received<<=1;       //左移一位，为最低位空出位置
        
        if(READ_SDA2)
            received++;    //最低位置1
        Delay_us(2);
    }
    if(Ack) 
        IIC2_Ack();
    else 
        IIC2_NAck();
    return received;
}

/****************************************************************
函数原型 uint8_t IIC_Read_OneByte(unsigned char IIC_Addr,unsigned char addr)
功    能  读取指定设备 指定寄存器的某个值
参    数  IIC_addr 指定设备地址
          addr 指定寄存器地址
*****************************************************************/
uint8_t IIC2_Read_OneByte(unsigned char IIC_Addr, unsigned char addr){
    unsigned char res;
    IIC2_START();
    IIC2_Send_Byte(IIC_Addr);
    res++;
    IIC2_Wait_Ack();
    IIC2_Send_Byte(addr);
    res++;
    IIC2_Wait_Ack();
    
    IIC2_START();                //重复启动，用于读写模式切换
    IIC2_Send_Byte(IIC_Addr+1);  
    res++;
    IIC2_Wait_Ack();
    res = IIC2_Read_Byte(1);     //读完一个字节就结束
    IIC2_STOP();
    return res;                 //返回读取到的值
}

/****************************************************************
函数原型 uint8_t IIC_Read_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data)
功    能  读取指定设备 指定寄存器的length个值
参    数  dev 指定设备地址
          reg 指定寄存器地址
          length 要读的字节数
          *data 读出的数据存放的指针
返回值    读出来的字节数量
*****************************************************************/
uint8_t IIC2_Read_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data){
    uint8_t count = 0;
    uint8_t temp;
    IIC2_START();
    IIC2_Send_Byte(dev);
    IIC2_Wait_Ack();
    IIC2_Send_Byte(reg);
    IIC2_Wait_Ack();
    IIC2_START();
	IIC2_Send_Byte(dev+1);  //进入接收模式	
	IIC2_Wait_Ack();
    for(count=0;count<length;count++){
        if(count!=(length-1))
            temp = IIC2_Read_Byte(1);
        else
            temp = IIC2_Read_Byte(0);         //最后一次不应答
        
        data[count] = temp;
    }
    IIC2_STOP();
    return count;
}

/****************************************************************
函数原型 uint8_t IIC_Write_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data)
功    能  写入指定设备 指定寄存器的length个值
参    数  dev 指定设备地址
          reg 指定寄存器地址
          length 要写的字节数
          *data 写入的数据存放的指针
返回值    写入的字节数量
*****************************************************************/
uint8_t IIC2_Write_Bytes(uint8_t dev,uint8_t reg,uint8_t length,uint8_t *data){
    uint8_t count = 0;
    IIC2_START();
    IIC2_Send_Byte(dev);
    IIC2_Wait_Ack();
    IIC2_Send_Byte(reg);
    IIC2_Wait_Ack();
    for(count=0;count<length;count++){
        IIC2_Send_Byte(data[count]);
        IIC2_Wait_Ack();
    }
    IIC2_STOP();
    return 1;
}

/****************************************************************
函数原型 uint8_t IIC_Creat_Byte(uint8_t dev,uint8_t reg,uint8_t *data)
功    能  读取指定设备 指定寄存器的一个值
参    数  dev 指定设备地址
          reg 指定寄存器地址
          *data 要读取的字节
返回值    1
*****************************************************************/
uint8_t IIC2_Creat_Byte(uint8_t dev,uint8_t reg,uint8_t *data){
    *data = IIC2_Read_OneByte(dev,reg);
    return 1;
}

/****************************************************************
函数原型 uint8_t IIC_Write_Byte(uint8_t dev,uint8_t reg,uint8_t data)
功    能  写入指定设备 指定寄存器的一个值
参    数  dev 指定设备地址
          reg 指定寄存器地址
          data 要写入的字节
返回值    1
*****************************************************************/
uint8_t IIC2_Write_Byte(uint8_t dev,uint8_t reg,uint8_t data){
    return IIC2_Write_Bytes(dev,reg,1,&data);
}

/****************************************************************
函数原型 uint8_t IIC_Write_Bits(uint8_t dev,uint8_t reg,uint8_t bitstart,uint8_t length,uint8_t data)
功    能  修改指定设备 指定寄存器的一个字节的几个位
参    数  dev 指定设备地址
          reg 指定寄存器地址
          bitstart 修改的起始位
          length 修改的位长度
          data 存放要修改的字节的值
返回值    修改成功 1
          失败 0
*****************************************************************/
uint8_t IIC2_Write_Bits(uint8_t dev,uint8_t reg,uint8_t bitstart,uint8_t length,uint8_t data){
    uint8_t b;
    if(IIC2_Creat_Byte(dev,reg,&b)!=0){
        uint8_t mask = 0xFF << (bitstart+1) | 0xFF >> ((7-bitstart)+length);    //只保留要修改的位为1.其它为0
        data <<= (8 - length);
        data >>= (7 - bitstart);
        b &= mask;
        b |= data;
        return IIC2_Write_Byte(dev,reg,b);
    }
    else
        return 0;
}

/****************************************************************
函数原型 uint8_t IIC_Write_Bit(uint8_t dev,uint8_t reg,uint8_t bitnum,uint8_t data)
功    能  修改指定设备 指定寄存器的一个字节的1个位
参    数  dev 指定设备地址
          reg 指定寄存器地址
          bitnum 修改的位地址
          data 存放要修改的字节的值
返回值    修改成功 1
          失败 0
*****************************************************************/
uint8_t IIC2_Write_Bit(uint8_t dev,uint8_t reg,uint8_t bitnum,uint8_t data){
    data &= 0x01;                       //确保只传入一位
    return IIC2_Write_Bits(dev,reg,bitnum,1,data);
}

