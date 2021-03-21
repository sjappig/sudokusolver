#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "0.0.2"

#define LOG_INF(fmt, ...) fprintf(stdout, "[INF] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) fprintf(stderr, "[ERR] " fmt "\n", ##__VA_ARGS__)

const uint16_t BIT_LOOKUP[9] = {1, 2, 4, 8, 16, 32, 64, 128, 256};  

typedef struct {
    uint16_t possible_numbers;
} field;

typedef struct {
    field* fields[9];
} block;

typedef struct {
    field* fields[9];
} row;

typedef struct {
    field* fields[9];
} column;

typedef struct {
    field fields[9][9];
    block blocks[9];
    row rows[9];
    column columns[9];
} sudoku;

// https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSet64
uint8_t count_bits(uint16_t v) {
    return (uint8_t)((v * 0x200040008001ULL & 0x111111111111111ULL) % 0xf);
}

char field_to_char(const field* f) {
    uint8_t nbits = count_bits(f->possible_numbers);

    if (nbits == 0) {
        return 'e';
    }

    if (nbits > 1) {
        return ' ';
    }

    for (uint8_t i = 0; i < 9; ++i) {
        if (BIT_LOOKUP[i] == f->possible_numbers) {
            return (char)(i + 49);
        }
    }

    return 'E';
}    

void init_derived_fields(sudoku* s) {
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            s->rows[j].fields[i] = &s->fields[i][j];
            s->columns[i].fields[j] = &s->fields[i][j];
            int block_num = j / 3 + i / 3 * 3;
            int inter_block_num = j % 3 + i % 3 * 3;
            s->blocks[block_num].fields[inter_block_num] = &s->fields[i][j];
        }
    }
}

int read_sudoku(const char* filename, sudoku* s) {
    FILE* fp = fopen(filename, "r");

    if (!fp) {
        LOG_ERR("Failed to open %s!", filename);
        return 1;
    }

    LOG_INF("Reading %s...", filename);
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            int c = fgetc(fp);
            if (c == EOF) {
                LOG_ERR("Error while reading the file!");
                return 1;
            }
            if (c == '|') {
                j -= 1;
                continue;
            }
            if (c == '-') {
                if (j > 0) {
                    LOG_ERR("Unexpected separator '-'");
                    return 1;
                }
                while (fgetc(fp) != '\n') {
                }
                fseek(fp, -1, SEEK_CUR);
                i -= 1;
                break;
            }
            if (c == ' ') {
                s->fields[i][j].possible_numbers = (1 << 9) - 1;
            } else if (c >= 49 && c <= 57) {
                s->fields[i][j].possible_numbers = 1 << (c - 49);
            } else {
                LOG_ERR("Unexpected character '%c'", (char)c);
                return 1;
            }
        }
        int eol = fgetc(fp);
        if (eol == '|')
            eol = fgetc(fp);

        if (eol != '\n') {
            LOG_ERR("Expected end of line, got '%c'", (char)eol);
            return 1;
        }
    }

    fclose(fp);

    init_derived_fields(s);

    return 0;
}

void print_sudoku(const sudoku* s) {
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            printf("%c", field_to_char(s->columns[i].fields[j]));
            if ((j+1) % 3 == 0 && j != 8)
                printf("|");
        }
        printf("\n");
        if ((i+1) % 3 == 0 && i != 8)
            printf("---+---+---\n");
    }
}

void print_sudoku_block(const sudoku* s, int block_num) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            printf("%c", field_to_char(s->blocks[block_num].fields[j + i*3]));
        }
        printf("\n");
    }
}

int has_fields_error(const field* fields[9], int* error_idx) {
    uint16_t used_bits = 0;
    for (int i = 0; i < 9; ++i) {
        uint16_t value = fields[i]->possible_numbers;
        uint8_t nbits = count_bits(value);
        if (nbits == 0) {
            *error_idx = i;
            return 1;
        }
        if (nbits == 1) {
            if (used_bits & value) {
                *error_idx = i;
                return 1;
            }
            used_bits |= value;
        }
    }
    return 0;
}

int has_sudoku_error(const sudoku* s, int log_error) {
    for (int i = 0; i < 9; ++i) {
        int error_idx = 0;
        if (has_fields_error(s->blocks[i].fields, &error_idx)) {
            if (log_error)
                LOG_ERR("Error in block %d, index %d", i, error_idx);
            return 1;
        }
        if (has_fields_error(s->rows[i].fields, &error_idx)) {
            if (log_error)
                LOG_ERR("Error in column %d, index %d", i, error_idx);
            return 1;
        }
        if (has_fields_error(s->columns[i].fields, &error_idx)) {
            if (log_error)
                LOG_ERR("Error in row %d, index %d", i, error_idx);
            return 1;
        }
    }
    return 0;
}

