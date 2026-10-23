# Builds NetTest.prc with prc-tools (m68k-palmos-gcc) and the Palm OS 5 SDK (R3).
# The SDK is not part of this repository; point PALMOS_SDK at its "include" directory, e.g.
#   make PALMOS_SDK=/opt/palmdev/sdk-5r3/include
# The tools come from https://github.com/jichu4n/prc-tools-remix (build from source).

PALMOS_SDK ?= /opt/palmdev/sdk-5r3/include
CC = m68k-palmos-gcc
BUILD_PRC = build-prc

INCLUDES = $(addprefix -I$(PALMOS_SDK)/, . Core Core/System Core/UI Core/Hardware \
	Dynamic Libraries Libraries/PalmOSGlue Extensions)
CFLAGS = -O2 -Wall -palmos-none $(INCLUDES) $(NETTEST_DEFS)

all: NetTest.prc

nettest: nettest.c
	$(CC) $(CFLAGS) -o $@ $<

NetTest.prc: nettest
	$(BUILD_PRC) $@ "NetTest" NTst $<

clean:
	rm -f nettest NetTest.prc

.PHONY: all clean
