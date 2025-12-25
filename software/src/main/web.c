//software/src/main/web.c

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include "esp_log.h"
#include "cJSON.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"

#include "utils.h"
#include "web.h"
#include "my_img.h"

#define TAG "WEB_SERVER"

httpd_handle_t server = NULL; // Инициализация дескриптора сервера
int client_fd = -1;           // Инициализация дескриптора клиента для WebSocket
TimerHandle_t inactivity_timer = NULL;

static char buffer[1024]; // для логов

// Функция для отправки ответа с кодом 400 (Bad Request)
esp_err_t httpd_resp_send_400(httpd_req_t *req)
{
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    const char *response = "{\"status\":\"error\",\"message\":\"Bad Request\"}";
    return httpd_resp_send(req, response, strlen(response));
}

esp_err_t send_405_response(httpd_req_t *req)
{
    httpd_resp_set_status(req, "405 Method Not Allowed"); // Устанавливаем статус 405 Method Not Allowed
    httpd_resp_set_hdr(req, "Allow", "POST");             // Добавляем заголовок Allow, чтобы указать допустимые методы
    httpd_resp_send(req, NULL, 0);                        // Отправляем пустое тело ответа
    return ESP_OK;
}

// Функция для перезапуска таймера неактивности клиента
void reset_inactivity_timer(void)
{
    if (inactivity_timer != NULL)
    {
        // ESP_LOGI(TAG, "Resetting inactivity timer...");
        xTimerReset(inactivity_timer, 0); // Перезапускаем таймер
    }
}

// Обработчик главной страницы
static esp_err_t root_handler(httpd_req_t *req)
{

    // Сбрасываем таймер бездействия
    reset_inactivity_timer();

    httpd_resp_set_type(req, "text/html");              // Устанавливаем тип контента
    httpd_resp_send(req, home_html, strlen(home_html)); // Отправляем статический HTML
    return ESP_OK;
}

// Обработчик запроса настроек
static esp_err_t load_settings_handler(httpd_req_t *req)
{
    // Сбрасываем таймер бездействия
    reset_inactivity_timer();

    // Загружаем данные для device1 и device2
    DeviceConfig device1, device2;
    esp_err_t err1 = load_device_config("device1", &device1);
    esp_err_t err2 = load_device_config("device2", &device2);

    // Проверяем успешность загрузки данных для device1 и device2
    if (err1 != ESP_OK || err2 != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load device config: device1=%s, device2=%s",
                 esp_err_to_name(err1), esp_err_to_name(err2));
        httpd_resp_send_500(req); // Возвращаем ошибку 500 Internal Server Error
        return ESP_FAIL;
    }

    // Загружаем общие настройки
    common_settings_t common_settings;
    esp_err_t err = read_common_settings_nvs(&common_settings);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read common settings");
        httpd_resp_send_500(req); // Возвращаем ошибку 500 Internal Server Error
        return ESP_FAIL;
    }

    // Создаем JSON с данными
    cJSON *root = cJSON_CreateObject();

    // Добавляем данные для device1
    cJSON_AddStringToObject(root, "device1_id", device1.id);
    cJSON_AddStringToObject(root, "device1_type", device1.type);
    cJSON_AddNumberToObject(root, "device1_x1", device1.x1);
    cJSON_AddNumberToObject(root, "device1_y1", device1.y1);
    cJSON_AddNumberToObject(root, "device1_x2", device1.x2);
    cJSON_AddNumberToObject(root, "device1_y2", device1.y2);

    // Добавляем данные для device2
    cJSON_AddStringToObject(root, "device2_id", device2.id);
    cJSON_AddStringToObject(root, "device2_type", device2.type);
    cJSON_AddNumberToObject(root, "device2_x1", device2.x1);
    cJSON_AddNumberToObject(root, "device2_y1", device2.y1);
    cJSON_AddNumberToObject(root, "device2_x2", device2.x2);
    cJSON_AddNumberToObject(root, "device2_y2", device2.y2);

    // Добавляем общие настройки
    cJSON_AddBoolToObject(root, "sleep_enabled", common_settings.sleep_enabled);
    cJSON_AddNumberToObject(root, "sleep_seconds", common_settings.sleep_seconds);
    cJSON_AddBoolToObject(root, "ocr_enabled", common_settings.ocr_enabled);
    cJSON_AddBoolToObject(root, "copy_to_server", common_settings.copy_to_server);
    cJSON_AddStringToObject(root, "server_path", common_settings.server_path);

    // Добавляем новые параметры камеры
    cJSON_AddNumberToObject(root, "agc_gain", common_settings.agc_gain);
    cJSON_AddNumberToObject(root, "aec_value", common_settings.aec_value);
    cJSON_AddNumberToObject(root, "flash_duty", common_settings.flash_duty);

    // Преобразуем JSON в строку
    char *json_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    // Отправляем JSON-ответ
    httpd_resp_set_type(req, "application/json");       // Устанавливаем тип контента
    httpd_resp_send(req, json_data, strlen(json_data)); // Отправляем JSON
    free(json_data);

    return ESP_OK;
}

