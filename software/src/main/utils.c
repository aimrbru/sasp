#include "utils.h"
#include "web.h"
#include "my_img.h"

#define TAG "UTILS"

#define SNTP_MAX_RETRIES 10
#define SNTP_RETRY_DELAY_MS 1000

#define NUM_RECORDS 100

#define MAX_FILES 30
// Структура для хранения информации о файле
typedef struct
{
    char name[64];
    uint32_t timestamp;
    uint32_t bootcount;
} file_info_t;

// Определение глобального массива device_keys
const char *device_keys[] = {"device1", "device2"};
const size_t num_devices = 2;

// Инициализация common_settings
common_settings_t default_common_settings = {
    .ocr_enabled = 0,
    .copy_to_server = 0,
    .server_path = "",
    .sleep_enabled = 0,
    .sleep_seconds = 180,
    .agc_gain = 10,   // Значение по умолчанию для AGC Gain (0-30)
    .aec_value = 500, // Значение по умолчанию для AEC Value (0-1200)
    .flash_duty = 100 // Значение по умолчанию для Flash Duty (0-255)
};

// Функция для вывода объема памяти
void print_free_memory()
{
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL); // DRAM
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);      // PSRAM (если есть)
    size_t total_free = free_internal + free_psram;

    ESP_LOGI("MEMORY", "Free Internal RAM (DRAM): %u bytes", free_internal);
    ESP_LOGI("MEMORY", "Free PSRAM: %u bytes", free_psram);
    ESP_LOGI("MEMORY", "Total Free RAM: %u bytes", total_free);
}

// Функция для стирания NVS
void erase_nvs()
{
    // Очистка NVS
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка при очистке NVS");
        return;
    }
    ESP_LOGI(TAG, "NVS успешно очищено");

    // Повторная инициализация NVS
    err = nvs_flash_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка при инициализации NVS");
        return;
    }
    ESP_LOGI(TAG, "NVS успешно инициализировано");
}

// Функция для получения статистики NVS
void print_nvs_stats()
{
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(NULL, &nvs_stats); // NULL означает использование раздела NVS по умолчанию
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get NVS stats: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "NVS Stats:");
    ESP_LOGI(TAG, "  Used entries: %u", nvs_stats.used_entries);
    ESP_LOGI(TAG, "  Free entries: %u", nvs_stats.free_entries);
    ESP_LOGI(TAG, "  Total entries: %u", nvs_stats.total_entries);
    ESP_LOGI(TAG, "  Namespace count: %u", nvs_stats.namespace_count);
}

