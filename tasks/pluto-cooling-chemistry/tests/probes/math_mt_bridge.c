/* Compile the separately vendored MT translation unit with private symbols so
 * it can coexist with PLUTO's embedded PRNG_MT implementation in math_random.c. */
#define init_genrand64 external_init_genrand64
#define genrand64_int64 external_genrand64_int64
#define genrand64_real1 external_genrand64_real1
#include "/task/code/pluto/Src/Math_Tools/math_rand_mt19937-64.c"