// Обработчик  снимка для настройки
static esp_err_t take_init_image_handler(httpd_req_t *req)
{
    // Сбрасываем таймер бездействия
    reset_inactivity_timer();

    // Задаем ROI
    int x1 = 0;
    int y1 = 0;
    int x2 = 1600;
    int y2 = 600;
    // Захват изображения
    camera_fb_t *frame = capture_image(60, x1, y1, x2, y2);
    if (frame)
    {
        ESP_LOGI(TAG, "Init image captured, size: %zu bytes", frame->len);
        // Отправляем HTTP-заголовки
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Expires", "0");
        // Отправляем данные изображения
        httpd_resp_send(req, (const char *)frame->buf, frame->len);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to capture frame");
        httpd_resp_send_500(req); // Возвращаем ошибку 500
    }
    // Освобождаем буфер
    esp_camera_fb_return(frame);
    return ESP_OK;
}

// Обработчик POST-запроса для save_common_settings
esp_err_t save_common_settings_handler(httpd_req_t *req)
{
    reset_inactivity_timer();

    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0 || ret >= sizeof(buf) - 1)
    {
        ESP_LOGE(TAG, "Failed to read request body");
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    common_settings_t current_settings;
    esp_err_t err = read_common_settings_nvs(&current_settings);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read current settings from NVS: %s", esp_err_to_name(err));
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    bool any_changes = false;

    // Макрос для обновления числовых полей
#define UPDATE_FIELD(json_key, field, type)                                          \
    do                                                                               \
    {                                                                                \
        cJSON *json_item = cJSON_GetObjectItem(root, json_key);                      \
        if (json_item && cJSON_IsNumber(json_item))                                  \
        {                                                                            \
            type new_value = json_item->valueint;                                    \
            if (current_settings.field != new_value)                                 \
            {                                                                        \
                current_settings.field = new_value;                                  \
                ESP_LOGI(TAG, "Updated %s=%" PRIu32, json_key, (uint32_t)new_value); \
                any_changes = true;                                                  \
            }                                                                        \
        }                                                                            \
    } while (0)

    // Макрос для обновления булевых полей (0 или 1)
#define UPDATE_BOOL_FIELD(json_key, field)                                                        \
    do                                                                                            \
    {                                                                                             \
        cJSON *json_item = cJSON_GetObjectItem(root, json_key);                                   \
        if (json_item && cJSON_IsNumber(json_item))                                               \
        {                                                                                         \
            uint8_t new_value = json_item->valueint;                                              \
            if (new_value == 0 || new_value == 1)                                                 \
            {                                                                                     \
                if (current_settings.field != new_value)                                          \
                {                                                                                 \
                    current_settings.field = new_value;                                           \
                    ESP_LOGI(TAG, "Updated %s=%u", json_key, new_value);                          \
                    any_changes = true;                                                           \
                }                                                                                 \
            }                                                                                     \
            else                                                                                  \
            {                                                                                     \
                ESP_LOGW(TAG, "Invalid value for %s: %u (expected 0 or 1)", json_key, new_value); \
            }                                                                                     \
        }                                                                                         \
    } while (0)

    // Обновляем числовые поля
    UPDATE_FIELD("sleep_seconds", sleep_seconds, uint32_t);
    UPDATE_FIELD("agc_gain", agc_gain, uint16_t);
    UPDATE_FIELD("aec_value", aec_value, uint16_t);
    UPDATE_FIELD("flash_duty", flash_duty, uint16_t);

    // Обновляем булевы поля
    UPDATE_BOOL_FIELD("ocr_enabled", ocr_enabled);
    UPDATE_BOOL_FIELD("copy_to_server", copy_to_server);
    UPDATE_BOOL_FIELD("sleep_enabled", sleep_enabled);

    // Обновляем строковое поле server_path
    cJSON *json_item = cJSON_GetObjectItem(root, "server_path");
    if (json_item && cJSON_IsString(json_item))
    {
        const char *new_server_path = json_item->valuestring;

        // Проверяем длину строки
        if (strlen(new_server_path) >= sizeof(current_settings.server_path))
        {
            ESP_LOGE(TAG, "New server_path is too long");
            cJSON_Delete(root);
            httpd_resp_send_400(req);
            return ESP_FAIL;
        }

        // Сравниваем строки
        if (strcmp(current_settings.server_path, new_server_path) != 0)
        {
            strncpy(current_settings.server_path, new_server_path, sizeof(current_settings.server_path) - 1);
            current_settings.server_path[sizeof(current_settings.server_path) - 1] = '\0';
            ESP_LOGI(TAG, "Updated server_path=%s", current_settings.server_path);
            any_changes = true;
        }
    }

    // Валидация данных
    if (current_settings.copy_to_server && strlen(current_settings.server_path) == 0)
    {
        ESP_LOGE(TAG, "Server path is required when 'copy_to_server' is enabled");
        cJSON_Delete(root);
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    // Сохраняем изменения в NVS, только если были обновления
    if (any_changes)
    {
        err = save_common_settings_nvs(&current_settings);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to save common settings: %s", esp_err_to_name(err));
            cJSON_Delete(root);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGI(TAG, "No changes detected, skipping NVS save");
    }

    // Отправляем успешный ответ
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\": \"success\", \"message\": \"Settings updated successfully\"}");
    cJSON_Delete(root);
    return ESP_OK;
}

