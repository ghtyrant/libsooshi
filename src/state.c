#include <stdio.h>
#include <zlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "sooshi.h"

G_DEFINE_TYPE(SooshiState, sooshi_state, G_TYPE_OBJECT)

const gchar *const __SOOSHI_NODE_TYPE_STR[] =
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

// GObject stuff
static void sooshi_state_class_init(SooshiStateClass *class);
static void sooshi_state_init(SooshiState *state);
static void sooshi_state_finalize(GObject *object);
static void sooshi_state_dispose(GObject *object);

static gboolean sooshi_cond_is_mooshimeter(GDBusInterface *interface, gpointer user_data);
static gboolean sooshi_cond_has_uuid(GDBusInterface *interface, gpointer user_data);
static gboolean sooshi_cond_adapter_is_powered(GDBusInterface *interface, gpointer user_data);

// Mooshimeter functions
static void sooshi_add_mooshi(SooshiState *state, GDBusProxy *meter);
static void sooshi_initialize_mooshi(SooshiState *state);
static gboolean sooshi_connect_mooshi(SooshiState *state);
static gboolean sooshi_disconnect_mooshi(SooshiState *state);
static gboolean sooshi_start_listening_to_mooshi(SooshiState *state);
static gboolean sooshi_stop_listening_to_mooshi(SooshiState *state);

// DBus functions
static void sooshi_on_object_added(GDBusObjectManager *objman, GDBusObject *obj, gpointer user_data);
static void sooshi_on_object_added_connected(GDBusObjectManager *objman, GDBusObject *obj, gpointer user_data);
static void sooshi_on_serial_out_ready(GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer user_data);
static gboolean sooshi_find_adapter(SooshiState *state);
static gboolean sooshi_find_mooshi(SooshiState *state);
static gboolean sooshi_start_scan(SooshiState *state);
static gboolean sooshi_stop_scan(SooshiState *state, gboolean stop_timeout);
static gboolean sooshi_heartbeat(gpointer user_data);

void
sooshi_on_mooshi_initialized(SooshiState *state)
{
    state->heartbeat_source_id = g_timeout_add_seconds(10, sooshi_heartbeat, (gpointer) state);
    state->init_handler(state, state->init_handler_data);
}

void
sooshi_send_bytes(SooshiState *state, guchar *buffer, gsize len, gboolean block)
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

    if (block == TRUE)
    {
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
    else
    {
        g_dbus_proxy_call(state->serial_in,
            "WriteValue",
            final,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            NULL,
            NULL);
    }

    g_variant_builder_unref(b);
}

void
sooshi_node_send_value(SooshiState *state, SooshiNode *node)
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

    sooshi_send_bytes(state, buffer, len, FALSE);
}

SooshiState *
sooshi_state_new(sooshi_error_t *error)
{
    SooshiState *state = g_object_new(SOOSHI_TYPE_STATE, 0);

    GError *err = NULL;
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
            &err);

    if (err != NULL)
    {
        if (error)
            *error = SOOSHI_ERROR_DBUS_CONNECTION_FAILED;

        g_error_free(err);
        g_object_unref(state);
        return NULL;
    }

    if (error)
        *error = SOOSHI_ERROR_SUCCESS;

    return state;
}

void
sooshi_state_delete(SooshiState *state)
{
    g_object_unref(state);
}

GDBusProxy *
sooshi_dbus_find_interface_proxy_if(SooshiState *state, const gchar* interface_name, dbus_conditional_func_t cond_func, gpointer user_data)
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

sooshi_error_t
sooshi_setup(SooshiState *state, sooshi_callback_t init_handler, gpointer init_data,
        sooshi_callback_t scan_timeout_handler, gpointer scan_timeout_data)
{
    state->init_handler = init_handler;
    state->init_handler_data = init_data;

    state->scan_timeout_handler = scan_timeout_handler;
    state->scan_timeout_data = scan_timeout_data;

    // We couldn't find it, let's scan
    if (!sooshi_find_adapter(state))
    {
        g_warning("Could not find bluetooth adapter!");
        return SOOSHI_ERROR_NO_ADAPTER_FOUND;
    }

    if (!sooshi_find_mooshi(state))
    {
        g_warning("Could not find Mooshimeter!");

        if (!sooshi_start_scan(state))
        {
            g_warning("Error starting bluetooth scan!");
            return SOOSHI_ERROR_SCAN_FAILED;
        }
    }
    else
        sooshi_connect_mooshi(state);

    return SOOSHI_ERROR_SUCCESS;
}

