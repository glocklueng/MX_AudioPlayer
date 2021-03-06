#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "stm32f1xx_hal.h"
#include "tx_cfg.h"
#include "AP_OS.h"
#include <stdio.h>
#include "lis3D.h"
#include "tx_cfg.h"
#include "math.h"
#include "USR_CFG.h"
#include "DEBUG_CFG.h"
#include "LED.h"
extern struct config SYS_CFG;
extern RGBL RGB_PROFILE[16][2];

extern osThreadId defaultTaskHandle;
extern osThreadId GPIOHandle;
extern osThreadId x3DList_CTLHandle;
extern osMessageQId SIG_GPIOHandle;
extern osMessageQId SIG_PLAYWAVHandle;
extern osMessageQId SIG_LEDHandle;
extern osTimerId TriggerFreezTimerHandle;

const uint32_t SIG_POWERKEY_DOWN = 0x0001;
const uint32_t SIG_POWERKEY_UP = 0x0002;
const uint32_t SIG_USERKEY_DOWN = 0x0004;
const uint32_t SIG_USERKEY_UP = 0x0008;

volatile static struct TFT {
  volatile int32_t TB;
  volatile int32_t TC;
  volatile int32_t TD;
} Trigger_Freeze_TIME;

/* System status in now */
__IO enum { SYS_close, SYS_ready, SYS_running } System_Status = SYS_close;

/* Bank now */
__IO uint8_t sBANK = 0;
uint8_t MUTE_FLAG = 0;

