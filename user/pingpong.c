#include "kernel/syscall.h"
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
	int len = 1;
	int pid;
	char c;

	int child_pipe[2];
	int parent_pipe[2];
	
	pipe(child_pipe);
	pipe(parent_pipe);

	pid = fork();

	if (pid == 0) {	// Child Process

		pid = getpid();

		close(parent_pipe[1]);
		len = read(parent_pipe[0], &c, len);	

		printf("%d: received ping\n", pid);

		close(parent_pipe[0]);
		close(child_pipe[0]);

		len = write(child_pipe[1], &c, 1);

		close(child_pipe[1]);

	} else {	// Parent Process

		pid = getpid();

		close(parent_pipe[0]);
		len = write(parent_pipe[1], &c, len);

		close(parent_pipe[1]);
		close(child_pipe[1]);

		len = read(child_pipe[0], &c, len);

		printf("%d: received pong\n", pid);

		close(child_pipe[0]);
	}
	exit();
}