// Обработчик POST-запроса для сохранения Device ID
esp_err_t save_device_id_handler(httpd_req_t *req)
{
    // Сбрасываем таймер бездействия
    reset_inactivity_timer();

    // Определяем буфер для чтения тела запроса
    char buf[512];
    memset(buf, 0, sizeof(buf));

    // Читаем тело запроса
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
    {
        ESP_LOGE(TAG, "Ошибка чтения тела запроса");
        httpd_resp_send_400(req); // Возвращаем ошибку 400 Bad Request
        return ESP_FAIL;
    }
    buf[ret] = '\0'; // Завершаем строку

    // Логируем запрос
    // ESP_LOGW(TAG, "запрос %s", buf);

    // Парсим JSON из тела запроса
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "Ошибка парсинга JSON");
        httpd_resp_send_400(req); // Возвращаем ошибку 400 Bad Request
        return ESP_FAIL;
    }

    // Извлекаем значения из JSON
    const cJSON *key_item = cJSON_GetObjectItemCaseSensitive(json, "key");
    const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(json, "id");
    const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(json, "type");

    if (!cJSON_IsString(key_item) || !cJSON_IsString(id_item) || !cJSON_IsString(type_item))
    {
        ESP_LOGE(TAG, "Ошибка при извлечении параметров из JSON");
        cJSON_Delete(json);
        httpd_resp_send_400(req); // Возвращаем ошибку 400 Bad Request
        return ESP_FAIL;
    }

    // Логируем полученные параметры
    // ESP_LOGI(TAG, "Получены параметры: key=%s, id=%s, type=%s", key_item->valuestring, id_item->valuestring, type_item->valuestring);

    // Сохраняем id и type под указанным ключом
    esp_err_t err = save_device_id(key_item->valuestring, id_item->valuestring, type_item->valuestring);
    cJSON_Delete(json); // Освобождаем память

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка сохранения id и type в NVS");
        httpd_resp_send_500(req); // Возвращаем ошибку 500 Internal Server Error
        return ESP_FAIL;
    }

    // Отправляем успешный ответ в JSON
    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "status", "success");
    cJSON_AddStringToObject(response_json, "key", key_item->valuestring);

    char *response_str = cJSON_PrintUnformatted(response_json);
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));
    cJSON_Delete(response_json); // Освобождаем память
    free(response_str);

    return ESP_OK;
}

