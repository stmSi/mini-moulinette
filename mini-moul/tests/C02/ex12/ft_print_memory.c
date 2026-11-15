#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include "../../../../ex12/ft_print_memory.c"
#include "../../../utils/constants.h"

#define MEMORY_INPUT_SIZE 256
#define MEMORY_OUTPUT_SIZE 4096

typedef struct s_memory_test
{
	char *desc;
	void *addr;
	unsigned int size;
} t_memory_test;

/* Addresses come from the current buffer, rather than a previous run. */
static size_t memory_expected_output(char *output, unsigned char *addr,
	unsigned int size)
{
	size_t length = 0;
	unsigned int offset;
	unsigned int i;

	for (offset = 0; offset < size; offset += 16)
	{
		length += snprintf(output + length, MEMORY_OUTPUT_SIZE - length,
			"%0*" PRIxPTR ": ", (int)(sizeof(uintptr_t) * 2),
			(uintptr_t)(addr + offset));
		for (i = 0; i < 16; i++)
		{
			if (offset + i < size)
				length += snprintf(output + length, MEMORY_OUTPUT_SIZE - length,
					"%02x", (unsigned int)addr[offset + i]);
			else
			{
				output[length++] = ' ';
				output[length++] = ' ';
			}
			if (i % 2 == 1)
				output[length++] = ' ';
		}
		for (i = 0; i < 16 && offset + i < size; i++)
		{
			if (addr[offset + i] >= 32 && addr[offset + i] <= 126)
				output[length++] = addr[offset + i];
			else
				output[length++] = '.';
		}
		output[length++] = '\n';
	}
	output[length] = '\0';
	return (length);
}

/* A temporary stream captures write(1, ...) without a shared output.txt file. */
static int memory_capture_output(t_memory_test *test, void **result,
	char *output, size_t *length)
{
	FILE *capture;
	int saved_stdout;
	int error;

	if (fflush(stdout) == EOF)
		return (1);
	capture = tmpfile();
	if (capture == NULL)
		return (1);
	saved_stdout = dup(STDOUT_FILENO);
	if (saved_stdout == -1)
	{
		fclose(capture);
		return (1);
	}
	if (dup2(fileno(capture), STDOUT_FILENO) == -1)
	{
		close(saved_stdout);
		fclose(capture);
		return (1);
	}
	*result = ft_print_memory(test->addr, test->size);
	error = (fflush(stdout) == EOF);
	if (dup2(saved_stdout, STDOUT_FILENO) == -1)
	{
		perror("Could not restore stdout");
		close(saved_stdout);
		fclose(capture);
		_exit(1);
	}
	close(saved_stdout);
	if (fseek(capture, 0, SEEK_SET) != 0)
		error = 1;
	*length = fread(output, 1, MEMORY_OUTPUT_SIZE, capture);
	if (ferror(capture))
		error = 1;
	fclose(capture);
	return (error);
}

