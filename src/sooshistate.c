#include <stdio.h>
#include <zlib.h>
#include <string.h>
#include "sooshistate.h"

G_DEFINE_TYPE(SooshiState, sooshi_state, G_TYPE_OBJECT)

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

    sooshi_crc32_init(state);
}

static void
sooshi_on_object_added(GDBusObjectManager *objman, GDBusObject *obj, gpointer user_data)
{
    SooshiState *state = (SooshiState*)user_data;

    GDBusInterface *inter = g_dbus_object_get_interface(obj, BLUEZ_DEVICE_INTERFACE);

    if (!inter)
        return;

    GVariant *v_name = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(inter), "Name");
    const gchar *name = g_variant_get_string(v_name, NULL);

    const gchar *uuid = METER_SERVICE_UUID;
    if (sooshi_cond_is_mooshimeter(inter, (gpointer)uuid))
    {
        g_mutex_lock(&state->mooshimeter_mutex);
        sooshi_add_mooshimeter(state, G_DBUS_PROXY(inter));
        g_cond_signal(&state->mooshimeter_found);
        g_mutex_unlock(&state->mooshimeter_mutex);

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

        g_info("Reading value ...");
        GVariantIter *iter = g_variant_iter_new(value);
        guint8 buf[20];
        gchar tmp;
        gint i = 0;
        while (g_variant_iter_loop(iter, "y", &tmp))
        {
            if (i == 0)
                g_info("Message Sequence: %d", tmp);
            else
            {
                buf[i-1] = tmp;
                g_info("    [%d:%d] %x (%c, %d)", i, state->buffer->len + (i - 1), buf[i-1], buf[i-1], buf[i-1]);
            }
            i++;
        }
        g_variant_unref(value);

        g_byte_array_append(state->buffer, buf, i - 1);
        sooshi_parse_response(state);

        //guint pcb_v = sooshi_convert_to_int24(buf);
        //g_info("PCB_VERSION: %d", pcb_v);
    }

    g_variant_dict_unref(dict);
}

void sooshi_parse_response(SooshiState *state)
{
    guint8 op_code = state->buffer->data[0];
    guint16 length = 0;

    switch(op_code)
    {
        case 1:
            length = sooshi_convert_to_uint16(state->buffer->data + 1);

            // Did we receive the full tree yet?
            if (state->buffer->len - 1 < length)
                return;

            g_debug("Size of tree: %d", length);
            sooshi_parse_admin_tree(state, length, state->buffer->data + 3);

            break;
        default:
            g_warning("Unknown opcode: %u", op_code);
    }
}

void sooshi_debug_dump_tree(SooshiNode *node, gint indent)
{
    printf("%*s (%d, %d children)\n", (gint)(indent + strlen(node->name)), node->name, node->type, g_list_length(node->children));

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
    
    gchar *sep = g_strstr_length(path, -1, ":");

    if (sep)
        *sep = '0';

    GList *elem;
    SooshiNode *item;
    for(elem = node->children; elem; elem = elem->next)
    {
        item = elem->data;

        if (g_strcmp0(path, item->name) == 0)
        {
            if (sep == NULL)
                return item;
            else
                return sooshi_node_find(state, sep + 1, item);
        }
    }
}

void sooshi_node_set_value(SooshiState *state, SooshiNode *node, gpointer value)
{
    g_return_if_fail(state != NULL);
    g_return_if_fail(node != NULL);

    switch(node->type)
    {
        case VAL_U8:
            node->value = g_variant_new_byte((guchar) value);
            break;
        case VAL_U16:
            node->value = g_variant_new_uint16((guint16)value);
            break;
        case VAL_U32:
            node->value = g_variant_new_uint32((guint32)value);
            break;
        case VAL_S8:
            node->value = g_variant_new_byte((gint8)value);
            break;
        case VAL_S16:
            node->value = g_variant_new_int16((gint16)value);
            break;
        case VAL_S32:
            node->value = g_variant_new_int32((gint32)value);
            break;
    }
}

static
SooshiNode *sooshi_parse_node(SooshiState *state, SooshiNode *parent, const guint8 *buffer, gulong *bytes_read)
{
    SooshiNode *node = g_new0(SooshiNode, 1);
    
    node->parent = parent;
    node->type = (SOOSHI_NODE_TYPE)buffer[0];
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
    crc32_t checksum = sooshi_crc32_calculate(state, result, g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(out)));
    g_info("Tree-CRC: %x", checksum);
    state->root_node = sooshi_parse_node(state, NULL, result, NULL);

    sooshi_debug_dump_tree(state->root_node, (guint)0);

    SooshiNode *crc_node = sooshi_node_find(state, "ADMIN:CRC32", NULL);

    if (crc_node)
    {
        sooshi_node_set_value(state, crc_node, (gpointer) checksum); 
    }

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
        sooshi_test(state);
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

void sooshi_wait_until_mooshimeter_found(SooshiState *state, gint64 timeout)
{
    gint64 end_time;
    end_time = g_get_monotonic_time () + timeout * G_TIME_SPAN_SECOND;

    g_mutex_lock(&state->mooshimeter_mutex); 

    while (!state->mooshimeter)
    {
        if (timeout > 0)
            g_cond_wait_until(&state->mooshimeter_found, &state->mooshimeter_mutex, end_time);
        else
            g_cond_wait(&state->mooshimeter_found, &state->mooshimeter_mutex);
    }

    g_mutex_unlock(&state->mooshimeter_mutex);
}

void sooshi_start(SooshiState *state, sooshi_run_handler handler)
{
    g_debug("Starting main loop ...");
    state->loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run(state->loop);
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

    state->connected = TRUE; 

    if (state->serial_in && state->serial_out)
        sooshi_test(state);
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

void sooshi_test(SooshiState *state)
{
    sooshi_enable_notify(state);

    GVariantBuilder *b;
    GVariant *final;

    b = g_variant_builder_new(G_VARIANT_TYPE("(aya{sv})"));
    g_variant_builder_open(b, G_VARIANT_TYPE("ay"));
    g_variant_builder_add(b, "y", (guchar)0);
    g_variant_builder_add(b, "y", (guchar)1);
    g_variant_builder_close(b);

    g_variant_builder_open(b, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(b, "{sv}", "offset", g_variant_new_int16(0));
    g_variant_builder_close(b);
    final = g_variant_builder_end(b);

    GVariant *v_name = g_dbus_proxy_get_cached_property(state->mooshimeter, "Connected");
    gboolean connected = g_variant_get_boolean(v_name);

    g_info("Mooshimeter still connected: %d", connected);

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
