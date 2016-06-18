#include <stdio.h>

#include "sooshi.h"

void battery_update(SooshiState *state, SooshiNode *node)
{
    float voltage = g_variant_get_double(node->value);
    printf("Battery Percent: %.2f\n", (voltage - 2.0) * 100.0);
}

void channel1_update(SooshiState *state, SooshiNode *node)
{
    //printf("Channel 1: %.2f\n", g_variant_get_double(node->value));
}

void channel2_update(SooshiState *state, SooshiNode *node)
{
    printf("Channel 2: %f\n", g_variant_get_double(node->value));
}


void mooshi_initialized(SooshiState *state)
{
    sooshi_node_subscribe(state, sooshi_node_find(state, "BAT_V", NULL), battery_update);
    sooshi_node_subscribe(state, sooshi_node_find(state, "CH1:VALUE", NULL), channel1_update);
    sooshi_node_subscribe(state, sooshi_node_find(state, "CH2:VALUE", NULL), channel2_update);

    sooshi_node_choose(state, sooshi_node_find(state, "SAMPLING:TRIGGER:CONTINUOUS", NULL));
    sooshi_node_choose(state, sooshi_node_find(state, "CH2:MAPPING:VOLTAGE:60", NULL));
}

int main()
{
    g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK, g_log_default_handler, NULL);

    SooshiState *state;
    state = sooshi_state_new();
    sooshi_run(state, mooshi_initialized);
    sooshi_state_delete(state);

    return 0;
}
