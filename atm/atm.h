// ATM client - handles user commands

#ifndef __ATM_H__
#define __ATM_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#define KEY_SIZE 32             // 256 bits for AES-256
#define CARD_SECRET_SIZE 32     // 256 bits for card secret

typedef struct _ATM
{
    // Networking state
    int sockfd;
    struct sockaddr_in rtr_addr;
    struct sockaddr_in atm_addr;

    // Protocol / session state
    int  logged_in;              // 0 = no user logged in, 1 = user logged in
    char current_user[251];      // currently logged-in username (if any)

    // Cryptographic state (Idea 1)
    unsigned char key_K[KEY_SIZE];                  // shared symmetric key from *.atm file
    unsigned long long seq;                         // sequence number for replay protection
    unsigned char card_secret[CARD_SECRET_SIZE];   // current user's card secret (loaded from .card)
    int key_loaded;                                 // 1 if key_K has been loaded, 0 otherwise

} ATM;

ATM* atm_create(const char *atm_init_file);
void atm_free(ATM *atm);
ssize_t atm_send(ATM *atm, char *data, size_t data_len);
ssize_t atm_recv(ATM *atm, char *data, size_t max_data_len);
void atm_process_command(ATM *atm, char *command);

#endif
