#include "test.h"

int main() {
    void* heap = heap_init(2);
    debug_heap(stdout, heap);
    start_test(heap);

    return 0;
}
