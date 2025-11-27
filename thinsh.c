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

// --- SIGNAL HANDLER ---
void handle_sigint(int sig) {
    printf("\n"); // Move to new line
    rl_on_new_line(); // Tell readline we moved
    rl_replace_line("", 0); // Clear current input buffer
    rl_redisplay(); // Show prompt again
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
    printf("history   : List past commands\n");
    printf("path      : Show current PATH\n");
    printf("addpath X : Add X to PATH\n");
    printf("exit      : Quit shell\n");
    printf("command & : Run in background\n");
    printf("Note: Use standard Linux commands (ps, kill) for process management.\n");
}

int main() {
    char *line;  // readline returns a malloc'd string
    char *argv[MAX_ARGS];
    char cwd[1024];
    char prompt[1024];
    int background;

    // Register Signal Handler
    signal(SIGINT, handle_sigint);

    // Configure Readline (Optional)
    using_history();

    while (1) {
        // 1. DYNAMIC PROMPT GENERATION
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            // Find the last slash '/'
            char *base_name = strrchr(cwd, '/');
            
            if (base_name == NULL) {
                // Should not happen, but safe fallback
                snprintf(prompt, sizeof(prompt), "%s> ", cwd); 
            } else if (strcmp(cwd, "/") == 0) {
                // Special case: We are at root
                snprintf(prompt, sizeof(prompt), "/ > ");
            } else {
                // Normal case: Skip the slash (pointer + 1)
                snprintf(prompt, sizeof(prompt), "%s/ > ", base_name + 1);
            }
        } else {
            // If getcwd fails, fallback to default
            snprintf(prompt, sizeof(prompt), "unknown> ");
        }

        // 2. GET INPUT (Replaces printf + fgets)
        // readline handles printing the prompt and capturing arrows/editing
        line = readline(prompt);

        // Check for EOF (Ctrl+D)
        if (!line) break;

        // Skip empty lines but do not crash
        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        // 2. ADD TO HISTORY
        add_history(line);

        // 3. PARSE INPUT
        // We need to copy line because strtok modifies it, 
        // and we might want to keep the original for history? 
        // (Actually readline already saved the original).
        
        int i = 0;
        argv[i] = strtok(line, " ");
        while (argv[i] != NULL && i < MAX_ARGS - 1) {
            argv[++i] = strtok(NULL, " ");
        }
        
        if (argv[0] == NULL) {
            free(line);
            continue;
        }

        // 4. CHECK BACKGROUND (&)
        background = 0;
        if (i > 0 && strcmp(argv[i-1], "&") == 0) {
            background = 1;
            argv[i-1] = NULL;
        }

        // 5. BUILT-INS
        if (strcmp(argv[0], "exit") == 0) {
            free(line);
            break;
        } 
        else if (strcmp(argv[0], "cd") == 0) {
            if (argv[1] == NULL || chdir(argv[1]) != 0) perror("cd failed");
        } 
        else if (strcmp(argv[0], "history") == 0) {
            cmd_history();
        }
        else if (strcmp(argv[0], "help") == 0) cmd_help();
        else if (strcmp(argv[0], "date") == 0 || strcmp(argv[0], "time") == 0) cmd_date();
        else if (strcmp(argv[0], "path") == 0 || strcmp(argv[0], "addpath") == 0) cmd_path(argv);
        else if (strcmp(argv[0], "dir") == 0) {
            argv[0] = "ls"; argv[1] = "-al"; argv[2] = NULL;
            goto execute; // Jump to execution
        }
        else {
            // 6. EXTERNAL COMMANDS
            execute:
            pid_t pid = fork();
            if (pid < 0) perror("Fork failed");
            else if (pid == 0) {
                if (execvp(argv[0], argv) < 0) printf("Command not found: %s\n", argv[0]);
                exit(1);
            } else {
                if (!background) waitpid(pid, NULL, 0);
                else printf("[Background PID: %d]\n", pid);
            }
        }

        // Free memory allocated by readline
        free(line);
    }
    return 0;
}
