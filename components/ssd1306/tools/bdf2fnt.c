
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define _countof(x) (sizeof(x)/sizeof(x[0]))

static bool ends_with(const char *str, char *end)
{
	int str_len = strlen(str);
	int end_len = strlen(end);
  
	if( end_len > str_len ) {
		return false;
	}

	while( end_len > 0 ) {
		if( str[--str_len] != end[--end_len] ) {
			return false;
		}
	}

	return true;
}

static struct {
	const char* program;

	unsigned from;
	char* output;
	unsigned to;
	int verbosity;

	const char* input;
} options = {
	.from = 0,
	.to = 255,
};

static int get_next_option(int argc, char* const argv[])
{
	static struct option long_options[] = {
		{ "from", required_argument, 0, 0 },
		{ "help", no_argument, 0, 0 },
		{ "output", required_argument, 0, 0 },
		{ "to", required_argument, 0, 0 },
		{ "verbose", no_argument, 0, 0 },
	};
	static int short_options[] = { 'f', 'h', 'o', 't', 'v', };

	assert(_countof(long_options) == _countof(short_options));

	int pos;
	int opt = getopt_long(argc, argv, "f:h?o:t:v", long_options, &pos);

	if( opt < 0 ) {
		return -1;
	}
	if( opt == 0 ) {
		assert(pos < _countof(short_options));

		opt = short_options[pos];
	}

	return opt;
}

static int parse_int_value(const char* value, char** endp, int min, int max, const char* format, ... ) {
	char* next;
	long result = strtol(value, &next, 10);

	if( next[0] && !isspace(next[0])) {
		va_list args;

		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);

		fprintf(stderr, ": invalid integer \"%s\"\n", value);

		exit(1);
	}

	if( result < min || result > max ) {
		va_list args;

		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);

		fprintf(stderr, ": invalid range, expecting [%d, %d]\n", min, max);

		exit(1);
	}

	if( endp ) {
		*endp = next;
	}

	return result;
}

static void usage(int code)
{
	fprintf(stderr, "Usage: %s [OPTIONS] <BDFFILE>\n", options.program);
	exit(code);
}

static bool is_missing_or_is_file(const char* file)
{
	int fd = open(options.output, O_RDONLY);

	if( fd == -1 ) {
		return true;
	}

	struct stat st;

	if( fstat(fd, &st) < 0 ) {
		perror(options.output);
		exit(1);
	}

	close(fd);

	return S_ISREG(st.st_mode);
}

static void parse_options(int argc, char* const argv[]) {
	options.program = argv[0];

	while( true ) {
		int opt = get_next_option(argc, argv);

		if( opt < 0 ) {
			break;
		}

		switch (opt) {
			case 'f':
				options.from = parse_int_value(optarg, NULL, 0, 255, "option -f/--from");
			break;

			case 'h':
			case '?':
				usage(0);
			break;

			case 'o':
				options.output = strdup(optarg);
			break;

			case 't':
				options.from = parse_int_value(optarg, NULL, 0, 255, "option -t/--to");
			break;

			case 'v':
				options.verbosity++;
			break;

			default:
				usage(1);
			break;
		}
	}

	if( optind == argc ) {
		usage(1);
	}

	options.input = argv[optind];

	if( !ends_with(options.input, ".bdf") ) {
		fprintf(stderr, "invalid extension of input file, should end with \".bdf\"\n");
		exit(1);
	}

	if( !options.output ) {
		options.output = strdup(options.input);

		int len = strlen(options.output);

		strcpy(options.output + len - 3, "fnt" );
	}

	if( !ends_with(options.output, ".fnt") && is_missing_or_is_file(options.output) ) {
		fprintf(stderr, "invalid extension of output file, should end with \".fnt\"\n");
		exit(1);
	}
}

typedef enum {
	state_idle,
	state_glyph,
	state_bitmap,
} bdf_state_t;

typedef struct {
	char name[16];
	unsigned width;
	union {
		unsigned index;
		unsigned height;
	};
	int bbx[4];
	unsigned encoding;
	uint16_t image[0];
} bdf_glyph_t;

typedef struct {
	uint8_t width;
	uint8_t height;
	uint8_t image[0];
} bdf_bitmap_t;