void
sooshi_run(SooshiState *state)
{
    g_debug("Starting main loop ...");
    state->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(state->loop);
}

SOOSHI_API void sooshi_stop(SooshiState *state)
{
    g_debug("Stopping main loop ...");
    g_main_loop_quit(state->loop);
}

/* Static function definitions */

static void
sooshi_state_class_init(SooshiStateClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    object_class->dispose = sooshi_state_dispose;
    object_class->finalize = sooshi_state_finalize;
}

static void
sooshi_state_dispose(GObject *object)
{
    SooshiState *state = SOOSHI_STATE(object);

    // Stop heartbeat source
    if (state->heartbeat_source_id > 0)
        g_source_remove(state->heartbeat_source_id);

    if (state->listening == TRUE)
        sooshi_stop_listening_to_mooshi(state);

    if (state->connected == TRUE)
        sooshi_disconnect_mooshi(state);

    if (state->scanning == TRUE)
        sooshi_stop_scan(state, TRUE);

    g_clear_object(&state->object_manager);
    g_clear_object(&state->adapter);
    g_clear_object(&state->mooshimeter);
    g_clear_object(&state->serial_in);
    g_clear_object(&state->serial_out);

    if (state->loop) g_main_loop_unref(state->loop);
    state->loop = NULL;
 
    G_OBJECT_CLASS(sooshi_state_parent_class)->dispose(object);
}

static void
sooshi_state_finalize(GObject *object)
{
    SooshiState *state = SOOSHI_STATE(object);

    g_free(state->mooshimeter_dbus_path);
    sooshi_node_free_all(state, NULL);

    if (state->buffer) g_byte_array_unref(state->buffer);
    state->buffer = NULL;

    if (state->op_code_map) g_ptr_array_free(state->op_code_map, TRUE);
    state->op_code_map = NULL;

    G_OBJECT_CLASS(sooshi_state_parent_class)->finalize(object);
}

static void
sooshi_state_init(SooshiState *state)
{
    state->buffer = g_byte_array_new();
    state->send_sequence = 0;
    state->recv_sequence = 0;

    state->op_code_map = g_ptr_array_new();

    sooshi_crc32_init(state);
}

/* DBus interface finding predicates */
static gboolean
sooshi_cond_is_mooshimeter(GDBusInterface *interface, gpointer user_data)
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
    g_variant_iter_free(iter);
    g_variant_unref(v_uuids);

    return found;
}

static gboolean
sooshi_cond_has_uuid(GDBusInterface *interface, gpointer user_data)
{
    GVariant *v_uuid = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(interface), "UUID");
    const gchar *uuid = g_variant_get_string(v_uuid, NULL);

    return (g_ascii_strcasecmp(uuid, (gchar*)user_data) == 0);
}

static gboolean
sooshi_cond_adapter_is_powered(GDBusInterface *interface, gpointer user_data)
{
    GVariant *v_powered = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(interface), "Powered");
    gboolean powered = g_variant_get_boolean(v_powered);

    return powered;
}


static void
sooshi_add_mooshi(SooshiState *state, GDBusProxy *meter)
{
    state->mooshimeter_dbus_path = g_strdup(g_dbus_proxy_get_object_path(meter));
    state->mooshimeter = meter;
    g_info("Added Mooshimeter (Path: %s)", state->mooshimeter_dbus_path);
}

static void
sooshi_initialize_mooshi(SooshiState *state)
{
    sooshi_start_listening_to_mooshi(state);

    guchar op_code = 1;
    sooshi_send_bytes(state, &op_code, 1, TRUE);
}

static gboolean
sooshi_connect_mooshi(SooshiState *state)
{
    if (state->connected == TRUE)
        return FALSE;

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
        g_signal_handler_disconnect(state->object_manager, state->scan_signal_id);
        return FALSE;
    }

    state->connected = TRUE;

    if (state->serial_in && state->serial_out)
    {
        g_info("Serial In & Serial Out already available!");
        sooshi_initialize_mooshi(state);
    }

    return TRUE;
}

static gboolean
sooshi_disconnect_mooshi(SooshiState *state)
{
    if (state->connected == FALSE)
        return FALSE;

    g_signal_handler_disconnect(state->object_manager, state->scan_signal_id);

    GError *error = NULL;
    g_dbus_proxy_call_sync(state->mooshimeter,
        "Disconnect",
        g_variant_new("()"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (error != NULL)
    {
        g_error("Error disconnecting from Mooshimeter: %s", error->message);
        g_error_free(error);
        return FALSE;
    }

    state->connected = FALSE;

    return TRUE;
}

static gboolean
sooshi_start_listening_to_mooshi(SooshiState *state)
{
    g_return_val_if_fail(state->serial_out != NULL, FALSE);

    if (state->listening == TRUE)
        return FALSE;

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
        g_signal_handler_disconnect(state->serial_out, state->properties_changed_id);
        g_error_free(error);
        return FALSE;
    }

    state->listening = TRUE;

    return TRUE;
}

static gboolean
sooshi_stop_listening_to_mooshi(SooshiState *state)
{
    g_return_val_if_fail(state->serial_out != NULL, FALSE);

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
        return FALSE;
    }

    state->listening = FALSE;

    return TRUE;
}

