#include <stdio.h>
#include <string.h>
#include "sooshi.h"

gchar*
sooshi_node_value_as_string(SooshiNode *node)
{
    if (node->value)
    {
        switch (node->type)
        {
            case CHOOSER: return g_strdup_printf("%u", g_variant_get_byte(node->value));
            case VAL_U8:  return g_strdup_printf("%u", g_variant_get_byte(node->value));
            case VAL_U16: return g_strdup_printf("%u", g_variant_get_uint16(node->value));
            case VAL_U32: return g_strdup_printf("%u", g_variant_get_uint32(node->value));
            case VAL_S8:  return g_strdup_printf("%d", g_variant_get_byte(node->value));
            case VAL_S16: return g_strdup_printf("%d", g_variant_get_int16(node->value));
            case VAL_S32: return g_strdup_printf("%d", g_variant_get_int32(node->value));
            case VAL_STR: return g_strdup_printf("'%s'", g_variant_get_string(node->value, NULL));
            case VAL_FLT: return g_strdup_printf("%f", g_variant_get_double(node->value));
            default: break;
        }
    }

    return g_strdup("");
}

void
sooshi_debug_dump_tree(SooshiNode *node, gint indent)
{
    gchar *node_value = sooshi_node_value_as_string(node);

    printf("%*s %d [%s] %s\n",
            (gint)(indent + strlen(node->name)), node->name,
            node->op_code,
            SOOSHI_NODE_TYPE_TO_STR(node->type),
            node_value);

    g_free(node_value);

    GList *elem;
    SooshiNode *item;
    for(elem = node->children; elem; elem = elem->next)
    {
        item = elem->data;
        sooshi_debug_dump_tree(item, indent + 4);
    }
}

