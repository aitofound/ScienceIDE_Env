#ifndef _DOMAINS_H_
#define _DOMAINS_H_

#include <memory>

#include "AMReX_Vector.H"
#include "Domain.h"

class Domains {
private:
  amrex::Vector<std::unique_ptr<Domain> > domainVector;
  int iSelected = 0;

public:
  Domains() = default;

  void add_new_domain() { domainVector.push_back(std::make_unique<Domain>()); }

  void clear() { domainVector.clear(); }

  Domain& operator()(int i) {
    if (i < 0 || i >= size())
      amrex::Abort("Error: index out of range for class Domains!");

    return *domainVector[i];
  };

  int size() const { return static_cast<int>(domainVector.size()); }

  void select(int i) { iSelected = i; }

  int selected() const { return iSelected; }
};

#endif
