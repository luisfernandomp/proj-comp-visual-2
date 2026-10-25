CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -g
TARGET = proj2

CFLAGS += $(shell pkg-config --cflags sdl3 sdl3-image sdl3-ttf zbar)
LDFLAGS = $(shell pkg-config --libs sdl3 sdl3-image sdl3-ttf zbar)
LDFLAGS += -lm

ifeq ($(OS),Windows_NT)
    TARGET := $(TARGET).exe
    LDFLAGS += -mconsole
endif

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