// Обработчик для добавления координат
esp_err_t save_coordinates_handler(httpd_req_t *req)
{
    // Сбрасываем таймер бездействия
    reset_inactivity_timer();

    // Определяем буфер для чтения тела запроса
    char buf[512];
    memset(buf, 0, sizeof(buf));

    // Читаем тело запроса
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
    {
        httpd_resp_send_400(req); // Возвращаем ошибку, если тело запроса пустое
        return ESP_FAIL;
    }

    // Парсим JSON
    cJSON *json = cJSON_Parse(buf);
    if (!json)
    {
        httpd_resp_send_400(req); // Возвращаем ошибку, если JSON некорректен
        return ESP_FAIL;
    }

    // Извлекаем данные из JSON
    const cJSON *key = cJSON_GetObjectItemCaseSensitive(json, "device");
    const cJSON *x1 = cJSON_GetObjectItemCaseSensitive(json, "x1");
    const cJSON *y1 = cJSON_GetObjectItemCaseSensitive(json, "y1");
    const cJSON *x2 = cJSON_GetObjectItemCaseSensitive(json, "x2");
    const cJSON *y2 = cJSON_GetObjectItemCaseSensitive(json, "y2");

    if (!cJSON_IsString(key) || !cJSON_IsNumber(x1) || !cJSON_IsNumber(y1) ||
        !cJSON_IsNumber(x2) || !cJSON_IsNumber(y2))
    {
        cJSON_Delete(json);       // Освобождаем память
        httpd_resp_send_400(req); // Возвращаем ошибку, если данные некорректны
        return ESP_FAIL;
    }

    // Преобразуем данные в нужные типы
    char device_key[20];
    strncpy(device_key, key->valuestring, sizeof(device_key) - 1);
    int32_t x1_val = (int32_t)x1->valuedouble;
    int32_t y1_val = (int32_t)y1->valuedouble;
    int32_t x2_val = (int32_t)x2->valuedouble;
    int32_t y2_val = (int32_t)y2->valuedouble;

    cJSON_Delete(json); // Освобождаем память

    // Добавляем координаты в NVS
    esp_err_t err = save_coordinates(device_key, x1_val, y1_val, x2_val, y2_val);
    if (err != ESP_OK)
    {
        httpd_resp_send_500(req); // Возвращаем ошибку сервера
        return ESP_FAIL;
    }

    // Отправляем успешный ответ
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    const char *response = "{\"status\":\"success\"}";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Обработчик POST-запроса для проверки захвата и сохранения изображений
esp_err_t process_handler(httpd_req_t *req)
{
    // Сбрасываем таймер бездействия
    reset_inactivity_timer();

    // Получаем тело запроса
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0 || ret >= sizeof(buf) - 1)
    {
        ESP_LOGE(TAG, "Invalid request data size");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"success\": false, \"message\": \"Invalid request data\"}");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Парсим JSON
    cJSON *json = cJSON_Parse(buf);
    if (!json)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr)
        {
            ESP_LOGE(TAG, "Failed to parse JSON at position: %s", error_ptr);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to parse JSON: Unknown error");
        }
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"success\": false, \"message\": \"Invalid JSON\"}");
        return ESP_FAIL;
    }

    // Извлекаем поле "action"
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");

    if (action && cJSON_IsString(action))
    {
        if (strcmp(action->valuestring, "process_enabled") == 0)
        {
            ESP_LOGI(TAG, "[%s] Enabling processing.", __func__);
        }
        else
        {
            ESP_LOGI(TAG, "[%s] Disabling processing.", __func__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "[%s] Missing or invalid 'action' field in JSON", __func__);
        cJSON_Delete(json);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"success\": false, \"message\": \"Missing or invalid 'action' field\"}");
        return ESP_FAIL;
    }
    cJSON_Delete(json);

    // Вызываем process_images для обработки изображений
    char **filenames = NULL;
    esp_err_t process_result = process_images(&filenames);
    if (process_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Ошибка при обработке изображений");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"success\": false, \"message\": \"Image processing failed\"}");
        return ESP_FAIL;
    }

    // Создаем JSON-ответ
    cJSON *root = cJSON_CreateObject();
    cJSON *devices_object = cJSON_CreateObject(); // Объект для хранения данных по device_keys
    bool at_least_one_success = false;
    for (size_t i = 0; i < num_devices; i++)
    {
        const char *device = device_keys[i];
        char *filename = filenames[i];
        if (!filename || strlen(filename) == 0)
        {
            ESP_LOGW(TAG, "File name is empty or null for device %s", device);
            continue;
        }

        // Формируем полный путь к файлу
        char full_path[64];
        snprintf(full_path, sizeof(full_path), "%s/%s", SPIFFS_DIR, filename);

        // Проверяем корректность файла
        struct stat st;
        if (stat(full_path, &st) != 0)
        {
            ESP_LOGE(TAG, " [%s] Failed to access file '%s': %s", __func__, full_path, strerror(errno));
            continue;
        }
        if (st.st_size == 0)
        {
            ESP_LOGE(TAG, "File '%s' is empty", full_path);
            continue;
        }

        // Читаем содержимое файла
        FILE *f = fopen(full_path, "rb");
        if (!f)
        {
            ESP_LOGE(TAG, "Failed to open file '%s'", full_path);
            continue;
        }
        uint8_t *file_data = malloc(st.st_size);
        if (!file_data)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for file '%s'", full_path);
            fclose(f);
            continue;
        }
        size_t read_bytes = fread(file_data, 1, st.st_size, f);
        fclose(f);
        if (read_bytes != st.st_size)
        {
            ESP_LOGE(TAG, "Failed to read file '%s'", full_path);
            free(file_data);
            continue;
        }

        // Извлечение пользовательских данных из JPEG
        cJSON *user_data = read_json_after_eoi(file_data, read_bytes);
        if (!user_data)
        {
            ESP_LOGW(TAG, "No user data found in file '%s'", full_path);
        }

        // Кодирование изображения в Base64
        size_t base64_len = ((read_bytes + 2) / 3) * 4 + 1; // Размер Base64
        char *base64_data = malloc(base64_len);
        if (!base64_data)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for Base64 encoding");
            free(file_data);
            if (user_data)
                cJSON_Delete(user_data);
            continue;
        }
        size_t out_len;
        mbedtls_base64_encode((unsigned char *)base64_data, base64_len, &out_len, file_data, read_bytes);
        free(file_data);

        // Создаем JSON-данные для устройства
        cJSON *device_data = cJSON_CreateObject();
        cJSON_AddStringToObject(device_data, "device_image", base64_data);
        free(base64_data);
        if (user_data)
        {
            cJSON_AddItemToObject(device_data, "device_data", user_data);
        }

        // Добавляем данные устройства в объект devices_object
        cJSON_AddItemToObject(devices_object, device, device_data);
        at_least_one_success = true;
    }

    // Если хотя бы одно изображение успешно обработано
    if (at_least_one_success)
    {
        cJSON_AddItemToObject(root, "devices", devices_object);
        char *response_data = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        if (!response_data)
        {
            ESP_LOGE(TAG, "Failed to generate JSON response");
            return ESP_FAIL;
        }

        // Отправляем JSON-ответ
        httpd_resp_set_type(req, "application/json");
        if (httpd_resp_send(req, response_data, strlen(response_data)) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send HTTP response");
            free(response_data);
        }
        else
        {
            free(response_data);
        }
    }
    else
    {
        // Если ни одно изображение не обработано
        cJSON_Delete(root);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"success\": false, \"message\": \"All image processing attempts failed\"}");
    }

    // Освобождаем память для имен файлов
    if (filenames)
    {
        for (size_t i = 0; i < num_devices; i++)
        {
            free(filenames[i]);
        }
        free(filenames);
    }
    return ESP_OK;
}

