#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

// Saldo compartilhado e mutex
double account_balance = 1000.0;
pthread_mutex_t balance_mutex;

// Protótipos de funções
void *handle_request(void *arg);
void banking_subprocess(int read_fd, int write_fd);

int main() {
    int pipe_fd[2]; // Pipe para comunicação
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // Subprocesso
        close(pipe_fd[1]); // Fecha a extremidade de escrita no subprocesso
        banking_subprocess(pipe_fd[0], pipe_fd[1]);
        exit(EXIT_SUCCESS);

        
    } else { // Processo principal
        close(pipe_fd[0]); // Fecha a extremidade de leitura no processo principal

        char command[256];
        while (1) {
            printf("\nDigite o comando seguido pelo valor (depositar, sacar, saldo, sair): ");
            fgets(command, sizeof(command), stdin);
            command[strcspn(command, "\n")] = '\0';

            if (strcmp(command, "sair") == 0) {
                write(pipe_fd[1], "sair", 5);
                break;
            }
            write(pipe_fd[1], command, strlen(command) + 1);
        }

        close(pipe_fd[1]); // Fecha a extremidade de escrita
        wait(NULL); // Aguarda pelo subprocesso
    }

    return 0;
}

void banking_subprocess(int read_fd, int write_fd) {
    char buffer[256];

    while (1) {
        ssize_t bytes_read = read(read_fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            break; // Pipe fechado ou erro
        }

        buffer[bytes_read] = '\0';
        if (strcmp(buffer, "sair") == 0) {
            printf("Saindo do subprocesso bancário...\n");
            break;
        }

        pthread_t thread;
        char *request = strdup(buffer); // Duplicar a string de requisição para a thread
        if (pthread_create(&thread, NULL, handle_request, request) != 0) {
            perror("pthread_create");
            free(request);
        }
        pthread_detach(thread); // Recolher automaticamente os recursos da thread
    }
}

void *handle_request(void *arg) {
    char *request = (char *)arg;
    pthread_mutex_lock(&balance_mutex);

    if (strncmp(request, "depositar ", 10) == 0) {
        double amount = atof(request + 10);
        account_balance += amount;
        printf("Depositado %.2f, Novo Saldo: %.2f\n", amount, account_balance);
    } else if (strncmp(request, "sacar ", 6) == 0) {
        double amount = atof(request + 6);
        if (amount > account_balance) {
            printf("Fundos insuficientes! Saldo Atual: %.2f\n", account_balance);
        } else {
            account_balance -= amount;
            printf("Sacado %.2f, Novo Saldo: %.2f\n", amount, account_balance);
        }
    } else if (strcmp(request, "saldo") == 0) {
        printf("Saldo Atual: %.2f\n", account_balance);
    } else {
        printf("Comando desconhecido: %s\n", request);
    }

    pthread_mutex_unlock(&balance_mutex);
    free(request); // Liberar a memória alocada dinamicamente
    return NULL;
}