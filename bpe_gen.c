#include <stdio.h>
#include <assert.h>

#include "bpe.h"

uint32_t get_id_weighted_random(Table table) {
    uint64_t freq_sum = 0;
    for (size_t i = 0; i < table.count; i++)
        freq_sum += table.items[i].frequency;

    size_t roll = ((uint64_t)rand() * freq_sum) / RAND_MAX;

    freq_sum = 0;
    for (size_t chosen_token = 0; chosen_token < table.count; chosen_token++) {
        freq_sum += table.items[chosen_token].frequency;
        if (freq_sum >= roll) return table.items[chosen_token].id;
    }

    printf("UNREACHABLE: get_id_weighted_random()\n");
    abort();
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

    srand(time(0));
    Table next = {0};
    String str_buffer = {0};

    // get first random token
    uint32_t chosen_id = get_id_weighted_random(table);

    size_t limit = 100;
    for (size_t i = 0; i < limit; i++) {
        // print chosen token
        render_id(table, chosen_id, &str_buffer, false);

        // create pool of next tokens
        next.count = 0;
        for (;;) {
            for (size_t j = 0; j < table.count; j++) {
                if (chosen_id == table.items[j].pair.l)
                    table_append(&next, table.items[j]);
            }

            if (next.count > 0) break;
            if (chosen_id < TABLE_OFFSET) break;

            // split token in half and repeat the search for only the right part if none found
            chosen_id = table_get(table, chosen_id)->pair.r;
        }

        if (next.count == 0) break;

        // taking only the right part of the random token from the collected pool
        chosen_id = table_get(next, get_id_weighted_random(next))->pair.r;
    }

    string_append(&str_buffer, (char)(0));
    printf("||%s||\n", str_buffer.items);

    return 0;
}