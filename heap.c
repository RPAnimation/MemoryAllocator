#include <stdio.h>
#include <string.h>
#include "heap.h"
#include "custom_unistd.h"

#define FEN_SIZE     16
#define DEFAULT_SIZE 128
#define PAGE_SIZE    4096

struct memory_manager_t memory_manager;
           
enum pointer_type_t get_pointer_type(const void* const pointer) {
	if (pointer != NULL) {
		// HEAP BROKEN
		if (heap_validate() != 0) {
			return pointer_heap_corrupted;
		}
		for (struct memory_chunk_t *ptr = memory_manager.first_memory_chunk; ptr != NULL ; ptr = ptr->next) {
			// PTR IN STRUCT
			for (size_t i = 0; i < sizeof(struct memory_chunk_t); ++i) {
				if ((uint8_t *)ptr + i == pointer) {
					return pointer_control_block;
				}
			}
			if (ptr->free) {
				// HIDDEN CASES
				if (ptr->next != NULL) {
					// HIDDEN CASE #1
					for (size_t i = 0; i < (size_t)((uint8_t*)ptr->next - (uint8_t*)ptr) - sizeof(struct memory_chunk_t); ++i) {
						if ((uint8_t *)ptr + sizeof(struct memory_chunk_t) + i == pointer) {
							return pointer_unallocated;
						}
					}
				} else {
					// HIDDEN CASE #2
					for (size_t i = 0; i < (uint8_t *)memory_manager.memory_start + memory_manager.memory_size - (uint8_t *)ptr - sizeof(struct memory_chunk_t); ++i) {
						if ((uint8_t *)ptr + sizeof(struct memory_chunk_t) + i == pointer) {
							return pointer_unallocated;
						}
					}
				}
			}
			// NOT FREE CASE
			else {
				// PTR IN FENCES
				for (size_t i = 0; i < FEN_SIZE; ++i) {
					// LEFT
					if ((uint8_t *)ptr + sizeof(struct memory_chunk_t) + i == pointer) {
						return pointer_inside_fences;
					}
					// RIGHT
					if ((uint8_t *)ptr + sizeof(struct memory_chunk_t) + i + FEN_SIZE + ptr->size == pointer) {
						return pointer_inside_fences;
					}
				}
				// PTR IN DATA BLOCK
				for (size_t i = 0; i < ptr->size; ++i) {
					if ((uint8_t *)ptr + sizeof(struct memory_chunk_t) + FEN_SIZE + i == pointer) {
						if (i == 0) {
							return pointer_valid;
						}
						return pointer_inside_data_block;
					}
				}
				// HIDDEN CASES
				if (ptr->next != NULL) {
					// HIDDEN CASE #1
					for (size_t i = 0; i < (size_t)((uint8_t*)ptr->next - (uint8_t*)ptr) - sizeof(struct memory_chunk_t) - ptr->size - FEN_SIZE*2; ++i) {
						if ((uint8_t *)ptr + sizeof(struct memory_chunk_t) + ptr->size + 2*FEN_SIZE + i == pointer) {
							return pointer_unallocated;
						}
					}
				} else {
					// HIDDEN CASE #2
					for (size_t i = 0; i < (uint8_t *)memory_manager.memory_start + memory_manager.memory_size - (uint8_t *)ptr - sizeof(struct memory_chunk_t) - ptr->size - FEN_SIZE*2; ++i) {
						if ((uint8_t *)ptr + sizeof(struct memory_chunk_t) + ptr->size + 2*FEN_SIZE + i == pointer) {
							return pointer_unallocated;
						}
					}
				}
			}
		}
	}
	return pointer_null;
}
int heap_setup(void) {
	if (memory_manager.memory_start) {
		return -1;
	}
	memory_manager.memory_start = custom_sbrk(0);
	if (memory_manager.memory_start == (void *) - 1) {
		return -1;
	}
	
	void *request = custom_sbrk(DEFAULT_SIZE);
	if (request == (void *) - 1) {
		return -1;
	}
	memory_manager.memory_size = DEFAULT_SIZE;
	memory_manager.first_memory_chunk = NULL;
	return 0;
}
void heap_clean(void) {
	if (memory_manager.memory_size >= DEFAULT_SIZE) {
		custom_sbrk(-memory_manager.memory_size);
		memory_manager.memory_start = NULL;
		memory_manager.first_memory_chunk = NULL;
	}
}