typedef struct {
	char name[80];
	unsigned ascent;
	unsigned descent;
	unsigned width;
	unsigned height;
	unsigned chars;

	// extra info
	FILE* input;
	FILE* output;
	char buff[1024];
	unsigned line;
	bdf_state_t state;
} bdf_info_t;

static void fail(bdf_info_t* info, const char* format, ...)
{
	va_list args;

	if( info->line > 0 ) {
		fprintf(stderr, "Failure at line %d, state %d: %s\n",
			info->line, info->state, info->buff);
	}

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	fputs("\n", stderr);

	if( info->input ) {
		fclose(info->input);
	}
	if( info->output ) {
		fclose(info->output);
	}

	exit(1);
}

static char* read_line(bdf_info_t* info)
{
	if( !fgets(info->buff, sizeof(info->buff), info->input) ) {
		return NULL;
	}

	info->line++;

	if( !ends_with(info->buff, "\n") ) {
		fail(info, "line too long");
	}

	if( options.verbosity > 3 ) {
		printf("read line %d as %s\n", info->line, info->buff);
	}

	for( char* p = info->buff + strlen(info->buff)-1; p >= info->buff && isspace(*p); p-- ) {
		*p = 0;
	}

	return info->buff;
}

static bool is_token(char** const line, const char* token)
{
	char* temp = *line;

	while( *temp && !isspace(*temp) ) {
		if( *temp++ != *token++ ) {
			return false;
		}
	}

	if( isspace(*temp) ) {
		*line = temp+1;

		return true;
	}

	return *temp == 0;
}

static bdf_info_t* parse_bdf_info()
{
	bdf_info_t* info = calloc(1, sizeof(bdf_info_t));

	if( (info->input = fopen(options.input, "r")) == NULL ) {
		fail(info, "cannot open input file: %s", strerror(errno));
	}
	if( (info->output = fopen(options.output, "w")) == NULL ) {
		fail(info, "cannot open output file: %s", strerror(errno));
	}

	info->line = 1;
	info->state = state_idle;

	char* line;

	while( (line = read_line(info)) ) {
		if( is_token(&line, "FONT") ) {
			strncpy(info->name, line, sizeof(info->name));

			continue;
		}
		if( is_token(&line, "FONT_ASCENT") ) {
			info->ascent += strtoul(line, &line, 10);
			info->height = info->ascent + info->descent;

			continue;
		}
		if( is_token(&line, "FONT_DESCENT") ) {
			info->descent += strtoul(line, &line, 10);
			info->height = info->ascent + info->descent;

			continue;
		}
		if( is_token(&line, "FIGURE_WIDTH") ) {
			info->width += strtoul(line, &line, 10);

			continue;
		}
		if( is_token(&line, "CHARS") ) {
			info->chars = strtoul(line, &line, 10);

			return info;
		}
	}

	fail(info, "unexpected EOF");

	return 0;
}

static bdf_glyph_t* parse_bdf_glyph(bdf_info_t* info)
{
	bdf_glyph_t* glyph = NULL;
	char* line;

	while( (line = read_line(info)) ) {
		switch( info->state ) {
			case state_idle:
				if( is_token(&line, "STARTCHAR") ) {
					glyph = calloc(1, sizeof(bdf_glyph_t) + (info->height) * sizeof(glyph->image[0]));

					strncpy(glyph->name, line, 15);

					info->state = state_glyph;

					continue;
				}
			continue;

			case state_glyph:
				if( is_token(&line, "DWIDTH") ) {
					glyph->width = strtoul(line, &line, 10);

					continue;
				}

				if( is_token(&line, "ENCODING") ) {
					glyph->encoding = strtoul(line, &line, 10);

					continue;
				}
				if( is_token(&line, "BBX") ) {
					glyph->bbx[0] = strtoul(line, &line, 10);
					glyph->bbx[1] = strtoul(line, &line, 10);
					glyph->bbx[2] = strtoul(line, &line, 10);
					glyph->bbx[3] = strtoul(line, &line, 10);

					if( options.verbosity > 2 ) {
						printf("BBX = %d, %d, %d, %d\n",
							glyph->bbx[0], glyph->bbx[1], glyph->bbx[2], glyph->bbx[3]);
					}

					continue;
				}
				if( is_token(&line, "BITMAP") ) {
					info->state = state_bitmap;

					glyph->index = info->height - (glyph->bbx[1] + (info->descent + glyph->bbx[3]));

					if( options.verbosity > 2 ) {
						printf("BITMAP %s starts at index = %d\n", glyph->name, glyph->index);
					}

					continue;
				}
			continue;

			case state_bitmap:
				if( is_token(&line, "ENDCHAR") ) {
					info->state = state_idle;

					glyph->height = info->height;

					return glyph;
				}

				if( glyph->index < info->height ) {
					glyph->image[glyph->index++] = strtoul(line, &line, 16) >> glyph->bbx[2];
				} else {
					fail(info, "glyph has too many bytes, index = %u, height = %u", glyph->index, info->height);
				}

			continue;
		}

		fail(info, "encountered invalid token");
	}

	return glyph;
}