static esp_err_t send_http_request(const char *server_url, const char *filename)
{
    char full_path[64];
    snprintf(full_path, sizeof(full_path), "%s/%s", SPIFFS_DIR, filename);

    // Check file existence and size
    struct stat st;
    if (stat(full_path, &st) != 0 || st.st_size == 0)
    {
        ESP_LOGE(TAG, "File '%s' does not exist or is empty", full_path);
        return ESP_FAIL;
    }

    // Read file into buffer
    uint8_t *file_data = NULL;
    size_t file_size = 0;
    esp_err_t err = read_file_to_buffer(full_path, &file_data, &file_size);
    if (err != ESP_OK)
    {
        return err;
    }

    // Initialize HTTP client
    esp_http_client_config_t http_config = {
        .url = server_url,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(file_data);
        return ESP_FAIL;
    }

    // Set headers and open connection
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    char content_disposition[128];
    snprintf(content_disposition, sizeof(content_disposition), "attachment; filename=\"%s\"", filename);
    esp_http_client_set_header(client, "Content-Disposition", content_disposition);

    err = esp_http_client_open(client, file_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(file_data);
        return err;
    }

    // Write file data to server
    int written_bytes = esp_http_client_write(client, (char *)file_data, file_size);
    if (written_bytes < 0 || (size_t)written_bytes != file_size)
    {
        ESP_LOGE(TAG, "Failed to write file data to server (%d/%zu bytes)", written_bytes, file_size);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(file_data);
        return ESP_FAIL;
    }

    // Get response status
    int http_status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP status code: %d", http_status);

    // Handle response body
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length > 0)
    {
        char *response_buffer = malloc(content_length + 1);
        if (response_buffer)
        {
            int bytes_read = esp_http_client_read_response(client, response_buffer, content_length);
            if (bytes_read > 0)
            {
                response_buffer[bytes_read] = '\0'; // Завершаем строку
                ESP_LOGI(TAG, "Server response: %s", response_buffer);

                // Parse JSON response
                cJSON *root = cJSON_Parse(response_buffer);
                if (root)
                {
                    cJSON *status = cJSON_GetObjectItem(root, "status");
                    if (status && cJSON_IsString(status) && strcmp(status->valuestring, "success") == 0)
                    {
                        ESP_LOGI(TAG, "File was saved successfully despite HTTP status %d", http_status);
                        cJSON_Delete(root);
                        free(response_buffer);
                        esp_http_client_close(client);
                        esp_http_client_cleanup(client);
                        free(file_data);
                        return ESP_OK; // Считаем операцию успешной
                    }
                    cJSON_Delete(root);
                }
            }
            free(response_buffer);
        }
    }

    // If HTTP status is not 200 and no success in JSON, log error
    if (http_status != 200)
    {
        ESP_LOGE(TAG, "Server returned error status: %d", http_status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(file_data);
        return ESP_FAIL;
    }

    // Cleanup
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(file_data);

    return ESP_OK;
}

