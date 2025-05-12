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

	ssd1306_clear_b(device, NULL);
	ssd1306_status(device, ssd1306_status_0, "idle...");
	vTaskDelay(pdMS_TO_TICKS(10000));
	ssd1306_status(device, ssd1306_status_1, "restart...");
	vTaskDelay(pdMS_TO_TICKS(2500));

	esp_restart();
}
