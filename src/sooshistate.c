#include <stdio.h>
#include <zlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "sooshistate.h"

G_DEFINE_TYPE(SooshiState, sooshi_state, G_TYPE_OBJECT)

const gchar *const SOOSHI_NODE_TYPE_STR[] =
{
    "NOTSET",
    "PLAIN",
    "LINK",
    "CHOOSER",
    "VAL_U8",
    "VAL_U16",
    "VAL_U32",
    "VAL_S8",
    "VAL_S16",
    "VAL_S32",
    "VAL_STR",
    "VAL_BIN",
    "VAL_FLT"
};


static void sooshi_state_class_init(SooshiStateClass *class)
{
}

static void sooshi_state_init(SooshiState *state)
{
    g_signal_new("mooshimeter-connected",
            SOOSHI_TYPE_STATE,
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            0);

    state->buffer = g_byte_array_new();
    state->send_sequence = 0;
    state->recv_sequence = 0;

    state->op_code_map = g_array_new(FALSE, FALSE, sizeof(SooshiNode*));

    sooshi_crc32_init(state);
}

static void
sooshi_on_object_added(GDBusObjectManager *objman, GDBusObject *obj, gpointer user_data)
{
    SooshiState *state = (SooshiState*)user_data;

    g_info("Found device %s ...", g_dbus_object_get_object_path(obj));
    GDBusInterface *inter = g_dbus_object_get_interface(obj, BLUEZ_DEVICE_INTERFACE);

    if (!inter)
        return;

    GVariant *v_name = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(inter), "Name");
    const gchar *name = g_variant_get_string(v_name, NULL);

    const gchar *uuid = METER_SERVICE_UUID;
    if (sooshi_cond_is_mooshimeter(inter, (gpointer)uuid))
    {
        sooshi_add_mooshimeter(state, G_DBUS_PROXY(inter));
        g_info("Found device '%s', looks like a Mooshimeter!", name);
        sooshi_stop_scan(state);
        sooshi_connect_mooshi(state);
    }
    else
    {
        g_info("Found device '%s', but it's not the droid we are looking for ...", name);
    }

    g_variant_unref(v_name);
}

static void
sooshi_on_serial_out_ready(GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer user_data)
{
    SooshiState *state = (SooshiState*)user_data;

    GVariantDict *dict = g_variant_dict_new(changed_properties);

    if (g_variant_dict_contains(dict, "Value"))
        g_info("Value has been updated!");

    GVariant *value = g_variant_dict_lookup_value(dict, "Value", G_VARIANT_TYPE("ay"));

    if (value)
    {
        //g_info("Just read: %d", g_variant_get_byte(value));

        g_debug("Reading value ...");
        GVariantIter *iter = g_variant_iter_new(value);
        guint8 buf[20];
        gchar tmp;
        gint i = 0;
        while (g_variant_iter_loop(iter, "y", &tmp))
        {
            if (i == 0)
                g_debug("Message Sequence: %d", tmp);
            else
            {
                buf[i-1] = tmp;
                g_debug("    [%d:%d] %x (%c, %d)", i, state->buffer->len + (i - 1), buf[i-1], buf[i-1], buf[i-1]);
            }
            i++;
        }
        g_variant_unref(value);

        g_byte_array_append(state->buffer, buf, i - 1);
        sooshi_parse_response(state);
    }

    g_variant_dict_unref(dict);
}

void sooshi_parse_response(SooshiState *state)
{
    guint8 op_code = state->buffer->data[0];
    guint16 length = 0;

    if (op_code == 1)
    {
        length = sooshi_convert_to_uint16(state->buffer->data + 1);

        // Did we receive the full tree yet?
        if (state->buffer->len - 1 < length)
            return;

        g_debug("Size of tree: %d", length);
        sooshi_parse_admin_tree(state, length, state->buffer->data + 3);
        state->buffer = g_byte_array_remove_range(state->buffer, 0, length + 3);
    }
    else
    {
        if (op_code >= state->op_code_map->len)
        {
            g_warning("Unknown opcode: %u", op_code);
            return;
        }

        SooshiNode *node = g_array_index(state->op_code_map, SooshiNode*, op_code);

        GVariant *v = sooshi_node_bytes_to_value(node, state->buffer);

        // bytes_to_value might return null if there's not enough data here
        // yet to fully parse a string
        if (v == NULL)
            return;

        sooshi_node_set_value(state, node, v, FALSE);

        g_info("Value for node '%s' updated: [%s]", node->name, sooshi_node_value_as_string(node));
        sooshi_node_notify_subscriber(state, node);

        // We have set and received back the CRC32 checksum of the tree - setup is finished
        if (node->op_code == 0)
        {
            sooshi_request_all_node_values(state, NULL);
            g_timeout_add_seconds(10, sooshi_heartbeat, (gpointer) state);
            state->init_handler(state);
        }
    }
}

