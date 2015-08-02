#include "shiori.h"
#ifndef SHIORI_EMERGENCY
#define SHIORI_EMERGENCY SHIORI

int LOAD_Emergency(void *h, long len);
int UNLOAD_Emergency(void);
void *REQUEST_Emergency(void *h, long *len);

#endif
