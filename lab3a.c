/* NAME: Jesse Catalan,Ricardo Kuchimpos
 * EMAIL: jessecatalan77@gmail.com,rkuchimpos@gmail.com
 * ID: 204785152,704827423
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "ext2_fs.h"

#define SUPER_BLOCK_OFFSET 1024

/* We assume there is a single block group in the file system */

int get_offset(int block_id, int block_size) {
    /* NOTE: Blocks are indexed starting from 1.
     * The initial block is reserved for the boot block.
     */
    int offset = SUPER_BLOCK_OFFSET + (block_id - 1) * block_size;

    return offset;
}

/* Print summary of superblock:
 * SUPERBLOCK
 * total number of blocks (decimal)
 * total number of i-nodes (decimal)
 * block size (in bytes, decimal)
 * i-node size (in bytes, decimal)
 * blocks per group (decimal)
 * i-nodes per group (decimal)
 * first non-reserved i-node (decimal)
 *
 * Return 0 on success, -1 on error
 */
int print_superblock_summary(struct ext2_super_block *sb) {
    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        return -1;
    }

    int num_blocks = sb->s_blocks_count;
    int num_inodes = sb->s_inodes_count;
    int block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
    int inode_size = sb->s_inode_size;
    int blocks_per_group = sb->s_blocks_per_group;
    int inodes_per_group = sb->s_inodes_per_group;
    int first_nr_inode = sb->s_first_ino;

    printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n",
            num_blocks, num_inodes, block_size, inode_size,
            blocks_per_group, inodes_per_group, first_nr_inode);

    return 0;
}

/* Print summary of each group:
 * GROUP
 * group number (decimal, starting from zero)
 * total number of blocks in this group (decimal)
 * total number of i-nodes in this group (decimal)
 * number of free blocks (decimal)
 * number of free i-nodes (decimal)
 * block number of free block bitmap for this group (decimal)
 * block number of free i-node bitmap for this group (decimal)
 * block number of first block of i-nodes in this group (decimal)
 */
void print_group_summary(struct ext2_super_block *sb, struct ext2_group_desc *grp) {
   int group_num;
   int blocks_in_group;
   int inodes_in_group;
   int num_free_blocks;
   int num_free_inodes;
   int free_block_bitmap;
   int free_inode_bitmap;
   int first_inode_block;

   /* NOTE: Number of blocks/inodes on last block may not be the same
    * as the number in previous blocks for systems with multiple
    * cylinder groups
    */

   group_num = 0;
   blocks_in_group = sb->s_blocks_count;
   inodes_in_group = sb->s_inodes_count;
   num_free_blocks = grp->bg_free_blocks_count;
   num_free_inodes = grp->bg_free_inodes_count;
   free_block_bitmap = grp->bg_block_bitmap;
   free_inode_bitmap = grp->bg_inode_bitmap;
   first_inode_block = grp->bg_inode_table;

   printf("GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
           group_num, blocks_in_group, inodes_in_group,
           num_free_blocks, num_free_inodes, free_block_bitmap,
           free_inode_bitmap, first_inode_block);
}
/* Print free block entries:
 * BFREE
 * number of the free block (decimal)
 */
void print_free_block_entries(int img_fd, struct ext2_super_block *sb,
        struct ext2_group_desc *grp) {
    int block_bitmap;
    int block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;

    block_bitmap = grp->bg_block_bitmap;

    unsigned char *bitmap = malloc(block_size);
    int offset = get_offset(block_bitmap, block_size);
    pread(img_fd, bitmap, block_size, offset);

    for (int j = 0; j < block_size; j++) {
        for (int i = 0; i < 8; i++) {
            int is_allocated = bitmap[j] & (1 << i);
            if (!is_allocated) {
                int block_id = (j * 8) + (i + 1);
                printf("BFREE,%d\n", block_id);
            }
        }
    }

    free(bitmap);
}

/* Print free inode entries
 * IFREE
 * number of the free I-node (decimal)
 */