// Функция для инициализации NVS и ключей
esp_err_t init_nvs(void)
{
    esp_err_t ret;

    // Инициализация NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    print_nvs_stats();

    // Открытие NVS с пространством имен "storage"
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));

    // Инициализация bootCount
    uint16_t bootCount = 0;
    ret = nvs_get_u16(nvs_handle, "bootCount", &bootCount);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "No bootCount found in NVS, setting default value: %u", bootCount);
        ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "bootCount", bootCount));
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading 'bootCount' from NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    else
    {
        ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "bootCount", bootCount + 1));
        ESP_LOGI(TAG, "Boot Count: %u", bootCount + 1);
    }

    // Инициализация saved_time
    uint64_t saved_time = 0;
    ret = nvs_get_u64(nvs_handle, "saved_time", &saved_time);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "No saved_time found in NVS, setting default value: %llu", saved_time);
        ESP_ERROR_CHECK(nvs_set_u64(nvs_handle, "saved_time", saved_time));
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading saved_time from NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    else
    {
        ESP_LOGI(TAG, "Read saved_time from NVS: %llu", saved_time);
    }

    // Инициализация device1
    size_t required_size_device1 = 0;
    ret = nvs_get_blob(nvs_handle, "device1", NULL, &required_size_device1);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'device1' not found in NVS. Creating with default values.");

        DeviceConfig default_config_device1 = {
            .id = "1", // Пустой ID
            .type = "",
            .x1 = 8,
            .y1 = 8,
            .x2 = 28,
            .y2 = 28};

        ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "device1", &default_config_device1, sizeof(DeviceConfig)));
        ESP_LOGI(TAG, "Key 'device1' successfully created with default values.");
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error checking key 'device1': %s", esp_err_to_name(ret));
        return ret;
    }
    else
    {
        ESP_LOGI(TAG, "Key 'device1' found in NVS. Data size: %zu bytes.", required_size_device1);
    }

    // Инициализация device2
    size_t required_size_device2 = 0;
    ret = nvs_get_blob(nvs_handle, "device2", NULL, &required_size_device2);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'device2' not found in NVS. Creating with default values.");

        DeviceConfig default_config_device2 = {
            .id = "2", // Пустой ID
            .type = "",
            .x1 = 8,
            .y1 = 8,
            .x2 = 28,
            .y2 = 28};

        ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "device2", &default_config_device2, sizeof(DeviceConfig)));
        ESP_LOGI(TAG, "Key 'device2' successfully created with default values.");
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error checking key 'device2': %s", esp_err_to_name(ret));
        return ret;
    }
    else
    {
        ESP_LOGI(TAG, "Key 'device2' found in NVS. Data size: %zu bytes.", required_size_device2);
    }

    bool need_update_common_settings = false;

    // Проверка sleep_enabled
    uint8_t sleep_enabled = 0;
    ret = nvs_get_u8(nvs_handle, "sleep_enabled", &sleep_enabled);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'sleep_enabled' not found in NVS. Setting default value: %d", default_common_settings.sleep_enabled);
        ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "sleep_enabled", default_common_settings.sleep_enabled ? 1 : 0));
        need_update_common_settings = true;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading 'sleep_enabled' from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Проверка sleep_seconds
    uint32_t sleep_seconds = 60 * 60 * 24;
    ret = nvs_get_u32(nvs_handle, "sleep_seconds", &sleep_seconds);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'sleep_seconds' not found in NVS. Setting default value: %lu", default_common_settings.sleep_seconds);
        ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "sleep_seconds", default_common_settings.sleep_seconds));
        need_update_common_settings = true;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading 'sleep_seconds' from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Проверка ocr_enabled
    uint8_t ocr_enabled = 0;
    ret = nvs_get_u8(nvs_handle, "ocr_enabled", &ocr_enabled);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'ocr_enabled' not found in NVS. Setting default value: %d", default_common_settings.ocr_enabled);
        ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "ocr_enabled", default_common_settings.ocr_enabled ? 1 : 0));
        need_update_common_settings = true;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading 'ocr_enabled' from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Проверка copy_to_server
    uint8_t copy_to_server = 0;
    ret = nvs_get_u8(nvs_handle, "copy_to_server", &copy_to_server);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'copy_to_server' not found in NVS. Setting default value: %d", default_common_settings.copy_to_server);
        ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "copy_to_server", default_common_settings.copy_to_server ? 1 : 0));
        need_update_common_settings = true;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading 'copy_to_server' from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Проверка server_path
    size_t required_size_server_path = 0;
    ret = nvs_get_str(nvs_handle, "server_path", NULL, &required_size_server_path);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'server_path' not found in NVS. Setting default value: '%s'", default_common_settings.server_path);
        ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "server_path", default_common_settings.server_path));
        need_update_common_settings = true;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading 'server_path' from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Проверка agc_gain
    uint16_t agc_gain = 0;
    ret = nvs_get_u16(nvs_handle, "agc_gain", &agc_gain);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'agc_gain' not found in NVS. Setting default value: %u", default_common_settings.agc_gain);
        ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "agc_gain", default_common_settings.agc_gain));
        need_update_common_settings = true;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading 'agc_gain' from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Проверка aec_value
    uint16_t aec_value = 0;
    ret = nvs_get_u16(nvs_handle, "aec_value", &aec_value);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'aec_value' not found in NVS. Setting default value: %u", default_common_settings.aec_value);
        ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "aec_value", default_common_settings.aec_value));
        need_update_common_settings = true;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading 'aec_value' from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Проверка flash_duty
    uint16_t flash_duty = 0;
    ret = nvs_get_u16(nvs_handle, "flash_duty", &flash_duty);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'flash_duty' not found in NVS. Setting default value: %u", default_common_settings.flash_duty);
        ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "flash_duty", default_common_settings.flash_duty));
        need_update_common_settings = true;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading 'flash_duty' from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Если были изменения, фиксируем их
    if (need_update_common_settings)
    {
        ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    }

    // Закрываем NVS
    nvs_close(nvs_handle);

    return ESP_OK;
}

