/*
 * Copyright (C) 2017  SRI International
 *
 */

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/mman.h>


#include "lfht.h"
#include "utils.h"
#include "sri_atomic.h"
#include "logging.h"

#define VERBOSE  false

/* logging currently only designed to be used in the single table use case */
#define LOGGING  false

/* migration tax rate.  */
#define MIGRATIONS_PER_ACCESS   8


/*
 * Ian's Tax rate calculation
 *
 * A table grows from N to 2N when there are at least N*R non-zero
 * keys, where R is the RESIZE_RATIO. In the worst case, the table may
 * contain N non-zero keys. This may happen if many threads add entries
 * to the table concurrently. More precisely, each thread checks whether
 * the resize threshold is reached *before* doing an addition. So if
 * N threads add entries concurrently, there may be N new entries in the
 * table.
 *
 * The new table will not grow until it contains at least 2N*R keys.
 * The migration process must then copy at most N keys into the new
 * table, and complete before there are 2N*R keys in the new table. We
 * force individual operation on the table to first move M entries
 * from the old to the new table. In the worst case, an individual
 * operation will then add (M+1) entries in the new table. Let k be
 * the number of operations needed to fully migrate the content from
 * old to new table.  We have: k = N/M, we want: k*(M+1) < 2N*R.
 *
 * N/M * (M + 1) < 2N*R
 * 1/M * (M + 1) < 2*R
 *  1 + 1/M < 2*R
 *      1/M < 2*R - 1
 *        M > 1/(2*R - 1)
 *
 * We have R = RESIZE_RATIO = 0.6, so M > 5 works. This also means
 * the R must be larger than 0.5.
 */

/*
 *
 * A key is marked as assimilated to indicate that the key-value pair
 * has been moved from the table it is in, to the current active
 * table.
 *
 * When all the key-value pairs in a table are marked as assimilated,
 * then the table_hdr itself is marked as assimilated.
 *
 * Slow threads present a nuisance here. They should check after
 * completing an operation, that they were not operating on a
 * assimilated table. If they were, then they need to repeat the
 * operation. Termination then becomes an issue.
 *
 */


/*
 * invariant:  lfht->hdr and lfht->state are only altered while lfht->lock is held.
 *
 * so when holding the lock, one is certain that the hdr and state are up
 * to date. apart from when we update these, crucial racey places are:
 *
 *  - initializing a handle (incrementing reference counters to tables 
 *    we may touch, current and last)
 * 
 *  - traversing the table chain when doing a free table check.
 *
 */

#define ASSIMILATED 0x1

static inline bool key_is_assimilated(uint64_t key){
  return (key & (KEY_ALIGNMENT - 1)) != 0;
}

static inline uint64_t set_assimilated(uint64_t key){
  return (key | ASSIMILATED);
}

static inline bool key_equal(uint64_t key0, uint64_t key1){
  return (key0 >> 1) == (key1 >> 1);
}

static inline bool table_is_assimilated(lfht_hdr_t *hdr) {
  return atomic_load(&hdr->assimilated);
}


bool free_lfht_hdr(lfht_hdr_t *hdr){
  if (hdr != NULL){
    int retcode = munmap(hdr, hdr->sz);
    if (retcode != 0){
      return false;
    }
  }
  return true;
}

lfht_hdr_t *alloc_lfht_hdr(uint32_t max){
  uint64_t sz;
  void *addr;
  lfht_hdr_t *hdr;
  sz = (max * sizeof(lfht_entry_t)) + sizeof(lfht_hdr_t);
  addr = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (addr != MAP_FAILED) {
    hdr = (lfht_hdr_t *)addr;
    atomic_init(&hdr->reference_count, 0);
    atomic_init(&hdr->assimilated, false);
    hdr->sz = sz;
    hdr->max = max;
    hdr->threshold = (uint32_t)(max * RESIZE_RATIO);
    atomic_init(&hdr->count, 0);
    hdr->next = NULL;
    hdr->table = addr + sizeof(lfht_hdr_t);
    return hdr;
  } else {
    return NULL;
  }
}

bool init_lfht(lfht_t *ht, uint32_t max){
  lfht_hdr_t *hdr;
  if(pthread_mutex_init(&ht->lock, NULL)){
    return false;
  }

  if( LOGGING ){ logging_init(); }

  if (ht != NULL && max != 0){
    hdr = alloc_lfht_hdr(max);
    if (hdr != NULL){
      lfht_set(ht, hdr, INITIAL);
      return true;
    }
  }
  return false;
}

