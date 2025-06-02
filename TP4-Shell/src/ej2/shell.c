#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_COMMANDS 200
#define MAX_ARGS 64

// Libera la memoria reservada para los comandos
void liberar(char *commands[], int cantidad) {
    for (int i = 0; i < cantidad; i++) {
        free(commands[i]);
    }
}

// Cuenta cuántos argumentos tiene un comando (considerando comillas dobles)
int contar_argumentos(const char *comando) {
    int count = 0, in_quotes = 0;
    for (const char *p = comando; *p; ++p) {
        if (*p == '"') {
            in_quotes = !in_quotes;  // alterna si está dentro o fuera de comillas
        } else if (!in_quotes && isspace(*p)) {
            // al encontrar espacio fuera de comillas, salta los espacios y cuenta el argumento
            while (isspace(*p)) p++;
            count++;
        }
    }
    return count + 1; // suma el último argumento
}

// Parsea una línea separada por '|' en varios comandos
int parsear_comandos(char *command, char *commands[]) {
    int command_count = 0;
    int entre_comillas = 0;
    char *inicio = command;

    for (char *p = command;; ++p) {
        if (*p == '"') entre_comillas = !entre_comillas;

        // Al encontrar un pipe fuera de comillas o el final de la línea
        if ((*p == '|' && !entre_comillas) || *p == '\0') {
            size_t len = p - inicio;

            // Elimina espacios a la izquierda
            while (len > 0 && isspace(inicio[0])) { inicio++; len--; }

            // Elimina espacios a la derecha
            while (len > 0 && isspace(inicio[len - 1])) len--;

            // Si el comando quedó vacío, se notifica error
            if (len == 0) {
                fprintf(stderr, "Error: comando vacío\n");
                liberar(commands, command_count);
                return -1;
            }

            // Reserva memoria y copia el comando
            commands[command_count] = malloc(len + 1);
            if (!commands[command_count]) {
                perror("malloc");
                exit(1);
            }
            strncpy(commands[command_count], inicio, len);
            commands[command_count][len] = '\0';

            // Verifica que no se exceda el máximo de argumentos por comando
            if (contar_argumentos(commands[command_count]) > MAX_ARGS) {
                fprintf(stderr, "Error: se excedió la cantidad máxima de argumentos permitidos\n");
                liberar(commands, command_count + 1);
                return -1;
            }

            command_count++;

            // Si llegó al final de la línea, corta
            if (*p == '\0') break;

            // Avanza al inicio del siguiente comando
            inicio = p + 1;
        }
    }

    return command_count;
}

// Ejecuta los comandos conectados por pipes
void ejecutar(char *commands[], int count) {
    int pipes[MAX_COMMANDS][2];
    pid_t pids[MAX_COMMANDS];

    for (int i = 0; i < count; i++) {
        // Si no es el último comando, crea un pipe
        if (i < count - 1 && pipe(pipes[i]) < 0) {
            perror("pipe");
            exit(1);
        }

        pids[i] = fork();
        if (pids[i] == 0) { // Proceso hijo
            // Redirige entrada si no es el primer comando
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            // Redirige salida si no es el último comando
            if (i < count - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Cierra todas las pipes innecesarias
            for (int j = 0; j < count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Ejecuta el comando usando el shell
            execlp("sh", "sh", "-c", commands[i], (char *)NULL);
            perror("exec");
            exit(1);
        } else if (pids[i] < 0) {
            perror("fork");
            exit(1);
        }
    }

    // Cierra pipes en el proceso padre
    for (int i = 0; i < count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Espera a que terminen todos los hijos
    for (int i = 0; i < count; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

int main() {
    char command[1024];
    char *commands[MAX_COMMANDS];

    while (1) {
        // Muestra el prompt si es una terminal interactiva
        if (isatty(STDIN_FILENO)) {
            printf("Shell> ");
            fflush(stdout);
        }

        /* Reads a line of input from the user from the standard input (stdin) and stores it in the variable command */
        if (fgets(command, sizeof(command), stdin) == NULL) {
            if (isatty(STDIN_FILENO)) printf("\n");
            break;
        }

        /* Removes the newline character (\n) from the end of the string stored in command, if present. 
           This is done by replacing the newline character with the null character ('\0').
           The strcspn() function returns the length of the initial segment of command that consists of 
           characters not in the string specified in the second argument ("\n" in this case). */
        command[strcspn(command, "\n")] = '\0';

        // Si el usuario escribe "exit", se termina la shell
        if (strcmp(command, "exit") == 0) break;

        /* Tokenizes the command string using the pipe character (|) as a delimiter using the strtok() function. 
           Each resulting token is stored in the commands[] array. 
           The strtok() function breaks the command string into tokens (substrings) separated by the pipe character |. 
           In each iteration of the while loop, strtok() returns the next token found in command. 
           The tokens are stored in the commands[] array, and command_count is incremented to keep track of the number of tokens found. */
        int count = parsear_comandos(command, commands);
        if (count < 0) continue;

        // Ejecuta y libera memoria
        ejecutar(commands, count);
        liberar(commands, count);
    }

    return 0;
}
