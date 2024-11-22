#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 256
#define MAX_KEY_LENGTH 128
#define MAX_VALUE_LENGTH 128

typedef struct Config {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
} Config;

Config* parse_config(const char* filename, int* count) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    Config* configs = NULL;
    char line[MAX_LINE_LENGTH];
    *count = 0;

    while (fgets(line, sizeof(line), file)) {
        char* equals_sign = strchr(line, '=');
        if (equals_sign) {
            *equals_sign = '\0';
            char* key = line;
            char* value = equals_sign + 1;

            // Remove newline character from value
            value[strcspn(value, "\n")] = '\0';

            configs = realloc(configs, sizeof(Config) * (*count + 1));
            if (!configs) {
                perror("Failed to allocate memory");
                fclose(file);
                return NULL;
            }

            strncpy(configs[*count].key, key, MAX_KEY_LENGTH);
            strncpy(configs[*count].value, value, MAX_VALUE_LENGTH);
            (*count)++;
        }
    }

    fclose(file);
    return configs;
}

void free_config(Config* configs) {
    free(configs);
}

int main() {
    int count;
    Config* configs = parse_config("config.txt", &count);

    if (configs) {
        for (int i = 0; i < count; i++) {
            printf("Key: %s, Value: %s\n", configs[i].key, configs[i].value);
        }
        free_config(configs);
    }

    return 0;
}