void write_bitmap(bdf_info_t* info, const bdf_bitmap_t* bitmap)
{
	assert(sizeof(bitmap->width) == 1);
	assert(sizeof(bitmap->height) == 1);

	fwrite(&bitmap->width, 1, 1, info->output);
	fwrite(&bitmap->height, 1, 1, info->output);

	unsigned rows = (unsigned)bitmap->height / 8 + (unsigned)((bitmap->height % 8) ? 1 : 0);

	for( unsigned row = 0; row < rows; row++ ) {
		fwrite(bitmap->image + row * bitmap->width, bitmap->width, 1, info->output);
	}
}

bool get_glyph_pixel(const bdf_glyph_t* glyph, unsigned x, unsigned y)
{
	assert(x < glyph->width);
	assert(y < glyph->height);

	unsigned position = (x / 8) * glyph->height + y;
	unsigned s_mask = 0x80 >> (x % 8);

	return glyph->image[position] & s_mask;
}

void set_bitmap_pixel(bdf_bitmap_t* bitmap, unsigned x, unsigned y, bool value)
{
	assert(x < bitmap->width);
	assert(y < bitmap->height);

	unsigned position = (y / 8) * bitmap->width + x;
	unsigned s_mask = 0x01 << (y % 8);
	unsigned d_mask = ~s_mask;

	bitmap->image[position] &= d_mask;

	if( value ) {
		bitmap->image[position] |= s_mask;
	}
}

bdf_bitmap_t* rotate_glyph(const bdf_glyph_t* glyph)
{
	unsigned pages = glyph->height / 8 + glyph->height % 8 ? 1 : 0;

	bdf_bitmap_t* bitmap = calloc(1, sizeof(bdf_bitmap_t) + pages * glyph->width);

	bitmap->width = glyph->width;
	bitmap->height = glyph->height;

	for( unsigned x = 0; x < bitmap->width; x++ ) {
		for( unsigned y = 0; y < bitmap->height; y++ ) {
			set_bitmap_pixel(bitmap, x, y, get_glyph_pixel(glyph, x, y));
		}
	}

	return bitmap;
}

int main(int argc, char* const argv[])
{
	parse_options(argc, argv);

	if( options.verbosity > 0 ) {
		printf("input is %s\n", options.input);
		printf("output is %s\n", options.output);
	}

	bdf_info_t* info = parse_bdf_info();

	bdf_glyph_t* glyph;

	while( (glyph = parse_bdf_glyph(info)) ) {
		if( options.verbosity > 0 ) {
			printf("read glyph %s\n", glyph->name);
		}
		if( options.verbosity > 1 ) {
			printf("\twidth %u\n", glyph->width);
			printf("\theight %u\n", glyph->height);
		}
		if( options.verbosity > 1 ) {
			printf("\tbitmap ");

			for( int b = 0; b < glyph->height; b++ ) {
				printf("%0*x ", glyph->width/4, glyph->image[b]);
			}

			printf("\n");
		}

		if( info->width && glyph->width > info->width ) {
			fail(info, "invalid glyph size %dx%d exceeding %dx%d",
				 glyph->width, glyph->height, info->width, info->height);
		}
		if( info->height && glyph->height > info->height ) {
			fail(info, "invalid glyph size %dx%d exceeding %dx%d",
				 glyph->width, glyph->height, info->width, info->height);
		}

		if( glyph->encoding >= options.from && glyph->encoding <= options.to ) {
			bdf_bitmap_t* bitmap = rotate_glyph(glyph);

			write_bitmap(info, bitmap);

			free(bitmap);
		}

		free(glyph);
	}

	fclose(info->input);
	fclose(info->output);

	return 0;
}
