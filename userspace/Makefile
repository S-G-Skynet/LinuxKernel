CC      := gcc

CFLAGS  := \
	-std=gnu11 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-O2

TARGET  := simplefs_cli

all: $(TARGET)

$(TARGET): simplefs_cli.c ../simplefs.h
	$(CC) $(CFLAGS) -o $@ simplefs_cli.c

clean:
	rm -f $(TARGET)

.PHONY: all clean