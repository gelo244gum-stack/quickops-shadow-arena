CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c2x -O2 -g
CFLAGS += -D_FORTIFY_SOURCE=3
CFLAGS += -fstack-protector-strong
CFLAGS += -fPIE
LDFLAGS = -pie -z relro -z now

SRCDIR = src
INCDIR = include
BUILDDIR = build

SRCS = $(wildcard $(SRCDIR)/*.c) main.c selftest_json.c retention_report.c
OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

TARGET = frailbox

.PHONY: all clean distclean test valgrind test_selftest_json test_retention test_newline_boundary

all: $(TARGET) test_selftest_json test_retention test_newline_boundary

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) $^ -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCDIR) -MMD -MP -c $< -o $@

-include $(DEPS)

# test_selftest_json.c has its own main() and links with selftest_json + retention_report
test_selftest_json: $(BUILDDIR)/test_selftest_json.o $(BUILDDIR)/selftest_json.o $(BUILDDIR)/retention_report.o
	$(CC) $(CFLAGS) -I$(INCDIR) $^ -o $@

# test_retention.c has its own main() and doesn't need $(TARGET)
test_retention: $(BUILDDIR)/test_retention.o $(BUILDDIR)/retention_report.o
	$(CC) $(CFLAGS) -I$(INCDIR) $^ -o $@ $(LDFLAGS)

# test_newline_boundary.c has its own main() and doesn't need $(TARGET)
test_newline_boundary: $(BUILDDIR)/test_newline_boundary.o
	$(CC) $(CFLAGS) -I$(INCDIR) $^ -o $@ $(LDFLAGS) -lpthread

clean:
	rm -rf $(BUILDDIR) $(TARGET) test_selftest_json test_retention test_newline_boundary

distclean: clean
	rm -rf *.o *.d

test: $(TARGET)
	./$(TARGET) --sandbox-type seccomp --memory-limit 64 --verbose

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

test_all: test_selftest_json test_retention test_newline_boundary
	./test_selftest_json
	./test_retention
	./test_newline_boundary
