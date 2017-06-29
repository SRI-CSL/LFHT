#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>
#include <unistd.h>


#include "lfht.h"

static lfht_t tbl;

typedef struct targs {
  uint32_t id;
  uint32_t val;
  uint32_t count;
  uint32_t successes;
} targs_t;


static uint32_t nthreads = 0;
static uint32_t waiting_count = 0;
static pthread_mutex_t waiting_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t waiting_cond = PTHREAD_COND_INITIALIZER;



void* thread_main(void* targ){
  targs_t* targsp = (targs_t*)targ;
  int i;
  lfht_handle_t handle;

  
  init_handle(&handle, &tbl);


  //here we want to wait until all the threads are at this point
  //i.e. we want a barrier.

  if(targsp->id == 1){
    fprintf(stderr, "Waiting at the gate\n");
  }

  //<barrier>
  pthread_mutex_lock(&waiting_lock);
  waiting_count++;
  fprintf(stderr, "Waiting count = %d\n", waiting_count);
  while(waiting_count != nthreads){
    pthread_cond_wait(&waiting_cond, &waiting_lock);
  }
  pthread_cond_broadcast(&waiting_cond);
  pthread_mutex_unlock(&waiting_lock);
  //</barrier>

  if(targsp->id == 1){
    fprintf(stderr, "Left the gate\n");
  }
  
  
  for(i = 1; i <= targsp->count; i++){
    if( lfht_add(&handle,  i * KEY_ALIGNMENT, i) ){ 
      targsp->successes ++;
    }
  }
  
  delete_handle(&handle);

  pthread_exit(NULL);
}


int main(int argc, char* argv[]){
  uint32_t i, total;
  int rc;
  pthread_t *threads;
  targs_t *targs;
  void* status;
  bool success;

  total = 0;

  if (argc != 3) {
    fprintf(stdout, "Usage: %s <nthreads> <count in K>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  nthreads = atoi(argv[1]);

  uint32_t count = 1024 * atoi(argv[2]);

  
  if(nthreads == 0){
    fprintf(stdout, "Usage: %s <nthreads>\n", argv[0]);
    fprintf(stdout, "\tnthreads > 0\n");
    exit(EXIT_FAILURE);
  }


  threads = calloc(nthreads, sizeof(pthread_t));

  targs = calloc(nthreads, sizeof(targs_t));

  if( !targs || !threads ){
    fprintf(stdout, "\tcallocing failed\n");
    exit(EXIT_FAILURE);
  }

  
  success = init_lfht(&tbl, 1024);

  if( !success ) exit(EXIT_FAILURE);

  for(i = 0; i < nthreads; i++){
    targs_t *targsp = &targs[i];
    targsp->id = i;
    targsp->val = 1;
    targsp->count = count;
    targsp->successes = 0;
  }

   for( i = 0; i < nthreads; i++){

     rc = pthread_create(&threads[i], NULL, thread_main, (void *)&targs[i]);

     if (rc){
       fprintf(stderr, "return code from pthread_create() is %d\n", rc);
       exit(EXIT_FAILURE);
     }
     
   }
  
  for( i = 0; i < nthreads; i++){

    rc = pthread_join(threads[i], &status);
    if (rc){
      fprintf(stderr, "return code from pthread_join() is %d\n", rc);
      exit(EXIT_FAILURE);
    }
  }

  
  
  for(i = 0; i < nthreads; i++){
    total += targs[i].successes;
    //fprintf(stdout, "thread %d with %d successes\n", targs[i].id, targs[i].successes);
  }

  lfht_stats(stderr, "static", &tbl);

  success = delete_lfht(&tbl);

  if( !success ) exit(EXIT_FAILURE);

  free(threads);
  free(targs);

  if( total != count * nthreads){
    fprintf(stderr, "%d successes out of %d attempts\n", total, count * nthreads);
    exit(EXIT_FAILURE);
  }
  
  fprintf(stdout, "[SUCCESS]\n");

  

  exit(EXIT_SUCCESS);
}
