set -e

gcc fs.c testfilesystem.c -o test_fs
./test_fs
