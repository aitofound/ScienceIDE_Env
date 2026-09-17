#ifndef _USERSOURCE_H_
#define _USERSOURCE_H_

#include "SourceInterface.h"

class UserSource : public SourceInterface {
public:
  UserSource(const FluidInterface& other, int id, std::string tag,
             FluidType typeIn, const DomainParameters& dp)
      : SourceInterface(other, id, tag, typeIn, dp) {
    info = "Empty user source class";
  }
};

#endif
