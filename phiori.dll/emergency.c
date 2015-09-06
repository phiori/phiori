#include "emergency.h"
#include "phiori.h"
#include "shiori.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHIORI2_VERSION_MAGIC "SHIORI/2"
#define SHIORI3_VERSION_MAGIC "SHIORI/3"
#define SHIORI25_VERSION_STRING SHIORI2_VERSION_MAGIC ".5"
#define SHIORI30_VERSION_STRING SHIORI3_VERSION_MAGIC ".0"
#define SHIORI_200 "200 OK"
#define SHIORI_204 "204 No Content"
#define SHIORI_400 "400 Bad Request"
#define SHIORI_500 "500 Internal Server Error"

#define GET_STRING "GET"
#define VERSION_STRING "Version"
#define SENTENCE_STRING "Sentence"
#define EVENT_STRING "Event"
#define SENDER_STRING "Sender"
#define CHARSET_STRING "Charset"
#define ID_STRING "ID"
#define VALUE_STRING "Value"

#define PHIORI_FETUS_STRING "phiori/fetus"
#define US_ASCII_STRING "US-ASCII"

#define UNKNOWN_ERROR_MESSAGE "Unknown error."

#define LICENSE_URL "http://www.gnu.org/licenses/lgpl-3.0.html"
#define PHIORI_URL "http://phiori.github.io/"

#define KVARR_CAPACITY_STEP 4

typedef struct _SHIORI_KV {
    char *key;
    char *value;
} SHIORI_KV;

typedef struct _SHIORI_REQ {
    char *req;
    char *name;
    char *ver;
    SHIORI_KV *kvarr;
    size_t kvarr_capacity;
    size_t kvarr_count;
} SHIORI_REQ;

typedef struct _SHIORI_RES {
    char *ver;
    char *stat;
    SHIORI_KV *kvarr;
    size_t kvarr_capacity;
    size_t kvarr_count;
} SHIORI_RES;

void shiori_kvarr_expand(SHIORI_KV **kvarr, size_t *kvarr_capacity, size_t kvarr_count) {
    if (kvarr_count >= *kvarr_capacity) {
        *kvarr_capacity += KVARR_CAPACITY_STEP;
        if (*kvarr_capacity == KVARR_CAPACITY_STEP)
            *kvarr = calloc(*kvarr_capacity, sizeof(SHIORI_KV));
        else {
            SHIORI_KV *kv_t = realloc(*kvarr, *kvarr_capacity * sizeof(SHIORI_KV));
            if (kv_t)
                *kvarr = kv_t;
        }
    }
}

SHIORI_KV *shiori_kv_get(SHIORI_KV *kvarr, size_t kvarr_count, const char *key) {
    for (size_t i = 0; i < kvarr_count; i++)
        if (strcmp(kvarr[i].key, key) == 0)
            return &kvarr[i];
    return NULL;
}

void shiori_kv_set(SHIORI_KV **kvarr, size_t *kvarr_capacity, size_t *kvarr_count, int count, ...) {
    va_list ap;
    va_start(ap, count);
    for (int i = 0; i < count; i++) {
        char *key = va_arg(ap, char *);
        char *value = va_arg(ap, char *);
        if (!(key && value)) {
            va_end(ap);
            return;
        }
        SHIORI_KV *kv = shiori_kv_get(*kvarr, *kvarr_count, key);
        if (!kv) {
            shiori_kvarr_expand(kvarr, kvarr_capacity, *kvarr_count);
            kv = &(*kvarr)[(*kvarr_count)++];
        }
        kv->key = malloc(strlen(key) + 1);
        if (!kv->key) {
            (*kvarr_count)--;
            continue;
        }
        strcpy(kv->key, key);
        kv->value = malloc(strlen(value) + 1);
        if (!kv->value) {
            free(kv->key);
            (*kvarr_count)--;
            continue;
        }
        strcpy(kv->value, value);
    }
    va_end(ap);
}

void shiori_kv_append(SHIORI_KV *kv, const char *value) {
    char *value_t = realloc(kv->value, strlen(kv->value) + strlen(value) + 1);
    if (!value_t)
        return;
    kv->value = value_t;
    strcat(kv->value, value);
}

