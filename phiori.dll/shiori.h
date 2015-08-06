#ifndef SHIORI
#define SHIORI ("phiori/1.10")
#include "debug.h"

__declspec(dllexport) int __cdecl load(void *h, long len);
__declspec(dllexport) int __cdecl unload(void);
__declspec(dllexport) void *__cdecl request(void *h, long *len);

int IS_LOADED;
int IS_ERROR;
char *ERROR_MESSAGE;
char *ERROR_TRACEBACK;
int SHOW_ERROR;

#endif