gchar *sooshi_node_value_as_string(SooshiNode *node)
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

void sooshi_debug_dump_tree(SooshiNode *node, gint indent)
{
    gchar *node_value = sooshi_node_value_as_string(node);

    g_printf("%*s %d [%s] %s\n",
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

SooshiNode *sooshi_node_find(SooshiState *state, gchar *path, SooshiNode *start)
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

void sooshi_node_set_value(SooshiState *state, SooshiNode *node, GVariant *value, gboolean send_update)
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

gint sooshi_node_value_to_bytes(SooshiNode *node, guchar *buffer)
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

GVariant *sooshi_node_bytes_to_value(SooshiNode *node, GByteArray *buffer)
{
    gchar* tmp;
    guint16 u16;
    guint32 u32;
    gint16 s16;
    gint32 s32;
    float flt;
    GVariant *v;

    // buffer->data[0] still contains the op code
    switch(node->type)
    {
        case VAL_U8:
        case CHOOSER:
            v = g_variant_new_byte(buffer->data[1]);
            g_byte_array_remove_range(buffer, 0, 2);
            return v;

        case VAL_U16:
            u16 = buffer->data[1] | (guint16)buffer->data[2] << 8;
            v = g_variant_new_uint16(u16);
            g_byte_array_remove_range(buffer, 0, 3);
            return v;

        case VAL_U32:
            u32 = buffer->data[1] | (guint32)buffer->data[2] << 8
                | (guint32)buffer->data[3] << 16 | (guint32)buffer->data[4] << 24;
            v = g_variant_new_uint32(u32);
            g_byte_array_remove_range(buffer, 0, 5);
            return v;

        case VAL_S8:
            v = g_variant_new_byte((gchar)buffer->data[1]);
            g_byte_array_remove_range(buffer, 0, 2);
            return v;

        case VAL_S16:
            s16 = buffer->data[1] | (gint16)buffer->data[2] << 8;
            v = g_variant_new_int16(s16);
            g_byte_array_remove_range(buffer, 0, 3);
            return v;

        case VAL_S32:
            s32 = buffer->data[1] | (gint32)buffer->data[2] << 8
                | (gint32)buffer->data[3] << 16 | (gint32)buffer->data[4] << 24;
            v = g_variant_new_int32(s32);
            g_byte_array_remove_range(buffer, 0, 5);
            return v;

        case VAL_STR:
            u16 = buffer->data[1] | (guint16)buffer->data[2] << 8;

            if (buffer->len < u16)
                return NULL;

            tmp = g_strndup((gchar*)buffer->data + 3, u16);
            v = g_variant_new_string(tmp);
            g_free(tmp);
            g_byte_array_remove_range(buffer, 0, u16 + 3);

            return v;

        case VAL_BIN:
            u16 = buffer->data[1] | (guint16)buffer->data[2] << 8;

            if (buffer->len < u16)
                return NULL;

            tmp = g_strndup((gchar*)buffer->data + 3, u16);
            v = g_variant_new_bytestring(tmp);
            g_free(tmp);
            g_byte_array_remove_range(buffer, 0, u16 + 3);

            return v;

        case VAL_FLT:
            ((guchar*)&flt)[0] = buffer->data[1];
            ((guchar*)&flt)[1] = buffer->data[2];
            ((guchar*)&flt)[2] = buffer->data[3];
            ((guchar*)&flt)[3] = buffer->data[4];
            v = g_variant_new_double(flt);
            g_byte_array_remove_range(buffer, 0, 5);
            return v;

        default:
            g_error("Unsupported data type in sooshi_node_bytes_to_value(): %d", node->type);
            return NULL;
    }
}

void sooshi_node_request_value(SooshiState *state, SooshiNode *node)
{
    sooshi_send_bytes(state, &node->op_code, 1);
}

void sooshi_node_choose(SooshiState *state, SooshiNode *node)
{
    g_return_if_fail(state != NULL);
    g_return_if_fail(node != NULL);
    g_return_if_fail(node->parent->type == CHOOSER);
    gint index = g_list_index(node->parent->children, node);

    sooshi_node_set_value(state, node->parent, g_variant_new_byte((guchar)index), TRUE);
}

void sooshi_node_subscribe(SooshiState *state, SooshiNode *node, sooshi_node_subscriber_t func)
{
    g_info("Subscribing to node '%s'", node->name);
    node->subscriber = g_list_append(node->subscriber, (gpointer)func);
}

void sooshi_node_notify_subscriber(SooshiState *state, SooshiNode *node)
{
    GList *elem;
    for(elem = node->subscriber; elem; elem = elem->next)
        ((sooshi_node_subscriber_t)elem->data)(state, node);
}

void sooshi_request_all_node_values(SooshiState *state, SooshiNode *start)
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

void sooshi_send_bytes(SooshiState *state, guchar *buffer, gsize len)
{
    GVariantBuilder *b;
    GVariant *final;

    b = g_variant_builder_new(G_VARIANT_TYPE("(aya{sv})"));
    g_variant_builder_open(b, G_VARIANT_TYPE("ay"));

    g_debug("Sending message #%d to Mooshimeter:", state->send_sequence);
    g_variant_builder_add(b, "y", (guchar)state->send_sequence++);
    for (guint i = 0; i < len; ++i)
    {
        g_debug("    [%d] %x (%c %d)", i, buffer[i], buffer[i], buffer[i]);
        g_variant_builder_add(b, "y", buffer[i]);
    }

    g_variant_builder_close(b);

    g_variant_builder_open(b, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(b, "{sv}", "offset", g_variant_new_int16(0));
    g_variant_builder_close(b);
    final = g_variant_builder_end(b);

    GError *error = NULL;
    g_dbus_proxy_call_sync(state->serial_in,
        "WriteValue",
        final,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (error != NULL)
    {
        g_error("Error calling WriteValue: %s", error->message);
        g_error_free(error);
        return;
    }
}

void sooshi_node_send_value(SooshiState *state, SooshiNode *node)
{
    static guchar buffer[20] = {0};
    buffer[0] = node->op_code | 0x80;
    gint len = 1;
    len += sooshi_node_value_to_bytes(node, buffer + 1);

    if (len >= 20)
    {
        g_warning("Message is larger than 20 bytes!");
        len = 20;
    }

    sooshi_send_bytes(state, buffer, len);
}

static
SooshiNode *sooshi_parse_node(SooshiState *state, SooshiNode *parent, const guint8 *buffer, gulong *bytes_read)
{
    static guchar opcode  = 0;

    SooshiNode *node = g_new0(SooshiNode, 1);
    
    node->parent = parent;
    node->type = (SOOSHI_NODE_TYPE)buffer[0];
    node->value = NULL;
    node->op_code = 0;
    node->has_value = FALSE;
    if (node->type >= CHOOSER)
    {
        node->op_code = opcode++;
        g_array_append_val(state->op_code_map, node);
        node->has_value = TRUE;
    }

    guchar name_len = buffer[1];
    if (name_len)
        node->name = g_strndup((const gchar*)buffer + 2, name_len);
    else
        node->name = g_strdup("ROOT");
    
    guchar num_childs = buffer[2 + name_len];

    buffer += 3 + name_len;
    gulong current_bytes_read = 3 + name_len;

    for (guint i = 0; i < num_childs; ++i)
    {
        gulong tmp_bytes_read = 0;
        node->children = g_list_append(node->children, sooshi_parse_node(state, node, buffer, &tmp_bytes_read));
        buffer += tmp_bytes_read;
        current_bytes_read += tmp_bytes_read;
    }

    if (bytes_read)
        *bytes_read = current_bytes_read;

    return node;
}

void sooshi_parse_admin_tree(SooshiState *state, gulong compressed_size, const guint8 *buffer)
{
    GInputStream *in = g_memory_input_stream_new_from_data(buffer, compressed_size, NULL);
    GZlibDecompressor *decompressor = g_zlib_decompressor_new(G_ZLIB_COMPRESSOR_FORMAT_ZLIB);
    GOutputStream *out = g_memory_output_stream_new_resizable();
    GOutputStream *z_out = g_converter_output_stream_new(G_OUTPUT_STREAM(out), G_CONVERTER(decompressor));

    GError *error = NULL;
    g_output_stream_splice(
            z_out, in,
            G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE|G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
            NULL,
            &error);

    if (error != NULL)
    {
        g_error("Error while splicing: %s", error->message);
        return;
    }

    gpointer result = g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(out));

    crc32_t checksum = sooshi_crc32_calculate(state, buffer, compressed_size);
    g_info("Tree-CRC: %x", checksum);
    state->root_node = sooshi_parse_node(state, NULL, result, NULL);

    sooshi_debug_dump_tree(state->root_node, (guint)0);

    SooshiNode *crc_node = sooshi_node_find(state, "ADMIN:CRC32", NULL);

    if (crc_node)
        sooshi_node_set_value(state, crc_node, g_variant_new_uint32(checksum), TRUE);
    else
        g_error("Error finding node ADMIN:CRC32!");

    g_output_stream_close(out, NULL, NULL);
    g_output_stream_close(z_out, NULL, NULL);
    g_input_stream_close(in, NULL, NULL);

    g_object_unref(out);
    g_object_unref(z_out);
    g_object_unref(decompressor);
    g_object_unref(in);
}

guint16 sooshi_convert_to_uint16(guint8 *buffer)
{
    return buffer[0] | buffer[1] << 8;
}

guint sooshi_convert_to_int24(gchar *buffer)
{
    // 
    guchar tmp[4];
    tmp[0] = buffer[0] < 0 ? (guchar)0xFF : (guchar)0x00;
    tmp[1] = buffer[0];
    tmp[2] = buffer[1];
    tmp[3] = buffer[2];

    return (guint)tmp[0] | (guint)tmp[1] << 8 | (guint)tmp[2] << 16 | (guint)tmp[3] << 24;
}

static void
sooshi_on_object_added_connected(GDBusObjectManager *objman, GDBusObject *obj, gpointer user_data)
{
    SooshiState *state = (SooshiState*)user_data;

    GDBusInterface *inter = g_dbus_object_get_interface(obj, BLUEZ_GATT_CHARACTERISTIC_INTERFACE);

    if (!inter)
        return;

    GVariant *v_uuid = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(inter), "UUID");
    const gchar *uuid = g_variant_get_string(v_uuid, NULL);

    g_info("Got GATT characteristic with UUID '%s'", uuid);

    const gchar *sin_uuid = METER_SERIAL_IN;
    const gchar *sout_uuid = METER_SERIAL_OUT;
    if (g_ascii_strcasecmp(uuid, sin_uuid) == 0)
    {
        g_info("Found serial in!");
        state->serial_in = G_DBUS_PROXY(inter);
    }
    else if (g_ascii_strcasecmp(uuid, sout_uuid) == 0)
    {
        g_info("Found serial out!");
        state->serial_out = G_DBUS_PROXY(inter);
    }

    g_variant_unref(v_uuid);

    if (state->serial_in && state->serial_out)
        sooshi_initialize_mooshi(state);
}


SooshiState *sooshi_state_new()
{
    SooshiState *state = g_object_new(SOOSHI_TYPE_STATE, 0);

    GError *error = NULL;
    state->object_manager = g_dbus_object_manager_client_new_for_bus_sync(
            /* connection */
            G_BUS_TYPE_SYSTEM,
            /* flags */
            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
            /* name */
            BLUEZ_NAME,
            /* path */
            "/",
            /* proxy type func */
            NULL,
            /* proxy type userdata */
            NULL,
            /* proxy type destroy cb */
            NULL,
            /* cancellable */
            NULL,
            /* error */
            &error);

    if (error != NULL)
    {
        printf("Error creating sooshi state: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }

    return state;
}

void sooshi_state_delete(SooshiState *state)
{
    g_return_if_fail(state != NULL);

    if (state->object_manager) g_object_unref(state->object_manager);
    if (state->adapter) g_object_unref(state->adapter);
    if (state->mooshimeter) g_object_unref(state->mooshimeter);
    if (state->mooshimeter_dbus_path) g_free(state->mooshimeter_dbus_path);

    g_byte_array_unref(state->buffer);

    g_object_unref(state);
}

GDBusProxy *sooshi_dbus_find_interface_proxy_if(SooshiState *state, const gchar* interface_name, dbus_conditional_func_t cond_func, gpointer user_data)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);

    GDBusInterface *found = NULL;
    GList *objects = g_dbus_object_manager_get_objects(state->object_manager);

    if (!objects)
        return NULL;

    GList *l = objects;
    while(l->next)
    {
        GDBusObject *obj = G_DBUS_OBJECT(l->data); 
        GDBusInterface *interface = g_dbus_object_get_interface(obj, interface_name);

        if (interface)
        {
            if (!cond_func || cond_func(interface, user_data) == TRUE)
            {
                g_info("Found interface %s!", interface_name);
                found = interface;
                break;
            }
        }

        l = l->next;
    }
    g_list_free_full(objects, g_object_unref);

    return found ? G_DBUS_PROXY(found) : NULL;
}

gboolean sooshi_find_adapter(SooshiState *state)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);

    state->adapter = sooshi_dbus_find_interface_proxy_if(state, BLUEZ_ADAPTER_INTERFACE, NULL, NULL);
    return (state->adapter != NULL);
}

