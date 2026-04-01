#include "task.h"

typedef struct
{
    uint8_t run;                //调度标志
    uint8_t TimCount;           //时间片周期,用于递减计数
    uint8_t TimRload;           //时间片周期,用于重载
    void (*pTaskFunc)(void);    //函数指针，保存任务地址
}TaskComps_t;

//void Max30102Task(void);
//void icm42688Task(void);

static TaskComps_t TaskComps[] =
{
    {0,10,10,icm42688Task},
    {0,10,10,max30102SampleTask},
    {0,10,10,max30102CalcTask},//1s计算一次
    {0,100,100,printTask},
} ;

#define TASK_NUM_MAX ((uint8_t)(sizeof(TaskComps) / sizeof(TaskComps[0])))

void TaskSchedule(void)
{
    for(uint8_t i=0;i<TASK_NUM_MAX;i++)
    {
        if(TaskComps[i].TimCount)
        {
            TaskComps[i].TimCount--;
            if(TaskComps[i].TimCount == 0)
            {
                TaskComps[i].TimCount = TaskComps[i].TimRload;
                TaskComps[i].run = 1;
            }
        }
    }
}

void TaskHandler(void)
{
    for(uint8_t i=0;i<TASK_NUM_MAX;i++)
    {
        if(TaskComps[i].run)
        {
            TaskComps[i].run = 0;
            TaskComps[i].pTaskFunc();
        }
    }
}

//void Max30102Task(void)
//{
//    uint32_t un_min, un_max;  // 信号范围变量
//    int i;
//    uint32_t timeout;
//    un_min = 0x3FFFF;
//    un_max = 0;
//}

//void icm42688Task(void)
//{
//    
//}
