#define _DEFAULT_SOURCE
#include "test.h"

//Обычное успешное выделение памяти
static bool test1(void* heap) {
    bool result = false;
    printf("test #1 started\n");
    printf("Normal memory allocation success\n");

    //Просто выделяем
    size_t* array = _malloc(sizeof(size_t));
    if (array) result = true;
    //Смотрим что все в норме
    debug_heap(stdout, heap);
    //Просто освобождаем
    _free(array);
    
    printf("test #1 finished\n");
    return result;
}

//Освобождение одного блока из нескольких выделенных
static bool test2(void* heap) {
    bool result = false;
    printf("test #2 started\n");
    printf("Freeing one block from several allocated\n");

    //выделяем память для трех переменных
    size_t* smt1 = _malloc(sizeof(size_t));
    uint64_t* smt2 = _malloc(sizeof(uint64_t));
    int64_t* smt3 = _malloc(sizeof(int64_t));
    
    debug_heap(stdout, heap);
    if (smt1 && smt2 && smt3) result = true;
    
    //освобождаем блок одной переменной
    _free(smt2);
    //проверяем, действительно ли память была освобождена
    debug_heap(stdout, heap);
    
    struct block_header* block = get_header(smt2);
    if (!block->is_free) result = false;
    
    _free(smt1);
    _free(smt3);

    printf("\ntest #2 finished\n");
    return result;
}

//Освобождение двух блоков из нескольких выделенных.
static bool test3(void* heap) {
    bool result = false;
    printf("test #3 started\n");
    printf("Freeing two blocks from several allocated ones.\n");

    //также выделяем память для трех переменных
    int* v1 = _malloc(sizeof(size_t));
    uint32_t* v2 = _malloc(sizeof(uint64_t));
    int32_t* v3 = _malloc(sizeof(int64_t));
    
    debug_heap(stdout, heap);
    if (v1 && v2 && v3) result = true;
    
    //освобождаем два блока
    _free(v2);
    _free(v3);
    //проверяем, действительно ли память была освобождена
    debug_heap(stdout, heap);
    
    struct block_header* block1 = get_header(v2);
    struct block_header* block2 = get_header(v3);
    if (!block1->is_free && !block2->is_free) result = false;
    
    _free(v1);

    printf("\ntest #3 finished\n");
    return result;
}

//Память закончилась, новый регион памяти расширяет старый.
static bool test4(void* heap) {
    bool result = false;
    printf("test #4 started\n");
    printf("The memory has run out, the new memory region expands the old one.\n");
    
    //Выделяем память
    void* something1 = _malloc(7000);
    void* something2 = _malloc(4500);
    
    debug_heap(stdout, heap);
    
    //Проверка, действительно ли память занята
    struct block_header* block1 = get_header(something1);
	struct block_header* block2 = get_header(something2);
    if (!block1->is_free && !block2->is_free && something1 && something2) result = true;
    
    //Освобождаем все
    _free(something1);
    _free(something2);

    printf("\ntest #4 finished\n");
    return result;
}

//Память закончилась, старый регион памяти не расширить из-за другого
//выделенного диапазона адресов, новый регион выделяется в другом месте.
static bool test5(void* heap) {
    bool result = false;
    printf("test #5 started\n");
    printf("The memory has run out,\nthe old memory region cannot be expanded due\nto a different allocated range of addresses,\nthe new region is allocated in a different place.\n");
    
    //Выделяем память и берем этот блок
    void* var1 = _malloc(4096);
    struct block_header* block1 = get_header(var1);

    //Берем слудующий блок, если доступен, то выделяем память там
    if (block1->next) block1 = block1->next;
    void* block = (void*)(((uint8_t*) var1) + size_from_capacity(block1->capacity).bytes);
    block = mmap(block, 228, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    void* var2 = _malloc(1024);

    debug_heap(stdout, heap);
    struct block_header* block2 = get_header(var2);
    if (block != MAP_FAILED && var2 && !block2->is_free) result = true;

    _free(var1);
    _free(var2);
    
    printf("\ntest #5 finished\n");
    return result;
}

void start_test(void* heap) {
    printf("Tests started");
    size_t count = 0;
    if (test1(heap)) count++;
    if (test2(heap)) count++;
    if (test3(heap)) count++;
    if (test4(heap)) count++;
    if (test5(heap)) count++;
    printf("Finish, %zu/5 - success", count);
}
