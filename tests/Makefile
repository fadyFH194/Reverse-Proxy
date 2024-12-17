# Variables
CC = gcc
CFLAGS = -Wall -Wextra -pthread -O0
COVERAGE_FLAGS = -fprofile-arcs -ftest-coverage
INCLUDE = -I../include
SRCDIR = ../src
OBJDIR = ../build
TESTBINDIR = ../build/tests
TESTS = test_route_manager test_json_parser test_proxy test_concurrency

all: $(TESTS)

test_route_manager: test_route_manager.c
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $(INCLUDE) test_route_manager.c $(OBJDIR)/route_manager.o -o $@

test_json_parser: test_json_parser.c
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $(INCLUDE) test_json_parser.c $(OBJDIR)/json_parser.o -o $@

test_proxy: test_proxy.c
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $(INCLUDE) test_proxy.c \
	$(OBJDIR)/proxy.o $(OBJDIR)/route_manager.o $(OBJDIR)/json_parser.o -o test_proxy

test_concurrency: test_concurrency.sh
	@chmod +x $<

coverage: clean all
	@echo "Tests compiled with coverage flags."

run_tests: $(TESTS)
	@echo "Running tests..."
	./test_route_manager
	./test_json_parser
	./test_proxy  # Run the C-based proxy test now
	./test_concurrency.sh

report:
	@echo "Generating coverage report..."
	gcov -o $(OBJDIR) $(SRCDIR)/*.c

clean:
	rm -f $(TESTS) *.gcov *.gcda *.gcno
	rm -rf $(TESTBINDIR)

.PHONY: all clean coverage run_tests report
