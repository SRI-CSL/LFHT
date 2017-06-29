/*
 * Copyright (C) 2017  SRI International
 *
 */
#ifndef __LFHT_LOGGING_H__
#define __LFHT_LOGGING_H__

#include <stdbool.h>
#include <stdint.h>


#include "lfht.h"

typedef enum logging_site { ADD = 'A', REMOVE = 'R', FIND = 'F', _ADD = 'a', _MOVE = 'v', _REMOVE = 'r', _FIND = 'f', MIGRATE = 'm', ASSIMILATE = 's'} logging_site_t;


bool logging_init();


bool logging_record(logging_site_t  site, bool entry, lfht_handle_t *h, uint64_t key, uint64_t val, uint64_t extra);


bool logging_shutdown();


#endif
