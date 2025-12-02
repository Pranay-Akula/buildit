#include "bank.h"
#include "ports.h"
#include "protocol.h"
#include "crypto.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

Bank* bank_create(const char *bank_init_file)
{
    Bank *bank = (Bank*) malloc(sizeof(Bank));
    if(bank == NULL)
    {
        perror("Could not allocate Bank");
        exit(1);
    }

    // Set up the network state
    bank->sockfd = socket(AF_INET,SOCK_DGRAM,0);

    bzero(&bank->rtr_addr,sizeof(bank->rtr_addr));
    bank->rtr_addr.sin_family = AF_INET;
    bank->rtr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bank->rtr_addr.sin_port = htons(ROUTER_PORT);

    bzero(&bank->bank_addr, sizeof(bank->bank_addr));
    bank->bank_addr.sin_family = AF_INET;
    bank->bank_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bank->bank_addr.sin_port = htons(BANK_PORT);
    bind(bank->sockfd,(struct sockaddr *)&bank->bank_addr,sizeof(bank->bank_addr));

    // Initialize account state
    bank->num_users = 0;
    
    bank->key_loaded = 0;
    memset(bank->key_K, 0, KEY_SIZE);
    
    // Load key from init file
    FILE *key_file = fopen(bank_init_file, "rb");
    if (key_file == NULL) {
        printf("Error opening bank initialization file\n");
        free(bank);
        exit(64);
    }
    
    size_t bytes_read = fread(bank->key_K, 1, KEY_SIZE, key_file);
    fclose(key_file);
    
    if (bytes_read != KEY_SIZE) {
        printf("Error opening bank initialization file\n");
        free(bank);
        exit(64);
    }
    
    bank->key_loaded = 1;

    return bank;
}

void bank_free(Bank *bank)
{
    if(bank != NULL)
    {
        close(bank->sockfd);
        free(bank);
    }
}

ssize_t bank_send(Bank *bank, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    // Send to router, which will forward back to ATM based on source port
    return sendto(bank->sockfd, data, data_len, 0,
                  (struct sockaddr*) &bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
}

static void trim_buffer(char *buf)
{
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' ||
                     buf[n-1] == ' '  || buf[n-1] == '\t')) {
        buf[n-1] = '\0';
        n--;
    }
}

static int is_valid_username(const char *u)
{
    size_t n = strlen(u);
    if (n == 0 || n > 250) return 0;
    for (size_t i = 0; i < n; i++) {
        if (!((u[i] >= 'a' && u[i] <= 'z') || (u[i] >= 'A' && u[i] <= 'Z')))
            return 0;
    }
    return 1;
}

static int is_valid_pin(const char *pin)
{
    if (strlen(pin) != 4) return 0;
    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)pin[i])) return 0;
    }
    return 1;
}

static int parse_amount(const char *s, int *out)
{
    if (s == NULL || *s == '\0') return 0;
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }

    char *endptr = NULL;
    long val = strtol(s, &endptr, 10);
    if (*endptr != '\0') return 0;
    if (val < 0 || val > INT_MAX) return 0;

    *out = (int)val;
    return 1;
}

