#pragma once
/* ESP-IDF/host adapter: all decoder memory stays in the normal internal heap. */
#include <stdlib.h>
static inline void *helix_malloc(size_t size) { return malloc(size); }
static inline void helix_free(void *ptr) { free(ptr); }
