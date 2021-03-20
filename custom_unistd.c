#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


#define PAGE_SIZE       4096
#define PAGE_FENCE      1
#define PAGES_AVAILABLE 16384
#define PAGES_TOTAL     (PAGES_AVAILABLE + 2 * PAGE_FENCE)

uint8_t memory[PAGE_SIZE * PAGES_TOTAL] __attribute__((aligned(PAGE_SIZE)));

struct memory_fence_t {
	uint8_t first_page[PAGE_SIZE];
	uint8_t last_page[PAGE_SIZE];
};

struct mm_struct {
	intptr_t start_brk;
	intptr_t brk;
	struct memory_fence_t fence;
	intptr_t start_mmap;
} mm;

void __attribute__((constructor)) memory_init(void);
void __attribute__((destructor)) memory_check(void);
void* custom_sbrk(intptr_t delta);

void __attribute__((constructor)) memory_init(void) {
	setvbuf(stdout, NULL, _IONBF, 0); 
	srand(time(NULL));
	assert(sizeof(intptr_t) == sizeof(void*));
	for (int i = 0; i < PAGE_SIZE; i++) {
			mm.fence.first_page[i] = rand();
			mm.fence.last_page[i] = rand();
	}
	memcpy(memory, mm.fence.first_page, PAGE_SIZE);
	memcpy(memory + (PAGE_FENCE + PAGES_AVAILABLE) * PAGE_SIZE, mm.fence.last_page, PAGE_SIZE);

	mm.start_brk = (intptr_t)(memory + PAGE_SIZE);
	mm.brk = (intptr_t)(memory + PAGE_SIZE);
	mm.start_mmap = (intptr_t)(memory + (PAGE_FENCE + PAGES_AVAILABLE) * PAGE_SIZE);
	
	assert(mm.start_mmap - mm.start_brk == PAGES_AVAILABLE * PAGE_SIZE);
} 

void __attribute__((destructor)) memory_check(void) {
	int first = memcmp(memory, mm.fence.first_page, PAGE_SIZE);
	int last = memcmp(memory + (PAGE_FENCE + PAGES_AVAILABLE) * PAGE_SIZE, mm.fence.last_page, PAGE_SIZE);
	
	printf("\n### Fence states:\n");
	printf("    First fence: [%s]\n", first == 0 ? "vaild" : "damaged");
	printf("    Last fence : [%s]\n", last == 0 ? "vaild" : "damaged");

	printf("### Summary: \n");
	printf("    Whole memory space       : %lu bytes\n", mm.start_mmap - mm.start_brk);
	printf("    Memory reserved by sbrk(): %lu bytes\n", mm.brk - mm.start_brk);
}

void* custom_sbrk(intptr_t delta) {
	intptr_t current_brk = mm.brk;
	if (mm.brk + delta < mm.start_brk) {
		errno = 0;
		return (void*)current_brk;
	}

	if (mm.brk + delta >= mm.start_mmap) {
		errno = ENOMEM;
		return (void*)-1;
	}

	mm.brk += delta;
	return (void*)current_brk;
}