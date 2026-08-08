# Builds mercs2_language.asi against the m2 SDK (the shared m2-sdk.dll layer).
#
#   make            ->  ../src/mercs2_language.asi   (builds the SDK first if needed)
#   make clean
#
# Requires: the i686-w64-mingw32 toolchain, and the sdk/ submodule initialized
# (git submodule update --init --recursive).
#
# m2-sdk.dll is a LOAD-TIME dependency: it must ship beside the .asi. The Shipment declares
# it via `load.requires` (pinned url + sha256) so qm fetches the exact published bytes.

include ../sdk/sdk.mk

CC   = i686-w64-mingw32-gcc
OUT  = ../src/mercs2_language.asi
SRCS = mercs2_language.c hook_stub.S

$(OUT): $(SRCS) $(M2_IMPLIB)
	$(CC) -O2 -shared $(M2_CFLAGS) -o $@ $(SRCS) $(M2_LDFLAGS) -lkernel32 -luser32
	@echo "Built $@"
	@echo "  ship m2-sdk.dll beside it (load-time import) — see load.requires in manifest.yaml"

# The SDK DLL + import library the plugin links against.
$(M2_IMPLIB):
	$(MAKE) -C ../sdk build

.PHONY: clean
clean:
	rm -f $(OUT) *.o
