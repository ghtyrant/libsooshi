#include <stdio.h>
#include <gio/gio.h>
#include <glib.h>

#define BLUEZ_NAME "org.bluez"
#define BLUEZ_ADAPTER_INTERFACE "org.bluez.Adapter1"
#define BLUEZ_DEVICE_INTERFACE "org.bluez.Device1"
#define SOOSHI_LOG "sooshi"

typedef struct
{
  GDBusObjectManager *object_manager;
  GDBusProxy* adapter;
  GDBusProxy* mooshimeter;
} SooshiState;

static void
on_object_added(GDBusObjectManager *objman, GDBusObject *obj, gpointer user_data)
{
    GError *error = NULL;
    GDBusProxy *adapter = G_DBUS_PROXY(user_data);

    gchar *path;
    g_object_get(obj, "g-object-path", &path, NULL);
    printf("Added object '%s'!\n", path);
    g_free(path);

    GDBusInterface *inter = g_dbus_object_get_interface(obj, "org.bluez.Device1");
    GDBusProxy *device = G_DBUS_PROXY(inter);

    GVariant *name = g_dbus_proxy_get_cached_property(device, "Name");
    gchar* name_str = g_variant_dup_string(name, NULL);
    printf("    Name: %s\n", name_str); 
    g_variant_unref(name);

    if (g_strcmp0(name_str, "Mooshimeter V.1") == 0)
    {
      printf("Found Mooshimeter!\n");
      g_dbus_proxy_call_sync(adapter,
          "StopDiscovery",
          g_variant_new("()"),
          G_DBUS_CALL_FLAGS_NONE,
          -1,
          NULL,
          &error);

      if (error != NULL)
      {
          printf("Error stopping discovery: %s\n", error->message);
          g_error_free(error);
      }
    }
}

SooshiState *sooshi_state_new(GError **error)
{
    SooshiState *state = g_new0(SooshiState, 1); 

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
            error);

    if (*error != NULL)
    {
        printf("Error creating sooshi state: %s\n", (*error)->message);
        return NULL;
    }

    return state;
}

void sooshi_state_delete(SooshiState *state)
{
    g_return_if_fail(state != NULL);

    if (state->object_manager) g_object_unref(state->object_manager);
    if (state->adapter) g_object_unref(state->adapter);

    g_free(state);
}

gboolean sooshi_find_adapter(SooshiState *state)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);

    g_debug("Fetching all objects of " BLUEZ_NAME " ...");

    GDBusInterface *interface = NULL;
    GList *objects = g_dbus_object_manager_get_objects(state->object_manager);
    GList *l = objects;
    while(l->next)
    {
        GDBusObject *obj = G_DBUS_OBJECT(l->data); 
        interface = g_dbus_object_get_interface(obj, BLUEZ_ADAPTER_INTERFACE);

        if (interface)
        {
            g_debug("Found adapter @ '%s'", g_dbus_object_get_object_path(obj));
            state->adapter = G_DBUS_PROXY(interface);
            break;
        }

        l = l->next;
    }
    g_list_free_full(objects, g_object_unref);

    return (state->adapter != NULL);
}

gboolean sooshi_find_mooshi(SooshiState *state, const gchar* name)
{
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(state->object_manager != NULL, FALSE);

    GDBusInterface *interface = NULL;
    GList *objects = g_dbus_object_manager_get_objects(state->object_manager);
    GList *l = objects;
    while(l->next)
    {
        GDBusObject *obj = G_DBUS_OBJECT(l->data); 
        interface = g_dbus_object_get_interface(obj, BLUEZ_DEVICE_INTERFACE);

        if (interface)
        {
            // This is a device ... now check the name
            g_debug("Found device @ '%s'", g_dbus_object_get_object_path(obj));

            GVariant *vname = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(interface), "Name");
            gchar* name_str = g_variant_get_string(vname, NULL);

            if (g_strcmp0(name_str, name) == 0)
            {
                g_debug("Found Mooshimeter!");
                state->mooshimeter = G_DBUS_PROXY(interface);
                break;
            }

            g_variant_unref(vname);
        }

        l = l->next;
    }
    g_list_free_full(objects, g_object_unref);

    return (state->mooshimeter != NULL);
}

int main()
{
    g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK, g_log_default_handler, NULL);

    SooshiState *state;
    GError *error = NULL;
    state = sooshi_state_new(&error);

    if (state == NULL)
    {
        g_error_free(error);
        return -1;
    }

    if (!sooshi_find_adapter(state))
    {
        g_warning("Could not find bluetooth adapter!");
        sooshi_state_delete(state);
        return -1;
    }

    if (!sooshi_find_mooshi(state, "Mooshimeter V.1"))
    {
        g_warning("Could not find Mooshimeter!");
        sooshi_state_delete(state);
        return -1;
    }

    sooshi_state_delete(state);

    /*GMainLoop *loop = g_main_loop_new (NULL, FALSE);





    GDBusObject *obj = g_dbus_object_manager_get_object(
            objman,
            "/org/bluez/hci0");

    if (!obj)
    {
        printf("Error getting object!\n");
        goto cleanup;
    }

    g_object_get(obj, "g-object-path", &name, NULL);
    printf("Got object '%s'!\n", name);
    g_free(name);

    GDBusInterface *inter = g_dbus_object_get_interface(obj, "org.bluez.Adapter1");
    GDBusProxy *proxy = G_DBUS_PROXY(inter);
    g_signal_connect(objman, 
        "object-added",
        G_CALLBACK(on_object_added),
        proxy);

    printf("Proxy: %s\n", g_dbus_proxy_get_interface_name(proxy));

    GVariant *addr = g_dbus_proxy_get_cached_property(proxy, "Address");
    const gchar* addr_str = g_variant_get_string(addr, NULL);
    printf("Bla: %s\n", addr_str); 
    g_variant_unref(addr);

    g_dbus_proxy_call_sync(proxy,
        "StartDiscovery",
        g_variant_new("()"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (error != NULL)
    {
        printf("Error creating object manager: %s\n", error->message);
        goto cleanup;
    }

    printf("Starting main loop!\n");

    g_main_loop_run(loop);


cleanup:
    g_error_free(error);
    g_object_unref(objman);
    g_object_unref(obj);*/
}
