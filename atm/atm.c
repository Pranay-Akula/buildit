#include "atm.h"
#include "ports.h"
#include "protocol.h"
#include "crypto.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

ATM* atm_create(const char *atm_init_file)
{
    ATM *atm = (ATM*) malloc(sizeof(ATM));
    if(atm == NULL)
    {
        perror("Could not allocate ATM");
        exit(1);
    }

    // Set up the network state
    atm->sockfd = socket(AF_INET,SOCK_DGRAM,0);

    bzero(&atm->rtr_addr,sizeof(atm->rtr_addr));
    atm->rtr_addr.sin_family = AF_INET;
    atm->rtr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    atm->rtr_addr.sin_port = htons(ROUTER_PORT);

    bzero(&atm->atm_addr, sizeof(atm->atm_addr));
    atm->atm_addr.sin_family = AF_INET;
    atm->atm_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    atm->atm_addr.sin_port = htons(ATM_PORT);
    bind(atm->sockfd,(struct sockaddr *)&atm->atm_addr,sizeof(atm->atm_addr));

    // Initialize protocol / session state
    atm->logged_in = 0;
    atm->current_user[0] = '\0';
    
    atm->seq = 1;
    atm->key_loaded = 0;
    memset(atm->key_K, 0, KEY_SIZE);
    memset(atm->card_secret, 0, CARD_SECRET_SIZE);
    
    FILE *key_file = fopen(atm_init_file, "rb");
    if (key_file == NULL) {
        printf("Error opening ATM initialization file\n");
        free(atm);
        exit(64);
    }
    
    size_t bytes_read = fread(atm->key_K, 1, KEY_SIZE, key_file);
    fclose(key_file);
    
    if (bytes_read != KEY_SIZE) {
        printf("Error opening ATM initialization file\n");
        free(atm);
        exit(64);
    }
    
    atm->key_loaded = 1;

    return atm;
}

void atm_free(ATM *atm)
{
    if(atm != NULL)
    {
        close(atm->sockfd);
        free(atm);
    }
}

ssize_t atm_send(ATM *atm, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    return sendto(atm->sockfd, data, data_len, 0,
                  (struct sockaddr*) &atm->rtr_addr, sizeof(atm->rtr_addr));
}

ssize_t atm_recv(ATM *atm, char *data, size_t max_data_len)
{
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(atm->sockfd, &readfds);
    
    int result = select(atm->sockfd + 1, &readfds, NULL, NULL, &tv);
    if (result <= 0) {
        return -1;
    }
    
    // Receive data
    return recvfrom(atm->sockfd, data, max_data_len, 0, NULL, NULL);
}

