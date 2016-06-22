#include <glib.h>
#include <string.h>

#include "sooshi.h"

SooshiNode *
sooshi_node_find(SooshiState *state, gchar *path, SooshiNode *start)
{
    if (!start)
       start = state->root_node; 
    
    gchar *sep = g_strstr_len(path, -1, ":");
    gulong node_name_len = strlen(path);

    if (sep)
        node_name_len = sep - path;

    GList *elem;
    SooshiNode *item;
    for(elem = start->children; elem; elem = elem->next)
    {
        item = elem->data;

        if (strncmp(path, item->name, node_name_len) == 0)
        {
            if (sep == NULL)
                return item;
            else
                return sooshi_node_find(state, sep + 1, item);
        }
    }

    return NULL;
}

void
sooshi_node_set_value(SooshiState *state, SooshiNode *node, GVariant *value, gboolean send_update)
{
    g_return_if_fail(state != NULL);
    g_return_if_fail(node != NULL);

    if (node->value)
    {
        g_variant_unref(node->value);
        node->value = NULL;
    }

    switch(node->type)
    {
        case VAL_U8:
        case CHOOSER:
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BYTE))
            {
                g_error("Wrong datatype for node '%s', expected byte, got %s", node->name, g_variant_get_type_string(value));
                return;
            }

            node->value = value;
            break;

        case VAL_U16:
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16))
            {
                g_error("Wrong datatype for node '%s', expected uint16, got %s", node->name, g_variant_get_type_string(value));
                return;
            }

            node->value = value;
            break;

        case VAL_U32:
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
            {
                g_error("Wrong datatype for node '%s', expected uint32, got %s", node->name, g_variant_get_type_string(value));
                return;
            }

            node->value = value;
            break;

        case VAL_S8:
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BYTE))
            {
                g_error("Wrong datatype for node '%s', expected byte, got %s", node->name, g_variant_get_type_string(value));
                return;
            }

            node->value = value;
            break;

        case VAL_S16:
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_INT16))
            {
                g_error("Wrong datatype for node '%s', expected int16, got %s", node->name, g_variant_get_type_string(value));
                return;
            }

            node->value = value;
            break;

        case VAL_S32:
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_INT32))
            {
                g_error("Wrong datatype for node '%s', expected int32, got %s", node->name, g_variant_get_type_string(value));
                return;
            }

            node->value = value;
            break;

        case VAL_STR:
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING))
            {
                g_error("Wrong datatype for node '%s', expected string, got %s", node->name, g_variant_get_type_string(value));
                return;
            }

            node->value = value;
            break;

        case VAL_BIN:
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BYTESTRING))
            {
                g_error("Wrong datatype for node '%s', expected bytestring, got %s", node->name, g_variant_get_type_string(value));
                return;
            }

            node->value = value;
            break;

        case VAL_FLT:
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_DOUBLE))
            {
                g_error("Wrong datatype for node '%s', expected double, got %s", node->name, g_variant_get_type_string(value));
                return;
            }

            node->value = value;
            break;

        default:
            g_error("Unsupported data type in sooshi_node_set_value(): %d", node->type);
            return;
    }

    if (send_update)
        sooshi_node_send_value(state, node);
}

gint
sooshi_node_value_to_bytes(SooshiNode *node, guchar *buffer)
{
    gsize len;
    const gchar* tmp;
    guint16 u16;
    guint32 u32;
    gint16 s16;
    gint32 s32;
    float flt;

    switch(node->type)
    {
        case VAL_U8:
        case CHOOSER:
            buffer[0] = g_variant_get_byte(node->value); return 1;

        case VAL_U16:
            u16 = g_variant_get_uint16(node->value);
            memcpy(buffer, (gchar*)&u16, 2); return 2;

        case VAL_U32:
            u32 = g_variant_get_uint32(node->value);
            memcpy(buffer, (gchar*)&u32, 4); return 4;

        case VAL_S8:
            buffer[0] = g_variant_get_byte(node->value); return 1;

        case VAL_S16:
            s16 = g_variant_get_int16(node->value);
            memcpy(buffer, (gchar*)&s16, 2); return 2;

        case VAL_S32:
            s32 = g_variant_get_int32(node->value);
            memcpy(buffer, (gchar*)&s32, 4); return 4;

        case VAL_STR:
            tmp = g_variant_get_string(node->value, &len); 
            memcpy(buffer, tmp, len); return len;

        case VAL_FLT:
            flt = g_variant_get_double(node->value);
            memcpy(buffer, (gchar*)&flt, 4); return 4;

        default:
            g_error("Unsupported data type in sooshi_node_value_to_bytes(): %d", node->type);
            break;
    }

    return 0;
}

