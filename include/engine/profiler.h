#ifndef PROFILER_H
#define PROFILER_H

#include "waddle.h"

extern void profiler_init(void);
extern void profiler_terminate(void);

extern void profiler_begin_frame(u32 frame_number);
extern void profiler_end_frame(void);
extern void profiler_dump_frame(void);

extern void prof_begin(WDL_Str name);
extern void prof_end(void);

#endif // PROFILER_H