uint16_t get_boot_count()
{
    uint16_t boot_count = 0; // Инициализация нулем
    nvs_handle_t nvs_handle;

    // Открытие NVS для чтения
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка открытия NVS");
        return 0; // Возвращаем 0 в случае ошибки
    }

    // Чтение значения 'bootCount' из NVS
    err = nvs_get_u16(nvs_handle, "bootCount", &boot_count);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Ключ 'bootCount' не найден в NVS");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка чтения 'bootCount' из NVS: %s", esp_err_to_name(err));
    }

    // Закрытие NVS-хендла
    nvs_close(nvs_handle);

    return boot_count;
}

// Функция сохранения общих настроек
esp_err_t save_common_settings_nvs(common_settings_t *settings)
{
    if (!settings)
    {
        ESP_LOGE(TAG, "Settings pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Открываем NVS
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Сохраняем значения
    err = nvs_set_u32(nvs_handle, "sleep_seconds", settings->sleep_seconds);
    if (err != ESP_OK)
        goto cleanup;

    err = nvs_set_u8(nvs_handle, "ocr_enabled", settings->ocr_enabled);
    if (err != ESP_OK)
        goto cleanup;

    err = nvs_set_u8(nvs_handle, "copy_to_server", settings->copy_to_server);
    if (err != ESP_OK)
        goto cleanup;

    err = nvs_set_str(nvs_handle, "server_path", settings->server_path);
    if (err != ESP_OK)
        goto cleanup;

    err = nvs_set_u8(nvs_handle, "sleep_enabled", settings->sleep_enabled);
    if (err != ESP_OK)
        goto cleanup;

    err = nvs_set_u16(nvs_handle, "agc_gain", settings->agc_gain);
    if (err != ESP_OK)
        goto cleanup;

    err = nvs_set_u16(nvs_handle, "aec_value", settings->aec_value);
    if (err != ESP_OK)
        goto cleanup;

    err = nvs_set_u16(nvs_handle, "flash_duty", settings->flash_duty);
    if (err != ESP_OK)
        goto cleanup;

    // Фиксируем изменения
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
        goto cleanup;

cleanup:
    nvs_close(nvs_handle);
    return err;
}

// Функция чтения общих настроек
esp_err_t read_common_settings_nvs(common_settings_t *settings)
{
    if (!settings)
    {
        ESP_LOGE(TAG, "Settings pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Инициализация значений по умолчанию
    settings->sleep_seconds = 60;    // По умолчанию 60 секунд
    settings->ocr_enabled = 0;       // По умолчанию выключено
    settings->copy_to_server = 0;    // По умолчанию выключено
    settings->server_path[0] = '\0'; // Пустая строка по умолчанию
    settings->sleep_enabled = 0;     // По умолчанию выключено
    settings->agc_gain = 15;         // По умолчанию 5
    settings->aec_value = 500;       // По умолчанию 500
    settings->flash_duty = 200;      // По умолчанию 20

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    bool has_error = false;

    // Чтение sleep_seconds
    err = nvs_get_u32(nvs_handle, "sleep_seconds", &settings->sleep_seconds);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'sleep_seconds' not found in NVS, using default value");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read sleep_seconds: %s", esp_err_to_name(err));
        has_error = true;
    }

    // Чтение ocr_enabled
    uint8_t temp;
    err = nvs_get_u8(nvs_handle, "ocr_enabled", &temp);
    settings->ocr_enabled = (err == ESP_OK && temp != 0);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'ocr_enabled' not found in NVS, using default value");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read ocr_enabled: %s", esp_err_to_name(err));
        has_error = true;
    }

    // Чтение copy_to_server
    err = nvs_get_u8(nvs_handle, "copy_to_server", &temp);
    settings->copy_to_server = (err == ESP_OK && temp != 0);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'copy_to_server' not found in NVS, using default value");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read copy_to_server: %s", esp_err_to_name(err));
        has_error = true;
    }

    // Чтение server_path
    size_t required_size = sizeof(settings->server_path);
    err = nvs_get_str(nvs_handle, "server_path", settings->server_path, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'server_path' not found in NVS, using default value");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read server_path: %s", esp_err_to_name(err));
        has_error = true;
    }
    else if (required_size >= sizeof(settings->server_path))
    {
        ESP_LOGE(TAG, "server_path too long, truncating or invalid data");
        has_error = true;
    }

    // Чтение sleep_enabled
    err = nvs_get_u8(nvs_handle, "sleep_enabled", &temp);
    settings->sleep_enabled = (err == ESP_OK && temp != 0);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'sleep_enabled' not found in NVS, using default value");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read sleep_enabled: %s", esp_err_to_name(err));
        has_error = true;
    }

    // Чтение новых параметров камеры
    err = nvs_get_u16(nvs_handle, "agc_gain", &settings->agc_gain);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'agc_gain' not found in NVS, using default value");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read agc_gain: %s", esp_err_to_name(err));
        has_error = true;
    }

    err = nvs_get_u16(nvs_handle, "aec_value", &settings->aec_value);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'aec_value' not found in NVS, using default value");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read aec_value: %s", esp_err_to_name(err));
        has_error = true;
    }

    err = nvs_get_u16(nvs_handle, "flash_duty", &settings->flash_duty);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'flash_duty' not found in NVS, using default value");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read flash_duty: %s", esp_err_to_name(err));
        has_error = true;
    }

    nvs_close(nvs_handle);

    return has_error ? ESP_FAIL : ESP_OK;
}

