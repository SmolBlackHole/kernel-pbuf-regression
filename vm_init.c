/* SPDX-License-Identifier: MIT */
/* Minimal init for a disposable, diskless kernel reproduction VM. */
#include <stdio.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
	pid_t child = fork();
	int status;

	if (!child)
	{
		execl("/pbuf_reserved", "pbuf_reserved", (char *)NULL);
		perror("execl");
		_exit(2);
	}
	if (child < 0 || waitpid(child, &status, 0) < 0)
	{
		perror("fork/waitpid");
		puts("VM_TEST_EXIT=2");
	}
	else
	{
		printf("VM_TEST_EXIT=%d\n", WIFEXITED(status) ? WEXITSTATUS(status) : 2);
	}
	fflush(NULL);
	/* QEMU's -no-reboot exits here without requiring ACPI power-off support. */
	reboot(RB_AUTOBOOT);
	return 2;
}
