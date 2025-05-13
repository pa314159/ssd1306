#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_random.h>

void app_main(void)
{
#if !CONFIG_SSD1306_SPLASH
# error no splash to display
#endif

	ssd1306_init_t init = ssd1306_create_init();
	ssd1306_t device = ssd1306_init(init);

	ssd1306_status(device, ssd1306_status_0, "initialised...");

	TickType_t ticks = xTaskGetTickCount();

	for( int k = 9; k > 0; k-- ) {
		ssd1306_status(device, ssd1306_status_1, "restart in %ds...", k);

		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	}

	ssd1306_clear_b(device, NULL);
	esp_restart();
}
