#ifndef _BC_H_
#define _BC_H_

#include <string>
#include <vector>

#include <AMReX_Geometry.H>
#include <AMReX_IntVect.H>
#include <AMReX_MFIter.H>
#include <AMReX_MultiFab.H>
#include <AMReX_PhysBCFunct.H>
#include <AMReX_iMultiFab.H>

//==========================================================
template <typename EnumT> struct BoxBC {
  amrex::IntVect lo, hi;

  BoxBC()
      : lo(static_cast<int>(EnumT::coupled)),
        hi(static_cast<int>(EnumT::coupled)) {}

  // side: 0 = lo, 1 = hi
  int face(const int d, const int side) const {
    return side == 0 ? lo[d] : hi[d];
  }

  void set(const int d, const int side, const EnumT type) {
    if (side == 0)
      lo[d] = static_cast<int>(type);
    else
      hi[d] = static_cast<int>(type);
  }
};

//==========================================================
namespace bc_detail {

enum class Legacy { none, silent, deprecated };

struct Entry {
  const char *name;
  int type;
  Legacy legacy;
};

inline const char *canonical_name(const std::vector<Entry> &table,
                                  const int type) {
  for (const Entry &e : table) {
    if (e.type == type && e.legacy == Legacy::none)
      return e.name;
  }
  return "?";
}

inline bool lookup(const std::string &str, const std::vector<Entry> &table,
                   int &mapped, std::string &valid) {
  valid.clear();

  for (const Entry &e : table) {
    if (e.legacy != Legacy::none)
      continue;
    if (!valid.empty())
      valid += ", ";
    valid += e.name;
  }

  for (const Entry &e : table) {
    if (str != e.name)
      continue;

    if (e.legacy == Legacy::none || e.legacy == Legacy::silent) {
      mapped = e.type;
      return true;
    }
    if (e.legacy == Legacy::deprecated)
      return false;
  }

  return false;
}

} // namespace bc_detail

//==========================================================
namespace ParticleBC {

enum Type {
  unset = -1,
  periodic = 0,
  coupled = 1,
  outflow = 2,
  vacuum = 3,
  reflect = 4,
  absorb = 5,
  inflow = 6
};

const std::vector<bc_detail::Entry> &table();

inline bool is_valid(const Type t) {
  for (const bc_detail::Entry &e : table()) {
    if (e.type == t && e.legacy == bc_detail::Legacy::none)
      return true;
  }
  return false;
}

inline const char *to_string(const Type t) {
  return bc_detail::canonical_name(table(), t);
}

inline Type parse(const std::string &str) {
  int mapped = unset;
  std::string valid;
  if (!bc_detail::lookup(str, table(), mapped, valid)) {
    amrex::Abort("Error: unrecognized particle boundary type '" + str +
                 "'. Accepted values: " + valid);
  }
  return static_cast<Type>(mapped);
}

} // namespace ParticleBC

//==========================================================
namespace FieldBC {

enum Type {
  unset = -1,
  periodic = 0,
  coupled = 1,
  outflow = 2,
  vacuum = 3,
  conducting = 4,
  absorb = 5,
  inflow = 6,
  fixed = 7,
  wave = 8
};

const std::vector<bc_detail::Entry> &table();

inline bool is_valid(const Type t) {
  for (const bc_detail::Entry &e : table()) {
    if (e.type == t && e.legacy == bc_detail::Legacy::none)
      return true;
  }
  return false;
}

inline const char *to_string(const Type t) {
  return bc_detail::canonical_name(table(), t);
}

inline Type parse(const std::string &str) {
  int mapped = unset;
  std::string valid;
  if (!bc_detail::lookup(str, table(), mapped, valid)) {
    amrex::Abort("Error: unrecognized field boundary type '" + str +
                 "'. Accepted values: " + valid);
  }
  return static_cast<Type>(mapped);
}

} // namespace FieldBC

#endif
