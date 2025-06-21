#include "fs.h"
#include <stdbool.h>
// Find an inode by filename
int find_inode ( const char * filename ) ;

// Find a free inode
int find_free_inode () ;

// Find a free block
int find_free_block () ;

// Mark a block as used
void mark_block_used ( int block_num ) ;

// Mark a block as free
void mark_block_free ( int block_num ) ;

// Read an inode from disk
void read_inode ( int inode_num , inode * target ) ;

// Write an inode to disk
void write_inode ( int inode_num , const inode * source ) ;


int disk_fd;
bool is_mounted = true; // Flag to check if the filesystem is mounted;


/**
 * @brief Creates and formats a new filesystem
 * 
 * This function creates a new disk image file and initializes the filesystem
 * structures within it (superblock, block bitmap, and inode table).
 * 
 * Disk layout:
 * - Block 0: Superblock (4KB)
 * - Block 1: Block bitmap (4KB)
 * - Blocks 2-9: Inode table (32KB = 256 inodes × 128B)
 * - Blocks 10-2559: Data blocks (~9.96MB)
 * 
 * @param disk_path Path where the disk image file will be created
 * @return 0 on success, -1 on error (e.g., cannot create file)
 */
int fs_format(const char* disk_path){
    // Opening the virtual disk
    int disk_fd = open ( disk_path , O_RDWR | O_CREAT , 0644) ;
    if (!disk_fd) {
        return -1; // Error opening file
    }

    // ==============================================================================
    // Initialize superblock
    superblock sb;
    sb.total_blocks = MAX_BLOCKS;
    sb.block_size = BLOCK_SIZE;
    sb.free_blocks = MAX_BLOCKS - 1 - 1 - 8; // 10 blocks reserved for metadata
    sb.total_inodes = MAX_FILES;
    sb.free_inodes = MAX_FILES; // Initially all inodes are free

    // Writing superblock 
    lseek ( disk_fd , 0 * BLOCK_SIZE , SEEK_SET ) ; // Move to the start of the disk
    write ( disk_fd , &sb , sizeof(superblock) ) ; // Write the superblock to block 0

    // ==============================================================================

    // Initialize block bitmap
    unsigned char bitmap [ MAX_BLOCKS / 8];

    bitmap[0] |= (1 << 0); // Mark block 0 as used (superblock)
    bitmap[1] |= (1 << 0); // Mark block 1 as used (block bitmap)
    for (int i = 2; i < 10; i++) {
        bitmap[i / 8] |= (1 << (i % 8)); // Mark blocks 2-9 as used (inode table)
    }
    for (int i = 10; i < MAX_BLOCKS / 8; i++) {
        bitmap [i/8] &= ~(1 << ( i %8) ); // Set all blocks as free (1)
    }

    // Writing block bitmap
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move to block 1
    write(disk_fd, bitmap, sizeof(bitmap)); // Write the bitmap to block 1

    // ==============================================================================

    // Initialize inodes
    inode inodes[MAX_FILES];

    for (int i = 0; i < MAX_FILES; i++) {
        inodes[i].used = 0; // Mark all inodes as free
        inodes[i].size = 0; // Initial size is 0
        inodes[i].name[0] = '\0'; // Initialize name to empty
        for (int j = 0; j < MAX_DIRECT_BLOCKS; j++) {
            inodes[i].blocks[j] = -1; // Initialize block pointers to -1
        }
    }
    bitmap[2] |= (1 << 0); // Mark block 2 as used (inode table)
    bitmap[3] |= (1 << 0); // Mark block 3 as used (inode table)

     // Writing inode table
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to block 2 (start of inode table)
    write(disk_fd, inodes, sizeof(inodes)); // Write the inode table to blocks 2-9

    // ==============================================================================

    close(disk_fd);
    return 0; // Success
}


/**
 * @brief Mounts an existing filesystem
 * 
 * Opens the disk image file and reads the filesystem metadata into memory,
 * preparing it for use. This function should verify that the disk image
 * contains a valid filesystem structure.
 * 
 * @param disk_path Path to the disk image file to mount
 * @return 0 on success, -1 on error (e.g., file not found or invalid filesystem)
 */