esp_err_t send_file_to_server(const char *server_url, const char *filename)
{
    if (!server_url || !filename)
    {
        ESP_LOGE(TAG, "Invalid arguments: server_url or filename is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Retry logic with exponential backoff
    for (int attempt = 1; attempt <= 2; attempt++)
    {
        esp_err_t result = send_http_request(server_url, filename);
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "File '%s' uploaded successfully on attempt %d", filename, attempt);
            return ESP_OK;
        }

        ESP_LOGW(TAG, "Attempt %d failed, retrying...", attempt);
        vTaskDelay(pdMS_TO_TICKS(1000 << (attempt - 1))); // Exponential backoff
    }

    ESP_LOGE(TAG, "Failed to upload file '%s' after %d attempts", filename, 2);
    return ESP_FAIL;
}

// Обработчик WebSocket
static esp_err_t websocket_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        // Обработка нового подключения WebSocket
        client_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "Client connected");

        // Сбрасываем таймер бездействия
        reset_inactivity_timer();
    }
    else if (req->method == HTTP_POST)
    {
        // Обработка входящих сообщений от клиента
        ESP_LOGI(TAG, "Message received from client");

        // Сбрасываем таймер бездействия
        reset_inactivity_timer();
    }
    else if (req->method == HTTP_DELETE)
    {
        // Обработка отключения клиента
        if (client_fd != -1)
        {
            client_fd = -1;
            ESP_LOGI(TAG, "Client disconnected. Entering deep sleep...");

            // Переход в режим глубокого сна
            go_to_deep_sleep();
        }
        else
        {
            ESP_LOGW(TAG, "Attempted to disconnect non-existent client");
        }
    }
    return ESP_OK;
}

