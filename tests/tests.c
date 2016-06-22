#include <glib.h>
#include <string.h>
#include <sooshi.h>

#ifndef g_assert_cmpmem
    #define g_assert_cmpmem(m1, l1, m2, l2) g_assert_true((l1) == (l2) && memcmp((m1), (m2), (l1)) == 0)
#endif

typedef struct
{
    SooshiState *state;
} StateWrapper;

static void
state_wrapper_set_up(StateWrapper *wrapper, gconstpointer user_data)
{
    wrapper->state = sooshi_state_new();
}

static void
state_wrapper_tear_down(StateWrapper *wrapper, gconstpointer user_data)
{
    sooshi_state_delete(wrapper->state);
}

static void
test_parse_chooser(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0x06 };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = CHOOSER;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_BYTE));
    g_assert_cmpuint(g_variant_get_byte(node->value), ==, 0x06);

    guchar buffer2[1];
    gint length = sooshi_node_value_to_bytes(node, buffer2);
    g_assert_cmpint(length, ==, sizeof(buffer2));
    g_assert_cmpmem(buffer2, length, buffer + 1, sizeof(buffer) - 1); 

    g_free(node);
}

static void
test_parse_uint8(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0xAB };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_U8;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_BYTE));
    g_assert_cmpuint(g_variant_get_byte(node->value), ==, 0xAB);

    guchar buffer2[1];
    gint length = sooshi_node_value_to_bytes(node, buffer2);
    g_assert_cmpint(length, ==, sizeof(buffer2));
    g_assert_cmpmem(buffer2, length, buffer + 1, sizeof(buffer) - 1); 

    g_free(node);
}


static void
test_parse_uint16(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0xAB, 0xCD };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_U16;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_UINT16));
    g_assert_cmpuint(g_variant_get_uint16(node->value), ==, (guint16) 0xCDAB);

    guchar buffer2[2];
    gint length = sooshi_node_value_to_bytes(node, buffer2);
    g_assert_cmpint(length, ==, sizeof(buffer2));
    g_assert_cmpmem(buffer2, length, buffer + 1, sizeof(buffer) - 1); 

    g_free(node);
}

static void
test_parse_uint32(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0x12, 0x34, 0x56, 0x78 };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_U32;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_UINT32));
    g_assert_cmpuint(g_variant_get_uint32(node->value), ==, (guint32) 0x78563412);

    guchar buffer2[4];
    gint length = sooshi_node_value_to_bytes(node, buffer2);
    g_assert_cmpint(length, ==, sizeof(buffer2));
    g_assert_cmpmem(buffer2, length, buffer + 1, sizeof(buffer) - 1); 

    g_free(node);
}

static void
test_parse_int8(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0xAB };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_S8;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_BYTE));
    g_assert_cmpint(g_variant_get_byte(node->value), ==, 0xAB);

    guchar buffer2[1];
    gint length = sooshi_node_value_to_bytes(node, buffer2);
    g_assert_cmpint(length, ==, sizeof(buffer2));
    g_assert_cmpmem(buffer2, length, buffer + 1, sizeof(buffer) - 1); 

    g_free(node);
}

static void
test_parse_int16(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0xAB, 0xCD };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_S16;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_INT16));
    g_assert_cmpint(g_variant_get_int16(node->value), ==, (gint16) 0xCDAB);

    guchar buffer2[2];
    gint length = sooshi_node_value_to_bytes(node, buffer2);
    g_assert_cmpint(length, ==, sizeof(buffer2));
    g_assert_cmpmem(buffer2, length, buffer + 1, sizeof(buffer) - 1); 

    g_free(node);
}

static void
test_parse_int32(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0x12, 0x34, 0x56, 0x78 };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_S32;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_INT32));
    g_assert_cmpint(g_variant_get_int32(node->value), ==, (gint32) 0x78563412);

    guchar buffer2[4];
    gint length = sooshi_node_value_to_bytes(node, buffer2);
    g_assert_cmpint(length, ==, sizeof(buffer2));
    g_assert_cmpmem(buffer2, length, buffer + 1, sizeof(buffer) - 1); 

    g_free(node);
}

static void
test_parse_flt(StateWrapper *wrapper, gconstpointer user_data)
{
    float real_value = 12.3456789;
    guchar buffer[] = { 0x00,
        ((guchar*)&real_value)[0],
        ((guchar*)&real_value)[1],
        ((guchar*)&real_value)[2],
        ((guchar*)&real_value)[3],
    };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_FLT;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_DOUBLE));
    g_assert_cmpfloat(g_variant_get_double(node->value), ==, real_value);

    guchar buffer2[4];
    gint length = sooshi_node_value_to_bytes(node, buffer2);
    g_assert_cmpint(length, ==, sizeof(buffer2));
    g_assert_cmpmem(buffer2, length, buffer + 1, sizeof(buffer) - 1); 

    g_free(node);
}

