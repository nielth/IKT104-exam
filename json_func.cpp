#include "mbed.h"
#include "main.h"
#include "jsmn.h"
#include <cstring>

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

char *json_parser(char *raw_json, char *key){
    int r;
    jsmn_parser p;
    jsmntok_t t[128];
    char *str = (char*)malloc(sizeof(char) * 130);

    char *ptr = (char*)malloc(sizeof(char) * 30); 

    jsmn_init(&p);
    r = jsmn_parse(&p, raw_json, strlen(raw_json), t,
             sizeof(t) / sizeof(t[0]));

    if (r < 0) {
        printf("Failed to parse JSON: %d\n", r);
        return NULL;
    }

    /* Assume the top-level element is an object */
    if (r < 1 || t[0].type != JSMN_OBJECT) {
        printf("Object expected\n");
        return NULL;
    }
    int value_length;

    for (int j = 1; j < r; j++) {
        if (jsoneq(raw_json, &t[j], key) == 0) {
        strcpy(ptr, raw_json + t[j + 1].start);
        value_length = t[j + 1].end - t[j + 1].start;
        strncpy(ptr, raw_json + t[j + 1].start, value_length);
        ptr[value_length] = '\0';
        j++;
        break;
        } 
    }
    return ptr;
}
