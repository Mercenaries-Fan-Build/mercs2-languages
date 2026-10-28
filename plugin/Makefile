# Builds mercs2_language.asi against the m2 SDK (the shared m2-sdk.dll layer).
#
#   make            ->  ../src/mercs2_language.asi   (builds the SDK first if needed)
#   make M2_IMPLIB=<dir>/libm2-sdk.dll.a M2_LDFLAGS="-L<dir> -lm2-sdk"
#                   ->  the same, linked against a released m2-sdk import library (what
#                       ../.github/actions/build-asi runs)
#   make clean
#
# The output is never committed (.gitignore: *.asi); the lint and release workflows build it.
#
# Requires: the i686-w64-mingw32 toolchain, and the sdk/ submodule initialized
# (git submodule update --init --recursive). Building the SDK's import library
# (`make -C ../sdk build`) also needs rustup, curl and shasum (see sdk/README.md).
#
# m2-sdk.dll is a LOAD-TIME dependency, placed in the game root by the m2-sdk Shipment. The
# Shipment requires it via `load.requires` (`{ shipment: m2-sdk, version: "^0.4" }`).

include ../sdk/sdk.mk

CC   = i686-w64-mingw32-gcc
OUT  = ../src/mercs2_language.asi
SRCS = mercs2_language.c hook_stub.S

$(OUT): $(SRCS) $(M2_IMPLIB)
	$(CC) -O2 -shared $(M2_CFLAGS) -o $@ $(SRCS) $(M2_LDFLAGS) -lkernel32 -luser32
	@echo "Built $@"
	@echo "  m2-sdk.dll (load-time import) comes from the m2-sdk Shipment — see load.requires in manifest.yaml"

# The SDK DLL + import library the plugin links against.
$(M2_IMPLIB):
	$(MAKE) -C ../sdk build

.PHONY: clean
clean:
	rm -f $(OUT) *.o
