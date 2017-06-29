#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>

#include "lfht.h"


/*
  testing add and find ans remove!

  each thread will work on an independent interval (of keys)

 */

uint32_t max = 1024;

#define MAX_THREADS 128

static lfht_t tbl;

typedef struct targs {
  int id;
  int count;
  int successes;
  int start;
} targs_t;


void* thread_main(void* targ){
  targs_t* targsp = (targs_t*)targ;
  int i;
  lfht_handle_t handle;
  uint32_t max = targsp->start + targsp->count;
  uint64_t val;

  init_handle(&handle, &tbl);

  for(i = targsp->start; i < max; i++){
    if( lfht_add(&handle, i * KEY_ALIGNMENT, i) && lfht_add(&handle, i * KEY_ALIGNMENT, 2*i) ){ // 16 is for alignment purposes!
      targsp->successes ++;
    } else {
      fprintf(stderr, "Thread %d adding  %d failed\n", targsp->id, i);
    }
  }

  if(targsp->successes == targsp->count){
    
    targsp->successes = 0;
    
    for(i = targsp->start; i < max; i++){
      if( lfht_remove(&handle, i * KEY_ALIGNMENT) ){
	targsp->successes ++;
      } else {
	fprintf(stderr, "Thread %d removal  of %d failed\n", targsp->id, i);
	
	if( lfht_remove(&handle, i * KEY_ALIGNMENT) ){
	  targsp->successes ++;
	  fprintf(stderr, "On the second attempt thread %d removal of %d succeeded\n", targsp->id, i);
	} else {
	  fprintf(stderr, "Second attempt of thread %d removal of %d ALSO failed\n", targsp->id, i);
	}
      }
    }
  }
  
  targsp->successes = 0;
 
  for(i = targsp->start; i < max; i++){
    if( lfht_add(&handle, i * KEY_ALIGNMENT, 3*i) ){ // 16 is for alignment purposes!
      targsp->successes ++;
    } else {
      fprintf(stderr, "Thread %d adding  %d failed\n", targsp->id, i);
    }
  }

  if(targsp->successes == targsp->count){
    
    targsp->successes = 0;
    
    for(i = targsp->start; i < max; i++){
      if( lfht_find(&handle, i * KEY_ALIGNMENT, &val)  && val == 3 * i ){
	targsp->successes ++;
      } else {
	fprintf(stderr, "Thread %d finding  %d failed\n", targsp->id, i);
	if( lfht_find(&handle, i * KEY_ALIGNMENT, &val)  && val == 3 * i ){
	  targsp->successes ++;
	  fprintf(stderr, "On the second attempt thread %d found  %d\n", targsp->id, i);
	} else {
	  fprintf(stderr, "Second attempt of thread %d finding  %d ALSO failed\n", targsp->id, i);
	}

	
      }
    }
  }
  
  if(targsp->successes == targsp->count){
    
    targsp->successes = 0;
    
    for(i = targsp->start; i < max; i++){
      if( lfht_remove(&handle, i * KEY_ALIGNMENT) ){
	targsp->successes ++;
      } else {
	fprintf(stderr, "Thread %d removal  of %d failed\n", targsp->id, i);

	if( lfht_remove(&handle, i * KEY_ALIGNMENT) ){
	  targsp->successes ++;
	  fprintf(stderr, "On the second attempt thread %d removal of %d succeeded\n", targsp->id, i);
	} else {
	  fprintf(stderr, "Second attempt of thread %d removal of %d ALSO failed\n", targsp->id, i);
	}
      }
    }
  }
  
  
  delete_handle(&handle);
  
  pthread_exit(NULL);
}


int main(int argc, char* argv[]){
  int nthreads;
  int rc;
  int i;
  pthread_t threads[MAX_THREADS];
  targs_t targs[MAX_THREADS];
  void* status;
  int total = 0;
  bool success;

  if (argc != 3) {
    fprintf(stdout, "Usage: %s <nthreads> <count in K>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  nthreads = atoi(argv[1]);

  uint32_t count = 1024 * atoi(argv[2]);

  
  if((nthreads == 0) ||  (nthreads >= MAX_THREADS)){
    fprintf(stdout, "Usage: %s <nthreads>\n", argv[0]);
    fprintf(stdout, "\t(nthreads > 0) and (nthreads < %d)\n", MAX_THREADS);
    exit(EXIT_FAILURE);
  }


  assert( (nthreads * count * KEY_ALIGNMENT) + 1 < UINT32_MAX );
  if((nthreads * count * KEY_ALIGNMENT) + 1 >= UINT32_MAX){
    fprintf(stdout, "nthreads * count * KEY_ALIGNMENT) + 1 >= UINT32_MAX\n");
    exit(EXIT_FAILURE);
  }
  
  success = init_lfht(&tbl, max);

  if( !success ) exit(EXIT_FAILURE);

  for(i = 0; i < nthreads; i++){
    targs_t *targsp = &targs[i];
    targsp->id = i;
    targsp->count = count;
    targsp->successes = 0;
    targsp->start = (i * count) + 1;
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

  lfht_stats(stderr, argv[0], &tbl);

  success = delete_lfht(&tbl);

  if( !success ) exit(EXIT_FAILURE);

  if( total != count * nthreads){
    fprintf(stderr, "%d successes out of %d attempts\n", total, count * nthreads);
    exit(EXIT_FAILURE);
  }
  
  fprintf(stdout, "[SUCCESS]\n");

  exit(EXIT_SUCCESS);
}
