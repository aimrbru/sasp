//software/src/main/my_img.c

#include "my_img.h"
#include "utils.h"
#include "web.h"
#include "esp_system.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "dirent.h"
#include <inttypes.h>

#include "driver/gpio.h"

static const char *TAG = "MY_IMG";

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27
#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, // YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_UXGA,   // QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 20, // 0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,      // When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
};

#ifndef SAFE_FREE
#define SAFE_FREE(ptr) \
    do                 \
    {                  \
        free(ptr);     \
        ptr = NULL;    \
    } while (0)
#endif

#define OCR_API_URL "http://ocr.local"
#define API_KEY "06fc7973b5eda6ed989"

#define MAX_RETRIES 3
#define RETRY_DELAY_MS 2000 // 2 секунды задержки между попытками
#define HTTP_TIMEOUT_MS 60000

#define MAX_HTTP_OUTPUT_BUFFER 1024
#define MAX_TASK_CHECKS 10

// Функция инициализации камеры
esp_err_t init_camera(void)
{
    // Инициализация камеры
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Дать время камере стабилизироваться

    sensor_t *s = esp_camera_sensor_get(); // Получаем объект сенсора
    if (s)
    {
        s->set_gain_ctrl(s, 0);     // Отключаем авто-усиление (AGC)
        s->set_exposure_ctrl(s, 0); // Отключаем авто-экспозицию (AEC)
        s->set_saturation(s, -1);   // Устанавливает насыщенность цветов (-2..2)
        s->set_denoise(s, 1);       // Включает или выключает шумоподавление
    }

    // Настройка таймера
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,   // Высокоскоростной режим
        .timer_num = LEDC_TIMER_0,            // Используем таймер 0
        .duty_resolution = LEDC_TIMER_10_BIT, // Разрешение 10 бит (1024 уровня яркости)
        .freq_hz = 10000,                     // Частота ШИМ  Гц
        .clk_cfg = LEDC_USE_APB_CLK,          // Источник тактового сигнала - APB
    };
    ledc_timer_config(&ledc_timer);

    // Настройка канала для встроенной вспышки
    ledc_channel_config_t ledc_channel = {
        .gpio_num = 4,                      // Пин для ШИМ (GPIO4 — встроенная вспышка на ESP32-CAM)
        .speed_mode = LEDC_HIGH_SPEED_MODE, // Высокоскоростной режим
        .channel = LEDC_CHANNEL_0,          // Канал 0
        .intr_type = LEDC_INTR_DISABLE,     // Без прерываний
        .timer_sel = LEDC_TIMER_0,          // Таймер 0
        .duty = 0,                          // Начальная скважность (выключен)
        .hpoint = 0,                        // Точка захвата
    };
    ledc_channel_config(&ledc_channel);

    return ESP_OK;
}

