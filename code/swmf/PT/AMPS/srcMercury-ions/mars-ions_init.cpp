
//$Id$
//functions for calculation of the time step and mesh resolution

/*
 * mars-ion__TimeStep_MeshResolution.cpp
 *
 *  Created on: May 26, 2015
 *      Author: vtenishe
 */


#include "pic.h"
#include "mars-ions.h"


//the mesh resolution at the lower boundary
double localSphericalSurfaceResolution(double *x) {
  double res;
  /*
  switch (_PIC_NIGHTLY_TEST_MODE_) {
  case _PIC_MODE_ON_:
    res=_RADIUS_(_TARGET_)/20.0;
    break;
  default:
    */
    res=_RADIUS_(_TARGET_)/2.0;

    //    printf("sph res:%e\n",res);
     // }

  return res;
}

//the mesh resulution within the domain
double localResolution(double *x) {
  //the new resolution fn
  double r = sqrt(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
  double rm = _RADIUS_(_TARGET_); // Mercury radius in meters
  double res;
  if (r<1*rm) {
   res = .5*rm;
   // printf("r<1, r=%e\n",r);
 } else if (r>=1*rm and r<2*rm){
   switch (_PIC_NIGHTLY_TEST_MODE_) {
   case _PIC_MODE_ON_:
     res=800.0e3;
     break;
   default:
     res = 80e3;
   }
  } else{
   res = 7*rm; //just use a random large number, for example 800km.
  }

 //the original resolution
 // double res=_RADIUS_(_TARGET_)/2.0;

 //always keep
 // printf("local res:%e\n",res);
 return res;
}


//the local time step
double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double CellSize=startNode->GetCharacteristicCellSize();
  double CharacteristicSpeed=1.0E5;

  switch (spec) {
  case _H_PLUS_SPEC_:
    CharacteristicSpeed=1.0E6;
    break;
  default:
    CharacteristicSpeed=1.0E5;
  }

  return 0.3*CellSize/CharacteristicSpeed;
}