gboolean sooshi_cond_is_mooshimeter(GDBusInterface *interface, gpointer user_data)
{
    GVariant *v_uuids = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(interface), "UUIDs");
    
    GVariantIter *iter = g_variant_iter_new(v_uuids);
    gchar* uuid;
    gboolean found = FALSE;
    while (g_variant_iter_loop(iter, "s", &uuid))
    {
        if (g_ascii_strcasecmp(uuid, (gchar*)user_data) == 0)
        {
            found = TRUE;
            g_free(uuid);
            break;
        }
    }
    g_variant_unref(v_uuids);

    return found;
}

gboolean sooshi_cond_has_uuid(GDBusInterface *interface, gpointer user_data)
{
    GVariant *v_uuid = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(interface), "UUID");
    const gchar *uuid = g_variant_get_string(v_uuid, NULL);

    return (g_ascii_strcasecmp(uuid, (gchar*)user_data) == 0);
}

gboolean sooshi_find_mooshi(SooshiState *state)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);

    const gchar* uuid = METER_SERVICE_UUID;
    GDBusProxy *meter = sooshi_dbus_find_interface_proxy_if(state, BLUEZ_DEVICE_INTERFACE, sooshi_cond_is_mooshimeter, (gpointer)uuid);

    if (meter)
        sooshi_add_mooshimeter(state, meter);

    return (state->mooshimeter != NULL);
}