void print_system_time()
{
    time_t now;
    struct tm *utc_timeinfo, *local_timeinfo;

    // Получаем текущее время в формате Unix-времени
    time(&now);

    // Преобразуем в UTC
    utc_timeinfo = gmtime(&now);
    printf("UTC time: %s", asctime(utc_timeinfo));

    // Устанавливаем часовой пояс для Уфы (UTC+5)
    setenv("TZ", "UTC-5", 1);
    tzset();

    // Преобразуем в локальное время
    local_timeinfo = localtime(&now);
    printf("System time (UTC+5): %s", asctime(local_timeinfo));
}

// Функция синхронизации времени от SNTP
bool obtain_time_from_sntp()
{
    // Проверяем подключение к Wi-Fi
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
    {
        ESP_LOGW(TAG, "Wi-Fi is not connected, skipping SNTP synchronization");
        return false;
    }

    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setservername(0, "ntp2.stratum2.ru");
    esp_sntp_setservername(1, "ntp1.stratum2.ru");
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    esp_sntp_init();

    // Ожидаем синхронизации времени
    for (int retry = 1; retry <= SNTP_MAX_RETRIES; retry++)
    {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // Проверяем, что год больше или равен 2025
        if (timeinfo.tm_year >= (2025 - 1900))
        {
            // ESP_LOGI(TAG, "Current time: %s", asctime(&timeinfo));
            return true;
        }

        ESP_LOGI(TAG, "Waiting for time update... (%d/%d)", retry, SNTP_MAX_RETRIES);
        vTaskDelay(pdMS_TO_TICKS(SNTP_RETRY_DELAY_MS));
    }

    ESP_LOGW(TAG, "Failed to synchronize time with SNTP");
    return false;
}

