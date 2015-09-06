#include "phiori.h"
#include "shiori.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <Windows.h>
#include <Python.h>

const wchar_t *PYTHON_DLL_NAME_W = L"python35.dll";
const char *PYTHON_LIB_NAME = "python35.zip";

BOOL checkPython(void);
void getTraceback(void);
PyObject *PyUnicode_ToSakuraScript(PyObject *value);

char *phioriRoot;
wchar_t *phioriRootW;
wchar_t *phioriNameW;

PyObject *globalModule;
PyObject *phioriModule;
PyObject *tracebackModule;

PyObject *errorType;
PyObject *errorValue;
PyObject *errorTraceback;

BOOL LOAD(HGLOBAL h, long len) {
    BOOL result = TRUE;
    phioriRoot = calloc(len + 1, sizeof(char));
    if (!phioriRoot)
        return FALSE;
    memcpy(phioriRoot, h, len);
    size_t root_sz = MultiByteToWideChar(CP_UTF8, 0, phioriRoot, -1, NULL, 0);
    phioriRootW = calloc(root_sz, sizeof(wchar_t));
    if (!phioriRootW)
        return FALSE;
    MultiByteToWideChar(CP_UTF8, 0, phioriRoot, -1, phioriRootW, root_sz);
    phioriNameW = calloc(root_sz, sizeof(wchar_t));
    if (!phioriNameW)
        return FALSE;
    wcscpy(phioriNameW, phioriRootW);
    if (!checkPython()) {
        IS_ERROR = TRUE;
        ERROR_MESSAGE = "Unable to load python library.";
        return FALSE;
    }
    SetCurrentDirectory(phioriRootW);
    Py_SetProgramName(phioriNameW);
    Py_SetPythonHome(phioriRootW);
    Py_Initialize();
    if (!Py_IsInitialized()) {
        ERROR_MESSAGE = "Failed to initialise python.";
        IS_ERROR = TRUE;
        return FALSE;
    }
    tracebackModule = PyImport_ImportModule("traceback");
    if (tracebackModule == NULL) {
        ERROR_MESSAGE = "Failed to initialise python.";
        IS_ERROR = TRUE;
        return FALSE;
    }
    phioriModule = PyImport_ImportModule("phiori");
    if (phioriModule == NULL) {
        PyObject *err = PyErr_Occurred();
        if (err != NULL) {
            Py_XDECREF(err);
            result = FALSE;
            getTraceback();
        }
    }
    else {
        PyObject *func = PyObject_GetAttrString(phioriModule, "load");
        if (func == NULL || !PyCallable_Check(func)) {
            PyObject *err = PyErr_Occurred();
            if (err != NULL) {
                Py_XDECREF(err);
                getTraceback();
                result = FALSE;
            }
        }
        else {
            PyObject *arg0 = PyUnicode_FromStringAndSize(phioriRoot, len);
            PyObject *callResult = PyObject_CallFunctionObjArgs(func, arg0, NULL);
            if (callResult != NULL)
                result = PyObject_IsTrue(callResult);
            Py_XDECREF(callResult);
            Py_XDECREF(arg0);
        }
    }
    IS_LOADED = result;
    return result;
}

BOOL UNLOAD(void) {
    BOOL result = TRUE;
    PyErr_Clear();
    if (IS_LOADED) {
        PyObject *func = PyObject_GetAttrString(phioriModule, "unload");
        if (func && PyCallable_Check(func)) {
            PyObject *callResult = PyObject_CallFunctionObjArgs(func, NULL);
            if (callResult != NULL)
                result = PyObject_IsTrue(callResult);
            Py_XDECREF(callResult);
        }
    }
    Py_Finalize();
    free(phioriNameW);
    free(phioriRootW);
    free(phioriRoot);
    return result;
}

HGLOBAL REQUEST(HGLOBAL h, long *len) {
    char *result = NULL;
    if (!IS_LOADED) {
        if (ERROR_MESSAGE == NULL)
            ERROR_MESSAGE = "Error has occurred while loading phiori core.";
        return NULL;
    }
    char *req = malloc(*len);
    if (!req) {
        *len = 0;
        return NULL;
    }
    memcpy(req, h, *len);
    PyObject *func = PyObject_GetAttrString(phioriModule, "request");
    if (func == NULL || !PyCallable_Check(func)) {
        PyObject *err = PyErr_Occurred();
        if (err != NULL) {
            Py_XDECREF(err);
            getTraceback();
        }
    }
    else {
        PyObject *arg0 = PyBytes_FromStringAndSize(req, *len);
        PyObject *arg1 = PyLong_FromLong(*len);
        PyObject *callResult = PyObject_CallFunctionObjArgs(func, arg0, arg1, NULL);
        if (callResult != NULL)
            result = PyBytes_AsString(callResult);
        Py_XDECREF(callResult);
        Py_XDECREF(arg1);
        Py_XDECREF(arg0);
    }
    return result;
}

