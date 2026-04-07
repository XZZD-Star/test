#include "health_monitor.h"

#include "uart_screen.h"

#include <stdio.h>

#define HEALTH_MONITOR_COMP_DATE          "txt_time"
#define HEALTH_MONITOR_COMP_TIME          "txt_time"
#define HEALTH_MONITOR_COMP_WEATHER_ICON  "x0"
#define HEALTH_MONITOR_COMP_WEATHER_TEXT  "txt_temp"
#define HEALTH_MONITOR_COMP_OUTDOOR_TEMP  "txt_temp"
#define HEALTH_MONITOR_COMP_INDOOR_TEMP   "txt_temp"
#define HEALTH_MONITOR_COMP_HUMIDITY      "txt_hum"
#define HEALTH_MONITOR_COMP_TEMP_PROGRESS "j0"
#define HEALTH_MONITOR_COMP_HEART_ICON    "x2"
#define HEALTH_MONITOR_COMP_HEART_RATE    "txt_rate"
#define HEALTH_MONITOR_COMP_HEART_UNIT    "txt_bpm"

#define HEALTH_MONITOR_HEART_ICON_PIC_ID 7U

static const char *const k_weather_text[] =
{
  "Sunny",
  "Cloudy",
  "Rainy",
  "Snowy",
  "Thunder",
  "Foggy"
};

static const uint8_t k_weather_pic_id[] =
{
  0U,
  1U,
  2U,
  3U,
  4U,
  5U
};

static const HealthData_t k_demo_data =
{
  2026U,
  4U,
  6U,
  20U,
  26U,
  0U,
  WEATHER_SUNNY,
  26,
  265,
  65U,
  72U
};

static uint8_t HealthMonitor_IsWeatherValid(WeatherType_t type)
{
  return ((uint32_t)type < (uint32_t)(sizeof(k_weather_pic_id) / sizeof(k_weather_pic_id[0]))) ? 1U : 0U;
}

void HealthMonitor_Init(void)
{
  if (Screen_IsReady() == 0U)
  {
    return;
  }

  HealthMonitor_SetPage(0U);
  Screen_Nextion_SetPicture(HEALTH_MONITOR_COMP_HEART_ICON, HEALTH_MONITOR_HEART_ICON_PIC_ID);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_HEART_UNIT, "BPM");
  HealthMonitor_SendDemoFrame();
}

void HealthMonitor_SetPage(uint8_t page_id)
{
  Screen_Nextion_SetPage(page_id);
}

void HealthMonitor_UpdateDateTime(uint16_t year,
                                  uint8_t month,
                                  uint8_t day,
                                  uint8_t hour,
                                  uint8_t minute,
                                  uint8_t second)
{
  char text[24];

  (void)snprintf(text, sizeof(text), "%04u-%02u-%02u",
                 (unsigned int)year,
                 (unsigned int)month,
                 (unsigned int)day);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_DATE, text);

  (void)snprintf(text, sizeof(text), "%02u:%02u:%02u",
                 (unsigned int)hour,
                 (unsigned int)minute,
                 (unsigned int)second);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_TIME, text);
}

void HealthMonitor_SetWeather(WeatherType_t type, int8_t outdoor_temp)
{
  char text[24];

  if (HealthMonitor_IsWeatherValid(type) != 0U)
  {
    Screen_Nextion_SetPicture(HEALTH_MONITOR_COMP_WEATHER_ICON, k_weather_pic_id[type]);
    Screen_Nextion_SetText(HEALTH_MONITOR_COMP_WEATHER_TEXT, k_weather_text[type]);
  }

  (void)snprintf(text, sizeof(text), "OUT:%dC", (int)outdoor_temp);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_OUTDOOR_TEMP, text);
}

void HealthMonitor_UpdateTempHumi(int16_t indoor_temp_tenths, uint8_t humidity)
{
  char text[24];
  int32_t temp_abs = indoor_temp_tenths;
  char sign = '+';
  int32_t progress = indoor_temp_tenths / 10;

  if (temp_abs < 0)
  {
    sign = '-';
    temp_abs = -temp_abs;
  }

  if (progress < 0)
  {
    progress = 0;
  }
  else if (progress > 100)
  {
    progress = 100;
  }

  (void)snprintf(text, sizeof(text), "IN:%c%ld.%ldC",
                 sign,
                 (long)(temp_abs / 10),
                 (long)(temp_abs % 10));
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_INDOOR_TEMP, text);

  (void)snprintf(text, sizeof(text), "RH:%u%%", (unsigned int)humidity);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_HUMIDITY, text);

  Screen_Nextion_SetValue(HEALTH_MONITOR_COMP_TEMP_PROGRESS, progress);
}

void HealthMonitor_SetHeartRate(uint8_t bpm)
{
  char text[8];

  (void)snprintf(text, sizeof(text), "%u", (unsigned int)bpm);
  Screen_Nextion_SetText(HEALTH_MONITOR_COMP_HEART_RATE, text);
}

void HealthMonitor_UpdateAll(const HealthData_t *data)
{
  if (data == NULL)
  {
    return;
  }

  HealthMonitor_UpdateDateTime(data->year,
                               data->month,
                               data->day,
                               data->hour,
                               data->minute,
                               data->second);
  HealthMonitor_SetWeather(data->weather_type, data->outdoor_temp);
  HealthMonitor_UpdateTempHumi(data->indoor_temp_tenths, data->humidity);
  HealthMonitor_SetHeartRate(data->heart_rate);
}

void HealthMonitor_SendDemoFrame(void)
{
  if (Screen_IsReady() == 0U)
  {
    return;
  }

  HealthMonitor_UpdateAll(&k_demo_data);
}
