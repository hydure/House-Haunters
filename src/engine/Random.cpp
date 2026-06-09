/* --------------------------------------------------------------------------
 * Park-Miller multiplicative congruential pseudo-random number generator
 * with 256 independent streams. Originally part of a larger library of
 * random variates (rvgs.c by Steve Park & Dave Geyer, rev 10-28-98); the
 * unused distributions have been stripped -- this file now only exposes
 * Random(), PlantSeeds(), SelectStream(), and Equilikely().
 * --------------------------------------------------------------------------
 */

#include "engine/Random.hpp"

#include <time.h>

#define MODULUS    2147483647 /* DON'T CHANGE THIS VALUE                  */
#define MULTIPLIER 48271      /* DON'T CHANGE THIS VALUE                  */
#define STREAMS    256        /* # of streams, DON'T CHANGE THIS VALUE    */
#define A256       22925      /* jump multiplier, DON'T CHANGE THIS VALUE */
#define DEFAULT    123456789  /* initial seed, use 0 < DEFAULT < MODULUS  */

static long seed[STREAMS] = {DEFAULT};  /* current state of each stream   */
static int  stream        = 0;          /* stream index, 0 is the default */
static int  initialized   = 0;          /* test for stream initialization */

/* Internal helper: install a state value into the current stream.
 *   if x > 0 the value is used directly (mod MODULUS)
 *   if x < 0 the state is seeded from the system clock
 *   if x = 0 the state is left unchanged (interactive seeding removed)
 */
static void PutSeed(long x)
{
  if (x > 0)
    x = x % MODULUS;                       /* correct if x is too large  */
  if (x < 0)
    x = ((unsigned long) time((time_t *) NULL)) % MODULUS;
  if (x != 0)
    seed[stream] = x;
}


double Random(void)
/* ----------------------------------------------------------------
 * Random returns a pseudo-random real number uniformly distributed 
 * between 0.0 and 1.0. 
 * ----------------------------------------------------------------
 */
{
  const long Q = MODULUS / MULTIPLIER;
  const long R = MODULUS % MULTIPLIER;
        long t;

  t = MULTIPLIER * (seed[stream] % Q) - R * (seed[stream] / Q);
  if (t > 0) 
    seed[stream] = t;
  else 
    seed[stream] = t + MODULUS;
  return ((double) seed[stream] / MODULUS);
}


void PlantSeeds(long x)
/* ---------------------------------------------------------------------
 * Use this function to set the state of all the random number generator 
 * streams by "planting" a sequence of states (seeds), one per stream, 
 * with all states dictated by the state of the default stream. 
 * The sequence of planted states is separated one from the next by 
 * 8,367,782 calls to Random().
 * ---------------------------------------------------------------------
 */
{
  const long Q = MODULUS / A256;
  const long R = MODULUS % A256;
        int  j;
        int  s;

  initialized = 1;
  s = stream;                            /* remember the current stream */
  SelectStream(0);                       /* change to stream 0          */
  PutSeed(x);                            /* set seed[0]                 */
  stream = s;                            /* reset the current stream    */
  for (j = 1; j < STREAMS; j++) {
    long y = A256 * (seed[j - 1] % Q) - R * (seed[j - 1] / Q);
    if (y > 0)
      seed[j] = y;
    else
      seed[j] = y + MODULUS;
   }
}


void SelectStream(int index)
/* ------------------------------------------------------------------
 * Use this function to set the current random number generator
 * stream -- that stream from which the next random number will come.
 * ------------------------------------------------------------------
 */
{
  stream = ((unsigned int) index) % STREAMS;
  if ((initialized == 0) && (stream != 0))   /* protect against        */
    PlantSeeds(DEFAULT);                     /* un-initialized streams */
}


long Equilikely(long a, long b)
/* ===================================================================
 * Returns an equilikely distributed integer between a and b inclusive. 
 * NOTE: use a < b
 * ===================================================================
 */
{
  return (a + (long) ((b - a + 1) * Random()));
}