int getPhioriVersion(char *buf) {
#ifdef _DEBUG
#if _PHIORI_VER_PATCH > 0
    return sprintf(buf, "%d.%d.%d-debug", _PHIORI_VER_MAJOR, _PHIORI_VER_MINOR, _PHIORI_VER_PATCH);
#else
    return sprintf(buf, "%d.%d-debug", _PHIORI_VER_MAJOR, _PHIORI_VER_MINOR);
#endif
#else
#if _PHIORI_VER_PATCH > 0
    return sprintf(buf, "%d.%d.%d", _PHIORI_VER_MAJOR, _PHIORI_VER_MINOR, _PHIORI_VER_PATCH);
#else
    return sprintf(buf, "%d.%d", _PHIORI_VER_MAJOR, _PHIORI_VER_MINOR);
#endif
#endif
}

BOOL checkPython(void) {
    wchar_t *pathW = calloc(wcslen(phioriRootW) + wcslen(PYTHON_DLL_NAME_W), sizeof(wchar_t));
    if (!pathW)
        return FALSE;
    wcscpy(pathW, phioriRootW);
    wcscat(pathW, PYTHON_DLL_NAME_W);
    HMODULE dll = LoadLibrary(pathW);
    free(pathW);
    if (!dll)
        return FALSE;
    FreeLibrary(dll);
    char *path = calloc(strlen(phioriRoot) + strlen(PYTHON_LIB_NAME), sizeof(char));
    if (!path)
        return FALSE;
    strcpy(path, phioriRoot);
    strcat(path, PYTHON_LIB_NAME);
    FILE *zip = fopen(path, "rb");
    free(path);
    if (!zip)
        return FALSE;
    fclose(zip);
    return TRUE;
}

void getException(void) {
    PyErr_Fetch(&errorType, &errorValue, &errorTraceback);
    PyErr_NormalizeException(&errorType, &errorValue, &errorTraceback);
}

void getTraceback(void) {
    getException();
    PyObject *func = PyObject_GetAttrString(tracebackModule, "format_exception");
    if (func && PyCallable_Check(func)) {
        PyObject *callResult = PyObject_CallFunctionObjArgs(func, errorType, errorValue, errorTraceback, NULL);
        if (callResult != NULL) {
            PyObject *newLine = PyUnicode_FromString("\n");
            PyObject *rslash = PyUnicode_FromString("\\");
            PyObject *sakura_newLine = PyUnicode_FromString("\\n");
            PyObject *sakura_rslash = PyUnicode_FromString("\\\\");
            PyObject *sakura_newLine2 = PyUnicode_FromString("\\n\\n");
            PyObject *sakura_newLineH = PyUnicode_FromString("\\n\\n[half]");
            PyObject *tracebackString = PyUnicode_Join(newLine, callResult);
            PyObject *newTracebackString = PyUnicode_Replace(tracebackString, rslash, sakura_rslash, -1);
            Py_XDECREF(tracebackString);
            tracebackString = newTracebackString;
            newTracebackString = PyUnicode_Replace(tracebackString, newLine, sakura_newLine, -1);
            Py_XDECREF(tracebackString);
            tracebackString = newTracebackString;
            newTracebackString = PyUnicode_Replace(tracebackString, sakura_newLine2, sakura_newLineH, -1);
            Py_XDECREF(tracebackString);
            tracebackString = newTracebackString;
            newTracebackString = NULL;
            Py_XDECREF(sakura_newLineH);
            Py_XDECREF(sakura_newLine2);
            Py_XDECREF(sakura_rslash);
            Py_XDECREF(sakura_newLine);
            Py_XDECREF(rslash);
            Py_XDECREF(newLine);
            PyObject *tracebackAscii = PyUnicode_AsEncodedString(tracebackString, "ascii", "replace");
            Py_XDECREF(tracebackString);
            ERROR_TRACEBACK = PyBytes_AsString(tracebackAscii);
            Py_XDECREF(tracebackAscii);
        }
        Py_XDECREF(callResult);
    }
}