gboolean sooshi_start_scan(SooshiState *state)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);
    g_return_val_if_fail(state->adapter != NULL, FALSE);

    state->scan_signal_id = g_signal_connect(state->object_manager, 
        "object-added",
        G_CALLBACK(sooshi_on_object_added),
        state);

    GError *error = NULL;
    g_dbus_proxy_call_sync(state->adapter,
        "StartDiscovery",
        g_variant_new("()"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (error != NULL)
    {
        g_error("Error starting bluetooth device discovery!");
        g_error_free(error);
        return FALSE;
    }

    g_info("Started bluetooth scan ...");

    return TRUE;
}

gboolean sooshi_stop_scan(SooshiState *state)
{
    g_signal_handler_disconnect(state->object_manager, state->scan_signal_id);
    state->scan_signal_id = 0;

    GError *error = NULL;
    g_dbus_proxy_call_sync(state->adapter,
        "StopDiscovery",
        g_variant_new("()"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (error != NULL)
    {
        g_error("Error stopping bluetooth device discovery!");
        g_error_free(error);
        return FALSE;
    }

    return TRUE;
}

void sooshi_connect_mooshi(SooshiState *state)
{
    g_debug("Connecting to Mooshimeter ...");

    // Try to find serial_in/serial_out before connecting ...
    gchar *uuid_serial_in = METER_SERIAL_IN;
    state->serial_in = sooshi_dbus_find_interface_proxy_if(
            state,
            BLUEZ_GATT_CHARACTERISTIC_INTERFACE,
            sooshi_cond_has_uuid,
            (gpointer)uuid_serial_in);

    gchar *uuid_serial_out = METER_SERIAL_OUT;
    state->serial_out = sooshi_dbus_find_interface_proxy_if(
            state,
            BLUEZ_GATT_CHARACTERISTIC_INTERFACE,
            sooshi_cond_has_uuid,
            (gpointer)uuid_serial_out);

    state->scan_signal_id = g_signal_connect(state->object_manager, 
        "object-added",
        G_CALLBACK(sooshi_on_object_added_connected),
        state);

    GError *error = NULL;
    g_dbus_proxy_call_sync(state->mooshimeter,
        "Connect",
        g_variant_new("()"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (error != NULL)
    {
        g_error("Error connecting to Mooshimeter: %s", error->message);
        g_error_free(error);
        return;
    }

    if (state->serial_in && state->serial_out)
        sooshi_initialize_mooshi(state);
}

void sooshi_add_mooshimeter(SooshiState *state, GDBusProxy *meter)
{
    state->mooshimeter_dbus_path = g_strdup(g_dbus_proxy_get_object_path(meter));
    state->mooshimeter = meter;
    g_info("Added Mooshimeter(Path: %s)", state->mooshimeter_dbus_path);
}

void sooshi_enable_notify(SooshiState *state)
{
    g_return_if_fail(state->serial_out);

    state->properties_changed_id = g_signal_connect(
        state->serial_out,
        "g-properties-changed",
        G_CALLBACK(sooshi_on_serial_out_ready),
        state);

    GError *error = NULL;
    g_dbus_proxy_call_sync(state->serial_out,
        "StartNotify",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (error != NULL)
    {
        g_error("Error starting read routine: %s", error->message);
        g_error_free(error);
        return;
    }
}

void sooshi_disable_notify(SooshiState *state)
{
    g_return_if_fail(state->serial_out);

    g_signal_handler_disconnect(state->serial_out, state->properties_changed_id);
    state->properties_changed_id = 0;

    GError *error = NULL;
    g_dbus_proxy_call_sync(state->serial_out,
        "StopNotify",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (error != NULL)
    {
        g_error("Error stopping read routine: %s", error->message);
        g_error_free(error);
        return;
    }
}

void sooshi_initialize_mooshi(SooshiState *state)
{
    sooshi_enable_notify(state);

    guchar op_code = 1;
    sooshi_send_bytes(state, &op_code, 1);
}

gboolean sooshi_heartbeat(gpointer user_data)
{
    SooshiState *state = SOOSHI_STATE(user_data);
    SooshiNode *node = sooshi_node_find(state, "PCB_VERSION", NULL);

    sooshi_node_request_value(state, node);
    return TRUE;
}

void sooshi_run(SooshiState *state, sooshi_initialized_handler_t init_handler)
{
    state->init_handler = init_handler;

    if (!sooshi_find_mooshi(state))
    {
        g_warning("Could not find Mooshimeter!");

        // We couldn't find it, let's scan
        if (!sooshi_find_adapter(state))
        {
            g_warning("Could not find bluetooth adapter!");
            sooshi_state_delete(state);
            return;
        }

        g_info("Found bluetooth adapter, ready to scan!");

        if (!sooshi_start_scan(state))
        {
            g_warning("Error starting bluetooth scan!");
            sooshi_state_delete(state);
            return;
        }
    }
    else
        sooshi_connect_mooshi(state);

    g_debug("Starting main loop ...");
    state->loop = g_main_loop_new(NULL, FALSE);

    g_main_loop_run(state->loop);
}
