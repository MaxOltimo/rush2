/*
Name: Daniel Lujo
NetID: daniellujo@usf.edu

Description: Simple Shell Implementation in C

This program implements a simple shell that can execute commands, change directories,
manage search paths, and handle output redirection. The shell supports built-in commands
such as "exit", "cd", and "path". It also allows for executing external commands found
in the specified search paths. The shell continuously prompts the user for input, processes the input, and executes
the appropriate commands. It handles errors gracefully and provides feedback to the user.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>

// Function prototypes
void ErrorHandler(int eFlag);
void EnterShell(void);
void ExecuteCommand(char *command);
int CountArgs(char *command);
char *trimWhitespace(char *str);
char **setupCommands(char *command, char **originalCommand);
void changeDirectory(char **args);
void exitCommand(char *command);
void pathCommand(char *command);
void initializeSearchPaths(void);
void freeArgs(char **args);

// Global variables for managing search paths
char **searchPaths = NULL;
int numPaths = 0;

// Function to handle errors and optionally exit the program
void ErrorHandler(int eFlag) {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    if (eFlag == 1) {
        exit(1);
    }
}

// Function to enter the shell loop and process user input
void EnterShell(void) {
    char *buffer = NULL;
    size_t bufferSize = 0;

    while (1) {
        printf("rush> ");
        fflush(stdout);

        if (getline(&buffer, &bufferSize, stdin) == -1) {
            free(buffer);
            continue;
        }

        char *newlinePos = strchr(buffer, '\n');
        if (newlinePos != NULL) {
            *newlinePos = '\0';
        }

        buffer = trimWhitespace(buffer);

        // Remove all space or tab characters
        char *cleanedBuffer = malloc(strlen(buffer) + 1);
        if (cleanedBuffer == NULL) {
            ErrorHandler(0);
            free(buffer);
            continue;
        }
        char *dst = cleanedBuffer;
        for (char *src = buffer; *src != '\0'; src++) {
            if (*src != ' ' && *src != '\t') {
                *dst++ = *src;
            }
        }
        *dst = '\0';

        // Check if the first character is ">"
        if (cleanedBuffer[0] == '>') {
            ErrorHandler(0);
            free(cleanedBuffer);
            free(buffer);
            continue;
        }

        free(cleanedBuffer);

        if (strncmp(buffer, "exit", 4) == 0) {
            exitCommand(buffer);
        } else if (strncmp(buffer, "cd", 2) == 0) {
            char **args = setupCommands(buffer, NULL);
            changeDirectory(args);
            freeArgs(args);
        } else if (strncmp(buffer, "path", 4) == 0) {
            pathCommand(buffer + 5);
        } else {
            if (buffer[0] == '\0') continue;
            char *commands = strdup(buffer);
            char *command;
            while ((command = strsep(&commands, "&")) != NULL) {
                command = trimWhitespace(command);
                if (command[0] == '\0') continue;
                pid_t pid = fork();
                if (pid == 0) {
                    ExecuteCommand(command);
                    exit(0);
                } else if (pid < 0) {
                    ErrorHandler(0);
                }
            }
            while (wait(NULL) > 0);
            free(commands);
        }
    }

    free(buffer);
}

// Function to execute a command
void ExecuteCommand(char *command) {
    char *originalCommand = NULL;
    char **args = setupCommands(command, &originalCommand);
    if (args == NULL) {
        ErrorHandler(0);
        return;
    }

    int outputRedirect = 0;
    char *outputFile = NULL;

    // Check for output redirection
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (outputRedirect) {
                ErrorHandler(0);
                freeArgs(args);
                free(originalCommand);
                return;
            }
            outputRedirect = 1;
            outputFile = args[i + 1];
            if (outputFile == NULL || args[i + 2] != NULL) {
                ErrorHandler(0);
                freeArgs(args);
                free(originalCommand);
                return;
            }
            args[i] = NULL;
        }
    }

    // Handle output redirection
    if (outputRedirect) {
        int fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            ErrorHandler(0);
            freeArgs(args);
            free(originalCommand);
            return;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    // If searchPaths is NULL, only built-in commands can be executed
    if (searchPaths == NULL) {
        freeArgs(args);
        free(originalCommand);
        ErrorHandler(0);
        return;
    }

    // Search for the executable in the searchPaths
    for (int j = 0; j < numPaths; j++) {
        char *path = malloc(strlen(searchPaths[j]) + strlen(args[0]) + 2);
        if (path == NULL) {
            freeArgs(args);
            free(originalCommand);
            ErrorHandler(0);
            return;
        }
        strcpy(path, searchPaths[j]);
        strcat(path, "/");
        strcat(path, args[0]);

        // Attempt to execute the command
        if (access(path, X_OK) == 0) {
            execv(path, args);
            // If execv returns, an error occurred
            free(path);
            freeArgs(args);
            free(originalCommand);
            ErrorHandler(0);
        }
        free(path);
    }

    // If no executable was found
    freeArgs(args);
    free(originalCommand);
    ErrorHandler(0);
}

// Function to free the memory allocated for arguments
void freeArgs(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

// Function to count the number of arguments in a command
int CountArgs(char *command) {
    int count = 0;
    char *temp = strdup(command);
    char *token;

    while ((token = strsep(&temp, " ")) != NULL) {
        if (*token != '\0') {
            count++;
        }
    }

    free(temp);
    return count;
}

// Function to trim whitespace from a string
char *trimWhitespace(char *str) {
    char *end;

    // Normalize the command string
    for (char *p = str; *p != '\0'; p++) {
        if (*p == '\t') {
            *p = ' ';
        }
    }

    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)
        return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    *(end + 1) = '\0';

    return str;
}

// Function to set up commands and arguments
char **setupCommands(char *command, char **originalCommand) {
    command = trimWhitespace(command);

    char *normalizedCommand = malloc(strlen(command) * 2 + 1);
    if (normalizedCommand == NULL) {
        ErrorHandler(0);
        return NULL;
    }
    char *dst = normalizedCommand;
    for (char *src = command; *src != '\0'; src++) {
        if (isspace((unsigned char)*src)) {
            *dst++ = ' ';
            while (isspace((unsigned char)*(src + 1))) src++;
        } else if (*src == '>') {
            if (dst != normalizedCommand && *(dst - 1) != ' ') {
                *dst++ = ' ';
            }
            *dst++ = *src;
            if (*(src + 1) != ' ') {
                *dst++ = ' ';
            }
        } else {
            *dst++ = *src;
        }
    }
    *dst = '\0';

    int numArgs = CountArgs(normalizedCommand);

    char **args = malloc((numArgs + 1) * sizeof(char *));
    if (args == NULL) {
        free(normalizedCommand);
        ErrorHandler(0);
        return NULL;
    }

    int i = 0;
    char *token;
    while ((token = strsep(&normalizedCommand, " ")) != NULL) {
        if (*token != '\0') {
            args[i] = strdup(token);
            if (args[i] == NULL) {
                freeArgs(args);
                free(normalizedCommand);
                ErrorHandler(0);
                return NULL;
            }
            i++;
        }
    }
    args[i] = NULL;

    // Store the original command name for error messages
    if (originalCommand != NULL) {
        *originalCommand = strdup(args[0]);
    }

    free(normalizedCommand);
    return args;
}

// Function to change the current directory
void changeDirectory(char **args) {
    if ((strcmp(args[0], "cd") == 0)) {
        if (args[1] == NULL) {
            ErrorHandler(0);
        } else {
            if (chdir(args[1]) != 0) {
                ErrorHandler(0);
            }
        }
    } else if ((strcmp(args[0], "cd") == 0) && args[2] != NULL) {
        ErrorHandler(0);
    }
}

// Function to handle the "exit" command
void exitCommand(char *command) {
    char **args = setupCommands(command, NULL);

    if (args[0] != NULL && strcmp(args[0], "exit") == 0) {
        if (args[1] != NULL) {
            ErrorHandler(0);
            freeArgs(args);
            return;
        }
        freeArgs(args);
        exit(0);
    }
}

// Function to handle the "path" command
void pathCommand(char *command) {
    if (searchPaths != NULL) {
        for (int i = 0; i < numPaths; i++) {
            free(searchPaths[i]);
        }
        free(searchPaths);
    }

    if (trimWhitespace(command)[0] == '\0') {
        searchPaths = NULL;
        numPaths = 0;
        return;
    }

    numPaths = CountArgs(command);

    searchPaths = malloc((numPaths + 1) * sizeof(char *));
    if (searchPaths == NULL) {
        ErrorHandler(0);
        return;
    }

    int i = 0;
    char *token;
    while ((token = strsep(&command, " ")) != NULL) {
        if (*token != '\0') {
            searchPaths[i] = strdup(token);
            if (searchPaths[i] == NULL) {
                for (int j = 0; j < i; j++) {
                    free(searchPaths[j]);
                }
                free(searchPaths);
                ErrorHandler(0);
                return;
            }
            i++;
        }
    }
    searchPaths[i] = NULL;
}

// Function to initialize the search paths with the default path "/bin"
void initializeSearchPaths(void) {
    numPaths = 1;
    searchPaths = malloc(numPaths * sizeof(char *));
    if (searchPaths == NULL) {
        ErrorHandler(0);
        return;
    }
    searchPaths[0] = strdup("/bin");
    if (searchPaths[0] == NULL) {
        ErrorHandler(0);
        return;
    }
}

// Main function to start the shell
int main(int argc, char *argv[]) {
    if (argc > 1) {
        ErrorHandler(1);
    }

    initializeSearchPaths();
    EnterShell();

    return 0;
}