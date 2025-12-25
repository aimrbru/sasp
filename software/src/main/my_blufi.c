/* software/src/main/my_blufi.c
 
* SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/****************************************************************************
 * This is a demo for bluetooth config wifi connection to ap. You can config ESP32 to connect a softap
 * or config ESP32 as a softap to be connected by other device. APP can be downloaded from github
 * android source code: https://github.com/EspressifApp/EspBlufi
 * iOS source code: https://github.com/EspressifApp/EspBlufiForiOS
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
#include "esp_bt.h"
#endif

#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#endif

#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "my_blufi.h"
#include "web.h"
#include "utils.h"

#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "console/console.h"
#endif

#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"
#include "mbedtls/md5.h"
#include "esp_crc.h"
#include "esp_random.h"

#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "my_blufi.h"

#define WIFI_CONNECTION_MAXIMUM_RETRY 2 // Определение максимального количества попыток подключения к Wi-Fi
#define INVALID_REASON 255
#define INVALID_RSSI -128 // Значения по умолчанию для RSSI и причины отключения

static void event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param); // Обработчик событий Blufi

#define WIFI_LIST_NUM 2 // Константа для определения количества точек доступа в списке

// Структуры конфигурации Wi-Fi для режима станции (STA) и точки доступа (AP)
static wifi_config_t sta_config;
static wifi_config_t ap_config;

static EventGroupHandle_t wifi_event_group; // Группа событий FreeRTOS для управления состоянием подключения Wi-Fi

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0; // Битовая маска для события подключения к Wi-Fi

static uint8_t wifi_retry = 0; // Счетчик попыток подключения к Wi-Fi

// Флаги для отслеживания состояния подключения
static bool gl_sta_connected = false; // Подключение к точке доступа
static bool gl_sta_got_ip = false;    // Получение IP-адреса
static bool ble_is_connected = false; // Подключение через BLE

// Информация о текущей точке доступа
static uint8_t gl_sta_bssid[6];                 // MAC-адрес точки доступа
static uint8_t gl_sta_ssid[32];                 // SSID точки доступа
static int gl_sta_ssid_len;                     // Длина SSID
static wifi_sta_list_t gl_sta_list;             // Список подключенных устройств
static bool gl_sta_is_connecting = false;       // Флаг процесса подключения
static esp_blufi_extra_info_t gl_sta_conn_info; // Дополнительная информация о подключении

// Функция для записи информации о подключении Wi-Fi
static void record_wifi_conn_info(int rssi, uint8_t reason)
{
    memset(&gl_sta_conn_info, 0, sizeof(esp_blufi_extra_info_t)); // Очистка структуры
    if (gl_sta_is_connecting)
    {
        // Если происходит подключение, записываем максимальное количество попыток
        gl_sta_conn_info.sta_max_conn_retry_set = true;
        gl_sta_conn_info.sta_max_conn_retry = WIFI_CONNECTION_MAXIMUM_RETRY;
    }
    else
    {
        // Иначе записываем информацию о RSSI и причине отключения
        gl_sta_conn_info.sta_conn_rssi_set = true;
        gl_sta_conn_info.sta_conn_rssi = rssi;
        gl_sta_conn_info.sta_conn_end_reason_set = true;
        gl_sta_conn_info.sta_conn_end_reason = reason;
    }
}

// Функция для начала подключения к Wi-Fi
static void wifi_connect(void)
{
    wifi_retry = 0;                                        // Сброс счетчика попыток
    gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK); // Попытка подключения
    record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);   // Запись начальной информации о подключении
}

// Функция для повторного подключения к Wi-Fi
static bool wifi_reconnect(void)
{
    bool ret = false;
    if (gl_sta_is_connecting && wifi_retry++ < WIFI_CONNECTION_MAXIMUM_RETRY)
    {
        BLUFI_INFO("BLUFI WiFi starts reconnection\n");        // Логирование попытки повторного подключения
        gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK); // Повторная попытка подключения
        record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);   // Запись информации о повторной попытке
        ret = true;
    }
    return ret; // Возвращаем результат попытки подключения
}

// Функция для получения текущего числа подключений к точке доступа
static int softap_get_current_connection_number(void)
{
    esp_err_t ret = esp_wifi_ap_get_sta_list(&gl_sta_list); // Получение списка подключенных устройств
    if (ret == ESP_OK)
    {
        return gl_sta_list.num; // Возвращаем количество подключений
    }
    return 0; // Если ошибка, возвращаем 0
}

// Обработчик событий IP-адресации
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_mode_t mode;
    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_blufi_extra_info_t info;                         // Структура для дополнительной информации
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT); // Установка бита подключения
        esp_wifi_get_mode(&mode);                            // Получение текущего режима Wi-Fi
        memset(&info, 0, sizeof(esp_blufi_extra_info_t));    // Очистка структуры
        memcpy(info.sta_bssid, gl_sta_bssid, 6);             // Копирование BSSID точки доступа
        info.sta_bssid_set = true;                           // Установка флага наличия BSSID
        info.sta_ssid = gl_sta_ssid;                         // Копирование SSID
        info.sta_ssid_len = gl_sta_ssid_len;                 // Копирование длины SSID
        gl_sta_got_ip = true;                                // Установка флага получения IP-адреса
        if (ble_is_connected == true)
        {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_get_current_connection_number(), &info); // Отправка отчета о подключении
        }
        else
        {
            BLUFI_INFO("BLUFI BLE is not connected yet\n"); // Логирование, если BLE не подключен
        }

        // Запуск HTTP-сервера после получения IP-адреса
       
        start_webserver();

        break;
    }
    }
}

// Обработчик событий Wi-Fi
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_sta_connected_t *event;
    wifi_event_sta_disconnected_t *disconnected_event;
    wifi_mode_t mode;

    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        wifi_connect(); // Начало подключения к Wi-Fi
        break;

    case WIFI_EVENT_STA_CONNECTED:
        gl_sta_connected = true;                           // Установка флага успешного подключения
        gl_sta_is_connecting = false;                      // Сброс флага процесса подключения
        event = (wifi_event_sta_connected_t *)event_data;  // Получение данных о подключении
        memcpy(gl_sta_bssid, event->bssid, 6);             // Копирование BSSID
        memcpy(gl_sta_ssid, event->ssid, event->ssid_len); // Копирование SSID
        gl_sta_ssid_len = event->ssid_len;                 // Сохранение длины SSID
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        if (!gl_sta_connected && !wifi_reconnect())
        {
            // Если подключение не удалось, записываем причину отключения
            disconnected_event = (wifi_event_sta_disconnected_t *)event_data;
            record_wifi_conn_info(disconnected_event->rssi, disconnected_event->reason);
        }
        gl_sta_connected = false;                              // Сброс флага подключения
        gl_sta_got_ip = false;                                 // Сброс флага получения IP
        memset(gl_sta_ssid, 0, 32);                            // Очистка SSID
        memset(gl_sta_bssid, 0, 6);                            // Очистка BSSID
        gl_sta_ssid_len = 0;                                   // Сброс длины SSID
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT); // Сброс бита подключения
        break;

    case WIFI_EVENT_AP_START:
        esp_wifi_get_mode(&mode); // Получение текущего режима Wi-Fi
        if (ble_is_connected == true)
        {
            if (gl_sta_connected)
            {
                // Если подключены к Wi-Fi, отправляем отчет о подключении
                esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &gl_sta_conn_info);
            }
            else if (gl_sta_is_connecting)
            {
                // Если идет процесс подключения, отправляем отчет о процессе
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_get_current_connection_number(), &gl_sta_conn_info);
            }
            else
            {
                // Если подключение не удалось, отправляем отчет об ошибке
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_get_current_connection_number(), &gl_sta_conn_info);
            }
        }
        else
        {
            BLUFI_INFO("BLUFI BLE is not connected yet\n"); // Логирование, если BLE не подключен
        }
        break;

    case WIFI_EVENT_SCAN_DONE:
    {
        uint16_t apCount = 0;               // Количество найденных сетей
        esp_wifi_scan_get_ap_num(&apCount); // Получение количества сетей
        if (apCount == 0)
        {
            BLUFI_INFO("Nothing AP found\n"); // Логирование, если сетей не найдено
            break;
        }
        wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount); // Выделение памяти для списка сетей
        if (!ap_list)
        {
            BLUFI_ERROR("malloc error, ap_list is NULL\n"); // Логирование ошибки выделения памяти
            esp_wifi_clear_ap_list();                       // Очистка списка сетей
            break;
        }
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, ap_list));                                                // Получение записей о сетях
        esp_blufi_ap_record_t *blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t)); // Выделение памяти для Blufi-списка
        if (!blufi_ap_list)
        {
            free(ap_list); // Освобождение памяти при ошибке
            BLUFI_ERROR("malloc error, blufi_ap_list is NULL\n");
            break;
        }
        for (int i = 0; i < apCount; ++i)
        {
            blufi_ap_list[i].rssi = ap_list[i].rssi;                                 // Копирование уровня сигнала
            memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid)); // Копирование SSID
        }
        if (ble_is_connected == true)
        {
            esp_blufi_send_wifi_list(apCount, blufi_ap_list); // Отправка списка сетей через Blufi
        }
        else
        {
            BLUFI_INFO("BLUFI BLE is not connected yet\n"); // Логирование, если BLE не подключен
        }
        esp_wifi_scan_stop(); // Остановка сканирования
        free(ap_list);        // Освобождение памяти
        free(blufi_ap_list);
        break;
    }

    case WIFI_EVENT_AP_STACONNECTED:
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data; // Получение данных о подключении устройства
        BLUFI_INFO("station " MACSTR " join, AID=%d\n", MAC2STR(event->mac), event->aid); // Логирование подключения
        break;
    }

    case WIFI_EVENT_AP_STADISCONNECTED:
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;                      // Получение данных об отключении устройства
        BLUFI_INFO("station " MACSTR " leave, AID=%d, reason=%d\n", MAC2STR(event->mac), event->aid, event->reason); // Логирование отключения
        break;
    }
    }
}

// Инициализация Wi-Fi
void initialise_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());                                                                    // Инициализация сетевого интерфейса
    wifi_event_group = xEventGroupCreate();                                                               // Создание группы событий
    ESP_ERROR_CHECK(esp_event_loop_create_default());                                                     // Создание цикла событий
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();                                         // Создание интерфейса станции
    assert(sta_netif);                                                                                    // Проверка создания интерфейса
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();                                           // Создание интерфейса точки доступа
    assert(ap_netif);                                                                                     // Проверка создания интерфейса
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL)); // Регистрация обработчика событий Wi-Fi
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));  // Регистрация обработчика событий IP
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();                                                  // Настройка Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));                                                                 // Инициализация Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));                                                    // Установка режима станции
    record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);                                                  // Запись начальной информации о подключении
    ESP_ERROR_CHECK(esp_wifi_start());                                                                    // Запуск Wi-Fi
}

// Коллбэки для Blufi
esp_blufi_callbacks_t callbacks = {
    .event_cb = event_callback,                                // Обработчик событий Blufi
    .negotiate_data_handler = blufi_dh_negotiate_data_handler, // Обработка данных для Diffie-Hellman
    .encrypt_func = blufi_aes_encrypt,                         // Функция шифрования
    .decrypt_func = blufi_aes_decrypt,                         // Функция расшифровки
    .checksum_func = blufi_crc_checksum,                       // Функция расчета контрольной суммы
};

// Обработчик событий Blufi
static void event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    /* actually, should post to blufi_task handle the procedure,
     * now, as a example, we do it more simply */
    switch (event)
    {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        BLUFI_INFO("BLUFI init finish\n"); // Логирование завершения инициализации
        esp_blufi_adv_start();             // Начало рекламы через BLE
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        BLUFI_INFO("BLUFI deinit finish\n");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        BLUFI_INFO("BLUFI ble connect\n"); // Логирование подключения через BLE
        ble_is_connected = true;           // Установка флага подключения BLE
        esp_blufi_adv_stop();              // Остановка рекламы
        blufi_security_init();             // Инициализация безопасности Blufi
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        BLUFI_INFO("BLUFI ble disconnect\n"); // Логирование отключения через BLE
        ble_is_connected = false;             // Сброс флага подключения BLE
        blufi_security_deinit();              // Деинициализация безопасности Blufi
        esp_blufi_adv_start();                // Возобновление рекламы
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        BLUFI_INFO("BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode); // Логирование изменения режима Wi-Fi
        ESP_ERROR_CHECK(esp_wifi_set_mode(param->wifi_mode.op_mode));       // Изменение режима Wi-Fi
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        BLUFI_INFO("BLUFI request wifi connect to AP\n"); // Логирование запроса подключения к точке доступа
                                                          /* there is no wifi callback when the device has already connected to this wifi so disconnect wifi before connection. */
        esp_wifi_disconnect();
        wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        BLUFI_INFO("BLUFI requset wifi disconnect from AP\n");
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        BLUFI_ERROR("BLUFI report error, error code %d\n", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS:
    {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;
        esp_wifi_get_mode(&mode); // Получение текущего режима Wi-Fi
        if (gl_sta_connected)
        {
            // Если подключены к Wi-Fi, отправляем успешный отчет
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, gl_sta_bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = gl_sta_ssid;
            info.sta_ssid_len = gl_sta_ssid_len;
            esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
        }
        else if (gl_sta_is_connecting)
        {
            // Если идет процесс подключения, отправляем соответствующий отчет
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_get_current_connection_number(), &gl_sta_conn_info);
        }
        else
        {
            // Если подключение не удалось, отправляем отчет об ошибке
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_get_current_connection_number(), &gl_sta_conn_info);
        }
        BLUFI_INFO("BLUFI get wifi status from AP\n"); // Логирование получения статуса Wi-Fi
        break;
    }

    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        BLUFI_INFO("blufi close a gatt connection");
        esp_blufi_disconnect();
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        break;
    case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA BSSID %s\n", sta_config.sta.ssid);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA SSID %s\n", sta_config.sta.ssid);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len); // Копирование пароля
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';                                             // Добавление завершающего нуля
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);                                                            // Применение настроек Wi-Fi
        BLUFI_INFO("Recv STA PASSWORD %s\n", sta_config.sta.password);                                            // Логирование полученного пароля
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
        strncpy((char *)ap_config.ap.ssid, (char *)param->softap_ssid.ssid, param->softap_ssid.ssid_len);
        ap_config.ap.ssid[param->softap_ssid.ssid_len] = '\0';
        ap_config.ap.ssid_len = param->softap_ssid.ssid_len;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP SSID %s, ssid len %d\n", ap_config.ap.ssid, ap_config.ap.ssid_len);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
        strncpy((char *)ap_config.ap.password, (char *)param->softap_passwd.passwd, param->softap_passwd.passwd_len);
        ap_config.ap.password[param->softap_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP PASSWORD %s len = %d\n", ap_config.ap.password, param->softap_passwd.passwd_len);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
        if (param->softap_max_conn_num.max_conn_num > 4)
        {
            return;
        }
        ap_config.ap.max_connection = param->softap_max_conn_num.max_conn_num;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP MAX CONN NUM %d\n", ap_config.ap.max_connection);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
        if (param->softap_auth_mode.auth_mode >= WIFI_AUTH_MAX)
        {
            return;
        }
        ap_config.ap.authmode = param->softap_auth_mode.auth_mode;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP AUTH MODE %d\n", ap_config.ap.authmode);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
        if (param->softap_channel.channel > 13)
        {
            return;
        }
        ap_config.ap.channel = param->softap_channel.channel;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP CHANNEL %d\n", ap_config.ap.channel);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_LIST:
    {
        wifi_scan_config_t scanConf = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = false};
        esp_err_t ret = esp_wifi_scan_start(&scanConf, true);
        if (ret != ESP_OK)
        {
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;
    }
    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
        BLUFI_INFO("Recv Custom Data %" PRIu32 "\n", param->custom_data.data_len);
        ESP_LOG_BUFFER_HEX("Custom Data", param->custom_data.data, param->custom_data.data_len);
        break;
    case ESP_BLUFI_EVENT_RECV_USERNAME:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_CA_CERT:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
        /* Not handle currently */
        break;
        ;
    case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
        /* Not handle currently */
        break;
    default:
        break;
    }
}

