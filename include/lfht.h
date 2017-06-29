/*
 * Copyright (C) 2017  SRI International
 *
 */


/* Expanding (almsot) Lock Free Hash Table (using 64 and 128 bit CAS operations) */

#ifndef __LFHT_H__
#define __LFHT_H__


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>

enum tombstone { TOMBSTONE = 0 };

#define RESIZE_RATIO 0.6

/* (1 << 31) or 2^31 */
#define MAX_TABLE_SIZE ((uint32_t)0x80000000u)

#define KEY_ALIGNMENT  0x4

/*
 * The states of the table:
 * INITIAL   - we have just began, we have not yet needed to grow.
 * EXPANDING - we are in the process of expanding the table (migration has commenced)
 * EXPANDED - the table has been grown and migrated
 * FINAL - the table has been deleted
 */
typedef enum lfht_state { INITIAL, EXPANDING, EXPANDED, FINAL } lfht_state_t;

typedef struct lfht_entry_s {
  volatile uint64_t  key;
  volatile uint64_t  val;
} lfht_entry_t;

typedef struct lfht_hdr_s lfht_hdr_t;

struct lfht_hdr_s {
  /* the number of threads  whose handle points to this table header */
  atomic_uint_least32_t reference_count;
  /* flag to indicate if this table no longer contains relevant key/value pairs */
  atomic_bool assimilated;
  /* the "sizeof" the mmapped region that is the header + table  (we need this to mmunmap) */
  uint64_t sz;
  /* length of the table in units of lfht_entry_t's */
  uint32_t max;
  /* threshold beyond which we should grow the table */
  uint32_t threshold;
  /* the number of non-zero keys in the table */
  atomic_uint_least32_t count;
  /* pointer to the immediate predecessor table */
  lfht_hdr_t *next;
  /* the actual table */
  lfht_entry_t *table;
};


typedef struct lfht_s {
  /* the pointer to the current hdr together with it's current state */
  uintptr_t hdr:62, state:2;
  /* 
   * a lock that allows for mutual exclusion when attempting to grow
   * the table or free
   * final segments of old tables in the linked list that have
   * reference count zero.
   *
   * it would be nice to eliminate this (but difficult)
   *
   */
   pthread_mutex_t lock;
} lfht_t;


static inline lfht_hdr_t *lfht_table_hdr(const lfht_t* ht){
  return (lfht_hdr_t *)(uintptr_t)(ht->hdr << 2);
}

static inline lfht_state_t lfht_state(const lfht_t* ht){
  return ht->state;
}

static inline void lfht_set(lfht_t* ht, lfht_hdr_t * hdr, lfht_state_t state){
  ht->state = state;
  ht->hdr = (uintptr_t)hdr >> 2;
}


typedef struct lfht_handle_s {
  pthread_t owner;
  lfht_t *table;
  lfht_hdr_t *table_hdr;
  /* the last table header in the linked list whose reference count we have incremented.
   * we should never trespass past the last pointer (except perhaps in a free_table_check)
   *
   */
  lfht_hdr_t *last;  
} lfht_handle_t;


/*
 *  Initializes an uninitialized lfht_t struct.
 *  Used like so:
 *
 *  lfht_t ht;
 *  bool success = init_lfht(&ht, 4096);
 *  if(success){
 *   //off to races
 *
 *  }
 *
 *   - max is the initial number of key value pairs that the table will be able to store.
 *
 */

extern bool init_lfht(lfht_t *ht, uint32_t max);

extern bool delete_lfht(lfht_t *ht);

extern void init_handle(lfht_handle_t *h, lfht_t* table);

extern void delete_handle(lfht_handle_t *h);

extern void lfht_stats(FILE *fp, const char *name, lfht_t *ht);

/*
 * Insert or update the value of key in the table to val.
 *
 * - key must be  KEY_ALIGNMENT byte aligned (so we can tag it with ASSIMILATED).
 * - val *must* not be TOMBSTONE.
 *
 */
extern bool lfht_add(lfht_handle_t *h, uint64_t key, uint64_t val);

/*
 * Remove the value of key in the table, i.e. set it to TOMBSTONE.
 */
extern bool lfht_remove(lfht_handle_t *h, uint64_t key);


extern bool lfht_find(lfht_handle_t *h, uint64_t key, uint64_t *valp);



#endif