static void
test_parse_str(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0x06, 0x00, 0x73, 0x6f, 0x6f, 0x73, 0x68, 0x69 };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_STR;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_STRING));
    g_assert_cmpstr(g_variant_get_string(node->value, NULL), ==, "sooshi");

    guchar buffer2[8];
    gint length = sooshi_node_value_to_bytes(node, buffer2);
    g_assert_cmpint(length, ==, sizeof(buffer2));
    g_assert_cmpmem(buffer2, length, buffer + 1, sizeof(buffer) - 1); 

    g_free(node);
}

static void
test_parse_partial_str(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0x0e, 0x00, 0x73, 0x6f, 0x6f, 0x73, 0x68, 0x69, 0x20 };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_STR;

    // First run should return NULL and not remove any bytes from the byte array
    gulong byte_array_size = wrapper->state->buffer->len;
    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);
    
    g_assert_null(node->value);
    g_assert_cmpuint(byte_array_size, ==, wrapper->state->buffer->len);

    guchar buffer2[] = { 0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67 };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer2, sizeof(buffer2));
    
    // Second run should now be able to succesfully parse the string
    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_STRING));
    g_assert_cmpstr(g_variant_get_string(node->value, NULL), ==, "sooshi testing");

    guchar buffer3[16];
    gint length = sooshi_node_value_to_bytes(node, buffer3);
    g_assert_cmpint(length, ==, sizeof(buffer3));
    g_assert_cmpmem(buffer3, sizeof(buffer) - 1, buffer + 1, sizeof(buffer) - 1); 
    g_assert_cmpmem(buffer3 + sizeof(buffer) - 1, sizeof(buffer3) - (sizeof(buffer) - 1), buffer2, sizeof(buffer2)); 

    g_free(node);
}

static void
test_parse_bin(StateWrapper *wrapper, gconstpointer user_data)
{
    guchar buffer[] = { 0x00, 0x06, 0x00, 0x73, 0x6f, 0x6f, 0x73, 0x68, 0x69 };
    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, buffer, sizeof(buffer));

    SooshiNode *node = g_new0(SooshiNode, 1);
    node->type = VAL_BIN;

    sooshi_node_bytes_to_value(node, wrapper->state->buffer, &node->value);

    g_assert_nonnull(node->value);
    g_assert_true(g_variant_is_of_type(node->value, G_VARIANT_TYPE_BYTESTRING));
    const gchar *mem = g_variant_get_bytestring(node->value);
    g_assert_cmpmem(mem, strlen(mem), buffer + 3, sizeof(buffer) - 3);

    g_free(node);
}

