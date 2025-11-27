#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

// --- SIGNAL HANDLER (Requirement: Abort/Ctrl+C) ---
// This ensures Ctrl+Shift+C kills the running process, not the shell itself.
void handle_sigint(int sig) {
    printf("\nCaught Signal %d (Ctrl+Shift+C). Type 'exit' to quit.\n", sig);
    printf("thinsh> "); // Re-print prompt
    fflush(stdout);
}

// --- BUILT-IN: TIME/DATE ---
void cmd_date() {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char s[64];
    strftime(s, sizeof(s), "%c", tm);
    printf("Current Date/Time: %s\n", s);
}

// --- BUILT-IN: HISTORY LIST ---
// This lists the commands stored in memory
void cmd_history() {
    HIST_ENTRY **the_list;
    int i;

    the_list = history_list();
    if (the_list) {
        for (i = 0; the_list[i]; i++) {
            printf("%d: %s\n", i + 1, the_list[i]->line);
        }
    }
}

// --- BUILT-IN: PATH MANIPULATION ---
void cmd_path(char **args) {
    if (args[1] == NULL) {
        // Just print current path
        printf("PATH: %s\n", getenv("PATH"));
    } else if (strcmp(args[0], "addpath") == 0) {
        // Append new path
        char *current_path = getenv("PATH");
        char new_path[2048];
        snprintf(new_path, sizeof(new_path), "%s:%s", current_path, args[1]);
        setenv("PATH", new_path, 1);
        printf("Path updated.\n");
    }
}

// --- BUILT-IN: HELP ---
void cmd_help() {
    printf("--- thinsh Help ---\n");
    printf("dir       : List files (ls -al)\n");
    printf("date      : Show time/date\n");
    printf("path      : Show current PATH\n");
    printf("addpath X : Add X to PATH\n");
    printf("exit      : Quit shell\n");
    printf("command & : Run in background\n");
    printf("Note: Use standard Linux commands (ps, kill) for process management.\n");
}

int main() {
    char line[MAX_CMD_LEN];
    char *argv[MAX_ARGS];
    int background;

    // Register Signal Handler
    signal(SIGINT, handle_sigint);

    while (1) {
        // 1. Prompt
        printf("thinsh> ");
        if (fgets(line, sizeof(line), stdin) == NULL) break; // Handle Ctrl+D

        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Skip empty lines
        if (strlen(line) == 0) continue;

        // 2. Parsing (Tokenize)
        int i = 0;
        argv[i] = strtok(line, " ");
        while (argv[i] != NULL && i < MAX_ARGS - 1) {
            argv[++i] = strtok(NULL, " ");
        }
        
        // 3. Check for Background Mode (&)
        // If the last argument is "&", remove it and set flag
        background = 0;
        if (i > 0 && strcmp(argv[i-1], "&") == 0) {
            background = 1;
            argv[i-1] = NULL; // Remove "&" from arguments passed to exec
        }

        if (argv[0] == NULL) continue;

        // 4. Built-in Commands Router
        if (strcmp(argv[0], "exit") == 0) {
            printf("Goodbye!\n");
            exit(0);
        } 
        else if (strcmp(argv[0], "cd") == 0) {
            if (argv[1] == NULL || chdir(argv[1]) != 0) {
                perror("cd failed");
            }
            continue;
        } 
        else if (strcmp(argv[0], "help") == 0) {
            cmd_help();
            continue;
        }
        else if (strcmp(argv[0], "date") == 0 || strcmp(argv[0], "time") == 0) {
            cmd_date();
            continue;
        }
        else if (strcmp(argv[0], "path") == 0 || strcmp(argv[0], "addpath") == 0) {
            cmd_path(argv);
            continue;
        }
        else if (strcmp(argv[0], "dir") == 0) {
            // Map "dir" to "ls -al"
            argv[0] = "ls";
            argv[1] = "-al";
            argv[2] = NULL;
            // Fall through to execvp...
        }

        // 5. Execute External Commands (Fork-Exec)
        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
        } 
        else if (pid == 0) {
            // --- CHILD PROCESS ---
            
            // Execute the command
            if (execvp(argv[0], argv) < 0) {
                printf("Command not found: %s\n", argv[0]);
            }
            exit(1); // Kill child if exec fails
        } 
        else {
            // --- PARENT PROCESS ---
            if (background) {
                // Background mode: Don't wait, just print PID
                printf("[Process running in background with PID: %d]\n", pid);
            } else {
                // Foreground mode: Wait for child to finish
                waitpid(pid, NULL, 0);
            }
        }
    }
    return 0;
}
