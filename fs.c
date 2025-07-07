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

int fs_get_free_blocks();


int static disk_fd = -1; // file descriptor for the disk image file
bool static is_mounted = false; // Flag to check if the filesystem is mounted;


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
    // check if a filesystem of this type is already mounted
    if (is_mounted) {
        return -1;
    }

    // Opening the virtual disk
    disk_fd = open(disk_path, O_RDWR | O_CREAT, 0644);
    if (disk_fd < 0) {
        return -1; // Error opening file
    }

    // Allocate 10 MB: seek to (10 * 1024 * 1024) - 1 and write one byte
    lseek(disk_fd, ((10 * 1024 * 1024) - 1), SEEK_SET);
    write(disk_fd, "", 1); // Write a single byte to allocate the space

    // ==============================================================================
    // Initialize superblock
    superblock sb;
    sb.total_blocks = MAX_BLOCKS;
    sb.block_size = BLOCK_SIZE;
    sb.free_blocks = MAX_BLOCKS - 1 - 1 - 8; // 10 blocks reserved for metadata
    sb.total_inodes = MAX_FILES;
    sb.free_inodes = MAX_FILES; // Initially all inodes are free

    // Writing superblock 
    lseek ( disk_fd ,  0 * BLOCK_SIZE , SEEK_SET ) ; // Move to the start of the disk
    write ( disk_fd , &sb , sizeof(superblock) ) ; // Write the superblock to block 0

    // ==============================================================================

    // Initialize block bitmap
    unsigned char bitmap [ MAX_BLOCKS / 8];

    bitmap[0] |= (1 << 0); // Mark block 0 as used (superblock)
    bitmap[1] |= (1 << 0); // Mark block 1 as used (block bitmap)
    for (int i = 2; i < 10; i++) {
        bitmap[i / 8] |= (1 << (i % 8)); // Mark blocks 2-9 as used (inode table)
    }
    for (int i = 10; i < MAX_BLOCKS; i++) {
        bitmap [i/8] &= ~(1 << ( i %8) ); // Set all blocks as free (0)
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

     // Writing inode table
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to block 2 (start of inode table)
    write(disk_fd, inodes, sizeof(inodes)); // Write the inode table to blocks 2-9

    // ==============================================================================

    close(disk_fd);
    disk_fd = -1; // reset disk_fd to indicate no open file
    is_mounted = false; // reset the is_mounted flag

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
    if (is_mounted) {
        return -1; // already mounted
    }

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
        if ( (i < 10) && !(bitmap[i / 8] & (1 << (i % 8))) ) {
            close(disk_fd);
            return -1; // Reserved blocks should be marked as used
        }
        // if ( (i >= 10) && (bitmap[i / 8] & (1 << (i % 8))) ) {
        //     close(disk_fd);
        //     return -1; // Data blocks should be marked as free
        // }
    }

    // Check if the superblock is valid
    if (sb.total_blocks != MAX_BLOCKS || sb.block_size != BLOCK_SIZE || sb.total_inodes != MAX_FILES) {
        close(disk_fd);
        return -1; // Invalid filesystem structure
    }

    is_mounted = true; // Set the mounted flag to true

    return 0;
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
    is_mounted = false; // Set the mounted flag to false
    close(disk_fd); // Close the file
}



/**
 * @brief Creates a new empty file
 * 
 * Allocates an inode for a new file with the specified name. The file
 * initially has zero size and no allocated data blocks.
 * 
 * @param filename Name of the file to create (null-terminated, max 28 chars)
 * @return 0 on success, -1 if file already exists, -2 if no free inodes, -3 for other errors
 */
int fs_create(const char* filename)
{
    // Check if the filename is already in use
    if (is_mounted == false) {
        return -3; // Filesystem not mounted
    }   

    if (filename == NULL || strlen(filename) == 0 || strlen(filename) > 28) {
        return -3; // Invalid filename
    }
    
    int existing_inode_index = find_inode(filename);
    if (existing_inode_index >= 0) {
        return -1; // File already exists
    }

    // Find a free inode
    int inode_index = find_free_inode();
    if (inode_index < 0) {
        return -2; // No free inode available
    }


    // Initialize the new inode
    inode new_inode;
    new_inode.used = true;
    new_inode.size = 0;
    strncpy(new_inode.name, filename, 28);

    // initialize block pointers to -1
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        new_inode.blocks[i] = -1;
    }

    // Write the new inode to the disk
    write_inode(inode_index, &new_inode);

    // update superblock - decrease free inode count
    superblock sb;
    lseek(disk_fd, 0, SEEK_SET);
    read(disk_fd, &sb, sizeof(superblock));

    sb.free_inodes--;
    lseek(disk_fd, 0, SEEK_SET);
    write(disk_fd, &sb, sizeof(superblock));

    return 0; // Success
}


