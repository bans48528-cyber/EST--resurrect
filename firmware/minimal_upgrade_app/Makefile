PROJECT := est_minimal_upgrade_app
BUILD_DIR ?= build
OPENCM3_DIR := third_party/libopencm3
APP_VERSION ?= M0.01A

PREFIX ?= arm-none-eabi
CC := $(PREFIX)-gcc
OBJCOPY := $(PREFIX)-objcopy
OBJDUMP := $(PREFIX)-objdump
SIZE := $(PREFIX)-size
PYTHON ?= python
MKDIR_P ?= mkdir -p

ARCH_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CPPFLAGS := -DSTM32F4 -Iinclude -I$(OPENCM3_DIR)/include \
	-I$(OPENCM3_DIR)/lib/usb \
	-DAPP_VERSION_TEXT=\"$(APP_VERSION)\"
ifeq ($(DIAGNOSTIC_SKIP_LCD_STARTUP),1)
CPPFLAGS += -DDIAGNOSTIC_SKIP_LCD_STARTUP=1
endif
CFLAGS := $(ARCH_FLAGS) -std=c11 -Os -g3 -Wall -Wextra -Werror \
	-Wshadow -Wundef -Wstrict-prototypes -Wmissing-prototypes \
	-ffunction-sections -fdata-sections -fno-common
LDFLAGS := $(ARCH_FLAGS) -Tmemory.ld -L$(OPENCM3_DIR)/lib -nostartfiles \
	-Wl,--gc-sections -Wl,--cref -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map
LDLIBS := -lopencm3_stm32f4 -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/obj/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
ELF := $(BUILD_DIR)/$(PROJECT).elf
BIN := $(BUILD_DIR)/$(PROJECT).bin
HEX := $(BUILD_DIR)/$(PROJECT).hex
LST := $(BUILD_DIR)/$(PROJECT).lst
PACKAGE := $(BUILD_DIR)/$(PROJECT).upgrade.bin
APP_PACKAGE := $(BUILD_DIR)/$(PROJECT).app.bin
MANIFEST := $(BUILD_DIR)/$(PROJECT).manifest.json
LIBOPENCM3 := $(OPENCM3_DIR)/lib/libopencm3_stm32f4.a

.PHONY: all clean lib test verify verification-pair

all: $(ELF) $(BIN) $(HEX) $(LST) $(PACKAGE) verify

$(BUILD_DIR)/obj/%.o: src/%.c
	@$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(ELF): $(OBJECTS) $(LIBOPENCM3) memory.ld
	@$(MKDIR_P) $(dir $@)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@
	$(SIZE) $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(LST): $(ELF)
	$(OBJDUMP) -h -S $< > $@

$(PACKAGE): $(BIN) tools/package_firmware.py
	$(PYTHON) tools/package_firmware.py --input $(BIN) \
		--output-dir $(BUILD_DIR) --stem $(PROJECT) --version $(APP_VERSION)

verify: $(ELF) $(BIN) $(PACKAGE) tools/verify_build.py
	$(PYTHON) tools/verify_build.py --objdump $(OBJDUMP) --elf $(ELF) \
		--bin $(BIN) --app-package $(APP_PACKAGE) \
		--upgrade-package $(PACKAGE) --manifest $(MANIFEST) \
		--version $(APP_VERSION)

$(LIBOPENCM3):
	$(MAKE) -C $(OPENCM3_DIR) TARGETS=stm32/f4

lib:
	$(MAKE) -C $(OPENCM3_DIR) TARGETS=stm32/f4

test:
	$(PYTHON) -m unittest discover -s tests -v
	$(PYTHON) tools/check_hardware_pins.py --repo-root ../..

verification-pair:
	$(MAKE) BUILD_DIR=build/v1 APP_VERSION=M0.01A all
	$(MAKE) BUILD_DIR=build/v2 APP_VERSION=M0.02A all
	$(PYTHON) tools/verify_build.py \
		--compare-bin build/v1/$(PROJECT).bin build/v2/$(PROJECT).bin \
		--compare-version M0.01A M0.02A

clean:
	$(RM) -r build

-include $(DEPS)
