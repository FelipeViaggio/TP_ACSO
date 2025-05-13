#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int directory_findname(struct unixfilesystem *fs, const char *name,
                       int dirinumber, struct direntv6 *dirEnt) {
    // Cargar el inodo del directorio
    struct inode dir_inode;
    if (inode_iget(fs, dirinumber, &dir_inode) < 0) {
        return -1;
    }

    // Verificar que sea un directorio
    if ((dir_inode.i_mode & IFMT) != IFDIR) {
        return -1;
    }

    // Calcular el tamaño total del directorio en bytes
    int size = inode_getsize(&dir_inode);

    // Asegurar que el tamaño sea múltiplo de una entrada de directorio
    if (size % sizeof(struct direntv6) != 0) {
        return -1;
    }

    // Recorrer los bloques del directorio
    int num_blocks = (size + DISKIMG_SECTOR_SIZE - 1) / DISKIMG_SECTOR_SIZE;
    for (int bno = 0; bno < num_blocks; bno++) {
        struct direntv6 entries[DISKIMG_SECTOR_SIZE / sizeof(struct direntv6)];
        int bytes_read = file_getblock(fs, dirinumber, bno, entries);
        if (bytes_read < 0) return -1;

        int num_entries = bytes_read / sizeof(struct direntv6);
        for (int i = 0; i < num_entries; i++) {
            if (strncmp(entries[i].d_name, name, 14) == 0) {
                *dirEnt = entries[i];
                return 0;
            }
        }
    }

    // Si no se encontró
    return -1;
}