/**
 * @brief Lists the files in the filesystem
 * 
 * Populates the provided array with the names of files in the filesystem,
 * up to the specified maximum.
 * 
 * @param filenames Pre-allocated 2D array to receive file names
 * @param max_files Maximum number of file names to retrieve
 * @return Number of files found (0 to max_files), or -1 on error
 */
int fs_list(char filenames[][MAX_FILENAME], int max_files){
    if (is_mounted == false) {
        return -1; // Filesystem not mounted
    }

    if (filenames == NULL || max_files <= 0 || max_files > MAX_FILES) {
        return -1; // Invalid parameters
    }

    
    int count = 0;
    inode inodes[MAX_FILES];
    bool found = false;
    
    // Read the inode table from disk
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to the start of the inode table
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table

    // Iterate through inodes and collect file names
    for (int i = 0; i < MAX_FILES && count < max_files; i++) {
        //ensure filenamr are not duplicated
        for (int j = 0; j < count; j++) {
            if (strcmp(filenames[j], inodes[i].name) == 0) {
                found = true; // File already exists in the list
                break; // No need to add it again
            }
        }
        if (inodes[i].used && !found) {
            strncpy(filenames[count], inodes[i].name, MAX_FILENAME);
            filenames[count][MAX_FILENAME - 1] = '\0'; // Ensure null-termination
            count++;
        }
        found = false; // Reset found for the next iteration
    }

    return count; // Return the number of files found
}

/**
 * @brief Writes data to a file
 * 
 * Writes the specified data to a file, overwriting any existing content.
 * The function allocates or frees blocks as necessary to accommodate the
 * new file size.
 * 
 * @param filename Name of the file to write to
 * @param data Pointer to the data to write
 * @param size Number of bytes to write
 * @return 0 on success, -1 if file not found, -2 if out of space, -3 for other errors
 */
int fs_write(const char* filename, const void* data, int size) {
    if (is_mounted == false) {
        return -3; // Filesystem not mounted
    }

    if (filename == NULL || strlen(filename) == 0 || strlen(filename) > 28) {
        return -3; // Invalid filename
    }

    if (data == NULL || size < 0) {
        return -3; // Invalid data or size
    }

    int inode_index = find_inode(filename);
    if (inode_index < 0) {
        return -1; // File not found
    }

    // Read the inode to get its current state
    inode target_inode;
    read_inode(inode_index, &target_inode);
   
    // TODO:  Calculate number of blocks needed
    int num_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE; // Calculate number of blocks needed

    // Check if the file already has enough blocks allocated
    if (num_blocks > MAX_DIRECT_BLOCKS ) {
        return -2; // Too many blocks requested, exceeds maximum direct blocks
    }

    // count existing blocks for superblock update
    int old_blocks_used = 0;
    for (int j = 0; j < MAX_DIRECT_BLOCKS; j++) {
        if (target_inode.blocks[j] != -1) {
            old_blocks_used++;
        }
    }

    // read superblock
    superblock sb;
    lseek(disk_fd, 0, SEEK_SET);
    read(disk_fd, &sb, sizeof(superblock));

    // Verify superblock free count against bitmap
    unsigned char bitmap[MAX_BLOCKS / 8];
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET);
    read(disk_fd, bitmap, sizeof(bitmap));
    int free_in_bitmap = 0;
    for (int i = 10; i < MAX_BLOCKS; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            free_in_bitmap++;
        }
    }
    if (free_in_bitmap < sb.free_blocks) {
        // Bitmap shows fewer free blocks than superblock thinks
        sb.free_blocks = free_in_bitmap;
        lseek(disk_fd, 0, SEEK_SET);
        write(disk_fd, &sb, sizeof(superblock));
    }

    // check if we have enough space (after freeing existing blocks)
    if (num_blocks > sb.free_blocks + old_blocks_used) {
        return -2; // Not enough space
    }

    // update superblock for freed blocks
    sb.free_blocks += old_blocks_used;

    // free existing blocks
    for (int j = 0; j < MAX_DIRECT_BLOCKS; j++) {
        if (target_inode.blocks[j] != -1) {
            mark_block_free(target_inode.blocks[j]);
            target_inode.blocks[j] = -1;
        }
    }

    for (int i = 0; i < num_blocks; i++)
    {
        int index_block = find_free_block();

        if (index_block < 0) {
            // update superblock with blocks used so far
            lseek(disk_fd, 0, SEEK_SET);
            write(disk_fd, &sb, sizeof(superblock));

            // save inode with partial blocks
            write_inode(inode_index, &target_inode);
            
            return -2; // Out of space
        }

        target_inode.blocks[i] = index_block; // Allocate a new block for the file
        mark_block_used(index_block); // Mark the block as used
        sb.free_blocks--;

        // Calculate how many bytes to write to this block
        int bytes_to_write;
        if (i == num_blocks - 1 && size % BLOCK_SIZE != 0) {
            bytes_to_write = size % BLOCK_SIZE; // Last block partial
        } 
        else {
            bytes_to_write = BLOCK_SIZE; // Full block
        }
        
        lseek(disk_fd, target_inode.blocks[i] * BLOCK_SIZE, SEEK_SET);
        write(disk_fd, (char*)data + (i * BLOCK_SIZE), bytes_to_write);
    }


    // Update the inode with new size and mark it as used
    target_inode.used = true;
    target_inode.size = size;

    // Write the updated inode back to disk
    write_inode(inode_index, &target_inode);

    // update superblock
    lseek(disk_fd, 0, SEEK_SET);
    write(disk_fd, &sb, sizeof(superblock));

    return 0; // Success
}