bool delete_lfht(lfht_t *ht){
  lfht_hdr_t *hdr, *next;
  bool success;

  if( LOGGING ){ logging_shutdown(); }

  if (ht != NULL){
    hdr = lfht_table_hdr(ht);
    while(hdr != NULL){
      next = hdr->next;
      success = free_lfht_hdr(hdr);
      assert(success);
      lfht_set(ht, NULL, FINAL);
      hdr = next;
    }
    if(pthread_mutex_destroy(&ht->lock)){
      return false;
    }
    return true;
  }
  return false;
}

/* check header chain */
uint32_t hdr_chain_length(lfht_t *ht){
  uint32_t length = 0;
  lfht_hdr_t *hdr;
  hdr = lfht_table_hdr(ht);
  while(hdr != NULL){
    hdr = hdr->next;
    length++;
  }
  return length;
}

/*
 *
 *
 */



/* 
 * penultimate_table_hdr(ht) returns the second last (non-NULL) TRAILING header, or NULL if there
 * is less than three hdrs in the chain. It is only called when holding the table lock.
*/
lfht_hdr_t *penultimate_table_hdr(lfht_t *ht){
  lfht_hdr_t *hdr, *prev, *next;
  hdr = lfht_table_hdr(ht);
  prev = hdr;
  while(hdr != NULL){
    next = hdr->next;
    if(next == NULL){
      break;
    } else {
      if(hdr != prev){ prev = hdr; }
      hdr = next;
    }
  }
  return (prev != hdr) ? prev : NULL;
}



/*
 * need to walk the linked list of table_hdrs
 * and see if it can delete a tails segment of zero ref counted hdrs.
 * note that more than one thread could be doing this, so we
 * will probably need to add a lock to the lfht_t structure.
 *
 * so when can we free a table? clearly its ref count must be zero,
 * and it must also be assimilated. but is this enough? i do not
 * think so. at least not while we are decrementing the ref count
 * in update_handle.
 *
 */
static void free_table_check(lfht_t *ht, const char* where, bool need_lock){
  int errcode, rcount;
  lfht_hdr_t *penultimate, *hdr;
  bool assimilated, success;

  if(VERBOSE){ fprintf(stderr, "free_table_check(%s)!\n", where); }

  if(need_lock){
    errcode = pthread_mutex_trylock(&ht->lock);
  } else {
    errcode = 0;
  }
  if( errcode == 0 ){
    penultimate  = penultimate_table_hdr(ht);

    assert((penultimate != NULL) ||  (hdr_chain_length(ht) < 3));

    if(penultimate != NULL){
      hdr = penultimate->next;
      rcount = atomic_load(&hdr->reference_count);
      assimilated = table_is_assimilated(hdr);
      if(assimilated && (rcount == 0)){
        penultimate->next = NULL;
        success = free_lfht_hdr(hdr);
        if (VERBOSE) {
          fprintf(stderr, "FREEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE (%s)!\n", where);
        }
        assert(success);
      } else {
        if (VERBOSE) {
          fprintf(stderr, "UNRIPE(%s) assimimated = %d reference_count = %d\n",
                  where, assimilated, rcount);
        }
      }
    }
    if(need_lock){
      pthread_mutex_unlock(&ht->lock);
    }
  }
}

/* lock free variant */
void init_handle(lfht_handle_t *h, lfht_t* table){
  lfht_hdr_t *actual, *next;
    
  h->owner = pthread_self();
  h->table = table;

  /* the lock is to prevent the table->table_hdr from changing before we can increment the counter */
  pthread_mutex_lock(&table->lock);
  
  actual = lfht_table_hdr(table);
  next = actual->next;
  /*
   * if the lock was not held, actual could by this point be out of date, and so we would actually
   * miss incrementing the current table's reference count.
   */

  
  atomic_fetch_add(&actual->reference_count, 1);
  /* 
   * if there is an old table, and it is not assimilated, then we will
   * probably need to help migrate it, so we increment its reference count.
   *
   */
  if(next != NULL && !table_is_assimilated(next)){
    atomic_fetch_add(&next->reference_count, 1);
    h->last = next;
  } else {
    h->last = actual;
  }
  h->table_hdr = actual;


  pthread_mutex_unlock(&table->lock);
  
}

/*
 * check whether our handle needs to be updated. possibly need to split into two ops.
 */
static bool upto_date(lfht_handle_t *h){
  return lfht_table_hdr(h->table) == h->table_hdr;
}


