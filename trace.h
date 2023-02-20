#ifndef EMU8051_TRACE_H
#define EMU8051_TRACE_H

#include <stdint.h>
#include <stdlib.h>

#define TRACE_MEM_MAX 0x10

#ifdef __cplusplus
extern "C" {
#endif

int trace_open(const char *filename);
void trace_close(void);
int trace_is_open(void);
void trace_push();

void register_push(const char *name, uint16_t v, size_t bits, bool w);
void mem_push(uint16_t addr, uint8_t v, bool w);
void set_trace_op(const char *op, uint size);

#ifdef __cplusplus
}
#endif

#endif // EMU8051_TRACE_H
