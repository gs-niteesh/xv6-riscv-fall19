#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include <stdarg.h>

/* command type */
typedef enum {
  EXEC,
  REDIR,
} TYPE;

#define PROMPT '@'

/* constants */
#define true  1
#define false 0

#define WRITE 1
#define READ  0

#define INPUT  0
#define OUTPUT 1
#define BOTH   2

#define NULL 0

/* contrains */
#define MAXCMDS  10
#define MAXARGS  20
#define BUFSIZE  200

struct {
  char buf[BUFSIZE];
  int length;
}buffer;

/* Represents a command */
struct command{
  TYPE type;
  char *name;
  char *argv[MAXARGS];
  char *inputfile;
  char *outputfile;
};

/* A placeholder for a list of commands */
struct {
  struct command cmds[MAXCMDS];
  int cmd_index;
}commands;

void parse_command();
void parse_exec(struct command *cmd, char *start, char *end);
void parse_redirs(struct command *cmd, char *start, char *end);
void parse_pipes(struct command *cmd, char *start, char *end);

void execute_command();
void execute(struct command *cmd);
void execute_redir(struct command *cmd);
void execute_pipes(struct command *cmd);

/* Utility functions */
int getline();
int openfile(const char *file, int mode);
int spawn_process(struct command *cmd, int in, int out);

char* get_token(char *start, char *end, char **next);
char* peek(char *start, char *end, char c);

void error(const char *msg);
void skip_whitespace(char **ptr, char *end);

int
main(int argc, char *argv[])
{
  while(1){
    fprintf(2, "%c ", PROMPT);

    memset(&commands, 0, sizeof(commands));

    if (getline() < 0) break;

    if (strcmp(buffer.buf, "exit") == 0) break;

    parse_command();
    execute_command();
  }
  exit(0);
}

void
parse_command()
{
  struct command *cmd;
  char *start = buffer.buf;
  char *end = buffer.buf + buffer.length;

  if (start == end) return ;

  cmd = &commands.cmds[0];
  commands.cmd_index = 0;

  parse_pipes(cmd, start, end);
}

void
parse_pipes(struct command *cmd, char *start, char *end)
{
  char *temp = start;
  char *prev = temp;
  int n = commands.cmd_index;
  struct command *cur_cmd = &commands.cmds[n];

  skip_whitespace(&start, end);

  while ((temp = peek(temp, end, '|')) != NULL) {
    cur_cmd = &commands.cmds[n];
    parse_redirs(cur_cmd, prev, temp);
    n ++;

    /* Skip '|' and start scanning again */
    temp ++;
    prev = temp;
  }

  /* Parse the last or first command in case of no pipes */
  if (prev) {
    cur_cmd = &commands.cmds[n++];
    parse_redirs(cur_cmd, prev, end);
  }

  commands.cmd_index = n;
}

void
parse_redirs(struct command *cmd, char *start, char *end)
{
  char *in = NULL;
  char *out = NULL;

  skip_whitespace(&start, end);

  in = peek(start, end, '<');
  out = peek(start, end, '>');

  if (in != NULL) {
    cmd->inputfile = get_token(in + 1, end, NULL);
    cmd->type = REDIR;
  }

  if (out != NULL) {
    cmd->outputfile = get_token(out + 1, end, NULL);
    cmd->type = REDIR;
  }

  parse_exec(cmd, start, end);
}

void
parse_exec(struct command *cmd, char *start, char *end)
{
  int i = 1;
  char *args = NULL;
  char *next;
  skip_whitespace(&start, end);

  if (cmd->type != REDIR) cmd->type = EXEC;

  cmd->name = get_token(start, end, &args);
  cmd->argv[0] = cmd->name;

  while ((args = get_token(args, end, &next)) != NULL) {
    if (strcmp(args, "<") == 0 || strcmp(args, ">") == 0) break;
    cmd->argv[i++] = args;
    args = next;
  }
}

