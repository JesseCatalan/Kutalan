#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include "ext2_fs.h"

#define SUPER_BLOCK_OFFSET 1024

int get_offset(int block_id, int block_size) {
    /* NOTE: Blocks are indexed starting from 1.
     * The initial block is reserved for the boot block.
     */
    int offset = SUPER_BLOCK_OFFSET + (block_id - 1) * block_size;

    return offset;
}

/* Convert ctime time format into desired format:
 * Www Mmm dd hh:mm:ss yyyy\n [ctime format]
 * mm/dd/yy hh:mm:ss [desired format]
 */
char *set_time(char *time) {
    /* NOTE: Each string is size+1 to allow for the null byte */
    int month;
    char month_s[4];
    char day_s[3];
    char year_s[3];
    char hour_s[3];
    char minute_s[3];
    char second_s[3];

    snprintf(month_s, 4, "%s", &time[4]);
    snprintf(day_s, 3, "%s", &time[8]);
    snprintf(year_s, 3, "%s", &time[22]);
    snprintf(hour_s, 3, "%s", &time[11]);
    snprintf(minute_s, 3, "%s", &time[14]);
    snprintf(second_s, 3, "%s", &time[17]);

    if (strcmp(month_s, "Jan") == 0) {
        month = 1;
    } else if (strcmp(month_s, "Feb") == 0) {
        month = 2;
    } else if (strcmp(month_s, "Mar") == 0) {
        month = 3;
    } else if (strcmp(month_s, "Apr") == 0) {
        month = 4;
    } else if (strcmp(month_s, "May") == 0) {
        month = 5;
    } else if (strcmp(month_s, "Jun") == 0) {
        month = 6;
    } else if (strcmp(month_s, "Jul") == 0) {
        month = 7;
    } else if (strcmp(month_s, "Aug") == 0) {
        month = 8;
    } else if (strcmp(month_s, "Sep") == 0) {
        month = 9;
    } else if (strcmp(month_s, "Oct") == 0) {
        month = 10;
    } else if (strcmp(month_s, "Nov") == 0) {
        month = 11;
    } else if (strcmp(month_s, "Dec") == 0) {
        month = 12;
    }

    char *return_string = malloc(sizeof(char) * 18);
    snprintf(return_string, 18, "%02d/%02d/%02d %02d:%02d:%02d", month, atoi(day_s), atoi(year_s), atoi(hour_s),
            atoi(minute_s), atoi(second_s));

    return return_string;
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
void print_group_summary(struct ext2_super_block *sb, struct ext2_group_desc grps[], int group_count) {
   int group_num;
   int blocks_in_group;
   int inodes_in_group;
   int num_free_blocks;
   int num_free_inodes;
   int free_block_bitmap;
   int free_inode_bitmap;
   int first_inode_block;

   for (int i = 0; i < group_count; i++) {
       group_num = i;
       if (i != group_count - 1) {
           blocks_in_group = sb->s_blocks_per_group;
           inodes_in_group = sb->s_inodes_per_group;
       } else {
           blocks_in_group = sb->s_blocks_count % sb->s_blocks_per_group;
           inodes_in_group = sb->s_inodes_count % sb->s_inodes_per_group;
       }
       num_free_blocks = grps[i].bg_free_blocks_count;
       num_free_inodes = grps[i].bg_free_inodes_count;
       free_block_bitmap = grps[i].bg_block_bitmap;
       free_inode_bitmap = grps[i].bg_inode_bitmap;
       first_inode_block = grps[i].bg_inode_table;

       printf("GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
               group_num, blocks_in_group, inodes_in_group,
               num_free_blocks, num_free_inodes, free_block_bitmap,
               free_inode_bitmap, first_inode_block);
   }
}
/* Print free block entries:
 * BFREE
 * number of the free block (decimal)
 */
void print_free_block_entries(int img_fd, struct ext2_super_block *sb, struct ext2_group_desc grps[], int group_count) {
    int block_bitmap;
    int blocks_in_group;
    int block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;

    for (int i = 0; i < group_count; i++) {
        block_bitmap = grps[i].bg_block_bitmap;
        if (i != group_count - 1) {
            blocks_in_group = sb->s_blocks_per_group;
        } else {
            blocks_in_group = sb->s_blocks_count % sb->s_blocks_per_group;
        }

        int *bitmap = malloc(block_size);
        int offset = get_offset(block_bitmap, block_size);
        pread(img_fd, bitmap, block_size, offset);

        for (int j = 0; j < blocks_in_group; j++) {
            int is_allocated = *bitmap & (1 << j);
            if (!is_allocated) {
                int block_id = (i * sb->s_blocks_per_group) + j;
                printf("BFREE,%d\n", block_id);
            }
        }

        free(bitmap);
    }
}

/* Print free inode entries
 * IFREE
 * number of the free I-node (decimal)
 */
void print_free_inode_entries(int img_fd, struct ext2_super_block *sb, struct ext2_group_desc grps[], int group_count) {
    int inode_bitmap;
    int inodes_in_group;
    int block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;

    for (int i = 0; i < group_count; i++) {
        inode_bitmap = grps[i].bg_inode_bitmap;
        if (i != group_count - 1) {
            inodes_in_group = sb->s_inodes_per_group;
        } else {
            inodes_in_group = sb->s_inodes_count % sb->s_inodes_per_group;
        }

        int *bitmap = malloc(block_size);
        int offset = get_offset(inode_bitmap, block_size);
        pread(img_fd, bitmap, block_size, offset);

        for (int j = 0; j < inodes_in_group; j++) {
            int is_allocated = *bitmap & (1 << j);
            if (!is_allocated) {
                int inode_id = (i * sb->s_inodes_per_group) + j;
                printf("IFREE,%d\n", inode_id);
            }
        }

        free(bitmap);
    }
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
void print_inode_summary(int img_fd, struct ext2_super_block *sb, struct ext2_group_desc grps[], int group_count) {
    int inode_bitmap;
    int inodes_per_group;
    int inode_table;
    struct ext2_inode table[sb->s_inodes_per_group];
    int block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
    int inodes_in_group;

    for (int i = 0; i < group_count; i++) {
        inode_bitmap = grps[i].bg_inode_bitmap;
        inode_table = grps[i].bg_inode_table;
        if (i != group_count -1 ) {
            inodes_in_group = sb->s_inodes_per_group;
        } else {
            inodes_in_group = sb->s_inodes_count % sb->s_inodes_per_group;
        }

        int *bitmap = malloc(block_size);
        int bitmap_offset = get_offset(inode_bitmap, block_size);
        int table_offset = get_offset(inode_table, block_size);
        pread(img_fd, bitmap, block_size, bitmap_offset);
        pread(img_fd, table, sizeof(table), table_offset);

        for (int j = 0; j < inodes_in_group; j++) {
            if (table[j].i_mode && table[j].i_links_count) {
                int inode_id = (i * sb->s_inodes_per_group) + j;
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
                time_t c_time = table[j].i_ctime;
                char *c_time_string = ctime(&c_time);
                char *fmtd_c_time = set_time(c_time_string);
                time_t m_time = table[j].i_mtime;
                char *m_time_string = ctime(&m_time);
                char *fmtd_m_time = set_time(m_time_string);
                time_t a_time = table[j].i_atime;
                char *a_time = ctime(&a_time);
                char *fmtd_a_time = set_time(a_time_string);
                int file_size = table[j].i_size;
                int num_blocks = table[j].i_blocks;

                printf("INODE,%d,%d,%o,%d,%d,%d,%s,%s,%s", inode_id, file_type, mode, owner, group, link_count,
                        fmtd_c_time, fmtd_m_time, fmtd_a_time);

                if (file_type == 'f' || file_type == 'd' || file_type == 's') {
                    if (file_type == 's') {
                        if (file_size < 60) {
                            break;
                        }
                    }

                    for (int k = 0; k < num_blocks; k++) {
                        printf(","); // print address if content != 0
                    }
                }

                free(fmtd_c_time);
                free(fmtd_m_time);
                free(fmtd_a_time);
            }
        }

        free(bitmap);
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
        exit(3);
    }

    struct ext2_super_block super_block;
    pread(img_fd, &super_block, sizeof(struct ext2_super_block), SUPER_BLOCK_OFFSET);

    /* Call print_superblock_summary */

    int block_size = super_block.s_log_block_size;
    double group_count = ceil((double) super_block.s_blocks_count / (double) super_block.s_blocks_per_group);

    struct ext2_group_desc grps[(int) group_count];
    int grp_desc_table_offset = get_offset(2, block_size);
    pread(img_fd, grps, sizeof(grps), grp_desc_table_offset);

    /* Call print_group_summary */

    
}
