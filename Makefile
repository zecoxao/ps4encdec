CC=gcc
CXX=g++
STRIP=strip
# NEON is untested
#AES_FLAGS = -D USE_NEON_AES
#AES_FLAGS = -D USE_CXX_AES
AES_FLAGS = -D USE_INTEL_AESNI -maes
CFLAGS= -std=c99
CXXFLAGS= -std=c++23 $(AES_FLAGS)
CPPFLAGS= -Wall -O3

TARGET = ps4-hdd
OBJS=main.o aes_xts.o

$(TARGET): $(OBJS)
	$(CXX) $(CPPFLAGS) $(OBJS) -o $(TARGET)
	$(STRIP) --strip-all $(TARGET)

all: $(TARGET)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
