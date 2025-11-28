/*
 * Init program - creates shared secret key files for ATM and Bank
 * 
 * Usage: init <filename>
 * 
 * Creates:
 *   <filename>.atm  - contains 32-byte shared key K
 *   <filename>.bank - contains 32-byte shared key K
 * 
 * This shared key is used for encrypting and authenticating all
 * ATM-Bank communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/rand.h>

#define KEY_SIZE 32

/*
 * Check if a file exists
 */
static int file_exists(const char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

/*
 * Write key to file
 * Returns 0 on success, -1 on error
 */
static int write_key_file(const char *filename, const unsigned char *key)
{
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        return -1;
    }

    size_t written = fwrite(key, 1, KEY_SIZE, f);
    fclose(f);

    if (written != KEY_SIZE) {
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc != 2) {
        printf("Usage:  init <filename>\n");
        return 62;
    }

    // Construct filenames
    char atm_filename[512];
    char bank_filename[512];
    
    int ret = snprintf(atm_filename, sizeof(atm_filename), "%s.atm", argv[1]);
    if (ret < 0 || ret >= (int)sizeof(atm_filename)) {
        printf("Error creating initialization files\n");
        return 64;
    }

    ret = snprintf(bank_filename, sizeof(bank_filename), "%s.bank", argv[1]);
    if (ret < 0 || ret >= (int)sizeof(bank_filename)) {
        printf("Error creating initialization files\n");
        return 64;
    }

    // Check if either file already exists
    if (file_exists(atm_filename) || file_exists(bank_filename)) {
        printf("Error: one of the files already exists\n");
        return 63;
    }

    // Generate random 32-byte key K
    unsigned char key[KEY_SIZE];
    if (RAND_bytes(key, KEY_SIZE) != 1) {
        printf("Error creating initialization files\n");
        return 64;
    }

    // Write key to both files
    if (write_key_file(atm_filename, key) != 0) {
        printf("Error creating initialization files\n");
        return 64;
    }

    if (write_key_file(bank_filename, key) != 0) {
        // Try to clean up the atm file we created
        remove(atm_filename);
        printf("Error creating initialization files\n");
        return 64;
    }

    printf("Successfully initialized bank state\n");
    return 0;
}