void print_free_inode_entries(int img_fd, struct ext2_super_block *sb,
        struct ext2_group_desc *grp) {
    int inode_bitmap;
    //int inodes_in_group;
    int block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;

    inode_bitmap = grp->bg_inode_bitmap;
    //inodes_in_group = sb->s_inodes_count;

    unsigned char *bitmap = malloc(block_size);
    int offset = get_offset(inode_bitmap, block_size);
    pread(img_fd, bitmap, block_size, offset);

    for (int j = 0; j < block_size; j++) {
        for (int i = 0; i < 8; i++) {
            int is_allocated = bitmap[j] & (1 << i);
            if (!is_allocated) {
                int inode_id = (j * 8) + (i + 1);
                printf("IFREE,%d\n", inode_id);
            }
        }
    }

    free(bitmap);
}


/* Print inode summary
 * INODE
 * inode number (decimal)
 * file type ('f' for file, 'd' for directory, 's' for symbolic link, '?" for anything else)
 * mode (low order 12-bits, octal ... suggested format "%o")
 * owner (decimal)
 * group (decimal)
 * link count (decimal)
 * time of last I-node change (mm/dd/yy hh:mm:ss, GMT)
 * modification time (mm/dd/yy hh:mm:ss, GMT)
 * time of last access (mm/dd/yy hh:mm:ss, GMT)
 * file size (decimal)
 * number of (512 byte) blocks of disk space (decimal) taken up by this file
 */
void print_inode_summary(int img_fd, struct ext2_super_block *sb,
        struct ext2_group_desc *grp) {
    int inode_table;
    struct ext2_inode table[sb->s_inodes_count];
    int block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
    int inodes_in_group;

    inode_table = grp->bg_inode_table;
    inodes_in_group = sb->s_inodes_count;

    int table_offset = get_offset(inode_table, block_size);
    pread(img_fd, table, sizeof(table), table_offset);

    for (int j = 0; j < inodes_in_group; j++) {
        if (table[j].i_mode && table[j].i_links_count) {
            int inode_id = j + 1;
            int file_type_mode = table[j].i_mode & 0xF000;
            char file_type = '?';
            switch (file_type_mode) {
                case 0x8000:
                    file_type = 'f';
                    break;
                case 0x4000:
                    file_type = 'd';
                    break;
                case 0xA000:
                    file_type = 's';
                    break;
                default:
                    file_type = '?';
                    break;
            }
            int mode = table[j].i_mode & 0x0FFF;
            int owner = table[j].i_uid;
            int group = table[j].i_gid;
            int link_count = table[j].i_links_count;

            struct tm *c_time_info;
            time_t c_time = table[j].i_ctime;
            char c_time_buf[64];
            c_time_info = gmtime(&c_time);
            strftime(c_time_buf, 64, "%x %X", c_time_info);

            struct tm *m_time_info;
            time_t m_time = table[j].i_mtime;
            char m_time_buf[64];
            m_time_info = gmtime(&m_time);
            strftime(m_time_buf, 64, "%x %X", m_time_info);

            struct tm *a_time_info;
            time_t a_time = table[j].i_atime;
            char a_time_buf[64];
            a_time_info = gmtime(&a_time);
            strftime(a_time_buf, 64, "%x %X", a_time_info);

            int file_size = table[j].i_size;
            int num_blocks = table[j].i_blocks;

            printf("INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d",
                    inode_id, file_type, mode, owner, group, link_count,
                    c_time_buf, m_time_buf, a_time_buf, file_size, num_blocks);

            if (file_type == 'f' || file_type == 'd' || file_type == 's') {
                if (file_type == 's' && file_size < 60) {
                    /* NOTE: This is value does not represent a valid block but the first
                     * four bytes of the file name the symlink points to. Print this to
                     * match the trivial.csv format.
                     */
                    printf(",%d", table[j].i_block[0]);
                }

                else {
                    for (int k = 0; k < 15; k++) {
                        int ptr = table[j].i_block[k];
                        printf(",%d", ptr);
                    }
                }
            }

            printf("\n");
        }
    }
}

