CC = gcc
CFLAGS = -Wall -g

BUILD_DIR = build
TARGET = ant

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