// Функция захвата области снимка
camera_fb_t *capture_image(int quality, int x1, int y1, int x2, int y2)
{

    // ----- Проверка входных аргументов --------
    if (quality < 0 || quality > 63)
    {
        ESP_LOGE(TAG, "Ошибка! Некорректное значение качества: %d (допустимый диапазон: 0-63)", quality);
        return NULL;
    }
    if (x2 == 0 || y2 == 0)
    {
        ESP_LOGE(TAG, "Ошибка: x2=%d, y2=%d", x2, y2);
        return NULL;
    }
    if (x2 <= x1 || y2 <= y1)
    {
        ESP_LOGE(TAG, "Ошибка! Некорректные размеры ROI: ширина=%d, высота=%d", x2 - x1, y2 - y1);
        return NULL;
    }

    // ----- Настройки ROI --------

    // Проверяем кратность всех координат ROI
    bool valid = true;

    if (x1 % 8 != 0)
    {
        ESP_LOGE(TAG, "Ошибка! x1 (%d) должен быть кратен 8", x1);
        valid = false;
    }
    if ((x2 - x1) % 16 != 0)
    {
        ESP_LOGE(TAG, "Ошибка! Ширина (x2 - x1 = %d) должна быть кратна 16", x2 - x1);
        valid = false;
    }
    if (y1 % 2 != 0)
    {
        ESP_LOGE(TAG, "Ошибка! y1 (%d) должен быть кратен 4", y1);
        valid = false;
    }
    if ((y2 - y1) % 8 != 0)
    {
        ESP_LOGE(TAG, "Ошибка! Высота (y2 - y1 = %d) должна быть кратна 8", y2 - y1);
        valid = false;
    }
    // Если хотя бы одно условие не выполнено, не продолжаем
    if (!valid)
    {
        ESP_LOGE(TAG, "Ошибка! Координаты ROI некорректны, установка невозможна.");
        return NULL;
    }
    // Всё ОК, можно устанавливать ROI
    //  ESP_LOGI(TAG, "ROI корректен: x1=%d, x2=%d, y1=%d, y2=%d", x1, x2, y1, y2);
    int offsetX = x1;     // Установка смещений X
    int offsetY = y1;     // Установка смещений Y
    int totalX = x2 - x1; // Ширина области захвата
    int totalY = y2 - y1; // Высота области захвата
    // Выходные размеры изображения (равны размерам захваченной области)
    int outputX = totalX;
    int outputY = totalY;

    // Загружаем общие настройки
    common_settings_t common_settings;
    esp_err_t err = read_common_settings_nvs(&common_settings);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read common settings");
        return NULL;
    }

    sensor_t *s = esp_camera_sensor_get(); // Получаем объект сенсора
    if (!s)
    {
        printf("Ошибка: sensor_t не инициализирован\n");
        return NULL;
    }
    s->set_quality(s, quality); // Установка качества jpg
    // Установка обасти захвата
    s->set_res_raw(s, x1, y1, x2, y2, offsetX, offsetY, totalX, totalY, outputX, outputY, false, false);
    //  ESP_LOGI(TAG, "Captured region: x1=%d, y1=%d, x2=%d, y2=%d", x1, y1, x2, y2);

    s->set_agc_gain(s, common_settings.agc_gain);
    s->set_aec_value(s, common_settings.aec_value);

    // Включаем вспышку
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, common_settings.flash_duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(100)); // Ждем стабилизации света

    // Отбрасываем первые 2 кадра
    for (int i = 0; i < 2; i++)
    {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame)
        {
            esp_camera_fb_return(frame); // Освобождаем буфер
        }
        else
        {
            ESP_LOGW(TAG, "Не удалось получить кадр %d", i + 1);
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Добавляем задержку
    }

    // Получаем третий кадр
    camera_fb_t *frame = NULL;
    for (int attempt = 0; attempt < 2; attempt++)
    {
        frame = esp_camera_fb_get();
        if (frame)
        {
            break; // Успешно получили кадр
        }
        ESP_LOGW(TAG, "Попытка %d: Не удалось получить кадр. Повтор...", attempt + 1);
        vTaskDelay(pdMS_TO_TICKS(200)); // Ждем перед следующей попыткой
    }

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0); // 0 — выключаем
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);

    if (!frame)
    {
        ESP_LOGE(TAG, "Не удалось получить кадр!");
        return NULL;
    }

    return frame; // Возвращаем гарантированно хороший кадр
}

// ------------ метаданные ----------------

/**
 * @brief Добавляет текст после маркера EOI (0xFFD9) и CUSTOM_MARKER в JPEG-файл.
 *
 * @param jpeg_data Указатель на указатель на буфер с JPEG-данными.
 *                  После выполнения функции указывает на новый буфер с добавленным текстом.
 * @param jpeg_len  Указатель на размер буфера с JPEG-данными.
 *                  После выполнения функции содержит новый размер данных.
 * @param text      Текст для добавления после EOI и CUSTOM_MARKER.
 * @return          0 в случае успеха, отрицательное значение в случае ошибки:
 *                  -1: Некорректные входные данные.
 *                  -2: Нехватка памяти.
 */