/*static void
test_parse_tree(StateWrapper *wrapper, gconstpointer user_data)
{
    unsigned char ztree[] = {
        // OP-Code for ADMIN:TREE
        0x01, 0x74, 0x01,

        // Zlib Compressed ADMIN:TREE
        0x78, 0x9c, 0xc5, 0x92, 0x4d, 0x72, 0xab, 0x30, 0x10, 0x84, 0xdb, 0x12,
        0x7f, 0x86, 0x54, 0x8e, 0xe2, 0x02, 0x1c, 0xbb, 0xb2, 0x15, 0x62, 0x8c,
        0x55, 0x05, 0x82, 0x92, 0x04, 0xef, 0x65, 0xc5, 0xfd, 0x6f, 0x91, 0x11,
        0xce, 0x26, 0x27, 0xc8, 0x66, 0xa6, 0x41, 0x68, 0xfa, 0xab, 0x1e, 0x80,
        0x37, 0xa4, 0xaa, 0x9f, 0x8c, 0x95, 0x69, 0xaa, 0x9d, 0xbe, 0xb6, 0x28,
        0x93, 0xe0, 0x88, 0x70, 0x2e, 0x7b, 0xa3, 0x06, 0x3b, 0xfb, 0x60, 0x34,
        0x64, 0xb5, 0xe8, 0x6e, 0xdf, 0xc8, 0x79, 0x33, 0x5b, 0x9c, 0x13, 0xab,
        0x26, 0x42, 0x5a, 0x04, 0x33, 0xd1, 0xbe, 0x06, 0x8d, 0x2a, 0xed, 0x54,
        0xd8, 0x37, 0xc8, 0xf3, 0xd3, 0x74, 0xe4, 0xac, 0x0a, 0x04, 0x14, 0x5e,
        0x4d, 0xcb, 0x68, 0xec, 0x20, 0x45, 0xe2, 0xf8, 0x4d, 0x0e, 0xd9, 0xb4,
        0x37, 0x40, 0xb6, 0xb7, 0x9a, 0xeb, 0xad, 0xe6, 0x9a, 0x34, 0xf5, 0xd1,
        0xda, 0x57, 0xfb, 0x78, 0xb5, 0xcf, 0xd8, 0x44, 0xda, 0xd3, 0x12, 0x9e,
        0x09, 0x04, 0x43, 0x41, 0xdc, 0x3f, 0x10, 0xef, 0x7f, 0x1e, 0xf7, 0xef,
        0x10, 0x79, 0x70, 0x66, 0x18, 0xc8, 0x49, 0xc8, 0xf9, 0xf1, 0x00, 0x32,
        0xcf, 0x4e, 0x23, 0xdb, 0x96, 0x7a, 0xb6, 0xc1, 0xd8, 0x75, 0x5e, 0x3d,
        0x7f, 0x3b, 0xce, 0x83, 0x94, 0x82, 0xa1, 0x93, 0xc2, 0xd8, 0x40, 0x6e,
        0x53, 0x23, 0x64, 0xe6, 0x83, 0x0a, 0xc7, 0xb1, 0x7e, 0x36, 0x85, 0xc8,
        0x27, 0xb5, 0x2c, 0x91, 0x13, 0xb9, 0x5e, 0x9d, 0x23, 0x1b, 0x4e, 0x10,
        0x4d, 0x04, 0x09, 0x34, 0x2d, 0x27, 0xc8, 0x2b, 0x03, 0x9f, 0x32, 0xff,
        0x54, 0x8e, 0x7a, 0xc8, 0xdc, 0x29, 0x3b, 0xd0, 0x6e, 0x20, 0x0a, 0x65,
        0xd5, 0xf8, 0xe5, 0x8d, 0x97, 0x48, 0x26, 0x52, 0x96, 0x07, 0xba, 0x89,
        0xc7, 0x66, 0xdd, 0xfa, 0x78, 0x90, 0xe3, 0x58, 0xd8, 0x6e, 0x25, 0x54,
        0x19, 0x23, 0x7a, 0x0a, 0x28, 0x25, 0x9f, 0xf0, 0x04, 0xae, 0x7b, 0xb7,
        0x78, 0x54, 0xef, 0x51, 0x8d, 0xbe, 0x6b, 0x39, 0x32, 0xb3, 0xd1, 0x41,
        0xd4, 0xfe, 0x22, 0xda, 0xe6, 0x31, 0xa8, 0x81, 0x04, 0x27, 0x10, 0x53,
        0xbb, 0xd7, 0x7f, 0xc2, 0x25, 0x7e, 0x5c, 0x24, 0xff, 0x2d, 0xeb, 0xff,
        0x7d, 0xe3, 0xd4, 0xeb, 0x4b, 0x83, 0x58, 0xaf, 0x71, 0x2f, 0x17, 0x5e,
        0x51, 0xe9, 0x88, 0x2d, 0x83, 0xb2, 0x9a, 0x52, 0x64, 0x71, 0xb1, 0x17,
        0x66, 0xcd, 0xa3, 0x38, 0x54, 0x71, 0xa8, 0x43, 0x9e, 0x5f, 0xf2, 0xd0,
        0xe5, 0x8f, 0x3e, 0x1e, 0xd2, 0xde, 0xcc, 0x3d, 0x9d, 0x5e, 0x13, 0xab,
        0xc2, 0x91, 0x1a, 0xf7, 0xe5, 0x9f, 0xc3, 0x37, 0x95, 0x42, 0x79, 0x4c
    };

    wrapper->state->buffer = g_byte_array_append(wrapper->state->buffer, ztree, sizeof(ztree));

    sooshi_parse_response(wrapper->state); 

    g_assert_nonnull(wrapper->state->root_node);
}*/

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("https://github.com/ghtyrant/libsooshi/issues/");

    g_test_add("/parser/chooser", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_chooser, state_wrapper_tear_down);

    g_test_add("/parser/uint8", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_uint8, state_wrapper_tear_down);

    g_test_add("/parser/uint16", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_uint16, state_wrapper_tear_down);

    g_test_add("/parser/uint32", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_uint32, state_wrapper_tear_down);

    g_test_add("/parser/int8", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_int8, state_wrapper_tear_down);

    g_test_add("/parser/int16", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_int16, state_wrapper_tear_down);

    g_test_add("/parser/int32", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_int32, state_wrapper_tear_down);

    g_test_add("/parser/float", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_flt, state_wrapper_tear_down);

    g_test_add("/parser/str", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_str, state_wrapper_tear_down);

    g_test_add("/parser/partial_str", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_partial_str, state_wrapper_tear_down);
    
    g_test_add("/parser/bin", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_bin, state_wrapper_tear_down);

    /*
    g_test_add("/parser/tree", StateWrapper, NULL,
            state_wrapper_set_up, test_parse_tree, state_wrapper_tear_down);
    */

    return g_test_run();
}
