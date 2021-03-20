#ifndef __HEAP_H__
#define __HEAP_H__

#include <stdint.h>

enum pointer_type_t {
	pointer_null,
	pointer_heap_corrupted,
	pointer_control_block,
	pointer_inside_fences,
	pointer_inside_data_block,
	pointer_unallocated,
	pointer_valid
};
struct memory_manager_t {
	void *memory_start;
	size_t memory_size;
	struct memory_chunk_t *first_memory_chunk;
};
struct memory_chunk_t {
	struct memory_chunk_t* prev;
	struct memory_chunk_t* next;
	size_t size;
	int free;
	uint8_t lrc;
	const char* filename;
	int fileline;
};


enum pointer_type_t get_pointer_type(const void* const pointer);

int heap_setup(void);
void heap_clean(void);

void *heap_malloc(size_t size);
void *heap_malloc_zero(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t size);
size_t heap_get_largest_used_block_size(void);
void *set_fences(void *address, size_t size);
void *set_fences_fill(void *address, size_t size);
void heap_free(void *address);
void merge_chunks(void);

uint8_t calculateLRC(struct memory_chunk_t *ptr);
int heap_validate(void);

void* heap_malloc_aligned(size_t count);       
void* heap_malloc_aligned_zero(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);    
void* heap_realloc_aligned(void* memblock, size_t size);

void* heap_malloc_debug(size_t count, int fileline, const char* filename);
void* heap_malloc_zero_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename);

void* heap_malloc_aligned_debug(size_t count, int fileline, const char* filename);
void* heap_malloc_aligned_zero_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline, const char* filename);

void print_mem(void);

#endif