/*
 * The Bank takes commands from stdin as well as from the ATM.  
 *
 * Commands from stdin be handled by bank_process_local_command.
 *
 * Remote commands from the ATM should be handled by
 * bank_process_remote_command.
 *
 * The Bank can read both .card files AND .pin files.
 *
 * Feel free to update the struct and the processing as you desire
 * (though you probably won't need/want to change send/recv).
 */

#ifndef __BANK_H__
#define __BANK_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#define MAX_USERS 1000
#define KEY_SIZE 32             // 256 bits for AES-256
#define CARD_SECRET_SIZE 32     // 256 bits for card secret

typedef struct _User {
    char username[251];                             // [a-zA-Z]+, up to 250 chars + null
    char pin[5];                                    // 4 digits + null
    int  balance;                                   // current balance
    unsigned char card_secret[CARD_SECRET_SIZE];   // per-user card secret for authentication
    unsigned long long last_seq;                    // last valid sequence number (replay protection)
} User;

typedef struct _Bank
{
    // Networking state
    int sockfd;
    struct sockaddr_in rtr_addr;
    struct sockaddr_in bank_addr;
    struct sockaddr_in last_client_addr;  // Address of last received packet (for replies)

    // Protocol / account state
    User users[MAX_USERS];
    int  num_users;

    // Cryptographic state (Idea 1)
    unsigned char key_K[KEY_SIZE];  // shared symmetric key from *.bank file
    int key_loaded;                  // 1 if key_K has been loaded, 0 otherwise

} Bank;

Bank* bank_create(const char *bank_init_file);
void bank_free(Bank *bank);
ssize_t bank_send(Bank *bank, char *data, size_t data_len);
ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len);
void bank_process_local_command(Bank *bank, char *command, size_t len);
void bank_process_remote_command(Bank *bank, char *command, size_t len);

#endif