// Инициализация HTTP-сервера
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;         // Увеличиваем стек для WebSocket
    config.lru_purge_enable = true;   // Включить очистку неактивных соединений
    config.keep_alive_enable = false; // Отключить Keep-Alive
    config.recv_wait_timeout = 5;     // Таймаут ожидания данных (в секундах)
    config.send_wait_timeout = 5;     // Таймаут отправки данных (в секундах)
    config.max_open_sockets = 8;      // Максимальное количество открытых сокетов

    if (httpd_start(&server, &config) == ESP_OK)
    {
        esp_err_t err;

        // Регистрация корневого обработчика "/"
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL};
        err = httpd_register_uri_handler(server, &root_uri);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register root handler: %s", esp_err_to_name(err));
        }

        // Регистрация обработчика загрузки настроек "/load_settings"
        httpd_uri_t load_settings_uri = {
            .uri = "/load_settings",
            .method = HTTP_GET,
            .handler = load_settings_handler,
            .user_ctx = NULL};
        err = httpd_register_uri_handler(server, &load_settings_uri);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register load_settings handler: %s", esp_err_to_name(err));
        }

        // Регистрация обработчика захвата изображения "/take_init_image"
        httpd_uri_t take_init_image_uri = {
            .uri = "/take_init_image",
            .method = HTTP_GET,
            .handler = take_init_image_handler,
            .user_ctx = NULL};
        err = httpd_register_uri_handler(server, &take_init_image_uri);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register take_init_image handler: %s", esp_err_to_name(err));
        }

        // Регистрация обработчика сохранения общих настроек "/save_common_settings"
        httpd_uri_t save_common_settings_uri = {
            .uri = "/save_common_settings",
            .method = HTTP_POST,
            .handler = save_common_settings_handler,
            .user_ctx = NULL};
        err = httpd_register_uri_handler(server, &save_common_settings_uri);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register save_common_settings handler: %s", esp_err_to_name(err));
        }

        // Регистрация обработчика сохранения ID устройства "/save_device_id"
        httpd_uri_t save_device_id_uri = {
            .uri = "/save_device_id",
            .method = HTTP_POST,
            .handler = save_device_id_handler,
            .user_ctx = NULL};
        err = httpd_register_uri_handler(server, &save_device_id_uri);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register save_device_id handler: %s", esp_err_to_name(err));
        }

        // Регистрация обработчика сохранения координат "/save_coordinates"
        httpd_uri_t add_coordinates_uri = {
            .uri = "/save_coordinates",
            .method = HTTP_POST,
            .handler = save_coordinates_handler,
            .user_ctx = NULL};
        err = httpd_register_uri_handler(server, &add_coordinates_uri);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register save_coordinates handler: %s", esp_err_to_name(err));
        }

        // Регистрация обработчика проверки для захвата изображений "/get_images"
        httpd_uri_t test_uri = {
            .uri = "/get_images",
            .method = HTTP_POST,
            .handler = process_handler,
            .user_ctx = NULL};
        err = httpd_register_uri_handler(server, &test_uri);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register get_images handler: %s", esp_err_to_name(err));
        }

        // Регистрация WebSocket URI "/ws"
        httpd_uri_t ws_uri = {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = websocket_handler,
            .user_ctx = NULL,
            .is_websocket = true};
        err = httpd_register_uri_handler(server, &ws_uri);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register WebSocket handler: %s", esp_err_to_name(err));
        }
    }
    return server;
}

// Остановка HTTP-сервера
void stop_webserver(void)
{
    if (server)
    {
        ESP_LOGI(TAG, "Stopping HTTP server...");
        httpd_stop(server);
        server = NULL; // Обнуляем дескриптор после остановки
    }
    else
    {
        ESP_LOGW(TAG, "HTTP server is already stopped");
    }
}

// Функция для отправки логов через WebSocket
void log_to_websocket(const char *message)
{
    if (server != NULL && client_fd >= 0)
    {
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        ws_pkt.final = true;
        ws_pkt.len = strlen(message);
        ws_pkt.payload = (uint8_t *)message;

        // Проверяем результат отправки
        esp_err_t err = httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send WebSocket frame: %s", esp_err_to_name(err));
            client_fd = -1; // Сбрасываем client_fd
        }
    }
    else
    {
        ESP_LOGE(TAG, "Invalid client socket or server is NULL. Unable to send message.");
    }
}

static bool is_logging = false;

// Пользовательская функция вывода логов
int custom_log_vprintf(const char *format, va_list args)
{
    if (is_logging)
    {
        return 0; // Пропускаем логирование, если уже выполняется
    }

    is_logging = true;

    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    printf("%s", buffer);
    log_to_websocket(buffer);

    is_logging = false;
    return len;
}

// Регистрация логера
void register_custom_logger()
{
    esp_log_set_vprintf(custom_log_vprintf);
}

// Обработчик истечения таймера
void inactivity_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "No activity detected. Scheduling deep sleep task...");
    go_to_deep_sleep();
}