/**
 * @brief Reads data from a file
 * 
 * Reads up to 'size' bytes from the specified file into the provided buffer.
 * If the file is smaller than the requested size, only the available data is read.
 * 
 * @param filename Name of the file to read from
 * @param buffer Pre-allocated buffer to receive the data
 * @param size Size of the buffer in bytes
 * @return Number of bytes read on success, -1 if file not found, -3 for other errors
 */
int fs_read(const char* filename, void* buffer, int size){
    if (is_mounted == false) {
        return -3; // Filesystem not mounted
    }

    if (filename == NULL || strlen(filename) == 0 || strlen(filename) > 28) {
        return -3; // Invalid filename
    }

    if (buffer == NULL || size < 0) {
        return -3; // Invalid buffer or size
    }

    int inode_index = find_inode(filename);
    if (inode_index < 0) {
        return -1; // File not found
    }

    // Read the inode to get its data blocks and size
    inode target_inode;
    read_inode(inode_index, &target_inode);

    // Check if the requested size exceeds the file size
    if (size > target_inode.size) {
        size = target_inode.size; // Adjust size to the actual file size
    }


    int bytes_read = 0;
    
    for (int i = 0; i < MAX_DIRECT_BLOCKS && bytes_read < size; i++) {
        if (target_inode.blocks[i] != -1) {
            lseek(disk_fd, target_inode.blocks[i] * BLOCK_SIZE, SEEK_SET);
            int bytes_to_read = (size - bytes_read < BLOCK_SIZE) ? (size - bytes_read) : BLOCK_SIZE;
            read(disk_fd, (char*)buffer + bytes_read, bytes_to_read);
            bytes_read += bytes_to_read;
        }
    }

    return bytes_read; // Return the number of bytes read
}

/**
 * @brief Deletes an existing file
 * 
 * Removes a file from the filesystem, freeing its inode and any
 * allocated data blocks.
 * 
 * @param filename Name of the file to delete (null-terminated)
 * @return 0 on success, -1 if file not found, -2 for other errors
 */
int fs_delete(const char* filename)
{
    if (is_mounted == false) {
        return -2; // Filesystem not mounted
    }

    if (filename == NULL || strlen(filename) == 0 || strlen(filename) > 28) {
        return -2; // Invalid filename
    }

    int inode_index = find_inode(filename);
    if (inode_index < 0) {
        return -1; // File not found
    }

    // Read the inode to get its data blocks
    inode target_inode;
    read_inode(inode_index, &target_inode);

    // Count blocks to free for superblock update
    int blocks_freed = 0;

    // Free the data blocks associated with the inode
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        if (target_inode.blocks[i] >= 0) {
            mark_block_free(target_inode.blocks[i]);
            target_inode.blocks[i] = -1; // Reset the block pointer
            blocks_freed++;
        }
    }

    // Mark the inode as free
    target_inode.used = false;
    target_inode.size = 0;

    // Write the updated inode back to disk
    write_inode(inode_index, &target_inode);

    // Update superblock - increase free block count
    superblock sb;
    lseek(disk_fd, 0, SEEK_SET);
    read(disk_fd, &sb, sizeof(superblock));

    sb.free_blocks += blocks_freed;
    sb.free_inodes++;

    lseek(disk_fd, 0, SEEK_SET);
    write(disk_fd, &sb, sizeof(superblock));

    return 0; // Success
}