void
execute_command()
{
  int n = commands.cmd_index;
  struct command *cmd;
  
  cmd = &commands.cmds[0];

  if (cmd == NULL) return ;

  if (n > 1) {
    execute_pipes(cmd);
  } else {
    switch (cmd->type) {
      case EXEC:
        execute(cmd);
        break;
      case REDIR:
        execute_redir(cmd);
        break;
    }
  }
}

void
execute(struct command *cmd)
{
  int rv;

  rv = spawn_process(cmd, 0, 1);
  if (rv < 0) return ;
  wait(0);
}

void
execute_redir(struct command *cmd)
{
  int in;
  int rv;
  int out;

  in = 0;
  if (cmd->inputfile)
    in = openfile(cmd->inputfile, O_RDONLY);

  out = 1;
  if (cmd->outputfile)
    out = openfile(cmd->outputfile, O_CREATE | O_WRONLY);

  rv = spawn_process(cmd, in, out);
  if (rv < 0) return ;

  wait(0);
}

void
execute_pipes(struct command *cmd)
{
  int i;
  int n;
  int fd;
  int rv;
  int pipes[2];

  n = commands.cmd_index; 

  fd = 0;
  if (cmd->inputfile) {
    fd = openfile(cmd->inputfile, O_RDONLY);
    if (fd < 0) return ;
  }

  for (i = 0; i < n - 1; i++) {
    pipe(pipes);

    rv = spawn_process(cmd + i, fd, pipes[1]);
    if (rv < 0) return ;

    fd = pipes[0];
    close(pipes[1]);
  }

  cmd = cmd + i;
  int out = 1;
  if (cmd->outputfile) {
    out = openfile(cmd->outputfile, O_CREATE | O_WRONLY);
    if (out < 0) return ;
  }

  rv = spawn_process(cmd, fd, out);
  if (rv < 0) {
    return ;
  }

  for (int i = 0; i < n; i++) wait(0);
}

int
spawn_process(struct command *cmd, int in, int out)
{
  int pid = fork();

  if (pid < 0) {
    fprintf(2, "fork: error forking %s\n", cmd->name);
    return -1;
  }

  if (pid == 0) {
    /* child */
    if (in != 0) {
      close(0);
      dup(in);
      close(in);
    }
    if (out != 1) {
      close(1);
      dup(out);
      close(out);
    }

    exec(cmd->name, cmd->argv);
    fprintf(2, "spawn process: error exec %s\n", cmd->name);
    close(in);
    close(out);
    exit(-1);
  }

  if (in != 0) close(in);
  if (out != 1) close(out);

  return 0;
}

int
getline()
{
  memset(&buffer, 0, sizeof(buffer));

  gets(buffer.buf, BUFSIZE);
  buffer.length = strlen(buffer.buf);

  if (buffer.buf[0] == 0) /* EOF */
    return -1;

  if (buffer.buf[buffer.length - 1] == '\n')
    buffer.buf[buffer.length - 1] = '\0';
  
  return (buffer.length = strlen(buffer.buf));
}

char *
peek(char *start, char *end, char c)
{
  while(start != end && *start){
    if(*start == c) return start;
    start++;
  }
  return NULL;
}

char *
get_token(char *start, char *end, char **next)
{
  skip_whitespace(&start, end);
  char *temp = start;

  if (start >= end) return NULL;

  while (*temp && temp < end) {
    if (*temp == ' ' || *temp == '\t' || *temp == '\n') {
      *temp = '\0';
      break;
    }

    temp ++;
  }

  if (next) *next = temp + 1;

  return start;
}

void
skip_whitespace(char **ptr, char *end)
{
  char *str = *ptr;
  while(str <= end && strchr(" \t\n", *str)) str++;
  *ptr = str;
}

int
openfile(const char *file, int mode)
{
  int fd;

  fd = open(file, mode);
  if (fd < 0) {
    fprintf(2, "error opening file %s\n", file);
    exit(-1);
  }
  return fd;
}

void
error(const char *msg)
{
  fprintf(2, msg);
  exit(-1);
}
