#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h> 


#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 100
#define PROMPT_SUFFIX "> "

// Function prototypes
void run_shell(FILE *input_stream);
int execute_command(char *command);
char **parse_input(char *input);
char *resolve_executable(char *command);

// Global variable for PATH
char *path[MAX_ARGS] = {NULL};  // Initial path is empty

int main(int argc, char *argv[]) {
    if (argc > 2) {
        fprintf(stderr, "An error has occurred\n");
        exit(1);
    }

    FILE *input_stream = (argc == 2) ? fopen(argv[1], "r") : stdin;
    if (argc == 2 && !input_stream) {
        fprintf(stderr, "An error has occurred\n");
        exit(1);
    }

    run_shell(input_stream);

    if (argc == 2) fclose(input_stream);
    return 0;
}

void run_shell(FILE *input_stream) {
    char *input = NULL;
    size_t len = 0;

    while (1) {
        if (input_stream == stdin) {
            char cwd[MAX_INPUT_SIZE];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                perror("getcwd");
                exit(1);
            }
            printf("Mark: %s%s", cwd, PROMPT_SUFFIX);
            fflush(stdout);
        }

        if (getline(&input, &len, input_stream) == -1) {
            free(input);
            break;  // Exit on EOF
        }

        if (execute_command(input)) {
            free(input);
            break;  // Exit if "exit" command is given
        }
    }
}

char *resolve_executable(char *command) {
    // If path is empty, commands cannot run unless they are built-in
    if (path[0] == NULL) {
        return NULL;
    }

    // Search for the command in the specified PATH directories
    for (int i = 0; path[i] != NULL; i++) {
        char full_path[MAX_INPUT_SIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", path[i], command);

        // Check if the file exists and is executable
        if (access(full_path, X_OK) == 0) {
            // If the executable is found and is accessible, return its full path
            return strdup(full_path);
        }
    }

    // If not found, return NULL
    return NULL;
}

int execute_command(char *command) {
    // Remove the trailing newline
    command[strcspn(command, "\n")] = '\0';

    // Parse the input
    char **args = parse_input(command);
    if (args[0] == NULL) {
        free(args);
        return 0;  // Empty command
    }

    // Check for redirection
    char *redirection_file = NULL;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL || args[i + 2] != NULL) {
                fprintf(stderr, "An error has occurred\n");
                free(args);
                return 0;
            }
            redirection_file = args[i + 1];
            args[i] = NULL;  // Terminate args at the redirection symbol
            break;
        }
    }

    // Built-in commands
    if (strcmp(args[0], "exit") == 0) {
        if (args[1] != NULL) fprintf(stderr, "An error has occurred\n");
        else {
            free(args);
            return 1;  // Exit the shell
        }
    } else if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL || args[2] != NULL || chdir(args[1]) != 0) {
            fprintf(stderr, "An error has occurred\n");
        }
        free(args);
        return 0;
    } else if (strcmp(args[0], "path") == 0) {
    // Clear the current path
    for (int i = 0; i < MAX_ARGS; i++) {
        if (path[i]) {
            free(path[i]);  // Free any previously allocated memory
            path[i] = NULL;
        }
    }

    // If no arguments are given, path is cleared
    if (args[1] == NULL) {
        free(args);
        return 0;
    }

    // Add the new directories to the path array
    for (int i = 1; args[i] != NULL; i++) {
        if (access(args[i], F_OK) != 0) {
            fprintf(stderr, "An error has occurred\n");
        } else if (access(args[i], X_OK) != 0) {
            fprintf(stderr, "An error has occurred\n");
        } else {
            path[i - 1] = strdup(args[i]);  // Copy the directory into the path array
        }
    }
    path[MAX_ARGS - 1] = NULL;  // Ensure the path array is null-terminated

    free(args);
    return 0;
    } else if (strcmp(args[0], "loop") == 0) {
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "An error has occurred\n");
        free(args);
        return 0;
    }

    int loop_count = atoi(args[1]);
    if (loop_count <= 0) {
        fprintf(stderr, "An error has occurred\n");
        free(args);
        return 0;
    }

    // The rest of the command after 'loop' and loop count
    char **loop_command = &args[2];

    // Loop through the iterations
    for (int i = 0; i < loop_count; i++) {
        // Create a new argument list for this iteration
        char *iter_command[MAX_ARGS];
        int j = 0;

        while (loop_command[j] != NULL) {
            // Check for $loop placeholder and replace with iteration number
            if (strcmp(loop_command[j], "$loop") == 0) {
                char loop_value[12];
                snprintf(loop_value, sizeof(loop_value), "%d", i + 1);
                iter_command[j] = strdup(loop_value);  // Replace with the current iteration number
            } else {
                iter_command[j] = strdup(loop_command[j]);  // Copy the original command part
            }
            j++;
        }
        iter_command[j] = NULL;  // Null-terminate the argument list

        // Find the executable path for the command
        char *exec_path = resolve_executable(iter_command[0]);
        if (!exec_path) {
            fprintf(stderr, "An error has occurred\n");
            free(args);
            return 0;
        }

        // Fork the process and execute the command
        pid_t pid = fork();
        if (pid == 0) {
            execv(exec_path, iter_command);
            fprintf(stderr, "An error has occurred\n");
            exit(1);
        } else if (pid > 0) {
            wait(NULL);  // Wait for the child process to complete
        } else {
            fprintf(stderr, "An error has occurred\n");
        }

        // Free allocated memory
        free(exec_path);
        for (int k = 0; iter_command[k] != NULL; k++) {
            free(iter_command[k]);
        }
    }

    free(args);
    return 0;
}


    // Resolve executable for non-built-in commands
    char *exec_path = resolve_executable(args[0]);
    if (!exec_path) {
        fprintf(stderr, "An error has occurred\n");
        free(args);
        return 0;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (redirection_file) {
            int fd = open(redirection_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                fprintf(stderr, "An error has occurred\n");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        execv(exec_path, args);
        fprintf(stderr, "An error has occurred\n");
        exit(1);
    } else if (pid > 0) {
        // Parent process
        wait(NULL);
    } else {
        fprintf(stderr, "An error has occurred\n");
    }

    free(exec_path);
    free(args);
    return 0;
}

char **parse_input(char *input) {
    char **args = malloc(MAX_ARGS * sizeof(char *));
    if (!args) {
        fprintf(stderr, "An error has occurred\n");
        exit(1);
    }

    int i = 0;
    char *start = input;
    char *end;
    while (*start && i < MAX_ARGS - 1) {
        // Skip leading whitespace
        while (isspace((unsigned char)*start)) {
            start++;
        }
        if (*start == '\0') break;

        char buffer[MAX_INPUT_SIZE];
        int buf_idx = 0;

        // Parse one argument
        while (*start && !isspace((unsigned char)*start)) {
            if (*start == '"') {
                // Handle quoted segment
                start++; // Skip the opening quote
                while (*start && *start != '"') {
                    buffer[buf_idx++] = *start++;
                }
                if (*start == '"') {
                    start++; // Skip the closing quote
                } else {
                    fprintf(stderr, "An error has occurred\n");
                    free(args);
                    return NULL; // Unmatched quotes
                }
            } else {
                // Handle unquoted segment
                buffer[buf_idx++] = *start++;
            }
        }

        buffer[buf_idx] = '\0'; // Null-terminate the argument
        args[i++] = strdup(buffer); // Store the parsed argument
    }
    args[i] = NULL;

    return args;
}