static void update_handle(lfht_handle_t *h){
  lfht_t *table;
  lfht_hdr_t *actual, *current, *last, *p;
  int prior_count;
  bool assimilated;

  assert(pthread_self() == h->owner);

  table = h->table;
  actual = lfht_table_hdr(table);
  current  = h->table_hdr;
  last = h->last;
  prior_count = 2;
  if(actual == current){
    return;
  }
  h->table_hdr = actual;
  /* increment to the left */
  p = actual;
  while(true){
    atomic_fetch_add(&p->reference_count, 1);
    p = p->next;
    if(p == current){ break; }
  }
  if(current == last){  return; }
  /* decrement to the right till last */
  p = current->next;
  while(true){
    assimilated = table_is_assimilated(p);
    assert(assimilated);
    prior_count = atomic_fetch_sub(&p->reference_count, 1);

    assert(prior_count > 0);

    if(p == last){ break; }
    p = p->next;
  }
  h->last = current;
  if(prior_count == 1){
    /* good time to do a free table check? */
    free_table_check(h->table, "update_handle", true);
  }
}


void delete_handle(lfht_handle_t *h){
  lfht_hdr_t *current, *p, *last;
  int prior_count;

  assert(pthread_self() == h->owner);
 
  current = h->table_hdr;
  last = h->last;
  /* decrement from current to last; */
  p = current;
  while(true){
    prior_count = atomic_fetch_sub(&p->reference_count, 1);

    assert(prior_count > 0);

    if(p == last){ break; }
    p = p->next;
  }
  h->last = NULL;
  h->table = NULL;
  h->table_hdr = NULL;
  /* 
   * iam: this works very well, but a library would not get this luxury.
   * so we leave it commented out for now.
   */
  //free_table_check(h->table, "delete_handle", true);
}


/* returns true if this attempt succeeded; false otherwise. */
bool _grow_table(lfht_t *ht,  lfht_hdr_t *hdr){
  lfht_hdr_t *ohdr, *nhdr;
  bool retval = false;
  uint32_t omax, nmax;

  pthread_mutex_lock(&ht->lock);
  
  ohdr = lfht_table_hdr(ht);


  /* check to see if someone beat us to it */
  if(hdr == ohdr){
    omax = ohdr->max;
  
    /* iam: we should be able to assert that the reference count of ohdr is >= 1 */
    if (omax < MAX_TABLE_SIZE) {
      nmax = 2 * ohdr->max;
      nhdr  = alloc_lfht_hdr(nmax);
      if (nhdr != NULL){
	
	nhdr->next = ohdr;
	
	
	/* iam: we should be able to assert that the reference count of ohdr is >= 1 */
	/*
	 *    New  Bug: May 3 2017
	 *
	 *	This has to be atomic, because otherwise the table_hdr can be updated, but the
	 *	state remains whatever (not EXPANDING), and so a find thread may not do any work in the
	 *	migrate_table other than UPDATE the handle. The find thread then looks in the new table
	 *	and doesn't find it, since it is upto date the find fails. BUT the key has a value in the
	 *	old table!
	 */
	/* we have the lock so we do not need a CAS! */
	lfht_set(ht, nhdr, EXPANDING);
	retval = true;
	
      }
    }
  }

  pthread_mutex_unlock(&ht->lock);
  
  return retval;
}


/* the taxation routines */

static uint32_t assimilate(lfht_handle_t *h, lfht_hdr_t *from_hdr, uint64_t key, uint32_t hash,  uint32_t count);

static int32_t _migrate_table(lfht_handle_t *h, uint64_t key, uint32_t hash);


/* internal versions of the hash table API */
static void _lfht_move(lfht_handle_t *h, uint64_t key, uint32_t hash, uint64_t val);
static bool _lfht_add(lfht_handle_t *h, uint64_t key, uint32_t hash, uint64_t val);
static bool _lfht_find(lfht_handle_t *h, uint64_t key, uint32_t hash, uint64_t *valp);
static bool _lfht_remove(lfht_handle_t *h, uint64_t key, uint32_t hash);


