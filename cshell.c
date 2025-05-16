#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT 1024
#define HISTORY_SIZE 100

// Global history variables
char* history[HISTORY_SIZE];
int history_count = 0;

// Signal handler (Feature 8)
void sigint_handler(int sig) {
    // Do nothing - allow interrupting a running command without closing the shell
    printf("\n");
}

int main() {
    char input[MAX_INPUT];
    char *commands[MAX_INPUT/2 + 1];
    pid_t pid;
    int status;

    // Setup signal handler for SIGINT (Ctrl+C)
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction failed");
        exit(1);
    }

    while (1) {
        printf("sh> ");
        fflush(stdout);

        // Read input from stdin
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            break; // Exit on EOF (Ctrl+D)
        }
        // Remove trailing newline
        input[strcspn(input, "\n")] = '\0';

        // Skip empty lines
        if (strlen(input) == 0)
            continue;

        // Check for history command (Feature 7)
        if (strcmp(input, "history") == 0) {
            for (int i = 0; i < history_count; i++) {
                printf("%d: %s\n", i + 1, history[i]);
            }
            continue;
        }

        // Store the command in history (Feature 7)
        if (history_count < HISTORY_SIZE) {
            history[history_count] = strdup(input);
            history_count++;
        } else {
            free(history[0]);
            for (int i = 0; i < HISTORY_SIZE - 1; i++) {
                history[i] = history[i + 1];
            }
            history[HISTORY_SIZE - 1] = strdup(input);
        }

        // Split input into individual commands.
        // Commands are split by ';' or '&' (for &&, note that this is a simple split)
        int num_commands = 0;
        commands[num_commands] = strtok(input, ";&");
        while (commands[num_commands] != NULL && num_commands < MAX_INPUT/2) {
            num_commands++;
            commands[num_commands] = strtok(NULL, ";&");
        }

        // Process each command sequentially (Feature 5 & 6)
        for (int cmd_i = 0; cmd_i < num_commands; cmd_i++) {
            char *args[MAX_INPUT/2 + 1];
            int arg_index = 0;
            // Tokenize the command into arguments (by whitespace)
            args[arg_index] = strtok(commands[cmd_i], " ");
            while (args[arg_index] != NULL && arg_index < MAX_INPUT/2) {
                arg_index++;
                args[arg_index] = strtok(NULL, " ");
            }
            if (args[0] == NULL) {
                // Empty command, skip
                continue;
            }

            // Detect redirection operators (Feature 3)
            int input_redirect = 0, output_redirect = 0, append_redirect = 0;
            char *input_file = NULL, *output_file = NULL;
            for (int j = 0; j < arg_index; j++) {
                if (args[j] != NULL) {
                    if (strcmp(args[j], "<") == 0) {
                        input_redirect = 1;
                        input_file = args[j + 1];
                        args[j] = NULL;
                    } else if (strcmp(args[j], ">") == 0) {
                        output_redirect = 1;
                        output_file = args[j + 1];
                        args[j] = NULL;
                    } else if (strcmp(args[j], ">>") == 0) {
                        append_redirect = 1;
                        output_file = args[j + 1];
                        args[j] = NULL;
                    }
                }
            }

            // Detect pipe operator(s) (Feature 4)
            int num_pipes = 0;
            int pipe_positions[MAX_INPUT/2];
            for (int j = 0; j < arg_index; j++) {
                if (args[j] != NULL && strcmp(args[j], "|") == 0) {
                    pipe_positions[num_pipes] = j;
                    num_pipes++;
                    args[j] = NULL; // Break the command sequence
                }
            }

            // If pipes exist, then handle pipeline execution
            if (num_pipes > 0) {
                int pipefds[2 * num_pipes];
                for (int i = 0; i < num_pipes; i++) {
                    if (pipe(pipefds + i*2) < 0) {
                        perror("pipe failed");
                        exit(1);
                    }
                }
                int cmd_start = 0;
                for (int i = 0; i <= num_pipes; i++) {
                    pid = fork();
                    if (pid == 0) {
                        // For first command, if input redirection is specified, apply it
                        if (i == 0 && input_redirect) {
                            int fd = open(input_file, O_RDONLY);
                            if (fd < 0) {
                                perror("open input file failed");
                                exit(1);
                            }
                            dup2(fd, STDIN_FILENO);
                            close(fd);
                        }
                        // For last command, if output redirection is specified, apply it
                        if (i == num_pipes && (output_redirect || append_redirect)) {
                            int fd;
                            if (output_redirect) {
                                fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                            } else { // append_redirect
                                fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0600);
                            }
                            if (fd < 0) {
                                perror("open output file failed");
                                exit(1);
                            }
                            dup2(fd, STDOUT_FILENO);
                            close(fd);
                        }
                        // Pipe redirection for intermediate commands
                        if (i < num_pipes) {
                            dup2(pipefds[i*2 + 1], STDOUT_FILENO);
                        }
                        if (i > 0) {
                            dup2(pipefds[(i - 1)*2], STDIN_FILENO);
                        }
                        // Close all pipe file descriptors in the child
                        for (int j = 0; j < 2 * num_pipes; j++) {
                            close(pipefds[j]);
                        }
                        // Execute the segmented command
                        execvp(args[cmd_start], args + cmd_start);
                        perror("execvp failed");
                        exit(1);
                    } else if (pid < 0) {
                        perror("fork failed");
                        exit(1);
                    }
                    // Set the start for next command segment after the current pipe symbol
                    cmd_start = (i < num_pipes) ? pipe_positions[i] + 1 : arg_index;
                }
                // Parent closes all pipe file descriptors
                for (int j = 0; j < 2 * num_pipes; j++) {
                    close(pipefds[j]);
                }
                // Wait for all pipe command children to finish
                for (int i = 0; i <= num_pipes; i++) {
                    wait(NULL);
                }
                // Skip the normal non-pipe execution
                continue;
            }

            // No pipe: execute single command with possible redirections (Feature 1,2,3)
            pid = fork();
            if (pid == 0) {
                // Apply input redirection if set
                if (input_redirect) {
                    int fd = open(input_file, O_RDONLY);
                    if (fd < 0) {
                        perror("open input file failed");
                        exit(1);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                // Apply output redirection (overwrite or append) if set
                if (output_redirect) {
                    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                    if (fd < 0) {
                        perror("open output file failed");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                if (append_redirect) {
                    int fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0600);
                    if (fd < 0) {
                        perror("open append file failed");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                // Execute command
                execvp(args[0], args);
                perror("execvp failed");
                exit(1);
            } else if (pid > 0) {
                // Parent process waits for the child
                wait(&status);
                // If command chaining with && was intended and the command failed, stop processing further commands.
                // (This checks if the original command string had any '&' characters.)
                if (strchr(commands[cmd_i], '&') != NULL && WEXITSTATUS(status) != 0) {
                    break;
                }
            } else {
                perror("fork failed");
            }
        } // end for each command
    } // end while

    // Free history memory before exiting
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }
    return 0;
}
