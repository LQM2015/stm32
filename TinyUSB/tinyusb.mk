# TinyUSB源文件路径
TINYUSB_DIR = TinyUSB/src

# TinyUSB源文件
TINYUSB_SOURCES = \
	$(TINYUSB_DIR)/tusb.c \
	$(TINYUSB_DIR)/common/tusb_fifo.c \
	$(TINYUSB_DIR)/device/usbd.c \
	$(TINYUSB_DIR)/device/usbd_control.c \
	$(TINYUSB_DIR)/class/cdc/cdc_device.c \
	$(TINYUSB_DIR)/class/msc/msc_device.c \
	$(TINYUSB_DIR)/class/vendor/vendor_device.c \
	$(TINYUSB_DIR)/portable/synopsys/dwc2/dcd_dwc2.c \
	TinyUSB/usb_descriptors.c \
	TinyUSB/bsp_board.c \
	TinyUSB/tinyusb_app.c

# TinyUSB头文件路径
TINYUSB_INCLUDES = \
	-ITinyUSB \
	-ITinyUSB/src \
	-ITinyUSB/src/common \
	-ITinyUSB/src/device \
	-ITinyUSB/src/class/cdc \
	-ITinyUSB/src/class/msc \
	-ITinyUSB/src/class/vendor \
	-ITinyUSB/src/portable/synopsys/dwc2

# 编译选项
TINYUSB_CFLAGS = \
	-DCFG_TUSB_MCU=OPT_MCU_STM32H7 \
	-DCFG_TUD_ENABLED=1 \
	-DCFG_TUSB_OS=OPT_OS_FREERTOS

# 添加到主makefile的变量中
C_SOURCES += $(TINYUSB_SOURCES)
C_INCLUDES += $(TINYUSB_INCLUDES)
CFLAGS += $(TINYUSB_CFLAGS)