#define SHIORI_KV_GET(shiori, key) (shiori_kv_get((shiori).kvarr, (shiori).kvarr_count, key))
#define SHIORI_KV_SET(shiori, key, value) (shiori_kv_set(&(shiori).kvarr, &(shiori).kvarr_capacity, &(shiori).kvarr_count, 1, key, value))
#define SHIORI_CONTENT_GET(shiori) (SHIORI_KV_GET(shiori,  strcmp((shiori).ver, SHIORI25_VERSION_STRING) == 0 ? SENTENCE_STRING : VALUE_STRING))
#define SHIORI_CONTENT_SET(shiori, value) (SHIORI_KV_SET(shiori, strcmp((shiori).ver, SHIORI25_VERSION_STRING) == 0 ? SENTENCE_STRING : VALUE_STRING, value))
#define SHIORI_CONTENT_APPEND(shiori, value) (shiori_kv_append(SHIORI_CONTENT_GET(shiori), value))

char *dllRoot;

void GET(const SHIORI_REQ *, SHIORI_RES *);

int LOAD_Emergency(void *h, long len) {
    dllRoot = calloc(len + 1, sizeof(char));
    if (!dllRoot)
        return 0;
    memcpy(dllRoot, h, len);
    return 1;
}

int UNLOAD_Emergency(void) {
    free(dllRoot);
    return 0;
}

void *REQUEST_Emergency(void *h, long *len) {
    // parse SHIORI Request Message.
    /*sample message:
        GET Sentence SHIORI/2.2
        Sender: embryo
        Event: OnFirstBoot
        Reference0: 0
        Charset: Shift_JIS
    */
    char *raw;
    raw = calloc(*len + 1, sizeof(char));
    if (!raw) {
        *len = 0;
        return NULL;
    }
    memcpy(raw, h, *len);
    int state = 0;
    size_t p = SIZE_MAX;
    size_t l = 0;
    size_t i;
    SHIORI_REQ req = {NULL, NULL, NULL, NULL, 0, 0};
    SHIORI_KV kv = {NULL, NULL};
    for (i = 0; i < (size_t)*len; i++) {
        // first line.
        if (state == 0) {
            if (raw[i] != ' ' && p == SIZE_MAX) {
                p = i;
                l = 0;
            }
            else if (raw[i] == ' ') {
                if (p == SIZE_MAX) {
                    state = -1;
                    break;
                }
                if (!req.req) {
                    req.req = calloc(l + 1, sizeof(char));
                    if (!req.req) {
                        state = -1;
                        break;
                    }
                    memcpy(req.req, raw + p, l);
                    p = SIZE_MAX;
                }
                else if (req.name == NULL && req.ver == NULL) {
                    req.name = calloc(l + 1, sizeof(char));
                    memcpy(req.name, raw + p, l);
                    if (strcmp(req.name, SHIORI30_VERSION_STRING) == 0) {
                        req.ver = req.name;
                        req.name = NULL;
                    }
                    p = SIZE_MAX;
                }
            }
            else if (raw[i - 1] == '\r' && raw[i] == '\n') {
                l--;
                if (!req.req) {
                    state = -1;
                    break;
                }
                else if (!req.ver) {
                    req.ver = calloc(l + 1, sizeof(char));
                    memcpy(req.ver, raw + p, l);
                }
                p = SIZE_MAX;
                state = 1;
            }
            l++;
        }
        // headers.
        else if (state > 0) {
            if (p == SIZE_MAX) {
                p = i;
                l = 0;
            }
            switch (state) {
            case 1: // "Key": Value\r\n
                if (raw[i] != ':') {
                    if (raw[i - 1] == '\r' && raw[i] == '\n') {
                        if (l == 1) {
                            i = *len;
                        }
                        state = 5;
                    }
                    else
                        l++;
                }
                else {
                    kv.key = calloc(l + 1, sizeof(char));
                    memcpy(kv.key, raw + p, l);
                    state = 2;
                    p = SIZE_MAX;
                }
                break;
            case 2: // Key":" Value\r\n
                if (raw[i] == ' ')
                    continue;
                else {
                    if (raw[i - 1] == '\r' && raw[i] == '\n')
                        state = 5;
                    else {
                        state = 3;
                        p = SIZE_MAX;
                        i--;
                    }
                }
                break;
            case 3: // Key: "Value"\r\n
                if (raw[i - 1] == '\r' && raw[i] == '\n') {
                    l--;
                    kv.value = calloc(l + 1, sizeof(char));
                    memcpy(kv.value, raw + p, l);
                    state = 4;
                }
                else
                    l++;
                break;
            case 4: // Key: Value"\r\n"
                shiori_kvarr_expand(&req.kvarr, &req.kvarr_capacity, req.kvarr_count + 1);
                int index = req.kvarr_count++;
                req.kvarr[index].key = calloc(strlen(kv.key) + 1, sizeof(char));
                req.kvarr[index].value = calloc(strlen(kv.value) + 1, sizeof(char));
                strcpy(req.kvarr[index].key, kv.key);
                strcpy(req.kvarr[index].value, kv.value);
                state = 5;
                i--;
                break;
            case 5: // next
                state = 1;
                p = SIZE_MAX;
                if (kv.key != NULL) {
                    free(kv.key);
                    kv.key = NULL;
                }
                if (kv.value != NULL) {
                    free(kv.value);
                    kv.value = NULL;
                }
                i--;
            }
        }
    }
    SHIORI_RES res = {SHIORI25_VERSION_STRING, SHIORI_500, NULL, 0, 0};
    shiori_kvarr_expand(&res.kvarr, &res.kvarr_capacity, 0);
    // parsing succeed.
    if (state > 0) {
        for (i = 0; i < strlen(req.req); i++)
            req.req[i] = (char)toupper(req.req[i]);
        // "GET" or quit.
        if (strcmp(req.req, GET_STRING) == 0)
            GET(&req, &res);
        else {
            if (!req.name)
                res.ver = SHIORI30_VERSION_STRING;
            res.stat = SHIORI_204;
        }
    }
    // parsing failed.
    else
        if (state == -1)
            res.stat = SHIORI_400;
    // build SHIORI Response Message.
    char *resraw;
    char *resraw_p;
    size_t resraw_len = strlen(res.stat) + strlen(res.ver) + 6;
    for (i = 0; i < res.kvarr_count; i++)
        resraw_len += strlen(res.kvarr[i].key) + strlen(res.kvarr[i].value) + 4;
    resraw = calloc(resraw_len, sizeof(char));
    if (!resraw) {
        free(req.kvarr);
        free(req.ver);
        free(req.name);
        free(req.req);
        for (i = 0; i < res.kvarr_count; i++)
            free(res.kvarr[i].value);
        free(res.kvarr);
        *len = 0;
        return NULL;
    }
    resraw_p = resraw + sprintf(resraw, "%s %s\r\n", res.ver, res.stat);
    for (i = 0; i < res.kvarr_count; i++)
        resraw_p += sprintf(resraw_p, "%s: %s\r\n", res.kvarr[i].key, res.kvarr[i].value);
    strcpy(resraw_p, "\r\n");
    // Free!
    for (i = 0; i < req.kvarr_count; i++) {
        free(req.kvarr[i].key);
        free(req.kvarr[i].value);
    }
    free(req.kvarr);
    free(req.ver);
    free(req.name);
    free(req.req);
    for (i = 0; i < res.kvarr_count; i++)
        free(res.kvarr[i].value);
    free(res.kvarr);
    // return handle.
    *len = resraw_len - 1;
    return resraw;
}