bool lfht_add(lfht_handle_t *h, uint64_t key, uint64_t val){
  uint32_t hash;
  bool retval;
  bool current;
  lfht_hdr_t *table_hdr;
  uint_least32_t count;
  int32_t migrated;
  
  if( false && LOGGING ){ logging_record(ADD, true, h, key, val, 0); }

  hash = jenkins_hash_ptr((void *)key);
  while(true){
    migrated = _migrate_table(h, key, hash);
    retval = _lfht_add(h, key, hash, val);
    current = upto_date(h);
    if( retval && current ){ break; }
    if( ! current ){ update_handle(h); }
  }
  table_hdr = h->table_hdr;
  count = atomic_load(&table_hdr->count);
  if (count >= table_hdr->threshold){
    _grow_table(h->table, table_hdr);
  }

  if( false && LOGGING ){ logging_record(ADD, false, h, key, val, migrated); }
  return retval;
}


static void _lfht_move(lfht_handle_t *h, uint64_t key, uint32_t hash, uint64_t val){
  uint32_t mask, j, i;
  lfht_hdr_t *hdr;
  lfht_entry_t*  table;
  lfht_entry_t current;
  lfht_entry_t desired;
  lfht_t *ht;

  ht = h->table;

  if( LOGGING ){ logging_record(_MOVE, true, h, key, val, 0); }

  assert( ht != NULL );
  assert( lfht_table_hdr(ht) != NULL );
  assert( key != 0 );
  assert( ! key_is_assimilated(key) );
  assert( val != TOMBSTONE );

  hdr = lfht_table_hdr(ht);
  table = hdr->table;
  mask = hdr->max - 1;
  j = hash & mask;
  i = j;


  desired.key = key;
  desired.val = val;

  /*
   * Warning: we want the 'continue' to go back to the start of the loop.
   * So a do { } while (i != j); does not work.
   * Same issue in _lfht_remove.
   */
  while (true) {
    current = table[i];
    if (current.key == 0){
      /* 
       * have to do a 128 bit cas here, otherwise a slow move thread can let a find thread think
       * this key has been tombstoned
       */
      if (cas_128((volatile u128_t *)&(table[i]), *((u128_t *)&current),  *((u128_t *)&desired))){
        atomic_fetch_add(&hdr->count, 1);
        goto exit;
      } else {
        continue;
      }
    }
    if ( current.key ==  key ){
      goto exit;
    }
    i++;
    i &= mask;
    if (i == j) break;
  }
  /* should never fail */
  assert(false);

 exit:
  
  if( LOGGING ){ logging_record(_MOVE, false, h, key, val, 0); }
 
}

static bool _lfht_add(lfht_handle_t *h, uint64_t key, uint32_t hash, uint64_t val){
  uint32_t mask, j, i;
  lfht_hdr_t *hdr;
  lfht_entry_t*  table;
  lfht_entry_t current;
  lfht_entry_t desired;
  bool retval;
  lfht_t *ht;
  ht = h->table;

  retval = false;

  if( LOGGING ){ logging_record(_ADD, true, h, key, val, 0); }

  assert( ht != NULL );
  assert( lfht_table_hdr(ht) != NULL );
  assert( key != 0 );
  assert( ! key_is_assimilated(key) );
  assert( val != TOMBSTONE );

  hdr = lfht_table_hdr(ht);
  table = hdr->table;
  mask = hdr->max - 1;
  j = hash & mask;
  i = j;


  desired.key = key;
  desired.val = val;
    
  /*
   * Warning: we want the 'continue' to go back to the start of the loop.
   * So a do { } while (i != j); does not work.
   * Same issue in _lfht_remove.
   */
  while (true) {
    current = table[i];
    if (current.key == 0){
      /* 
       * have to do a 128 bit cas here, otherwise a slow add thread can let a find thread think
       * this key has been tombstoned
       */
      if (cas_128((volatile u128_t *)&(table[i]), *((u128_t *)&current),  *((u128_t *)&desired))){
        atomic_fetch_add(&hdr->count, 1);
	retval = true;
        goto exit;
      } else {
        continue;
      }
    }
    if ( key_equal(current.key, key) ){
      if( key_is_assimilated(current.key) ){
        retval = false;
        goto exit;
      } else {
	if (cas_128((volatile u128_t *)&(table[i]), *((u128_t *)&current),  *((u128_t *)&desired))){
          retval = true;
          goto exit;
        } else {
          continue;
        }
      }
    }
    i++;
    i &= mask;
    if (i == j) break;
  }

 exit:

  if( LOGGING ){ logging_record(_ADD, false, h, key, val, 0); }

  return retval;
}

