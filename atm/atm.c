#include "atm.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

ATM* atm_create()
{
    ATM *atm = (ATM*) malloc(sizeof(ATM));
    if(atm == NULL)
    {
        perror("Could not allocate ATM");
        exit(1);
    }

    // Set up the network state (we'll use this later when we add real protocol)
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
    // Returns the number of bytes received; negative on error
    return recvfrom(atm->sockfd, data, max_data_len, 0, NULL, NULL);
}

// Helper: trim trailing newline from a string
static void trim_newline(char *s)
{
    size_t n = strlen(s);
    if (n > 0 && s[n-1] == '\n') {
        s[n-1] = '\0';
    }
}

// Helper: check that username is [a-zA-Z]+ and <= 250 chars
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

// Helper: check PIN is exactly 4 digits
static int is_valid_pin(const char *pin)
{
    if (strlen(pin) != 4) return 0;
    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)pin[i])) return 0;
    }
    return 1;
}

// Helper: check amt is [0-9]+ and fits into int (non-negative)
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

        // In the final version, we'll ask the bank if the user exists.
        // For now, approximate "No such user" by checking for a card file.
        char card_filename[300];
        snprintf(card_filename, sizeof(card_filename), "%s.card", user);

        FILE *cf = fopen(card_filename, "r");
        if (cf == NULL) {
            // The spec distinguishes "No such user" vs "Unable to access...".
            // Without talking to the bank yet, we just treat this as card error.
            printf("Unable to access %s's card\n", user);
            return;
        }
        // Later we will read card contents (card_secret, etc.) here.
        fclose(cf);

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

        // In the final version, we'll actually authenticate with the bank
        // using Idea 1 (card_secret + PIN + encrypted channel).
        // For now, any 4-digit PIN is "Authorized".
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

        // In the final version, we'll ask the bank if funds are sufficient.
        // For now, pretend there are always enough funds and dispense.
        printf("$%d dispensed\n", amt);
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

        // In the final version, we'll query the bank for the real balance.
        // For now, report a dummy balance of 0.
        printf("$0\n");
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
