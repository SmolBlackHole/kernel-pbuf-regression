/* SPDX-License-Identifier: MIT */
/* Linux 6.8+ provided-buffer-ring reserved-field regression reproducer. */
#define _GNU_SOURCE
#include <errno.h>
#include <linux/io_uring.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

enum operation
{
	REGISTER,
	UNREGISTER,
	STATUS
};

static const char *const names[] = {"register", "unregister", "status"};
static unsigned int tests, failures, bootstraps;

static int register_call(int fd, unsigned int opcode, void *argument)
{
	int result = (int)syscall(__NR_io_uring_register, fd, opcode, argument, 1);

	return result < 0 ? -errno : result;
}

static void outcome(const char *name, int word, int actual, int expected)
{
	int passed = actual == expected;

	tests++;
	failures += !passed;
	printf("%s %u - %s resv=%s", passed ? "ok" : "not ok", tests,
		   name, word < 0 ? "zero" : "nonzero");
	if (word >= 0)
		printf(" word=%d", word);
	printf(" result=%d expected=%d\n", actual, expected);
}

/* Each case owns a fresh ring; close releases registrations before munmap. */
static int run_case(enum operation operation, int word, size_t page_size)
{
	struct io_uring_params parameters = {0};
	struct io_uring_buf_reg registration = {0};
	struct io_uring_buf_reg removal = {0};
	struct io_uring_buf_status status = {0};
	void *memory;
	int fd, result, expected = word < 0 ? 0 : -EINVAL;

	fd = (int)syscall(__NR_io_uring_setup, 2, &parameters);
	if (fd < 0)
	{
		fprintf(stderr, "io_uring_setup: %s (errno=%d); no ABI verdict\n",
				strerror(errno), errno);
		return 4;
	}
	memory = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
				  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (memory == MAP_FAILED)
	{
		perror("mmap");
		close(fd);
		return 2;
	}
	registration.ring_addr = (uintptr_t)memory;
	registration.ring_entries = 8;
	registration.bgid = 7;
	removal.bgid = registration.bgid;
	status.buf_group = registration.bgid;
	if (operation == REGISTER && word >= 0)
		memset(&registration.resv[word], 0xff, sizeof(registration.resv[word]));
	result = register_call(fd, IORING_REGISTER_PBUF_RING, &registration);
	if (operation == REGISTER)
	{
		outcome(names[operation], word, result, expected);
		goto done;
	}
	/* Diagnostic only: reach the other two checks on an inverted kernel. */
	if (result == -EINVAL)
	{
		memset(&registration.resv[0], 0xff, sizeof(registration.resv[0]));
		result = register_call(fd, IORING_REGISTER_PBUF_RING, &registration);
		if (!result)
			bootstraps++;
	}
	if (result)
	{
		fprintf(stderr, "%s prerequisite registration failed: %d\n",
				names[operation], result);
		close(fd);
		munmap(memory, page_size);
		return 2;
	}
	if (operation == UNREGISTER)
	{
		if (word >= 0)
			memset(&removal.resv[word], 0xff, sizeof(removal.resv[word]));
		result = register_call(fd, IORING_UNREGISTER_PBUF_RING, &removal);
	}
	else
	{
		if (word >= 0)
			status.resv[word] = 1;
		result = register_call(fd, IORING_REGISTER_PBUF_STATUS, &status);
	}
	outcome(names[operation], word, result, expected);
done:
	close(fd);
	munmap(memory, page_size);
	return 0;
}

int main(void)
{
	struct io_uring_buf_reg registration;
	struct io_uring_buf_status status;
	struct utsname system;
	size_t reg_words = sizeof(registration.resv) / sizeof(registration.resv[0]);
	size_t status_words = sizeof(status.resv) / sizeof(status.resv[0]);
	long page_size = sysconf(_SC_PAGESIZE);
	enum operation operation;
	int result, word;

	if (page_size <= 0)
	{
		fprintf(stderr, "Cannot determine page size\n");
		return 2;
	}
	if (!uname(&system))
		printf("# kernel=%s arch=%s\n", system.release, system.machine);
	puts("TAP version 13");
	printf("1..%zu\n", 3 + reg_words * 2 + status_words);
	for (operation = REGISTER; operation <= STATUS; operation++)
	{
		size_t words = operation == STATUS ? status_words : reg_words;

		for (word = -1; word < (int)words; word++)
		{
			result = run_case(operation, word, (size_t)page_size);
			if (result)
				return result;
		}
	}
	printf("# tests=%u failures=%u diagnostic_bootstraps=%u\n",
		   tests, failures, bootstraps);
	return failures ? 1 : 0;
}
