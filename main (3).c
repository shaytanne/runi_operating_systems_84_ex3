/**
 * @file main.c
 * @brief Basic test program for the OnlyFiles filesystem
 *
 * This program demonstrates the basic functionality of the OnlyFiles filesystem
 * implementation. It performs the following operations:
 * 1. Formats a new filesystem
 * 2. Mounts the filesystem
 * 3. Creates a file
 * 4. Writes data to the file
 * 5. Reads the data back from the file
 * 6. Unmounts the filesystem
 *
 * Use this file as a starting point for testing your filesystem implementation.
 * For more thorough testing, you should extend this program or create additional
 * test programs to cover edge cases and error conditions.
 */

#include "fs.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Main function demonstrating basic filesystem operations
 * 
 * @return 0 on successful execution
 */
int main() {
    int result;
    
    // Step 1: Format a new filesystem
    // This creates a fresh 10MB disk image file called "disk.img"
    printf("Formatting filesystem...\n");
    result = fs_format("disk.img");
    if (result != 0) {
        printf("Error formatting filesystem (code: %d)\n", result);
        return 1;
    }
    
    // Step 2: Mount the filesystem
    // This opens the disk image and prepares it for operations
    printf("Mounting filesystem...\n");
    result = fs_mount("disk.img");
    if (result != 0) {
        printf("Error mounting filesystem (code: %d)\n", result);
        return 1;
    }
    
    // Step 3: Create a new file
    // This allocates an inode for a file named "file1.txt"
    printf("Creating file...\n");
    result = fs_create("file1.txt");
    if (result != 0) {
        printf("Error creating file (code: %d)\n", result);
        fs_unmount();
        return 1;
    }
    
    // Step 4: Write data to the file
    // This allocates blocks and writes the string data to the file
    printf("Writing to file...\n");
    char data[] = "Hello, filesystem!";  // Note that sizeof(data) includes the null terminator
    result = fs_write("file1.txt", data, sizeof(data));
    if (result != 0) {
        printf("Error writing to file (code: %d)\n", result);
        fs_unmount();
        return 1;
    }
    
    // Step 5: Read data from the file
    // This reads the file content into a buffer
    printf("Reading from file...\n");
    char buffer[100];  // Buffer to hold the file content
    memset(buffer, 0, sizeof(buffer));  // Initialize buffer to zeros
    
    result = fs_read("file1.txt", buffer, sizeof(buffer));
    if (result < 0) {
        printf("Error reading from file (code: %d)\n", result);
        fs_unmount();
        return 1;
    }
    
    // Display the read data
    printf("Read: %s\n", buffer);
    
    // Step 6: Unmount the filesystem
    // This ensures all changes are written to disk and closes the file
    printf("Unmounting filesystem...\n");
    fs_unmount();
    
    printf("Test completed successfully.\n");
    return 0;
}
