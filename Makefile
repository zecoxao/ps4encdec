include Common.mak

CC=gcc
CXX=g++
STRIP=strip
# NEON is untested
#AES_FLAGS = -D USE_NEON_AES
#AES_FLAGS = -D USE_CXX_AES
CPPFLAGS=$(BASE_CPPFLAGS) -D USE_INTEL_AESNI -maes

#$(TARGET): $(info $(shell $(CXX) --version))
$(TARGET): $(OBJS)
	$(CXX) $(CPPFLAGS) $(OBJS) -o $(TARGET)
	$(STRIP) --strip-all $(TARGET)

all: $(TARGET)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
