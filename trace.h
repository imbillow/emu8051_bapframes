#ifndef EMU8051_TRACE_H
#define EMU8051_TRACE_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct trace_mem_t {
		uint16_t addr;
		uint8_t val;
} TraceMem;

#define TRACE_MEM_MAX 0x10

typedef struct trace_operands_t {
		uint8_t SFRs[128]; // Special Function Registers 128 bytes
		uint8_t lower[128]; // Lower 128 bytes
		uint16_t pc; // Program Counter; outside memory area
		TraceMem mems[TRACE_MEM_MAX];
		size_t mems_count;
} TraceOperands8051;

typedef struct trace_frame_t {
		uint8_t op[3];
		size_t op_size;
		TraceOperands8051 pre;
		TraceOperands8051 post;
} TraceFrame8051;

int trace_open(const char *filename);
void trace_close(void);
int trace_is_open(void);
void trace_push(TraceFrame8051 *tf);

extern TraceFrame8051 build_frame;

#ifdef __cplusplus
}
#endif

#endif // EMU8051_TRACE_H