// ==============================================================================

// Sample Helper Functions

// ==============================================================================

// Find an inode by filename
int find_inode ( const char * filename ) {


    inode inodes[MAX_FILES];
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to the start of the inode table
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table

    // Search for the inode with the given filename
    for (int i = 0; i < MAX_FILES; i++) {
        if (inodes[i].used && strcmp(inodes[i].name, filename) == 0) {
            return i; // Found the inode
        }
    }

    return -1; // Inode not found
}

// Find a free inode
int find_free_inode () {

    inode inodes[MAX_FILES];
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to the start of the inode table
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table

    // Search for a free inode
    for (int i = 0; i < MAX_FILES; i++) {
        if (!inodes[i].used) {
            return i; // Found a free inode
        }
    }

    return -1; // No free inode found
}

// Find a free block
int find_free_block () {

    unsigned char bitmap[MAX_BLOCKS / 8];
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move to the block bitmap
    read(disk_fd, bitmap, sizeof(bitmap)); // Read the block bitmap

    // Search for a free block
    // Skip the first 10 blocks (0-9) as they are reserved for superblock and metadata
    // TODO: Check if the bitmap is correctly initialized
    for (int i = 10; i < MAX_BLOCKS; i++) {
        if ( !(bitmap[i / 8] & (1 << (i % 8))) ) {
            return i; // Found a free block
        }
    }

    // No free blocks found - fix superblock if needed
    superblock sb;
    lseek(disk_fd, 0, SEEK_SET);
    read(disk_fd, &sb, sizeof(superblock));
    
    if (sb.free_blocks > 0) {
        // Superblock thinks there are free blocks but bitmap shows none
        // This is a filesystem inconsistency - fix it
        sb.free_blocks = 0;
        lseek(disk_fd, 0, SEEK_SET);
        write(disk_fd, &sb, sizeof(superblock));
    }

    return -1; // No free block found
}

// Mark a block as used
void mark_block_used ( int block_num ) {
    
    // Ensure block_num is within valid range
    if (block_num < 0 || block_num >= MAX_BLOCKS) {
        return; // Invalid block number
    }
    unsigned char bitmap[MAX_BLOCKS / 8];
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move to the block bitmap
    read(disk_fd, bitmap, sizeof(bitmap)); // Read the block bitmap

    // Mark the specified block as used
    bitmap[block_num / 8] |= (1 << (block_num % 8));

    // Write the updated bitmap back to disk
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move back to the block bitmap
    write(disk_fd, bitmap, sizeof(bitmap)); // Write the updated bitmap
}

// Mark a block as free
void mark_block_free ( int block_num ) {

    if (disk_fd < 0) {
        return; // Error opening file
    }

    // Ensure block_num is within valid range
    if (block_num < 0 || block_num >= MAX_BLOCKS) {
        return; // Invalid block number
    }
    unsigned char bitmap[MAX_BLOCKS / 8];
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move to the block bitmap
    read(disk_fd, bitmap, sizeof(bitmap)); // Read the block bitmap

    // Mark the specified block as free
    bitmap[block_num / 8] &= ~(1 << (block_num % 8));

    // Write the updated bitmap back to disk
    lseek(disk_fd, 1 * BLOCK_SIZE, SEEK_SET); // Move back to the block bitmap
    write(disk_fd, bitmap, sizeof(bitmap)); // Write the updated bitmap
}

// Read an inode from disk
void read_inode ( int inode_num , inode * target ) {

    // Read the inode table from disk
    inode inodes[MAX_FILES];
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to the start of the inode table
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table

    // Copy the specified inode to target
    *target = inodes[inode_num];
}

// Write an inode to disk
void write_inode ( int inode_num , const inode * source ) {

    // Read the inode table from disk
    inode inodes[MAX_FILES];
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move to the start of the inode table
    read(disk_fd, inodes, sizeof(inodes)); // Read the inode table

    // Update the specified inode with source data
    inodes[inode_num] = *source;

    // Write the updated inode table back to disk
    lseek(disk_fd, 2 * BLOCK_SIZE, SEEK_SET); // Move back to the start of the inode table
    write(disk_fd, inodes, sizeof(inodes)); // Write the updated inode table
}

// Add to fs.c
int fs_get_free_blocks() {
    if (!is_mounted || disk_fd < 0) {
        return -1;
    }
    
    superblock sb;
    lseek(disk_fd, 0, SEEK_SET);
    read(disk_fd, &sb, sizeof(superblock));
    return sb.free_blocks;
}