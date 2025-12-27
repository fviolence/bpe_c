#include <stdio.h>
#include <assert.h>

#include "bpe.h"

void render_id_tree(Table table, uint32_t id, String *header, String *str) {
    string_append(header, '|');
    string_append(header, ' ');

    // Save a copy of a header for right part of the pair
    String header_save = {0};
    header_save.count = header->count;
    header_save.capacity = header->capacity;
    header_save.items = realloc(header_save.items, sizeof(char)*header_save.capacity);
    memcpy(header_save.items, header->items, sizeof(char) * header_save.count);

    for (size_t i = 0; i < header->count; i++)
        string_append(str, header->items[i]);
    string_append(str, '-');
    string_append(str, '>');
    string_append(str, ' ');

    if (id < TABLE_OFFSET) {
        if (!isprint((char)id)) {
            char temp_buf[10];
            sprintf(temp_buf, "\\x%02X", (uint8_t)id);
            for (size_t i = 0; i < strlen(temp_buf); i++) string_append(str, temp_buf[i]);
        } else {
            string_append(str, '[');
            string_append(str, (char)id);
            string_append(str, ']');
        }
        string_append(str, '\n');
    } else {
        char temp_buf[10];
        sprintf(temp_buf, "(0x%05x)", (uint8_t)id);
        for (size_t i = 0; i < strlen(temp_buf); i++) string_append(str, temp_buf[i]);
        string_append(str, '\n');

        Entry *e = table_get(table, id);
        assert(e != NULL);
        render_id_tree(table, e->pair.l, header, str);
        render_id_tree(table, e->pair.r, &header_save, str);
    }
    free(header_save.items);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    Table table = {0};

    // Read table from file
    char *table_name = argv[1];
    FILE *table_file = fopen(table_name, "rb");
    if (!table_file) {
        printf("Can't open file: %s\n", table_name);
        return 1;
    }
    if (fread(&table.count, sizeof(table.count), 1, table_file) != 1) {
        printf("Can't read size of a table from file: %s\n", table_name);
        fclose(table_file);
        return 1;
    }
    table.capacity = table.count;
    table.items = realloc(table.items, sizeof(Entry)*table.capacity);
    if (fread(table.items, sizeof(Entry), table.count, table_file) != table.count) {
        printf("Can't read a table from file: %s\n", table_name);
        fclose(table_file);
        return 1;
    }
    if (fclose(table_file) != 0) {
        printf("Can't close file: %s\n", table_name);
        return 1;
    }

    // print table
    String str_buffer = {0};
    String tree_header = {0};
    for (size_t i = 0; i < table.count; i++) {
        str_buffer.count = 0;
        render_id(table, table.items[i].id, &str_buffer, true);
        string_append(&str_buffer, (char)(0));
        printf("(0x%05x) -> [%s] (%u)\n", table.items[i].id, str_buffer.items, table.items[i].frequency);
        str_buffer.count = 0;
        tree_header.count = 0;
        render_id_tree(table, table.items[i].pair.l, &tree_header, &str_buffer);
        tree_header.count = 0;
        render_id_tree(table, table.items[i].pair.r, &tree_header, &str_buffer);
        string_append(&str_buffer, (char)(0));
        printf("%s\n", str_buffer.items);
    }

    return 0;
}