// Функция для сохранения времени в NVS
esp_err_t save_time_to_nvs()
{
    uint32_t sleep_seconds = 0; // Инициализируем значение по умолчанию
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Открываем NVS для чтения и записи
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err; // Возвращаем ошибку, если не удалось открыть NVS
    }

    // Читаем значение sleep_seconds из NVS
    err = nvs_get_u32(nvs_handle, "sleep_seconds", &sleep_seconds);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Failed to read sleep_seconds from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err; // Возвращаем ошибку, если чтение завершилось неудачей
    }

    // Если ключ "sleep_seconds" не найден, используем значение по умолчанию (0)
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'sleep_seconds' not found in NVS. Using default value: 0");
        sleep_seconds = 0;
    }

    // Вычисляем предполагаемое время пробуждения
    uint64_t wakeup_time = time(NULL) + sleep_seconds;

    // Сохраняем время в NVS
    err = nvs_set_u64(nvs_handle, "saved_time", wakeup_time);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set 'saved_time' in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err; // Возвращаем ошибку, если запись завершилась неудачей
    }

    // Фиксируем изменения в NVS
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit changes to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err; // Возвращаем ошибку, если фиксация завершилась неудачей
    }

    // Закрываем NVS
    nvs_close(nvs_handle);

    // Логируем успешное сохранение
    ESP_LOGI(TAG, "Saved wakeup time: %llu", wakeup_time);

    return ESP_OK; // Возвращаем успех
}

esp_err_t restore_time_from_nvs()
{
    uint64_t saved_time = 0;
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Открываем NVS для чтения
    err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for reading: %s", esp_err_to_name(err));
        return err; // Возвращаем ошибку, если не удалось открыть NVS
    }

    // Читаем сохранённое время из NVS
    err = nvs_get_u64(nvs_handle, "saved_time", &saved_time);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Failed to read 'saved_time' from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err; // Возвращаем ошибку, если чтение завершилось неудачей
    }

    // Закрываем NVS
    nvs_close(nvs_handle);

    // Проверяем, что сохранённое время корректно
    if (err == ESP_ERR_NVS_NOT_FOUND || saved_time == 0)
    {
        ESP_LOGW(TAG, "No valid time found in NVS");
        return ESP_ERR_NOT_FOUND; // Возвращаем ошибку, если время не найдено или некорректно
    }

    // Устанавливаем системное время
    struct timeval tv = {.tv_sec = saved_time, .tv_usec = 0};
    err = settimeofday(&tv, NULL);
    if (err != 0)
    {
        ESP_LOGE(TAG, "Failed to set system time: %d", err);
        return ESP_FAIL; // Возвращаем ошибку, если установка времени не удалась
    }

    // Логируем успешное восстановление времени
    ESP_LOGI(TAG, "Restored time: %lld", saved_time);

    return ESP_OK; // Возвращаем успех
}

// Функция для загрузки конфигурации из NVS
esp_err_t load_device_config(const char *key, DeviceConfig *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка открытия NVS");
        return err;
    }

    // Определяем размер данных
    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, key, NULL, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка получения размера данных для ключа '%s': %s", key, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Читаем данные
    err = nvs_get_blob(nvs_handle, key, config, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка чтения данных для ключа '%s': %s", key, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);

    // Логирование загруженных данных
    // ESP_LOGI(TAG, "Загружена конфигурация для '%s': id=%s, x1=%" PRId32 ", y1=%" PRId32 ", x2=%" PRId32 ", y2=%" PRId32,
    // key, config->id, config->x1, config->y1, config->x2, config->y2);
    return ESP_OK;
}

