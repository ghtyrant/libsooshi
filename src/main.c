#include <stdio.h>

#include "sooshi.h"

void handler(SooshiState *state)
{

}

int main()
{
    g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK, g_log_default_handler, NULL);

    SooshiState *state;
    state = sooshi_state_new();

    // Let's see if the Mooshimeter has already been scanned
    if (!sooshi_find_mooshi(state))
    {
        g_warning("Could not find Mooshimeter!");

        // We couldn't find it, let's scan
        if (!sooshi_find_adapter(state))
        {
            g_warning("Could not find bluetooth adapter!");
            sooshi_state_delete(state);
            return -1;
        }

        g_info("Found bluetooth adapter, ready to scan!");

        if (!sooshi_start_scan(state))
        {
            g_warning("Error starting bluetooth scan!");
            sooshi_state_delete(state);
            return -1;
        }
    }
    else
        sooshi_connect_mooshi(state);

    sooshi_start(state, handler);

    sooshi_test(state);
    
    sooshi_state_delete(state);

    return 0;
}