GByteArray *
sooshi_node_bytes_to_value(SooshiNode *node, GByteArray *buffer, GVariant **result)
{
    gchar* tmp;
    guint16 u16;
    guint32 u32;
    gint16 s16;
    gint32 s32;
    float flt;

    // buffer->data[0] still contains the op code
    switch(node->type)
    {
        case VAL_U8:
        case CHOOSER:
            *result = g_variant_new_byte(buffer->data[1]);
            return g_byte_array_remove_range(buffer, 0, 2);

        case VAL_U16:
            u16 = buffer->data[1] | (guint16)buffer->data[2] << 8;
            *result = g_variant_new_uint16(u16);
            return g_byte_array_remove_range(buffer, 0, 3);

        case VAL_U32:
            u32 = buffer->data[1] | (guint32)buffer->data[2] << 8
                | (guint32)buffer->data[3] << 16 | (guint32)buffer->data[4] << 24;
            *result = g_variant_new_uint32(u32);
            return g_byte_array_remove_range(buffer, 0, 5);

        case VAL_S8:
            *result = g_variant_new_byte((gchar)buffer->data[1]);
            return g_byte_array_remove_range(buffer, 0, 2);

        case VAL_S16:
            s16 = buffer->data[1] | (gint16)buffer->data[2] << 8;
            *result = g_variant_new_int16(s16);
            return g_byte_array_remove_range(buffer, 0, 3);

        case VAL_S32:
            s32 = buffer->data[1] | (gint32)buffer->data[2] << 8
                | (gint32)buffer->data[3] << 16 | (gint32)buffer->data[4] << 24;
            *result = g_variant_new_int32(s32);
            return g_byte_array_remove_range(buffer, 0, 5);

        case VAL_STR:
            u16 = buffer->data[1] | (guint16)buffer->data[2] << 8;

            if (buffer->len < u16)
            {
                *result = NULL;
                return buffer;
            }

            tmp = g_strndup((gchar*)buffer->data + 3, u16);
            *result = g_variant_new_string(tmp);
            g_free(tmp);
            return g_byte_array_remove_range(buffer, 0, u16 + 3);

        case VAL_BIN:
            u16 = buffer->data[1] | (guint16)buffer->data[2] << 8;

            if (buffer->len < u16)
            {
                *result = NULL;
                return buffer;
            }

            tmp = g_strndup((gchar*)buffer->data + 3, u16);
            *result = g_variant_new_bytestring(tmp);
            g_free(tmp);
            return g_byte_array_remove_range(buffer, 0, u16 + 3);

        case VAL_FLT:
            ((guchar*)&flt)[0] = buffer->data[1];
            ((guchar*)&flt)[1] = buffer->data[2];
            ((guchar*)&flt)[2] = buffer->data[3];
            ((guchar*)&flt)[3] = buffer->data[4];
            *result = g_variant_new_double(flt);
            return g_byte_array_remove_range(buffer, 0, 5);

        default:
            g_error("Unsupported data type in sooshi_node_bytes_to_value(): %d", node->type);
            return NULL;
    }
}

void
sooshi_node_request_value(SooshiState *state, SooshiNode *node)
{
    sooshi_send_bytes(state, &node->op_code, 1, TRUE);
}

void
sooshi_node_choose(SooshiState *state, SooshiNode *node)
{
    g_return_if_fail(state != NULL);
    g_return_if_fail(node != NULL);
    gint index = g_list_index(node->parent->children, node);

    sooshi_node_set_value(state, node->parent, g_variant_new_byte((guchar)index), TRUE);
}

guint
sooshi_node_subscribe(SooshiState *state, SooshiNode *node, sooshi_node_subscriber_handler_t func, gpointer user_data)
{
    g_info("Subscribing to node '%s'", node->name);

    SooshiNodeSubscriber *sub = g_new0(SooshiNodeSubscriber, 1);
    sub->id = g_list_length(node->subscriber) + 1;
    sub->handler = func;
    sub->user_data = user_data;
    node->subscriber = g_list_append(node->subscriber, (gpointer)sub);

    return sub->id;
}

void
sooshi_node_notify_subscribers(SooshiState *state, SooshiNode *node)
{
    if (node->subscriber == NULL)
        return;

    GList *elem;
    for(elem = node->subscriber; elem; elem = elem->next)
    {
        SooshiNodeSubscriber *sub = (SooshiNodeSubscriber*)elem->data;
        ((sooshi_node_subscriber_handler_t)sub->handler)(state, node, sub->user_data);
    }
}

void
sooshi_request_all_node_values(SooshiState *state, SooshiNode *start)
{
    if (start == NULL)
        start = state->root_node;

    // Only request nodes that can have a value and don't request the ADMIN nodes again
    if (start->has_value == TRUE && start->op_code >= 3)
        sooshi_node_request_value(state, start);

    GList *elem;
    SooshiNode *item;
    for(elem = start->children; elem; elem = elem->next)
    {
        item = elem->data;
        sooshi_request_all_node_values(state, item);
    }
}

void
sooshi_node_free_all(SooshiState *state, SooshiNode *start_node)
{
    if (start_node == NULL)
    {
	if (state->root_node == NULL)
	    return;

        start_node = state->root_node;
    }

    if (start_node->value) g_variant_unref(start_node->value);
    g_free(start_node->name);

    GList *elem;
    SooshiNode *item;
    for(elem = start_node->children; elem; elem = elem->next)
    {
        item = elem->data;
        sooshi_node_free_all(state, item);
    }

    g_list_free_full(start_node->children, g_free);
    g_list_free_full(start_node->subscriber, g_free);
    start_node->subscriber = NULL;

    state->root_node = NULL;
}