int fs_mount(const char* disk_path){
    // Open the disk image file
    disk_fd = open(disk_path, O_RDWR);
    if (disk_fd < 0) {
        return -1; // Error opening file
    }

    // Read the superblock
    superblock sb;
    lseek(disk_fd, 0 * BLOCK_SIZE, SEEK_SET); // Move to block 0 (superblock)
    read(disk_fd, &sb, sizeof(superblock)); // Read the superblock

    // Read inode table

    inode inodes[MAX_FILES];
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to block 2 (start of inode table)
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table
    // Check if the inode table is valid
    for (int i = 0; i < MAX_FILES; i++) {
        if (inodes[i].used && inodes[i].size < 0) {
            close(disk_fd);
            return -1; // Invalid inode found
        }
    }
    // Read block bitmap
    unsigned char bitmap[MAX_BLOCKS / 8];
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move to block 1 (block bitmap)
    read(disk_fd, bitmap, sizeof(bitmap)); // Read the block bitmap
    // Check if the block bitmap is valid
    for (int i = 0; i < MAX_BLOCKS / 8; i++) {
        if (bitmap[i] < 0 || bitmap[i] > 255) {
            close(disk_fd);
            return -1; // Invalid block bitmap found
        }
    }
    // Check if the block bitmap is correctly initialized
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (i < 10 && (bitmap[i / 8] & (1 << (i % 8)))) {
            close(disk_fd);
            return -1; // Reserved blocks should be marked as used
        }
        if (i >= 10 && !(bitmap[i / 8] & (1 << (i % 8)))) {
            close(disk_fd);
            return -1; // Data blocks should be marked as free
        }
    }
    // Check if the superblock is valid
    if (sb.total_blocks != MAX_BLOCKS || sb.block_size != BLOCK_SIZE || sb.total_inodes != MAX_FILES) {
        close(disk_fd);
        return -1; // Invalid filesystem structure
    }
    is_mounted = true; // Set the mounted flag to true
}


/**
 * @brief Unmounts the filesystem
 * 
 * Ensures all pending changes are written to the disk image file and
 * closes the file. After unmounting, no further filesystem operations
 * should be performed until the filesystem is mounted again.
 */
void fs_unmount()
{
    // Close the disk image file
    // TODO change the disk image file name to a constant
    // int disk_fd = open("disk.img", O_RDWR);
    // if (disk_fd < 0) {
    //     return; // Error opening file
    // }
    is_mounted = false; // Set the mounted flag to false
    close(disk_fd); // Close the file
}














// ==============================================================================

// Sample Helper Functions

// ==============================================================================

// Find an inode by filename
int find_inode ( const char * filename ) {
    // Open the disk image file
    // TODO change the disk image file name to a constant
    int disk_fd = open("disk.img", O_RDWR);
    if (disk_fd < 0) {
        return -1; // Error opening file
    }

    inode inodes[MAX_FILES];
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to the start of the inode table
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table

    // Search for the inode with the given filename
    for (int i = 0; i < MAX_FILES; i++) {
        if (inodes[i].used && strcmp(inodes[i].name, filename) == 0) {
            close(disk_fd);
            return i; // Found the inode
        }
    }

    close(disk_fd);
    return -1; // Inode not found
}

// Find a free inode
int find_free_inode () {
    // Open the disk image file
    // TODO change the disk image file name to a constant
    int disk_fd = open("disk.img", O_RDWR);
    if (disk_fd < 0) {
        return -3; // Error opening file
    }

    inode inodes[MAX_FILES];
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to the start of the inode table
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table

    // Search for a free inode
    for (int i = 0; i < MAX_FILES; i++) {
        if (!inodes[i].used) {
            close(disk_fd);
            return i; // Found a free inode
        }
    }

    close(disk_fd);
    return -2; // No free inode found
}

