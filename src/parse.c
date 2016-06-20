#include "sooshi.h"

static SooshiNode *
sooshi_parse_node(SooshiState *state, SooshiNode *parent, const guint8 *buffer, gulong *bytes_read)
{
    static guchar opcode  = 0;

    SooshiNode *node = g_new0(SooshiNode, 1);
    
    node->parent = parent;
    node->type = (SOOSHI_NODE_TYPE)((gchar)buffer[0]);
    node->value = NULL;
    node->subscriber = NULL;
    node->op_code = 0;
    node->has_value = FALSE;
    if (node->type >= CHOOSER)
    {
        node->op_code = opcode++;
        g_ptr_array_add(state->op_code_map, (gpointer)node);
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


static void
sooshi_parse_admin_tree(SooshiState *state, gulong compressed_size, const guint8 *buffer)
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

    // Calculate CRC32 checksum of zipped payload
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

void
sooshi_parse_response(SooshiState *state)
{
    guint8 op_code = state->buffer->data[0];
    guint16 length = 0;

    if (op_code == 1)
    {
        length = state->buffer->data[1] | state->buffer->data[2] << 8;

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

        SooshiNode *node = (SooshiNode*)g_ptr_array_index(state->op_code_map, op_code);

        if (op_code == 3)
        {
            gint64 time = g_get_real_time() - state->pcb_start;
            g_info("Ping-Time: %.2fms", time/1000.0);
        }

        GVariant *v = sooshi_node_bytes_to_value(node, state->buffer);

        // bytes_to_value might return null if there's not enough data here
        // yet to fully parse a string
        if (v == NULL)
            return;

        sooshi_node_set_value(state, node, v, FALSE);

        gchar *strval = sooshi_node_value_as_string(node);
        g_info("Value for node '%s' updated: [%s]", node->name, strval);
        g_free(strval);

        sooshi_node_notify_subscribers(state, node);

        if (node->op_code == 25)
        {
            gint64 time = g_get_real_time() - state->pcb_start;
            g_info("Last time updated: %.2fms (Frequency: %.2fHz)", time / 1000.0, 1.0 / time);
            state->pcb_start = g_get_real_time();
        }

        // We have set and received back the CRC32 checksum of the tree - setup is finished
        if (node->op_code == 0 && state->initialized == FALSE)
        {
            sooshi_request_all_node_values(state, NULL);
	    sooshi_on_mooshi_initialized(state);
            state->initialized = TRUE;
        }
    }
}

