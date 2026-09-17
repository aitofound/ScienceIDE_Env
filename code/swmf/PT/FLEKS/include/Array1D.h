#ifndef _Arr1D_H_
#define _Arr1D_H_

#include <iostream>

#include <AMReX_REAL.H>

template <class T, const int n> class Arr1D {
public:
  Arr1D(const T& b = T(0)) {
    for (int i = 0; i < n; ++i)
      data[i] = b;
  }

  Arr1D(const Arr1D<T, n>& b) {
    for (int i = 0; i < n; ++i)
      data[i] = b[i];
  }

  T& operator[](const int i) { return data[i]; }

  const T& operator[](const int i) const { return data[i]; }

  Arr1D<T, n>& operator=(const Arr1D<T, n>& b) {
    for (int i = 0; i < n; ++i)
      data[i] = b[i];
    return *this;
  }

  Arr1D<T, n>& operator=(const T& b) {
    for (int i = 0; i < n; ++i)
      data[i] = b;
    return *this;
  }

  Arr1D<T, n>& operator+=(const Arr1D<T, n>& b) {
    for (int i = 0; i < n; ++i)
      data[i] += b[i];
    return *this;
  }

  Arr1D<T, n>& operator*=(const Arr1D<T, n>& b) {
    for (int i = 0; i < n; ++i)
      data[i] *= b[i];
    return *this;
  }

  Arr1D<T, n>& operator<<(const Arr1D<T, n>& b) {
    for (int i = 0; i < n; ++i)
      data[i] += b[i];
    return *this;
  }

private:
  T data[n];
};

template <class T, const int n>
inline Arr1D<T, n> operator+(Arr1D<T, n> a, const Arr1D<T, n>& b) {
  for (int i = 0; i < n; ++i)
    a[i] += b[i];
  return a;
}

// It looks not right. --Yuxi  $$ I think the new version is right. --Talha
template <class T, const int n>
inline Arr1D<T, n> operator-(Arr1D<T, n> a, const Arr1D<T, n>& b) {
  for (int i = 0; i < n; ++i)
    a[i] -= b[i];
  return a;
}

template <class T, const int n>
inline Arr1D<T, n> operator/(Arr1D<T, n> a, const Arr1D<T, n>& b) {
  for (int i = 0; i < n; ++i)
    a[i] /= b[i];
  return a;
}

template <class T, const int n>
inline Arr1D<T, n> operator*(Arr1D<T, n> a, const Arr1D<T, n>& b) {
  for (int i = 0; i < n; ++i)
    a[i] *= b[i];
  return a;
}

template <class T, const int n>
inline Arr1D<T, n> operator*(Arr1D<T, n> a, const double& b) {
  for (int i = 0; i < n; ++i)
    a[i] *= b;
  return a;
}

template <class T, const int n>
inline Arr1D<T, n> operator*(const double& b, Arr1D<T, n> a) {
  for (int i = 0; i < n; ++i)
    a[i] *= b;
  return a;
}

template <class T, const int n>
std::ostream& operator<<(std::ostream& out, const Arr1D<T, n>& b) {
  for (int i = 0; i < n; ++i)
    out << " i = " << i << " data[i] = " << b[i] << "\t";
  out << std::endl;
  return out;
}

using RealMM = Arr1D<amrex::Real, 243>;
using RealCMM = Arr1D<amrex::Real, 27>;

#endif
