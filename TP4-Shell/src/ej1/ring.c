#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{	
	int start, status, pid, n;
	int buffer[1];

	if (argc != 4){ 
        printf("Uso: anillo <n> <c> <s> \n"); 
        exit(0);
    }
    
    /* Parsing of arguments */
  	n = atoi(argv[1]);
    buffer[0] = atoi(argv[2]);
    start = atoi(argv[3]);

    // Validación de argumentos
    if (n < 2 || start < 0 || start >= n) {
        fprintf(stderr, "Error: argumentos fuera de rango.\n");
        return 1;
    }

    printf("Se crearán %i procesos, se enviará el caracter %i desde proceso %i \n", n, buffer[0], start);
    
   	/* You should start programming from here... */

    // Creamos los pipes para formar el anillo entre los procesos hijos
    int pipes[n][2];
    for (int i = 0; i < n; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("Falla al crear pipe");
            exit(1);
        }
    }

    // Pipes auxiliares:
    // - pipe_to_start: el padre envía el valor inicial al proceso 'start'
    // - pipe_to_parent: el proceso anterior a 'start' envía el valor final al padre
    int pipe_to_start[2], pipe_to_parent[2];
    if (pipe(pipe_to_start) == -1 || pipe(pipe_to_parent) == -1) {
        perror("Falla al crear pipes auxiliares");
        exit(1);
    }

    // Creamos n procesos hijos
    for (int i = 0; i < n; i++) {
        pid = fork();
        if (pid < 0) {
            perror("Error en fork");
            exit(1);
        }

        if (pid == 0) {
            // En el hijo: cerramos todos los extremos que no vamos a usar
            for (int j = 0; j < n; j++) {
                if (j != i) close(pipes[j][0]);             // solo leo del anterior
                if (j != (i + 1) % n) close(pipes[j][1]);   // solo escribo al siguiente
            }

            if (i != start) {
                close(pipe_to_start[0]);
                close(pipe_to_start[1]);
            }

            if (i != (start - 1 + n) % n) {
                close(pipe_to_parent[0]);
                close(pipe_to_parent[1]);
            }

            int tmp;

            // Proceso que inicia: lee el valor enviado por el padre
            if (i == start) {
                close(pipe_to_start[1]);
                if (read(pipe_to_start[0], &tmp, sizeof(int)) != sizeof(int)) {
                    perror("Lectura inicial fallida");
                    exit(1);
                }
                close(pipe_to_start[0]);
            } 
            // Resto de procesos: leen del proceso anterior en el anillo
            else {
                if (read(pipes[i][0], &tmp, sizeof(int)) != sizeof(int)) {
                    perror("Lectura del pipe anterior fallida");
                    exit(1);
                }
                close(pipes[i][0]);
            }

            tmp++; // Incrementamos el valor recibido

            // El proceso anterior al que comenzó devuelve el valor final al padre
            if (i == (start - 1 + n) % n) {
                close(pipes[(i + 1) % n][1]); // cerramos el pipe de salida que no vamos a usar
                if (write(pipe_to_parent[1], &tmp, sizeof(int)) != sizeof(int)) {
                    perror("Escritura al padre fallida");
                    exit(1);
                }
                close(pipe_to_parent[1]);
            } 
            // Resto de procesos: envían al siguiente en el anillo
            else {
                if (write(pipes[(i + 1) % n][1], &tmp, sizeof(int)) != sizeof(int)) {
                    perror("Escritura al siguiente proceso fallida");
                    exit(1);
                }
                close(pipes[(i + 1) % n][1]);
            }

            exit(0);
        }
    }

    // Código del proceso padre
    // Cierra todos los pipes del anillo (no los necesita)
    for (int i = 0; i < n; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Cierra extremos no utilizados de los pipes auxiliares
    close(pipe_to_start[0]);
    close(pipe_to_parent[1]);

    // El padre envía el valor inicial al proceso 'start'
    if (write(pipe_to_start[1], &buffer[0], sizeof(int)) != sizeof(int)) {
        perror("Fallo al enviar valor inicial");
        exit(1);
    }
    close(pipe_to_start[1]);

    // Espera el resultado final del anillo
    if (read(pipe_to_parent[0], &buffer[0], sizeof(int)) != sizeof(int)) {
        perror("Fallo al recibir resultado final");
        exit(1);
    }
    close(pipe_to_parent[0]);

    // Muestra el resultado final
    printf("El padre recibió el resultado final: %d\n", buffer[0]);

    // Espera que terminen todos los hijos
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }

    return 0;
}
