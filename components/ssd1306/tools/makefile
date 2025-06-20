
TOOLS = bdf2fnt

SOURCE = .
OUTPUT = .

CFLAGS = -g -Wall

VPATH = $(SOURCE):$(OUTPUT)

all:: $(OUTPUT) $(TOOLS)

clean::
	rm -f $(TOOLS)

$(OUTPUT):
	mkdir -p $@

%: %.c
	$(CC) $(CFLAGS) -o $(OUTPUT)/$* $<