int add_text_after_eoi(uint8_t **jpeg_data, size_t *jpeg_len, const char *text)
{
    if (!jpeg_data || !*jpeg_data || !jpeg_len || !text)
    {
        ESP_LOGE(TAG, "Invalid input parameters");
        return -1; // Ошибка: некорректные входные данные
    }

    // Вычисляем длину текста и маркера
    size_t marker_len = strlen(CUSTOM_MARKER); // 4 bytes
    size_t text_len = strlen(text);
    if (text_len == 0)
    {
        ESP_LOGE(TAG, "Empty text");
        return -1; // Ошибка: пустой текст
    }

    // Проверяем, что файл заканчивается маркером EOI (0xFFD9)
    if ((*jpeg_len < 2) || ((*jpeg_data)[*jpeg_len - 2] != 0xFF) || ((*jpeg_data)[*jpeg_len - 1] != 0xD9))
    {
        ESP_LOGE(TAG, "JPEG file does not end with EOI marker");
        return -1; // Ошибка: файл не заканчивается на 0xFFD9
    }

    // Создаем новый буфер с дополнительным пространством для маркера и текста
    size_t new_jpeg_len = *jpeg_len + marker_len + text_len;
    uint8_t *new_jpeg_data = (uint8_t *)malloc(new_jpeg_len);
    if (!new_jpeg_data)
    {
        ESP_LOGE(TAG, "Out of memory");
        return -2; // Ошибка: нехватка памяти
    }

    // Копируем оригинальный JPEG-файл
    memcpy(new_jpeg_data, *jpeg_data, *jpeg_len);

    // Добавляем маркер перед текстом
    memcpy(new_jpeg_data + *jpeg_len, CUSTOM_MARKER, marker_len);

    // Добавляем текст после маркера
    memcpy(new_jpeg_data + *jpeg_len + marker_len, text, text_len);

    // Обновляем указатель и размер буфера
    free(*jpeg_data); // Освобождаем старый буфер
    *jpeg_data = new_jpeg_data;
    *jpeg_len = new_jpeg_len;

    ESP_LOGI(TAG, "[%s] User data added to JPG successfully. New size: %zu bytes", __func__, new_jpeg_len);
    return 0; // Успех
}

// Функция

