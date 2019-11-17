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

############################################################################### 
AS      = $(GCC_BIN)arm-none-eabi-gcc
CC      = $(GCC_BIN)arm-none-eabi-gcc
CPP     = $(GCC_BIN)arm-none-eabi-g++
LD      = $(GCC_BIN)arm-none-eabi-gcc
OBJCOPY = $(GCC_BIN)arm-none-eabi-objcopy
OBJDUMP = $(GCC_BIN)arm-none-eabi-objdump
SIZE    = $(GCC_BIN)arm-none-eabi-size

CPU = -mcpu=cortex-m0 -mthumb

AS_FLAGS = $(CPU) -c

CC_FLAGS = $(CPU) -c -Wall -Wextra -fno-common -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -MMD -MP

CC_SYMBOLS = -DCORE_M0PLUS -D__USE_ROMDIVIDE -D__LPC80X__ -D__CODE_RED

INCLUDE_PATHS += -I$(SRCROOT)/../LPC802/include

LD_FLAGS = $(CPU) -Wl,--gc-sections -Wl,--print-memory-usage
LD_SYS_LIBS = -lc_nano -lnosys

ifeq ($(DEBUG), 1)
  CC_FLAGS += -DDEBUG -O0 -g
else
  CC_FLAGS += -DNDEBUG -Os
endif

all: clean build upload

createObjDir:
	@if [ ! -d $(OBJDIR) ]; then mkdir $(OBJDIR); fi

build: createObjDir $(OBJDIR)/$(PROJECT).elf

upload:
	@echo "Upload"
	/usr/local/bin/openocd -s $(HOME)/Library/Developer/Xcode/Templates/embedded/LPC80x.xctemplate -f interface/$(INTERFACE) -c "transport select swd" -f lpc80x_swd_flash.cfg -c "mt_flash $(OBJDIR)/$(PROJECT).elf"

buildAndUpload: build upload

clean:
	@if [ -d $(OBJDIR) ]; then rm -rf $(OBJDIR); fi

debug: build
	/usr/local/bin/openocd -s $(HOME)/Library/Developer/Xcode/Templates/embedded/LPC80x.xctemplate -f interface/$(INTERFACE) -c "transport select swd" -f lpc80x_swd_flash.cfg &
	$(HOME)/Library/Developer/Xcode/Templates/embedded/usr/bin/arm-none-eabi-insight "$(SRCROOT)/$(OBJDIR)/$(PROJECT).elf"
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

$(OBJDIR)/$(PROJECT).elf: $(OBJECTS) $(SYS_OBJECTS)
	@echo Link
	$(LD) $(LD_FLAGS) -TLPC80x.ld $(LIBRARY_PATHS) -o $@ $^ -Wl,--start-group $(LIBRARIES) $(LD_SYS_LIBS) -Wl,--end-group
	$(SIZE) $@

-include $(OBJECTS:.o=.d)
