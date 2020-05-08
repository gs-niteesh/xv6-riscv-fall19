#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

const int end = 35;

void error(const char *str);

void task(int pipes[2]);

int
main(int argc, char *argv[])
{
	int rv;
	int pid;
	int pipes[2];

	rv = pipe(pipes);
	
	if(rv < 0) {
		error("Cannot create pipes");
	}

	pid = fork();
	if (pid < 0) {
		error("Cannot fork");
	}	

	if (pid == 0) { // Child process.

		task(pipes);

	} else { // Parent process.

		close(pipes[0]);
		for(int i=2; i<=end; i++){

			rv = write(pipes[1], &i, sizeof(int));
			if (rv < 0) {
				error("Error writing data");
			}

		}
		close(pipes[1]);
	}
	exit();
}

void
task(int m_pipes[2])
{
	int prime;
	int data;
	int rv;
	int pipes[2];

	close(m_pipes[1]);

	rv = read(m_pipes[0], &prime, sizeof(int));
	if (rv < 0){
		error("Error reading data");
	} else if (rv == 0){
		exit();
	}

	printf("prime %d\n", prime);

	rv = pipe(pipes);

	if(rv < 0) {
		error("Cannot create pipes");
	}

	int pid = fork();

	if (pid == 0) {

		close(m_pipes[0]);
		task(pipes);

	} else {

		close(pipes[0]);
		while(read(m_pipes[0], &data, sizeof(int))){

			if (data % prime != 0){
				rv = write(pipes[1], &data, sizeof(int));
				if (rv < 0){
					error("Error writing data");
				}
			}

		}
		close(pipes[1]);
		close(m_pipes[0]);
	}
	exit();
}

void
error(const char *str)
{
	printf("%s\n", str);
	exit();
}
