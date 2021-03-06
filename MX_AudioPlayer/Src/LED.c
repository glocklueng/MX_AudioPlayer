#include "LED.h"
#include "DEBUG_CFG.h"
#include "tx_cfg.h"
extern struct config SYS_CFG;
extern uint8_t sBANK;
extern osMessageQId SIG_LEDHandle;
extern RGBL RGB_PROFILE[16][2];
/* When System from Close into Ready */
const uint8_t SIG_LED_STARTUP = 1;

/* When System from Ready into Close */
const uint8_t SIG_LED_POWEROFF = 2;

/* When SYstem from Ready into running */
const uint8_t SIG_LED_INTORUN = 3;

/* When System from Running into Reayd */
const uint8_t SIG_LED_OUTRUN = 4;

/* When System in Ready switch the Bank */
const uint8_t SIG_LED_BANKSWITCH = 5;

/* Triggers */
/* When System in Running, Trigger B */
const uint8_t SIG_LED_TRIGGERB = 0x10;
/* When System in Running, Trigger C */
const uint8_t SIG_LED_TRIGGERC = 0x20;
/* When System in Running, Trigger D */
const uint8_t SIG_LED_TRIGGERD = 0x30;
/* When System in Running, Trigger E */
const uint8_t SIG_LED_TRIGGERE = 0x40;
/* When System in Running, Trigger E off */
const uint8_t SIG_LED_TRIGGEREOFF = 0x50;

const uint8_t SIG_LED_SWITCHBANK = 0x60;

void LEDHandle(void const *argument) {
  osEvent evt;
  static uint8_t shift_bank = 0;
  enum { IN_TRIGGER_E, OUT_TRIGGER_E } trigger_e = OUT_TRIGGER_E;
  uint8_t flag = 1;
  printf_LED("LED Handle init start\n");
  HAL_TIM_Base_Start(&htim1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
  while (1) {
    if (trigger_e == OUT_TRIGGER_E) {
      evt = osMessageGet(SIG_LEDHandle, osWaitForever);
    } else {
      evt = osMessageGet(SIG_LEDHandle, SYS_CFG.Ecycle / 2);
    }
    if (trigger_e == IN_TRIGGER_E) {
      LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][flag], 0xFF, 1);
      printf_LED("Trigger E Half Cycle hit\n");
      if (flag)
        flag = 0;
      else
        flag = 1;
    }
    if (evt.status != osEventMessage) continue;
    printf_LED("LED message GET\n");

    switch (evt.value.v) {
      case SIG_LED_INTORUN: {
        uint16_t cnt;
        printf_LED("&LED\tGet mode running message\n");
        for (cnt = 1; cnt <= SYS_CFG.TLon / 10; cnt++) {
          LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][0],
                        0xFF * cnt / (SYS_CFG.TLon / 10), 1);
          osDelay(10);
        }
        break;
      }
      case SIG_LED_OUTRUN: {
        uint16_t cnt;
        printf_LED("&LED\tGet mode ready message\n");
        cnt = SYS_CFG.TLoff / 10;
        while (cnt--) {
          LED_COLOR_SET(RGB_PROFILE[0][0], 0xFF * cnt / (SYS_CFG.TLoff / 10),
                        0);
          osDelay(10);
        }
        break;
      }
      case SIG_LED_TRIGGERB: {
        printf_LED("&LED\tGet trigger B message\n");
        printf_LED("&No oprat should do\n");
        break;
      }
      case SIG_LED_TRIGGERC: {
        uint8_t cycle;
        printf_LED("&LED\tGet trigger C message\n");
        cycle = SYS_CFG.Ccount - 1;
        while (cycle--) {
          LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][1], 0xFF, 1);
          osDelay(SYS_CFG.TCflip);
          LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][0], 0xFF, 1);
          osDelay(SYS_CFG.TCflip);
        }
        LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][1], 0xFF, 1);
        osDelay(SYS_CFG.TCflip);
        LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][0], 0xFF, 1);
        break;
      }
      case SIG_LED_TRIGGERD: {
        printf_LED("&LED\tGet trigger D message\n");
        LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][1], 0xFF, 1);
        osDelay(SYS_CFG.TDflip);
        LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][0], 0xFF, 1);
        break;
      }
      case SIG_LED_TRIGGERE: {
        printf_LED("&LED\tGet trigger E on message\n");
        LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][1], 0xFF, 1);
        trigger_e = IN_TRIGGER_E;
        flag = 0;
        break;
      }
      case SIG_LED_TRIGGEREOFF: {
        printf_LED("&LED\tGet trigger E off message\n");
        LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][0], 0xFF, 1);
        trigger_e = OUT_TRIGGER_E;
        break;
      }
      case SIG_LED_SWITCHBANK: {
        printf_LED("&LED\tGet switch bank\n");
        shift_bank += 1;
        shift_bank %= nBank;
        // Power off LED first
        {
          uint16_t cnt;
          printf_LED("&LED\tGet mode ready message\n");
          cnt = SYS_CFG.TLoff / 10;
          while (cnt--) {
            LED_COLOR_SET(RGB_PROFILE[0][0], 0xFF * cnt / (SYS_CFG.TLoff / 10),
                          0);
            osDelay(10);
          }
          {
            uint16_t cnt;
            printf_LED("&LED\tGet mode running message\n");
            for (cnt = 1; cnt <= SYS_CFG.TLon / 10; cnt++) {
              LED_COLOR_SET(RGB_PROFILE[(sBANK + shift_bank) % nBank][0],
                            0xFF * cnt / (SYS_CFG.TLon / 10), 1);
              osDelay(10);
            }
            break;
          }
          break;
        }
        // Power up LED then
      }
      default: {
        printf_LED("&LED\tGet undefine SIG of LED:%d\n", evt.value.v);
        break;
      }
    }
  }
}

void LED_COLOR_SET(RGBL data, uint8_t DC, uint8_t mode) {
  if (mode) {
    TIM1->CCR1 = (uint32_t)data.R * SYS_CFG.Lbright * DC / 0xFF / 1024;
    TIM1->CCR2 = (uint32_t)data.G * SYS_CFG.Lbright * DC / 0xFF / 1024;
    TIM1->CCR3 = (uint32_t)data.B * SYS_CFG.Lbright * DC / 0xFF / 1024;
    TIM1->CCR4 = (uint32_t)data.L * SYS_CFG.Lbright * DC / 0xFF / 1024;
  } else {
    TIM1->CCR1 = TIM1->CCR1 * DC / 0xFF;
    TIM1->CCR2 = TIM1->CCR2 * DC / 0xFF;
    TIM1->CCR3 = TIM1->CCR3 * DC / 0xFF;
    TIM1->CCR4 = TIM1->CCR4 * DC / 0xFF;
  }
}
