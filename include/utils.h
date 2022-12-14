/*
 * Copyright (C) 2017  SRI International
 *
 */
#ifndef __UTIL_H__
#define __UTIL_H__

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


/* for sanity checking */
static inline bool is_power_of_two(uint64_t n) {
  return (n & (n - 1)) == 0;
}

/* Fast modulo arithmetic, assuming that y is a power of 2 */
static inline size_t mod_power_of_two(uint64_t x, uint64_t y){
  assert(is_power_of_two(y));
  return x & (y - 1);
}


extern uint32_t jenkins_hash_uint64(uint64_t x);

extern uint32_t jenkins_hash_ptr(const void *p);


#endif


