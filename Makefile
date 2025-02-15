include Common.mak

CC=gcc
CXX=g++
STRIP=strip
# NEON is untested
#AES_FLAGS = -D USE_NEON_AES
#AES_FLAGS = -D USE_CXX_AES
CPPFLAGS=$(BASE_CPPFLAGS) -D USE_INTEL_AESNI -maes
OBJDIR=build

#$(TARGET): $(info $(shell $(CXX) --version))
$(TARGET): $(addprefix $(OBJDIR)/, $(OBJS))
	$(CXX) $(CPPFLAGS) $^ -o $@
	$(STRIP) --strip-all $@

$(OBJDIR)/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) -c -o $@ $<

all: $(TARGET)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
