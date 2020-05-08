#include "kernel/types.h"
#include "user/user.h"
#include "kernel/syscall.h"

int
main(int argc, char *argv[])
{
	if (argc <= 1){
		exit();
	}

	sleep(atoi(argv[1]));
	exit();
}
