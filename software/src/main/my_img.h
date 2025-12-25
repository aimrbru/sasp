
#include <stdbool.h> // Для типа bool
#include <stddef.h>  // Для типа size_t
#include "esp_err.h"
#include "esp_camera.h"

#include "utils.h"

#include "esp_sntp.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>

#define CUSTOM_MARKER "\xFF\xFF\xFF\x7B" // Маркер для хранения метаданных

esp_err_t init_camera(void);
camera_fb_t *capture_image(int quality, int x1, int y1, int x2, int y2);
esp_err_t process_images(char ***out_filenames);
void process_image_task(void *param);
cJSON *read_json_after_eoi(const uint8_t *jpeg_data, size_t jpeg_len);