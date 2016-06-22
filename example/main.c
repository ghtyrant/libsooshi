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

    if (counter == 200)
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

int main()
{
    g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK, g_log_default_handler, NULL);

    SooshiState *state;
    state = sooshi_state_new();
    sooshi_setup(state, mooshi_initialized, NULL);
    sooshi_run(state);

    return 0;
}