void Handle_System(void const* argument) {
  uint32_t cnt;
  osEvent evt;
  extern struct config SYS_CFG;
  printf_SYSTEM(">>>System in ready mode\n");
  if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET) MUTE_FLAG = 1;
  System_Status = SYS_ready;
  Trigger_Freeze_TIME.TB = 0, Trigger_Freeze_TIME.TC = 0,
  Trigger_Freeze_TIME.TD = 0;
  osTimerStart(TriggerFreezTimerHandle, 10);

  // System into READY
  if (MUTE_FLAG)
    osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_STARTUP, PUT_MESSAGE_WAV_TIMEOUT);
  while (1) {
    evt = osMessageGet(SIG_GPIOHandle, 10);

    if (System_Status == SYS_ready && evt.status == osEventMessage &&
        evt.value.signals & SIG_POWERKEY_DOWN) {
      cnt = 0;
      // wait for power key rise.
      while (1) {
        evt = osMessageGet(SIG_GPIOHandle, 1);

        if (evt.status == osEventMessage && evt.value.signals & SIG_POWERKEY_UP)
          break;
        else
          cnt++;
      }
      printf_KEY("  Counting power key T:%dms\n", cnt);
      if (cnt >=
          (SYS_CFG.Tpoff > SYS_CFG.Tout ? SYS_CFG.Tout : SYS_CFG.Tpoff)) {
        if ((SYS_CFG.Tpoff >= SYS_CFG.Tout && SYS_CFG.Tpoff <= cnt) ||
            (SYS_CFG.Tpoff <= SYS_CFG.Tout && SYS_CFG.Tout > cnt)) {
          printf_SYSTEM(">>>System in close mode\n");
          System_Status = SYS_close;
          // System into CLOSE
          if (MUTE_FLAG)
            osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_POWEROFF,
                         PUT_MESSAGE_WAV_TIMEOUT);
          osDelay(2000);
          HAL_GPIO_WritePin(Power_EN_GPIO_Port, Power_EN_Pin, GPIO_PIN_RESET);
          continue;
        } else {
          printf_SYSTEM(">>>System in running mode\n");
          System_Status = SYS_running;
          // System into RUNNING
          if (MUTE_FLAG)
            osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_INTORUN,
                         PUT_MESSAGE_WAV_TIMEOUT);
          osMessagePut(SIG_LEDHandle, SIG_LED_INTORUN, PUT_MESSAGE_LED_TIMEOUT);
          continue;
        }
      }
    }  // end of System = ready, event == Power key

    if (System_Status == SYS_running && evt.status == osEventMessage &&
        evt.value.signals & SIG_POWERKEY_DOWN) {
      cnt = 0;
      // wait for power key rise.
      while (1) {
        evt = osMessageGet(SIG_GPIOHandle, 1);

        if (evt.status == osEventMessage && evt.value.signals & SIG_POWERKEY_UP)
          break;
        else
          cnt++;
      }
      printf_KEY("  Counting power key T:%dms\n", cnt);
      if (cnt >= SYS_CFG.Tin) {
        printf_SYSTEM(">>>System in ready mode\n");
        System_Status = SYS_ready;
        // System into READY
        if (MUTE_FLAG)
          osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_OUTRUN,
                       PUT_MESSAGE_WAV_TIMEOUT);
        osMessagePut(SIG_LEDHandle, SIG_LED_OUTRUN, PUT_MESSAGE_LED_TIMEOUT);
        continue;
      }
    }  // end of System = running, event == Power key

    if (System_Status == SYS_ready && evt.status == osEventMessage &&
        evt.value.signals & SIG_USERKEY_DOWN) {
      cnt = 0;
      for (;;) {
        evt = osMessageGet(SIG_GPIOHandle, 1);

        if (evt.status == osEventMessage && evt.value.signals & SIG_USERKEY_UP)
          break;
        else
          cnt++;
      }
      printf_KEY("  Counting usr key T:%dms\n", cnt);
      if (cnt >= SYS_CFG.Ts_switch) {
        sBANK += 1;
        sBANK %= nBank;
        printf_SYSTEM(">>>System put bank switch message\n");
        if (MUTE_FLAG)
          osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_BANKSWITCH,
                       PUT_MESSAGE_WAV_TIMEOUT);
      }
      continue;
    }
    // end of System = ready, event == User Key

    if (System_Status == SYS_running && evt.status == osEventMessage &&
        evt.value.signals & SIG_USERKEY_DOWN) {
      cnt = 0;
      for (;;) {
        evt = osMessageGet(SIG_GPIOHandle, 1);

        if (evt.status == osEventMessage && evt.value.signals == SIG_USERKEY_UP)
          break;
        else if (evt.status == osEventMessage && evt.value.signals == SIG_POWERKEY_DOWN) {
          cnt = 0;
          break;
        }
        else if (++cnt >= SYS_CFG.TEtrigger) {
          printf_SYSTEM(">>>System put Trigger E\n");
          if (MUTE_FLAG)
            osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_TRIGGERE,
                         PUT_MESSAGE_WAV_TIMEOUT);
          osMessagePut(SIG_LEDHandle, SIG_LED_TRIGGERE,
                       PUT_MESSAGE_LED_TIMEOUT);
          break;
        }
      }
      if (cnt >= SYS_CFG.TEtrigger) {
        for (;;) {
          evt = osMessageGet(SIG_GPIOHandle, 1);
          if (evt.status == osEventMessage &&
              evt.value.signals & SIG_USERKEY_UP)
            break;
        }
        printf_SYSTEM(">>>System put Trigger E off\n");
        if (MUTE_FLAG)
          osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_TRIGGEREOFF,
                       PUT_MESSAGE_WAV_TIMEOUT);
        osMessagePut(SIG_LEDHandle, SIG_LED_TRIGGEREOFF,
                     PUT_MESSAGE_LED_TIMEOUT);
      } else if (!Trigger_Freeze_TIME.TD && cnt) {
        Trigger_Freeze_TIME.TD = SYS_CFG.TDfreeze;
        printf_SYSTEM(">>>System put Trigger D\n");
        if (MUTE_FLAG)
          osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_TRIGGERD,
                       PUT_MESSAGE_WAV_TIMEOUT);
        osMessagePut(SIG_LEDHandle, SIG_LED_TRIGGERD, PUT_MESSAGE_LED_TIMEOUT);
      } else {
				printf_SYSTEM(">>>System put LED switch BANK\n");
				osMessagePut(SIG_LEDHandle, SIG_LED_SWITCHBANK, PUT_MESSAGE_LED_TIMEOUT);
				osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_COLORSWITCH, PUT_MESSAGE_WAV_TIMEOUT);
			}
    }
    // end of System = running , event == User Key
  }
}