esp_err_t send_image_to_text(uint8_t *jpg_data, size_t jpg_size, char **text, char **errorCode)
{
    esp_err_t err;
    int retries = 0;
    int content_length = 0;
    uint32_t task_id;
    char *json_string = NULL;

    // Кодируем изображение в base64
    size_t base64_size = (4 * ((jpg_size + 2) / 3)) + 1;
    char *base64_image = (char *)malloc(base64_size);
    if (base64_image == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for Base64 string");
        return ESP_ERR_NO_MEM;
    }

    int ret = mbedtls_base64_encode((unsigned char *)base64_image, base64_size, &base64_size, jpg_data, jpg_size);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Base64 encoding failed with error code: %d", ret);
        free(base64_image);
        return ESP_FAIL;
    }
    base64_image[base64_size] = '\0';

    // Настройка клиента
    esp_http_client_config_t config = {
        .url = OCR_API_URL,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(base64_image);
        return ESP_FAIL;
    }
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Создаем JSON-запрос
    cJSON *request_json = cJSON_CreateObject();
    if (request_json == NULL)
    {
        ESP_LOGE(TAG, "Failed to create JSON object");
        free(base64_image);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(request_json, "clientKey", API_KEY);
    cJSON *task_object = cJSON_CreateObject();
    if (task_object == NULL)
    {
        ESP_LOGE(TAG, "Failed to create JSON task object");
        cJSON_Delete(request_json);
        free(base64_image);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(task_object, "type", "ImageToTextTask");
    cJSON_AddStringToObject(task_object, "body", base64_image);
    cJSON_AddItemToObject(request_json, "task", task_object);
    free(base64_image);
    json_string = cJSON_PrintUnformatted(request_json);
    cJSON_Delete(request_json);
    if (json_string == NULL)
    {
        ESP_LOGE(TAG, "Failed to print JSON string");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Открываем соединение
    esp_http_client_set_url(client, OCR_API_URL "/createTask");

    while (retries < MAX_RETRIES)
    {
        err = esp_http_client_open(client, strlen(json_string));
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            retries++;
            if (retries >= MAX_RETRIES)
            {
                free(json_string);
                esp_http_client_cleanup(client);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "Retrying in %d ms...", RETRY_DELAY_MS);
            vTaskDelay(RETRY_DELAY_MS / portTICK_PERIOD_MS);
        }
        else
        {
            break;
        }
    }

    // Отправка данных
    int wlen = esp_http_client_write(client, json_string, strlen(json_string));
    free(json_string);
    if (wlen < 0)
    {
        ESP_LOGE(TAG, "Failed to write HTTP request");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Чтение ответа
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0)
    {
        ESP_LOGE(TAG, "HTTP client fetch headers failed");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int data_read = esp_http_client_read(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
    if (data_read < 0)
    {
        ESP_LOGE(TAG, "HTTP read response failed");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Парсинг JSON-ответа
    cJSON *response_json = cJSON_Parse(output_buffer);
    if (response_json == NULL)
    {
        ESP_LOGE(TAG, "Error parsing JSON response: %s", output_buffer);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    cJSON *task_id_json = cJSON_GetObjectItem(response_json, "taskId");
    if (task_id_json == NULL || !cJSON_IsNumber(task_id_json))
    {
        ESP_LOGE(TAG, "Failed to get taskId from response");
        cJSON_Delete(response_json);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    task_id = task_id_json->valueint;
    cJSON_Delete(response_json);
    ESP_LOGI(TAG, "Task ID: %ld", task_id);

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // Формируем запрос на получение решения
    request_json = cJSON_CreateObject();
    if (request_json == NULL)
    {
        ESP_LOGE(TAG, "Failed to create JSON object");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(request_json, "clientKey", API_KEY);
    cJSON_AddNumberToObject(request_json, "taskId", task_id);
    json_string = cJSON_PrintUnformatted(request_json);
    cJSON_Delete(request_json);
    if (json_string == NULL)
    {
        ESP_LOGE(TAG, "Failed to print JSON string");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    esp_http_client_set_url(client, OCR_API_URL "/getTaskResult");
    esp_http_client_set_post_field(client, json_string, strlen(json_string));

    int attempt = 0;
    while (attempt < MAX_TASK_CHECKS)
    {
        attempt++;

        err = esp_http_client_open(client, strlen(json_string));
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
            free(json_string);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        // Отправка данных
        wlen = esp_http_client_write(client, json_string, strlen(json_string));
        if (wlen < 0)
        {
            ESP_LOGE(TAG, "Failed to write HTTP request");
            free(json_string);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        // Чтение ответа
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0)
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
            free(json_string);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        memset(output_buffer, 0, sizeof(output_buffer));
        data_read = esp_http_client_read(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
        if (data_read < 0)
        {
            ESP_LOGE(TAG, "Failed to read response");
            free(json_string);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        // Парсим JSON-ответ
        cJSON *response_json = cJSON_Parse(output_buffer);
        if (response_json == NULL)
        {
            ESP_LOGE(TAG, "Error parsing JSON response: %s", output_buffer);
            ESP_LOGI(TAG, "Retrying in %d ms...", RETRY_DELAY_MS);
            vTaskDelay(RETRY_DELAY_MS / portTICK_PERIOD_MS);
            continue;
        }

        // Получаем код ошибки
        cJSON *errorCode_json = cJSON_GetObjectItem(response_json, "errorCode");
        if (errorCode_json != NULL && cJSON_IsString(errorCode_json))
        {
            const char *errorCodeValue = cJSON_GetStringValue(errorCode_json);
            if (errorCodeValue != NULL)
            {
                // Определяем размер строки и выделяем память
                size_t bufferSize = strlen(errorCodeValue) + 1; // +1 для завершающего нуля
                *errorCode = (char *)malloc(bufferSize);
                if (*errorCode != NULL)
                {
                    strcpy(*errorCode, errorCodeValue); // Копируем строку
                    ESP_LOGI(TAG, "Error on solving task %ld Error code: %s", task_id, *errorCode);
                    free(json_string);
                    cJSON_Delete(response_json);
                    esp_http_client_cleanup(client);
                    return ESP_ERR_INVALID_ARG;
                }
            }
        }
        // Получаем статус задачи
        cJSON *status_json = cJSON_GetObjectItem(response_json, "status");
        if (status_json && cJSON_IsString(status_json) && status_json->valuestring != NULL)
        {
            if (strcmp(status_json->valuestring, "ready") == 0)
            { // Если задача готова, извлекаем данные
                cJSON *solution_json = cJSON_GetObjectItem(response_json, "solution");
                if (solution_json)
                {
                    cJSON *text_json = cJSON_GetObjectItem(solution_json, "text");
                    if (text_json && cJSON_IsString(text_json) && text_json->valuestring != NULL)
                    {
                        // Выделяем память для текста
                        size_t text_length = strlen(text_json->valuestring) + 1; // +1 для завершающего нуля
                        *text = (char *)malloc(text_length);
                        if (*text != NULL)
                        {
                            strcpy(*text, text_json->valuestring); // Копируем строку
                            free(json_string);
                            cJSON_Delete(response_json);
                            esp_http_client_cleanup(client);
                            return ESP_OK;
                        }
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Failed to get text from response");
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to get solution");
                }
                free(json_string);
                cJSON_Delete(response_json);
                esp_http_client_cleanup(client);
                return ESP_OK;
            }
            // Если задача не готова, повторяем попытку через 5 сек
            ESP_LOGI(TAG, "Task %ld still processing, retrying in 5 seconds... (%d/%d)", task_id, attempt, MAX_TASK_CHECKS);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }
    free(json_string);
    cJSON_Delete(response_json);
    // Если после MAX_TASK_CHECKS не получили результат — ошибка
    ESP_LOGE(TAG, "Task did not complete in time");
    esp_http_client_cleanup(client);
    return ESP_FAIL;
}

// Функция для задачи выполнения обработки изображения и сохранения файла
void process_image_task(void *param)
{
    TaskParams *task_params = (TaskParams *)param;
    uint8_t *image_data = task_params->image_data;
    size_t image_len = task_params->image_len;
    char *device_id = task_params->device_id;
    char *device_type = task_params->device_type;

    // Загружаем текущие настройки из NVS
    common_settings_t common_settings;
    esp_err_t err = read_common_settings_nvs(&common_settings);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "[%s]  Failed to read common settings from NVS: %s", __func__, esp_err_to_name(err));
        return;
    }

    // Переменные для хранения текста и кода ошибки
    char *text = NULL;
    char *errorCode = NULL;

    // Передача изображения на распознавание (если включено)
    if (common_settings.ocr_enabled)
    {
        esp_err_t send_result = send_image_to_text(image_data, image_len, &text, &errorCode);
        if (send_result != ESP_OK)
        {
            ESP_LOGE(TAG, "[%s] Failed to send image for recognition, error: %s", __func__, errorCode);
            free(errorCode);
        }
        else
        {
            ESP_LOGI(TAG, "[%s] OCR text recognized: %s", __func__, text ? text : "N/A");
        }
    }
    else
    {
        ESP_LOGI(TAG, "[%s] OCR is disabled. Skipping recognition.", __func__);
    }

    // Формируем полное имя файла с timestamp-bootcount
    char filename[64];
    time_t timestamp = time(NULL);
    uint16_t bootcount = 0;
    uint16_t boot_count = get_boot_count();
    if (boot_count > 0)
    {
        bootcount = boot_count;
    }
    snprintf(filename, sizeof(filename), "%s_%lld_%u.jpg", device_id, (long long)timestamp, bootcount);

    // Формируем данные для добавления в JPEG
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to create JSON object");
    }
    else
    {
        cJSON_AddStringToObject(root, "device_id", device_id);
        cJSON_AddStringToObject(root, "device_type", device_type);
        cJSON_AddNumberToObject(root, "timestamp", (double)timestamp);
        cJSON_AddStringToObject(root, "text", text ? text : "N/A"); // Добавляем "N/A", если text == NULL
        char *json_string = cJSON_PrintUnformatted(root);           // Преобразуем JSON в строку
        cJSON_Delete(root);

        if (!json_string)
        {
            ESP_LOGE(TAG, "Failed to print JSON");
        }
        else
        {
            // Добавляем данные в JPEG
            if (add_text_after_eoi(&image_data, &image_len, json_string) != 0)
            {
                ESP_LOGE(TAG, "Failed to add data to jpg, process without data");
            }
            free(json_string);
        }
    }

    // Сохраняем файл в SPIFFS
    esp_err_t save_result = save_to_spiffs(NULL, filename, image_data, image_len);
    if (save_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save file");
    }
    else
    {
        ESP_LOGI(TAG, "[%s] File saved successfully to SPIFFS: %s", __func__, filename);
    }

    // Отправляем копию на сервер (если включено)
    if (common_settings.copy_to_server)
    {
        esp_err_t send_server_result = send_file_to_server(common_settings.server_path, filename);
        if (send_server_result != ESP_OK)
        {
            ESP_LOGE(TAG, "[%s] Failed to send file to server for device %s", __func__, device_id);
        }
        else
        {
            ESP_LOGI(TAG, "[%s] File %s sent successfully to %s", __func__, filename, common_settings.server_path);
        }
    }
    else
    {
        ESP_LOGI(TAG, "[%s] Copying to server is disabled. Skipping file upload.", __func__);
    }

    // Отправляем полное имя файла в очередь
    if (xQueueSend(task_params->filenames_queue, filename, portMAX_DELAY) != pdPASS)
    {
        ESP_LOGE(TAG, "[%s] Failed to send filename to queue", __func__);
    }

    // Освобождение памяти
    heap_caps_free(image_data);
    free(text);
    free(task_params); // Освобождаем память для TaskParams
    vTaskDelete(NULL); // Удаление задачи
}

// Функция создания задач FreeRTOS для параллельной обработки
esp_err_t process_images(char ***out_filenames)
{
    // Создаем очередь для передачи имен файлов
    QueueHandle_t filenames_queue = xQueueCreate(num_devices, sizeof(char[64])); // Очередь на num_devices элементов
    if (!filenames_queue)
    {
        ESP_LOGE(TAG, "[%s] Failed to create filenames queue", __func__);
        return ESP_FAIL;
    }

    // Выделяем память для массива имен файлов
    *out_filenames = malloc((num_devices + 1) * sizeof(char *)); // +1 для завершающего NULL
    if (!*out_filenames)
    {
        ESP_LOGE(TAG, "[%s] Failed to allocate memory for filenames array", __func__);
        vQueueDelete(filenames_queue);
        return ESP_FAIL;
    }

    // Массивы для хранения данных
    uint8_t **image_data_array = malloc(num_devices * sizeof(uint8_t *));
    size_t *image_len_array = malloc(num_devices * sizeof(size_t));
    TaskParams **params_array = malloc(num_devices * sizeof(TaskParams *));
    if (!image_data_array || !image_len_array || !params_array)
    {
        ESP_LOGE(TAG, "[%s] Failed to allocate memory for arrays", __func__);
        free(*out_filenames);
        free(image_data_array);
        free(image_len_array);
        free(params_array);
        vQueueDelete(filenames_queue);
        return ESP_FAIL;
    }

    // Захват изображений для каждого устройства
    for (size_t i = 0; i < num_devices; i++)
    {
        const char *device_key = device_keys[i];

        // Загрузка конфигурации устройства
        DeviceConfig config;
        if (load_device_config(device_key, &config) != ESP_OK)
        {
            ESP_LOGE(TAG, "[%s] Failed to load configuration for %s", __func__, device_key);
            continue; // Пропускаем устройство при ошибке
        }

        // Захват изображения
        camera_fb_t *frame = capture_image(16, config.x1, config.y1, config.x2, config.y2);
        if (!frame)
        {
            ESP_LOGE(TAG, "[%s] Failed to capture image for %s", __func__, device_key);
            continue; // Пропускаем устройство при ошибке
        }

        // Выделение памяти для изображения
        image_len_array[i] = frame->len;
        image_data_array[i] = heap_caps_malloc(image_len_array[i], MALLOC_CAP_SPIRAM);
        if (!image_data_array[i])
        {
            ESP_LOGE(TAG, "[%s] Failed to allocate memory for image data for %s", __func__, device_key);
            esp_camera_fb_return(frame);
            continue; // Пропускаем устройство при ошибке
        }
        memcpy(image_data_array[i], frame->buf, image_len_array[i]);
        esp_camera_fb_return(frame);

        // Подготовка параметров задачи
        params_array[i] = malloc(sizeof(TaskParams));
        if (!params_array[i])
        {
            ESP_LOGE(TAG, "[%s] Failed to allocate memory for task parameters for %s", __func__, device_key);
            heap_caps_free(image_data_array[i]);
            continue; // Пропускаем при ошибке
        }
        params_array[i]->image_data = image_data_array[i];
        params_array[i]->image_len = image_len_array[i];
        // Копируем device_id из config.id в params_array[i]->device_id
        strncpy(params_array[i]->device_id, config.id, sizeof(params_array[i]->device_id) - 1);
        params_array[i]->device_id[sizeof(params_array[i]->device_id) - 1] = '\0'; // Гарантируем завершающий '\0'
        // Аналогично для device_type
        strncpy(params_array[i]->device_type, config.type, sizeof(params_array[i]->device_type) - 1);
        params_array[i]->device_type[sizeof(params_array[i]->device_type) - 1] = '\0'; // Гарантируем завершающий '\0'

        params_array[i]->filenames_queue = filenames_queue;

        // Создание задачи для обработки изображения
        xTaskCreate(process_image_task, "ProcessImageTask", 8192, (void *)params_array[i], 1, NULL);
    }

    // Получаем имена файлов из очереди
    for (size_t i = 0; i < num_devices; i++)
    {
        char filename[64];
        if (xQueueReceive(filenames_queue, filename, portMAX_DELAY) != pdPASS)
        {
            ESP_LOGE(TAG, "[%s] Failed to receive filename from queue for device %zu", __func__, i);
            free(*out_filenames);
            vQueueDelete(filenames_queue);
            return ESP_FAIL;
        }
        (*out_filenames)[i] = strdup(filename); // Копируем полное имя файла
    }
    (*out_filenames)[num_devices] = NULL; // Завершающий NULL

    // Удаляем очередь после завершения работы
    vQueueDelete(filenames_queue);
    return ESP_OK;
}

/**
 * Чтение JSON-данных после объединенного маркера (EOI + CUSTOM_MARKER).
 *
 * @param jpeg_data Указатель на данные JPEG-файла.
 * @param jpeg_len Длина данных JPEG-файла.
 * @return cJSON* Распарсенный JSON-объект или NULL в случае ошибки.
 */
cJSON *read_json_after_eoi(const uint8_t *jpeg_data, size_t jpeg_len)
{
    if (!jpeg_data || jpeg_len == 0)
    {
        ESP_LOGE(TAG, "Invalid input parameters");
        return NULL;
    }

    // Определяем EOI внутри функции
    const char *eoi = "\xFF\xD9";
    size_t eoi_len = 2; // Длина EOI всегда равна 2 байтам

    // Вычисляем длину пользовательского маркера
    size_t custom_marker_len = sizeof(CUSTOM_MARKER) - 1; // Без завершающего нуля
    size_t combined_marker_len = eoi_len + custom_marker_len;

    // Создаем буфер для объединенного маркера
    char combined_marker[combined_marker_len];
    memcpy(combined_marker, eoi, eoi_len);
    memcpy(combined_marker + eoi_len, CUSTOM_MARKER, custom_marker_len);

    // Поиск объединенного маркера (EOI + CUSTOM_MARKER)
    size_t marker_pos = 0;
    for (size_t i = 0; i <= jpeg_len - combined_marker_len; i++)
    {
        if (memcmp(&jpeg_data[i], combined_marker, combined_marker_len) == 0)
        {
            marker_pos = i + combined_marker_len; // Позиция после объединенного маркера
            break;
        }
    }
    if (marker_pos == 0)
    {
        ESP_LOGW(TAG, "Combined marker not found");
        return NULL;
    }

    // Проверяем, есть ли данные после объединенного маркера
    if (marker_pos >= jpeg_len)
    {
        ESP_LOGE(TAG, "No data after combined marker");
        return NULL;
    }
    // Извлекаем данные JSON
    size_t data_len = jpeg_len - marker_pos;
    char *json_str = (char *)malloc(data_len + 1); // +1 для завершающего нуля
    if (!json_str)
    {
        ESP_LOGE(TAG, "Out of memory");
        return NULL;
    }
    memcpy(json_str, jpeg_data + marker_pos, data_len);
    json_str[data_len] = '\0'; // Завершаем строку нулем

    // Парсим JSON
    cJSON *json = cJSON_Parse(json_str);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
        free(json_str);
        return NULL;
    }

    free(json_str);

    return json;
}
