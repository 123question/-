CC = gcc
CFLAGS = -O3 -march=native -Wall -Wextra -std=c99 -Iinclude
LDFLAGS = -lm

# 源文件
SRCS = src/sm4_baseline.c \
       src/sm4_ttable.c \
       src/sm4_shuffle.c \
       src/sm4_ctr.c


OBJS = $(SRCS:.c=.o)

# 目标
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
