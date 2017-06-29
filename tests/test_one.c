#include <stdlib.h>
#include <stdio.h>

#include "lfht.h"


uint32_t max = KEY_ALIGNMENT * 4096;

uint32_t count = 8 * 4096;


static lfht_t ht;


int main(int argc, char* argv[]){
  uint32_t i;
  bool success;
  uint64_t val;
  lfht_handle_t handle;
  
  success = init_lfht(&ht, max);

  if( !success ) exit(EXIT_FAILURE);

  init_handle(&handle, &ht);

  for(i = 1; i <= count; i++){
    if( ! lfht_add(&handle, i * KEY_ALIGNMENT, i) ){ 
      fprintf(stderr, "%s insert failed for i = %d, max = %d\n", argv[0], i, max);
      exit(EXIT_FAILURE);
    }
  }

  for(i = 1; i <= count; i++){
    if( ! lfht_find(&handle, i * KEY_ALIGNMENT, &val) ){ 
      fprintf(stderr, "%s find failed for i = %d, max = %d\n", argv[0], i, max);
      exit(EXIT_FAILURE);
    }
    if(val != i){
      fprintf(stderr, "%s find integrity failed for i = %d, max = %d\n", argv[0], i, max);
      exit(EXIT_FAILURE);
    }
  }

  delete_handle(&handle);

  lfht_stats(stderr, argv[0], &ht);

  success = delete_lfht(&ht);

  if( !success ) exit(EXIT_FAILURE);

  fprintf(stdout, "[SUCCESS]\n");

  exit(EXIT_SUCCESS);

}


