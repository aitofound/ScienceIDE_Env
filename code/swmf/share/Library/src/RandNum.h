#ifndef RANDNUM_H
#define RANDNUM_H

#include <cstdint>

#define IA 16807
#define IM 2147483647
#define AM (1.0 / IM)
#define IQ 127773
#define invIQ (1.0 / IQ)
#define IR 2836
#define MASK 123459876

class RandNum {
private:
  // This algorithm requires the seed occupies 4 bytes. It can be negative.
  int32_t idum;

public:
  RandNum() { idum = 1; }
  ~RandNum() {}
  void set_seed(long in) {
    // The input seed 'in' can larger than 2^31-1, it will be truncated and
    // asigned to idum.
    idum = in;

    if ((*this)() == 0) {
      // Ensure operator() does NOT always generate 0
      idum = in % MASK;
    } else {
      idum = in;
    }
  }

  // Overloaded the () operator. Then argument list is empty
  double operator()() {

    // "minimal" random number generator of Park and Miller. Taken from
    // "numerical recipes in C"
    // Return a uniform random deviate between 0.0 and 1.0. Set or reset idum
    // to any integer value (except the unlikely value MASK) it initialize
    // the sequence; idum must not be alterd between calls or successive
    // deviates in a sequence.

    int32_t k;
    double ans;

    idum ^= MASK;
    k = (idum)*invIQ;
    idum = IA * (idum - k * IQ) - IR * k;
    if (idum < 0)
      idum += IM;
    ans = AM * idum;
    idum ^= MASK;
    return ans;
  }
};

// cleaning
#undef IA
#undef IM
#undef AM
#undef IQ
#undef IR
#undef MASK

#endif
