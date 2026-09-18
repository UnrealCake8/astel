CXX ?= g++
PJ ?= $(HOME)/pjproject
TARGET = astel-daemon
SRC = src/main.cpp

CXXFLAGS += -std=c++17 -O2 -pthread \
	-I$(PJ)/pjsip/include \
	-I$(PJ)/pjlib/include \
	-I$(PJ)/pjlib-util/include \
	-I$(PJ)/pjmedia/include \
	-I$(PJ)/pjnath/include

# PJProject is installed as static archives on the OCI host. --static is
# important here: it includes PJProject's private/transitive dependencies
# (OpenSSL, ALSA, Speex, GSM, SRTP, etc.) required when linking .a files.
PJLIBS := $(shell PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig pkg-config --static --libs libpjproject 2>/dev/null)

ifeq ($(strip $(PJLIBS)),)
$(error libpjproject pkg-config metadata not found. Run 'sudo make install' from ~/pjproject first)
endif

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(PJLIBS)

clean:
	rm -f $(TARGET)
