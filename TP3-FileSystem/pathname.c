#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int pathname_lookup(struct unixfilesystem *fs, const char *pathname) {
    // Verificamos que sea path absoluto
    if (pathname[0] != '/') return -1;

    int inumber = ROOT_INUMBER; // raíz = inodo 1
    const char *curr = pathname + 1; // salteamos el '/'

    char component[15]; // espacio para nombre (14 chars + null terminator)
    while (*curr != '\0') {
        // Extraer siguiente componente del path
        int len = 0;
        while (*curr != '/' && *curr != '\0' && len < 14) {
            component[len++] = *curr++;
        }
        component[len] = '\0';

        // Saltear slashes repetidos (aunque en este TP no hace falta)
        while (*curr == '/') curr++;

        // Buscar el componente dentro del directorio actual
        struct direntv6 dirEnt;
        if (directory_findname(fs, component, inumber, &dirEnt) < 0) {
            return -1;
        }

        // Avanzar al inodo encontrado
        inumber = dirEnt.d_inumber;
    }

    return inumber;
}
