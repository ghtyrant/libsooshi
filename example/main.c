#include <stdio.h>

#include "sooshi.h"

void battery_update(SooshiState *state, SooshiNode *node, gpointer user_data)
{
    float voltage = g_variant_get_double(node->value);
    printf("Battery Percent: %.2f\n", (voltage - 2.0) * 100.0);
}

void channel1_update(SooshiState *state, SooshiNode *node, gpointer user_data)
{
    //printf("Channel 1: %.2f\n", g_variant_get_double(node->value));
}

void channel2_update(SooshiState *state, SooshiNode *node, gpointer user_data)
{
    static gint counter = 0;
    counter++;
    printf("Channel 2: %f\n", g_variant_get_double(node->value));

    if (counter == 2)
    {
        sooshi_stop(state);
        sooshi_state_delete(state);
    }
}

void mooshi_initialized(SooshiState *state, gpointer user_data)
{
    g_info("Mooshimeter initialized, starting subscribers!");

    //sooshi_node_subscribe(state, sooshi_node_find(state, "BAT_V", NULL), battery_update, NULL);
    //sooshi_node_subscribe(state, sooshi_node_find(state, "CH1:VALUE", NULL), channel1_update, NULL);
    sooshi_node_subscribe(state, sooshi_node_find(state, "CH2:VALUE", NULL), channel2_update, NULL);

    sooshi_node_choose(state, sooshi_node_find(state, "SAMPLING:TRIGGER:CONTINUOUS", NULL));
    sooshi_node_choose(state, sooshi_node_find(state, "SAMPLING:RATE:1000", NULL));
    //sooshi_node_choose(state, sooshi_node_find(state, "CH2:MAPPING:TEMP:350", NULL));
}

void bt_scan_timeout(SooshiState *state, gpointer user_data)
{
    g_info("Scanning timed out!");
    sooshi_stop(state);
}

int main()
{
    g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK, g_log_default_handler, NULL);

    sooshi_error_t error = 0;
    SooshiState *state = sooshi_state_new(&error);

    if (error != SOOSHI_ERROR_SUCCESS)
    {
        g_error(sooshi_error_message(error));
        return -1;
    }

    error = sooshi_setup(state, mooshi_initialized, NULL,
                                bt_scan_timeout, NULL);

    if (error != SOOSHI_ERROR_SUCCESS)
    {
        g_error(sooshi_error_message(error));
        return -1;
    }

    sooshi_run(state);

    return 0;
}
