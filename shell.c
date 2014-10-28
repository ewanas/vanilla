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
  // The command name
  char*       command; 

  // A pointer to another command to which output is piped
  // Or NULL if none exist
  struct cmd* pipe;     
  
  // The options to the command, including the command itself
  char*       options[MAX_OPT_COUNT + 1];
} Cmd;

// Used whenever there is an error during malloc
#define MEMERROR(reason) { printf("Memory error: %s\n", reason); shl_exit(FAILURE); }

/*
 * Parses the given string into a linked list showing each command linked
 * to the command that it pipes to
 */
Cmd* shl_parse_cmd(char* rawcmd) {
  char* buffer;
  Cmd* first_cmd = NULL;
  Cmd* last_cmd = NULL;

  buffer = strtok(rawcmd, PIPE);

  // Parse everything in between PIPE's
  if(buffer != NULL) {
    do {
      Cmd* new_cmd = malloc(sizeof(Cmd));
      if(new_cmd == NULL) MEMERROR(rawcmd);

      if(first_cmd == NULL) { // Create the first element of the list
        first_cmd = new_cmd;
        first_cmd->command = buffer;
        first_cmd->pipe = NULL;
        last_cmd = first_cmd;
      } else {                // Append a command to the command list
        last_cmd->pipe = new_cmd;
        last_cmd = last_cmd->pipe;
        last_cmd->command = buffer;
        last_cmd->pipe = NULL;
      }
    } while((buffer = (char*)strtok(NULL, PIPE)) != NULL);
  }

  // Explode the program options and place them in the right place
  // Place the program name as the first option
  Cmd* c;
  for(c = first_cmd; c != NULL; c = c->pipe){
    c->command = strtok(c->command, " ");
    c->options[0] = c->command;

    int i;
    for(i = 1; i < MAX_OPT_COUNT + 1; i++){
      c->options[i] = strtok(NULL, " ");
      if(c->options[i] == NULL) break;
    }
  }

  return first_cmd;
}

/*
 * Frees the resources used by the command list
 */
void shl_free_cmd(Cmd* c) {
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
int shl_exec(Cmd* cmd_line) {
  int pid;
  int pipefds[2];

  if (cmd_line != NULL && cmd_line->pipe == NULL) { // A program without piping
    if(execvp(cmd_line->command, cmd_line->options)) {
      perror(cmd_line->command);
      exit(errno);
    }
  } else if (cmd_line != NULL) {  // A program piped to another
    pipe(pipefds);

    if(pid = fork()) {  // First program
      close(1);
      close(pipefds[0]);
      dup(pipefds[1]);

      if(execvp(cmd_line->command, cmd_line->options)) {
        perror(cmd_line->command);
        exit(errno);
      }
    } else {  // Whatever else exists after the first program that is piped to
      close(0);
      dup(pipefds[0]);
      close(pipefds[1]);

      if(!fork()) shl_exec(cmd_line->pipe);
    }
  }

  exit(wait());
}

/*
 * exits the shell either by quitting or some kind of failure
 */
void shl_exit(int reason){
  switch(reason){
    case FAILURE:
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
    Cmd* cmd_line = shl_parse_cmd(shl_prompt());

    if(fork()) wait();
    else shl_exec(cmd_line);

    shl_free_cmd(cmd_line);
  }
}