void build_essential(SHIORI_RES *res) {
    res->stat = SHIORI_200;
    const char *showSakura = "\\h\\s0";
    SHIORI_KV_SET(*res, SENDER_STRING, PHIORI_FETUS_STRING);
    SHIORI_KV_SET(*res, CHARSET_STRING, US_ASCII_STRING);
    SHIORI_CONTENT_SET(*res, showSakura);
}

void build_emergency_message(SHIORI_RES *res) {
    SHIORI_KV *kv = SHIORI_CONTENT_GET(*res);
    if (!kv)
        return;
    char *value_t;
    if (ERROR_MESSAGE) {
        if (ERROR_TRACEBACK) {
            value_t = realloc(kv->value, strlen(kv->value) + strlen(ERROR_MESSAGE) + strlen(ERROR_TRACEBACK) + 20);
            if (!value_t)
                return;
            kv->value = value_t;
            char *value_p = kv->value + strlen(kv->value);
            sprintf(value_p, "\\_q%s\\n\\n%s\\x\\c\\b[-1]\\e", ERROR_MESSAGE, ERROR_TRACEBACK);
        }
        else {
            value_t = realloc(kv->value, strlen(kv->value) + strlen(ERROR_MESSAGE) + 16);
            if (!value_t)
                return;
            kv->value = value_t;
            char *value_p = kv->value + strlen(kv->value);
            sprintf(value_p, "\\_q%s\\x\\c\\b[-1]\\e", ERROR_MESSAGE);
        }
    }
    else {
        value_t = realloc(kv->value, strlen(kv->value) + sizeof(UNKNOWN_ERROR_MESSAGE) + 16);
        if (!value_t)
            return;
        kv->value = value_t;
        char *value_p = kv->value + strlen(kv->value);
        sprintf(value_p, "\\_q%s\\x\\c\\b[-1]\\e", UNKNOWN_ERROR_MESSAGE);
    }
}

