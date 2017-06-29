
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "utils.h"


int main(int argc, char* argv[]){		
  
  if(argc != 2){
    fprintf(stderr, "Usage: %s <key>\n", argv[0]);
    exit(1);
  }  else  {
    uint64_t key = atol(argv[1]);
    uint32_t hash = jenkins_hash_ptr((void *)key);
    fprintf(stdout, "jenkins_hash_ptr(%s) = %"PRIu32"\n", argv[1], hash);
     exit(0);
  }
}
