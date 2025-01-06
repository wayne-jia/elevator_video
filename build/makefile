SDK            := /home/wayne/sdk/sama5/arm-buildroot-linux-gnueabihf_sdk-buildroot
CXX            := $(SDK)/bin/arm-buildroot-linux-gnueabi-g++ -std=c++17
SRC_DIR        := ../src
EGT_CXXFLAGS   := $(shell $(SDK)/bin/pkg-config libegt --cflags)
EGT_LDFLAGS    := $(shell $(SDK)/bin/pkg-config libegt --libs)
LIBS           := -lplanes

SRC            := $(SRC_DIR)/*
TARGET         := elevator

all: $(TARGET)

$(TARGET): $(SRC)
        $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EGT_CXXFLAGS) $^ -o $@ $(EGT_LDFLAGS) $(LIBS)

clean:
        rm -f $(TARGET)

