#include "bank.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

Bank* bank_create()
{
    Bank *bank = (Bank*) malloc(sizeof(Bank));
    if(bank == NULL)
    {
        perror("Could not allocate Bank");
        exit(1);
    }

    // Set up the network state (for later when we add remote protocol)
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
    return sendto(bank->sockfd, data, data_len, 0,
                  (struct sockaddr*) &bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
}

// Helper: trim trailing whitespace/newline from a buffer (in-place)
static void trim_buffer(char *buf)
{
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' ||
                     buf[n-1] == ' '  || buf[n-1] == '\t')) {
        buf[n-1] = '\0';
        n--;
    }
}

// Helper: check username is [a-zA-Z]+ and <= 250 chars
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

// Helper: PIN is exactly 4 digits
static int is_valid_pin(const char *pin)
{
    if (strlen(pin) != 4) return 0;
    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)pin[i])) return 0;
    }
    return 1;
}

// Helper: parse amount as non-negative int, detect overflow
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

// Helper: find user index by name, or -1 if not found
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

        // Create card file first (so if it fails, we don't modify state)
        char card_filename[300];
        snprintf(card_filename, sizeof(card_filename), "%s.card", user);
        FILE *cf = fopen(card_filename, "w");
        if (cf == NULL) {
            printf("Error creating card file for user %s\n", user);
            return;
        }

        // For now, the card file just stores the username.
        // Later we'll add card_secret, etc.
        fprintf(cf, "%s\n", user);
        fclose(cf);

        // Now update bank state
        User *u = &bank->users[bank->num_users++];
        strncpy(u->username, user, sizeof(u->username));
        u->username[sizeof(u->username)-1] = '\0';
        strncpy(u->pin, pin, sizeof(u->pin));
        u->pin[sizeof(u->pin)-1] = '\0';
        u->balance = balance;

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

        // Check overflow: if users[idx].balance + amt > INT_MAX
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
        char user[256];
        int num = sscanf(p, "%*s %255s", user);
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

void bank_process_remote_command(Bank *bank, char *command, size_t len)
{
    // TODO: Implement the bank side of the ATM-bank protocol using Idea 1
    // For now, we leave this unimplemented so that you can focus
    // on the local bank commands and ATM session logic first.

    (void)bank;
    (void)command;
    (void)len;

    // Example placeholder (do nothing).
    // When you implement Idea 1, you'll:
    //  - decrypt & verify MAC on 'command'
    //  - parse message type (login, balance, withdraw)
    //  - check accounts and respond via bank_send()
}