int simplify_fields(field* fields[9]) {
    int rv = 0;
    for (int i = 0; i < 9; ++i) {
        uint16_t value = fields[i]->possible_numbers;
        uint8_t nbits = count_bits(value);
        if (nbits == 1) {
            for (int j = 0; j < 9; ++j) {
                if (j != i) {
                    uint16_t old_value = fields[j]->possible_numbers;
                    fields[j]->possible_numbers &= ~value;
                    rv |= old_value ^ fields[j]->possible_numbers;
                }
            }
        } else if (nbits > 1) {
            uint16_t available_bits = (1 << 9) - 1;
            for (int j = 0; j < 9; ++j) {
                if (j != i) {
                    available_bits &= ~fields[j]->possible_numbers;
                }
            }
            if (count_bits(available_bits) == 1) {
                if (value & available_bits) {
                    fields[i]->possible_numbers = available_bits;
                    rv |= 1;
                }
            }
        }
    }
    return rv;
}

int is_sudoku_solved(const sudoku* s) {
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            uint8_t nbits = count_bits(s->fields[i][j].possible_numbers);
            if (nbits != 1)
                return 0;

        }
    }
    return 1;
}

int simplify_sudoku(sudoku* s) {
    int rv = 0;
    for (int i = 0; i < 9; ++i) {
        int error_idx = 0;
        rv |= simplify_fields(s->blocks[i].fields);
        if (has_fields_error(s->blocks[i].fields, &error_idx)) {
            return 0;
        }
        rv |= simplify_fields(s->rows[i].fields);
        if (has_fields_error(s->rows[i].fields, &error_idx)) {
            return 0;
        }
        rv |= simplify_fields(s->columns[i].fields);
        if (has_fields_error(s->columns[i].fields, &error_idx)) {
            return 0;
        }
    }
    return rv;
}

void copy_sudoku_with_guess(sudoku* dest, const sudoku* src, int guesses) {
    memcpy(dest, src, sizeof(sudoku));
    init_derived_fields(dest);

    int n_guess = 0;

    while (n_guess < guesses) {
        int type = rand() % 3;
        int index = rand() % 9;
        int sub_index = rand() % 9;
        field* field_to_change = NULL;
        switch (type) {
            case 0:
                field_to_change = dest->blocks[index].fields[sub_index];
                break;
            case 1:
                field_to_change = dest->rows[index].fields[sub_index];
                break;
            case 2:
                field_to_change = dest->columns[index].fields[sub_index];
                break;
        }
        if (count_bits(field_to_change->possible_numbers) <= 1)
            continue;

        uint16_t guess = (1 << (rand() % 8)) & field_to_change->possible_numbers;
        if (!guess)
            continue;

        field_to_change->possible_numbers = guess;
        ++n_guess;
    }
}

int solve_sudoku(sudoku* s, int max_rounds) {
    while (simplify_sudoku(s)) {
    }
    if (is_sudoku_solved(s)) {
        return 1;
    }

    int round = 0;
    sudoku fork_s;
    LOG_INF("Guessing numbers...");
    while (round++ < max_rounds) {
        copy_sudoku_with_guess(&fork_s, s, 1);
        while (simplify_sudoku(&fork_s)) {
        }
        if (is_sudoku_solved(&fork_s)) {
            copy_sudoku_with_guess(s, &fork_s, 0);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    sudoku s;
    memset(&s, 0, sizeof(sudoku));

    LOG_INF("sudokusolver %s", VERSION);

    if (argc != 2) {
        LOG_ERR("usage: %s <file>", argv[0]);
        return 1;
    }

    if (read_sudoku(argv[1], &s)) {
        LOG_ERR("Exiting since error occured");
        return 1;
    }

    LOG_INF("Read sudoku:");
    print_sudoku(&s);

    if (has_sudoku_error(&s, 1)) {
        LOG_ERR("Exiting since given sudoku has errors");
        return 1;
    }

    LOG_INF("Given sudoku seems correct, trying to solve");
    
    if (!solve_sudoku(&s, 100)) {
        LOG_ERR("Solving sudoku failed");
    } else {
        LOG_INF("Sudoku solved!");
    }
    if (has_sudoku_error(&s, 1)) {
        LOG_ERR("Sudoko has error");
    }
    LOG_INF("Final sudoku:");
    print_sudoku(&s);
    LOG_INF("Done");
    return 0; 
}