// Find a free block
int find_free_block () {
    // Open the disk image file
    int disk_fd = open("disk.img", O_RDWR);
    if (disk_fd < 0) {
        return -1; // Error opening file
    }

    unsigned char bitmap[MAX_BLOCKS / 8];
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move to the block bitmap
    read(disk_fd, bitmap, sizeof(bitmap)); // Read the block bitmap

    // Search for a free block
    // Skip the first 4 blocks (0-3) as they are reserved for superblock and metadata
    // TODO: Check if the bitmap is correctly initialized
    for (int i = 4; i < MAX_BLOCKS; i++) {
        if ((bitmap[i / 8] & (1 << (i % 8)))) {
            close(disk_fd);
            return i; // Found a free block
        }
    }

    close(disk_fd);
    return -1; // No free block found

}

// Mark a block as used
void mark_block_used ( int block_num ) {
    // Open the disk image file
    // TODO change the disk image file name to a constant
    int disk_fd = open("disk.img", O_RDWR);
    if (disk_fd < 0) {
        return -3; // Error opening file
    }
    // Ensure block_num is within valid range
    if (block_num < 0 || block_num >= MAX_BLOCKS) {
        close(disk_fd);
        return -3; // Invalid block number
    }
    unsigned char bitmap[MAX_BLOCKS / 8];
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move to the block bitmap
    read(disk_fd, bitmap, sizeof(bitmap)); // Read the block bitmap

    // Mark the specified block as used
    bitmap[block_num / 8] |= (1 << (block_num % 8));

    // Write the updated bitmap back to disk
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move back to the block bitmap
    write(disk_fd, bitmap, sizeof(bitmap)); // Write the updated bitmap

    close(disk_fd);
}

// Mark a block as free
void mark_block_free ( int block_num ) {
    // Open the disk image file
    // TODO change the disk image file name to a constant
    int disk_fd = open("disk.img", O_RDWR);
    if (disk_fd < 0) {
        return -3; // Error opening file
    }
    // Ensure block_num is within valid range
    if (block_num < 0 || block_num >= MAX_BLOCKS) {
        close(disk_fd);
        return -3; // Invalid block number
    }
    unsigned char bitmap[MAX_BLOCKS / 8];
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move to the block bitmap
    read(disk_fd, bitmap, sizeof(bitmap)); // Read the block bitmap

    // Mark the specified block as free
    bitmap[block_num / 8] &= ~(1 << (block_num % 8));

    // Write the updated bitmap back to disk
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move back to the block bitmap
    write(disk_fd, bitmap, sizeof(bitmap)); // Write the updated bitmap

    close(disk_fd);
}

// Read an inode from disk
void read_inode ( int inode_num , inode * target ) {
    // Open the disk image file
    // TODO change the disk image file name to a constant
    int disk_fd = open("disk.img", O_RDWR);
    if (disk_fd < 0) {
        return -3; // Error opening file
    }

    // Ensure inode_num is within valid range
    if (inode_num < 0 || inode_num >= MAX_FILES) {
        close(disk_fd);
        return -3; // Invalid inode number
    }

    // Read the inode table from disk
    inode inodes[MAX_FILES];
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to the start of the inode table
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table

    // Copy the specified inode to target
    *target = inodes[inode_num];

    close(disk_fd);
}

// Write an inode to disk
void write_inode ( int inode_num , const inode * source ) {
    // Open the disk image file
    // TODO change the disk image file name to a constant
    int disk_fd = open("disk.img", O_RDWR);
    if (disk_fd < 0) {
        return -3; // Error opening file
    }

    // Ensure inode_num is within valid range
    if (inode_num < 0 || inode_num >= MAX_FILES) {
        close(disk_fd);
        return -3; // Invalid inode number
    }

    // Read the inode table from disk
    inode inodes[MAX_FILES];
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to the start of the inode table
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table

    // Update the specified inode with source data
    inodes[inode_num] = *source;

    // Write the updated inode table back to disk
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move back to the start of the inode table
    write(disk_fd, inodes, sizeof(inodes)); // Write the updated inode table

    close(disk_fd);
}