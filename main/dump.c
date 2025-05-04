#include "sdkconfig.h"
#include "ssd1306.h"
#include "ssd1306-priv.h"

#define NUM_DIGITS 32

void ssd1306_dump(const void* data, size_t size, const char* format, ...)
{
	const uint8_t* buff = (uint8_t*)data;
	char text[2 + 8 + 2 + 4*NUM_DIGITS + 1];

	size_t tpos = 0;
	size_t offs = 0;

	va_list args;

	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	for( ; offs < size; offs++, buff++ ) {
		const unsigned rest = offs % NUM_DIGITS;

		if( rest == 0 && text[0] ) {
			LOG_V("%s", text);

			tpos = 0;
			tpos += snprintf(text + tpos, sizeof(text) - tpos, "%p: ", buff);
		}

		tpos += snprintf(text + tpos, sizeof(text) - tpos, " %02x", *buff);

		ESP_RETURN_VOID_ON_FALSE(tpos <= sizeof(text), LOG_DOMAIN, "tpos = %u, offs = %u", tpos, offs);
	}

	if( offs % NUM_DIGITS ) {
		LOG_V("%s", text);
	}
}
