#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

_Noreturn void err( const char* msg, ... ) {
  va_list args;
  va_start (args, msg);
  vfprintf(stderr, msg, args);
  va_end (args);
  abort();
}

//Возвращает максимальный размер из двух
//Части блока сравниваются с минимальным размером, такие блоки мы делить не будем
extern inline size_t size_max( size_t x, size_t y );
