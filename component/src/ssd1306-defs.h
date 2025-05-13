
/**
 * Borrowed from
 * - https://robotcantalk.blogspot.com/2015/03/interfacing-arduino-with-ssd1306-driven.html
 * - https://github.com/nopnop2002/esp-idf-ssd1306/blob/master/components/ssd1306/ssd1306.h
 */

 // SLA (0x3C) + WRITE_MODE (0x00) =  0x78 (0b01111000)
#define OLED_I2C_ADDRESS                0x3C

// Control byte
#define OLED_CTL_BYTE_CMD_SINGLE        0x80
#define OLED_CTL_BYTE_CMD_STREAM        0x00
#define OLED_CTL_BYTE_DATA_STREAM       0x40

// Fundamental commands (pg.28)
#define OLED_CMD_SET_CONTRAST           0x81
#define OLED_CMD_DISPLAY_RAM            0xA4
#define OLED_CMD_DISPLAY_NORMAL         0xA6
#define OLED_CMD_DISPLAY_OFF            0xAE
#define OLED_CMD_DISPLAY_ON             0xAF

// Addressing Command Table (pg.30)
#define OLED_CMD_SET_MEMORY_ADDR_MODE   0x20	// OR with one of the follow addressing modes
#define OLED_CMD_SET_HORI_ADDR_MODE     0x00    // Horizontal Addressing Mode
#define OLED_CMD_SET_VERT_ADDR_MODE     0x01    // Vertical Addressing Mode
#define OLED_CMD_SET_PAGE_ADDR_MODE     0x02    // Page Addressing Mode

#define OLED_CMD_SET_COLUMN_RANGE       0x21    // can be used only in HORZ/VERT mode
#define OLED_CMD_SET_PAGE_RANGE         0x22    // can be used only in HORZ/VERT mode
#define OLED_CMD_SET_PAGE_ADDRESS(num)  (0xB0 | (num))    // can be used only in PAGE mode - OR with page number

#define OLED_CMD_SET_COLUMN_LOW(col)    ((col) & 0xF)
#define OLED_CMD_SET_COLUMN_HIGH(col)   (0x10 | (((col) >> 4) & 0xF))

// Hardware Config (pg.31)
#define OLED_CMD_SET_DISPLAY_START_LINE 0x40
#define OLED_CMD_SET_SEGMENT_REMAP      0xA0    // SEG0...SEG127 or SEG127...SEG0 (|0x01)
#define OLED_CMD_SET_MUX_RATIO          0xA8    // follow with 0x3F = 64 MUX
#define OLED_CMD_SET_COM_SCAN_MODE      0xC0    // COM0...COM[N-1] or COM[N-1]...COM0 (|0x08)
#define OLED_CMD_SET_DISPLAY_OFFSET     0xD3    // follow with 0x00
#define OLED_CMD_SET_COM_PIN_MAP        0xDA    // follow with 0x12
#define OLED_CMD_NOP                    0xE3    // NOP

// Timing and Driving Scheme (pg.32)
#define OLED_CMD_SET_DISPLAY_CLK_DIV    0xD5    // follow with 0x80
#define OLED_CMD_SET_PRECHARGE          0xD9    // follow with 0xF1
#define OLED_CMD_SET_VCOMH_DESELCT      0xDB    // follow with 0x30

// Charge Pump (pg.62)
#define OLED_CMD_SET_CHARGE_PUMP        0x8D    // follow with 0x14

// Scrolling Command
#define OLED_CMD_HORIZONTAL_RIGHT       0x26
#define OLED_CMD_HORIZONTAL_LEFT        0x27
#define OLED_CMD_CONTINUOUS_SCROLL      0x29
#define OLED_CMD_DEACTIVE_SCROLL        0x2E
#define OLED_CMD_ACTIVE_SCROLL          0x2F
#define OLED_CMD_VERTICAL               0xA3