// Функция обновления номера прибора в NVS
esp_err_t save_device_id(const char *key, const char *id, const char *type)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка открытия NVS");
        return err;
    }

    // Загружаем текущую конфигурацию
    DeviceConfig config;
    size_t required_size = sizeof(DeviceConfig);
    err = nvs_get_blob(nvs_handle, key, &config, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка чтения данных для ключа '%s': %s", key, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Обновляем id и type
    strncpy(config.id, id, sizeof(config.id) - 1);
    config.id[sizeof(config.id) - 1] = '\0'; // Гарантируем завершающий нуль
    strncpy(config.type, type, sizeof(config.type) - 1);
    config.type[sizeof(config.type) - 1] = '\0';

    // Сохраняем обновленную конфигурацию
    err = nvs_set_blob(nvs_handle, key, &config, sizeof(DeviceConfig));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка записи данных для ключа '%s'", key);
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка фиксации изменений в NVS");
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);

    return ESP_OK;
}

// Функция для обновления координат в NVS
esp_err_t save_coordinates(const char *key, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка открытия NVS");
        return err;
    }

    // Загружаем текущую конфигурацию
    DeviceConfig config;
    size_t required_size = sizeof(DeviceConfig);
    err = nvs_get_blob(nvs_handle, key, &config, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Ключ '%s' отсутствует в NVS. Создаем новую конфигурацию.", key);
        memset(&config, 0, sizeof(DeviceConfig));
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка чтения данных для ключа '%s': %s", key, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Обновляем координаты
    config.x1 = x1;
    config.y1 = y1;
    config.x2 = x2;
    config.y2 = y2;

    // Сохраняем обновленную конфигурацию
    err = nvs_set_blob(nvs_handle, key, &config, sizeof(DeviceConfig));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка записи данных для ключа '%s'", key);
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка фиксации изменений в NVS");
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Координаты успешно сохранены для ключа: %s", key);
    return ESP_OK;
}

// Функция для инициализации SPIFFS
esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_DIR,
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true};
    return esp_vfs_spiffs_register(&conf);
}

void format_spiffs()
{
    ESP_LOGI(TAG, "Форматирование SPIFFS...");
    esp_err_t ret = esp_spiffs_format(NULL);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SPIFFS успешно очищен!");
    }
    else
    {
        ESP_LOGE(TAG, "Ошибка форматирования SPIFFS: %s", esp_err_to_name(ret));
    }
}

void list_spiffs_files()
{
    DIR *dir = opendir(SPIFFS_DIR);
    if (!dir)
    {
        ESP_LOGE(TAG, "Ошибка открытия каталога");
        return;
    }

    struct dirent *entry;
    struct stat file_stat;
    char full_path[270];

    ESP_LOGI(TAG, "Файлы в /spiffs:");
    while ((entry = readdir(dir)) != NULL)
    {
        snprintf(full_path, sizeof(full_path), SPIFFS_DIR "/%.114s", entry->d_name);
        if (stat(full_path, &file_stat) == 0)
        {
            ESP_LOGI(TAG, "  %s (%ld байт)", entry->d_name, file_stat.st_size);
        }
    }
    closedir(dir);
}

// Функция сохранения в SPIFFS
esp_err_t save_to_spiffs(const char *dir, const char *filename, const uint8_t *data, size_t size)
{

    // Удаляем старые файлы из spiffs
    delete_old_files();

    if (!filename || !data || size == 0)
    {
        return ESP_ERR_INVALID_ARG; // Проверка аргументов
    }

    // Если директория не передана, используем значение по умолчанию
    if (dir == NULL)
    {
        dir = SPIFFS_DIR; // Используем SPIFFS_DIR как путь по умолчанию
    }

    // Буфер для полного пути
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir, filename);

    FILE *file = fopen(filepath, "wb");
    if (!file)
    {
        printf("SPIFFS: Файл не может быть записан: %s\n", filepath);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, size, file); // Записываем данные
    fclose(file);                                 // Закрываем файл

    if (written != size)
    {
        ESP_LOGE(TAG, "Ошибка записи в файл: %s\n", filepath);
        return ESP_FAIL;
    }

    // ESP_LOGI(TAG, "[%s] Файл сохранён: %s (%zu байт)\n",__func__, filepath, size);
    return ESP_OK;
}