void *heap_malloc(size_t size) {
	if (memory_manager.memory_start == NULL || size < 1 || heap_validate() > 0) {
		return NULL;
	}

	// FREE MEM CASE
	if (memory_manager.first_memory_chunk == NULL) {
		uint8_t *ptr_i = (uint8_t *)memory_manager.memory_start;
		for (size_t i = 0 ; i < memory_manager.memory_size; ++i, ++ptr_i) {
			// LEFT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				// WORD SIZE COND
				if ((intptr_t)ptr_i % sizeof(void *) == 0) {
					// RIGHT COND
					if (memory_manager.memory_size - i < size + FEN_SIZE) {
						void *req = custom_sbrk(size + FEN_SIZE - memory_manager.memory_size + i);
						if (req == (void *) - 1) {
							return NULL;
						}
						memory_manager.memory_size += size + FEN_SIZE - memory_manager.memory_size + i;
					}
					struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
					n_chunk->prev = NULL;
					n_chunk->next = NULL;
					n_chunk->size = size;
					n_chunk->free = 0;
					n_chunk->lrc = calculateLRC(n_chunk);
					memory_manager.first_memory_chunk = n_chunk;
					return set_fences(n_chunk + 1, size);
				}
			}
		}
		return NULL;
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(1) {
		// FREE BLOCK CASE
		if (ptr->free) {
			// WORD SIZE COND
			if ((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) % sizeof(void *) == 0) {
				if (ptr->size >= size + 2*FEN_SIZE) {
					// ADD BLOCK COND
					if (ptr->size >= size + 2*FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = ptr->size - size - 2*FEN_SIZE - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
						}
						ptr->next = a_chunk;
					}
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size + 2*FEN_SIZE - ptr->size);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size + 2*FEN_SIZE - ptr->size;
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
			}
			// SPLIT CASE NEXT BLOCK
			uint8_t *ptr_i = (uint8_t *)(ptr + 1);
			for (size_t i = 0 ; i < ptr->size; ++i, ++ptr_i) {
				// WORD SIZE COND
				if ((intptr_t)(ptr_i) % sizeof(void *) == 0) {
					// LEFT COND
					if ((size_t)(ptr_i - (uint8_t *)(ptr + 1)) >= (size_t)((2*FEN_SIZE + 1) + sizeof(struct memory_chunk_t) + FEN_SIZE)) {
						// RIGHT COND
						if (ptr->size - i >= size + FEN_SIZE) {
							struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)(ptr_i + FEN_SIZE + size);
							struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);

							n_chunk->prev = ptr;
							n_chunk->size = size;
							n_chunk->free = 0;

							// ADD BLOCK COND
							if (ptr->size - i >= size + FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
								a_chunk->prev = n_chunk;
								a_chunk->next = ptr->next;
								a_chunk->size = ptr->size - i - size - FEN_SIZE - sizeof(struct memory_chunk_t);
								a_chunk->free = 1;
								a_chunk->lrc = calculateLRC(a_chunk);

								if (ptr->next != NULL) {
									ptr->next->prev = a_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
								n_chunk->next = a_chunk;
							} else {
								if (ptr->next != NULL) {
									ptr->next->prev = n_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
								n_chunk->next = ptr->next;
							}
							n_chunk->lrc = calculateLRC(n_chunk);

							ptr->next = n_chunk;
							ptr->size = ptr_i - (uint8_t *)(ptr + 1);
							ptr->lrc = calculateLRC(ptr);

							return set_fences(n_chunk + 1, size);
						}
					}
				}
			}
 		}
 		if (ptr->next == NULL) {
 			break;
 		}
		ptr = ptr->next;
	}
	uint8_t *ptr_i = custom_sbrk(0);
	for (size_t i = 0 ; ; ++i, ++ptr_i) {
		// WORD SIZE COND
		if ((intptr_t)(ptr_i) % sizeof(void *) == 0) {
			// STRCT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				void *req = custom_sbrk(size + FEN_SIZE + i);
				if (req == (void *) - 1) {
					return NULL;
				}
				memory_manager.memory_size += size + FEN_SIZE + i;
				struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
				n_chunk->prev = ptr;
				n_chunk->next = NULL;
				n_chunk->size = size;
				n_chunk->free = 0;
				n_chunk->lrc = calculateLRC(n_chunk);

				ptr->next = n_chunk;
				ptr->lrc = calculateLRC(ptr);
				return set_fences(n_chunk + 1, size);	
			}
		}
	}
	return NULL;
}
void *heap_malloc_zero(size_t size) {
	if (memory_manager.memory_start == NULL || size < 1 || heap_validate() > 0) {
		return NULL;
	}

	// FREE MEM CASE
	if (memory_manager.first_memory_chunk == NULL) {
		uint8_t *ptr_i = (uint8_t *)memory_manager.memory_start;
		for (size_t i = 0 ; i < memory_manager.memory_size; ++i, ++ptr_i) {
			// LEFT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				// WORD SIZE COND
				if ((intptr_t)ptr_i % sizeof(void *) == 0) {
					// RIGHT COND
					if (memory_manager.memory_size - i < size + FEN_SIZE) {
						void *req = custom_sbrk(size + FEN_SIZE - memory_manager.memory_size + i);
						if (req == (void *) - 1) {
							return NULL;
						}
						memory_manager.memory_size += size + FEN_SIZE - memory_manager.memory_size + i;
					}
					struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
					n_chunk->prev = NULL;
					n_chunk->next = NULL;
					n_chunk->size = size;
					n_chunk->free = 0;
					n_chunk->lrc = calculateLRC(n_chunk);
					memory_manager.first_memory_chunk = n_chunk;
					return set_fences_fill(n_chunk + 1, size);
				}
			}
		}
		return NULL;
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(1) {
		// FREE BLOCK CASE
		if (ptr->free) {
			// WORD SIZE COND
			if ((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) % sizeof(void *) == 0) {
				if (ptr->size >= size + 2*FEN_SIZE) {
					// ADD BLOCK COND
					if (ptr->size >= size + 2*FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = ptr->size - size - 2*FEN_SIZE - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
						}
						ptr->next = a_chunk;
					}
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences_fill(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size + 2*FEN_SIZE - ptr->size);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size + 2*FEN_SIZE - ptr->size;
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences_fill(ptr + 1, size);
				}
			}
			// SPLIT CASE NEXT BLOCK
			uint8_t *ptr_i = (uint8_t *)(ptr + 1);
			for (size_t i = 0 ; i < ptr->size; ++i, ++ptr_i) {
				// WORD SIZE COND
				if ((intptr_t)(ptr_i) % sizeof(void *) == 0) {
					// LEFT COND
					if ((size_t)(ptr_i - (uint8_t *)(ptr + 1)) >= (size_t)((2*FEN_SIZE + 1) + sizeof(struct memory_chunk_t) + FEN_SIZE)) {
						// RIGHT COND
						if (ptr->size - i >= size + FEN_SIZE) {
							struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)(ptr_i + FEN_SIZE + size);
							struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);

							n_chunk->prev = ptr;
							n_chunk->size = size;
							n_chunk->free = 0;

							// ADD BLOCK COND
							if (ptr->size - i >= size + FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
								a_chunk->prev = n_chunk;
								a_chunk->next = ptr->next;
								a_chunk->size = ptr->size - i - size - FEN_SIZE - sizeof(struct memory_chunk_t);
								a_chunk->free = 1;
								a_chunk->lrc = calculateLRC(a_chunk);

								if (ptr->next != NULL) {
									ptr->next->prev = a_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
								n_chunk->next = a_chunk;

							} else {
								if (ptr->next != NULL) {
									ptr->next->prev = n_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
								n_chunk->next = ptr->next;
							}
							n_chunk->lrc = calculateLRC(n_chunk);

							ptr->next = n_chunk;
							ptr->size = ptr_i - (uint8_t *)(ptr + 1);
							ptr->lrc = calculateLRC(ptr);

							return set_fences_fill(n_chunk + 1, size);
						}
					}
				}
			}
 		}
 		if (ptr->next == NULL) {
 			break;
 		}
		ptr = ptr->next;
	}
	uint8_t *ptr_i = custom_sbrk(0);
	for (size_t i = 0 ; ; ++i, ++ptr_i) {
		// WORD SIZE COND
		if ((intptr_t)(ptr_i) % sizeof(void *) == 0) {
			// STRCT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				void *req = custom_sbrk(size + FEN_SIZE + i);
				if (req == (void *) - 1) {
					return NULL;
				}
				memory_manager.memory_size += size + FEN_SIZE + i;
				struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
				n_chunk->prev = ptr;
				n_chunk->next = NULL;
				n_chunk->size = size;
				n_chunk->free = 0;
				n_chunk->lrc = calculateLRC(n_chunk);

				ptr->next = n_chunk;
				ptr->lrc = calculateLRC(ptr);
				return set_fences_fill(n_chunk + 1, size);	
			}
		}
	}
	return NULL;
}
void* heap_calloc(size_t number, size_t size) {
	if (memory_manager.memory_start == NULL || size < 1 || number < 1 || heap_validate() > 0) {
		return NULL;
	}
	return heap_malloc_zero(number*size);
}
void* heap_realloc(void* memblock, size_t size) {
	if (memory_manager.memory_start == NULL || heap_validate() > 0) {
		return NULL;
	}
	if (memblock == NULL) {
		return heap_malloc(size);
	}
	if (size == 0) {
		heap_free(memblock);
		return NULL;
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(ptr != NULL) {
		if (memblock == (void *)((uint8_t *)(ptr + 1) + FEN_SIZE)) {
			// FREE BLOCK PTR
			if (ptr->free == 1) {
				return NULL;
			}
			if ((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) % sizeof(void *) == 0) {
				size_t real_sum = ptr->size;
				if (ptr->next != NULL) {
					real_sum = (uint8_t *)ptr->next - (uint8_t *)ptr - sizeof(struct memory_chunk_t) - 2*FEN_SIZE;
				}
				// FITS
				if (real_sum >= size) {
					if (real_sum - size >= sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = real_sum - size - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
							ptr->next = a_chunk;
						}
					}
					ptr->size = size;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size - real_sum);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size - real_sum;
					ptr->size = size;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// IF NEXT IS FREE
				if (ptr->next != NULL && ptr->next->free == 1) {
					size_t right_size = ptr->next->size;
					if (ptr->next->next != NULL) {
						right_size = (uint8_t *)ptr->next->next - (uint8_t *)ptr->next;
					}
					// IF IT HAS THE SPACE
					if (right_size + real_sum >= size) {
						if (ptr->next->next != NULL) {
							ptr->next->next->prev = ptr;
							ptr->next->next->lrc = calculateLRC(ptr->next->next);
						}
						ptr->next = ptr->next->next;
						ptr->size = size;
						ptr->lrc = calculateLRC(ptr);
						return set_fences(ptr + 1, size);
					}
				}
			}
			// ADD NEW BLOCK CASE
			uint8_t *req = heap_malloc(size);
			if (req == NULL) {
				return NULL;
			}
			memcpy(req, memblock, ptr->size);
			heap_free(memblock);
			return req;
		}
		ptr = ptr->next;
	}
	// INVAID PTR
	return NULL;
}
size_t heap_get_largest_used_block_size(void) {
	if (memory_manager.memory_start == NULL || heap_validate()) {
		return 0;
	}
	size_t max = 0;
	struct memory_chunk_t *ptr = memory_manager.first_memory_chunk;
	if (ptr == NULL) {
		return 0;
	}
	while(ptr != NULL) {
		if (!ptr->free && ptr->size > max) {
			max = ptr->size;
		}
		ptr = ptr->next;
	}
	return max;
}
void *set_fences(void *address, size_t size) {
	if (address == NULL || size < 1) {
		return NULL;
	}
	for (int i = 0; i < FEN_SIZE; ++i) {
		*((uint8_t *)(address) + i) = 0xFF;
		*((uint8_t *)(address) + FEN_SIZE + size + i) = 0xFF;
	}
	return (void *)((uint8_t *)(address) + FEN_SIZE);
}
void *set_fences_fill(void *address, size_t size) {
	if (address == NULL || size < 1) {
		return NULL;
	}
	for (int i = 0; i < FEN_SIZE; ++i) {
		*((uint8_t *)(address) + i) = 0xFF;
		*((uint8_t *)(address) + FEN_SIZE + size + i) = 0xFF;
	}
	memset((uint8_t *)(address) + FEN_SIZE, 0, size);
	return (void *)((uint8_t *)(address) + FEN_SIZE);
}
void heap_free(void *address) {
	if (memory_manager.first_memory_chunk != NULL && address != NULL && heap_validate() == 0) {
		struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
		while(ptr != NULL) {
			// PTR EXISTS
			if (address == (void *)((uint8_t *)(ptr + 1) + FEN_SIZE)) {
				// FREE CURRENT BLOCK
				ptr->free = 1;
				// SET REAL SIZE
				if (ptr->next != NULL) {
					ptr->size = (uint8_t *)ptr->next - (uint8_t *)ptr - sizeof(struct memory_chunk_t);
				} else {
					ptr->size = (uint8_t *)memory_manager.memory_start + memory_manager.memory_size - (uint8_t *)ptr - sizeof(struct memory_chunk_t);
				}
				merge_chunks();
				ptr->lrc = calculateLRC(ptr);
				return;
			}
			ptr = ptr->next;
		}
	}
}
void merge_chunks(void) {
	if (memory_manager.first_memory_chunk == NULL) {
		return;
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while (ptr != NULL) {
		if (ptr->free) {
			int lcs = 0;
			struct memory_chunk_t *h_ptr = ptr;
			while(1) {
				if (h_ptr->free) {
					lcs += 1;
				} else {
					h_ptr = h_ptr->prev;
					break;
				}
				if (h_ptr->next == NULL) {
					break;
				}
				h_ptr = h_ptr->next;
			}
			if (lcs > 1) {
				if (h_ptr->next != NULL) {
					h_ptr->next->prev = ptr;
					h_ptr->next->lrc = calculateLRC(h_ptr->next);
				}
				ptr->next = h_ptr->next;
				if (ptr->next == NULL) {
					ptr->size = (size_t)((uint8_t *)memory_manager.memory_start + memory_manager.memory_size - (uint8_t *)ptr - sizeof(struct memory_chunk_t));
				} else {
					ptr->size = (uint8_t *)h_ptr->next - (uint8_t *)ptr - sizeof(struct memory_chunk_t);
				}
				ptr->lrc = calculateLRC(ptr);
				break;
			}
		}
		ptr = ptr->next;
	}
}

uint8_t calculateLRC(struct memory_chunk_t *ptr) {
	uint8_t tmp = ptr->lrc;
	ptr->lrc = 0;
	uint8_t LRC = 0;
	for (uint8_t i = 0; i < sizeof(struct memory_chunk_t); ++i) {
		LRC = (LRC + *((uint8_t *)ptr + i)) & 0xFF;
	}
	ptr->lrc = tmp;
	return ((LRC ^ 0xFF) + 1) & 0xFF;
}
int heap_validate(void) {
	if (memory_manager.memory_start == NULL) {
		return 2;
	}
	if (memory_manager.first_memory_chunk == NULL) {
		return 0;
	}
	// CHECK ALL LRC & FENCES
	struct memory_chunk_t *ptr = memory_manager.first_memory_chunk;
	while(ptr != NULL) {
		if (ptr->lrc != calculateLRC(ptr)) {
			return 3;
		}
		if (!ptr->free) {
			for (int i = 0; i < FEN_SIZE; ++i) {
				if (*((uint8_t *)(ptr) + sizeof(struct memory_chunk_t) + i) != 0xFF) {
					return 1;
				}
				if (*((uint8_t *)(ptr) + sizeof(struct memory_chunk_t) + FEN_SIZE + ptr->size + i) != 0xFF) {
					return 1;
				}
			}
		}
		ptr = ptr->next;
	}
	return 0;
}

void* heap_malloc_aligned(size_t count) {
	if (memory_manager.memory_start == NULL || count < 1 || heap_validate() > 0) {
		return NULL;
	}
	int size = count;

	// FREE MEM CASE
	if (memory_manager.first_memory_chunk == NULL) {
		// ALLOC NEW BLOCK
		if (memory_manager.memory_size < (size_t)(PAGE_SIZE + size + FEN_SIZE)) {
			void *request = custom_sbrk(PAGE_SIZE + size + FEN_SIZE - memory_manager.memory_size);
			if (request == (void *) - 1) {
				return NULL;
			}
			memory_manager.memory_size += PAGE_SIZE + size + FEN_SIZE - memory_manager.memory_size;
		}
		struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)((uint8_t *)memory_manager.memory_start + PAGE_SIZE - sizeof(struct memory_chunk_t) - FEN_SIZE);
		n_chunk->next = NULL;
		n_chunk->size = size;
		n_chunk->free = 0;
		
		// ALLOC FREE BLOCK
		struct memory_chunk_t *b_chunk = (struct memory_chunk_t *)memory_manager.memory_start;
		b_chunk->prev = NULL;
		b_chunk->size = (size_t)((uint8_t *)n_chunk - (uint8_t *)b_chunk - sizeof(struct memory_chunk_t));
		b_chunk->free = 1;


		// CORRECT PTRS & LRCs
		b_chunk->next = n_chunk;
		n_chunk->prev = b_chunk;
		b_chunk->lrc = calculateLRC(b_chunk);
		n_chunk->lrc = calculateLRC(n_chunk);

		memory_manager.first_memory_chunk = b_chunk;
		return set_fences(n_chunk + 1, size);
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(1) {
		// FREE BLOCK CASE
		if (ptr->free) {
			// PAGE COND
			if (((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
				if (ptr->size >= (size_t)size + 2*FEN_SIZE) {
					// ADD BLOCK COND
					if (ptr->size >= size + 2*FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = ptr->size - size - 2*FEN_SIZE - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
						}
						ptr->next = a_chunk;
					}
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size + 2*FEN_SIZE - ptr->size);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size + 2*FEN_SIZE - ptr->size;
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
			}
			// SPLIT CASE NEXT BLOCK
			uint8_t *ptr_i = (uint8_t *)(ptr + 1);
			for (size_t i = 0 ; i < ptr->size; ++i, ++ptr_i) {
				// PAGE COND
				if (((intptr_t)(ptr_i) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
					// LEFT COND
					if (i >= (size_t)(3*FEN_SIZE + sizeof(struct memory_chunk_t) + 1)) {
						// RIGHT COND
						if (ptr->size - i >= (size_t)size + FEN_SIZE) {
							struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)(ptr_i + FEN_SIZE + size);
							struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
							// ADD BLOCK COND
							if (ptr->size - i >= size + FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
								a_chunk->prev = n_chunk;
								a_chunk->next = ptr->next;
								a_chunk->size = ptr->size - i - size - FEN_SIZE - sizeof(struct memory_chunk_t);
								a_chunk->free = 1;
								a_chunk->lrc = calculateLRC(a_chunk);
								
								n_chunk->next = a_chunk;

								if (ptr->next != NULL) {
									ptr->next->prev = a_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
							} else {
								n_chunk->next = ptr->next;
								
								if (ptr->next != NULL) {
									ptr->next->prev = n_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
							}
							ptr->next = n_chunk;
							ptr->size = ptr_i - (uint8_t *)(ptr + 1) - sizeof(struct memory_chunk_t) - FEN_SIZE;
							
							n_chunk->prev = ptr;
							n_chunk->size = size;
							n_chunk->free = 0;
							
							n_chunk->lrc = calculateLRC(n_chunk);
							ptr->lrc = calculateLRC(ptr);

							return set_fences(n_chunk + 1, size);
						}
					}
				}
			}
 		}
 		if (ptr->next == NULL) {
 			break;
 		}
		ptr = ptr->next;
	}
	uint8_t *ptr_i = custom_sbrk(0);
	for (size_t i = 0 ; ; ++i, ++ptr_i) {
		// PAGE COND
		if (((intptr_t)(ptr_i) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
			// STRCT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
				struct memory_chunk_t *a_chunk  = custom_sbrk(0);
				void *req = custom_sbrk(size + FEN_SIZE + i);
				if (req == (void *) - 1) {
					return NULL;
				}
				memory_manager.memory_size += size + FEN_SIZE + i;

				n_chunk->prev = ptr;
				n_chunk->next = NULL;
				n_chunk->size = size;
				n_chunk->free = 0;
				ptr->next = n_chunk;
				// NEW BLOCK CASE
				if (i - FEN_SIZE - sizeof(struct memory_chunk_t) >= sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
					a_chunk->prev = ptr;
					a_chunk->next = n_chunk;
					a_chunk->size = i - 2*sizeof(struct memory_chunk_t) - 3*FEN_SIZE;
					a_chunk->free = 1;
					a_chunk->lrc = calculateLRC(a_chunk);

					n_chunk->prev = a_chunk;
					ptr->next = a_chunk;
				}
				n_chunk->lrc = calculateLRC(n_chunk);
				ptr->lrc = calculateLRC(ptr);
				return set_fences(n_chunk + 1, size);	
			}
		}
	}
	return NULL;
}
void* heap_malloc_aligned_zero(size_t count) {
	if (memory_manager.memory_start == NULL || count < 1 || heap_validate() > 0) {
		return NULL;
	}
	int size = count;

	// FREE MEM CASE
	if (memory_manager.first_memory_chunk == NULL) {
		// ALLOC NEW BLOCK
		if (memory_manager.memory_size < (size_t)(PAGE_SIZE + size + FEN_SIZE)) {
			void *request = custom_sbrk(PAGE_SIZE + size + FEN_SIZE - memory_manager.memory_size);
			if (request == (void *) - 1) {
				return NULL;
			}
			memory_manager.memory_size += PAGE_SIZE + size + FEN_SIZE - memory_manager.memory_size;
		}
		struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)((uint8_t *)memory_manager.memory_start + PAGE_SIZE - sizeof(struct memory_chunk_t) - FEN_SIZE);
		n_chunk->next = NULL;
		n_chunk->size = size;
		n_chunk->free = 0;
		
		// ALLOC FREE BLOCK
		struct memory_chunk_t *b_chunk = (struct memory_chunk_t *)memory_manager.memory_start;
		b_chunk->prev = NULL;
		b_chunk->size = (size_t)((uint8_t *)n_chunk - (uint8_t *)b_chunk - sizeof(struct memory_chunk_t));
		b_chunk->free = 1;


		// CORRECT PTRS & LRCs
		b_chunk->next = n_chunk;
		n_chunk->prev = b_chunk;
		b_chunk->lrc = calculateLRC(b_chunk);
		n_chunk->lrc = calculateLRC(n_chunk);

		memory_manager.first_memory_chunk = b_chunk;
		return set_fences_fill(n_chunk + 1, size);
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(1) {
		// FREE BLOCK CASE
		if (ptr->free) {
			// PAGE COND
			if (((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
				if (ptr->size >= (size_t)size + 2*FEN_SIZE) {
					// ADD BLOCK COND
					if (ptr->size >= size + 2*FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = ptr->size - size - 2*FEN_SIZE - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
						}
						ptr->next = a_chunk;
					}
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences_fill(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size + 2*FEN_SIZE - ptr->size);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size + 2*FEN_SIZE - ptr->size;
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences_fill(ptr + 1, size);
				}
			}
			// SPLIT CASE NEXT BLOCK
			uint8_t *ptr_i = (uint8_t *)(ptr + 1);
			for (size_t i = 0 ; i < ptr->size; ++i, ++ptr_i) {
				// PAGE COND
				if (((intptr_t)(ptr_i) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
					// LEFT COND
					if (i >= (size_t)(3*FEN_SIZE + sizeof(struct memory_chunk_t) + 1)) {
						// RIGHT COND
						if (ptr->size - i >= (size_t)size + FEN_SIZE) {
							struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)(ptr_i + FEN_SIZE + size);
							struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
							// ADD BLOCK COND
							if (ptr->size - i >= size + FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
								a_chunk->prev = n_chunk;
								a_chunk->next = ptr->next;
								a_chunk->size = ptr->size - i - size - FEN_SIZE - sizeof(struct memory_chunk_t);
								a_chunk->free = 1;
								a_chunk->lrc = calculateLRC(a_chunk);
								
								n_chunk->next = a_chunk;

								if (ptr->next != NULL) {
									ptr->next->prev = a_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
							} else {
								n_chunk->next = ptr->next;
								
								if (ptr->next != NULL) {
									ptr->next->prev = n_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
							}
							ptr->next = n_chunk;
							ptr->size = ptr_i - (uint8_t *)(ptr + 1) - sizeof(struct memory_chunk_t) - FEN_SIZE;
							
							n_chunk->prev = ptr;
							n_chunk->size = size;
							n_chunk->free = 0;
							
							n_chunk->lrc = calculateLRC(n_chunk);
							ptr->lrc = calculateLRC(ptr);
							
							return set_fences_fill(n_chunk + 1, size);
						}
					}
				}
			}
 		}
 		if (ptr->next == NULL) {
 			break;
 		}
		ptr = ptr->next;
	}
	uint8_t *ptr_i = custom_sbrk(0);
	for (size_t i = 0 ; ; ++i, ++ptr_i) {
		// PAGE COND
		if (((intptr_t)(ptr_i) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
			// STRCT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
				struct memory_chunk_t *a_chunk  = custom_sbrk(0);
				void *req = custom_sbrk(size + FEN_SIZE + i);
				if (req == (void *) - 1) {
					return NULL;
				}
				memory_manager.memory_size += size + FEN_SIZE + i;

				n_chunk->prev = ptr;
				n_chunk->next = NULL;
				n_chunk->size = size;
				n_chunk->free = 0;
				ptr->next = n_chunk;
				// NEW BLOCK CASE
				if (i - FEN_SIZE - sizeof(struct memory_chunk_t) >= sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
					a_chunk->prev = ptr;
					a_chunk->next = n_chunk;
					a_chunk->size = i - 2*sizeof(struct memory_chunk_t) - 3*FEN_SIZE;
					a_chunk->free = 1;
					a_chunk->lrc = calculateLRC(a_chunk);

					n_chunk->prev = a_chunk;
					ptr->next = a_chunk;
				}
				n_chunk->lrc = calculateLRC(n_chunk);
				ptr->lrc = calculateLRC(ptr);
				return set_fences_fill(n_chunk + 1, size);	
			}
		}
	}
	return NULL;
}
void* heap_calloc_aligned(size_t number, size_t size) {
	if (memory_manager.memory_start == NULL || size < 1 || number < 1 || heap_validate() > 0) {
		return NULL;
	}
	return heap_malloc_aligned_zero(number*size);
}
void* heap_realloc_aligned(void* memblock, size_t size) {
	if (memory_manager.memory_start == NULL || heap_validate() > 0) {
		return NULL;
	}
	if (memblock == NULL) {
		return heap_malloc_aligned(size);
	}
	if (size == 0) {
		heap_free(memblock);
		return NULL;
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(ptr != NULL) {
		if (memblock == (void *)((uint8_t *)(ptr + 1) + FEN_SIZE)) {
			// FREE BLOCK PTR
			if (ptr->free == 1) {
				return NULL;
			}
			if (((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
				size_t real_sum = ptr->size;
				if (ptr->next != NULL) {
					real_sum = (uint8_t *)ptr->next - (uint8_t *)ptr - sizeof(struct memory_chunk_t) - 2*FEN_SIZE;
				}
				// FITS
				if (real_sum >= size) {
					if (real_sum - size >= sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = real_sum - size - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
							ptr->next = a_chunk;
						}
					}
					ptr->size = size;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size - real_sum);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size - real_sum;
					ptr->size = size;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// IF NEXT IS FREE
				if (ptr->next != NULL && ptr->next->free == 1) {
					size_t right_size = ptr->next->size;
					if (ptr->next->next != NULL) {
						right_size = (uint8_t *)ptr->next->next - (uint8_t *)ptr->next;
					}
					// IF IT HAS THE SPACE
					if (right_size + real_sum >= size) {
						if (ptr->next->next != NULL) {
							ptr->next->next->prev = ptr;
							ptr->next->next->lrc = calculateLRC(ptr->next->next);
						}
						ptr->next = ptr->next->next;
						ptr->size = size;
						ptr->lrc = calculateLRC(ptr);
						return set_fences(ptr + 1, size);
					}
				}
			}
			// ADD NEW BLOCK CASE
			uint8_t *req = heap_malloc_aligned(size);
			if (req == NULL) {
				return NULL;
			}
			memcpy(req, memblock, ptr->size);
			heap_free(memblock);
			return req;
		}
		ptr = ptr->next;
	}
	// INVAID PTR
	return NULL;
}

void* heap_malloc_debug(size_t count, int fileline, const char* filename) {
	size_t size = count;
	if (memory_manager.memory_start == NULL || size < 1 || heap_validate() > 0) {
		return NULL;
	}

	// FREE MEM CASE
	if (memory_manager.first_memory_chunk == NULL) {
		uint8_t *ptr_i = (uint8_t *)memory_manager.memory_start;
		for (size_t i = 0 ; i < memory_manager.memory_size; ++i, ++ptr_i) {
			// LEFT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				// WORD SIZE COND
				if ((intptr_t)ptr_i % sizeof(void *) == 0) {
					// RIGHT COND
					if (memory_manager.memory_size - i < size + FEN_SIZE) {
						void *req = custom_sbrk(size + FEN_SIZE - memory_manager.memory_size + i);
						if (req == (void *) - 1) {
							return NULL;
						}
						memory_manager.memory_size += size + FEN_SIZE - memory_manager.memory_size + i;
					}
					struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
					n_chunk->prev = NULL;
					n_chunk->next = NULL;
					n_chunk->size = size;
					n_chunk->free = 0;
					n_chunk->filename = filename;
					n_chunk->fileline = fileline;
					n_chunk->lrc = calculateLRC(n_chunk);
					memory_manager.first_memory_chunk = n_chunk;
					return set_fences(n_chunk + 1, size);
				}
			}
		}
		return NULL;
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(1) {
		// FREE BLOCK CASE
		if (ptr->free) {
			// WORD SIZE COND
			if ((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) % sizeof(void *) == 0) {
				if (ptr->size >= size + 2*FEN_SIZE) {
					// ADD BLOCK COND
					if (ptr->size >= size + 2*FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = ptr->size - size - 2*FEN_SIZE - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->filename = filename;
						a_chunk->fileline = fileline;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
						}
						ptr->next = a_chunk;
					}
					ptr->size = size;
					ptr->free = 0;
					ptr->filename = filename;
					ptr->fileline = fileline;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size + 2*FEN_SIZE - ptr->size);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size + 2*FEN_SIZE - ptr->size;
					ptr->size = size;
					ptr->free = 0;
					ptr->filename = filename;
					ptr->fileline = fileline;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
			}
			// SPLIT CASE NEXT BLOCK
			uint8_t *ptr_i = (uint8_t *)(ptr + 1);
			for (size_t i = 0 ; i < ptr->size; ++i, ++ptr_i) {
				// WORD SIZE COND
				if ((intptr_t)(ptr_i) % sizeof(void *) == 0) {
					// LEFT COND
					if ((size_t)(ptr_i - (uint8_t *)(ptr + 1)) >= (size_t)((2*FEN_SIZE + 1) + sizeof(struct memory_chunk_t) + FEN_SIZE)) {
						// RIGHT COND
						if (ptr->size - i >= size + FEN_SIZE) {
							struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)(ptr_i + FEN_SIZE + size);
							struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);

							n_chunk->prev = ptr;
							n_chunk->size = size;
							n_chunk->filename = filename;
							n_chunk->fileline = fileline;
							n_chunk->free = 0;

							// ADD BLOCK COND
							if (ptr->size - i >= size + FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
								a_chunk->prev = n_chunk;
								a_chunk->next = ptr->next;
								a_chunk->size = ptr->size - i - size - FEN_SIZE - sizeof(struct memory_chunk_t);
								a_chunk->free = 1;
								a_chunk->filename = filename;
								a_chunk->fileline = fileline;
								a_chunk->lrc = calculateLRC(a_chunk);

								if (ptr->next != NULL) {
									ptr->next->prev = a_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
								n_chunk->next = a_chunk;
							} else {
								if (ptr->next != NULL) {
									ptr->next->prev = n_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
								n_chunk->next = ptr->next;
							}
							n_chunk->lrc = calculateLRC(n_chunk);
							ptr->filename = filename;
							ptr->fileline = fileline;
							ptr->next = n_chunk;
							ptr->size = ptr_i - (uint8_t *)(ptr + 1);
							ptr->lrc = calculateLRC(ptr);

							return set_fences(n_chunk + 1, size);
						}
					}
				}
			}
 		}
 		if (ptr->next == NULL) {
 			break;
 		}
		ptr = ptr->next;
	}
	uint8_t *ptr_i = custom_sbrk(0);
	for (size_t i = 0 ; ; ++i, ++ptr_i) {
		// WORD SIZE COND
		if ((intptr_t)(ptr_i) % sizeof(void *) == 0) {
			// STRCT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				void *req = custom_sbrk(size + FEN_SIZE + i);
				if (req == (void *) - 1) {
					return NULL;
				}
				memory_manager.memory_size += size + FEN_SIZE + i;
				struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
				n_chunk->prev = ptr;
				n_chunk->next = NULL;
				n_chunk->size = size;
				n_chunk->free = 0;
				n_chunk->filename = filename;
				n_chunk->fileline = fileline;
				n_chunk->lrc = calculateLRC(n_chunk);

				ptr->next = n_chunk;
				ptr->lrc = calculateLRC(ptr);
				return set_fences(n_chunk + 1, size);	
			}
		}
	}
	return NULL;
}
void* heap_malloc_zero_debug(size_t count, int fileline, const char* filename) {
	size_t size = count;
	if (memory_manager.memory_start == NULL || size < 1 || heap_validate() > 0) {
		return NULL;
	}

	// FREE MEM CASE
	if (memory_manager.first_memory_chunk == NULL) {
		uint8_t *ptr_i = (uint8_t *)memory_manager.memory_start;
		for (size_t i = 0 ; i < memory_manager.memory_size; ++i, ++ptr_i) {
			// LEFT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				// WORD SIZE COND
				if ((intptr_t)ptr_i % sizeof(void *) == 0) {
					// RIGHT COND
					if (memory_manager.memory_size - i < size + FEN_SIZE) {
						void *req = custom_sbrk(size + FEN_SIZE - memory_manager.memory_size + i);
						if (req == (void *) - 1) {
							return NULL;
						}
						memory_manager.memory_size += size + FEN_SIZE - memory_manager.memory_size + i;
					}
					struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
					n_chunk->prev = NULL;
					n_chunk->next = NULL;
					n_chunk->size = size;
					n_chunk->free = 0;
					n_chunk->filename = filename;
					n_chunk->fileline = fileline;
					n_chunk->lrc = calculateLRC(n_chunk);
					memory_manager.first_memory_chunk = n_chunk;
					return set_fences_fill(n_chunk + 1, size);
				}
			}
		}
		return NULL;
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(1) {
		// FREE BLOCK CASE
		if (ptr->free) {
			// WORD SIZE COND
			if ((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) % sizeof(void *) == 0) {
				if (ptr->size >= size + 2*FEN_SIZE) {
					// ADD BLOCK COND
					if (ptr->size >= size + 2*FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = ptr->size - size - 2*FEN_SIZE - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->filename = filename;
						a_chunk->fileline = fileline;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
						}
						ptr->next = a_chunk;
					}
					ptr->size = size;
					ptr->free = 0;
					ptr->filename = filename;
					ptr->fileline = fileline;
					ptr->lrc = calculateLRC(ptr);
					return set_fences_fill(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size + 2*FEN_SIZE - ptr->size);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size + 2*FEN_SIZE - ptr->size;
					ptr->size = size;
					ptr->free = 0;
					ptr->filename = filename;
					ptr->fileline = fileline;
					ptr->lrc = calculateLRC(ptr);
					return set_fences_fill(ptr + 1, size);
				}
			}
			// SPLIT CASE NEXT BLOCK
			uint8_t *ptr_i = (uint8_t *)(ptr + 1);
			for (size_t i = 0 ; i < ptr->size; ++i, ++ptr_i) {
				// WORD SIZE COND
				if ((intptr_t)(ptr_i) % sizeof(void *) == 0) {
					// LEFT COND
					if ((size_t)(ptr_i - (uint8_t *)(ptr + 1)) >= (size_t)((2*FEN_SIZE + 1) + sizeof(struct memory_chunk_t) + FEN_SIZE)) {
						// RIGHT COND
						if (ptr->size - i >= size + FEN_SIZE) {
							struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)(ptr_i + FEN_SIZE + size);
							struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);

							n_chunk->prev = ptr;
							n_chunk->size = size;
							n_chunk->filename = filename;
							n_chunk->fileline = fileline;
							n_chunk->free = 0;

							// ADD BLOCK COND
							if (ptr->size - i >= size + FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
								a_chunk->prev = n_chunk;
								a_chunk->next = ptr->next;
								a_chunk->size = ptr->size - i - size - FEN_SIZE - sizeof(struct memory_chunk_t);
								a_chunk->free = 1;
								a_chunk->filename = filename;
								a_chunk->fileline = fileline;
								a_chunk->lrc = calculateLRC(a_chunk);

								if (ptr->next != NULL) {
									ptr->next->prev = a_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
								n_chunk->next = a_chunk;
							} else {
								if (ptr->next != NULL) {
									ptr->next->prev = n_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
								n_chunk->next = ptr->next;
							}
							n_chunk->lrc = calculateLRC(n_chunk);
							ptr->filename = filename;
							ptr->fileline = fileline;
							ptr->next = n_chunk;
							ptr->size = ptr_i - (uint8_t *)(ptr + 1);
							ptr->lrc = calculateLRC(ptr);

							return set_fences_fill(n_chunk + 1, size);
						}
					}
				}
			}
 		}
 		if (ptr->next == NULL) {
 			break;
 		}
		ptr = ptr->next;
	}
	uint8_t *ptr_i = custom_sbrk(0);
	for (size_t i = 0 ; ; ++i, ++ptr_i) {
		// WORD SIZE COND
		if ((intptr_t)(ptr_i) % sizeof(void *) == 0) {
			// STRCT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				void *req = custom_sbrk(size + FEN_SIZE + i);
				if (req == (void *) - 1) {
					return NULL;
				}
				memory_manager.memory_size += size + FEN_SIZE + i;
				struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
				n_chunk->prev = ptr;
				n_chunk->next = NULL;
				n_chunk->size = size;
				n_chunk->free = 0;
				n_chunk->filename = filename;
				n_chunk->fileline = fileline;
				n_chunk->lrc = calculateLRC(n_chunk);

				ptr->next = n_chunk;
				ptr->lrc = calculateLRC(ptr);
				return set_fences_fill(n_chunk + 1, size);	
			}
		}
	}
	return NULL;
}
void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename) {
	if (memory_manager.memory_start == NULL || size < 1 || number < 1 || heap_validate() > 0) {
		return NULL;
	}
	return heap_malloc_zero_debug(number*size, fileline, filename);
}
void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename) {
	if (memory_manager.memory_start == NULL || heap_validate() > 0) {
		return NULL;
	}
	if (memblock == NULL) {
		return heap_malloc_debug(size, fileline, filename);
	}
	if (size == 0) {
		heap_free(memblock);
		return NULL;
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(ptr != NULL) {
		if (memblock == (void *)((uint8_t *)(ptr + 1) + FEN_SIZE)) {
			// FREE BLOCK PTR
			if (ptr->free == 1) {
				return NULL;
			}
			if ((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) % sizeof(void *) == 0) {
				size_t real_sum = ptr->size;
				if (ptr->next != NULL) {
					real_sum = (uint8_t *)ptr->next - (uint8_t *)ptr - sizeof(struct memory_chunk_t) - 2*FEN_SIZE;
				}
				// FITS
				if (real_sum >= size) {
					if (real_sum - size >= sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = real_sum - size - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->fileline = fileline;
						a_chunk->filename = filename;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
							ptr->next = a_chunk;
						}
					}
					ptr->fileline = fileline;
					ptr->filename = filename;
					ptr->size = size;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size - real_sum);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size - real_sum;
					ptr->fileline = fileline;
					ptr->filename = filename;
					ptr->size = size;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// IF NEXT IS FREE
				if (ptr->next != NULL && ptr->next->free == 1) {
					size_t right_size = ptr->next->size;
					if (ptr->next->next != NULL) {
						right_size = (uint8_t *)ptr->next->next - (uint8_t *)ptr->next;
					}
					// IF IT HAS THE SPACE
					if (right_size + real_sum >= size) {
						if (ptr->next->next != NULL) {
							ptr->next->next->prev = ptr;
							ptr->next->next->lrc = calculateLRC(ptr->next->next);
						}
						ptr->fileline = fileline;
						ptr->filename = filename;
						ptr->next = ptr->next->next;
						ptr->size = size;
						ptr->lrc = calculateLRC(ptr);
						return set_fences(ptr + 1, size);
					}
				}
			}
			// ADD NEW BLOCK CASE
			uint8_t *req = heap_malloc_debug(size, fileline, filename);
			if (req == NULL) {
				return NULL;
			}
			memcpy(req, memblock, ptr->size);
			heap_free(memblock);
			return req;
		}
		ptr = ptr->next;
	}
	// INVAID PTR
	return NULL;
}
void* heap_malloc_aligned_debug(size_t count, int fileline, const char* filename) {
	if (memory_manager.memory_start == NULL || count < 1 || heap_validate() > 0) {
		return NULL;
	}
	int size = count;

	// FREE MEM CASE
	if (memory_manager.first_memory_chunk == NULL) {
		// ALLOC NEW BLOCK
		if (memory_manager.memory_size < (size_t)(PAGE_SIZE + size + FEN_SIZE)) {
			void *request = custom_sbrk(PAGE_SIZE + size + FEN_SIZE - memory_manager.memory_size);
			if (request == (void *) - 1) {
				return NULL;
			}
			memory_manager.memory_size += PAGE_SIZE + size + FEN_SIZE - memory_manager.memory_size;
		}
		struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)((uint8_t *)memory_manager.memory_start + PAGE_SIZE - sizeof(struct memory_chunk_t) - FEN_SIZE);
		n_chunk->next = NULL;
		n_chunk->size = size;
		n_chunk->free = 0;
		n_chunk->fileline = fileline;
		n_chunk->filename = filename;
		
		// ALLOC FREE BLOCK
		struct memory_chunk_t *b_chunk = (struct memory_chunk_t *)memory_manager.memory_start;
		b_chunk->prev = NULL;
		b_chunk->size = (size_t)((uint8_t *)n_chunk - (uint8_t *)b_chunk - sizeof(struct memory_chunk_t));
		b_chunk->free = 1;
		b_chunk->fileline = fileline;
		b_chunk->filename = filename;


		// CORRECT PTRS & LRCs
		b_chunk->next = n_chunk;
		n_chunk->prev = b_chunk;
		b_chunk->lrc = calculateLRC(b_chunk);
		n_chunk->lrc = calculateLRC(n_chunk);

		memory_manager.first_memory_chunk = b_chunk;
		return set_fences(n_chunk + 1, size);
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(1) {
		// FREE BLOCK CASE
		if (ptr->free) {
			// PAGE COND
			if (((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
				if (ptr->size >= (size_t)size + 2*FEN_SIZE) {
					// ADD BLOCK COND
					if (ptr->size >= size + 2*FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = ptr->size - size - 2*FEN_SIZE - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->fileline = fileline;
						a_chunk->filename = filename;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
						}
						ptr->next = a_chunk;
					}
					ptr->fileline = fileline;
					ptr->filename = filename;
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size + 2*FEN_SIZE - ptr->size);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size + 2*FEN_SIZE - ptr->size;
					ptr->fileline = fileline;
					ptr->filename = filename;
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
			}
			// SPLIT CASE NEXT BLOCK
			uint8_t *ptr_i = (uint8_t *)(ptr + 1);
			for (size_t i = 0 ; i < ptr->size; ++i, ++ptr_i) {
				// PAGE COND
				if (((intptr_t)(ptr_i) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
					// LEFT COND
					if (i >= (size_t)(3*FEN_SIZE + sizeof(struct memory_chunk_t) + 1)) {
						// RIGHT COND
						if (ptr->size - i >= (size_t)size + FEN_SIZE) {
							struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)(ptr_i + FEN_SIZE + size);
							struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
							// ADD BLOCK COND
							if (ptr->size - i >= size + FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
								a_chunk->prev = n_chunk;
								a_chunk->next = ptr->next;
								a_chunk->size = ptr->size - i - size - FEN_SIZE - sizeof(struct memory_chunk_t);
								a_chunk->free = 1;
								a_chunk->fileline = fileline;
								a_chunk->filename = filename;
								a_chunk->lrc = calculateLRC(a_chunk);
								
								n_chunk->next = a_chunk;

								if (ptr->next != NULL) {
									ptr->next->prev = a_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
							} else {
								n_chunk->next = ptr->next;
								
								if (ptr->next != NULL) {
									ptr->next->prev = n_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
							}
							ptr->next = n_chunk;
							ptr->size = ptr_i - (uint8_t *)(ptr + 1) - sizeof(struct memory_chunk_t) - FEN_SIZE;
							
							n_chunk->prev = ptr;
							n_chunk->size = size;
							n_chunk->free = 0;
							n_chunk->fileline = fileline;
							n_chunk->filename = filename;
							
							n_chunk->lrc = calculateLRC(n_chunk);
							ptr->fileline = fileline;
							ptr->filename = filename;
							ptr->lrc = calculateLRC(ptr);

							return set_fences(n_chunk + 1, size);
						}
					}
				}
			}
 		}
 		if (ptr->next == NULL) {
 			break;
 		}
		ptr = ptr->next;
	}
	uint8_t *ptr_i = custom_sbrk(0);
	for (size_t i = 0 ; ; ++i, ++ptr_i) {
		// PAGE COND
		if (((intptr_t)(ptr_i) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
			// STRCT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
				struct memory_chunk_t *a_chunk  = custom_sbrk(0);
				void *req = custom_sbrk(size + FEN_SIZE + i);
				if (req == (void *) - 1) {
					return NULL;
				}
				memory_manager.memory_size += size + FEN_SIZE + i;
				n_chunk->fileline = fileline;
				n_chunk->filename = filename;
				n_chunk->prev = ptr;	
				n_chunk->next = NULL;
				n_chunk->size = size;
				n_chunk->free = 0;
				ptr->next = n_chunk;
				ptr->fileline = fileline;
				ptr->filename = filename;
				// NEW BLOCK CASE
				if (i - FEN_SIZE - sizeof(struct memory_chunk_t) >= sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
					a_chunk->prev = ptr;
					a_chunk->next = n_chunk;
					a_chunk->size = i - 2*sizeof(struct memory_chunk_t) - 3*FEN_SIZE;
					a_chunk->free = 1;
					a_chunk->fileline = fileline;
					a_chunk->filename = filename;
					a_chunk->lrc = calculateLRC(a_chunk);

					n_chunk->prev = a_chunk;
					ptr->next = a_chunk;
				}
				n_chunk->lrc = calculateLRC(n_chunk);
				ptr->lrc = calculateLRC(ptr);
				return set_fences(n_chunk + 1, size);	
			}
		}
	}
	return NULL;
}
void* heap_malloc_aligned_zero_debug(size_t count, int fileline, const char* filename) {
	if (memory_manager.memory_start == NULL || count < 1 || heap_validate() > 0) {
		return NULL;
	}
	int size = count;

	// FREE MEM CASE
	if (memory_manager.first_memory_chunk == NULL) {
		// ALLOC NEW BLOCK
		if (memory_manager.memory_size < (size_t)(PAGE_SIZE + size + FEN_SIZE)) {
			void *request = custom_sbrk(PAGE_SIZE + size + FEN_SIZE - memory_manager.memory_size);
			if (request == (void *) - 1) {
				return NULL;
			}
			memory_manager.memory_size += PAGE_SIZE + size + FEN_SIZE - memory_manager.memory_size;
		}
		struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)((uint8_t *)memory_manager.memory_start + PAGE_SIZE - sizeof(struct memory_chunk_t) - FEN_SIZE);
		n_chunk->next = NULL;
		n_chunk->size = size;
		n_chunk->free = 0;
		n_chunk->fileline = fileline;
		n_chunk->filename = filename;
		
		// ALLOC FREE BLOCK
		struct memory_chunk_t *b_chunk = (struct memory_chunk_t *)memory_manager.memory_start;
		b_chunk->prev = NULL;
		b_chunk->size = (size_t)((uint8_t *)n_chunk - (uint8_t *)b_chunk - sizeof(struct memory_chunk_t));
		b_chunk->free = 1;
		b_chunk->fileline = fileline;
		b_chunk->filename = filename;


		// CORRECT PTRS & LRCs
		b_chunk->next = n_chunk;
		n_chunk->prev = b_chunk;
		b_chunk->lrc = calculateLRC(b_chunk);
		n_chunk->lrc = calculateLRC(n_chunk);

		memory_manager.first_memory_chunk = b_chunk;
		return set_fences_fill(n_chunk + 1, size);
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(1) {
		// FREE BLOCK CASE
		if (ptr->free) {
			// PAGE COND
			if (((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
				if (ptr->size >= (size_t)size + 2*FEN_SIZE) {
					// ADD BLOCK COND
					if (ptr->size >= size + 2*FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = ptr->size - size - 2*FEN_SIZE - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->fileline = fileline;
						a_chunk->filename = filename;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
						}
						ptr->next = a_chunk;
					}
					ptr->fileline = fileline;
					ptr->filename = filename;
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences_fill(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size + 2*FEN_SIZE - ptr->size);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size + 2*FEN_SIZE - ptr->size;
					ptr->fileline = fileline;
					ptr->filename = filename;
					ptr->size = size;
					ptr->free = 0;
					ptr->lrc = calculateLRC(ptr);
					return set_fences_fill(ptr + 1, size);
				}
			}
			// SPLIT CASE NEXT BLOCK
			uint8_t *ptr_i = (uint8_t *)(ptr + 1);
			for (size_t i = 0 ; i < ptr->size; ++i, ++ptr_i) {
				// PAGE COND
				if (((intptr_t)(ptr_i) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
					// LEFT COND
					if (i >= (size_t)(3*FEN_SIZE + sizeof(struct memory_chunk_t) + 1)) {
						// RIGHT COND
						if (ptr->size - i >= (size_t)size + FEN_SIZE) {
							struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)(ptr_i + FEN_SIZE + size);
							struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
							// ADD BLOCK COND
							if (ptr->size - i >= size + FEN_SIZE + sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
								a_chunk->prev = n_chunk;
								a_chunk->next = ptr->next;
								a_chunk->size = ptr->size - i - size - FEN_SIZE - sizeof(struct memory_chunk_t);
								a_chunk->free = 1;
								a_chunk->fileline = fileline;
								a_chunk->filename = filename;
								a_chunk->lrc = calculateLRC(a_chunk);
								
								n_chunk->next = a_chunk;

								if (ptr->next != NULL) {
									ptr->next->prev = a_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
							} else {
								n_chunk->next = ptr->next;
								
								if (ptr->next != NULL) {
									ptr->next->prev = n_chunk;
									ptr->next->lrc = calculateLRC(ptr->next);
								}
							}
							ptr->next = n_chunk;
							ptr->size = ptr_i - (uint8_t *)(ptr + 1) - sizeof(struct memory_chunk_t) - FEN_SIZE;
							
							n_chunk->prev = ptr;
							n_chunk->size = size;
							n_chunk->free = 0;
							n_chunk->fileline = fileline;
							n_chunk->filename = filename;
							
							n_chunk->lrc = calculateLRC(n_chunk);
							ptr->fileline = fileline;
							ptr->filename = filename;
							ptr->lrc = calculateLRC(ptr);

							return set_fences_fill(n_chunk + 1, size);
						}
					}
				}
			}
 		}
 		if (ptr->next == NULL) {
 			break;
 		}
		ptr = ptr->next;
	}
	uint8_t *ptr_i = custom_sbrk(0);
	for (size_t i = 0 ; ; ++i, ++ptr_i) {
		// PAGE COND
		if (((intptr_t)(ptr_i) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
			// STRCT COND
			if (i >= sizeof(struct memory_chunk_t) + FEN_SIZE) {
				struct memory_chunk_t *n_chunk = (struct memory_chunk_t *)(ptr_i - sizeof(struct memory_chunk_t) - FEN_SIZE);
				struct memory_chunk_t *a_chunk  = custom_sbrk(0);
				void *req = custom_sbrk(size + FEN_SIZE + i);
				if (req == (void *) - 1) {
					return NULL;
				}
				memory_manager.memory_size += size + FEN_SIZE + i;
				n_chunk->fileline = fileline;
				n_chunk->filename = filename;
				n_chunk->prev = ptr;	
				n_chunk->next = NULL;
				n_chunk->size = size;
				n_chunk->free = 0;
				ptr->next = n_chunk;
				ptr->fileline = fileline;
				ptr->filename = filename;
				// NEW BLOCK CASE
				if (i - FEN_SIZE - sizeof(struct memory_chunk_t) >= sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
					a_chunk->prev = ptr;
					a_chunk->next = n_chunk;
					a_chunk->size = i - 2*sizeof(struct memory_chunk_t) - 3*FEN_SIZE;
					a_chunk->free = 1;
					a_chunk->fileline = fileline;
					a_chunk->filename = filename;
					a_chunk->lrc = calculateLRC(a_chunk);

					n_chunk->prev = a_chunk;
					ptr->next = a_chunk;
				}
				n_chunk->lrc = calculateLRC(n_chunk);
				ptr->lrc = calculateLRC(ptr);
				return set_fences_fill(n_chunk + 1, size);	
			}
		}
	}
	return NULL;
}
void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline, const char* filename) {
	if (memory_manager.memory_start == NULL || size < 1 || number < 1 || heap_validate() > 0) {
		return NULL;
	}
	return heap_malloc_aligned_zero_debug(number*size, fileline, filename);
}
void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline, const char* filename) {
	if (memory_manager.memory_start == NULL || heap_validate() > 0) {
		return NULL;
	}
	if (memblock == NULL) {
		return heap_malloc_aligned_debug(size, fileline, filename);
	}
	if (size == 0) {
		heap_free(memblock);
		return NULL;
	}
	struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
	while(ptr != NULL) {
		if (memblock == (void *)((uint8_t *)(ptr + 1) + FEN_SIZE)) {
			// FREE BLOCK PTR
			if (ptr->free == 1) {
				return NULL;
			}
			if (((intptr_t)((uint8_t *)(ptr + 1) + FEN_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
				size_t real_sum = ptr->size;
				if (ptr->next != NULL) {
					real_sum = (uint8_t *)ptr->next - (uint8_t *)ptr - sizeof(struct memory_chunk_t) - 2*FEN_SIZE;
				}
				// FITS
				if (real_sum >= size) {
					if (real_sum - size >= sizeof(struct memory_chunk_t) + 2*FEN_SIZE + 1) {
						struct memory_chunk_t *a_chunk  = (struct memory_chunk_t *)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + size + 2*FEN_SIZE);
						a_chunk->prev = ptr;
						a_chunk->next = ptr->next;
						a_chunk->size = real_sum - size - sizeof(struct memory_chunk_t);
						a_chunk->free = 1;
						a_chunk->filename = filename;
						a_chunk->fileline = fileline;
						a_chunk->lrc = calculateLRC(a_chunk);
						if (ptr->next != NULL) {
							ptr->next->prev = a_chunk;
							ptr->next->lrc = calculateLRC(ptr->next);
							ptr->next = a_chunk;
						}
					}
					ptr->filename = filename;
					ptr->fileline = fileline;
					ptr->size = size;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// EXPAND LAST BLOCK CASE
				if (ptr->next == NULL) {
					void *req = custom_sbrk(size - real_sum);
					if (req == (void *) - 1) {
						return NULL;
					}
					memory_manager.memory_size += size - real_sum;
					ptr->filename = filename;
					ptr->fileline = fileline;
					ptr->size = size;
					ptr->lrc = calculateLRC(ptr);
					return set_fences(ptr + 1, size);
				}
				// IF NEXT IS FREE
				if (ptr->next != NULL && ptr->next->free == 1) {
					size_t right_size = ptr->next->size;
					if (ptr->next->next != NULL) {
						right_size = (uint8_t *)ptr->next->next - (uint8_t *)ptr->next;
					}
					// IF IT HAS THE SPACE
					if (right_size + real_sum >= size) {
						if (ptr->next->next != NULL) {
							ptr->next->next->prev = ptr;
							ptr->next->next->lrc = calculateLRC(ptr->next->next);
						}
						ptr->filename = filename;
						ptr->fileline = fileline;
						ptr->next = ptr->next->next;
						ptr->size = size;
						ptr->lrc = calculateLRC(ptr);
						return set_fences(ptr + 1, size);
					}
				}
			}
			// ADD NEW BLOCK CASE
			uint8_t *req = heap_malloc_aligned_debug(size, fileline, filename);
			if (req == NULL) {
				return NULL;
			}
			memcpy(req, memblock, ptr->size);
			heap_free(memblock);
			return req;
		}
		ptr = ptr->next;
	}
	// INVAID PTR
	return NULL;
}
void print_mem(void) {
	if (memory_manager.first_memory_chunk != NULL) {
		struct memory_chunk_t* ptr = memory_manager.first_memory_chunk;
		size_t i = 0;
		size_t page_num = 0;
		while(ptr != NULL) {
			if (((intptr_t)((uint8_t *)ptr + sizeof(struct memory_chunk_t) + FEN_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0) {
				page_num++;
			}
			if (ptr->prev == NULL) {
				printf("\t----------START----------\n");
			}
			printf("\t    ╔═══════════════════════╗\n");
			printf("\t    ║       Block %4zu      ║\n", i);
			printf("\t    ║Page num :        %5zu║\n", page_num);
			printf("\t    ║Free     :        %5s║\n", ptr->free ? "Yes" : "No");
			printf("\t    ║Size     :        %4zuB║\n", ptr->size);
			printf("\t    ║Real size:        %4zuB║\n", ptr->next ? (size_t)((uint8_t*)ptr->next - (uint8_t*)ptr) : (ptr->free ? ptr->size + sizeof(struct memory_chunk_t) : ptr->size + sizeof(struct memory_chunk_t) + 2*FEN_SIZE));
			printf("\t    ║LRC      :         0x%02x║\n", ptr->lrc);
			printf("\t    ║Filename :   %10s║\n", ptr->filename);
			printf("\t    ║Fileline :        %5d║\n", ptr->fileline);
			printf("\t    ╚═══════════════════════╝\n");
			if (ptr->next == NULL) {
				printf("\t----------STOP----------\n");
			}
			ptr = ptr->next;
			i++;
		}
	}
}