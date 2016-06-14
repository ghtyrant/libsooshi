#ifndef SOOSHISTATE_H_
#define SOOSHISTATE_H_

#include <gio/gio.h>
#include <gobject/gobject.h>

#define BLUEZ_NAME "org.bluez"
#define BLUEZ_ADAPTER_INTERFACE "org.bluez.Adapter1"
#define BLUEZ_DEVICE_INTERFACE "org.bluez.Device1"
#define BLUEZ_GATT_CHARACTERISTIC_INTERFACE "org.bluez.GattCharacteristic1"

#define METER_SERVICE_UUID "1BC5FFA0-0200-62AB-E411-F254E005DBD4"
#define METER_SERIAL_IN    "1BC5FFA1-0200-62AB-E411-F254E005DBD4"
#define METER_SERIAL_OUT   "1BC5FFA2-0200-62AB-E411-F254E005DBD4"

typedef struct _SooshiState SooshiState;
typedef struct _SooshiStateClass SooshiStateClass;

struct _SooshiState
{
    GObject parent_instance;

    GDBusObjectManager *object_manager;
    GDBusProxy* adapter;
    GDBusProxy* mooshimeter;

    gboolean connected;
    gchar *mooshimeter_dbus_path;

    gulong scan_signal_id;
    GCond mooshimeter_found;
    GMutex mooshimeter_mutex;

    GDBusProxy *serial_in;
    gulong properties_changed_id;
    GDBusProxy *serial_out;

    GMainLoop *loop;

    GByteArray *buffer;
};

struct _SooshiStateClass
{
    GObjectClass parent_class;
};

#define SOOSHI_TYPE_STATE                   (sooshi_state_get_type ())
#define SOOSHI_STATE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), SOOSHI_TYPE_STATE, SooshiState))
#define SOOSHI_IS_STATE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SOOSHI_TYPE_STATE))
#define SOOSHI_STATE_CLASS(_class)          (G_TYPE_CHECK_CLASS_CAST ((_class), SOOSHI_TYPE_STATE, SooshiStateClass))
#define SOOSHI_IS_STATE_CLASS(_class)       (G_TYPE_CHECK_CLASS_TYPE ((_class), SOOSHI_TYPE_STATE))
#define SOOSHI_STATE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), SOOSHI_TYPE_STATE, SooshiStateClass))

typedef gboolean (*dbus_conditional_func_t)(GDBusInterface* interface, gpointer user_data);
typedef void (sooshi_run_handler)(SooshiState *state);

GType sooshi_state_get_type();
SooshiState *sooshi_state_new();
void sooshi_state_delete(SooshiState *state);
GDBusProxy *sooshi_dbus_find_interface_proxy_if(SooshiState *state, const gchar* interface_name, dbus_conditional_func_t cond_func, gpointer user_data);
gboolean sooshi_find_adapter(SooshiState *state);
gboolean sooshi_cond_is_mooshimeter(GDBusInterface *interface, gpointer user_data);
gboolean sooshi_find_mooshi(SooshiState *state);
void sooshi_start(SooshiState *state, sooshi_run_handler handler);
void sooshi_add_mooshimeter(SooshiState *state, GDBusProxy *meter);
void sooshi_test(SooshiState *state);
guint sooshi_convert_to_int24(gchar *buffer);
guint16 sooshi_convert_to_uint16(guint8 *buffer);
void sooshi_parse_response(SooshiState *state);
void sooshi_parse_admin_tree(SooshiState *state, gulong compressed_size, guint8 *buffer);


// Bluetooth Scanning
gboolean sooshi_start_scan(SooshiState *state);
gboolean sooshi_stop_scan(SooshiState *state);
void sooshi_wait_until_mooshimeter_found(SooshiState *state, gint64 timeout);
void sooshi_connect_mooshi(SooshiState *state);


#endif
