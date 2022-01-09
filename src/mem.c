#include <stdarg.h>
#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "mem_internals.h"
#include "mem.h"
#include "util.h"

void debug_block(struct block_header* b, const char* fmt, ... );
void debug(const char* fmt, ... );

extern inline block_size size_from_capacity( block_capacity cap );
extern inline block_capacity capacity_from_size( block_size sz );

static bool            block_is_big_enough( size_t query, struct block_header* block ) { return block->capacity.bytes >= query; }
static size_t          pages_count   ( size_t mem )                      { return mem / getpagesize() + ((mem % getpagesize()) > 0); }
static size_t          round_pages   ( size_t mem )                      { return getpagesize() * pages_count( mem ) ; }

static void block_init( void* restrict addr, block_size block_sz, void* restrict next ) {
  *((struct block_header*)addr) = (struct block_header) {
    .next = next,
    .capacity = capacity_from_size(block_sz),
    .is_free = true
  };
}

static size_t region_actual_size( size_t query ) { return size_max( round_pages( query ), REGION_MIN_SIZE ); }

extern inline bool region_is_invalid( const struct region* r );



static void* map_pages(void const* addr, size_t length, int additional_flags) {
  return mmap( (void*) addr, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | additional_flags , 0, 0 );
}

/*  аллоцировать регион памяти и инициализировать его блоком */
static struct region alloc_region(void const * addr, size_t query) { 
  size_t size_of_region = region_actual_size(query);
  bool extends = true;
  void* map_addr = map_pages(addr, size_of_region, MAP_FIXED_NOREPLACE);
  struct region region;

  if (map_addr == MAP_FAILED) {
    extends = false;
    map_addr = map_pages(addr, size_of_region, MAP_FILE);
  }

  region = (struct region) {map_addr, query, extends};
  block_size block_size = {size_of_region};
  block_init(map_addr, block_size, NULL);

  return region;
}

static void* block_after( struct block_header const* block )         ;

void* heap_init( size_t initial ) {
  const struct region region = alloc_region( HEAP_START, initial );
  if ( region_is_invalid(&region) ) return NULL;

  return region.addr;
}

#define BLOCK_MIN_CAPACITY 24

/*  --- Разделение блоков (если найденный свободный блок слишком большой )--- */

static bool block_splittable( struct block_header* restrict block, size_t query) {
  return block-> is_free && query + offsetof( struct block_header, contents ) + BLOCK_MIN_CAPACITY <= block->capacity.bytes;
}

static bool split_if_too_big( struct block_header* block, size_t query ) {
  if (!block_splittable(block, query)) return false;

  size_t capacity = (query < BLOCK_MIN_CAPACITY) ? BLOCK_MIN_CAPACITY : query;
  block_size size = size_from_capacity((block_capacity) {block->capacity.bytes-offsetof(struct block_header, contents)-capacity});
  void* new_block = block->contents + capacity;
  block_init(new_block, size, block->next);
  block->next = new_block;
  block->capacity = (block_capacity) {capacity};

  return true;
}


/*  --- Слияние соседних свободных блоков --- */

static void* block_after( struct block_header const* block )              {
  return  (void*) (block->contents + block->capacity.bytes);
}
static bool blocks_continuous (
                               struct block_header const* fst,
                               struct block_header const* snd ) {
  return (void*)snd == block_after(fst);
}

static bool mergeable(struct block_header const* restrict fst, struct block_header const* restrict snd) {
  return fst->is_free && snd->is_free && blocks_continuous( fst, snd ) ;
}

static bool try_merge_with_next( struct block_header* block ) {
  if (!(block != NULL && block->next != NULL && mergeable(block, block->next))) return false;

  struct block_header* next = block->next;
  block_capacity capacity = (block_capacity) {.bytes = (block->capacity.bytes + size_from_capacity(next->capacity).bytes)};
  block->capacity = capacity;
  block->next = next->next;

  return true;
  
}


/*  --- ... ecли размера кучи хватает --- */

struct block_search_result {
  enum {BSR_FOUND_GOOD_BLOCK, BSR_REACHED_END_NOT_FOUND, BSR_CORRUPTED} type;
  struct block_header* block;
};


static struct block_search_result find_good_or_last  ( struct block_header* restrict block, size_t sz )    {
  if (block == NULL) return (struct block_search_result) {.type = BSR_CORRUPTED, .block = NULL};

  struct block_search_result block_result = {.type = BSR_REACHED_END_NOT_FOUND, .block = NULL};
  while (block_result.block == NULL && block != NULL) {
    if (block->is_free) {
      while (try_merge_with_next(block));
      if (block_is_big_enough(sz, block)) block_result = (struct block_search_result){.type = BSR_FOUND_GOOD_BLOCK, .block = block};
    }
    block = block->next;
  }

  return block_result;
}

/*  Попробовать выделить память в куче начиная с блока `block` не пытаясь расширить кучу
 Можно переиспользовать как только кучу расширили. */
static struct block_search_result try_memalloc_existing ( size_t query, struct block_header* block ) {
  if (block->capacity.bytes < BLOCK_MIN_CAPACITY) return (struct block_search_result) {.type = BSR_CORRUPTED, .block = block};

  struct block_search_result result = find_good_or_last(block,query);
  if (result.type == BSR_FOUND_GOOD_BLOCK) split_if_too_big(result.block,query);

  return result;
}



static struct block_header* grow_heap( struct block_header* restrict last, size_t query ) {
  struct block_header* result = block_after(last);
  struct region region = alloc_region(result,size_from_capacity((block_capacity){query}).bytes);
  last->next = region.addr;
  
  return region.addr;
}

/*  Реализует основную логику malloc и возвращает заголовок выделенного блока */
static struct block_header* memalloc( size_t query, struct block_header* heap_start) {
  struct block_search_result allocate_result = try_memalloc_existing(query, heap_start);

  if (allocate_result.type == BSR_REACHED_END_NOT_FOUND) {
    struct block_header *block_header = heap_start;
    while (block_header->next != NULL) block_header = block_header->next;
    grow_heap(block_header, query);
    allocate_result = try_memalloc_existing(query, heap_start);
  } 
  else if (allocate_result.type == BSR_CORRUPTED) {
    allocate_result.block = heap_init(query);
    allocate_result = try_memalloc_existing(query, allocate_result.block);
  }
  allocate_result.block->is_free = false;

  return allocate_result.block;
}

void* _malloc( size_t query ) {
  struct block_header* const addr = memalloc( query, (struct block_header*) HEAP_START );
  if (addr) return addr->contents;
  else return NULL;
}

static struct block_header* block_get_header(void* contents) {
  return (struct block_header*) (((uint8_t*)contents)-offsetof(struct block_header, contents));
}

//for debuging
struct block_header* get_header(void* contents) {
  return (struct block_header*) (((uint8_t*)contents)-offsetof(struct block_header, contents));
}

void _free( void* mem ) {
  if (!mem) return ;
  struct block_header* header = block_get_header( mem );
  header->is_free = true;
  
  while (try_merge_with_next(header));
}
