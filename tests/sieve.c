/* $Id: Sieve.c $ */

/* Sieve of Eratosthenes algorithm 
 * Ref: http://www.math.utah.edu/~alfeld/Eratosthenes.html
 * This implementation by David Ireland <www.di-mgt.com.au>
 * $Date: 2012/12/14 11:02Z $
 * $Author: dai $
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>


/* Usage: Sieve n
 * where n = largest integer to test for prime
 * (default = 1000)
 */

/* For first 10,000 primes use 
      Sieve 104729
   because 104729 is the 10,000th prime
   Ref: http://www.utm.edu/research/primes
*/

int main(int argc, char *argv[])
{
  int i, m, k;
  int klimit, nlimit;
  int *mark;

  if (argc > 1)
    nlimit = atoi(argv[1]);
  else
    nlimit = 1000;

  mark = calloc(nlimit, sizeof(int));

  /* Calculate limit for k */
  klimit = (int)sqrt((double)nlimit) + 1;

  /* Mark the composites */
  /* Special case */
  mark[1] = -1;

  /* Set k=1. Loop until k >= sqrt(n) */
  for (k = 1; k <= klimit; k = m)
  {
    /* Find first non-composite in list > k */
    for (m = k + 1; m < nlimit; m++)
      if (!mark[m])
        break;

    /* Mark the numbers 2m, 3m, 4m, ... */
    for (i = m * 2; i < nlimit; i += m)
      mark[i] = -1;
  }

  /* Now display results - all unmarked numbers are prime */
  for (k = 0, i = 1; i < nlimit; i++)
  {
    if (!mark[i])
    {
      if (k % 10 == 0)
        printf("%d: ", k);
      printf("%d ", i);
      k++;
      if (k % 10 == 0 && k > 0)
        printf("\n");
    }
  }
  printf("\n");

  free(mark);

  return 0;
}

