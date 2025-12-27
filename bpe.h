#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

typedef struct {
    uint32_t *items;
    size_t count;
    size_t capacity;
} Tokens;

void tokens_append(Tokens *to, uint32_t item) {
    if (to->count == to->capacity) {
        if (to->capacity == 0) to->capacity = 256;
        else to->capacity *= 2;
        to->items = realloc(to->items, sizeof(uint32_t)*to->capacity);
    }
    to->items[to->count++] = item;
}

typedef struct {
    uint32_t l, r;
} Pair; 

bool pair_cmp(Pair l, Pair r) {
    return memcmp(&l, &r, sizeof(Pair)) == 0;
}

typedef struct {
    Pair key;
    uint32_t value;
    bool occupied;
    bool deleted;
} Freq;

typedef struct {
    Freq *items;
    size_t capacity;
} Freqs;

// The starting value for the IDs in the compression table
// Also used to distinguish if character is printable
#define TABLE_OFFSET (1 << 8 * sizeof(char))

typedef struct {
    Pair pair;
    uint32_t id;
    uint32_t frequency;
} Entry;

typedef struct {
    Entry *items;
    size_t count;
    size_t capacity;
} Table;

void table_append(Table *to, Entry item) {
    if (to->count == to->capacity) {
        if (to->capacity == 0) to->capacity = 256;
        else to->capacity *= 2;
        to->items = realloc(to->items, sizeof(Entry)*to->capacity);
    }
    to->items[to->count++] = item;
}

double get_secs() {
    struct timespec tp = {0};
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (double)tp.tv_sec + (double)tp.tv_nsec*1e-9;
}

Entry *table_get(Table table, uint32_t id) {
    for (size_t i = 0; i < table.count; i++)
        if (table.items[i].id == id)
            return &table.items[i];
    return NULL;
}

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} String;

void string_append(String *to, char item) {
    if (to->count == to->capacity) {
        if (to->capacity == 0) to->capacity = 256;
        else to->capacity *= 2;
        to->items = realloc(to->items, sizeof(char)*to->capacity);
    }
    to->items[to->count++] = item;
}

void render_id(Table table, uint32_t id, String *str, bool inspect_mode) {
    if (id < TABLE_OFFSET) {
        if (inspect_mode && !isprint((char)id)) {
            char temp_buf[10];
            sprintf(temp_buf, "\\x%02X", (uint8_t)id);
            for (size_t i = 0; i < strlen(temp_buf); i++) string_append(str, temp_buf[i]);
        } else {
            string_append(str, (char)id);
        }
    } else {
        Entry *e = table_get(table, id);
        assert(e != NULL);
        render_id(table, e->pair.l, str, inspect_mode);
        render_id(table, e->pair.r, str, inspect_mode);
    } 
}