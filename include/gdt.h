#ifndef GDT_H
#define GDT_H

#include <common.h>

// Initialisation function.
void init_gdt();

typedef struct {
	uint16_t limit_low; // The lower 16 bits of the limit.
	uint16_t base_low; // The lower 16 bits of the base.
	uint8_t base_middle; // The next 8 bits of the base.
	uint8_t access; // Access flags, determines what ring this segment can be used in.
	uint8_t granularity;
	uint8_t base_high; // The last 8 bits of the base.
}__attribute__((packed)) gdt_entry_t;

// This struct describes a GDT pointer. It points to the start of
// our array of GDT entries, and is in the format required by the
// lgdt instruction.
typedef struct {
	uint16_t limit; // The Global Descriptor Table limit.
	uint32_t base; // The address of the first gdt_entry_t struct.
}__attribute__((packed)) gdt_ptr_t;

// A struct describing a Task State Segment.
struct tss_entry_struct {
	uint32_t prev_tss; // The previous TSS - if we used hardware task switching this would form a linked list.
	uint32_t esp0; // The stack pointer to load when we change to kernel mode.
	uint32_t ss0; // The stack segment to load when we change to kernel mode.
	uint32_t esp1; // Unused...
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es; // The value to load into ES when we change to kernel mode.
	uint32_t cs; // The value to load into CS when we change to kernel mode.
	uint32_t ss; // The value to load into SS when we change to kernel mode.
	uint32_t ds; // The value to load into DS when we change to kernel mode.
	uint32_t fs; // The value to load into FS when we change to kernel mode.
	uint32_t gs; // The value to load into GS when we change to kernel mode.
	uint32_t ldt; // Unused...
	uint16_t trap;
	uint16_t iomap_base;
}__attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;

#endif