bool lfht_remove(lfht_handle_t *h, uint64_t key){
  uint32_t hash;
  bool retval, current;
  int32_t migrated;

  if( false && LOGGING ){ logging_record(REMOVE, true, h, key, 0, 0); }

  hash = jenkins_hash_ptr((void *)key);
  while(true){
    migrated = _migrate_table(h, key, hash);
    retval = _lfht_remove(h, key, hash);
    current = upto_date(h);
    if( retval || current ){ break; }
    update_handle(h);
  }

  if( false && LOGGING ){ logging_record(REMOVE, false, h, key, retval, migrated); }

  return retval;
}


static bool _lfht_remove(lfht_handle_t *h, uint64_t key, uint32_t hash){
  uint32_t mask, j, i;
  lfht_hdr_t *hdr;
  lfht_entry_t*  table;
  lfht_entry_t current;

  lfht_t *ht;
  ht = h->table;

  assert( ht != NULL );
  assert( lfht_table_hdr(ht) != NULL );
  assert( key != 0 );
  assert( ! key_is_assimilated(key) );

  hdr = lfht_table_hdr(ht);
  table = hdr->table;
  mask = hdr->max - 1;
  j = hash & mask;
  i = j;

  while (true) {
    current = table[i];
    if (key_equal(current.key, key)){
      if( key_is_assimilated(current.key) ){
        return false;
      } else {
        if (cas_64((volatile uint64_t *)&(table[i].val), current.val, TOMBSTONE)){
          return true;
        } else {
          continue;
        }
      }
    } else if (current.key == 0){
      return false;
    }
    i++;
    i &= mask;
    if (i == j) break;
  }

  return false;
}



bool lfht_find(lfht_handle_t *h, uint64_t key, uint64_t *valp){
  uint32_t hash;
  bool retval, current;
  int32_t migrated;

  if( false && LOGGING ){ logging_record(FIND, true, h, key, 0, 0); }

  hash = jenkins_hash_ptr((void *)key);
  while(true){
    migrated = _migrate_table(h, key, hash);
    retval =  _lfht_find(h, key, hash, valp);
    current = upto_date(h);
    /*
     * BD suggests this is wrong.
     * we should ignore the retval in the test, and only
     * return retval when we are up to date.
     *
     * if(retval || current ){ break; }
     *
     */
    if( current ){ break; }
    update_handle(h);
  }

  if( false && LOGGING ){ logging_record(REMOVE, false, h, key, retval, migrated); }

  return retval;
}


static bool _lfht_find(lfht_handle_t *h, uint64_t key, uint32_t hash, uint64_t *valp){
  uint32_t mask, j, i;
  uint64_t kval;
  lfht_hdr_t *hdr;
  lfht_entry_t*  table;

  lfht_t *ht;
  ht = h->table;

  assert( ht != NULL );
  assert( lfht_table_hdr(ht) != NULL );
  assert( key != 0 );
  assert( ! key_is_assimilated(key) );
  assert( valp != NULL );

  hdr = lfht_table_hdr(ht);
  table = hdr->table;
  mask = hdr->max - 1;
  j = hash & mask;
  i = j;
  while (true) {
    kval = read_64((volatile uint64_t *)&table[i].key);
    if (kval == 0){
      return false;
    }
    if ( key_equal(kval, key) ){
      if( key_is_assimilated(kval) ){
        return false;
      } else {
        *valp = table[i].val;
        return true;
      }
    }
    i++;
    i &= mask;
    if (i == j) break;
  }

  return false;
}

/*
 * The migration tax.
 *
 * The thread attempts to move at least count key-value pairs from the
 * old table (from_hdr) to the new table ht->table_hdr. It starts
 * the job where the key of interest may lie. It also makes sure that
 * the key of interest, if it has a non-TOMBSTONE value in the table,
 * has been moved.  Thus after paying the migration tax, the operation
 * can concentrate on the current table to service the request.
 *
 * Drew says we could think about doing this in a cache friendly fashion.
 *
 * FIXME: Drew could you, for the record, elaborate on this, here.
 *
 */