/* SHIORI/2.0 */

void GET_Version(const SHIORI_REQ *req, SHIORI_RES *res) {
    // nothing to do.
}

/* SHIORI/2.2 */

void GET_OnFirstBoot(const SHIORI_REQ *req, SHIORI_RES *res) {
    build_essential(res);
    build_emergency_message(res);
}

void GET_OnBoot(const SHIORI_REQ *req, SHIORI_RES *res) {
    build_essential(res);
    build_emergency_message(res);
}

void GET_OnClose(const SHIORI_REQ *req, SHIORI_RES *res) {
    build_essential(res);
    SHIORI_CONTENT_APPEND(*res, "\\-\\e");
}

void GET_OnGhostChanged(const SHIORI_REQ *req, SHIORI_RES *res) {
    build_essential(res);
    build_emergency_message(res);
}

void GET_OnShellChanged(const SHIORI_REQ *req, SHIORI_RES *res) {
    build_essential(res);
    build_emergency_message(res);
}

void GET_OnMouseDoubleClick(const SHIORI_REQ *req, SHIORI_RES *res) {
    build_essential(res);
    SHIORI_KV *kv = SHIORI_CONTENT_GET(*res);
    size_t i;
    if (!kv)
        return;
    const char *entry[] = {"Show Traceback", "Change Ghost", "Homepage", "Version", "License", "Close", "Quit"};
    size_t len = strlen(kv->value) + 13;
    if (ERROR_MESSAGE)
        len += strlen(ERROR_MESSAGE);
    else
        len += sizeof(UNKNOWN_ERROR_MESSAGE) - 1;
    for (i = ERROR_TRACEBACK ? 0 : 1; i < sizeof(entry) / sizeof(char *); i++)
        len += strlen(entry[i]) + 10 + (i / 10);
    char *value_t = realloc(kv->value, len);
    if (!value_t)
        return;
    kv->value = value_t;
    char *value_p = kv->value + strlen(kv->value);
    value_p += sprintf(value_p, "\\_q%s\\n\\n", ERROR_MESSAGE ? ERROR_MESSAGE : UNKNOWN_ERROR_MESSAGE);
    for (i = ERROR_TRACEBACK ? 0 : 1; i < sizeof(entry) / sizeof(char *); i++)
        value_p += sprintf(value_p, "- \\q[%s,%d]\\n", entry[i], (signed)i);
    sprintf(value_p, "\\_q\\e");
}

void GET_OnChoiceSelect(const SHIORI_REQ *req, SHIORI_RES *res) {
    SHIORI_KV *kv = NULL;
    for (size_t i = 0; i < req->kvarr_count; i++)
        if (strcmp(req->kvarr[i].key, "Reference0") == 0)
            kv = &req->kvarr[i];
    if (!kv)
        return;
    char *script = "";
    int isDynamic = 0;
    // show traceback
    if (strcmp(kv->value, "0") == 0 && ERROR_TRACEBACK) {
        isDynamic = 1;
        script = malloc(strlen(ERROR_MESSAGE) + strlen(ERROR_TRACEBACK) + 20);
        if (!script)
            return;
        sprintf(script, "\\_q%s\\n\\n%s\\x\\c\\b[-1]\\e", ERROR_MESSAGE, ERROR_TRACEBACK);
    }
    // change ghost
    else if (strcmp(kv->value, "1") == 0)
        script = "\\b[-1]\\![open,ghostexplorer]\\e";
    // homepage
    else if (strcmp(kv->value, "2") == 0)
        script = "\\b[-1]\\![open,browser," PHIORI_URL "]\\e";
    // version
    else if (strcmp(kv->value, "3") == 0) {
        isDynamic = 1;
        script = malloc(BUFSIZ);
        if (!script)
            return;
        char *script_p = script;
        script_p += sprintf(script_p, "\\_q" _PHIORI_NAME "/");
        script_p += getPhioriVersion(script_p);
        script_p += sprintf(script_p, "\\e");
    }
    // license
    else if (strcmp(kv->value, "4") == 0)
        script = "\\b[-1]\\![open,browser," LICENSE_URL "]\\e";
    // close
    else if (strcmp(kv->value, "5") == 0)
        script = "\\b[-1]\\e";
    // quit
    else if (strcmp(kv->value, "6") == 0)
        script = "\\-\\e";
    if (script) {
        build_essential(res);
        SHIORI_CONTENT_APPEND(*res, script);
        if (isDynamic)
            free(script);
    }
}

/* SHIORI/2.5 */

void GET_String(const SHIORI_REQ *req, SHIORI_RES *res) {
    // return no content always.
    res->stat = SHIORI_204;
}