static int find_user(Bank *bank, const char *username)
{
    for (int i = 0; i < bank->num_users; i++) {
        if (strcmp(bank->users[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

void bank_process_local_command(Bank *bank, char *command, size_t len)
{
    // command is not guaranteed to be null-terminated, so copy it
    char buf[1000];
    size_t n = (len < sizeof(buf) - 1) ? len : (sizeof(buf) - 1);
    memcpy(buf, command, n);
    buf[n] = '\0';
    trim_buffer(buf);

    // Skip leading whitespace
    char *p = buf;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return; // empty line

    // Extract first token (command word)
    char cmd[32];
    if (sscanf(p, "%31s", cmd) != 1) {
        printf("Invalid command\n");
        return;
    }

    // CREATE-USER
    if (strcmp(cmd, "create-user") == 0) {
        char user[256], pin[16], bal_str[64];

        // We want exactly 3 arguments after the command
        int num = sscanf(p, "%*s %255s %15s %63s", user, pin, bal_str);
        if (num != 3 || !is_valid_username(user) || !is_valid_pin(pin)) {
            printf("Usage:  create-user <user-name> <pin> <balance>\n");
            return;
        }

        int balance = 0;
        if (!parse_amount(bal_str, &balance)) {
            printf("Usage:  create-user <user-name> <pin> <balance>\n");
            return;
        }

        // Check if user already exists
        if (find_user(bank, user) != -1) {
            printf("Error:  user %s already exists\n", user);
            return;
        }

        if (bank->num_users >= MAX_USERS) {
            // Not in spec; we'll just refuse silently
            return;
        }

        unsigned char card_secret[CARD_SECRET_SIZE];
        if (generate_random_bytes(card_secret, CARD_SECRET_SIZE) != 0) {
            printf("Error creating card file for user %s\n", user);
            return;
        }

        char card_filename[300];
        snprintf(card_filename, sizeof(card_filename), "%s.card", user);
        FILE *cf = fopen(card_filename, "wb");
        if (cf == NULL) {
            printf("Error creating card file for user %s\n", user);
            return;
        }

        size_t written = fwrite(card_secret, 1, CARD_SECRET_SIZE, cf);
        fclose(cf);
        
        if (written != CARD_SECRET_SIZE) {
            remove(card_filename);
            printf("Error creating card file for user %s\n", user);
            return;
        }

        User *u = &bank->users[bank->num_users++];
        strncpy(u->username, user, sizeof(u->username));
        u->username[sizeof(u->username)-1] = '\0';
        strncpy(u->pin, pin, sizeof(u->pin));
        u->pin[sizeof(u->pin)-1] = '\0';
        u->balance = balance;
        
        memcpy(u->card_secret, card_secret, CARD_SECRET_SIZE);
        u->last_seq = 0;

        printf("Created user %s\n", user);
        return;
    }

    // DEPOSIT
    if (strcmp(cmd, "deposit") == 0) {
        char user[256], amt_str[64];
        int num = sscanf(p, "%*s %255s %63s", user, amt_str);
        if (num != 2 || !is_valid_username(user)) {
            printf("Usage:  deposit <user-name> <amt>\n");
            return;
        }

        int amt = 0;
        if (!parse_amount(amt_str, &amt)) {
            printf("Usage:  deposit <user-name> <amt>\n");
            return;
        }

        int idx = find_user(bank, user);
        if (idx == -1) {
            printf("No such user\n");
            return;
        }

        if (amt > 0 && bank->users[idx].balance > INT_MAX - amt) {
            printf("Too rich for this program\n");
            return;
        }

        bank->users[idx].balance += amt;
        printf("$%d added to %s's account\n", amt, user);
        return;
    }

    // BALANCE
    if (strcmp(cmd, "balance") == 0) {
        char user[256], extra[64];
        int num = sscanf(p, "%*s %255s %63s", user, extra);
        // There should be exactly one argument
        if (num != 1 || !is_valid_username(user)) {
            printf("Usage:  balance <user-name>\n");
            return;
        }

        int idx = find_user(bank, user);
        if (idx == -1) {
            printf("No such user\n");
            return;
        }

        printf("$%d\n", bank->users[idx].balance);
        return;
    }

    // Any other command
    printf("Invalid command\n");
}

// Encrypt and send message
static int bank_send_encrypted(Bank *bank, const unsigned char *plaintext, size_t plaintext_len)
{
    unsigned char encrypted[MAX_ENCRYPTED_SIZE];
    unsigned char iv[16];
    unsigned char ciphertext[MAX_ENCRYPTED_SIZE];
    size_t ciphertext_len = 0;
    unsigned char hmac[32];
    
    // Encrypt the plaintext
    if (aes_encrypt(bank->key_K, plaintext, plaintext_len, 
                    ciphertext, &ciphertext_len, iv) != 0) {
        return -1;
    }
    
    // Build the packet: IV || ciphertext
    memcpy(encrypted, iv, 16);
    memcpy(encrypted + 16, ciphertext, ciphertext_len);
    size_t data_len = 16 + ciphertext_len;
    
    if (hmac_sha256(bank->key_K, encrypted, data_len, hmac) != 0) {
        return -1;
    }
    
    // Append HMAC
    memcpy(encrypted + data_len, hmac, 32);
    size_t total_len = data_len + 32;
    
    // Send to ATM via router
    ssize_t sent = bank_send(bank, (char*)encrypted, total_len);
    if (sent < 0 || (size_t)sent != total_len) {
        return -1;
    }
    
    return 0;
}

// Decrypt received message
static int bank_decrypt_message(Bank *bank, const unsigned char *encrypted, size_t encrypted_len,
                                 unsigned char *plaintext, size_t max_plaintext_len)
{
    if (encrypted_len < (16 + 32)) {  // Minimum: IV + HMAC
        return -1;
    }
    
    // Extract components
    const unsigned char *iv = encrypted;
    size_t data_len = encrypted_len - 32;  // Everything except HMAC
    const unsigned char *received_hmac = encrypted + data_len;
    
    if (hmac_verify(bank->key_K, encrypted, data_len, received_hmac) != 0) {
        return -1;
    }
    
    // Decrypt ciphertext
    const unsigned char *ciphertext = encrypted + 16;
    size_t ciphertext_len = data_len - 16;
    size_t plaintext_len = 0;
    
    if (aes_decrypt(bank->key_K, ciphertext, ciphertext_len, iv,
                    plaintext, &plaintext_len) != 0) {
        return -1;
    }
    
    if (plaintext_len > max_plaintext_len) {
        return -1;
    }
    
    return (int)plaintext_len;
}

void bank_process_remote_command(Bank *bank, char *command, size_t len)
{
    unsigned char plaintext[MAX_PLAINTEXT_SIZE];
    
    int plaintext_len = bank_decrypt_message(bank, (unsigned char*)command, len, 
                                             plaintext, sizeof(plaintext));
    if (plaintext_len < 0) {
        return;
    }
    
    // All messages have at least a header
    if (plaintext_len < (int)sizeof(msg_header_t)) {
        return;
    }
    
    msg_header_t *header = (msg_header_t*)plaintext;
    
    // Route based on message type
    switch (header->msg_type) {
        case MSG_LOGIN_REQ: {
            if (plaintext_len < (int)sizeof(msg_login_req_t)) {
                return;
            }
            
            msg_login_req_t *req = (msg_login_req_t*)plaintext;
            
            char username[USERNAME_SIZE + 1];
            memcpy(username, req->header.username, USERNAME_SIZE);
            username[USERNAME_SIZE] = '\0';
            
            for (int i = 0; i < USERNAME_SIZE; i++) {
                if (username[i] == '\0') break;
            }
            
            // Find user
            int user_idx = find_user(bank, username);
            if (user_idx == -1) {
                msg_login_resp_t resp;
                memset(&resp, 0, sizeof(resp));
                resp.header.msg_type = MSG_LOGIN_RESP;
                prepare_username(resp.header.username, username);
                resp.success = 0;
                resp.seq_num = req->seq_num;  // Echo back the sequence number
                
                bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
                return;
            }
            
            User *user = &bank->users[user_idx];
            uint64_t req_seq = ntohll(req->seq_num);
            
            // Replay protection
            if (req_seq <= user->last_seq) {
                msg_login_resp_t resp;
                memset(&resp, 0, sizeof(resp));
                resp.header.msg_type = MSG_LOGIN_RESP;
                prepare_username(resp.header.username, username);
                resp.success = 0;
                resp.seq_num = req->seq_num;
                
                bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
                return;
            }
            
            unsigned char expected_token[AUTH_TOKEN_SIZE];
            if (compute_auth_token(user->card_secret, user->pin, expected_token) != 0) {
                msg_login_resp_t resp;
                memset(&resp, 0, sizeof(resp));
                resp.header.msg_type = MSG_LOGIN_RESP;
                prepare_username(resp.header.username, username);
                resp.success = 0;
                resp.seq_num = req->seq_num;
                
                bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
                return;
            }
            
            volatile unsigned char tokens_match = 0;
            for (int i = 0; i < AUTH_TOKEN_SIZE; i++) {
                tokens_match |= (expected_token[i] ^ req->auth_token[i]);
            }
            
            if (tokens_match != 0) {
                msg_login_resp_t resp;
                memset(&resp, 0, sizeof(resp));
                resp.header.msg_type = MSG_LOGIN_RESP;
                prepare_username(resp.header.username, username);
                resp.success = 0;
                resp.seq_num = req->seq_num;
                
                bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
                return;
            }
            
            user->last_seq = req_seq;
            
            msg_login_resp_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.msg_type = MSG_LOGIN_RESP;
            prepare_username(resp.header.username, username);
            resp.success = 1;
            resp.seq_num = req->seq_num;
            
            bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
            break;
        }
        
        case MSG_BALANCE_REQ: {
            if (plaintext_len < (int)sizeof(msg_balance_req_t)) {
                return;
            }

            msg_balance_req_t *req = (msg_balance_req_t*)plaintext;

            char username[USERNAME_SIZE + 1];
            memcpy(username, req->header.username, USERNAME_SIZE);
            username[USERNAME_SIZE] = '\0';

            int user_idx = find_user(bank, username);
            if (user_idx == -1) {
                msg_balance_resp_t resp;
                memset(&resp, 0, sizeof(resp));
                resp.header.msg_type = MSG_BALANCE_RESP;
                prepare_username(resp.header.username, username);
                resp.balance = htonl(0);
                resp.seq_num = req->seq_num;
                bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
                return;
            }

            User *user = &bank->users[user_idx];
            uint64_t req_seq = ntohll(req->seq_num);

            if (req_seq <= user->last_seq) {
                msg_balance_resp_t resp;
                memset(&resp, 0, sizeof(resp));
                resp.header.msg_type = MSG_BALANCE_RESP;
                prepare_username(resp.header.username, username);
                resp.balance = htonl(user->balance);
                resp.seq_num = req->seq_num;
                bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
                return;
            }

            user->last_seq = req_seq;

            msg_balance_resp_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.msg_type = MSG_BALANCE_RESP;
            prepare_username(resp.header.username, username);
            resp.balance = htonl(user->balance);
            resp.seq_num = req->seq_num;
            bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
            break;
        }

        case MSG_WITHDRAW_REQ: {
            if (plaintext_len < (int)sizeof(msg_withdraw_req_t)) {
                return;
            }

            msg_withdraw_req_t *req = (msg_withdraw_req_t*)plaintext;

            char username[USERNAME_SIZE + 1];
            memcpy(username, req->header.username, USERNAME_SIZE);
            username[USERNAME_SIZE] = '\0';

            int user_idx = find_user(bank, username);
            if (user_idx == -1) {
                msg_withdraw_resp_t resp;
                memset(&resp, 0, sizeof(resp));
                resp.header.msg_type = MSG_WITHDRAW_RESP;
                prepare_username(resp.header.username, username);
                resp.success = 0;
                resp.new_balance = htonl(0);
                resp.seq_num = req->seq_num;
                bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
                return;
            }

            User *user = &bank->users[user_idx];
            uint64_t req_seq = ntohll(req->seq_num);

            if (req_seq <= user->last_seq) {
                msg_withdraw_resp_t resp;
                memset(&resp, 0, sizeof(resp));
                resp.header.msg_type = MSG_WITHDRAW_RESP;
                prepare_username(resp.header.username, username);
                resp.success = 0;
                resp.new_balance = htonl(user->balance);
                resp.seq_num = req->seq_num;
                bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
                return;
            }

            int32_t amount = ntohl(req->amount);
            uint8_t success = 0;

            if (amount >= 0 && amount <= user->balance) {
                user->balance -= amount;
                success = 1;
            }

            user->last_seq = req_seq;

            msg_withdraw_resp_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.msg_type = MSG_WITHDRAW_RESP;
            prepare_username(resp.header.username, username);
            resp.success = success;
            resp.new_balance = htonl(user->balance);
            resp.seq_num = req->seq_num;
            bank_send_encrypted(bank, (unsigned char*)&resp, sizeof(resp));
            break;
        }
            
        default:
            break;
    }
}
