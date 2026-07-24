# Makefile — SM3 multi-architecture build
#
# Targets:
#   make              → auto-detect arch, build optimized
#   make ref          → scalar reference only
#   make avx2         → x86-64 with AVX2
#   make avx512       → x86-64 with AVX-512
#   make neon         → ARM64 with NEON
#   make clean
#
# Cross-compilation examples:
#   make ARCH=arm64 CROSS=aarch64-linux-gnu- neon

CC      ?= gcc
CFLAGS  ?= -O3 -Wall -Wextra -std=c99
LDFLAGS ?= -lrt

# Architecture detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
UNAME_M := $(shell uname -m 2>/dev/null || echo x86_64)

ifeq ($(UNAME_M),aarch64)
  HOST_ARCH := arm64
else ifeq ($(UNAME_M),arm64)
  HOST_ARCH := arm64
else
  HOST_ARCH := x86
endif

# Cross-compilation support
ARCH ?= $(HOST_ARCH)
CROSS ?=

ifeq ($(CROSS),)
  CC_CMD = $(CC)
else
  CC_CMD = $(CROSS)gcc
endif

SRCDIR    = src
SRCS_REF  = $(SRCDIR)/sm3_ref.c
SRCS_HYBR = $(SRCDIR)/sm3_hybrid.c
SRCS_MAIN = $(SRCDIR)/main.c

# ---- x86 AVX2 --------------------------------------------------------
ifeq ($(ARCH),x86)
  TARGET  = sm3_bench
  CFLAGS += -march=native -mavx2
  SRCS   += $(SRCS_REF) $(SRCS_HYBR) $(SRCDIR)/sm3_avx2.c $(SRCS_MAIN)
endif

# ---- x86 AVX-512 -----------------------------------------------------
ifeq ($(ARCH),avx512)
  TARGET  = sm3_bench_avx512
  CFLAGS += -march=native -mavx512f -mavx512vl
  SRCS   += $(SRCS_REF) $(SRCS_HYBR) $(SRCDIR)/sm3_avx2.c $(SRCDIR)/sm3_avx512.c $(SRCS_MAIN)
endif

# ---- ARM64 NEON ------------------------------------------------------
ifeq ($(ARCH),arm64)
  TARGET  = sm3_bench
  CFLAGS += -march=armv8-a+simd
  SRCS   += $(SRCS_REF) $(SRCS_HYBR) $(SRCDIR)/sm3_neon.c $(SRCS_MAIN)
endif

# ---- Reference only --------------------------------------------------
ifeq ($(ARCH),ref)
  TARGET  = sm3_bench_ref
  CFLAGS += -std=c99
  SRCS   += $(SRCS_REF) $(SRCS_MAIN)
endif

OBJS = $(SRCS:.c=.o)

.PHONY: all ref avx2 avx512 neon clean

all: $(TARGET)

ref:
	$(MAKE) ARCH=ref

avx2:
	$(MAKE) ARCH=x86

avx512:
	$(MAKE) ARCH=avx512

neon:
	$(MAKE) ARCH=arm64

$(TARGET): $(OBJS)
	$(CC_CMD) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "  Built: $(TARGET)  [ARCH=$(ARCH)]"

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/sm3.h
	$(CC_CMD) $(CFLAGS) -I$(SRCDIR) -c -o $@ $<

clean:
	rm -f $(SRCDIR)/*.o sm3_bench sm3_bench_ref sm3_bench_avx512

# Print detected configuration
info:
	@echo "Host OS:    $(UNAME_S)"
	@echo "Host arch:  $(UNAME_M)  → detected $(HOST_ARCH)"
	@echo "Build arch: $(ARCH)"
	@echo "Compiler:   $(CC_CMD)"
	@echo "CFLAGS:     $(CFLAGS)"
	@echo "Sources:    $(SRCS)"
	@echo "Target:     $(TARGET)"
