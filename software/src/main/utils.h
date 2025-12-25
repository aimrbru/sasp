#ifndef UTILS_H
#define UTILS_H

#include "esp_err.h"         // Для использования esp_err_t
#include <stdbool.h>         // Для типа bool
#include <stddef.h>          // Для типа size_t
#include "cJSON.h"           // Для работы с JSON
#include "esp_blufi_api.h"    // Для работы с BLUFI API
#include "esp_http_client.h"  // Для работы с HTTP-клиентом
#include <sys/time.h>        // Для работы со временем (например, gettimeofday)
#include "esp_sntp.h"
#include "esp_sleep.h"
#include "esp_timer.h"       // Для работы с таймерами ESP
#include "freertos/timers.h" // Для работы с FreeRTOS-таймерами
#include "esp_wifi.h"        // Для работы с Wi-Fi
#include "esp_heap_caps.h"   // Для работы с кучей (heap)
#include "esp_psram.h"       // Для работы с PSRAM
#include "nvs_flash.h"       // Для работы с NVS (Non-Volatile Storage)
#include "nvs.h"             // Для работы с NVS API
#include <stdint.h>          // Для типов данных, таких как uint32_t
#include <string.h>          // Для работы со строками (например, memset, memcpy)
#include <stdio.h>           // Для стандартного ввода/вывода
#include <stdlib.h>          // Для работы с malloc, free и т.д.
#include "esp_log.h"         // Для логирования
#include "mbedtls/base64.h"  // Для работы с Base64
#include <dirent.h>          // Для работы с каталогами
#include <time.h>            // Для работы со временем (например, time, localtime)
#include "esp_spiffs.h"      // Для работы с SPIFFS
#include "esp_netif.h"       // Для работы с сетевым интерфейсом

#define STORAGE_NAMESPACE "storage"
#define SPIFFS_DIR "/spiffs"

typedef struct
{
    char id[21];  // Device ID (максимум 20 символов + завершающий '\0')
    char type[9]; // тип
    int x1, y1;   // Левый верхний угол области
    int x2, y2;   // Правый нижний угол области
} DeviceConfig;

// Объявление сруктуры Общие настройки 
typedef struct
{
    uint8_t ocr_enabled;    
    uint8_t copy_to_server; 
    uint8_t sleep_enabled; 
    char server_path[128];
    uint32_t sleep_seconds;
    uint16_t agc_gain;       // Значение усиления AGC (Automatic Gain Control)
    uint16_t aec_value;      // Значение экспозиции AEC (Automatic Exposure Control)
    uint16_t flash_duty;     // Значение яркости вспышки (LED Duty Cycle)
} common_settings_t;

// Структура указателей для process_images
typedef struct
{
    uint8_t *image_data;               // Указатель на данные изображения
    size_t image_len;                  // Длина данных изображения
    char device_id[21];  // 
    char device_type[9]; // 
    QueueHandle_t filenames_queue;     // Очередь для передачи имен файлов
} TaskParams;

// Объявление глобального массива device_keys
extern const char *device_keys[];
extern const size_t num_devices;

void print_free_memory();

void print_system_time();
esp_err_t save_time_to_nvs();
esp_err_t restore_time_from_nvs();

void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

void erase_nvs();
esp_err_t init_nvs(void);
uint16_t get_boot_count();
void delete_old_files();
void format_spiffs();
esp_err_t init_spiffs(void);
void list_spiffs_files();

bool obtain_time_from_sntp();

esp_err_t save_common_settings_nvs(common_settings_t *settings);
esp_err_t save_device_id(const char *key, const char *id, const char *type);
esp_err_t save_coordinates(const char *key, int32_t x1, int32_t y1, int32_t x2, int32_t y2);

esp_err_t load_device_config(const char *key, DeviceConfig *config);
esp_err_t read_common_settings_nvs(common_settings_t *settings);

esp_err_t read_from_spiffs(const char *dir, const char *filename, uint8_t **buffer, size_t *size, const char *mode);
esp_err_t save_to_spiffs(const char *dir, const char *filename, const uint8_t *data, size_t size);

esp_err_t read_file_to_buffer(const char *filepath, uint8_t **buffer, size_t *file_size);

void go_to_deep_sleep();

#endif // UTILS_H