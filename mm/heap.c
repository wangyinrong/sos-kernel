#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

static void alloc_chunk(uint32_t start, uint32_t len);
static void free_chunk(header_t* chunk);
static void split_chunk(header_t* chunk, uint32_t len);
static void glue_chunk(header_t* chunk);

uint32_t heap_max = HEAP_START;
header_t* heap_first = 0;

void init_heap() {
}

void* kmalloc(uint32_t len) {
	len += sizeof(header_t);

	header_t *cur_header = heap_first, *prev_header = 0;
	while (cur_header) {
		if (cur_header->allocated == 0 && cur_header->length >= len) {
			split_chunk(cur_header, len);
			cur_header->allocated = 1;
			return (void*) ((uint32_t) cur_header + sizeof(header_t));
		}
		prev_header = cur_header;
		cur_header = cur_header->next;
	}

	uint32_t chunk_start;
	if (prev_header)
		chunk_start = (uint32_t) prev_header + prev_header->length;
	else {
		chunk_start = HEAP_START;
		heap_first = (header_t *) chunk_start;
	}

	alloc_chunk(chunk_start, len);
	cur_header = (header_t *) chunk_start;
	cur_header->prev = prev_header;
	cur_header->next = 0;
	cur_header->allocated = 1;
	cur_header->length = len;

	prev_header->next = cur_header;

	return (void*) (chunk_start + sizeof(header_t));
}

void kfree(void *p) {
	header_t *header = (header_t*) ((uint32_t) p - sizeof(header_t));
	header->allocated = 0;
	glue_chunk(header);
}

void alloc_chunk(uint32_t start, uint32_t len) {
	// sanity check (watch out for overflow)
	if (HEAP_END - start < len)
		panic("Heap: out of memory.");
	while (start + len > heap_max) {
		uint32_t page = pmm_alloc_page();
		map(heap_max, page, PAGE_PRESENT | PAGE_WRITE); // TODO: | PAGE_USER);
		heap_max += 0x1000;
	}
}

void free_chunk(header_t *chunk) {
	chunk->prev->next = 0;
	if (chunk->prev == 0)
		heap_first = 0;
	// While the heap max can contract by a page and still be greater than the chunk address...
	while ((heap_max - 0x1000) >= (uint32_t) chunk) {
		heap_max -= 0x1000;
		uint32_t page;
		get_mapping(heap_max, &page);
		pmm_free_page(page);
		unmap(heap_max);
	}
}

void split_chunk(header_t *chunk, uint32_t len) {
	if (chunk->length - len > sizeof(header_t)) {
		header_t *newchunk = (header_t *) ((uint32_t) chunk + chunk->length);
		newchunk->prev = chunk;
		newchunk->next = 0;
		newchunk->allocated = 0;
		newchunk->length = chunk->length - len;

		chunk->next = newchunk;
		chunk->length = len;
	}
}

void glue_chunk(header_t *chunk) {
	if (chunk->next && chunk->next->allocated == 0) {
		chunk->length = chunk->length + chunk->next->length;
		chunk->next->next->prev = chunk;
		chunk->next = chunk->next->next;
	}

	if (chunk->prev && chunk->prev->allocated == 0) {
		chunk->prev->length = chunk->prev->length + chunk->length;
		chunk->prev->next = chunk->next;
		chunk->next->prev = chunk->prev;
		chunk = chunk->prev;
	}

	if (chunk->next == 0)
		free_chunk(chunk);
}
