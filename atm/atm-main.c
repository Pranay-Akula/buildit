// ATM main loop

#include "atm.h"
#include <stdio.h>
#include <stdlib.h>

static const char prompt[] = "ATM: ";

int main(int argc, char **argv)
{
    char user_input[1000];

    // Check command line arguments
    if (argc != 2) {
        printf("Error opening ATM initialization file\n");
        return 64;
    }

    ATM *atm = atm_create(argv[1]);

    printf("%s", prompt);
    fflush(stdout);

    while (fgets(user_input, 10000,stdin) != NULL)
    {
        atm_process_command(atm, user_input);
        
        // Update prompt based on login state
        if (atm->logged_in) {
            printf("ATM (%s):  ", atm->current_user);
        } else {
            printf("%s", prompt);
        }
        fflush(stdout);
    }
	return EXIT_SUCCESS;
}
