# Host unit tests for ESPSW pure logic. Run: make -C test/host
CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -g -I../../main
BUILD   := build

.PHONY: test clean
test: $(BUILD)/test_startup
	$(BUILD)/test_startup

$(BUILD)/test_startup: test_startup.c ../../main/startup.c ../../main/startup.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_startup.c ../../main/startup.c

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
