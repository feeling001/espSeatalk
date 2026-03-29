.PHONY: all build flash monitor clean help setup-venv \
        upload erase \
        build-zero build-n16r8v \
        flash-zero flash-n16r8v

# ─────────────────────────────────────────────────────────────────────────────
# Board selection
#   Default board: esp32s3_zero
#   Override:      make flash BOARD=esp32s3_n16r8v
#
# Available boards:
#   esp32s3_zero       ESP32-S3 Zero    4MB Flash / 2MB PSRAM  (default)
#   esp32s3_n16r8v     ESP32-S3 N16R8  16MB Flash / 8MB PSRAM
# ─────────────────────────────────────────────────────────────────────────────
BOARD   ?= esp32s3_zero
PORT    ?= /dev/ttyACM0

# ─────────────────────────────────────────────────────────────────────────────
# Virtual environment
# ─────────────────────────────────────────────────────────────────────────────
VENV_DIR = .venv
VENV_BIN = $(VENV_DIR)/bin
PIO      = $(VENV_BIN)/pio

# Build output directory for the selected board
BUILD_DIR = .pio/build/$(BOARD)

# PlatformIO port flags
PIO_PORT_FLAG    = --upload-port $(PORT)
PIO_MONITOR_FLAG = --port $(PORT)

# ─────────────────────────────────────────────────────────────────────────────
# Internal helpers
# ─────────────────────────────────────────────────────────────────────────────

define section
	@echo ""
	@echo "── $(1) ──────────────────────────────────────────────"
endef

# ─────────────────────────────────────────────────────────────────────────────
# Virtual environment setup
# ─────────────────────────────────────────────────────────────────────────────
$(VENV_DIR):
	@echo "Creating Python virtual environment..."
	python3 -m venv $(VENV_DIR)
	$(VENV_BIN)/pip install --upgrade pip
	$(VENV_BIN)/pip install platformio
	@echo "✓ Virtual environment ready"

setup-venv: $(VENV_DIR)

# ─────────────────────────────────────────────────────────────────────────────
# Firmware build
# ─────────────────────────────────────────────────────────────────────────────
firmware: setup-venv
	$(call section,Building firmware [BOARD=$(BOARD)])
	$(PIO) run -e $(BOARD)

build: firmware

# ─────────────────────────────────────────────────────────────────────────────
# Upload targets
# ─────────────────────────────────────────────────────────────────────────────

# Upload firmware only
upload: setup-venv
	$(call section,Uploading firmware [BOARD=$(BOARD)])
	$(PIO) run -e $(BOARD) -t upload $(PIO_PORT_FLAG)


# flash = upload firmware + upload filesystem
flash: upload 

# ─────────────────────────────────────────────────────────────────────────────
# flash-all — full erase → firmware → filesystem
# ─────────────────────────────────────────────────────────────────────────────
flash-all: setup-venv firmware
	$(call section,Full flash sequence [BOARD=$(BOARD)])
	@echo "[1/2] Erasing flash..."
	$(PIO) run -e $(BOARD) -t erase $(PIO_PORT_FLAG)
	@echo "[2/2] Uploading firmware..."
	$(PIO) run -e $(BOARD) -t upload $(PIO_PORT_FLAG)
	@echo ""
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "✓ Flash complete [BOARD=$(BOARD)]"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# ─────────────────────────────────────────────────────────────────────────────
# Named aliases — ESP32-S3 Zero (4MB)
# ─────────────────────────────────────────────────────────────────────────────
build-zero:
	$(MAKE) build BOARD=esp32s3_zero

flash-zero:
	$(MAKE) flash BOARD=esp32s3_zero PORT=$(PORT)

flash-all-zero:
	$(MAKE) flash-all BOARD=esp32s3_zero PORT=$(PORT)

