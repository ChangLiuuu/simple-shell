#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
## Internal Commands:
	1. cd <directory> - Change the current default directory to <directory>.
	   If the <directory> argument is not present, report the current directory.
	   If the directory does not exist, an appropriate error should be reported.
	   This command should also change the PWD environment variable.
	2. clr – Clear the screen. Like the command “clear”.
	3. dir <directory> - List the contents of directory <directory>. Like “ls”.
	4. environ – List all the environments strings. (PWD)
	5. echo <comment> - Display <comment> on the display followed by a new line
	(multiple spaces/tabs may be reduced to a single space).
	6. help – Display the user manual.

## Other Commands:
	1. A command "exit" which terminates the shell.
	# Concepts: shell commands, exiting the shell
	# System calls: exit()

	2. A command “ls” with no arguments
	# Example: ls
	# Details: Your shell must block until the command completes and,
	  if the return code is abnormal, print out a message to that effect.
	# Concepts: Forking a child process, waiting for it to complete,
	  synchronous execution
	# System calls: fork(), execvp(), exit(), wait()

	3. A command “ls” with arguments, like “ls –l”
	# Example: ls -l
	# Details: Argument 0 is the name of the command
	# Concepts: Command-line parameters

	4. A command, with or without arguments, whose output is redirected to a file
	# Example: ls -l > foo
	# Details: This takes the output of the command and
	  put it in the named file (you can choose the name)
	# Concepts: File operations, output redirection
	# System calls: freopen()

	5. A command, with or without arguments, whose input is redirected from a file
	# Example: sort < testfile
	# Details: This takes the named file (your choice of filename) as input to the command
	# Concepts: Input redirection, more file operations
	# System calls: freopen()

	6. A command, with or without arguments,
	   whose output is piped to the input of another command.
	# Example: ls -l | more
	# Details: This takes the output of the first command and makes it the input
	  to the second command
	# Concepts: Pipes, synchronous operation
	# System calls: pipe()
 */

#define LINE_MAX    1024 // max length of input
#define PATH_MAX    1024 // max length of buffer that stores path (used in getcwd())
#define ARG_MAX     8    // max quantity of arguments
#define NUM_CMD_MAX 2    // our shell at most supports 2 commands

extern char** environ;

// structs, at most 2 Cmd in this project
struct Cmd {
    char *cmd;
    char *infile;
    char *outfile;
    int infd;
    int infdpipe;
    int outfd;
    int outfdpipe;
};

// global vars
char cdir[PATH_MAX];

char * trimSpace(char * buffer)
{
    char * new_buffer = NULL;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] != ' ') {
            new_buffer = &buffer[i];
            break;
        }
    }
    for (int j = strlen(buffer) - 1; j >= 0; j--) {
        if (buffer[j] != ' ') {
            buffer[j + 1] = '\0';
            break;
        }
    }
    return new_buffer;
}

void read_line(char *line)
{
    fgets(line, LINE_MAX, stdin);
    // removing the trailing newline
    line[strlen(line) - 1] = '\0';
}

int parse_line(char *line, struct Cmd *cmds)
{
    memset(cmds, 0, sizeof(struct Cmd) * NUM_CMD_MAX);

    int num_of_commands = 0;
    // at most allow two commands
    char *token[2];
    cmds[0].infile = NULL;
    cmds[1].infile = NULL;
    cmds[0].outfile = NULL;
    cmds[1].outfile = NULL;
    cmds[0].cmd = NULL;
    cmds[1].cmd = NULL;
    token[0] = strtok(line, "|");
    token[1] = strtok(NULL, "|");

    if (token[0] != NULL) {
        num_of_commands++;
        for (int i = 0; token[0][i] != '\0'; i++) {
            cmds[0].cmd = &token[0][0];
            if (token[0][i] == '<') {
                token[0][i] = '\0';
                cmds[0].infile = &token[0][i + 1];
            }
            if (token[0][i] == '>') {
                token[0][i] = '\0';
                cmds[0].outfile = &token[0][i + 1];
            }
        }
    }
    if (token[1] != NULL) {
        num_of_commands++;
        for (int j = 0; token[1][j] != '\0'; j++) {
            cmds[1].cmd = &token[1][0];
            if (token[1][j] == '>') {
                token[1][j] = '\0';
                cmds[1].outfile = &token[1][j + 1];
            }
        }
    }
    if (cmds[0].cmd != NULL) cmds[0].cmd = trimSpace(cmds[0].cmd);
    if (cmds[0].infile != NULL) cmds[0].infile = trimSpace(cmds[0].infile);
    if (cmds[0].outfile != NULL) cmds[0].outfile = trimSpace(cmds[0].outfile);
    if (cmds[1].cmd != NULL) cmds[1].cmd = trimSpace(cmds[1].cmd);
    if (cmds[1].infile != NULL) cmds[1].infile = trimSpace(cmds[1].infile);
    if (cmds[1].outfile != NULL) cmds[1].outfile = trimSpace(cmds[1].outfile);
    return num_of_commands;
}

