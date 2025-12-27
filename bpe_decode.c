#include <stdio.h>
#include <assert.h>

#include "bpe.h"

int main(int argc, char* argv[]) {
    char *table_name = NULL;
    char *tokens_name = NULL;

    for (int i = 1; i < argc;) {
        if (strcmp(argv[i], "-bpe") == 0 && (i + 1) < argc) {
            table_name = argv[i + 1];
            i += 2;
            continue;
        }
        if (strcmp(argv[i], "-tkn") == 0 && (i + 1) < argc) {
            tokens_name = argv[i + 1];
            i += 2;
            continue;
        }
        i++;
    }

    if (table_name == NULL || tokens_name == NULL) {
        printf("Usage: %s -bpe input.bpe -tkn input.tkn <-o output>\n", argv[0]);
        return 1;
    }

    Table table = {0};
    Tokens tokens = {0};

    // Read table from file
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
    table.items = realloc(table.items, sizeof(Entry) * table.capacity);
    if (fread(table.items, sizeof(Entry), table.count, table_file) != table.count) {
        printf("Can't read a table from file: %s\n", table_name);
        fclose(table_file);
        return 1;
    }
    if (fclose(table_file) != 0) {
        printf("Can't close file: %s\n", table_name);
        return 1;
    }

    // Read tokens from file
    FILE *tokens_file = fopen(tokens_name, "rb");
    if (!tokens_file) {
        printf("Can't open file: %s\n", tokens_name);
        return 1;
    }
    if (fread(&tokens.count, sizeof(tokens.count), 1, tokens_file) != 1) {
        printf("Can't read amount of tokens from file: %s\n", tokens_name);
        fclose(tokens_file);
        return 1;
    }
    tokens.capacity = tokens.count;
    tokens.items = realloc(tokens.items, sizeof(uint32_t)*tokens.capacity);
    if (fread(tokens.items, sizeof(uint32_t), tokens.count, tokens_file) != tokens.count) {
        printf("Can't read tokens from file: %s\n", tokens_name);
        fclose(tokens_file);
        return 1;
    }
    if (fclose(tokens_file) != 0) {
        printf("Can't close file: %s\n", tokens_name);
        return 1;
    }

    // restore text from table and tokens
    String restored_text = {0};
    for (size_t i = 0; i < tokens.count; i++) {
        render_id(table, tokens.items[i], &restored_text, false);
    }

    // Print restored text to console
    string_append(&restored_text, '\0');
    printf("%s\n", restored_text.items);

    return 0;
}