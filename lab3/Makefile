CC = gcc
CFLAGS = -O3 -march=native -Wall -Wextra -std=c99 -Iinclude
LDFLAGS = -lm

# AVX-512 支持（如果CPU支持）
AVX512_FLAGS = -mavx512f -mavx512vl -mavx512bw -mavx512dq

# 检测 AVX-512 支持
HAS_AVX512 := $(shell grep -q avx512f /proc/cpuinfo 2>/dev/null && echo yes || echo no)
ifeq ($(HAS_AVX512),yes)
    CFLAGS += $(AVX512_FLAGS)
endif

# 源文件
SRCS = src/sm4_baseline.c \
       src/sm4_ttable.c \
       src/sm4_shuffle.c \
       src/sm4_ctr.c \
       src/sm4_avx512.c

OBJS = $(SRCS:.c=.o)

TARGET_TEST = test_sm4
TARGET_PERF = test_perf

all: $(TARGET_TEST) $(TARGET_PERF)

$(TARGET_TEST): $(OBJS) test/test_sm4.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_PERF): $(OBJS) test/test_perf.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET_TEST) $(TARGET_PERF)

run_test: $(TARGET_TEST)
	./$(TARGET_TEST)

run_perf: $(TARGET_PERF)
	./$(TARGET_PERF)

.PHONY: all clean run_test run_perf
