#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include "heap.h"

#define TEST_SIZE 8181
#define TEST_PAGE_SIZE 4096

int main() {
	srand (time(NULL));

	int status = heap_setup();
	assert(status == 0);

	char *ptr[TEST_SIZE];
	int ptr_state[TEST_SIZE] = { 0 };

	int is_allocated = 0;
	int rand_value = 0;

	for (int i = 0; i < TEST_SIZE; ++i) {
		rand_value = rand() % 100;
		if (rand_value < 10) {
			for (int j = 0; j < TEST_SIZE; ++j)
				if (ptr_state[j] == 0) {
					ptr_state[j] = 1;
					ptr[j] = heap_realloc_aligned(NULL, rand() % 1000 + 500);  
					assert(((intptr_t)ptr[j] & (intptr_t)(TEST_PAGE_SIZE - 1)) == 0);
					is_allocated++;
					break;
				}
	 	}
		else if (rand_value < 20) {
			for (int j = 0; j < TEST_SIZE; ++j)
				if (ptr_state[j] == 0) {
					ptr_state[j] = 1;
					ptr[j] = heap_calloc_aligned(rand() % 1000 + 500, rand() % 4 + 1);  
					assert(((intptr_t)ptr[j] & (intptr_t)(TEST_PAGE_SIZE - 1)) == 0);
					is_allocated++;
					break;
				}
		}
		else if (rand_value < 30) {
			for (int j = 0; j < TEST_SIZE; ++j)
				if (ptr_state[j] == 0) {
					ptr_state[j] = 1;
					ptr[j] = heap_malloc_aligned(rand() % 1000 + 500);  
					assert(((intptr_t)ptr[j] & (intptr_t)(TEST_PAGE_SIZE - 1)) == 0);
					is_allocated++;
					break;
				}
		}
		else if (rand_value < 40) {
			for (int j = 0; j < TEST_SIZE; ++j)
				if (ptr_state[j] == 0) {
					ptr_state[j] = 1;
					ptr[j] = heap_realloc(NULL, rand() % 1000 + 500);  
					assert((intptr_t)ptr[j] % sizeof(void *) == 0);
					is_allocated++;
					break;
				}
 		}
		else if (rand_value < 50) {
			for (int j = 0; j < TEST_SIZE; ++j)
				if (ptr_state[j] == 0) {
					ptr_state[j] = 1;
					ptr[j] = heap_calloc(rand() % 1000 + 500, rand() % 4 + 1);  
					assert((intptr_t)ptr[j] % sizeof(void *) == 0);
					is_allocated++;
					break;
				}
			}
		else if (rand_value < 60) {
			for (int j = 0; j < TEST_SIZE; ++j)
				if (ptr_state[j] == 0) {
					ptr_state[j] = 1;
					ptr[j] = heap_malloc(rand() % 1000 + 500);  
					assert((intptr_t)ptr[j] % sizeof(void *) == 0);
					is_allocated++;
					break;
				}
			}
		else if (is_allocated) {
			if (rand_value < 70) {
				int to_reallocate = rand() % is_allocated;
				for (int j = 0; j < TEST_SIZE; ++j) {
					if (ptr_state[j] == 1 && !to_reallocate) {
						if (rand() % 100 < 50)
							ptr[j] = heap_realloc(ptr[j], rand() % 100 + 50);
						else
							ptr[j] = heap_realloc(ptr[j], rand() % 1000 + 500);    
						assert((intptr_t)ptr[j] % sizeof(void *) == 0);
						break;
					}
					to_reallocate--;
				}
			}
			else if (rand_value < 80) {
				int to_free = rand() % is_allocated;
				for (int j = 0; j < TEST_SIZE; ++j) {
					if (ptr_state[j] == 1 && !to_free) {
						ptr_state[j] = 0;
						is_allocated--;
						heap_realloc_aligned(ptr[j], 0);
						break;
					}
					to_free--;
				}
			}
			else if (rand_value < 90) {
				int to_free = rand() % is_allocated;
				for (int j = 0; j < TEST_SIZE; ++j) {
					if (ptr_state[j] == 1 && !to_free) {
						ptr_state[j] = 0;
						is_allocated--;
						heap_realloc(ptr[j], 0);
						break;
					}
					to_free--;
				}
			}
			else {
				int to_free = rand() % is_allocated;
				for (int j = 0; j < TEST_SIZE; ++j) {
					if (ptr_state[j] == 1 && !to_free) {
						ptr_state[j] = 0;
						is_allocated--;
						heap_free(ptr[j]);
						break;
					}
					to_free--;
				}
 			}
		}
		status = heap_validate();
		assert(status == 0);
	}

	for (int j = 0; j < TEST_SIZE; ++j)
		if (ptr_state[j] == 1)
			heap_realloc_aligned(ptr[j], 0);

	assert(heap_get_largest_used_block_size() == 0);

	heap_clean();
	return 0;
}