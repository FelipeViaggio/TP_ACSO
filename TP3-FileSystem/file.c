#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "file.h"
#include "inode.h"
#include "diskimg.h"

/**
 * TODO
 */
int file_getblock(struct unixfilesystem *fs, int inumber, int blockNum, void *buf) {
    // Cargar el inodo
    struct inode in;
    if (inode_iget(fs, inumber, &in) < 0) {
        return -1;
    }

    // Obtener número de bloque físico
    int physical_block = inode_indexlookup(fs, &in, blockNum);
    if (physical_block < 0) {
        fprintf(stderr, "DEBUG: inumber %d, blockNum %d → physical_block = %d (invalid)\n",
                inumber, blockNum, physical_block);
        return -1;
    }

    // Leer el bloque físico del disco
    int bytes_read = diskimg_readsector(fs->dfd, physical_block, buf);
    if (bytes_read != DISKIMG_SECTOR_SIZE) {
        fprintf(stderr, "DEBUG: Failed to read physical block %d for inumber %d\n",
                physical_block, inumber);
        return -1;
    }
    

    // Calcular cuántos bytes son válidos (puede ser < 512 en el último bloque)
    int filesize = inode_getsize(&in);
    int block_start = blockNum * DISKIMG_SECTOR_SIZE;

    if (block_start >= filesize) {
        return 0; // este bloque está más allá del fin del archivo
    }

    int remaining = filesize - block_start;
    return (remaining < DISKIMG_SECTOR_SIZE) ? remaining : DISKIMG_SECTOR_SIZE;
}


