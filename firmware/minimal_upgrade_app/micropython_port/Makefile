include $(TOP)/py/mkenv.mk

QSTR_DEFS = qstrdefsport.h
MICROPY_ROM_TEXT_COMPRESSION ?= 1
FROZEN_MANIFEST = manifest.py

include $(TOP)/py/py.mk

CROSS_COMPILE ?= arm-none-eabi-

INC += -I.
INC += -I../include
INC += -I../third_party/libopencm3/include
INC += -I$(TOP)
INC += -I$(BUILD)

ARCH_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += $(INC) $(ARCH_FLAGS) -std=c11 -Os -g3 -Wall -Werror
CFLAGS += -ffunction-sections -fdata-sections -fno-common
CSUPEROPT = -Os

SRC_C = \
	est_micropython.c \
	modest.c \
	shared/runtime/gchelper_native.c \
	shared/runtime/stdout_helpers.c

SRC_S = shared/runtime/gchelper_thumb2.s
SRC_QSTR += modest.c

OBJ += $(PY_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_S:.s=.o))

LIB = $(BUILD)/libest_micropython.a

.PHONY: all

all: $(LIB)

$(LIB): $(OBJ)
	$(ECHO) "AR $@"
	$(Q)$(AR) rcs $@ $^

include $(TOP)/py/mkrules.mk
