#ifndef _BITARRAY_H_
#define _BITARRAY_H_

#include <cstdio>
#include <cstdlib>
#include <vector>

class BitArray {

private:
  std::vector<int> data;
  int nInt = 0;
  int nBit = 0;

public:
  BitArray() = default;

  BitArray(int nBitIn) { init(nBitIn); }

  int size_int() const { return nInt; }
  int size_bit() const { return nBit; }

  //====================================================
  void clear() {
    data.clear();
    nInt = 0;
    nBit = 0;
  }

  //====================================================
  void init(int nBitIn) {
    nBit = nBitIn;
    const int intSize = sizeof(int) * 8;
    nInt = (nBit + intSize - 1) / intSize;
    data.assign(nInt, 0);
  }

  //====================================================
  void set(int i, int val) {
    if (i < 0 || i >= nBit)
      std::abort();

    const int intSize = sizeof(int) * 8;

    const int iInt = floor(i / intSize);

    const int iBit = i % intSize;

    int& number = data[iInt];

    if (val == 1) {
      number |= 1 << iBit;
    } else if (val == 0) {
      number &= ~(1 << iBit);
    } else {
      std::abort();
    }
  }

  //====================================================
  int get(int i) const {
    if (i < 0 || i >= nBit)
      std::abort();

    const int intSize = sizeof(int) * 8;

    const int iInt = floor(i / intSize);

    const int iBit = i % intSize;

    int number = data[iInt];

    int val = (number >> iBit) & 1;

    return val;
  }

  //====================================================
  // Return an integer pointer.
  int* get() { return data.data(); }

  void print() const {
    for (int i = 0; i < nBit; ++i) {
      printf("i = %i, bit = %i \n", i, get(i));
    }
  }
};

#endif
