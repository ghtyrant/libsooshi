#include "sooshistate.h"

G_DEFINE_TYPE(SooshiState, sooshi_state, G_TYPE_OBJECT);

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

        /*GVariantBuilder *b;
        GVariant *dict;

        b = g_variant_builder_new(G_VARIANT_TYPE("({sv})"));
        g_variant_builder_add(b, "{sv}", "offset", g_variant_new_int32(0));
        dict = g_variant_builder_end(b);

        GError *error = NULL;
        GVariant *ret = g_dbus_proxy_call_sync(state->info_characteristic,
            "ReadValue",
            dict,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &error);

        //g_variant_unref(dict);

        if (error)
        {
            g_info("Error ReadValue(): %s", error->message);
            g_error_free(error);
            return;
        }

        guchar bytes[sizeof(MeterInfo)];

        GVariantIter *iter;
        g_variant_get(ret, "a(y)", &iter);
        guchar str;
        guint actual_length = 0;
        while (g_variant_iter_loop (iter, "(y)", &str))
        {
            bytes[actual_length++] = str;
        }
        g_variant_iter_free (iter);

        MeterInfo *tmp = (MeterInfo*)bytes;
        g_info("MeterInfo: PCB-Version: %d, Assembly-Variant: %d, Lot-Number: %d, Buildtime: %d",
                tmp->pcb_version, tmp->assembly_variant, tmp->lot_number, tmp->build_time);
        g_variant_unref(ret);*/
    }
    else if (g_ascii_strcasecmp(uuid, sout_uuid) == 0)
    {
        g_info("Found serial out!");
        state->serial_out = G_DBUS_PROXY(inter);
    }

    g_variant_unref(v_uuid);

    if (state->serial_in && state->serial_out)
        g_main_loop_quit(state->loop);
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

    g_object_unref(state);
}

GDBusProxy *sooshi_dbus_find_interface_proxy_if(SooshiState *state, const gchar* interface_name, dbus_conditional_func_t cond_func, gpointer user_data)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);

    GDBusInterface *found = NULL;
    GList *objects = g_dbus_object_manager_get_objects(state->object_manager);
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
    GVariant *uuids = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(interface), "UUIDs");
    
    GVariantIter *iter = g_variant_iter_new (uuids);
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
    g_variant_unref(uuids);

    return found;
}

gboolean sooshi_find_mooshi(SooshiState *state)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);

    const gchar* uuid = METER_SERVICE_UUID;
    GDBusProxy *meter = sooshi_dbus_find_interface_proxy_if(state, BLUEZ_DEVICE_INTERFACE, sooshi_cond_is_mooshimeter, (gpointer)uuid);
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
    state->loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run(state->loop);
}

void sooshi_connect_mooshi(SooshiState *state)
{
    g_debug("Connecting to Mooshimeter ...");

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
}

void sooshi_add_mooshimeter(SooshiState *state, GDBusProxy *meter)
{
    state->mooshimeter_dbus_path = g_strdup(g_dbus_proxy_get_object_path(meter));
    state->mooshimeter = meter;
    g_info("Added Mooshimeter(Path: %s)", state->mooshimeter_dbus_path);
}

void sooshi_test(SooshiState *state)
{
    GVariantBuilder *b;
    GVariant *str;

    b = g_variant_builder_new(G_VARIANT_TYPE("((ay)({sv}))"));
    g_variant_builder_open(b, G_VARIANT_TYPE("ay"));
    const gchar* tmp = "ADMIN:TREE";
    while (*tmp++)
        g_variant_builder_add(b, "y", g_variant_new_byte(*tmp));
    g_variant_builder_close(b);

    str = g_variant_builder_end(b);

    g_variant_builder_open(b, G_VARIANT_TYPE("{sv}"));
    g_variant_builder_add(b, "{sv}", "offset", g_variant_new_int32(0));
    g_variant_builder_close(b);
    str = g_variant_builder_end(b);

    GError *error = NULL;
    g_dbus_proxy_call_sync(state->serial_in,
        "WriteValue",
        str,
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
}