static int32_t _migrate_table(lfht_handle_t *h, uint64_t key, uint32_t hash){
  int table_state;
  lfht_t *ht;
  lfht_hdr_t *hdr;
  lfht_hdr_t *ohdr;
  uint32_t moved;
  bool finished, assimilated;
  int32_t migrated;

  migrated = -1;
  
 restart:

  if( false && LOGGING ){ logging_record(MIGRATE, true, h, key, 0, 0); }


  update_handle(h);

  ht = h->table;

  table_state = lfht_state(ht);

  if (table_state == EXPANDING){
    /* gotta try and pitch in and do some migrating */
    hdr = lfht_table_hdr(ht);
    ohdr = hdr->next;
    /* could move this into assimilate now too if we thought that was a good idea... */
    if (!upto_date(h) ){ goto restart; }

    if(hdr == h->last){ return migrated; }
    
    moved = assimilate(h, ohdr, key, hash,  MIGRATIONS_PER_ACCESS);
    
    finished = moved < MIGRATIONS_PER_ACCESS;
    
    assimilated = false;
    migrated = moved;

    if (finished  &&  atomic_compare_exchange_strong(&ohdr->assimilated, &assimilated, true)){
      
      pthread_mutex_lock(&ht->lock);

      /* we hold the lock so ht->table_hdr cannot change while we are in here. */
      if (lfht_state(ht) == EXPANDING  && lfht_table_hdr(ht) == hdr){
	lfht_set(ht, hdr, EXPANDED);
        free_table_check(h->table, "_migrate_table", false);
      }

      pthread_mutex_unlock(&ht->lock);

      
    }

  }
  if( false && LOGGING ){ logging_record(MIGRATE, false, h, key, 0, 0); }

  return migrated;
}




static uint32_t assimilate(lfht_handle_t *h, lfht_hdr_t *from_hdr, uint64_t key, uint32_t hash,  uint32_t count){
  uint32_t retval, mask, j, i;
  lfht_entry_t current;
  uint64_t akey;
  lfht_entry_t*  table;
  bool success;

  if( false && LOGGING ){ logging_record(ASSIMILATE, true, h, key, 0, (uint64_t)from_hdr); }


  /* success indicates that the key is either not in the table or has been assimilated */
  success = false;
  retval = 0;
  mask = from_hdr->max - 1;
  table = from_hdr->table;
  if (table_is_assimilated(from_hdr)) {
    return retval;
  }
  j = hash & mask;
  i = j;

  while (true) {
    current = table[i];

    if (current.key == 0 || key_equal(current.key, key)) {
      success = true;
    }

    /*
     * move the current entry to the new table if it has the key we want,
     * or if its key is non zero and not assimilated and we haven't moved our quota yet.
     */
    if (current.key == key ||
	(retval < count && current.key != 0 && !key_is_assimilated(current.key))) {

      /*
       * ***WARNING****
       *
       *  we cannot CAS the key, then move the key val pair, then increment the counters
       *  because we could be the last one, and go to sleep before we increment,
       *  and never wake before the new table fills.
       *
       *  an actual bug that was revealed by the logging.
       */
      if (current.val != TOMBSTONE){
	
	_lfht_move(h, current.key, jenkins_hash_ptr((void *)current.key), current.val);

      }

      akey = set_assimilated(current.key);

      if(cas_64((volatile uint64_t *)&(table[i].key), current.key, akey)){
	retval ++;
      }
    }

    if (success && retval >= count){
      break;
    }

    i++;
    i &= mask;
    if ( i == j ){
      break;
    }
  }
  
  if( false && LOGGING ){ logging_record(ASSIMILATE, false, h, key, 0, (uint64_t)from_hdr); }
  
  return retval;
}



void lfht_hdr_dump(FILE* fp, lfht_hdr_t *hdr, uint32_t index){
  uint32_t i, max, count, moved, tombstoned;
  lfht_entry_t*  table;
  lfht_entry_t  current;
  max = hdr->max;
  count = 0;
  moved = 0;
  tombstoned = 0;
  table = hdr->table;
  for(i = 0; i < max; i++){
    current = table[i];
    if(current.key != 0){
      count++;
      if(current.val == TOMBSTONE){ tombstoned++; }
      if( key_is_assimilated(current.key) ){ moved++; }
    }
  }
  fprintf(fp, "table[%"PRIu32"]@%p: reference_count = %"PRIu32" assimilated = %d max = %"PRIu32\
          " count =  %"PRIu32" threshold =  %"PRIu32" actual =  %"PRIu32\
          " moved =  %"PRIu32" tombstoned =  %"PRIu32"\n",
          index, hdr, hdr->reference_count, table_is_assimilated(hdr), hdr->max, hdr->count, hdr->threshold,
          count, moved, tombstoned);
}

void lfht_stats(FILE* fp, const char* name, lfht_t *ht){
  uint32_t index;
  lfht_hdr_t *hdr;
  index = 0;
  hdr = lfht_table_hdr(ht);
  fprintf(fp, "%s table state: %u\n", name,  lfht_state(ht));
  while(hdr != NULL){
    lfht_hdr_dump(fp, hdr, index);
    hdr = hdr->next;
    index++;
  }
}
