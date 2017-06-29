/*
 * Copyright (C) 2017  SRI International
 *
 */
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>

#include "logging.h"


static int logfd = -1;

static char logfilename[] = "/tmp/lfht.log";


bool logging_init(){
  bool retval = false;
  if(logfd == -1){
    int fd = open(logfilename, O_WRONLY | O_TRUNC | O_CREAT | O_APPEND, 0600);
    if (fd >= 0) {
      logfd = fd;
      retval = true;
    }
  }
  return retval;
}

/*
 *   - site  the calling site (routine)
 *   - entry  true if we are entering, false if we are exiting
 *   - key    the key or zero if N/A
 *   - val    the value or 0 in N/A
 *
 *   bool lfht_add(       lfht_handle_t *h, uint64_t key, uint64_t val);
 *   bool lfht_remove(    lfht_handle_t *h, uint64_t key);
 *   bool lfht_find(      lfht_handle_t *h, uint64_t key, uint64_t *valp);
 *   bool _lfht_add(      lfht_handle_t *h, uint64_t key, uint32_t hash, uint64_t val, bool external);
 *   bool _lfht_find(     lfht_handle_t *h, uint64_t key, uint32_t hash, uint64_t *valp);
 *   bool _lfht_remove(   lfht_handle_t *h, uint64_t key, uint32_t hash);
 *   uint32_t assimilate( lfht_handle_t *h, lfht_hdr_t *from_hdr, uint64_t key, uint32_t hash,  uint32_t count);
 *   void _migrate_table( lfht_handle_t *h, uint64_t key, uint32_t hash);
 *
 *
 * typedef struct lfht_handle_s {
 *  pthread_t owner;
 *  lfht_t *table;
 *  lfht_hdr_t *table_hdr;
 *  lfht_hdr_t *last;  //the last table header in the linked list whose reference count we have incremented.
 *} lfht_handle_t;
 *
 */
bool logging_record(logging_site_t  site, bool entry, lfht_handle_t *h, uint64_t key, uint64_t val, uint64_t extra){
  char buffer[1024];
  if(logfd != -1){
    lfht_state_t state = lfht_state(h->table);
    uint32_t threshold = h->table_hdr->threshold;
    lfht_hdr_t *table_hdr = h->table_hdr;
    lfht_hdr_t *next =  h->table_hdr->next;
    uint32_t count = atomic_load(&table_hdr->count);

    snprintf(buffer, 1024, "%c%c %p %d %p, %p, %d %d %"PRIu64"\n", entry ? '>' : '<',
	     site, (void*) h->owner, state, table_hdr, next, threshold, count, extra);
    write(logfd, buffer, strlen(buffer));
    return true;
  }
  return false;
}

bool logging_shutdown(){
  bool retval = false;
  if(logfd >= 0){
    int fd = logfd;
    logfd = -1;
    retval = (close(fd) == 0);
  }
  return retval;
}
