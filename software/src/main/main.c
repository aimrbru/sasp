
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"

#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_blufi_api.h"

#include "my_blufi.h"
#include "my_img.h"
#include "utils.h"
#include "web.h"

#include "esp_sleep.h"

#define TAG "MAIN"

#define INACTIVITY_TIMEOUT_SEC 300     // Время бездействия перед переходом в deep sleep

void app_main(void)
{
  // erase_nvs();
  // format_spiffs();
  // print_free_memory();

  ESP_ERROR_CHECK(init_nvs());
  ESP_ERROR_CHECK(init_spiffs());

  ESP_ERROR_CHECK(init_camera());
  vTaskDelay(pdMS_TO_TICKS(500));

  ESP_ERROR_CHECK(start_blufi());
  vTaskDelay(pdMS_TO_TICKS(2000));

  // Регистрация пользовательского логгера
  register_custom_logger();

  // Синхронизация времени
  if (!obtain_time_from_sntp())
  {
    // устанавливаем текущее время из nvs
    restore_time_from_nvs();
  }
  print_system_time();
  // Сохраняем текущее время в nvs
  save_time_to_nvs();

  // Вызываем process_images для обработки изображений
  char **filenames = NULL;
  esp_err_t process_result = process_images(&filenames);
  if (process_result != ESP_OK)
  {
    ESP_LOGE(TAG, "Ошибка при обработке изображений");
    return;
  }

  // Создание таймера для отслеживания бездействия
  inactivity_timer = xTimerCreate(
      "InactivityTimer",                            // Имя таймера
      pdMS_TO_TICKS(INACTIVITY_TIMEOUT_SEC * 1000), // Период таймера (в тиках)
      pdFALSE,                                      // Однократный таймер (не периодический)
      (void *)0,                                    // Идентификатор таймера (не используется)
      inactivity_timer_callback                     // Функция обратного вызова
  );

  if (inactivity_timer == NULL)
  {
    ESP_LOGE(TAG, "Failed to create inactivity timer.");
    return;
  }

  // Запуск таймера
  xTimerStart(inactivity_timer, 0);

  ESP_LOGI(TAG, "System initialized.");
}