# ─────────────────────────────────────────────────────────────────────────────
# Named aliases — ESP32-S3 N16R8 (16MB)
# ─────────────────────────────────────────────────────────────────────────────
build-n16r8v:
	$(MAKE) build BOARD=esp32s3_n16r8v

flash-n16r8v:
	$(MAKE) flash BOARD=esp32s3_n16r8v PORT=$(PORT)

flash-all-n16r8v:
	$(MAKE) flash-all BOARD=esp32s3_n16r8v PORT=$(PORT)

# ─────────────────────────────────────────────────────────────────────────────
# Default target
# ─────────────────────────────────────────────────────────────────────────────
all: setup-venv build flash-all

# ─────────────────────────────────────────────────────────────────────────────
# Monitor
# ─────────────────────────────────────────────────────────────────────────────
monitor: setup-venv
	@echo "Opening serial monitor (board: $(BOARD), port: $(PORT))..."
	$(PIO) device monitor -e $(BOARD) $(PIO_MONITOR_FLAG)

# ─────────────────────────────────────────────────────────────────────────────
# Erase
# ─────────────────────────────────────────────────────────────────────────────
erase: setup-venv
	@echo "Erasing flash (board: $(BOARD), port: $(PORT))..."
	$(PIO) run -e $(BOARD) -t erase $(PIO_PORT_FLAG)
	@echo "✓ Flash erased"

# ─────────────────────────────────────────────────────────────────────────────
# Clean
# ─────────────────────────────────────────────────────────────────────────────
clean:
	@echo "Cleaning build artifacts..."
	@if [ -d $(VENV_DIR) ]; then $(PIO) run -e $(BOARD) -t clean 2>/dev/null || true; fi

clean-all: clean
	@echo "Cleaning dependencies..."
	rm -rf $(VENV_DIR)
	@echo "✓ Cleaned venv and dependencies"

# ─────────────────────────────────────────────────────────────────────────────
# Help
# ─────────────────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "ESP SeaTalk — Makefile"
	@echo ""
	@echo "Boards (BOARD=):"
	@echo "  esp32s3_zero     ESP32-S3 Zero    4MB Flash / 2MB PSRAM  (default)"
	@echo "  esp32s3_n16r8v   ESP32-S3 N16R8  16MB Flash / 8MB PSRAM"
	@echo ""
	@echo "Port (default: /dev/ttyACM0):"
	@echo "  All targets accept PORT=/dev/ttyUSB0 (or any path)"
	@echo ""
	@echo "Named targets (board pre-selected, PORT= still applies):"
	@echo "  make build-zero                        Build for ESP32-S3 Zero"
	@echo "  make build-n16r8v                      Build for ESP32-S3 N16R8"
	@echo "  make flash-zero                        Flash (firmware + fs)"
	@echo "  make flash-n16r8v"
	@echo "  make flash-all-zero                    Erase + full flash"
	@echo "  make flash-all-n16r8v"
	@echo ""
	@echo "Generic targets:"
	@echo "  make build            BOARD=...           Build"
	@echo "  make flash            BOARD=... PORT=...  Upload fw + fs"
	@echo "  make flash-all        BOARD=... PORT=...  Erase + full flash"
	@echo "  make upload           BOARD=... PORT=...  Upload firmware only"
	@echo "  make firmware         BOARD=...           Build only"
	@echo "  make monitor          BOARD=... PORT=...  Serial monitor"
	@echo "  make erase            BOARD=... PORT=...  Erase flash"
	@echo "  make clean            BOARD=...           Clean build artifacts"
	@echo "  make clean-all                        Clean everything"
	@echo "  make setup-venv                       Create Python venv"
	@echo ""
	@echo "Examples:"
	@echo "  make flash-all-n16r8v PORT=/dev/ttyUSB0"
	@echo "  make flash-all-zero   PORT=/dev/ttyACM0"
	@echo "  make build            BOARD=esp32s3_zero"
	@echo "  make monitor          PORT=/dev/ttyACM0"
	@echo ""


