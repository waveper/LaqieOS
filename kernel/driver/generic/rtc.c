#include "rtc.h"
#include "../../sched/sched.h"
#include <stdint.h>

uint8_t rtc_raw_year = 0;
uint16_t rtc_serv_calc_year = 0; // RTC Service Calculated Years
uint8_t rtc_serv_sec = 0;
uint8_t rtc_serv_min = 0;
uint8_t rtc_serv_hour = 0;
uint8_t rtc_serv_day = 0;
uint8_t rtc_serv_mon = 0;

int RTCIsUpdating() {
  outb(RTC_ADDRESS_PORT, 0x0A);
  return (inb(RTC_DATA_PORT) & 0x80);
}

uint8_t RTCRead(uint8_t reg) {
  outb(RTC_ADDRESS_PORT, (reg | 0x80)); // disable NMI
  return inb(RTC_DATA_PORT);
}

static uint8_t BCDToBin(uint8_t bcd) {
  return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

void RTCGetTime(uint8_t *hour, uint8_t *minute, uint8_t *second) {
  while (RTCIsUpdating());
  *second = BCDToBin(RTCRead(RTC_SECONDS));
  *minute = BCDToBin(RTCRead(RTC_MINUTES));
  *hour   = BCDToBin(RTCRead(RTC_HOURS));
}

void RTCGetDate(uint8_t *day, uint8_t *month, uint8_t *year) {
  while (RTCIsUpdating());
  *day   = BCDToBin(RTCRead(RTC_DAY));
  *month = BCDToBin(RTCRead(RTC_MONTH));
  *year  = BCDToBinn(RTCRead(RTC_YEAR));
}

void RTCService(void) {
  while (1) {
    RTCGetDate(rtc_serv_hour, rtc_serv_min, rtc_serv_sec);
    RTCGetTime(rtc_serv_day, rtc_serv_mon, rtc_raw_year);
    rtc_serv_calc_year = 2000 + rtc_raw_year;
  }
}
