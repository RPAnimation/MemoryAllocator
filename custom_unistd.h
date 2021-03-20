#ifndef __CUSTOM_UNISTD_H__
#define __CUSTOM_UNISTD_H__

#include <unistd.h>

void* custom_sbrk(intptr_t delta);

#endif