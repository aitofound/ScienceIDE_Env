
#include "pic.h"
#include "Exosphere_Neon.h"


        double NeonDesorption::GetSurfaceElementProductionRate(int spec,int SurfaceElement,void *SphereDataPointer) {
                double res=0.0,norm[3],sun[3],NeonSurfaceElementPopulation;

                NeonSurfaceElementPopulation=((cInternalSphericalData*)SphereDataPointer)->SurfaceElementPopulation[spec][SurfaceElement];
                if (NeonSurfaceElementPopulation<0.0) return 0.0;

                memcpy(sun,Exosphere::OrbitalMotion::SunDirection_IAU_OBJECT,3*sizeof(double));
                memcpy(norm,(((cInternalSphericalData*)SphereDataPointer)->SurfaceElementExternalNormal+SurfaceElement)->norm,3*sizeof(double));

                //dayside production of helium
                for (int idim=0;idim<DIM;idim++) res+=sun[idim]*norm[idim];
                      res*=(res>0.0) ? Ne_Flux_1AU*pow(_AU_/Exosphere::xObjectRadial,2.0) : 0.0;
                 return res;

        }

