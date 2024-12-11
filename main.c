#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

// Shared balance and mutex
double account_balance = 1000.0;
pthread_mutex_t balance_mutex;

// Function prototypes
void *handle_request(void *arg);
void banking_subprocess(int read_fd, int write_fd);

int main() {
    int pipe_fd[2]; // Pipe for communication
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // Subprocess
        close(pipe_fd[1]); // Close write end in subprocess
        banking_subprocess(pipe_fd[0], pipe_fd[1]);
        exit(EXIT_SUCCESS);
    } else { // Main process
        close(pipe_fd[0]); // Close read end in main process

        char command[256];
        while (1) {
            printf("\nEnter command (deposit, withdraw, balance, exit): ");
            fgets(command, sizeof(command), stdin);
            command[strcspn(command, "\n")] = '\0';

            if (strcmp(command, "exit") == 0) {
                write(pipe_fd[1], "exit", 5);
                break;
            }
            write(pipe_fd[1], command, strlen(command) + 1);
        }

        close(pipe_fd[1]); // Close write end
        wait(NULL); // Wait for subprocess
    }

    return 0;
}

void banking_subprocess(int read_fd, int write_fd) {
    char buffer[256];

    while (1) {
        ssize_t bytes_read = read(read_fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            break; // Pipe closed or error
        }

        buffer[bytes_read] = '\0';
        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting banking subprocess...\n");
            break;
        }

        pthread_t thread;
        char *request = strdup(buffer); // Duplicate request string for thread
        if (pthread_create(&thread, NULL, handle_request, request) != 0) {
            perror("pthread_create");
            free(request);
        }
        pthread_detach(thread); // Automatically reclaim thread resources
    }
}

void *handle_request(void *arg) {
    char *request = (char *)arg;
    pthread_mutex_lock(&balance_mutex);

    if (strncmp(request, "deposit ", 8) == 0) {
        double amount = atof(request + 8);
        account_balance += amount;
        printf("Deposited %.2f, New Balance: %.2f\n", amount, account_balance);
    } else if (strncmp(request, "withdraw ", 9) == 0) {
        double amount = atof(request + 9);
        if (amount > account_balance) {
            printf("Insufficient funds! Current Balance: %.2f\n", account_balance);
        } else {
            account_balance -= amount;
            printf("Withdrew %.2f, New Balance: %.2f\n", amount, account_balance);
        }
    } else if (strcmp(request, "balance") == 0) {
        printf("Current Balance: %.2f\n", account_balance);
    } else {
        printf("Unknown command: %s\n", request);
    }

    pthread_mutex_unlock(&balance_mutex);
    free(request); // Free dynamically allocated memory
    return NULL;
}
