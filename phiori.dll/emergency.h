#ifndef _SHIORI_EMERGENCY
#define _SHIORI_EMERGENCY 1

int LOAD_Emergency(void *h, long len);
int UNLOAD_Emergency(void);
void *REQUEST_Emergency(void *h, long *len);

#endif
