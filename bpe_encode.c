// Faster algorithm with frequency table calculated once and adjusted on each iteration
// and optimized Hash-table with ability to tombstone elements

#include <stdio.h>
#include <assert.h>

#include "bpe.h"

// Hashing 2-symboled key, HASH_SHIFT bits for each symbol
#define HASH_SHIFT 10
#define HASH_TABLE_SIZE 1 << (HASH_SHIFT * 2)

// hash function
uint32_t hash_pair(Pair pair) {
    return pair.l | (pair.r << HASH_SHIFT);
}

void hash_init(Freqs *freqs) {
    freqs->items = malloc(sizeof(Freq) * HASH_TABLE_SIZE);
    memset(freqs->items, 0, sizeof(Freq) * HASH_TABLE_SIZE);
    freqs->capacity = HASH_TABLE_SIZE;
}

// returns an index for a given pair
// this approach makes it possible to de-occupy entry using tombstone marking
uint32_t hash_geti(Freqs freqs, Pair pair) {
    uint32_t h = hash_pair(pair) % freqs.capacity;
    uint32_t tombstone = 0;
    bool tombstone_found = false;

    for (size_t i = 0; i < freqs.capacity; i++) {
        Freq *e = &freqs.items[h];

        // empty found, insert here or on tombstone, if found
        if (!e->occupied) return tombstone_found ? tombstone : h;

        if (e->deleted) {
            // mark first found deleted element
            if (!tombstone_found) tombstone = h;
            tombstone_found = true;
        } else if (pair_cmp(e->key, pair)) {
            // found existing key
            return h;
        }

        h = (h + 1) % freqs.capacity;
    }

    if (tombstone_found) return tombstone;

    printf("Hash-table overflow\n");
    abort();
}

