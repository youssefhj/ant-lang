CC = gcc
CFLAGS = #-Wall -pg

BUILD_DIR = build
TARGET = ant

SRCS = main.c \
       compiler/compiler.c \
       compiler/scanner/scanner.c \
       vm/vm.c \
       vm/chunk/chunk.c \
       vm/disassembler/debug.c \
       vm/memory/memory.c \
       vm/hash-table/table.c \
       vm/value/value.c \
       vm/value/object/object.c \

OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
