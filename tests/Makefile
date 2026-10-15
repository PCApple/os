CC = gcc
CFLAGS = -std=c99 -g -O0 -Wall -Wextra -fno-builtin -ffunction-sections -fdata-sections
BUILD = .build

.PHONY: test coverage sanitize clean
test: $(BUILD)/fs_lookup
	./$(BUILD)/fs_lookup

$(BUILD):
	mkdir -p $@

$(BUILD)/fs.o: ../src/fs.c $(wildcard ../src/include/*.h) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/strings.o: ../src/strings.c ../src/include/strings.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fs_lookup: fs_lookup.c $(BUILD)/fs.o $(BUILD)/strings.o
	$(CC) $(CFLAGS) $^ -Wl,--gc-sections -o $@

# Keep coverage separate from the normal build. Generate a report even when
# regression tests fail; return the test status after reporting.
coverage:
	$(MAKE) BUILD=.coverage CFLAGS='$(CFLAGS) --coverage' .coverage/fs_lookup
	find .coverage -name '*.gcda' -delete
	@./.coverage/fs_lookup; result=$$?; gcov -f -b -c -o .coverage ../src/fs.c; exit $$result

sanitize:
	ASAN_OPTIONS=detect_leaks=0 $(MAKE) BUILD=.sanitize CFLAGS='$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer' test

clean:
	rm -rf .build .coverage .sanitize fs.c.gcov