int eval_commands(struct Cmd *cmds, int numCmd)
{
    for (int i = 0; i < numCmd; i++) {
        struct Cmd *cmd = &cmds[i];
        cmd->infd = -1;
        cmd->infdpipe = -1;
        cmd->outfd = -1;
        cmd->outfdpipe = -1;
        if (cmd->infile) {
            if ((cmd->infd = open(cmd->infile, O_RDONLY)) == -1) {
                perror(cmd->infile);
                return -1;
            }
        }
        if (cmd->outfile) {
            if ((cmd->outfd = open(cmd->outfile, O_RDWR | O_CREAT, 0644)) == -1) {
                perror(cmd->outfile);
                return -1;
            }
        }
    }
    if (numCmd == 2) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            exit(-1);
        }
        cmds[0].outfd = pipefd[1];
        cmds[0].outfdpipe = pipefd[0];
        cmds[1].infd = pipefd[0];
        cmds[1].infdpipe = pipefd[1];
    }
    return 0;
}

int execute(struct Cmd *c)
{
    int pid = -1;
    if (!strncmp(c->cmd, "cd", 2)) {
        // builtin function for changing directory
        char *newDir = c->cmd + 3;
        if (!*(c->cmd + 2)) {
            printf("%s\n", cdir);
            return pid;
        } else if (newDir[0] != '/') {
            char tempPath[PATH_MAX];
            strcpy(tempPath, cdir);
            int len = strlen(tempPath);
            sprintf(tempPath, "%s/%s", tempPath, newDir);
            if (chdir(tempPath) == -1) {
                perror(tempPath);
            } else {
                char **var;
                for (var = environ; *var != NULL; var++) {
                    if (strncmp(*var,"PWD", 3) == 0) {
                        char cwd[LINE_MAX];
                        getcwd(cwd, sizeof(cwd));
                        strcpy(*var, "PWD=");
                        strcat(*var, cwd);
                        //printf("environ's PWD=%s\n", *var);
                    }
                }
            }
        } else {
            if (chdir(newDir) == -1) {
                perror(newDir);
            }
        }
        if (!getcwd(cdir, PATH_MAX)) {
            perror("getcwd");
            exit(-1);
        }

    } else if (!strcmp(c->cmd, "about")) {
        printf("Jui-Chun Huang, W1284254\nChang Liu, W1272763\n");
    } else if (!strcmp(c->cmd, "clr")) {
        system("clear");
    } else if (!strcmp(c->cmd, "dir")) {
        system("ls");
    } else if (!strcmp(c->cmd, "dir -l")) {
    	system("ls -l");
    } else if (!strcmp(c->cmd, "environ")) {
        char **var;
        for (var = environ; *var != NULL; ++var) {
            printf("%s\n", *var);
        }
    } else if (!strcmp(c->cmd, "help")) {
        system("help");
    } else if (!strcmp(c->cmd, "exit")) {
    	printf("Successfully Exit The Shell\n");
        exit(0);
    } else {
        // fork-exec
        pid = fork();
        if (pid == 0) {
            // setup the correct fd to implement pipe and redirection
            if (c->infd != -1) {
                if (dup2(c->infd, 0) == -1) {
                    perror("dup2");
                    exit(-1);
                } else {
                    close(c->infd);
                    if (c->infdpipe != -1) {
                        close(c->infdpipe);
                    }
                }
            }
            if (c->outfd != -1) {
                if (dup2(c->outfd, 1) == -1) {
                    perror("dup2");
                    exit(-1);
                } else {
                    close(c->outfd);
                    if (c->outfdpipe != -1) {
                        close(c->outfdpipe);
                    }
                }
            }

            char *argv[ARG_MAX];
            int i = 0;
            char *token = strtok(c->cmd, " ");
            while(token) {
                argv[i++] = token;
                token = strtok(NULL, " ");
            }
            argv[i] = NULL;
            if (execvp(argv[0], argv) == -1) {
                perror(argv[0]);
                exit(-1);
            };
        } else if (pid < 0) {
            perror("fork failed");
            exit(-1);
        }
    }
    return pid;
}

int main()
{
	char line[LINE_MAX];
	struct Cmd cmds[NUM_CMD_MAX];

	// copy current dir to cdir buffer, PATH_MAX is the size of buffer
	getcwd(cdir, PATH_MAX);

	while (1) {
		printf("shell> ");
		read_line(line);
		int numCmd = parse_line(line, cmds);

		if (numCmd == 0) continue; // empty command

		// Figure out the input fd and output fd for each command.
		// Set up pipe if necessary
		if (eval_commands(cmds, numCmd) == -1) continue;

        int pid[2];
        for (int i = 0; i < numCmd; i++) {
            pid[i] = execute(&cmds[i]);
        }
        for (int i = 0; i < numCmd; i++) {
            int status;
            if (pid[i] != -1) {
                if (waitpid(pid[i], &status, 0) == -1) {
                    perror("waitpid");
                } else {
                    if (cmds[i].infd != -1) close(cmds[i].infd);
                    if (cmds[i].outfd != -1) close(cmds[i].outfd);
                }
            }
        }
    }
}
