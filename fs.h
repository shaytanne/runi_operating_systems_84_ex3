/**
 * @file fs.h
 * @brief Header file for the OnlyFiles filesystem implementation
 * 
 * This header defines the structures and function prototypes for a simple
 * block-based filesystem implementation. The filesystem supports basic file
 * operations but does not include directories, permissions, or other advanced
 * features found in production filesystems.
 *
 * The filesystem is designed to be contained within a single disk image file,
 * with a fixed layout of metadata and data blocks.
 *
 * DO NOT MODIFY THIS HEADER FILE FOR YOUR IMPLEMENTATION.
 */

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#ifndef FS_H
#define FS_H

/**
 * @brief Maximum length of a filename (excluding null terminator)
 * 
 * Files can have names up to 28 characters in length. The actual string
 * in memory will be 29 bytes to accommodate the null terminator.
 */
#define MAX_FILENAME 28

/**
 * @brief Maximum number of files supported by the filesystem
 * 
 * The filesystem can hold up to 256 files simultaneously. This value
 * determines the size of the inode table.
 */
#define MAX_FILES 256

/**
 * @brief Total number of blocks in the filesystem
 * 
 * The filesystem contains 2560 blocks in total. With a block size of 4KB,
 * this gives a total virtual disk size of 10MB (2560 * 4096 = 10,485,760 bytes).
 */
#define MAX_BLOCKS 2560

/**
 * @brief Size of each block in bytes
 * 
 * Each block is 4KB (4096 bytes) in size. This value affects file I/O operations
 * and determines the granularity of storage allocation.
 */
#define BLOCK_SIZE 4096

/**
 * @brief Maximum number of direct block pointers per file
 * 
 * Each file can reference up to 12 direct blocks, which means the maximum
 * file size is 12 * 4KB = 48KB. This filesystem does not support indirect blocks.
 */
#define MAX_DIRECT_BLOCKS 12

/**
 * @brief Superblock structure containing filesystem metadata
 * 
 * The superblock is stored at the beginning of the disk image and contains
 * critical information about the filesystem's structure and state. It occupies
 * the first block of the filesystem (block 0).
 */
typedef struct {
    int total_blocks;  /**< Total number of blocks in the filesystem (2560) */
    int block_size;    /**< Size of each block in bytes (4096) */
    int free_blocks;   /**< Number of blocks currently available for allocation */
    int total_inodes;  /**< Total number of inodes/files the filesystem can hold (256) */
    int free_inodes;   /**< Number of inodes currently available for allocation */
} superblock;

/**
 * @brief Inode structure representing a file
 * 
 * Each file in the filesystem is represented by an inode, which stores
 * metadata about the file and pointers to its data blocks. The inode table
 * starts at block 2 and occupies 8 blocks (blocks 2-9).
 */
typedef struct {
    int used;                          /**< Flag indicating if this inode is in use (1) or free (0) */
    char name[MAX_FILENAME];           /**< Name of the file (up to 28 characters + null terminator) */
    int size;                          /**< Size of the file in bytes */
    int blocks[MAX_DIRECT_BLOCKS];     /**< Array of block indices containing file data */
} inode;

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
int fs_format(const char* disk_path);

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
int fs_mount(const char* disk_path);

/**
 * @brief Unmounts the filesystem
 * 
 * Ensures all pending changes are written to the disk image file and
 * closes the file. After unmounting, no further filesystem operations
 * should be performed until the filesystem is mounted again.
 */
void fs_unmount();

/**
 * @brief Creates a new empty file
 * 
 * Allocates an inode for a new file with the specified name. The file
 * initially has zero size and no allocated data blocks.
 * 
 * @param filename Name of the file to create (null-terminated, max 28 chars)
 * @return 0 on success, -1 if file already exists, -2 if no free inodes, -3 for other errors
 */
int fs_create(const char* filename);

/**
 * @brief Deletes an existing file
 * 
 * Removes a file from the filesystem, freeing its inode and any
 * allocated data blocks.
 * 
 * @param filename Name of the file to delete (null-terminated)
 * @return 0 on success, -1 if file not found, -2 for other errors
 */
int fs_delete(const char* filename);

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
int fs_list(char filenames[][MAX_FILENAME], int max_files);

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
int fs_write(const char* filename, const void* data, int size);

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
int fs_read(const char* filename, void* buffer, int size);

#endif /* FS_H */