/* Iterate through directory entries of a data block */
void scan_dir(int img_fd, int block_id, int block_size, int inode_id, int lbo) {
    char block[block_size];
    int offset = get_offset(block_id, block_size);
    pread(img_fd, block, block_size, offset);

    struct ext2_dir_entry *dirent = (struct ext2_dir_entry *) block;
    int size = 0;
    while (size < block_size) {
        if (dirent->inode != 0) {
            int name_len = dirent->name_len;
            char name[name_len + 1];
            memcpy(name, dirent->name, name_len);
            name[name_len] = '\0';

            printf("DIRENT,%d,%d,%d,%d,%d,'%s'\n",
                    inode_id, size + lbo * block_size, dirent->inode,
                    dirent->rec_len, name_len, name);
            size += dirent->rec_len;
        }

        dirent = (void *) dirent + dirent->rec_len;

        if (dirent->rec_len == 0) {
            break;
        }

    }

}

/* Visit indirect directory entries */
void visit_indirect_dirents(int level, int block_id, int block_size, int num_entries,
        int img_fd, int inode_id, int lbo) {
    char block[block_size];
    int offset = get_offset(block_id, block_size);
    pread(img_fd, block, block_size, offset);

    int *ptr = (int *) block;

    if (level == 1) {
        for (int i = 0; i < num_entries; i++) {
            scan_dir(img_fd, *ptr, block_size, inode_id, lbo + i);
            ptr++;
        }   
        return;
    }
    
    for (int i = 0; i < num_entries; i++) {
        visit_indirect_dirents(level - 1, *ptr, block_size, num_entries,
                img_fd, inode_id, lbo);
        lbo += num_entries;
        ptr++;
    }
}

/* Print directory entry summary:
 * DIRENT
 * parent inode number (decimal) ... the I-node number of the directory that contains this entry
 * logical byte offset (decimal) of this entry within the directory
 * inode number of the referenced file (decimal)
 * entry length (decimal)
 * name length (decimal)
 * name (string, surrounded by single-quotes). Don't worry about escaping, we promise there will be no single-quotes or commas in any of the file names.
 */
void print_dir_entries(int img_fd, struct ext2_super_block *sb,
        struct ext2_group_desc *grp) {
    int inodes_in_group;
    int inode_table;
    struct ext2_inode table[sb->s_inodes_count];
    int block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;

    /* Number of 32-bit block pointers in a block */
    /* NOTE: block size is in bytes */
    int num_entries = (block_size * 8) / 32;

    inode_table = grp->bg_inode_table;
    inodes_in_group = sb->s_inodes_count;

    int table_offset = get_offset(inode_table, block_size);
    pread(img_fd, table, sizeof(table), table_offset);

    for (int j = 0; j < inodes_in_group; j++) {
        int inode_id = j + 1; 
        struct ext2_inode *inode_entry = &table[j];
        if (S_ISDIR(inode_entry->i_mode)) {
            int lbo = 0;
            for (int k = 0; k < 15; k++) {
                int ptr = inode_entry->i_block[k];

                if (ptr == 0) {
                    break;
                }

                if (k < 12) {
                    scan_dir(img_fd, ptr, block_size, inode_id, lbo);
                    lbo++;
                } if (k == 12) {
                    visit_indirect_dirents(1, ptr, block_size, num_entries,
                            img_fd, inode_id, lbo);
                    lbo += num_entries;
                } else if (k == 13) {
                    visit_indirect_dirents(2, ptr, block_size, num_entries,
                            img_fd, inode_id, lbo);
                    lbo += num_entries * num_entries;
                } else if (k == 14) {
                    visit_indirect_dirents(3, ptr, block_size, num_entries,
                            img_fd, inode_id, lbo);
                    lbo += num_entries * num_entries * num_entries;
                }
            }
        }
    }
}