/* DBus Callbacks */
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
        sooshi_add_mooshi(state, G_DBUS_PROXY(inter));
        g_info("Found device '%s', looks like a Mooshimeter!", name);
        sooshi_stop_scan(state, TRUE);
        sooshi_connect_mooshi(state);
    }
    else
    {
        g_info("Found device '%s', but it's not the droid we are looking for ...", name);
    }

    g_variant_unref(v_name);
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

static void
sooshi_on_serial_out_ready(GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer user_data)
{
    SooshiState *state = (SooshiState*)user_data;

    GVariantDict *dict = g_variant_dict_new(changed_properties);

    if (g_variant_dict_contains(dict, "Value") == FALSE)
    {
        g_variant_dict_unref(dict);
        return;
    }

    GVariant *value = g_variant_dict_lookup_value(dict, "Value", G_VARIANT_TYPE("ay"));

    if (value)
    {
        GVariantIter *iter = g_variant_iter_new(value);
        guint8 buf[20];
        gchar tmp;
        gint i = 0;
        while (g_variant_iter_loop(iter, "y", &tmp))
        {
            // Don't add recv sequence to result buffer
            if (i == 0)
            {
                i++;
                continue;
            }

            buf[i-1] = tmp;
            i++;
        }
        g_variant_unref(value);

        state->buffer = g_byte_array_append(state->buffer, buf, i - 1);
        sooshi_parse_response(state);
    }

    g_variant_dict_unref(dict);
}


static gboolean
sooshi_find_adapter(SooshiState *state)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);

    state->adapter = sooshi_dbus_find_interface_proxy_if(state, BLUEZ_ADAPTER_INTERFACE, sooshi_cond_adapter_is_powered, NULL);
    return (state->adapter != NULL);
}

static gboolean
sooshi_find_mooshi(SooshiState *state)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);

    const gchar* uuid = METER_SERVICE_UUID;
    GDBusProxy *meter = sooshi_dbus_find_interface_proxy_if(state, BLUEZ_DEVICE_INTERFACE, sooshi_cond_is_mooshimeter, (gpointer)uuid);

    if (meter)
        sooshi_add_mooshi(state, meter);

    return (state->mooshimeter != NULL);
}

static gboolean
sooshi_scan_timed_out(gpointer user_data)
{
    SooshiState *state = user_data;
    sooshi_stop_scan(state, FALSE);

    if (state->scan_timeout_handler)
        state->scan_timeout_handler(state, state->scan_timeout_data);

    state->scan_timeout_source_id = 0;    
    return FALSE;
}

static gboolean
sooshi_start_scan(SooshiState *state)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);
    g_return_val_if_fail(state->adapter != NULL, FALSE);
    
    // Let's not start a scan twice
    if (state->scanning != FALSE)
        return FALSE;

    state->scan_signal_id = g_signal_connect(state->object_manager, 
        "object-added",
        G_CALLBACK(sooshi_on_object_added),
        state);

    state->scan_timeout_source_id = g_timeout_add_seconds(10, sooshi_scan_timed_out, (gpointer) state);

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

    state->scanning = TRUE;

    g_info("Started bluetooth scan ...");

    return TRUE;
}

static gboolean
sooshi_stop_scan(SooshiState *state, gboolean stop_timeout)
{
    // Can't stop what wasn't started!
    if (state->scanning != TRUE)
        return FALSE;

    if (stop_timeout)
    {
        if (state->scan_timeout_source_id > 0)
        {
            g_source_remove(state->scan_timeout_source_id);
            state->scan_timeout_source_id = 0;
        }
    }

    g_info("Stopping Bluetooth scan!");

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

    state->scanning = FALSE;

    return TRUE;
}

static gboolean
sooshi_heartbeat(gpointer user_data)
{
    SooshiState *state = SOOSHI_STATE(user_data);
    SooshiNode *node = sooshi_node_find(state, "PCB_VERSION", NULL);
    sooshi_node_request_value(state, node);

    return TRUE;
}

