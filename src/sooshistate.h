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

typedef enum
{
    NOTSET = -1,
    PLAIN,
    LINK,
    CHOOSER,
    VAL_U8,
    VAL_U16,
    VAL_U32,
    VAL_S8,
    VAL_S16,
    VAL_S32,
    VAL_STR,
    VAL_BIN,
    VAL_FLT
} SOOSHI_NODE_TYPE;

extern const gchar* const SOOSHI_NODE_TYPE_STR[];

#define SOOSHI_NODE_TYPE_TO_STR(x) (SOOSHI_NODE_TYPE_STR[(x)+1])

typedef struct _SooshiNode SooshiNode;
struct _SooshiNode
{
    gchar *name;
    guchar op_code;
    SOOSHI_NODE_TYPE type;
    GList *children;
    SooshiNode *parent;
    gboolean has_value;

    GVariant *value;

    GList *subscriber;
};

typedef guint32 crc32_t;
#define CRC32_POLYNOMIAL          0x04C11DB7
#define CRC32_INITIAL_REMAINDER   0xFFFFFFFF
#define CRC32_FINAL_XOR_VALUE     0xFFFFFFFF
#define CRC32_CHECK_VALUE         0xCBF43926
#define CRC32_WIDTH               (8 * sizeof(crc32_t))
#define CRC32_TOPBIT              (1 << (CRC32_WIDTH - 1))

typedef struct _SooshiState SooshiState;
typedef struct _SooshiStateClass SooshiStateClass;

typedef void (*sooshi_initialized_handler_t)(SooshiState *state);

struct _SooshiState
{
    GObject parent_instance;

    // DBus stuff
    GDBusObjectManager *object_manager;
    GDBusProxy* adapter;
    GDBusProxy* mooshimeter;

    gchar *mooshimeter_dbus_path;

    GDBusProxy *serial_in;
    GDBusProxy *serial_out;

    // Signals
    gulong properties_changed_id;
    gulong scan_signal_id;

    GMainLoop *loop;

    // Message parsing & sending
    GByteArray *buffer;
    guint send_sequence;
    guint recv_sequence;

    // Config tree root
    SooshiNode *root_node;
    GArray *op_code_map;

    //CRC32 helper
    crc32_t crc_table[256];

    // This will me called once the mooshimeter is initialized
    sooshi_initialized_handler_t init_handler;
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
typedef void (*sooshi_node_subscriber_t)(SooshiState *state, SooshiNode *node);

GType sooshi_state_get_type();
SooshiState *sooshi_state_new();
void sooshi_state_delete(SooshiState *state);
GDBusProxy *sooshi_dbus_find_interface_proxy_if(SooshiState *state, const gchar* interface_name, dbus_conditional_func_t cond_func, gpointer user_data);
gboolean sooshi_find_adapter(SooshiState *state);
gboolean sooshi_cond_is_mooshimeter(GDBusInterface *interface, gpointer user_data);
gboolean sooshi_cond_has_uuid(GDBusInterface *interface, gpointer user_data);
gboolean sooshi_find_mooshi(SooshiState *state);
void sooshi_add_mooshimeter(SooshiState *state, GDBusProxy *meter);
void sooshi_initialize_mooshi(SooshiState *state);
guint sooshi_convert_to_int24(gchar *buffer);
guint16 sooshi_convert_to_uint16(guint8 *buffer);
void sooshi_parse_response(SooshiState *state);
void sooshi_parse_admin_tree(SooshiState *state, gulong compressed_size, const guint8 *buffer);
void sooshi_debug_dump_tree(SooshiNode *node, gint indent);
void sooshi_enable_notify(SooshiState *state);
void sooshi_send_bytes(SooshiState *state, guchar *buffer, gsize len);
void sooshi_request_all_node_values(SooshiState *state, SooshiNode *start);

gboolean sooshi_heartbeat(gpointer user_data);
void sooshi_run(SooshiState *state, sooshi_initialized_handler_t init_handler);

// Node methods
SooshiNode *sooshi_node_find(SooshiState *state, gchar *path, SooshiNode *start);
void sooshi_node_set_value(SooshiState *state, SooshiNode *node, GVariant *value, gboolean send_update);
void sooshi_node_send_value(SooshiState *state, SooshiNode *node);
void sooshi_node_request_value(SooshiState *state, SooshiNode *node);
void sooshi_node_choose(SooshiState *state, SooshiNode *node);
void sooshi_node_subscribe(SooshiState *state, SooshiNode *node, sooshi_node_subscriber_t func);
void sooshi_node_notify_subscriber(SooshiState *state, SooshiNode *node);
gchar *sooshi_node_value_as_string(SooshiNode *node);

// Transfer helper functions
GVariant *sooshi_node_bytes_to_value(SooshiNode *node, GByteArray *buffer);
gint sooshi_node_value_to_bytes(SooshiNode *node, guchar *buffer);

// Bluetooth Scanning
gboolean sooshi_start_scan(SooshiState *state);
gboolean sooshi_stop_scan(SooshiState *state);
void sooshi_wait_until_mooshimeter_found(SooshiState *state, gint64 timeout);
void sooshi_connect_mooshi(SooshiState *state);

// CRC-32 Stuff
void sooshi_crc32_init(SooshiState *state);
crc32_t sooshi_crc32_calculate(SooshiState *state, guchar const message[], gint nBytes);


#endif
