#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>

#define TAG "MAIN"

void app_main(void)
{
	ssd1306_init_t init = ssd1306_create_init();
	ssd1306_t device = ssd1306_init(init);

	ESP_LOGI(TAG, "size = %ux%u, font['@'][0] =  %02x", init->width, init->height, init->font['@'][0]);

	// ssd1306_text(device, "hello", 0, 0, 5);

	// vTaskDelay(pdMS_TO_TICKS(60000));

	// esp_restart();
}
