#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define true 1
#define false 0

int match(char *re, char *text);

int
chckname(char *filename, char *regex)
{
	char *p;
	int n = strlen(filename);

	for(p=filename+n; p >= filename && *p != '/'; p--);
	p++;

	if (match(regex, p)){
		return true;
	}
	return false;
}

void
find(char *path, char *regex)
{
  char buf[512], *p;
  int fd;
	int rv;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

			if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
				continue;

      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
			find(buf, regex);
			rv = chckname(buf, regex);
			if(rv == true){
				printf("%s\n", buf);
			}
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{

	if (argc < 2){
		fprintf(2, "Usage: find path filename\n");
		exit();
	}
  if(argc < 3){
    find(".", argv[1]);
    exit();
  }
	find(argv[1], argv[2]);
  exit();
}

int matchhere(char *re, char *text);
int matchstar(int c, char *re, char *text);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

