CXX ?= g++
PJ ?= $(HOME)/pjproject
TARGET = astel-daemon
SRC = src/main.cpp
CXXFLAGS += -std=c++17 -O2 -pthread -I$(PJ)/pjsip/include -I$(PJ)/pjlib/include -I$(PJ)/pjlib-util/include -I$(PJ)/pjmedia/include -I$(PJ)/pjnath/include
PJLIBS := $(shell PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig pkg-config --libs libpjproject 2>/dev/null)
ifeq ($(strip $(PJLIBS)),)
PJLIBS = $(shell $(PJ)/build.mak 2>/dev/null)
endif
all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(shell PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig pkg-config --libs libpjproject)
clean:
	rm -f $(TARGET)
