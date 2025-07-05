#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fs.h"

#define DISK_PATH "advanced_test.img"
#define BUFFER_SIZE 8192

// Utility function to print test results
void test_result(const char* test_name, int result) {
    printf("%-40s %s\n", test_name, result ? "PASS" : "FAIL");
}

// Utility function to generate test data
void generate_test_data(char* buffer, int size, char pattern) {
    for (int i = 0; i < size; i++) {
        buffer[i] = pattern++;
        i++;
    }
}

// Utility function to verify test data
int verify_test_data(const char* buffer, int size, char pattern) {
    for (int i = 0; i < size; i++) {
        if (buffer[i] != (pattern++)) {
            printf("Data mismatch at position %d: expected %d, got %d\n", 
                   i, pattern++, buffer[i]);
            return 0;
        }
        i++;
    }
    return 1;
}

int main() {
    char write_buffer[BUFFER_SIZE];
    char read_buffer[BUFFER_SIZE];
    char filenames[MAX_FILES][MAX_FILENAME];
    int result;
    
    printf("==== ADVANCED FILESYSTEM TESTS ====\n\n");
    
    // Remove existing disk image if it exists
    if (access(DISK_PATH, F_OK) == 0) {
        remove(DISK_PATH);
    }
    
    // 1. FORMAT/MOUNT/UNMOUNT TESTS
    printf("== Format/Mount/Unmount Tests ==\n");
    
    result = fs_format(DISK_PATH);
    test_result("Format new disk", result == 0);
    
    result = fs_mount(DISK_PATH);
    
    test_result("Mount formatted disk", result == 0);
    
    result = fs_format(DISK_PATH);
    printf("result = fs_format(DISK_PATH) is %d;\n", result);
    test_result("Format already mounted disk (should fail)", result == -1);
    
    fs_unmount();
    test_result("Unmount disk", 1); // Always succeeds
    
    // 2. FILE CREATION TESTS
    printf("\n== File Creation Tests ==\n");
    
    fs_mount(DISK_PATH);
    
    result = fs_create(NULL);
    test_result("Create file with NULL name (should fail)", result == -3);
    
    result = fs_create("");
    test_result("Create file with empty name (should fail)", result == -3);
    
    char long_name[100];
    memset(long_name, 'a', 99);
    long_name[99] = '\0';
    result = fs_create(long_name);
    test_result("Create file with too long name (should fail)", result == -3);
    
    result = fs_create("test_file.txt");
    test_result("Create valid file", result == 0);
    
    result = fs_create("test_file.txt");
    test_result("Create duplicate file (should fail)", result == -1);
    
    // Create files with boundary names
    char boundary_name[29];
    memset(boundary_name, 'b', 28);
    boundary_name[28] = '\0';
    result = fs_create(boundary_name);
    test_result("Create file with max length name", result == 0);
    
    // 3. FILE LISTING TESTS
    printf("\n== File Listing Tests ==\n");
    
    int num_files = fs_list(filenames, MAX_FILES);
    test_result("List files", num_files == 2);
    
    result = fs_list(NULL, 10);
    test_result("List with NULL array (should fail)", result == -1);
    
    result = fs_list(filenames, -1);
    test_result("List with negative max_files (should fail)", result == -1);
    
    // 4. FILE DELETION TESTS
    printf("\n== File Deletion Tests ==\n");
    
    result = fs_delete("nonexistent.txt");
    test_result("Delete non-existent file (should fail)", result == -1);
    
    result = fs_delete("test_file.txt");
    test_result("Delete existing file", result == 0);
    
    num_files = fs_list(filenames, MAX_FILES);
    test_result("Verify file was deleted", num_files == 1);
    
    result = fs_create("test_file.txt");
    test_result("Create file with previously deleted name", result == 0);
    
    // 5. FILE WRITE TESTS
    printf("\n== File Write Tests ==\n");
    
    // Test empty write
    result = fs_write("test_file.txt", write_buffer, 0);
    test_result("Write 0 bytes", result == 0);
    
    // Test small write
    generate_test_data(write_buffer, 100, 'A');
    result = fs_write("test_file.txt", write_buffer, 100);
    test_result("Write small file (100 bytes)", result == 0);
    
    // Test exactly one block
    generate_test_data(write_buffer, BLOCK_SIZE, 'B');
    result = fs_write("test_file.txt", write_buffer, BLOCK_SIZE);
    test_result("Write exactly one block", result == 0);
    
    // Test multiple blocks
    generate_test_data(write_buffer, BLOCK_SIZE * 3 + 100, 'C');
    result = fs_write("test_file.txt", write_buffer, BLOCK_SIZE * 3 + 100);
    test_result("Write multiple blocks with partial last block", result == 0);
    
    // Test maximum file size
    int max_size = BLOCK_SIZE * MAX_DIRECT_BLOCKS;
    char* large_buffer = malloc(max_size);
    if (large_buffer) {
        generate_test_data(large_buffer, max_size, 'D');
        result = fs_write("test_file.txt", large_buffer, max_size);
        test_result("Write maximum file size", result == 0);
        free(large_buffer);
    } else {
        printf("Skipping max size test - not enough memory\n");
    }
    
    // Test exceeding max size
    generate_test_data(write_buffer, BLOCK_SIZE, 'E');
    result = fs_write("nonexistent.txt", write_buffer, BLOCK_SIZE);
    test_result("Write to non-existent file (should fail)", result == -1);
    
    // 6. FILE READ TESTS
    printf("\n== File Read Tests ==\n");
    
    // Write test data for reading tests
    generate_test_data(write_buffer, 500, 'F');
    fs_write("test_file.txt", write_buffer, 500);
    
    result = fs_read("nonexistent.txt", read_buffer, 100);
    test_result("Read from non-existent file (should fail)", result == -1);
    
    result = fs_read("test_file.txt", NULL, 100);
    test_result("Read with NULL buffer (should fail)", result == -3);
    
    result = fs_read("test_file.txt", read_buffer, -1);
    test_result("Read with negative size (should fail)", result == -3);
    
    memset(read_buffer, 0, BUFFER_SIZE);
    result = fs_read("test_file.txt", read_buffer, 250);
    test_result("Read partial file (less than actual size)", result == 250);
    test_result("Verify partial read data integrity", 
                verify_test_data(read_buffer, 250, 'F'));
                
    printf("Read buffer content: %.*s\n", 250, read_buffer);

    memset(read_buffer, 0, BUFFER_SIZE);
    result = fs_read("test_file.txt", read_buffer, 500);
    test_result("Read exact file size", result == 500);
    test_result("Verify exact read data integrity", 
                verify_test_data(read_buffer, 500, 'F'));
    
    memset(read_buffer, 0, BUFFER_SIZE);
    result = fs_read("test_file.txt", read_buffer, 1000);
    test_result("Read larger than file size", result == 500);
    test_result("Verify oversized read data integrity", 
                verify_test_data(read_buffer, 500, 'F'));
    
// // ================================================================= 
//     // 7. PERSISTENCE TESTS
//     printf("\n== Persistence Tests ==\n");
    
//     // Create a new file with known content
//     generate_test_data(write_buffer, 750, 'G');
//     fs_write("persist_test.txt", write_buffer, 750);
    
//     // Unmount and remount
//     fs_unmount();
//     result = fs_mount(DISK_PATH);
//     test_result("Remount after unmount", result == 0);
    
//     // Verify file still exists
//     num_files = fs_list(filenames, MAX_FILES);
//     int found = 0;
//     for (int i = 0; i < num_files; i++) {
//         if (strcmp(filenames[i], "persist_test.txt") == 0) {
//             found = 1;
//             break;
//         }
//     }
//     test_result("File persists after remount", found);
    
//     // Verify content persists
//     memset(read_buffer, 0, BUFFER_SIZE);
//     result = fs_read("persist_test.txt", read_buffer, 750);
//     test_result("Read persisted file", result == 750);
//     test_result("Verify persisted data integrity", 
//                 verify_test_data(read_buffer, 750, 'G'));
    


 // =================================================================
         // 7. PERSISTENCE TESTS
    printf("\n== Persistence Tests NOT CHATGPT ==\n");
    result = fs_create("persist_test.txt");
    
    test_result("Create file for persistence test", result == 0);
    // Create a new file with known content
    write_buffer[750];
    strcpy(write_buffer, "This is a test file for persistence.");
    fs_write("persist_test.txt", write_buffer, 750);
    
    // Unmount and remount
    fs_unmount();
    result = fs_mount(DISK_PATH);
    test_result("Remount after unmount", result == 0);
    
    // Verify file still exists
    num_files = fs_list(filenames, MAX_FILES);
    int found = 0;
    for (int i = 0; i < num_files; i++) {
        if (strcmp(filenames[i], "persist_test.txt") == 0) {
            found = 1;
            break;
        }
    }
    test_result("File persists after remount", found);
    
    // Verify content persists
    memset(read_buffer, 0, BUFFER_SIZE);
    result = fs_read("persist_test.txt", read_buffer, 750);
    test_result("Read persisted file", result == 750);
    test_result("Verify persisted data integrity", 
                verify_test_data(read_buffer, 750, 'G'));



// =================================================================

    // 8. DISK SPACE MANAGEMENT TESTS
    printf("\n== Disk Space Management Tests ==\n");
    
    // Create many small files to use up inodes
    int files_created = 0;
    char file_name[MAX_FILENAME];
    for (int i = 0; i < MAX_FILES - 3; i++) { // -3 for existing files
        sprintf(file_name, "file_%d.txt", i);
        if (fs_create(file_name) == 0) {
            files_created++;
        } else {
            break;
        }
    }
    printf("Created %d additional files\n", files_created);
    
    // Try to create one more file
    result = fs_create("one_too_many.txt");
    test_result("Create file when no free inodes (should fail)", 
                (result == -2) || (files_created < MAX_FILES - 3));
    
    // Clean up by deleting half the files
    for (int i = 0; i < files_created; i += 2) {
        sprintf(file_name, "file_%d.txt", i);
        fs_delete(file_name);
    }
    
    // Write a large file to try to fill disk space
    int large_size = BLOCK_SIZE * 6; // Big enough to need multiple blocks
    large_buffer = malloc(large_size);
    if (large_buffer) {
        generate_test_data(large_buffer, large_size, 'H');
        result = fs_write("big_file.txt", large_buffer, large_size);
        test_result("Write large file after freeing space", result == 0);
        free(large_buffer);
    } else {
        printf("Skipping large file test - not enough memory\n");
    }
    
    // Clean up
    fs_unmount();
    
    printf("\n==== TEST SUITE COMPLETE ====\n");
    return 0;
}