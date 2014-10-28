#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

void shl_exit(int);

#define PIPE            "|"
#define PROMPT          ">"
#define MAX_LINE        1000
#define MAX_OPT_COUNT   5

#define FAILURE       1
#define QUIT          0

typedef struct cmd {
  char*       command;
  struct cmd* pipe;
  char*       options[MAX_OPT_COUNT + 1];
} Cmd;

#define MEMERROR(reason) { printf("Memory error: %s\n", reason); shl_exit(FAILURE); }

/*
 * Parses the given string into a linked list showing each command linked
 * to the command that it pipes to
 */
Cmd* getCmds(char* rawcmd) {
  char* buffer;
  Cmd* firstCmd = NULL;
  Cmd* lastCmd = NULL;

  buffer = strtok(rawcmd, PIPE);

  if(buffer != NULL) {
    do {
      Cmd* newCmd = malloc(sizeof(Cmd));
      if(newCmd == NULL) MEMERROR(rawcmd);

      if(firstCmd == NULL) {
        firstCmd = newCmd;
        firstCmd->command = buffer;
        firstCmd->pipe = NULL;
        lastCmd = firstCmd;
      } else {
        lastCmd->pipe = newCmd;
        lastCmd = lastCmd->pipe;
        lastCmd->command = buffer;
        lastCmd->pipe = NULL;
      }
    } while((buffer = (char*)strtok(NULL, PIPE)) != NULL);
  }

  // Explode the program options and place them in the right place
  Cmd* c;
  for(c = firstCmd; c != NULL; c = c->pipe){
    c->command = strtok(c->command, " ");
    c->options[0] = c->command;

    int i;
    for(i = 1; i < MAX_OPT_COUNT + 1; i++){
      c->options[i] = strtok(NULL, " ");
      if(c->options[i] == NULL) break;
    }
  }

  return firstCmd;
}

/*
 * Frees the resources used by the command list
 */
void freeCmds(Cmd* c) {
  Cmd* nextCmd;

  if(c != NULL) {
    do {
      nextCmd = c->pipe;
      free(c);
      c = nextCmd;
    }while(c != NULL);
  }
}

/*
 * Runs a given list of commands while setting up the appropriate pipes
 */
int shl_exec(Cmd* cmdLine) {
  int pid;
  int pipefds[2];

  if (cmdLine != NULL && cmdLine->pipe == NULL) {
    if(pid = fork()) {
      exit(wait());
    } else {
      if(execvp(cmdLine->command, cmdLine->options)) {
        perror(cmdLine->command);
        exit(errno);
      }
    }
  } else if (cmdLine != NULL) {
    pipe(pipefds);
    if(pid = fork()) {
      exit(wait());
    } else {
      if(pid = fork()) {
        close(1); dup(pipefds[1]);
        close(pipefds[0]);
        if(execvp(cmdLine->command, cmdLine->options)) {
          perror(cmdLine->command);
          exit(errno);
        }
      } else {
        close(0); dup(pipefds[0]);
        close(pipefds[1]);
        if(fork()) wait();
        else shl_exec(cmdLine->pipe);
      }
    }
  } else {
    exit(0);
  }
}

/*
 * exits the shell either by quitting or some kind of failure
 */
void shl_exit(int reason){
  switch(reason){
    case FAILURE:
      printf("\nNo more input...\n");
      break;
    case QUIT:
      printf("\nQuitting...\n");
      break;
  }

  exit(reason);
}

/* 
 * Read a line from the prompt.
 *
 * If the line says "quit", exit the program successfully.
 * If the program couldn't read the input for some reason, exit abnormally.
 * Otherwise, run the command line.
 *
 * Return the line that was read from the prompt.
 */
char* shl_prompt(){
  static char buffer[MAX_LINE];
  int i = 0;
  bzero(buffer, MAX_LINE);

  printf("%s", PROMPT);

  if(!fgets(buffer, MAX_LINE, stdin)) exit(1);

  // Remove trailing new line
  while(buffer[i++] != '\n');
  buffer[i - 1] = '\0';

  // Check for quit message
  if(!strcmp("quit", buffer)) shl_exit(QUIT);

  return buffer;
}

/*
 * Continiously reads a line, runs the command in a separate process
 * then frees the used resources
 */
int main(int argc, char** argv){
  while(1){
    Cmd* cmdLine = getCmds(shl_prompt());
    if(fork()) wait();
    else shl_exec(cmdLine);
    freeCmds(cmdLine);
  }
}
