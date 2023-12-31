#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ext2.h"


// Pointer to the beginning of the disk (byte 0)
static const unsigned char *disk = NULL;

int in_use(unsigned char *bitmap, int byte, int bit){
    int in_use = (bitmap[byte] & (1 << bit));
    if (in_use) {
        return 1;
    } else {
        return 0;
    }
}


void print_inode(struct ext2_inode* inode_table, int index) {

    struct ext2_inode *inode = &inode_table[index - 1];
    printf("[%d]", index);
    if (inode->i_mode & EXT2_S_IFREG) {
        printf(" type: f");
    } else {
        printf(" type: d");
    }
    printf(" size: %d", inode->i_size);
    printf(" links: %d", inode->i_links_count);
    printf(" blocks: %d\n", inode->i_blocks);

    printf("[%d] Blocks:", index);
    for (int i = 0 ; i < ((inode->i_blocks / 2) & 13) ; i++) {
        printf(" %d", inode->i_block[i]);
    }
    printf("\n");
}


int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
		exit(1);
	}
	int fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	// Map the disk image into memory so that we don't have to do any explicit reads
	disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	if (disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	const struct ext2_super_block *sb = (const struct ext2_super_block*)(disk + EXT2_BLOCK_SIZE);
	printf("Inodes: %d\n", sb->s_inodes_count);
	printf("Blocks: %d\n", sb->s_blocks_count);

    const struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + (EXT2_BLOCK_SIZE * 2));
    printf("Block group:\n");
    printf("    block bitmap: %d\n", gd->bg_block_bitmap);
    printf("    inode bitmap: %d\n", gd->bg_inode_bitmap);
    printf("    inode table: %d\n", gd->bg_inode_table);
    printf("    free blocks: %d\n", gd->bg_free_blocks_count);
    printf("    free inodes: %d\n", gd->bg_free_inodes_count);
    printf("    used_dirs: %d\n", gd->bg_used_dirs_count);

    unsigned char *block_bitmap = (unsigned char *)(disk + (EXT2_BLOCK_SIZE * gd->bg_block_bitmap));
    printf("Block bitmap: ");
    for (int byte = 0 ; byte < sb->s_blocks_count / 8 ; byte++) {
        for (int bit = 0 ; bit < 8 ; bit++) {
            if (in_use(block_bitmap, byte, bit)) {
                printf("1");
            } else {
                printf("0");
            }
        }
        printf(" ");
    }
    printf("\n");

    unsigned char *inode_bitmap = (unsigned char *)(disk + (EXT2_BLOCK_SIZE * gd->bg_inode_bitmap));
    printf("Inode bitmap: ");
    for (int byte = 0 ; byte < sb->s_inodes_count / 8 ; byte++) {
        for (int bit = 0 ; bit < 8 ; bit++) {
            if (in_use(inode_bitmap, byte, bit)) {
                printf("1");
            } else {
                printf("0");
            }
        }
        printf(" ");
    }
    printf("\n\n");

    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    printf("Inodes:\n");
    print_inode(inode_table, EXT2_ROOT_INO);

    for (int i = EXT2_GOOD_OLD_FIRST_INO ; i < sb->s_inodes_count ; i++) {
        int byte = i / 8;
        int bit = i - (8 * byte);

        if (in_use(inode_bitmap, byte, bit)) {
            print_inode(inode_table, i + 1);
        }
    }

	return 0;
}
