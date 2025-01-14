#ifndef UTILS_H
#define UTILS_H

#include "waddle.h"

extern WDL_Str read_file(WDL_Arena* arena, WDL_Str filename);
extern u64     fvn1a_hash(const void* data, u64 len);

#endif // UTILS_H
