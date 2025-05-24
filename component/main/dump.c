
#include <stdio.h>
#include <stdio.h>
#include <esp_check.h>

#define NUM_DIGITS 32

void ssd1306_dump_it(const void* data, uint16_t size, const char* format, ...)
{
	const uint8_t* buff = (uint8_t*)data;
	char text[2 + 8 + 2 + 4*NUM_DIGITS + NUM_DIGITS / 8 + 1];

	uint16_t tpos = 0;
	uint16_t offs = 0;

	va_list args;

	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	for( ; offs < size; offs++, buff++ ) {
		const unsigned rest = offs % NUM_DIGITS;

		if( rest == 0 ) {
			if( text[0] ) {
				printf("%s\n", text);
			}

			tpos = 0;
			tpos += snprintf(text + tpos, sizeof(text) - tpos, "%p:", buff);
		}

		if( rest > 11 && rest % 8 == 0 ) {
			text[tpos++] = ' ';
		}

		tpos += snprintf(text + tpos, sizeof(text) - tpos, " %02x", *buff);

		ESP_RETURN_VOID_ON_FALSE(tpos <= sizeof(text), "SSD1306", "tpos = %u, offs = %u", tpos, offs);
	}

	if( tpos ) {
		printf("%s\n", text);
	}
}
