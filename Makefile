# Variables
CC = gcc
CFLAGS = -Wall -Wextra -pthread
COVERAGE_FLAGS = -fprofile-arcs -ftest-coverage
INCLUDE = -Iinclude
SRCDIR = src
OBJDIR = build
BINDIR = bin
LOGDIR = logs
CONFIGDIR = config
EXEC = $(BINDIR)/proxy_server
TESTDIR = tests
TESTBINDIR = build/tests

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Default target
all: $(EXEC)

# Ensure directories exist
$(OBJDIR) $(BINDIR) $(LOGDIR) $(TESTBINDIR):
	@mkdir -p $(OBJDIR) $(BINDIR) $(LOGDIR) $(TESTBINDIR)

# Build the executable
$(EXEC): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Clean up
clean:
	rm -rf $(OBJDIR) $(BINDIR) $(LOGDIR)/*.log $(TESTBINDIR) $(TESTDIR)/*.gcov $(TESTDIR)/*.gcda $(TESTDIR)/*.gcno $(SRCDIR)/*.gcda $(SRCDIR)/*.gcno

# Coverage build and test execution
coverage: clean
	$(MAKE) CFLAGS="$(CFLAGS) $(COVERAGE_FLAGS)" all
	$(MAKE) -C $(TESTDIR) CFLAGS="$(CFLAGS) $(COVERAGE_FLAGS)" coverage
	@echo "Running tests..."
	$(MAKE) -C $(TESTDIR) run_tests
	@echo "Generating coverage report..."
	gcov -o $(OBJDIR) $(SRCDIR)/*.c

.PHONY: all clean coverage