/* Visit indirect blocks */
void visit_indirect_refs(int level_current, int block_id,
            int block_size, int num_entries, int img_fd,
            int inode_id, int lbo) {
    char block[block_size];
    int offset = get_offset(block_id, block_size);
    pread(img_fd, block, block_size, offset);

    int *ptr = (int *) block;


    if (level_current == 1) {
        for (int i = 0; i < num_entries; i++) {
            if (*ptr) {
                printf("INDIRECT,%d,%d,%d,%d,%d\n",
                        inode_id, level_current, lbo + i,
                        block_id, *ptr);
            }
            ptr++;
        }   
        return;
    }
    
    for (int i = 0; i < num_entries; i++) {
        if (*ptr == 0) {
            return;
        }
        printf("INDIRECT,%d,%d,%d,%d,%d\n",
                inode_id, level_current, lbo,
                block_id, *ptr);
        visit_indirect_refs(level_current - 1, *ptr, block_size,
                num_entries, img_fd, inode_id, lbo);
        lbo += num_entries;
        ptr++;
    }
}

/* Print indirect block references:
 * INDIRECT
 * I-node number of the owning file (decimal)
 * (decimal) level of indirection for the block being scanned ... 1 for single indirect, 2 for double indirect, 3 for triple
 * logical block offset (decimal) represented by the referenced block. If the referenced block is a data block, this is the logical block offset of that block within the file. If the referenced block is a single- or double-indirect block, this is the same as the logical offset of the first data block to which it refers.
 * block number of the (1, 2, 3) indirect block being scanned (decimal) . . . not the highest level block (in the recursive scan), but the lower level block that contains the block reference reported by this entry.
 * block number of the referenced block (decimal)
 */
void print_indirect_block_refs(int img_fd, struct ext2_super_block *sb,
        struct ext2_group_desc *grp) {
    int inodes_in_group;
    int inode_table;
    struct ext2_inode table[sb->s_inodes_per_group];
    int block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;

    /* Number of 32-bit block pointers in a block */
    /* NOTE: block size is in bytes */
    int num_entries = (block_size * 8) / 32;

    inode_table = grp->bg_inode_table;
    inodes_in_group = sb->s_inodes_per_group;

    int table_offset = get_offset(inode_table, block_size);
    pread(img_fd, table, sizeof(table), table_offset);

    for (int j = 0; j < inodes_in_group; j++) {
        int inode_id = j + 1;
        struct ext2_inode *inode_entry = &table[j];
        int lbo = 12;
        if (S_ISDIR(inode_entry->i_mode) || S_ISREG(inode_entry->i_mode)) {
            /* Scan indirect blocks */ 
            visit_indirect_refs(1, inode_entry->i_block[12], block_size,
                    num_entries, img_fd, inode_id, lbo);
            lbo += num_entries;

            /* Scan double indirect blocks */
            visit_indirect_refs(2, inode_entry->i_block[13], block_size,
                    num_entries, img_fd, inode_id, lbo);
            lbo += num_entries * num_entries;

            /* Scan triple indirect blocks */
            visit_indirect_refs(3, inode_entry->i_block[14], block_size,
                    num_entries, img_fd, inode_id, lbo);
            lbo += num_entries * num_entries * num_entries;
        }
    }
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid invocation!\nUsage: ./lab3a [image]\n");
        exit(1);
    }

    char *img_name = argv[1];
    int img_fd = open(img_name, O_RDONLY);
    if (img_fd == -1) {
        fprintf(stderr, "%s is a nonexistent file!\n", img_name);
        exit(1);
    }

    struct ext2_super_block sb;
    pread(img_fd, &sb, sizeof(struct ext2_super_block), SUPER_BLOCK_OFFSET);

    int block_size = EXT2_MIN_BLOCK_SIZE  << sb.s_log_block_size;

    struct ext2_group_desc grp;
    int grp_desc_table_offset = get_offset(2, block_size);
    pread(img_fd, &grp, sizeof(grp), grp_desc_table_offset);

    int rc = print_superblock_summary(&sb);
    if (rc == -1) {
        fprintf(stderr, "Corrupted file system!\n");
        exit(2);
    }
    
    print_group_summary(&sb, &grp);

    print_free_block_entries(img_fd, &sb, &grp);

    print_free_inode_entries(img_fd, &sb, &grp);

    print_inode_summary(img_fd, &sb, &grp);

    print_dir_entries(img_fd, &sb, &grp);

    print_indirect_block_refs(img_fd, &sb, &grp);

    exit(0);
}
