/*
 * PhotolyticReactions.h
 *
 *  Created on: Feb 5, 2015
 *      Author: vtenishe
 */

//$Id$
//the data base of the photolytic reaction parameters for different species

#ifndef _PHOTOLYTICREACTIONS_
#define _PHOTOLYTICREACTIONS_

#include "pic.h"
#include "PhotolyticReactions.dfn"


/*-----------------------------  !!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!! ---------------------------------------------
 * In the list of the reaction products it is assumed that the products are ordered by deacreasing of their mass (first heavy that light)
 */

namespace PhotolyticReactions {

  //the general functions and variables used by particular model
  namespace Huebner1992ASS {

    //generate the channel of the reaction, determin its products and velocities
    void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* ReturnReactionProductTable,double *ReturnReactionProductVelocityTable,
        double *ReactionRateTable, int nReactionChannels,int* TotalReactionProductTable,int *ReactionChannelProductNumber,double *ReactionProductMassTable,int nMaxReactionProducts,
        double TotalReactionRate,double *ExcessEnergyTable);

    void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* ReturnReactionProductTable,double *ReturnReactionProductVelocityTable,
    double *ReactionRateTable, int nReactionChannels,int* TotalReactionProductTable,int *ReactionChannelProductNumber,double *ReactionProductMassTable,int nMaxReactionProducts,
								    double TotalReactionRate,double *ExcessEnergyTable);


    //calculate the yield fpr a particular specie
    double GetSpeciesReactionYield(int spec,double *ReactionRateTable, int nReactionChannels, int* TotalReactionProductTable, int nMaxReactionProducts);
    
    void GetProductVelocity_2(double excessEnergy,double * productMassArray, double * productVelocityTable);
  
    void GetProductVelocity_3(double excessEnergy,double * productMassArray, double * productVelocityTable);

  }


  //reaction rates for H2O
  namespace H2O {
    namespace Huebner1992ASS {
      const int nMaxReactionProducts=3;
      const int nReactionChannels=7;
      const double TableHeliocentricDistance=1.0*_AU_;

      extern int ReactionProducts[nReactionChannels][nMaxReactionProducts],ReactionChannelProductNumber[nReactionChannels];
      extern double EmissionWaveLength[nReactionChannels],ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];

      //the table of the rates for each channel
      extern double ReactionRateTable_QuietSun[nReactionChannels],ReactionRateTable_ActiveSun[nReactionChannels];
      extern double ExcessEnergyTable_QuietSun[nReactionChannels],ExcessEnergyTable_ActiveSun[nReactionChannels];     
      extern double *ReactionRateTable,*ExcessEnergyTable;
      extern double TotalReactionRate;

      //the buffer to return the list of the reaction products
      extern int ReturnReactionProductList[nMaxReactionProducts];
      extern double ReturnReactionProductVelocity[3*nMaxReactionProducts];


      void Init();

      inline double GetTotalReactionRate(double HeliocentricDistance) {
        return TotalReactionRate*pow(TableHeliocentricDistance/HeliocentricDistance,2);
      }

      inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);
	
	//printf("ReactionChannel:%d, nReactionProducts:%d\n", ReactionChannel,nReactionProducts);
	
	/*
	if (ReactionChannel==0){
	printf("H2O ReactionChannel:%d,nReactionProducts:%d\n", ReactionChannel,nReactionProducts);
	double mx=0,my=0,mz=0, energy=0;
	for (int i=0; i<nReactionProducts; i++){
	  double vx,vy,vz,mass;
	  mass = ReactionProductMassTable[ReactionChannel][i];
	  vx = ReturnReactionProductVelocity[3*i+0];
	  vy = ReturnReactionProductVelocity[3*i+1];
	  vz = ReturnReactionProductVelocity[3*i+2];
	  mx += mass*vx;
	  my += mass*vy;
	  mz += mass*vz;
	  energy +=0.5*mass*(vx*vx+vy*vy+vz*vz);
	  printf("iProduct:%d,mass:%e,v:%e,%e,%e\n",i,mass,vx,vy,vz);
	}
	
	printf("reactionChannel:%d mx:%e,%e,%e, energy:%e\n", ReactionChannel, mx,my,mz, energy/eV2J);

	}
	*/
	  
        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }

      
      inline void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateGivenProducts(prodSpec, ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);
	
	  
        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }


      inline double GetSpeciesReactionYield(int spec) {
        return PhotolyticReactions::Huebner1992ASS::GetSpeciesReactionYield(spec,ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],nMaxReactionProducts);
      }


    }

    inline double GetTotalReactionRate(double HeliocentricDistance) {
      return Huebner1992ASS::GetTotalReactionRate(HeliocentricDistance);
    }

    inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }

    inline void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateGivenProducts(prodSpec, ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }


    inline double GetSpeciesReactionYield(int spec) {
      return Huebner1992ASS::GetSpeciesReactionYield(spec);
    }

  }

  //reaction rates for O2
  namespace O2 {
    namespace Huebner1992ASS {
      const int nMaxReactionProducts=3;
      const int nReactionChannels=5;
      const double TableHeliocentricDistance=1.0*_AU_;

      extern int ReactionProducts[nReactionChannels][nMaxReactionProducts],ReactionChannelProductNumber[nReactionChannels];
      extern double EmissionWaveLength[nReactionChannels],ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];

      //the table of the rates for each channel
      extern double ReactionRateTable_QuietSun[nReactionChannels],ReactionRateTable_ActiveSun[nReactionChannels];
      extern double ExcessEnergyTable_QuietSun[nReactionChannels],ExcessEnergyTable_ActiveSun[nReactionChannels];
      extern double *ReactionRateTable,*ExcessEnergyTable;
      extern double TotalReactionRate;

      //the buffer to return the list of the reaction products
      extern int ReturnReactionProductList[nMaxReactionProducts];
      extern double ReturnReactionProductVelocity[3*nMaxReactionProducts];

      void Init();

      inline double GetTotalReactionRate(double HeliocentricDistance) {
        return TotalReactionRate*pow(TableHeliocentricDistance/HeliocentricDistance,2);
      }

      inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);

        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }

      inline void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateGivenProducts(prodSpec, ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);

        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }


      inline double GetSpeciesReactionYield(int spec) {
        return PhotolyticReactions::Huebner1992ASS::GetSpeciesReactionYield(spec,ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],nMaxReactionProducts);
      }
    }

    inline double GetTotalReactionRate(double HeliocentricDistance) {
      return Huebner1992ASS::GetTotalReactionRate(HeliocentricDistance);
    }

    inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }

    inline void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateGivenProducts(prodSpec, ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }


    inline double GetSpeciesReactionYield(int spec) {
      return Huebner1992ASS::GetSpeciesReactionYield(spec);
    }
  }


  namespace H {
    namespace Huebner1992ASS {
      const int nMaxReactionProducts=2;
      const int nReactionChannels=1;
      const double TableHeliocentricDistance=1.0*_AU_;

      extern int ReactionProducts[nReactionChannels][nMaxReactionProducts],ReactionChannelProductNumber[nReactionChannels];
      extern double EmissionWaveLength[nReactionChannels],ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];

      //the table of the rates for each channel
      extern double ReactionRateTable_QuietSun[nReactionChannels],ReactionRateTable_ActiveSun[nReactionChannels];
      extern double ExcessEnergyTable_QuietSun[nReactionChannels],ExcessEnergyTable_ActiveSun[nReactionChannels];
      extern double *ReactionRateTable,*ExcessEnergyTable;
      extern double TotalReactionRate;

      //the buffer to return the list of the reaction products
      extern int ReturnReactionProductList[nMaxReactionProducts];
      extern double ReturnReactionProductVelocity[3*nMaxReactionProducts];

      void Init();

      inline double GetTotalReactionRate(double HeliocentricDistance) {
        return TotalReactionRate*pow(TableHeliocentricDistance/HeliocentricDistance,2);
      }

      inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);

        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }

      inline void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateGivenProducts(prodSpec, ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);

        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }


      inline double GetSpeciesReactionYield(int spec) {
        return PhotolyticReactions::Huebner1992ASS::GetSpeciesReactionYield(spec,ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],nMaxReactionProducts);
      }
    }

    inline double GetTotalReactionRate(double HeliocentricDistance) {
      return Huebner1992ASS::GetTotalReactionRate(HeliocentricDistance);
    }

    inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }
    
    inline void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateGivenProducts(prodSpec, ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }
    

    inline double GetSpeciesReactionYield(int spec) {
      return Huebner1992ASS::GetSpeciesReactionYield(spec);
    }
  }

  namespace H2 {
    namespace Huebner1992ASS {
      const int nMaxReactionProducts=3;
      const int nReactionChannels=4;
      const double TableHeliocentricDistance=1.0*_AU_;

      extern int ReactionProducts[nReactionChannels][nMaxReactionProducts],ReactionChannelProductNumber[nReactionChannels];
      extern double EmissionWaveLength[nReactionChannels],ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];

      //the table of the rates for each channel
      extern double ReactionRateTable_QuietSun[nReactionChannels],ReactionRateTable_ActiveSun[nReactionChannels];
      extern double ExcessEnergyTable_QuietSun[nReactionChannels],ExcessEnergyTable_ActiveSun[nReactionChannels];
      extern double *ReactionRateTable,*ExcessEnergyTable;
      extern double TotalReactionRate;

      //the buffer to return the list of the reaction products
      extern int ReturnReactionProductList[nMaxReactionProducts];
      extern double ReturnReactionProductVelocity[3*nMaxReactionProducts];

      void Init();

      inline double GetTotalReactionRate(double HeliocentricDistance) {
        return TotalReactionRate*pow(TableHeliocentricDistance/HeliocentricDistance,2);
      }

      inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);

        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }


      inline void GenerateGivenProducts(int prodSpec,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateGivenProducts(prodSpec,ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);

        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }


      inline double GetSpeciesReactionYield(int spec) {
        return PhotolyticReactions::Huebner1992ASS::GetSpeciesReactionYield(spec,ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],nMaxReactionProducts);
      }
    }

    inline double GetTotalReactionRate(double HeliocentricDistance) {
      return Huebner1992ASS::GetTotalReactionRate(HeliocentricDistance);
    }

    inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }

    inline void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateGivenProducts(prodSpec, ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }


    inline double GetSpeciesReactionYield(int spec) {
      return Huebner1992ASS::GetSpeciesReactionYield(spec);
    }
  }

  namespace OH {
    namespace Huebner1992ASS {
      const int nMaxReactionProducts=2;
      const int nReactionChannels=4;
      const double TableHeliocentricDistance=1.0*_AU_;

      extern int ReactionProducts[nReactionChannels][nMaxReactionProducts],ReactionChannelProductNumber[nReactionChannels];
      extern double EmissionWaveLength[nReactionChannels],ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];

      //the table of the rates for each channel
      extern double ReactionRateTable_QuietSun[nReactionChannels],ReactionRateTable_ActiveSun[nReactionChannels];
      extern double ExcessEnergyTable_QuietSun[nReactionChannels],ExcessEnergyTable_ActiveSun[nReactionChannels];
      extern double *ReactionRateTable,*ExcessEnergyTable;
      extern double TotalReactionRate;

      //the buffer to return the list of the reaction products
      extern int ReturnReactionProductList[nMaxReactionProducts];
      extern double ReturnReactionProductVelocity[3*nMaxReactionProducts];

      void Init();

      inline double GetTotalReactionRate(double HeliocentricDistance) {
        return TotalReactionRate*pow(TableHeliocentricDistance/HeliocentricDistance,2);
      }

      inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);

        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }

      inline void GenerateGivenProducts(int prodSpec,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateGivenProducts(prodSpec,ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
								   ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);
	
        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }


      inline double GetSpeciesReactionYield(int spec) {
        return PhotolyticReactions::Huebner1992ASS::GetSpeciesReactionYield(spec,ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],nMaxReactionProducts);
      }
    }

    inline double GetTotalReactionRate(double HeliocentricDistance) {
      return Huebner1992ASS::GetTotalReactionRate(HeliocentricDistance);
    }

    inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }
    
    inline void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateGivenProducts(prodSpec, ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }


    inline double GetSpeciesReactionYield(int spec) {
      return Huebner1992ASS::GetSpeciesReactionYield(spec);
    }
  }

  namespace O {
    namespace Huebner1992ASS {
      const int nMaxReactionProducts=2;
      const int nReactionChannels=1;
      const double TableHeliocentricDistance=1.0*_AU_;

      extern int ReactionProducts[nReactionChannels][nMaxReactionProducts],ReactionChannelProductNumber[nReactionChannels];
      extern double EmissionWaveLength[nReactionChannels],ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];

      //the table of the rates for each channel
      extern double ReactionRateTable_QuietSun[nReactionChannels],ReactionRateTable_ActiveSun[nReactionChannels];
      extern double ExcessEnergyTable_QuietSun[nReactionChannels],ExcessEnergyTable_ActiveSun[nReactionChannels];
      extern double *ReactionRateTable,*ExcessEnergyTable;
      extern double TotalReactionRate;

      //the buffer to return the list of the reaction products
      extern int ReturnReactionProductList[nMaxReactionProducts];
      extern double ReturnReactionProductVelocity[3*nMaxReactionProducts];

      void Init();

      inline double GetTotalReactionRate(double HeliocentricDistance) {
        return TotalReactionRate*pow(TableHeliocentricDistance/HeliocentricDistance,2);
      }

      inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
            ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);

        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }

      inline void GenerateGivenProducts(int prodSpec,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        PhotolyticReactions::Huebner1992ASS::GenerateGivenProducts(prodSpec,ReactionChannel,nReactionProducts,ReturnReactionProductList,ReturnReactionProductVelocity,
								   ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],ReactionChannelProductNumber,&ReactionProductMassTable[0][0],nMaxReactionProducts,TotalReactionRate,ExcessEnergyTable);
	
        ReactionProductTable=ReturnReactionProductList;
        ReactionProductVelocityTable=ReturnReactionProductVelocity;
      }
      

      inline double GetSpeciesReactionYield(int spec) {
        return PhotolyticReactions::Huebner1992ASS::GetSpeciesReactionYield(spec,ReactionRateTable,nReactionChannels,&ReactionProducts[0][0],nMaxReactionProducts);
      }
    }

    inline double GetTotalReactionRate(double HeliocentricDistance) {
      return Huebner1992ASS::GetTotalReactionRate(HeliocentricDistance);
    }

    inline void GenerateReactionProducts(int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }

    inline void GenerateGivenProducts(int prodSpec, int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Huebner1992ASS::GenerateGivenProducts(prodSpec, ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }

    inline double GetSpeciesReactionYield(int spec) {
      return Huebner1992ASS::GetSpeciesReactionYield(spec);
    }
  }

  inline void Init() {
    H2O::Huebner1992ASS::Init();
    O2::Huebner1992ASS::Init();
    H2::Huebner1992ASS::Init();
    H::Huebner1992ASS::Init();
    OH::Huebner1992ASS::Init();
    O::Huebner1992ASS::Init();
  }

  inline void GenerateReactionProducts(int spec,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
    switch (spec) {
    case _H2O_SPEC_ :
      H2O::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _O2_SPEC_:
      O2::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _H2_SPEC_:
      H2::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _H_SPEC_:
      H::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _OH_SPEC_:
      OH::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _O_SPEC_:
      O::GenerateReactionProducts(ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the species is unknown");
    }
  }

  inline void GenerateGivenProducts(int spec,int prodSpec,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
    switch (spec) {
    case _H2O_SPEC_ :
      H2O::GenerateGivenProducts( prodSpec,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _O2_SPEC_:
      O2::GenerateGivenProducts( prodSpec,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _H2_SPEC_:
      H2::GenerateGivenProducts( prodSpec,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _H_SPEC_:
      H::GenerateGivenProducts( prodSpec,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _OH_SPEC_:
      OH::GenerateGivenProducts( prodSpec,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _O_SPEC_:
      O::GenerateGivenProducts( prodSpec,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the species is unknown");
    }
  }

  
  inline void GenerateReactionProducts_test(){
    int ReactionChannel, nReactionProducts;
    int *ReactionProductsList;
    double * ReactionProductVelocity;
    

    for (int iSpec=0; iSpec<6;iSpec++){
      
      printf("iSpec:%d start\n", iSpec); 
     
      for (int j=0; j<10; j++){
	GenerateReactionProducts(iSpec, ReactionChannel,nReactionProducts, ReactionProductsList,ReactionProductVelocity);
	  //printf("ReactionChannel:%d, nReactionProducts:%d\n", ReactionChannel,nReactionProducts);	
	  printf("iSpec:%d, ReactionChannel:%d,nReactionProducts:%d\n",iSpec,ReactionChannel,nReactionProducts);
      }
      printf("iSpec:%d end\n", iSpec);
    }
    
  };
  


  //calculate the fraction of product reaction output
  inline double GetSpeciesReactionYield(int ProductSpec, int ParentSpec) {
    double res;

    switch (ParentSpec) {
    case _H2O_SPEC_ :
      res=H2O::GetSpeciesReactionYield(ProductSpec);
      break;
    case _O2_SPEC_:
      res=O2::GetSpeciesReactionYield(ProductSpec);
      break;
    case _H2_SPEC_:
      res=H2::GetSpeciesReactionYield(ProductSpec);
      break;
    case _H_SPEC_:
      res=H::GetSpeciesReactionYield(ProductSpec);
      break;
    case _OH_SPEC_:
      res=OH::GetSpeciesReactionYield(ProductSpec);
      break;
    case _O_SPEC_:
      res=O::GetSpeciesReactionYield(ProductSpec);
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the species is unknown");
    }

    return res;
  }

  //get the total reaction rate
  inline double GetTotalReactionRate(int ParentSpec,double HeliocentricDistance) {
    double res;

    switch (ParentSpec) {
    case _H2O_SPEC_ :
      res=H2O::GetTotalReactionRate(HeliocentricDistance);
      break;
    case _O2_SPEC_:
      res=O2::GetTotalReactionRate(HeliocentricDistance);
      break;
    case _H2_SPEC_:
      res=H2::GetTotalReactionRate(HeliocentricDistance);
      break;
    case _H_SPEC_:
      res=H::GetTotalReactionRate(HeliocentricDistance);
      break;
    case _OH_SPEC_:
      res=OH::GetTotalReactionRate(HeliocentricDistance);
      break;
    case _O_SPEC_:
      res=O::GetTotalReactionRate(HeliocentricDistance);
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the species is unknown");
    }

    return res;
  }

  //return model for which species is available
  inline bool ModelAvailable(int spec) {
    bool res;

    switch (spec) {
    case _H2O_SPEC_: case _O2_SPEC_: case _H2_SPEC_: case _H_SPEC_: case _OH_SPEC_: case _O_SPEC_:
      res=true;
      break;
    default:
      res=false;
    }

    return res;
  }


  inline bool hasDaughterSpec(int parentSpec, int daughterSpec) {
    bool res= false;
    int * ProdTable=NULL;
    int nMaxProducts=0, nChannels=0;
    switch (parentSpec) {
    case _H2O_SPEC_ :
      ProdTable=&H2O::Huebner1992ASS::ReactionProducts[0][0];
      nMaxProducts=H2O::Huebner1992ASS::nMaxReactionProducts;
      nChannels =H2O::Huebner1992ASS::nReactionChannels;
      break;
    case _O2_SPEC_:
      ProdTable=&O2::Huebner1992ASS::ReactionProducts[0][0];
      nMaxProducts=O2::Huebner1992ASS::nMaxReactionProducts;
      nChannels =O2::Huebner1992ASS::nReactionChannels;
      break;
    case _H2_SPEC_:
      ProdTable=&H2::Huebner1992ASS::ReactionProducts[0][0];
      nMaxProducts=H2::Huebner1992ASS::nMaxReactionProducts;
      nChannels =H2::Huebner1992ASS::nReactionChannels;
      break;
    case _H_SPEC_:
      ProdTable=&H::Huebner1992ASS::ReactionProducts[0][0];
      nMaxProducts=H::Huebner1992ASS::nMaxReactionProducts;
      nChannels =H::Huebner1992ASS::nReactionChannels;
      break;
    case _OH_SPEC_:
      ProdTable=&OH::Huebner1992ASS::ReactionProducts[0][0];
      nMaxProducts=OH::Huebner1992ASS::nMaxReactionProducts;
      nChannels =OH::Huebner1992ASS::nReactionChannels;
      break;
    case _O_SPEC_:
      ProdTable=&O::Huebner1992ASS::ReactionProducts[0][0];
      nMaxProducts=O::Huebner1992ASS::nMaxReactionProducts;
      nChannels =O::Huebner1992ASS::nReactionChannels;
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the species is unknown");
    }
   
    for (int iChannel=0; iChannel<nChannels; iChannel++){
      for (int i=0; i<nMaxProducts; i++){
	if (ProdTable[iChannel*nMaxProducts+i]==daughterSpec){
	  res=true;
	  break;
	}
      }
    }

    return res;

  }

}



#endif /* PHOTOLYTICREACTIONS_H_ */