#define PROFILE_ITER_WINDOW 1000

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    char *input_name = argv[1];

    FILE *input_file = fopen(input_name, "rb");
    if (!input_file) {
        printf("Can't open file: %s\n", input_name);
        return 1;
    }

    if (fseek(input_file, 0, SEEK_END) != 0) {
        printf("Can't seek the end of the file: %s\n", input_name);
        fclose(input_file);
        return 1;
    }

    long fsize = ftell(input_file);
    if (fsize < 0) {
        printf("Can't get file size: %s\n", input_name);
        fclose(input_file);
        return 1;
    }
    rewind(input_file);

    char *text = malloc(fsize + 1);
    if (!text) {
        printf("Can't allocate %ld bytes\n", fsize + 1);
        fclose(input_file);
        return 1;
    }
    size_t nread = fread((void*)text, 1, fsize, input_file);
    if (nread < (size_t)fsize && ferror(input_file)) {
        printf("Can't read file: %s\n", input_name);
        fclose(input_file);
        return 1;
    }
    fclose(input_file);
    text[nread] = '\0';

    // --- Main app
    double total_time = get_secs();

    Table table = {0};
    Tokens tokens = {0};
    Tokens tokens_temp = {0};
    Freqs freqs = {0};
    hash_init(&freqs);

    int test_size = strlen(text);
    for (int i = 0; i < test_size; i++)
        tokens_append(&tokens, (uint32_t)text[i]);
    free(text);

    int tokens_size = tokens.count;
    for (int i = 0; i < tokens_size - 1; i++) {
        Pair pair = {.l = tokens.items[i], .r = tokens.items[i + 1]};
        int32_t place = hash_geti(freqs, pair);
        assert(!freqs.items[place].deleted);
        if (freqs.items[place].occupied) {
            freqs.items[place].value += 1;
        } else {
            freqs.items[place].occupied = true;
            freqs.items[place].deleted = false;
            freqs.items[place].key = pair;
            freqs.items[place].value = 1;
        }
    }

    double begin_profile_total = 0;
    double begin_profile_search = 0;
    double begin_profile_replace = 0;
    double begin;

    // the loop
    size_t iter_count;
    for (iter_count = 0;; iter_count++) {
        if (iter_count % PROFILE_ITER_WINDOW == 1) begin_profile_total = get_secs();

        if (iter_count % PROFILE_ITER_WINDOW == 0) {
            printf("Table size: %zu\n", table.count);
            printf("Tokens in size: %zu\n", tokens.count);
        }

        begin = get_secs();
        Freq* max_freq_elem = NULL;
        for (size_t i = 0; i < freqs.capacity; i++) {
            Freq* elem = &freqs.items[i];
            if (!elem->occupied || elem->deleted) continue;

            // compare by keys in case of identical frequency for greater predictability
            if (max_freq_elem == NULL || elem->value > max_freq_elem->value ||
                (elem->value == max_freq_elem->value && memcmp(&elem->key, &max_freq_elem->key, sizeof(Pair)) > 0))
            max_freq_elem = elem;
        }
        begin_profile_search += get_secs() - begin;

        if (iter_count % PROFILE_ITER_WINDOW == 0) {
            printf("Most frequent pair found on avg in %.05lfs\n", begin_profile_search / PROFILE_ITER_WINDOW);
            begin_profile_search = 0;
            printf("Most frequent pair found %u times\n", max_freq_elem->value);
        }

        if (!max_freq_elem->occupied || max_freq_elem->deleted) {
            printf("No occupied hash element found!\n");
            abort();
        }
        if (max_freq_elem->value == 1) break;

        Entry e = {.pair=max_freq_elem->key, .id=(table.count + TABLE_OFFSET), .frequency=max_freq_elem->value};
        table_append(&table, e);

        begin = get_secs();
        tokens_temp.count = 0;
        for (size_t i = 0; i < tokens.count; ) {
            if (i + 1 < tokens.count &&
                pair_cmp(e.pair, (Pair){.l = tokens.items[i], .r = tokens.items[i + 1]})) {
                // Update frequency table while replacing most frequent pair
                // So freqs will be updated as such:
                // [abcd] -> [aZd] :
                //  * remove appearances of [ab] and [cd]
                //  * decrease frequency value of [bc], if zero - mark as tombstone
                //  * add [aZ] and [Zd] pairs
                int32_t place;
                Pair temp_pair = {0};

                // First process pair to the left of the most frequent pair
                if (tokens_temp.count > 0) {
                    temp_pair.l = tokens_temp.items[tokens_temp.count - 1];

                    temp_pair.r = tokens.items[i];
                    place = hash_geti(freqs, temp_pair);
                    assert(freqs.items[place].occupied);
                    assert(!freqs.items[place].deleted);
                    assert(freqs.items[place].value > 0);
                    freqs.items[place].value -= 1;
                    if (freqs.items[place].value == 0) freqs.items[place].deleted = true;

                    temp_pair.r = e.id;
                    place = hash_geti(freqs, temp_pair);
                    if (freqs.items[place].occupied && !freqs.items[place].deleted) {
                        freqs.items[place].value += 1;
                    } else {
                        freqs.items[place].occupied = true;
                        freqs.items[place].deleted = false;
                        freqs.items[place].key = temp_pair;
                        freqs.items[place].value = 1;
                    }
                }

                // Decrease frequency value of the most frequent pair
                place = hash_geti(freqs, e.pair);
                assert(freqs.items[place].occupied);
                assert(!freqs.items[place].deleted);
                assert(freqs.items[place].value > 0);
                freqs.items[place].value -= 1;
                if (freqs.items[place].value == 0) freqs.items[place].deleted = true;

                tokens_append(&tokens_temp, e.id);
                i += 2;

                // Last process pair to the right of the most frequent pair
                if (i < tokens.count) {
                    temp_pair.r = tokens.items[i];

                    temp_pair.l = tokens.items[i - 1];
                    place = hash_geti(freqs, temp_pair);
                    assert(freqs.items[place].occupied);
                    assert(!freqs.items[place].deleted);
                    assert(freqs.items[place].value > 0);
                    freqs.items[place].value -= 1;
                    if (freqs.items[place].value == 0) freqs.items[place].deleted = true;

                    temp_pair.l = e.id;
                    place = hash_geti(freqs, temp_pair);
                    if (freqs.items[place].occupied && !freqs.items[place].deleted) {
                        freqs.items[place].value += 1;
                    } else {
                        freqs.items[place].occupied = true;
                        freqs.items[place].deleted = false;
                        freqs.items[place].key = temp_pair;
                        freqs.items[place].value = 1;
                    }
                }
            } else {
                tokens_append(&tokens_temp, tokens.items[i]);
                i += 1;
            }
        }
        begin_profile_replace += get_secs() - begin;

        if (iter_count % PROFILE_ITER_WINDOW == 0) {
            printf("Most frequent pair replaced on avg in %.05lfs\n", begin_profile_replace / PROFILE_ITER_WINDOW);
            begin_profile_replace = 0;
            printf("Tokens out size: %zu\n", tokens_temp.count);
        }

        // swap
        Tokens *tokens_ptr = &tokens;
        tokens = tokens_temp;
        tokens_temp = *tokens_ptr;

        if (iter_count % PROFILE_ITER_WINDOW == 0) {
            printf("Total cycle complete on avg in %.05lfs\n", (get_secs() - begin_profile_total) / PROFILE_ITER_WINDOW);
            printf("---\n");
        }
    }

    printf("Total number of iterations: %zu\n", iter_count);
    printf("Total number of tokens in: %d\n", tokens_size);
    printf("Total number of tokens out: %zu\n", tokens.count);
    printf("Total number of pairs: %zu\n", table.count);

    // look for input file extention to extract base name
    int dot_pos = 0, start_pos = 0;
    size_t input_name_len = strlen(input_name);
    for (int i = input_name_len - 1; i >= 0; i--) {
        if (input_name[i] == '/' && (i > start_pos)) start_pos = i + 1;
        if (input_name[i] == '.' && (i > dot_pos)) dot_pos = i;
    }

    // allocate and copy name without extention
    size_t copy_len = ((dot_pos == 0) ? input_name_len : input_name_len - 4) - start_pos;
    size_t output_name_len = copy_len + 4;
    char *output_name = malloc(sizeof(char) * output_name_len);
    memcpy(output_name, input_name + start_pos, sizeof(char) * copy_len);

    // Save table to disk
    char *table_name = output_name;
    // adjust extention for table
    table_name[copy_len + 0] = '.';
    table_name[copy_len + 1] = 'b';
    table_name[copy_len + 2] = 'p';
    table_name[copy_len + 3] = 'e';
    table_name[copy_len + 4] = '\0';

    FILE *table_file = fopen(table_name, "wb");
    if (!table_file) {
        printf("Can't open file: %s\n", table_name);
        return 1;
    }
    if (fwrite(&table.count, sizeof(table.count), 1, table_file) != 1) {
        printf("Can't write size of a table to file: %s\n", table_name);
        fclose(table_file);
        return 1;
    }
    if (fwrite(table.items, sizeof(Entry), table.count, table_file) != table.count) {
        printf("Can't write a table to file: %s\n", table_name);
        fclose(table_file);
        return 1;
    }
    if (fclose(table_file) != 0) {
        printf("Can't close file: %s\n", table_name);
        return 1;
    }
    printf("BPE table saved to file: %s\n", table_name);

    // Save tokens to disk
    char *tokens_name = output_name;
    // adjust extention for table
    tokens_name[copy_len + 0] = '.';
    tokens_name[copy_len + 1] = 't';
    tokens_name[copy_len + 2] = 'k';
    tokens_name[copy_len + 3] = 'n';
    tokens_name[copy_len + 4] = '\0';

    FILE *tokens_file = fopen(tokens_name, "wb");
    if (!tokens_file) {
        printf("Can't open file: %s\n", tokens_name);
        return 1;
    }
    if (fwrite(&tokens.count, sizeof(tokens.count), 1, tokens_file) != 1) {
        printf("Can't write amount of tokens to file: %s\n", tokens_name);
        fclose(tokens_file);
        return 1;
    }
    if (fwrite(tokens.items, sizeof(uint32_t), tokens.count, tokens_file) != tokens.count) {
        printf("Can't write a tokens to file: %s\n", tokens_name);
        fclose(tokens_file);
        return 1;
    }
    if (fclose(tokens_file) != 0) {
        printf("Can't close file: %s\n", tokens_name);
        return 1;
    }
    printf("Comprased tokens saved to file: %s\n", tokens_name);

    printf("Total execution time: %.03lfs\n", get_secs() - total_time);
    return 0;
}