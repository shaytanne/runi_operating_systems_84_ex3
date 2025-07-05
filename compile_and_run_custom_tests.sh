set -e

gcc fs.c custom_tests.c -o custom_tests
./custom_tests
