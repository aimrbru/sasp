#ifndef WEB_H
#define WEB_H

#include <stdint.h>
#include "utils.h"
#include "esp_http_server.h"

extern const char* home_html;

extern TimerHandle_t inactivity_timer; // Таймер для отслеживания бездействия
extern httpd_handle_t server; // Дескриптор HTTP-сервера
extern int client_fd; // Дескриптор клиента для WebSocket

httpd_handle_t start_webserver(void);
void stop_webserver(void);
void inactivity_timer_callback(TimerHandle_t xTimer);

void register_custom_logger();
esp_err_t send_file_to_server(const char *server_url, const char *filename);

#endif // WEB_H