// Функция чтения файла из SPIFFS
esp_err_t read_from_spiffs(const char *dir, const char *filename, uint8_t **buffer, size_t *size, const char *mode)
{
    // Проверка входных аргументов
    if (!dir || !filename || !buffer || !size || !mode)
    {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // Формируем путь к файлу
    char path[64];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);

    // Открываем файл с указанным режимом
    FILE *file = fopen(path, mode);
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open file '%s' with mode '%s'", path, mode);
        return ESP_FAIL;
    }

    // Проверяем, поддерживается ли режим чтения
    bool is_read_mode = (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0 ||
                         strcmp(mode, "r+") == 0 || strcmp(mode, "rb+") == 0);

    if (!is_read_mode)
    {
        ESP_LOGW(TAG, "Mode '%s' does not support reading. File opened for writing/appending.", mode);
        fclose(file);  // Закрываем файл
        return ESP_OK; // Возвращаем успех, так как ошибка не критична
    }

    // Получаем размер файла
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    if (*size == 0)
    {
        ESP_LOGE(TAG, "File '%s' is empty", path);
        fclose(file); // Закрываем файл
        return ESP_FAIL;
    }

    // Выделяем память под буфер
    *buffer = (uint8_t *)malloc(*size);
    if (!(*buffer))
    {
        ESP_LOGE(TAG, "Memory allocation failed for buffer of size %d", (int)*size);
        fclose(file); // Закрываем файл
        return ESP_FAIL;
    }

    // Читаем данные в буфер
    size_t bytes_read = fread(*buffer, 1, *size, file);
    if (bytes_read != *size)
    {
        ESP_LOGE(TAG, "Failed to read file '%s'. Expected %d bytes, but read %d bytes", path, (int)*size, (int)bytes_read);
        free(*buffer); // Освобождаем память
        fclose(file);  // Закрываем файл
        return ESP_FAIL;
    }

    // Закрываем файл
    fclose(file);

    // Логирование успешного чтения
    ESP_LOGI(TAG, "File '%s' read successfully, size: %d bytes", path, (int)*size);

    return ESP_OK;
}

/// Разбирает имя файла в структуру
bool parse_filename(const char *filename, file_info_t *file_info)
{
    return sscanf(filename, "%*u_%lu_%lu.jpg", &file_info->timestamp, &file_info->bootcount) == 2;
}
// Сортировка по timestamp, затем по bootcount (возрастание)
int compare_files(const void *a, const void *b)
{
    file_info_t *fa = (file_info_t *)a;
    file_info_t *fb = (file_info_t *)b;
    if (fa->timestamp != fb->timestamp)
        return fa->timestamp - fb->timestamp;
    return fa->bootcount - fb->bootcount;
}
// Функция для удаления старых файлов
void delete_old_files()
{
    DIR *dir = opendir(SPIFFS_DIR);
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", SPIFFS_DIR);
        return;
    }

    file_info_t *files = malloc(256 * sizeof(file_info_t));
    if (!files)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for files array");
        closedir(dir);
        return;
    }

    int file_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (file_count >= 256)
        {
            ESP_LOGW(TAG, "Too many files in directory. Some files may be skipped.");
            break;
        }

        file_info_t file_info;
        if (parse_filename(entry->d_name, &file_info))
        {
            strncpy(file_info.name, entry->d_name, sizeof(file_info.name) - 1);
            file_info.name[sizeof(file_info.name) - 1] = '\0';
            files[file_count++] = file_info;
        }
        else
        {
            char path[255];
            snprintf(path, sizeof(path), "%s/%.120s", SPIFFS_DIR, entry->d_name);
            if (unlink(path) == 0)
            {
                ESP_LOGI(TAG, "Deleted invalid file: %s", path);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to delete invalid file: %s. Error: %s", path, strerror(errno));
            }
        }
    }
    closedir(dir);

    ESP_LOGI(TAG, "File count %d/%d", file_count, MAX_FILES);

    if (file_count <= MAX_FILES)
    {
        ESP_LOGI(TAG, "File count does not exceed limit (%d/%d). No cleanup needed.", file_count, MAX_FILES);
        free(files);
        return;
    }

    qsort(files, file_count, sizeof(file_info_t), compare_files);

    int files_to_delete = file_count - MAX_FILES;
    for (int i = 0; i < files_to_delete; i++)
    {
        char path[128];
        snprintf(path, sizeof(path), "%s/%s", SPIFFS_DIR, files[i].name);
        if (unlink(path) == 0)
        {
            ESP_LOGI(TAG, "Deleted: %s", path);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to delete: %s. Error: %s", path, strerror(errno));
        }
    }

    free(files);
}