void Handle_GPIO(void const* argument) {
  GPIO_PinState power;
  GPIO_PinState usr;
  GPIO_PinState GPIO_Buffer;
  power = HAL_GPIO_ReadPin(Power_KEY_GPIO_Port, Power_KEY_Pin);
  usr = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);
  for (;;) {
    GPIO_Buffer = HAL_GPIO_ReadPin(Power_KEY_GPIO_Port, Power_KEY_Pin);
    if (power != GPIO_Buffer) {
      osDelay(10);
      GPIO_Buffer = HAL_GPIO_ReadPin(Power_KEY_GPIO_Port, Power_KEY_Pin);
      if (GPIO_Buffer == power) continue;

      power = GPIO_Buffer;
      if (GPIO_Buffer == GPIO_PIN_SET) {
        printf_KEY("Power KEY down\n");
      } else {
        printf_KEY("Power KEY UP\n");
      }
      if (GPIO_Buffer == GPIO_PIN_SET)
        osMessagePut(SIG_GPIOHandle, SIG_POWERKEY_DOWN, osWaitForever);
      else
        osMessagePut(SIG_GPIOHandle, SIG_POWERKEY_UP, osWaitForever);
    }
    GPIO_Buffer = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);
    if (usr != GPIO_Buffer) {
      osDelay(10);
      GPIO_Buffer = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);
      if (GPIO_Buffer == usr) continue;
      usr = GPIO_Buffer;
      if (GPIO_Buffer == GPIO_PIN_RESET) {
        printf_KEY("User KEY down\n");
      } else {
        printf_KEY("User KEY UP\n");
      }
      if (GPIO_Buffer == GPIO_PIN_RESET)
        osMessagePut(SIG_GPIOHandle, SIG_USERKEY_DOWN, osWaitForever);
      else
        osMessagePut(SIG_GPIOHandle, SIG_USERKEY_UP, osWaitForever);
    }
    osDelay(3);
  }
}

void x3DListHandle(void const* argument) {
  Lis3dData data;
  float ans;
  taskENTER_CRITICAL();
  Lis3d_Init();
  taskEXIT_CRITICAL();
  for (;;) {
    if (System_Status != SYS_running) {
      osDelay(50);
      continue;
    }

    taskENTER_CRITICAL();
    Lis3dGetData(&data);
    taskEXIT_CRITICAL();

    ans = data.Dx * data.Dx;
    ans += data.Dy * data.Dy;
    ans += data.Dz * data.Dz;
    ans = sqrt(ans);
    ans = ans * 4 / 0x10000 * 512;

    // Trigger B
    if (SYS_CFG.Cl <= ans && SYS_CFG.Ch >= ans && !Trigger_Freeze_TIME.TC) {
      Trigger_Freeze_TIME.TC = SYS_CFG.TCfreeze;
      printf_SYSTEM(">>>System put Trigger C\n");
      if (MUTE_FLAG) osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_TRIGGERC, 10);
      osMessagePut(SIG_LEDHandle, SIG_LED_TRIGGERC, 10);
      continue;
    }
    // Trigger C
    else if (SYS_CFG.S1 <= ans && SYS_CFG.Sh >= ans &&
             !Trigger_Freeze_TIME.TB) {
      Trigger_Freeze_TIME.TB = SYS_CFG.TBfreeze;
      printf_SYSTEM(">>>System put Trigger B\n");
      if (MUTE_FLAG) osMessagePut(SIG_PLAYWAVHandle, SIG_AUDIO_TRIGGERB, 10);
      osMessagePut(SIG_LEDHandle, SIG_LED_TRIGGERB, 10);
      continue;
    }

    osDelay(10);
  }
}

void TriggerFreez(void const* argument) {
  if (Trigger_Freeze_TIME.TB > 10)
    Trigger_Freeze_TIME.TB -= 10;
  else if (Trigger_Freeze_TIME.TB > 0)

  {
    printf_TRIGGERFREEZ("Trigger B reset\n");
    Trigger_Freeze_TIME.TB = 0;
  } else if (Trigger_Freeze_TIME.TB < 0) {
    printf_TRIGGERFREEZ("Trigger B reset\n");
    Trigger_Freeze_TIME.TB = 0;
  }

  if (Trigger_Freeze_TIME.TC > 10)
    Trigger_Freeze_TIME.TC -= 10;
  else if (Trigger_Freeze_TIME.TC > 0) {
    printf_TRIGGERFREEZ("Trigger C reset\n");
    Trigger_Freeze_TIME.TC = 0;
  } else if (Trigger_Freeze_TIME.TC < 0) {
    printf_TRIGGERFREEZ("Trigger C reset\n");
    Trigger_Freeze_TIME.TC = 0;
  }

  if (Trigger_Freeze_TIME.TD > 10)
    Trigger_Freeze_TIME.TD -= 10;
  else if (Trigger_Freeze_TIME.TD > 0) {
    printf_TRIGGERFREEZ("Trigger D reset\n");
    Trigger_Freeze_TIME.TD = 0;
  } else if (Trigger_Freeze_TIME.TD < 0) {
    printf_TRIGGERFREEZ("Trigger D reset\n");
    Trigger_Freeze_TIME.TD = 0;
  }
}
