#ifndef SHIORI
#define SHIORI ("phiori/1.10")

int LOAD(void *h, long len);
int UNLOAD(void);
void *REQUEST(void *h, long *len);

int IS_LOADED;
int IS_ERROR;
char *ERROR_MESSAGE;
char *ERROR_TRACEBACK;
int SHOW_ERROR;

#endif
