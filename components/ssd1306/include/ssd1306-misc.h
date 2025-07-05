#if !defined(__SSD1306_MISC_H)
#define __SSD1306_MISC_H

#if !defined(__SSD1306_H)
#error must include <ssd1306.h> first
#endif

#if defined(__cplusplus)
extern "C" {
#endif

void ssd1306_fill_randomly(ssd1306_t device, const ssd1306_bounds_t* bounds);

#if defined(__cplusplus)
}
#endif

#endif
