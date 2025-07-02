#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"

static void ssd1306_text_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const char* format, va_list args);
static ssd1306_bitmap_t* ssd1306_text_bitmapv(ssd1306_t device, const char* format, va_list args);
static char* ssd1306_text_formatv(uint16_t* length, const char* format, va_list args);

static const char TEXT_SEPA[] = " \x4 ";
static const unsigned TEXT_SEPA_Z = 3;

void ssd1306_status(ssd1306_t device, ssd1306_status_t status, const char* format, ...)
{
	if( !ssd1306_acquire(device) ) {
		LOG_W("couldn't take mutex");

		return;
	}

	ssd1306_int_t dev = (ssd1306_int_t)device;
	const uint16_t index = ssd1306_status_index(dev, status);
	status_info_t* si = &dev->statuses[index];

	if( si->bitmap ) {
		free(si->bitmap);

		si->bitmap = NULL;
	}

	const ssd1306_bounds_t* bounds = (ssd1306_bounds_t*)si;

	LOG_D("status info at index %u(%u)", index, status);
	LOG_BOUNDS_D("              bounds", bounds);

	ssd1306_clear_internal(device, bounds);

	if( format ) {
		ssd1306_bitmap_t* bitmap;
		va_list args;

		va_start(args, format);
		bitmap = ssd1306_text_bitmapv(device, format, args);
		va_end(args);

		ssd1306_bounds_t trimmed = *bounds;

		ssd1306_trim(device, &trimmed, &bitmap->size);
		ssd1306_draw_internal(device, bounds, &trimmed, bitmap);

		if( bitmap->w > device->w ) {
			si->bitmap = bitmap;
			si->state = anim_init;

			LOG_D("text will scroll in background");
		} else {
			free(bitmap);
		}
	}

	ssd1306_update(device, bounds);
	ssd1306_release(device);
}

const ssd1306_bounds_t* ssd1306_status_bounds(ssd1306_t device, ssd1306_status_t status, ssd1306_bounds_t* target)
{
	ssd1306_int_t dev = (ssd1306_int_t)device;
	const uint16_t index = ssd1306_status_index(dev, status);
	const ssd1306_bounds_t* result = (ssd1306_bounds_t*)&dev->statuses[index];

	if( target ) {
		memcpy(target, result, sizeof(ssd1306_bounds_t));
	}

	return result;
}

void ssd1306_text(ssd1306_t device, const ssd1306_bounds_t* bounds, const char* format, ...)
{
	va_list args;

	va_start(args, format);
	ssd1306_text_internal(device, bounds, format, args);
	va_end(args);
}

uint16_t ssd1306_text_width(ssd1306_t device, const char* text)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(text);

	uint16_t width = 0;

	for( ; *text; text++ ) {
		if( *text == CONFIG_SSD1306_TEXT_INVERT ) {
			continue;
		}

		width += device->font[(uint8_t)(*text)].w;
	}

	return width;
}

ssd1306_bitmap_t* ssd1306_text_bitmap(ssd1306_t device, const char* format, ...)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(format);

	ssd1306_bitmap_t* bitmap;
	va_list args;

	va_start(args, format);
	bitmap = ssd1306_text_bitmapv(device, format, args);
	va_end(args);

	return bitmap;
}

void ssd1306_text_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const char* format, va_list args)
{
	ssd1306_bitmap_t* bitmap = ssd1306_text_bitmapv(device, format, args);
	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, &bitmap->size) ) {
		LOG_W("not visible");

		free(bitmap);

		return;
	}
	if( !ssd1306_acquire(device) ) {
		free(bitmap);

		LOG_W("couldn't take mutex");

		return;
	}

	ssd1306_draw_internal(device, bounds, &trimmed, bitmap);

	free(bitmap);

	ssd1306_update(device, &trimmed);
	ssd1306_release(device);
}

ssd1306_bitmap_t* ssd1306_text_bitmapv(ssd1306_t device, const char* format, va_list args)
{
	uint16_t length;
	char* text = ssd1306_text_formatv(&length, format, args);

	uint16_t width = ssd1306_text_width(device, text);

	if( width > device->w ) {
		length += TEXT_SEPA_Z;

		strcat(text, TEXT_SEPA);

		width = ssd1306_text_width(device, text);
	}

	ssd1306_bitmap_t* bitmap = ssd1306_create_bitmap((ssd1306_size_t){ width, SSD1306_TEXT_HEIGHT });

	bool invert = false;

	for( uint16_t index = 0, offset = 0; index < length && offset < width; index++ ) {
		if( text[index] == CONFIG_SSD1306_TEXT_INVERT ) {
			invert = !invert;

			continue;
		}

		const uint16_t cpos = text[index];
		const ssd1306_glyph_t* glyph = device->font + cpos;

		if( invert ) {
			for( int k = 0; k < glyph->w; k++ ) {
				bitmap->image[offset + k] = ~glyph->image[k];
			}
		} else {
			memcpy(bitmap->image + offset, glyph->image, glyph->w);
		}

		offset += glyph->w;
	}

	free(text);

	return bitmap;
}

char* ssd1306_text_formatv(uint16_t* length, const char* format, va_list args)
{
	uint16_t needed = vsnprintf(NULL, 0, format, args) + 1;
	char* text = malloc(needed + TEXT_SEPA_Z);

	ABORT_IF(text == NULL, "cannot allocate memory for text of size %u", needed);

	vsnprintf(text, needed, format, args);

	LOG_D("text formatted as \"%s\"", text);

	*length = needed;

	return text;
}
