

#ifndef _EXOSPHERE__NE_
#define _EXOSPHERE__NE_


#include "Exosphere_Helium.h"




      namespace NeonDesorption {
      using namespace HeliumDesorption;
        //typical solar wind conditions far from the planet

        //Killen-2019-icarus
        const static double Ne_Flux_1AU = 1.6E8;

        //the object for distribution of injection positino on the planet's surface
        double GetSurfaceElementProductionRate(int nElement,int *spec);

        inline double GetTotalProductionRate(int spec,int BoundaryElementType,void *SphereDataPointer) {
		double res=0.0;

		res=(spec==_NE_SPEC_) ? Ne_Flux_1AU*Pi*_MOON__RADIUS_*_MOON__RADIUS_ : 0.0;
		return res; 
       }

        double GetSurfaceElementProductionRate(int spec,int SurfaceElement,void *SphereDataPointer);

      }

#endif /* OBJECT_H_ */
