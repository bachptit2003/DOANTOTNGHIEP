# Makefile for LoRa Gateway
# BeagleBone Black LoRa Gateway Project

CC = gcc
CFLAGS = -Wall -O2 -Iinclude
LDFLAGS = -lcjson -lmosquitto -lsqlite3 -lpthread

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INCLUDE_DIR = include

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/gateway

# Colors for output
COLOR_RESET = \033[0m
COLOR_BOLD = \033[1m
COLOR_GREEN = \033[32m
COLOR_BLUE = \033[34m

.PHONY: all clean install uninstall help

all: $(TARGET)
	@echo "$(COLOR_GREEN)$(COLOR_BOLD)✓ Build complete!$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)Run: sudo ./$(TARGET)$(COLOR_RESET)"

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@echo "$(COLOR_BLUE)Linking...$(COLOR_RESET)"
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "$(COLOR_BLUE)Compiling $<...$(COLOR_RESET)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	@mkdir -p $@

clean:
	@echo "$(COLOR_BLUE)Cleaning build files...$(COLOR_RESET)"
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "$(COLOR_GREEN)✓ Clean complete!$(COLOR_RESET)"

install: $(TARGET)
	@echo "$(COLOR_BLUE)Installing gateway to /usr/local/bin/...$(COLOR_RESET)"
	sudo cp $(TARGET) /usr/local/bin/lora_gateway
	sudo chmod +x /usr/local/bin/lora_gateway
	@echo "$(COLOR_GREEN)✓ Installation complete!$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)Run: sudo lora_gateway$(COLOR_RESET)"

uninstall:
	@echo "$(COLOR_BLUE)Uninstalling gateway...$(COLOR_RESET)"
	sudo rm -f /usr/local/bin/lora_gateway
	@echo "$(COLOR_GREEN)✓ Uninstall complete!$(COLOR_RESET)"

help:
	@echo "$(COLOR_BOLD)LoRa Gateway Build System$(COLOR_RESET)"
	@echo ""
	@echo "$(COLOR_BLUE)Available targets:$(COLOR_RESET)"
	@echo "  make          - Build the gateway"
	@echo "  make clean    - Remove build files"
	@echo "  make install  - Install to /usr/local/bin"
	@echo "  make uninstall- Remove from system"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "$(COLOR_BLUE)Dependencies:$(COLOR_RESET)"
	@echo "  - libcjson-dev"
	@echo "  - libmosquitto-dev"
	@echo "  - libsqlite3-dev"
	@echo ""
	@echo "$(COLOR_BLUE)Install dependencies:$(COLOR_RESET)"
	@echo "  sudo apt-get install libcjson-dev libmosquitto-dev libsqlite3-dev"
