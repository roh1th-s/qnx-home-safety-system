DEBUG = -g
CC = qcc
LD = qcc

#TARGET = -Vgcc_ntox86_64
TARGET = -Vgcc_ntoaarch64le

CFLAGS += $(DEBUG) $(TARGET) -Wall
LDFLAGS+= $(DEBUG) $(TARGET)

SRC_DIR=src
OUT_DIR=bins
BINS=central_analyzer stats_update alert_mgr event_logger
OUT_BINS=$(addprefix $(OUT_DIR)/,$(BINS))
COMMON_SRC=$(SRC_DIR)/common/rpi_gpio.c

all:$(OUT_BINS)
	@echo "Binaries built into bins/"

$(OUT_DIR)/%: $(SRC_DIR)/%.c $(COMMON_SRC)
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -rf $(OUT_DIR)