// blufi_init------------------------------------------------

#ifdef CONFIG_BT_BLUEDROID_ENABLED
esp_err_t esp_blufi_host_init(void)
{
    int ret;
    ret = esp_bluedroid_init();
    if (ret)
    {
        BLUFI_ERROR("%s init bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        BLUFI_ERROR("%s init bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    BLUFI_INFO("BD ADDR: " ESP_BD_ADDR_STR "\n", ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));

    return ESP_OK;
}

esp_err_t esp_blufi_host_deinit(void)
{
    int ret;
    ret = esp_blufi_profile_deinit();
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = esp_bluedroid_disable();
    if (ret)
    {
        BLUFI_ERROR("%s deinit bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ret = esp_bluedroid_deinit();
    if (ret)
    {
        BLUFI_ERROR("%s deinit bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_blufi_gap_register_callback(void)
{
    int rc;
    rc = esp_ble_gap_register_callback(esp_blufi_gap_event_handler);
    if (rc)
    {
        return rc;
    }
    return esp_blufi_profile_init();
}

esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *callbacks)
{
    esp_err_t ret = ESP_OK;

    ret = esp_blufi_host_init();
    if (ret)
    {
        BLUFI_ERROR("%s initialise host failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_blufi_register_callbacks(callbacks);
    if (ret)
    {
        BLUFI_ERROR("%s blufi register failed, error code = %x\n", __func__, ret);
        return ret;
    }

    ret = esp_blufi_gap_register_callback();
    if (ret)
    {
        BLUFI_ERROR("%s gap register failed, error code = %x\n", __func__, ret);
        return ret;
    }

    return ESP_OK;
}

#endif
/*CONFIG_BT_BLUEDROID_ENABLED */

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
esp_err_t esp_blufi_controller_init()
{
    esp_err_t ret = ESP_OK;
#if CONFIG_IDF_TARGET_ESP32
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
#endif

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        BLUFI_ERROR("%s initialize bt controller failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        BLUFI_ERROR("%s enable bt controller failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }
    return ret;
}
#endif

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
esp_err_t esp_blufi_controller_deinit()
{
    esp_err_t ret = ESP_OK;
    ret = esp_bt_controller_disable();
    if (ret)
    {
        BLUFI_ERROR("%s disable bt controller failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_deinit();
    if (ret)
    {
        BLUFI_ERROR("%s deinit bt controller failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    return ret;
}
#endif

#ifdef CONFIG_BT_NIMBLE_ENABLED
void ble_store_config_init(void);
static void blufi_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
blufi_on_sync(void)
{
    esp_blufi_profile_init();
}

void bleprph_host_task(void *param)
{
    ESP_LOGI(BLUFI_TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

esp_err_t esp_blufi_host_init(void)
{
    esp_err_t err;
    err = esp_nimble_init();
    if (err)
    {
        BLUFI_ERROR("%s failed: %s\n", __func__, esp_err_to_name(err));
        return ESP_FAIL;
    }

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = blufi_on_reset;
    ble_hs_cfg.sync_cb = blufi_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = 4;
#ifdef CONFIG_BONDING
    ble_hs_cfg.sm_bonding = 1;
#endif
#ifdef CONFIG_MITM
    ble_hs_cfg.sm_mitm = 1;
#endif
#ifdef CONFIG_USE_SC
    ble_hs_cfg.sm_sc = 1;
#else
    ble_hs_cfg.sm_sc = 0;
#ifdef CONFIG_BONDING
    ble_hs_cfg.sm_our_key_dist = 1;
    ble_hs_cfg.sm_their_key_dist = 1;
#endif
#endif

    int rc;
    rc = esp_blufi_gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set(BLUFI_DEVICE_NAME);
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    esp_blufi_btc_init();

    err = esp_nimble_enable(bleprph_host_task);
    if (err)
    {
        BLUFI_ERROR("%s failed: %s\n", __func__, esp_err_to_name(err));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_blufi_host_deinit(void)
{
    esp_err_t ret = ESP_OK;

    ret = nimble_port_stop();

    if (ret == 0)
    {
        esp_nimble_deinit();
    }

    ret = esp_blufi_profile_deinit();
    if (ret != ESP_OK)
    {
        return ret;
    }

    esp_blufi_btc_deinit();

    return ret;
}

esp_err_t esp_blufi_gap_register_callback(void)
{
    return ESP_OK;
}

esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *callbacks)
{
    esp_err_t ret = ESP_OK;

    ret = esp_blufi_register_callbacks(callbacks);
    if (ret)
    {
        BLUFI_ERROR("%s blufi register failed, error code = %x\n", __func__, ret);
        return ret;
    }

    ret = esp_blufi_gap_register_callback();
    if (ret)
    {
        BLUFI_ERROR("%s gap register failed, error code = %x\n", __func__, ret);
        return ret;
    }

    ret = esp_blufi_host_init();
    if (ret)
    {
        BLUFI_ERROR("%s initialise host failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    return ret;
}
#endif /* CONFIG_BT_NIMBLE_ENABLED */

// blufi_security------------------------------------------------

#define SEC_TYPE_DH_PARAM_LEN 0x00
#define SEC_TYPE_DH_PARAM_DATA 0x01
#define SEC_TYPE_DH_P 0x02
#define SEC_TYPE_DH_G 0x03
#define SEC_TYPE_DH_PUBLIC 0x04

struct blufi_security
{
#define DH_SELF_PUB_KEY_LEN 128
#define DH_SELF_PUB_KEY_BIT_LEN (DH_SELF_PUB_KEY_LEN * 8)
    uint8_t self_public_key[DH_SELF_PUB_KEY_LEN];
#define SHARE_KEY_LEN 128
#define SHARE_KEY_BIT_LEN (SHARE_KEY_LEN * 8)
    uint8_t share_key[SHARE_KEY_LEN];
    size_t share_len;
#define PSK_LEN 16
    uint8_t psk[PSK_LEN];
    uint8_t *dh_param;
    int dh_param_len;
    uint8_t iv[16];
    mbedtls_dhm_context dhm;
    mbedtls_aes_context aes;
};
static struct blufi_security *blufi_sec;

static int myrand(void *rng_state, unsigned char *output, size_t len)
{
    esp_fill_random(output, len);
    return (0);
}

extern void btc_blufi_report_error(esp_blufi_error_state_t state);

void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free)
{
    int ret;
    uint8_t type = data[0];

    if (blufi_sec == NULL)
    {
        BLUFI_ERROR("BLUFI Security is not initialized");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }

    switch (type)
    {
    case SEC_TYPE_DH_PARAM_LEN:
        blufi_sec->dh_param_len = ((data[1] << 8) | data[2]);
        if (blufi_sec->dh_param)
        {
            free(blufi_sec->dh_param);
            blufi_sec->dh_param = NULL;
        }
        blufi_sec->dh_param = (uint8_t *)malloc(blufi_sec->dh_param_len);
        if (blufi_sec->dh_param == NULL)
        {
            btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
            BLUFI_ERROR("%s, malloc failed\n", __func__);
            return;
        }
        break;
    case SEC_TYPE_DH_PARAM_DATA:
    {
        if (blufi_sec->dh_param == NULL)
        {
            BLUFI_ERROR("%s, blufi_sec->dh_param == NULL\n", __func__);
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
            return;
        }
        uint8_t *param = blufi_sec->dh_param;
        memcpy(blufi_sec->dh_param, &data[1], blufi_sec->dh_param_len);
        ret = mbedtls_dhm_read_params(&blufi_sec->dhm, &param, &param[blufi_sec->dh_param_len]);
        if (ret)
        {
            BLUFI_ERROR("%s read param failed %d\n", __func__, ret);
            btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
            return;
        }
        free(blufi_sec->dh_param);
        blufi_sec->dh_param = NULL;

        const int dhm_len = mbedtls_dhm_get_len(&blufi_sec->dhm);
        ret = mbedtls_dhm_make_public(&blufi_sec->dhm, dhm_len, blufi_sec->self_public_key, dhm_len, myrand, NULL);
        if (ret)
        {
            BLUFI_ERROR("%s make public failed %d\n", __func__, ret);
            btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
            return;
        }

        ret = mbedtls_dhm_calc_secret(&blufi_sec->dhm,
                                      blufi_sec->share_key,
                                      SHARE_KEY_BIT_LEN,
                                      &blufi_sec->share_len,
                                      myrand, NULL);
        if (ret)
        {
            BLUFI_ERROR("%s mbedtls_dhm_calc_secret failed %d\n", __func__, ret);
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
            return;
        }

        ret = mbedtls_md5(blufi_sec->share_key, blufi_sec->share_len, blufi_sec->psk);

        if (ret)
        {
            BLUFI_ERROR("%s mbedtls_md5 failed %d\n", __func__, ret);
            btc_blufi_report_error(ESP_BLUFI_CALC_MD5_ERROR);
            return;
        }

        mbedtls_aes_setkey_enc(&blufi_sec->aes, blufi_sec->psk, 128);

        /* alloc output data */
        *output_data = &blufi_sec->self_public_key[0];
        *output_len = dhm_len;
        *need_free = false;
    }
    break;
    case SEC_TYPE_DH_P:
        break;
    case SEC_TYPE_DH_G:
        break;
    case SEC_TYPE_DH_PUBLIC:
        break;
    }
}

int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    int ret;
    size_t iv_offset = 0;
    uint8_t iv0[16];

    memcpy(iv0, blufi_sec->iv, sizeof(blufi_sec->iv));
    iv0[0] = iv8; /* set iv8 as the iv0[0] */

    ret = mbedtls_aes_crypt_cfb128(&blufi_sec->aes, MBEDTLS_AES_ENCRYPT, crypt_len, &iv_offset, iv0, crypt_data, crypt_data);
    if (ret)
    {
        return -1;
    }

    return crypt_len;
}

int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    int ret;
    size_t iv_offset = 0;
    uint8_t iv0[16];

    memcpy(iv0, blufi_sec->iv, sizeof(blufi_sec->iv));
    iv0[0] = iv8; /* set iv8 as the iv0[0] */

    ret = mbedtls_aes_crypt_cfb128(&blufi_sec->aes, MBEDTLS_AES_DECRYPT, crypt_len, &iv_offset, iv0, crypt_data, crypt_data);
    if (ret)
    {
        return -1;
    }

    return crypt_len;
}

uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len)
{
    /* This iv8 ignore, not used */
    return esp_crc16_be(0, data, len);
}

esp_err_t blufi_security_init(void)
{
    blufi_sec = (struct blufi_security *)malloc(sizeof(struct blufi_security));
    if (blufi_sec == NULL)
    {
        return ESP_FAIL;
    }

    memset(blufi_sec, 0x0, sizeof(struct blufi_security));

    mbedtls_dhm_init(&blufi_sec->dhm);
    mbedtls_aes_init(&blufi_sec->aes);

    memset(blufi_sec->iv, 0x0, 16);
    return 0;
}

void blufi_security_deinit(void)
{
    if (blufi_sec == NULL)
    {
        return;
    }
    if (blufi_sec->dh_param)
    {
        free(blufi_sec->dh_param);
        blufi_sec->dh_param = NULL;
    }
    mbedtls_dhm_free(&blufi_sec->dhm);
    mbedtls_aes_free(&blufi_sec->aes);

    memset(blufi_sec, 0x0, sizeof(struct blufi_security));

    free(blufi_sec);
    blufi_sec = NULL;
}

esp_err_t ret;
// Функция включения blufi
esp_err_t start_blufi(void)
{
    initialise_wifi();

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
    ret = esp_blufi_controller_init();
    if (ret)
    {
        BLUFI_ERROR("%s BLUFI controller init failed: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
#endif

    ret = esp_blufi_host_and_cb_init(&callbacks);
    if (ret)
    {
        BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    BLUFI_INFO("BLUFI VERSION %04x\n", esp_blufi_get_version());

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    return ESP_OK;
}
