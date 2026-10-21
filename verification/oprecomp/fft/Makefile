AXPIKE_ROOT := $(abspath $(CURDIR)/../../..)

OPRECOMP_DIR ?= $(abspath $(AXPIKE_ROOT)/../oprecomp-micro-benchmarks)
OPRECOMP_COMMIT := 1afb13560b6c2cc01ce6f933c1f61a926b799163
FFT_DIR := $(OPRECOMP_DIR)/mb/fft
COMMON_DIR := $(OPRECOMP_DIR)/mb/common

OUT_DIR ?= $(AXPIKE_ROOT)/verification/out/oprecomp-fft-rv64
BINARY := $(OUT_DIR)/run_fftw

CC = riscv64-unknown-elf-gcc
CPPFLAGS = -I$(COMMON_DIR)
CFLAGS = -O2 -march=rv64gc -mabi=lp64d
LDFLAGS = -static
LDLIBS = -lm

SOURCES := \
	$(FFT_DIR)/fft_stick.c \
	$(FFT_DIR)/run_fftw.c \
	$(CURDIR)/oprecomp_pk.c

.PHONY: build check-source clean

build: check-source $(BINARY)

check-source:
	@test "$$(git -C "$(OPRECOMP_DIR)" rev-parse HEAD)" = "$(OPRECOMP_COMMIT)" || \
		{ echo "OPRECOMP HEAD does not match $(OPRECOMP_COMMIT)" >&2; exit 1; }
	@test -z "$$(git -C "$(OPRECOMP_DIR)" status --short --untracked-files=no)" || \
		{ echo "OPRECOMP tracked sources are dirty" >&2; exit 1; }

$(BINARY): $(SOURCES) $(COMMON_DIR)/oprecomp.h | $(OUT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(OUT_DIR):
	mkdir -p $@

clean:
	rm -f $(BINARY)
