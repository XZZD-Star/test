/**
 ****************************************************************************************************
 * @file        delay.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-09-06
 * @brief       使用SysTick的普通计数模式对延迟进行管理(支持ucosii)
 *              提供delay_init初始化函数， delay_us和delay_ms等延时函数
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 阿波罗 H743开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20220906
 * 第一次发布
 *
 ****************************************************************************************************
 */

#include "./sys/sys.h"
#include "./delay/delay.h"


static uint16_t g_fac_us = 0;  /* us延时倍乘数 */

/* 如果SYS_SUPPORT_OS定义了,说明要支持OS了(不限于UCOS) */
#if SYS_SUPPORT_OS

/* 添加公共头文件 ( ucos需要用到) */
#include "FreeRTOS.h"
#include "task.h"

extern void xPortSysTickHandler(void);

/**
 * @brief     systick中断服务函数,使用OS时用到
 * @param     无
 * @retval    无
 */  
extern void SysTick_Handler(void);

#endif

/**
 * @brief     初始化延迟函数
 * @param     sysclk: 系统时钟频率, 即CPU频率(rcc_c_ck), 480Mhz
 * @retval    无
 */  
void delay_init(uint16_t sysclk)
{
    #if SYS_SUPPORT_OS
     uint32_t reload;
    #endif
     HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
     g_fac_us = sysclk;
     
    #if SYS_SUPPORT_OS
     reload = sysclk;
     /* 使用 configTICK_RATE_HZ 计算重装载值
     * configTICK_RATE_HZ 在 FreeRTOSConfig.h 中定义
     */
     reload *= 1000000 / configTICK_RATE_HZ;
     /* 删除不用的 g_fac_ms 相关代码 */
     SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
     SysTick->LOAD = reload;
     SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    #endif 
}
 
#if SYS_SUPPORT_OS  /* 如果需要支持OS, 用以下代码 */

/**
 * @brief     延时nus
 * @param     nus: 要延时的us数
 * @note      nus取值范围: 0~8947848(最大值即2^32 / g_fac_us @g_fac_us = 480)
 * @retval    无
 */ 
void delay_us(uint32_t nus)
{
    uint32_t ticks;
     uint32_t told, tnow, tcnt = 0;
     uint32_t reload = SysTick->LOAD;
     /* 删除适用于 μC/OS 用于锁定任务调度器的自定义函数 */
     ticks = nus * g_fac_us;
     told = SysTick->VAL;
     while (1)
     {
         tnow = SysTick->VAL;
         if (tnow != told)
         {
            if (tnow < told)
             {
                tcnt += told - tnow;
             }
             else
             {
                tcnt += reload - tnow + told;
             }
             told = tnow;
             if (tcnt >= ticks)
             {
                break;
             }
        }
     }
 /* 删除适用于 μC/OS 用于解锁任务调度器的自定义函数 */
} 

/**
 * @brief     延时nms
 * @param     nms: 要延时的ms数 (0< nms <= 65535) 
 * @retval    无
 */
void delay_ms(uint16_t nms)
{
    uint32_t i;
 
     for (i=0; i<nms; i++)
     {
        delay_us(1000);
     }
}

#else  /* 不使用OS时, 用以下代码 */

/**
 * @brief       延时nus
 * @param       nus: 要延时的us数.
 * @note        注意: nus的值,不要大于34952us(最大值即2^24 / g_fac_us @g_fac_us = 480)
 * @retval      无
 */
void delay_us(uint32_t nus)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;  /* LOAD的值 */
    ticks = nus * g_fac_us;           /* 需要的节拍数 */
    told = SysTick->VAL;              /* 刚进入时的计数器值 */
    while (1)
    {
        tnow = SysTick->VAL;
        if (tnow != told)
        {
            if (tnow < told)
            {
                tcnt += told - tnow;  /* 这里注意一下SYSTICK是一个递减的计数器就可以了 */
            }
            else 
            {
                tcnt += reload - tnow + told;
            }
            told = tnow;
            if (tcnt >= ticks)
            {
                break;                /* 时间超过/等于要延迟的时间,则退出 */
            }
        }
    }
}

/**
 * @brief       延时nms
 * @param       nms: 要延时的ms数 (0< nms <= 65535)
 * @retval      无
 */
void delay_ms(uint16_t nms)
{
    uint32_t repeat = nms / 30;     /*  这里用30,是考虑到可能有超频应用,
                                     *  比如500Mhz的时候, delay_us最大只能延时33554us左右了
                                     */
    uint32_t remain = nms % 30;

    while (repeat)
    {
        delay_us(30 * 1000);        /* 利用delay_us 实现 1000ms 延时 */
        repeat--;
    }

    if (remain)
    {
        delay_us(remain * 1000);    /* 利用delay_us, 把尾数延时(remain ms)给做了 */
    }
}

#endif