static int memory_run_tests(t_memory_test *tests, size_t count)
{
	char expected[MEMORY_OUTPUT_SIZE];
	char output[MEMORY_OUTPUT_SIZE];
	unsigned char original[MEMORY_INPUT_SIZE];
	size_t expected_length;
	size_t output_length;
	void *result;
	int failed;
	int errors = 0;
	size_t i;

	for (i = 0; i < count; i++)
	{
		if (tests[i].size > MEMORY_INPUT_SIZE)
		{
			printf("    " RED "[%zu] %s: test input exceeds buffer capacity\n"
				DEFAULT, i + 1, tests[i].desc);
			errors++;
			continue;
		}
		if (tests[i].size != 0)
			memcpy(original, tests[i].addr, tests[i].size);
		expected_length = memory_expected_output(expected, tests[i].addr,
			tests[i].size);
		if (memory_capture_output(&tests[i], &result, output, &output_length))
		{
			printf("    " RED "[%zu] %s: could not capture stdout\n" DEFAULT,
				i + 1, tests[i].desc);
			errors++;
			continue;
		}
		failed = 0;
		/* Compare lengths too, so extra output and embedded NULs cannot pass. */
		if (output_length != expected_length
			|| memcmp(output, expected, expected_length) != 0)
		{
			printf("    " RED "[%zu] %s: expected %zu output bytes, got %zu\n"
				"Expected:\n" DEFAULT, i + 1, tests[i].desc,
				expected_length, output_length);
			fwrite(expected, 1, expected_length, stdout);
			printf(RED "Got:\n" DEFAULT);
			fwrite(output, 1, output_length, stdout);
			printf("\n");
			failed = 1;
		}
		if (result != tests[i].addr)
		{
			printf("    " RED "[%zu] %s: expected return %p, got %p\n" DEFAULT,
				i + 1, tests[i].desc, tests[i].addr, result);
			failed = 1;
		}
		if (tests[i].size != 0
			&& memcmp(tests[i].addr, original, tests[i].size) != 0)
		{
			printf("    " RED "[%zu] %s: input memory was modified\n" DEFAULT,
				i + 1, tests[i].desc);
			memcpy(tests[i].addr, original, tests[i].size);
			failed = 1;
		}
		if (failed)
			errors++;
		else
			printf("  " GREEN CHECKMARK GREY " [%zu] %s\n" DEFAULT,
				i + 1, tests[i].desc);
	}
	return (errors != 0);
}

int main(void)
{
	unsigned char text[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz! ~";
	unsigned char binary[] = {0x00, 0x01, 0x09, 0x0a, 0x0d, 0x1f, 0x20, 0x21,
		0x7e, 0x7f, 0x80, 0x81, 0x9f, 0xa0, 0xfe, 0xff,
		0x00, 'A', 0x00, 'B', 0x7e, 0x20, 0x7f, 0xff};
	unsigned char zeroes[33] = {0};
	unsigned char single[] = {'X'};
	unsigned char all_bytes[MEMORY_INPUT_SIZE];
	unsigned int i;
	t_memory_test tests[] = {
		{.desc = "Zero size with a valid address", .addr = text, .size = 0},
		{.desc = "Zero size with NULL", .addr = NULL, .size = 0},
		{.desc = "One byte without a string terminator", .addr = single, .size = 1},
		{.desc = "One NUL byte", .addr = zeroes, .size = 1},
		{.desc = "One high-bit byte (0xff)", .addr = binary + 15, .size = 1},
		{.desc = "Two bytes, one hex pair", .addr = text, .size = 2},
		{.desc = "Three bytes, odd-length padding", .addr = text, .size = 3},
		{.desc = "Seven bytes", .addr = text, .size = 7},
		{.desc = "Eight bytes", .addr = text, .size = 8},
		{.desc = "Nine bytes", .addr = text, .size = 9},
		{.desc = "Fifteen bytes, just below a full line", .addr = text, .size = 15},
		{.desc = "Exactly sixteen bytes, no extra line", .addr = text, .size = 16},
		{.desc = "Seventeen bytes, one-byte final line", .addr = text, .size = 17},
		{.desc = "Thirty-one bytes", .addr = text, .size = 31},
		{.desc = "Exactly thirty-two bytes", .addr = text, .size = 32},
		{.desc = "Thirty-three bytes, three lines", .addr = text, .size = 33},
		{.desc = "Long printable buffer", .addr = text, .size = sizeof(text) - 1},
		{.desc = "Include the trailing NUL", .addr = text, .size = sizeof(text)},
		{.desc = "Start inside a buffer", .addr = text + 3, .size = 17},
		{.desc = "Printable boundaries, DEL and high-bit bytes",
			.addr = binary + 5, .size = 11},
		{.desc = "Embedded NULs and control characters across lines",
			.addr = binary, .size = sizeof(binary)},
		{.desc = "All-zero memory across three lines", .addr = zeroes,
			.size = sizeof(zeroes)},
		{.desc = "Every byte value from 0x00 to 0xff", .addr = all_bytes,
			.size = sizeof(all_bytes)},
	};

	for (i = 0; i < sizeof(all_bytes); i++)
		all_bytes[i] = (unsigned char)i;
	return (memory_run_tests(tests, sizeof(tests) / sizeof(tests[0])));
}
