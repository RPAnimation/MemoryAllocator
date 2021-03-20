# Memory Allocator
Simple memory allocator, implements most of the POSIX functions.
## Prerequisites
This program uses custom `sbrk()` and `brk()` functions which are provided in `custom_unistd.h` header in order to safely request memory from artificial heap. It prevents user from corrupting system's memory and provides with easier debugging.
## Building
Program can be built with most compiliers such as GCC or Clang. It doesn't need any external dependencies.
## Description
All of the implemented functions have a `heap_` prefix in order to differentiate them from their POSIX counterparts. There are also functions that allign allocated memory to the `PAGE_SIZE` constant which in most systems is usually `4096` bytes.

There is a simple safety mechanism implementded, namely fences. Those are blocks of bytes around each memory block and are meant to detect any unsupervised write outside of a particular block. Their size can by adjusted by modyfying `FEN_SIZE` constant.

Additionaly, there are functions with `_debug` suffix. Those functions take `__LINE__` and `__FILE__` preprocessor's macros in order to save that information to newly allocated memory block's structure for easier debugging of the code.
## Sample program
Before we allocate any memory, we need to initialize the heap with `heap_setup()` function, simillarly when we are done using our allocator, we should call `heap_clean()`.
```
#include "heap.h"

int main() {
    heap_setup();
    int *chunk = (int *)heap_malloc(sizeof(int));
    if (!chunk) {
        return 1;
    }
    // Prints every chunk on the heap
    print_mem();
  
    heap_free(chunk);
    heap_clean();
    return 0;
}
```
