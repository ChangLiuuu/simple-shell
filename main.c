#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

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

int about(char **args)
{
    printf("Jui-Chun Huang, W1284254\n");
    printf("Chang Liu, W1272763\n");
    return 1;
}

int cd(char **args)
{
    return 1;
}

int clr(char **args)
{
    system("clear");
    return 1;
}

int dir(char **args)
{
    return 1;
}

int environ(char **args)
{
    return 1;
}

int echo(char **args)
{
    return 1;
}

int help(char **args)
{
    return 1;
}

int cexit(char **args)
{
    return 0;
}

int ls(char **args)
{
    return 1;
}

char *command_list[] = {
        "about",
        "cd",
        "clr",
        "dir",
        "environ",
        "echo",
        "help",
        "cexit",
        "ls"
};

int (*command_idx[]) (char**) = {
        &about,
        &cd,
        &clr,
        &dir,
        &environ,
        &echo,
        &help,
        &cexit,
        &ls
};

char *read_line()
{
    char *line = NULL;
    size_t bufsize = 0;
    getline(&line, &bufsize, stdin); // getline() also allocates a buffer
    return line;
}

#define TOKEN_BUFSIZE 64
#define TOKEN_DELIMITER " \t\r\n\a"

char **split_line(char *line)
{
    int bufsize = TOKEN_BUFSIZE, position = 0;
    char **tokens = malloc(sizeof(char*) * bufsize);
    char *token;

    if (!tokens) {
        fprintf(stderr, "Error: allocation error\n");
        exit(EXIT_FAILURE);
    }

    // return pointers to within the string you give it,
    // and place \0 bytes at the end of each token
    token = strtok(line, TOKEN_DELIMITER);

    while (token != NULL) {
        tokens[position] = token;
        position++;
        if (position >= bufsize) {
            bufsize += TOKEN_BUFSIZE;
            tokens = realloc(tokens, sizeof(char*) * bufsize);
            if (!tokens) {
                fprintf(stderr, "Error: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, TOKEN_DELIMITER);
    }
    tokens[position] = NULL;
    return tokens;
}

int create_process(char **args)
{
    pid_t pid;
    int status;

    pid = fork();
    // create child process
    if (pid < 0) {
        // forking was unsuccessful
        perror("Error: forking error");
    } else if (pid == 0) {
        // the execution fails
        if (execvp(args[0], args) == -1) {
            printf("Error: no such command exists\n");
        }
        exit(EXIT_FAILURE);
    } else {
        // fork() returns positive number (process id of pid, the child process)
        do {
            // wait for the pid to terminate
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

int execute(char **args)
{
    int i;

    if (args[0] == NULL) {
        return 1;
    }
    for (i = 0; i < 9; i++) {
        if (strcmp(args[0], command_list[i]) == 0) {
            return (*command_idx[i])(args);
        }
    }
    return create_process(args);
}

/*
	Three steps in each command_loop:
	1. Read the command from standard input.
	2. Separate the command into a series of arguments.
	3. Execute with parsed commands.

	After the execution of each loop, free up the space of line and args.
 */

int command_loop()
{
    char *line;
    char **args;
    int keep_in_loop;

    do {
        printf("shell> ");
        line = read_line();
        args = split_line(line);
        keep_in_loop = execute(args);

        free(line);
        free(args);
    } while (keep_in_loop);
    return 0;
}

int main(int argc, char **argv)
{
    command_loop();
    printf("Successfully exit the shell");
    exit(EXIT_SUCCESS);
    return 0;
}