static void trim_newline(char *s)
{
    size_t n = strlen(s);
    if (n > 0 && s[n-1] == '\n') {
        s[n-1] = '\0';
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

    // Use strtol to detect overflow
    char *endptr = NULL;
    long val = strtol(s, &endptr, 10);
    if (*endptr != '\0') return 0;
    if (val < 0 || val > INT_MAX) return 0;

    *out = (int)val;
    return 1;
}

// Encrypt and send message
static int atm_send_encrypted(ATM *atm, const unsigned char *plaintext, size_t plaintext_len)
{
    unsigned char encrypted[MAX_ENCRYPTED_SIZE];
    unsigned char iv[16];
    unsigned char ciphertext[MAX_ENCRYPTED_SIZE];
    size_t ciphertext_len = 0;
    unsigned char hmac[32];
    
    if (aes_encrypt(atm->key_K, plaintext, plaintext_len, 
                    ciphertext, &ciphertext_len, iv) != 0) {
        return -1;
    }
    
    // Build the packet: IV || ciphertext
    memcpy(encrypted, iv, 16);
    memcpy(encrypted + 16, ciphertext, ciphertext_len);
    size_t data_len = 16 + ciphertext_len;
    
    // HMAC over IV + ciphertext
    if (hmac_sha256(atm->key_K, encrypted, data_len, hmac) != 0) {
        return -1;
    }
    
    // Append HMAC
    memcpy(encrypted + data_len, hmac, 32);
    size_t total_len = data_len + 32;
    
    // Send to bank via router
    ssize_t sent = atm_send(atm, (char*)encrypted, total_len);
    if (sent < 0 || (size_t)sent != total_len) {
        return -1;
    }
    
    return 0;
}

// Receive and decrypt message
static int atm_recv_encrypted(ATM *atm, unsigned char *plaintext, size_t max_plaintext_len)
{
    unsigned char encrypted[MAX_ENCRYPTED_SIZE];
    
    // Receive encrypted packet
    ssize_t recv_len = atm_recv(atm, (char*)encrypted, sizeof(encrypted));
    if (recv_len < (16 + 32)) {  // Minimum: IV + HMAC
        return -1;
    }
    
    // Extract components
    unsigned char *iv = encrypted;
    size_t data_len = recv_len - 32;  // Everything except HMAC
    unsigned char *received_hmac = encrypted + data_len;
    
    if (hmac_verify(atm->key_K, encrypted, data_len, received_hmac) != 0) {
        return -1;
    }
    
    // Decrypt ciphertext
    unsigned char *ciphertext = encrypted + 16;
    size_t ciphertext_len = data_len - 16;
    size_t plaintext_len = 0;
    
    if (aes_decrypt(atm->key_K, ciphertext, ciphertext_len, iv,
                    plaintext, &plaintext_len) != 0) {
        return -1;
    }
    
    if (plaintext_len > max_plaintext_len) {
        return -1;
    }
    
    return (int)plaintext_len;
}

void atm_process_command(ATM *atm, char *command)
{
    // command comes from fgets in atm-main, so it's null-terminated
    trim_newline(command);

    // Tokenize
    char *cmd = strtok(command, " \t");
    if (cmd == NULL) {
        // Empty line: do nothing
        return;
    }

    // BEGIN-SESSION
    if (strcmp(cmd, "begin-session") == 0)
    {
        char *user = strtok(NULL, " \t");
        char *extra = strtok(NULL, " \t");

        // If someone is already logged in
        if (atm->logged_in) {
            printf("A user is already logged in\n");
            return;
        }

        // Invalid inputs: wrong number of arguments or invalid username
        if (user == NULL || extra != NULL || !is_valid_username(user)) {
            printf("Usage: begin-session <user-name>\n");
            return;
        }

        // Read card file to get card_secret
        char card_filename[300];
        snprintf(card_filename, sizeof(card_filename), "%s.card", user);

        FILE *cf = fopen(card_filename, "rb");
        if (cf == NULL) {
            printf("Unable to access %s's card\n", user);
            return;
        }
        
        size_t bytes_read = fread(atm->card_secret, 1, CARD_SECRET_SIZE, cf);
        fclose(cf);
        
        if (bytes_read != CARD_SECRET_SIZE) {
            printf("Unable to access %s's card\n", user);
            return;
        }

        // Prompt for PIN
        printf("PIN? ");
        fflush(stdout);

        char pinbuf[100];
        if (fgets(pinbuf, sizeof(pinbuf), stdin) == NULL) {
            printf("Not authorized\n");
            return;
        }
        trim_newline(pinbuf);

        if (!is_valid_pin(pinbuf)) {
            printf("Not authorized\n");
            return;
        }

        // Compute auth_token = HMAC(card_secret || PIN)
        unsigned char auth_token[AUTH_TOKEN_SIZE];
        if (compute_auth_token(atm->card_secret, pinbuf, auth_token) != 0) {
            printf("Not authorized\n");
            return;
        }

        // Build login request message
        msg_login_req_t login_req;
        memset(&login_req, 0, sizeof(login_req));
        login_req.header.msg_type = MSG_LOGIN_REQ;
        prepare_username(login_req.header.username, user);
        memcpy(login_req.auth_token, auth_token, AUTH_TOKEN_SIZE);
        memcpy(login_req.pin, pinbuf, PIN_SIZE);
        login_req.seq_num = htonll(atm->seq);

        // Send encrypted login request
        if (atm_send_encrypted(atm, (unsigned char*)&login_req, sizeof(login_req)) != 0) {
            printf("Not authorized\n");
            return;
        }
        
        atm->seq++;

        // Receive encrypted login response
        unsigned char response_buf[MAX_PLAINTEXT_SIZE];
        int response_len = atm_recv_encrypted(atm, response_buf, sizeof(response_buf));
        
        if (response_len < (int)sizeof(msg_login_resp_t)) {
            printf("Not authorized\n");
            return;
        }

        // Parse login response
        msg_login_resp_t *login_resp = (msg_login_resp_t*)response_buf;
        
        // Verify message type
        if (login_resp->header.msg_type != MSG_LOGIN_RESP) {
            printf("Not authorized\n");
            return;
        }
        
        uint64_t resp_seq = ntohll(login_resp->seq_num);
        if (resp_seq != atm->seq - 1) {
            printf("Not authorized\n");
            return;
        }

        if (login_resp->success != 1) {
            printf("Not authorized\n");
            return;
        }

        printf("Authorized\n");
        atm->logged_in = 1;
        strncpy(atm->current_user, user, sizeof(atm->current_user));
        atm->current_user[sizeof(atm->current_user)-1] = '\0';
        return;
    }

    // WITHDRAW
    if (strcmp(cmd, "withdraw") == 0)
    {
        char *amt_str = strtok(NULL, " \t");
        char *extra = strtok(NULL, " \t");

        if (!atm->logged_in) {
            printf("No user logged in\n");
            return;
        }

        if (amt_str == NULL || extra != NULL) {
            printf("Usage: withdraw <amt>\n");
            return;
        }

        int amt = 0;
        if (!parse_amount(amt_str, &amt)) {
            printf("Usage: withdraw <amt>\n");
            return;
        }

        // Build withdraw request
        msg_withdraw_req_t req;
        memset(&req, 0, sizeof(req));
        req.header.msg_type = MSG_WITHDRAW_REQ;
        prepare_username(req.header.username, atm->current_user);
        req.amount = htonl(amt);
        req.seq_num = htonll(atm->seq);

        // Send encrypted request
        if (atm_send_encrypted(atm, (unsigned char*)&req, sizeof(req)) != 0) {
            return;
        }
        atm->seq++;

        // Receive encrypted response
        unsigned char resp_buf[MAX_PLAINTEXT_SIZE];
        int resp_len = atm_recv_encrypted(atm, resp_buf, sizeof(resp_buf));
        if (resp_len < (int)sizeof(msg_withdraw_resp_t)) {
            return;
        }

        msg_withdraw_resp_t *resp = (msg_withdraw_resp_t*)resp_buf;
        if (resp->header.msg_type != MSG_WITHDRAW_RESP) {
            return;
        }

        uint64_t resp_seq = ntohll(resp->seq_num);
        if (resp_seq != atm->seq - 1) {
            return;
        }

        if (resp->success == 1) {
            printf("$%d dispensed\n", amt);
        } else {
            printf("Insufficient funds\n");
        }
        return;
    }

    // BALANCE
    if (strcmp(cmd, "balance") == 0)
    {
        char *extra = strtok(NULL, " \t");

        if (!atm->logged_in) {
            printf("No user logged in\n");
            return;
        }

        if (extra != NULL) {
            printf("Usage: balance\n");
            return;
        }

        // Build balance request
        msg_balance_req_t req;
        memset(&req, 0, sizeof(req));
        req.header.msg_type = MSG_BALANCE_REQ;
        prepare_username(req.header.username, atm->current_user);
        req.seq_num = htonll(atm->seq);

        // Send encrypted request
        if (atm_send_encrypted(atm, (unsigned char*)&req, sizeof(req)) != 0) {
            return;
        }
        atm->seq++;

        // Receive encrypted response
        unsigned char resp_buf[MAX_PLAINTEXT_SIZE];
        int resp_len = atm_recv_encrypted(atm, resp_buf, sizeof(resp_buf));
        if (resp_len < (int)sizeof(msg_balance_resp_t)) {
            return;
        }

        msg_balance_resp_t *resp = (msg_balance_resp_t*)resp_buf;
        if (resp->header.msg_type != MSG_BALANCE_RESP) {
            return;
        }

        uint64_t resp_seq = ntohll(resp->seq_num);
        if (resp_seq != atm->seq - 1) {
            return;
        }

        int32_t balance = ntohl(resp->balance);
        printf("$%d\n", balance);
        return;
    }

    // END-SESSION
    if (strcmp(cmd, "end-session") == 0)
    {
        char *extra = strtok(NULL, " \t");

        if (!atm->logged_in) {
            printf("No user logged in\n");
            return;
        }

        if (extra != NULL) {
            // Technically invalid usage, but spec does not give a usage string.
            // We'll just ignore extra tokens and log out.
        }

        atm->logged_in = 0;
        atm->current_user[0] = '\0';
        printf("User logged out\n");
        return;
    }

    // Any other command
    printf("Invalid command\n");
}
