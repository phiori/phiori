#include "shiori.h"
#include "emergency.h"
#include <stdlib.h>
#include <Windows.h>

int IS_LOADED = 0;
int IS_ERROR = 0;
char *ERROR_MESSAGE = NULL;

__declspec(dllexport) BOOL __cdecl load(HGLOBAL h, long len) {
    int result = 0;
    result |= LOAD_Emergency(h, len);
    result |= LOAD(h, len);
    GlobalFree(h);
    return result;
}

__declspec(dllexport) BOOL __cdecl unload(void) {
    int result = 0;
    result |= UNLOAD_Emergency();
    if (!IS_ERROR)
        result |= UNLOAD();
    return result;
}

__declspec(dllexport) HGLOBAL __cdecl request(HGLOBAL h, long *len) {
    void *result = NULL;
    HGLOBAL gResult = NULL;
    if (!IS_ERROR)
        result = REQUEST(h, len);
    if (result == NULL)
        result = REQUEST_Emergency(h, len);
    GlobalFree(h);
    if (result) {
        gResult = GlobalAlloc(GMEM_FIXED, *len);
        free(result);
    }
    return gResult;
}
