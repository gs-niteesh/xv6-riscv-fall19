#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

#define stdin 0

void
xargs(int argc, char *cmd[])
{
	char c;
	char buf[MAXARG][50];
	int count = 0;

	// Copy the arguments provided in cmdline to buffer.
	for(int i=1; i<argc; i++){
		strcpy(buf[count++], cmd[i]);
	}

	int index = 0;
	while(read(stdin, &c, 1) == 1){
		if (c == '\n'){
			count++;
			if (count >= MAXARG) break;
			index = 0;
			continue;
		}
		buf[count][index++] = c;
	}

	char *ptr[MAXARG + 1];
	for(int i=0; i<count; i++){
		ptr[i] = buf[i];
	}
	ptr[count] = 0;

	int pid = fork();
	if(pid == 0){
		exec(buf[0], ptr);
	}else{
		wait();
	}

}

int
main(int argc, char *argv[])
{
	if(argc < 1){
		fprintf(2, "Usage: xargs cmd args\n");
		exit();
	}

	xargs(argc, argv);

	exit();
}
