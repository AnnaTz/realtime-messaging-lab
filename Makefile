

CXX      := C:\SysGCC\raspberry\bin\arm-linux-gnueabihf-gcc.exe

CXXFLAGS := -std=c11 -Wall -Wextra 
CXXFEXTR := -c -fmessage-length=0
LDFLAGS  := -lm -lpthread
BUILD    := ./bin
OBJ_DIR  := $(BUILD)/obj
APP_DIR  := $(BUILD)
TARGET   := rtes_final
SRC      := $(wildcard src/*.c)

OBJECTS := $(SRC:%.c=$(OBJ_DIR)/%.o)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CXX) $(CXXFEXTR) $(CXXFLAGS) -o $@ -c $<

$(APP_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(APP_DIR)/$(TARGET) $(OBJECTS)

# === Rules ===
rtes_final: build $(APP_DIR)/$(TARGET)

build:
	@mkdir -p $(APP_DIR)
	@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g3
debug: rtes_final

release: CXXFLAGS += -O2
release: rtes_final

upload:
	scp $(BUILD)/$(TARGET) root@192.168.0.1:/root/

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*

all: clean release upload

.PHONY: rtes_final build debug release upload clean all

