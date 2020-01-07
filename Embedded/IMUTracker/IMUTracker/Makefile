GCC_BIN = /usr/local/bin/
OBJDIR = Builds
SOURCES = $(shell find -E . -regex '^.*\.(c(pp)?|[sS])$$' | sed -E -e 's/\.(c(pp)?|[sS])$$/.o/g')
OBJECTS = $(patsubst %,$(OBJDIR)/%,$(filter-out $(PROJECT).o,$(SOURCES)))
SYS_OBJECTS = $(shell find -E . -regex '^.*\.o$$' | sed -e 's/^.\/Builds\/.*$$//g')
INCLUDE_PATHS = -I. $(shell find -E . -regex '^.*\.h(pp)?$$' | sed -e 's/^\(.*\)\/[^\/]*$$/-I\1/g' | sort | uniq)
LIBRARY_PATHS = $(shell find -E . -regex '^.*\.a$$' | sed -e 's/^\(.*\)\/[^\/]*$$/-L\1/g' | sort | uniq)
LIBRARIES =
INTERFACE = cmsis-dap.cfg
# INTERFACE = jlink.cfg
# INTERFACE = stlink-v2-1.cfg
DEBUG = 0
INLINE_ALL = 1

############################################################################### 
AS      = $(GCC_BIN)arm-none-eabi-gcc
CC      = $(GCC_BIN)arm-none-eabi-gcc
CPP     = $(GCC_BIN)arm-none-eabi-g++
LD      = $(GCC_BIN)arm-none-eabi-gcc
OBJCOPY = $(GCC_BIN)arm-none-eabi-objcopy
OBJDUMP = $(GCC_BIN)arm-none-eabi-objdump
SIZE    = $(GCC_BIN)arm-none-eabi-size

CPU = -mcpu=cortex-m0plus -mthumb

AS_FLAGS = $(CPU) -c

CC_FLAGS = $(CPU) -c -Wall -Wextra -fno-common -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -MMD -MP

CC_SYMBOLS = -DCORE_M0PLUS -D__USE_ROMDIVIDE -D__LPC80X__ -D__CODE_RED

INCLUDE_PATHS += -I$(SRCROOT)/../LPC802/include

LD_FLAGS = $(CPU) -Wl,--gc-sections -Wl,--print-memory-usage
LD_SYS_LIBS = -lc_nano -lnosys

ifeq ($(DEBUG), 1)
	CC_FLAGS += -DDEBUG -O0 -g
else
	CC_FLAGS += -DNDEBUG -O2 -g
endif

ifeq ($(INLINE_ALL), 1)
	CC_FLAGS += -DINLINE="static inline __attribute__((always_inline))" -DSTATIC="" -DINLINE_ALL
	OBJECTS = Builds/cr_startup_lpc80x.o Builds/aeabi_romdiv_patch.o Builds/crp.o Builds/main.o
else
	CC_FLAGS += -DINLINE="" -DSTATIC="static"
endif

all: clean build upload

createObjDir:
	@if [ ! -d $(OBJDIR) ]; then mkdir $(OBJDIR); fi

build: createObjDir $(OBJDIR)/$(PROJECT)-Release.bin $(OBJDIR)/$(PROJECT)-Initial.bin

upload:
	@echo "Upload Release Firmware"
#	openocd -f interface/$(INTERFACE) -c "transport select swd" -f "$(SRCROOT)/../LPC802/lpc80x.cfg" -c "program $(OBJDIR)/$(PROJECT)-Release.elf verify reset exit"
	"$(SRCROOT)/../Flasher/flasher" "$(OBJDIR)/$(PROJECT)-Release.bin" 0

initialize:
	@echo "Upload Initial Firmware"
	openocd -f interface/$(INTERFACE) -c "transport select swd" -f "$(SRCROOT)/../LPC802/lpc80x.cfg" -f load.cfg -c "load $(SRCROOT)/../InitialLoader/InitialLoader/Builds/InitialLoader.elf"
	"$(SRCROOT)/../InitialUploader/initial_uploader" "$(OBJDIR)/$(PROJECT)-Initial.bin"

buildAndUpload: build upload

clean:
	@if [ -d $(OBJDIR) ]; then rm -rf $(OBJDIR); fi

debug: build
	openocd -f interface/$(INTERFACE) -c "transport select swd" -f "$(SRCROOT)/../LPC802/lpc80x.cfg" &
	arm-none-eabi-insight "$(SRCROOT)/$(PROJECT)/$(OBJDIR)/$(PROJECT)-Initial.elf"
	@killall openocd

$(OBJDIR)/%.o:	%.asm
	@echo "Assemble $<"
	@mkdir -p $(dir $@)
	$(AS) $(AS_FLAGS) -o $@ $<

$(OBJDIR)/%.o: 	%.s
	@echo "Assemble $<"
	@mkdir -p $(dir $@)
	$(AS) $(AS_FLAGS) -o $@ $<

$(OBJDIR)/%.o: 	%.S
	@echo "Assemble $<"
	@mkdir -p $(dir $@)
	$(AS) $(AS_FLAGS) -o $@ $<

$(OBJDIR)/%.o: 	%.c
	@echo "Compile $<"
	@mkdir -p $(dir $@)
	$(CC)  $(CC_FLAGS) $(CC_SYMBOLS) -std=gnu99 $(INCLUDE_PATHS) -o $@ "$(SRCROOT)/$(PROJECT)/$<"

$(OBJDIR)/%.d: %.c
	@echo "Generate dependency of $<"
	@mkdir -p $(dir $@)
	$(CC) -MM $(CC_FLAGS) $(CC_SYMBOLS) -std=gnu99 $(INCLUDE_PATHS) $< -MF $@ -MT $(@:.d=.o)

$(OBJDIR)/$(PROJECT)-Release.elf: $(OBJECTS) $(SYS_OBJECTS)
	@echo Link
	$(LD) $(LD_FLAGS) -TRelease.ld $(LIBRARY_PATHS) -o $@ $^ -Wl,--start-group $(LIBRARIES) $(LD_SYS_LIBS) -Wl,--end-group
	$(SIZE) $@

$(OBJDIR)/$(PROJECT)-Initial.elf: $(OBJECTS) $(SYS_OBJECTS)
	@echo Link
	$(LD) $(LD_FLAGS) -TInitial.ld $(LIBRARY_PATHS) -o $@ $^ -Wl,--start-group $(LIBRARIES) $(LD_SYS_LIBS) -Wl,--end-group
	$(SIZE) $@

$(OBJDIR)/%.bin: $(OBJDIR)/%.elf
	@echo Copy
	$(OBJCOPY) -O binary $< $@

-include $(OBJECTS:.o=.d)