/* SHIORI/3.0 */

void GET_craftman(const SHIORI_REQ *req, SHIORI_RES *res) {
    build_essential(res);
    SHIORI_CONTENT_SET(*res, _PHIORI_CREATOR);
}

void GET_name(const SHIORI_REQ *req, SHIORI_RES *res) {
    build_essential(res);
    SHIORI_CONTENT_SET(*res, _PHIORI_NAME);
}

void GET_version(const SHIORI_REQ *req, SHIORI_RES *res) {
    build_essential(res);
    SHIORI_KV *kv = SHIORI_CONTENT_GET(*res);
    char *value_t = realloc(kv->value, BUFSIZ);
    if (!value_t)
        return;
    kv->value = value_t;
    getPhioriVersion(kv->value);
}

/* SHIORI GET */

void GET(const SHIORI_REQ *req, SHIORI_RES *res) {
    res->stat = SHIORI_200;
    // SHIORI2
    if (req->name) {
        if (strcmp(req->name, VERSION_STRING) == 0)
            GET_Version(req, res);
        else if (strcmp(req->name, "String") == 0)
            GET_String(req, res);
        else if (strcmp(req->name, SENTENCE_STRING) == 0) {
            char *event = NULL;
            for (size_t i = 0; i < req->kvarr_count; i++)
                if (strcmp(req->kvarr[i].key, EVENT_STRING) == 0)
                    event = req->kvarr[i].value;
            if (!event) {
                res->stat = SHIORI_400;
                return;
            }
            if (!IS_LOADED) {
                if (strcmp(event, "OnFirstBoot") == 0)
                    GET_OnFirstBoot(req, res);
                else if (strcmp(event, "OnBoot") == 0)
                    GET_OnBoot(req, res);
                else if (strcmp(event, "OnClose") == 0)
                    GET_OnClose(req, res);
                else if (strcmp(event, "OnGhostChanged") == 0)
                    GET_OnGhostChanged(req, res);
                else if (strcmp(event, "OnShellChanged") == 0)
                    GET_OnShellChanged(req, res);
                else if (strcmp(event, "OnMouseDoubleClick") == 0)
                    GET_OnMouseDoubleClick(req, res);
                else if (strcmp(event, "OnChoiceSelect") == 0)
                    GET_OnChoiceSelect(req, res);
                else if (strcmp(event, "OnShellChanged") == 0)
                    GET_OnShellChanged(req, res);
                else
                    res->stat = SHIORI_204;
            }
            else if (SHOW_ERROR)
                if (*event == 'O' && event[1] == 'n') {
                    build_essential(res);
                    build_emergency_message(res);
                    SHOW_ERROR = 0;
                }
        }
    }
    // SHIORI3
    else {
        res->ver = SHIORI30_VERSION_STRING;
        char *id = NULL;
        size_t i;
        for (i = 0; i < req->kvarr_count; i++) {
            if (strcmp(req->kvarr[i].key, ID_STRING) == 0)
                id = req->kvarr[i].value;
        }
        if (id == NULL) {
            return;
        }
        if (strcmp(id, "craftman") == 0)
            GET_craftman(req, res);
        else if (strcmp(id, "name") == 0)
            GET_name(req, res);
        else if (strcmp(id, "version") == 0)
            GET_version(req, res);
        else if (!IS_LOADED) {
            if (strcmp(id, "OnFirstBoot") == 0)
                GET_OnFirstBoot(req, res);
            else if (strcmp(id, "OnBoot") == 0)
                GET_OnBoot(req, res);
            else if (strcmp(id, "OnClose") == 0)
                GET_OnClose(req, res);
            else if (strcmp(id, "OnGhostChanged") == 0)
                GET_OnGhostChanged(req, res);
            else if (strcmp(id, "OnShellChanged") == 0)
                GET_OnShellChanged(req, res);
            else if (strcmp(id, "OnMouseDoubleClick") == 0)
                GET_OnMouseDoubleClick(req, res);
            else if (strcmp(id, "OnChoiceSelect") == 0)
                GET_OnChoiceSelect(req, res);
            else if (strcmp(id, "OnShellChanged") == 0)
                GET_OnShellChanged(req, res);
            else if (*id >= 'a' && *id <= 'z')
                GET_String(req, res);
            else
                res->stat = SHIORI_204;
        }
        else if (SHOW_ERROR)
            if (*id == 'O' && id[1] == 'n') {
                build_essential(res);
                build_emergency_message(res);
                SHOW_ERROR = 0;
            }
    }
}