// Функция чтения файла в буфер
esp_err_t read_file_to_buffer(const char *filepath, uint8_t **buffer, size_t *file_size) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file '%s'", filepath);
        return ESP_FAIL;
    }

    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        ESP_LOGE(TAG, "Failed to get file stats for '%s'", filepath);
        fclose(f);
        return ESP_FAIL;
    }

    *file_size = st.st_size;
    *buffer = malloc(*file_size);
    if (!*buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for file '%s'", filepath);
        fclose(f);
        return ESP_FAIL;
    }

    size_t read_bytes = fread(*buffer, 1, *file_size, f);
    fclose(f);

    if (read_bytes != *file_size) {
        ESP_LOGE(TAG, "Failed to read file '%s' (%zu/%zu bytes)", filepath, read_bytes, *file_size);
        free(*buffer);
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Функция перехода в глубокий сон
void go_to_deep_sleep()
{
    // Чтение данных из NVS
    uint32_t sleep_seconds;
    uint8_t sleep_enabled;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return;
    }

    // Загружаем значение sleep_enabled
    err = nvs_get_u8(nvs_handle, "sleep_enabled", &sleep_enabled);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key 'sleep_enabled' not found in NVS. Using default value: false");
        sleep_enabled = false;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read sleep_enabled: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }

    // Загружаем значение sleep_seconds
    err = nvs_get_u32(nvs_handle, "sleep_seconds", &sleep_seconds);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read sleep_seconds from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }

    nvs_close(nvs_handle);

    // Проверка значения sleep_seconds
    if (sleep_seconds <= 0)
    {
        ESP_LOGE(TAG, "Invalid sleep duration: %lu seconds. Deep sleep aborted.", sleep_seconds);
        return;
    }

    // Логирование
    ESP_LOGI(TAG, "Configuring deep sleep for %lu seconds...", sleep_seconds);

    // Переводим sleep_seconds в микросекунды
    uint64_t sleep_duration_us = (uint64_t)sleep_seconds * 1000000;

    // Настройка пробуждения по таймеру
    esp_err_t timer_result = esp_sleep_enable_timer_wakeup(sleep_duration_us);
    if (timer_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable timer wakeup: %s", esp_err_to_name(timer_result));
        return;
    }

    // Сохранение времени в NVS (если необходимо)
    esp_err_t save_result = save_time_to_nvs();
    if (save_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save time to NVS: %s", esp_err_to_name(save_result));
        return;
    }

    if (sleep_enabled == 1)
    {
        // Деинициализация камеры перед сном
        esp_err_t err = esp_camera_deinit();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to deinitialize camera: %s", esp_err_to_name(err));
        }
        // Остановка HTTP-сервера перед сном, если он не был уже остановлен
        if (server != NULL)
        {
            stop_webserver();
        }
        else
        {
            ESP_LOGW(TAG, "HTTP server is already stopped.");
        }
        ESP_LOGI(TAG, "Entering deep sleep mode for %lu seconds...", sleep_seconds);
        esp_deep_sleep_start(); // Переход в глубокий сон
    }
    else
    {
        ESP_LOGI(TAG, "Deep sleep is disabled. Aborting...");
    }
}
