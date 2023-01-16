#ifndef DISPLAYD_UTILS_H_
#define DISPLAYD_UTILS_H_

#include <unistd.h>
#include "sunxi_display2.h"

#undef  NELEM
#define NELEM(name) (sizeof(name) / sizeof(name[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

int getConnectStateFromFile(const char *path);
int readIntFromFile(const char *path);
int writeIntToFile(const char *path, int t);
int getDispModeFormFile(int type);
void saveDispModeToFile(int saveType, int saveMode);
int getSavedVendorIdFromFile();
void savedVendorIdToFile(int vendorId);

#endif
