#ifndef _PHIORI
#define _PHIORI 1
#define _PHIORI_VER_MAJOR 2
#define _PHIORI_VER_MINOR 0
#define _PHIORI_VER_PATCH 0
#define _PHIORI_NAME "phiori"
#define _PHIORI_CREATOR "Mayu Laierlence"

int LOAD(void *h, long len);
int UNLOAD(void);
void *REQUEST(void *h, long *len);

int getPhioriVersion(char *);

#endif
