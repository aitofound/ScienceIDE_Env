//$Id$
//The electron impact reactions


#ifndef _ELECTRON_IMPACT_
#define _ELECTRON_IMPACT_

#include "pic.h"

//Below is calcualtion of the parameters of the electron impact processes
//Units:
//   1. Electron Temeprature is in eV
//   2. The rate coefficient is in m^3 s^{-1}

namespace ElectronImpact {

  //the ElectronImpact::Burger2010SSR functions and variables used by particular model
  namespace Burger2010SSR {
    static const double minlog10RateCoefficientTableElectronTemeprature_LOG10=0.0;
    static const double maxlog10RateCoefficientTableElectronTemeprature_LOG10=2.0;
    static const int log10RateCoefficientTableLength=51;
    static const double dlog10RateCoefficientTableElectronTemeprature_LOG10=(maxlog10RateCoefficientTableElectronTemeprature_LOG10-minlog10RateCoefficientTableElectronTemeprature_LOG10)/(log10RateCoefficientTableLength-1);

    void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int* ReturnReactionProductTable,double *ReturnReactionProductVelocityTable,int &nReactionProducts,int *ReactionChannelProducts,int nMaxReactionProducts,double *log10RateCoefficientTable,int nReactionChannels, double *minTemp, int *ReactionChannelProductNumber,double *ReactionProductMassTable);
    void GenerateGivenProducts(int prodSpec,double ElectronTemeprature,int &ReactionChannel,int* ReturnReactionProductTable,double *ReturnReactionProductVelocityTable,int &nReactionProducts,int *ReactionChannelProducts,int nMaxReactionProducts,double *log10RateCoefficientTable,int nReactionChannels, double *minTemp, int *ReactionChannelProductNumber,double *ReactionProductMassTable);
    void GetProductVelocity_3(double electronTemeprature, double minTemp,double * productMassArray, double * productVelocityTable);
    void GetProductVelocity_4(double electronTemeprature, double minTemp,double * productMassArray, double * productVelocityTable);
    double GetTotalRateCoefficient(double *RateCoefficientTable,double ElectronTemeprature,int nReactionChannels,double *log10RateCoefficientTable, double *minTemp); 

    void Print(const char* fname,const char* OutputDirectory,int nReactionChannels,int nMaxReactionProducts,int *ReactionChannelProducts,double *log10RateCoefficientTable,char *ReactionChannelProductsString);

    //calculate the yield fpr a particular specie
    double GetSpeciesReactionYield(int spec,double ElectronTemeprature,double *log10RateCoefficientTable, int nReactionChannels, int* ReactionChannelProducts, int nMaxReactionProducts);
  }


  //the models of the electron impact of H2O
  namespace H2O {

    namespace Burger2010SSR { //the model data are digitized from Burger et al., 2014 in Space Science Review
      static const int nReactionChannels=5;
      static const int nMaxReactionProducts=4;

      extern int ReactionChannelProducts[nReactionChannels][nMaxReactionProducts];
      extern char ReactionChannelProductsString[nReactionChannels][_MAX_STRING_LENGTH_PIC_];
      extern double log10RateCoefficientTable[nReactionChannels][ElectronImpact::Burger2010SSR::log10RateCoefficientTableLength];
      extern double minTemp[nReactionChannels];
      
      extern double ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];
      extern int ReactionChannelProductNumber[nReactionChannels];
      extern int ReturnReactionProductTable[nMaxReactionProducts];
      extern double ReturnReactionProductVelocityTable[3*nMaxReactionProducts];

      inline double GetTotalRateCoefficient(double ElectronTemeprature) {
        double RateCoefficientTable[nReactionChannels];

        return ElectronImpact::Burger2010SSR::GetTotalRateCoefficient(RateCoefficientTable,ElectronTemeprature,nReactionChannels,&log10RateCoefficientTable[0][0],minTemp);
      }

      inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
	&ReactionChannelProducts[0][0],nMaxReactionProducts,
        &log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);

        ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }

      inline void GenerateGivenProducts(int prodSpec, double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
								&ReactionChannelProducts[0][0],nMaxReactionProducts,
        &log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);
	
        ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }
      

      // *ReactionChannelProductNumber,double *ReactionProductMassTable,int nMaxReactionProducts

      inline void Print(const char* fname,const char* OutputDirectory) {
        ElectronImpact::Burger2010SSR::Print(fname,OutputDirectory,nReactionChannels,nMaxReactionProducts,&ReactionChannelProducts[0][0],&log10RateCoefficientTable[0][0],&ReactionChannelProductsString[0][0]);
      }

      inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
        return ElectronImpact::Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature,&log10RateCoefficientTable[0][0],nReactionChannels,&ReactionChannelProducts[0][0],nMaxReactionProducts);
      }

    }

    inline double RateCoefficient(double ElectronTemeprature) {
      return Burger2010SSR::GetTotalRateCoefficient(ElectronTemeprature);
    }

    /*
    inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable,minTemp);
    }
    */
    inline void Print(const char* fname,const char* OutputDirectory) {
      Burger2010SSR::Print(fname,OutputDirectory);
    }

    inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
      return Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature);
    }
  }

  //the model of the electron impact of O2
  namespace O2 {

    namespace Burger2010SSR { //the model data are digitized from Burger et al., 2014 in Space Science Review
      static const int nReactionChannels=3;
      static const int nMaxReactionProducts=4;

      extern int ReactionChannelProducts[nReactionChannels][nMaxReactionProducts];
      extern char ReactionChannelProductsString[nReactionChannels][_MAX_STRING_LENGTH_PIC_];
      extern double log10RateCoefficientTable[nReactionChannels][ElectronImpact::Burger2010SSR::log10RateCoefficientTableLength];
      extern double minTemp[nReactionChannels];
       

      extern int ReturnReactionProductTable[nMaxReactionProducts];
      extern double ReturnReactionProductVelocityTable[3*nMaxReactionProducts];
      extern double ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];
      extern int ReactionChannelProductNumber[nReactionChannels];

      

      inline double GetTotalRateCoefficient(double ElectronTemeprature) {
        double RateCoefficientTable[nReactionChannels];

        return ElectronImpact::Burger2010SSR::GetTotalRateCoefficient(RateCoefficientTable,ElectronTemeprature,nReactionChannels,&log10RateCoefficientTable[0][0],minTemp);
      }

      
      inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
            &ReactionChannelProducts[0][0],nMaxReactionProducts,
								&log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);
	
	ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }


      inline void GenerateGivenProducts(int prodSpec, double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
								&ReactionChannelProducts[0][0],nMaxReactionProducts,
        &log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);
	
        ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }



      inline void Print(const char* fname,const char* OutputDirectory) {
        ElectronImpact::Burger2010SSR::Print(fname,OutputDirectory,nReactionChannels,nMaxReactionProducts,&ReactionChannelProducts[0][0],&log10RateCoefficientTable[0][0],&ReactionChannelProductsString[0][0]);
      }

      inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
        return ElectronImpact::Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature,&log10RateCoefficientTable[0][0],nReactionChannels,&ReactionChannelProducts[0][0],nMaxReactionProducts);
      }

    }

    inline double RateCoefficient(double ElectronTemeprature) {
      return Burger2010SSR::GetTotalRateCoefficient(ElectronTemeprature);
    }

    /*
    inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }
    */
    inline void Print(const char* fname,const char* OutputDirectory) {
      Burger2010SSR::Print(fname,OutputDirectory);
    }

    inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
      return Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature);
    }
  }


  namespace H2 {

    namespace Burger2010SSR { //the model data are digitized from Burger et al., 2014 in Space Science Review
      static const int nReactionChannels=3;
      static const int nMaxReactionProducts=4;

      extern int ReactionChannelProducts[nReactionChannels][nMaxReactionProducts];
      extern char ReactionChannelProductsString[nReactionChannels][_MAX_STRING_LENGTH_PIC_];
      extern double log10RateCoefficientTable[nReactionChannels][ElectronImpact::Burger2010SSR::log10RateCoefficientTableLength];
      extern double minTemp[nReactionChannels];


      extern int ReturnReactionProductTable[nMaxReactionProducts];
      extern double ReturnReactionProductVelocityTable[3*nMaxReactionProducts];
      extern double ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];
      extern int ReactionChannelProductNumber[nReactionChannels];


      inline double GetTotalRateCoefficient(double ElectronTemeprature) {
        double RateCoefficientTable[nReactionChannels];

        return ElectronImpact::Burger2010SSR::GetTotalRateCoefficient(RateCoefficientTable,ElectronTemeprature,nReactionChannels,&log10RateCoefficientTable[0][0],minTemp);
      }

      
      inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
								&ReactionChannelProducts[0][0],nMaxReactionProducts,
								&log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);
	
	ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }

      inline void GenerateGivenProducts(int prodSpec, double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
							     &ReactionChannelProducts[0][0],nMaxReactionProducts,
							     &log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);
	
        ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }
      
	
	inline void Print(const char* fname,const char* OutputDirectory) {
        ElectronImpact::Burger2010SSR::Print(fname,OutputDirectory,nReactionChannels,nMaxReactionProducts,&ReactionChannelProducts[0][0],&log10RateCoefficientTable[0][0],&ReactionChannelProductsString[0][0]);
      }

      inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
        return ElectronImpact::Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature,&log10RateCoefficientTable[0][0],nReactionChannels,&ReactionChannelProducts[0][0],nMaxReactionProducts);
      }

    }

    inline double RateCoefficient(double ElectronTemeprature) {
      return Burger2010SSR::GetTotalRateCoefficient(ElectronTemeprature);
    }

    /*
    inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }
    */

    inline void Print(const char* fname,const char* OutputDirectory) {
      Burger2010SSR::Print(fname,OutputDirectory);
    }

    inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
      return Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature);
    }
  }

  namespace H {

    namespace Burger2010SSR { //the model data are digitized from Burger et al., 2014 in Space Science Review
      static const int nReactionChannels=1;
      static const int nMaxReactionProducts=3;

      extern int ReactionChannelProducts[nReactionChannels][nMaxReactionProducts];
      extern char ReactionChannelProductsString[nReactionChannels][_MAX_STRING_LENGTH_PIC_];
      extern double log10RateCoefficientTable[nReactionChannels][ElectronImpact::Burger2010SSR::log10RateCoefficientTableLength];
      extern double minTemp[nReactionChannels];

      
      extern int ReturnReactionProductTable[nMaxReactionProducts];
      extern double ReturnReactionProductVelocityTable[3*nMaxReactionProducts];
      extern double ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];
      extern int ReactionChannelProductNumber[nReactionChannels];
      
      
      inline double GetTotalRateCoefficient(double ElectronTemeprature) {
        double RateCoefficientTable[nReactionChannels];

        return ElectronImpact::Burger2010SSR::GetTotalRateCoefficient(RateCoefficientTable,ElectronTemeprature,nReactionChannels,&log10RateCoefficientTable[0][0],minTemp);
      }

      
      inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
								&ReactionChannelProducts[0][0],nMaxReactionProducts,
								&log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);
	
       
	ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }
      
      inline void GenerateGivenProducts(int prodSpec, double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
							     &ReactionChannelProducts[0][0],nMaxReactionProducts,
							     &log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);
	
        ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }



      inline void Print(const char* fname,const char* OutputDirectory) {
        ElectronImpact::Burger2010SSR::Print(fname,OutputDirectory,nReactionChannels,nMaxReactionProducts,&ReactionChannelProducts[0][0],&log10RateCoefficientTable[0][0],&ReactionChannelProductsString[0][0]);
      }

      inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
        return ElectronImpact::Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature,&log10RateCoefficientTable[0][0],nReactionChannels,&ReactionChannelProducts[0][0],nMaxReactionProducts);
      }

    }

    inline double RateCoefficient(double ElectronTemeprature) {
      return Burger2010SSR::GetTotalRateCoefficient(ElectronTemeprature);
    }

    /*
    inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }
    */

    inline void Print(const char* fname,const char* OutputDirectory) {
      Burger2010SSR::Print(fname,OutputDirectory);
    }

    inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
      return Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature);
    }
  }


  namespace O {

    namespace Burger2010SSR { //the model data are digitized from Burger et al., 2014 in Space Science Review
      static const int nReactionChannels=1;
      static const int nMaxReactionProducts=3;

      extern int ReactionChannelProducts[nReactionChannels][nMaxReactionProducts];
      extern char ReactionChannelProductsString[nReactionChannels][_MAX_STRING_LENGTH_PIC_];
      extern double log10RateCoefficientTable[nReactionChannels][ElectronImpact::Burger2010SSR::log10RateCoefficientTableLength];
      extern double minTemp[nReactionChannels];

      extern int ReturnReactionProductTable[nMaxReactionProducts];
      extern double ReturnReactionProductVelocityTable[3*nMaxReactionProducts];
      extern double ReactionProductMassTable[nReactionChannels][nMaxReactionProducts];
      extern int ReactionChannelProductNumber[nReactionChannels];

      
      inline double GetTotalRateCoefficient(double ElectronTemeprature) {
        double RateCoefficientTable[nReactionChannels];

        return ElectronImpact::Burger2010SSR::GetTotalRateCoefficient(RateCoefficientTable,ElectronTemeprature,nReactionChannels,&log10RateCoefficientTable[0][0],minTemp);
      }

      
      inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
								&ReactionChannelProducts[0][0],nMaxReactionProducts,
								&log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);
	
	ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }
      
      inline void GenerateGivenProducts(int prodSpec, double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
        ElectronImpact::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,ReturnReactionProductTable,ReturnReactionProductVelocityTable,nReactionProducts,
							     &ReactionChannelProducts[0][0],nMaxReactionProducts,
							     &log10RateCoefficientTable[0][0],nReactionChannels,minTemp, ReactionChannelProductNumber, &ReactionProductMassTable[0][0]);
	
        ReactionProductTable=ReturnReactionProductTable;
        ReactionProductVelocityTable=ReturnReactionProductVelocityTable;
      }




      inline void Print(const char* fname,const char* OutputDirectory) {
        ElectronImpact::Burger2010SSR::Print(fname,OutputDirectory,nReactionChannels,nMaxReactionProducts,&ReactionChannelProducts[0][0],&log10RateCoefficientTable[0][0],&ReactionChannelProductsString[0][0]);
      }

      inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
        return ElectronImpact::Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature,&log10RateCoefficientTable[0][0],nReactionChannels,&ReactionChannelProducts[0][0],nMaxReactionProducts);
      }

    }

    inline double RateCoefficient(double ElectronTemeprature) {
      return Burger2010SSR::GetTotalRateCoefficient(ElectronTemeprature);
    }

    /*
    inline void GenerateReactionProducts(double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
      Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
    }
    */
    inline void Print(const char* fname,const char* OutputDirectory) {
      Burger2010SSR::Print(fname,OutputDirectory);
    }

    inline double GetSpeciesReactionYield(int spec,double ElectronTemeprature) {
      return Burger2010SSR::GetSpeciesReactionYield(spec,ElectronTemeprature);
    }
  }
  /*
  void GenerateReactionProducts_test(){
    int ReactionChannel, nReactionProducts;
    int *ReactionProductsList;
    double *ReactionProductVelocity;
    

    for (int iSpec=0; iSpec<5;iSpec++){
      
      printf("iSpec:%d start\n", iSpec);
      double emin=-2, emax=4, de =0.1; 
      for (int i=0; i<20; i++){
	double eV= pow(10, emin+de*i);
	printf("iSpec:%d, eV:%e\n",iSpec,eV);
	for (int j=0; j<10; j++){
	  GenerateReactionProducts(iSpec,eV, ReactionChannel,nReactionProducts, ReactionProductsList,ReactionProductVelocity);
	  //printf("ReactionChannel:%d, nReactionProducts:%d\n", ReactionChannel,nReactionProducts);	
	  printf("iSpec:%d, ReactionChannel:%d,nReactionProducts:%d,eV:%e,j:%d\n",iSpec,ReactionChannel,nReactionProducts,eV,j);
	  double mx=0,my=0,mz=0, energy=0;
	  double ** ReactionProductMassTable;
	  
	  
	  switch (iSpec) {
	  case _H2O_SPEC_ :
	  ReactionProductMassTable=H2O::Burger2010SSR::ReactionProductMassTable;
	  break;
	  case _O2_SPEC_:
	    ReactionProductMassTable=O2::Burger2010SSR::ReactionProductMassTable;  
	    break;
	  case _H2_SPEC_:
	    ReactionProductMassTable=H2::Burger2010SSR::ReactionProductMassTable;  
	    break;
	  case _H_SPEC_:
	    ReactionProductMassTable=H::Burger2010SSR::ReactionProductMassTable;  
	    break;
	  case _O_SPEC_:
	    ReactionProductMassTable=O::Burger2010SSR::ReactionProductMassTable;  
	    break;
	  default:
	    exit(__LINE__,__FILE__,"Error: the species is unknown");
	  }
	  
	
	for (int ii=0; ii<nReactionProducts; ii++){
	  double vx,vy,vz,mass;
	  mass = ReactionProductMassTable[ReactionChannel][ii];
	  vx = ReturnReactionProductVelocity[3*ii+0];
	  vy = ReturnReactionProductVelocity[3*ii+1];
	  vz = ReturnReactionProductVelocity[3*ii+2];
	  mx += mass*vx;
	  my += mass*vy;
	  mz += mass*vz;
	  energy +=0.5*mass*(vx*vx+vy*vy+vz*vz);
	  printf("iProduct:%d,mass:%e,v:%e,%e,%e\n",ii,mass,vx,vy,vz);
	}
	
	printf("iSpec:%d,reactionChannel:%d mx:%e,%e,%e, energy:%e,eV:%e, j:%d\n", iSpec, ReactionChannel, mx,my,mz, energy/eV2J,eV,j);
	}
      }
      
      printf("iSpec:%d end\n", iSpec);
    }
    
  };
  */

  inline void GenerateReactionProducts(int spec,double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
    switch (spec) {
    case _H2O_SPEC_ :
      H2O::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _O2_SPEC_:
      O2::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _H2_SPEC_:
      H2::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _H_SPEC_:
      H::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _O_SPEC_:
      O::Burger2010SSR::GenerateReactionProducts(ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;

    default:
      exit(__LINE__,__FILE__,"Error: the species is unknown");
    }
  }

  inline void GenerateGivenProducts(int spec,int prodSpec,double ElectronTemeprature,int &ReactionChannel,int &nReactionProducts, int* &ReactionProductTable,double* &ReactionProductVelocityTable) {
    switch (spec) {
    case _H2O_SPEC_ :
      H2O::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _O2_SPEC_:
      O2::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _H2_SPEC_:
      H2::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _H_SPEC_:
      H::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
    case _O_SPEC_:
      O::Burger2010SSR::GenerateGivenProducts(prodSpec,ElectronTemeprature,ReactionChannel,nReactionProducts,ReactionProductTable,ReactionProductVelocityTable);
      break;
      
    default:
      exit(__LINE__,__FILE__,"Error: the species is unknown");
    }
  }


  
    inline void GenerateReactionProducts_test(){
    int ReactionChannel, nReactionProducts;
    int *ReactionProductsList;
    double * ReactionProductVelocity;
    

    for (int iSpec=0; iSpec<5;iSpec++){
      
      printf("iSpec:%d start\n", iSpec);
      double emin=-2, emax=4, de =0.3; 
      for (int i=0; i<20; i++){
	double eV= pow(10, emin+de*i);
	printf("iSpec:%d, ieV, %d,eV:%e\n",iSpec,i, eV);
	for (int j=0; j<10; j++){
	  GenerateReactionProducts(iSpec,eV, ReactionChannel,nReactionProducts, ReactionProductsList,ReactionProductVelocity);
	  //printf("ReactionChannel:%d, nReactionProducts:%d\n", ReactionChannel,nReactionProducts);	
	  printf("iSpec:%d, ReactionChannel:%d,nReactionProducts:%d,eV:%e,j:%d\n",iSpec,ReactionChannel,nReactionProducts,eV,j);
	  double mx=0,my=0,mz=0, energy=0;
	  double * ReactionProductMassTable;
	  int  nMaxReactionProducts;
	  
	  switch (iSpec) {
	  case _H2O_SPEC_ :
	    ReactionProductMassTable=&H2O::Burger2010SSR::ReactionProductMassTable[0][0];
	    nMaxReactionProducts=H2O::Burger2010SSR::nMaxReactionProducts;
	    break;
	  case _O2_SPEC_:
	    ReactionProductMassTable=&O2::Burger2010SSR::ReactionProductMassTable[0][0];
	    nMaxReactionProducts=O2::Burger2010SSR::nMaxReactionProducts;
	    break;
	  case _H2_SPEC_:
	    ReactionProductMassTable=&H2::Burger2010SSR::ReactionProductMassTable[0][0];
	    nMaxReactionProducts=H2::Burger2010SSR::nMaxReactionProducts;
	    break;
	  case _H_SPEC_:
	    ReactionProductMassTable=&H::Burger2010SSR::ReactionProductMassTable[0][0];
	    nMaxReactionProducts=H::Burger2010SSR::nMaxReactionProducts;
	    break;
	  case _O_SPEC_:
	    ReactionProductMassTable=&O::Burger2010SSR::ReactionProductMassTable[0][0];
	    nMaxReactionProducts=O::Burger2010SSR::nMaxReactionProducts;
	    break;
	  default:
	    exit(__LINE__,__FILE__,"Error: the species is unknown");
	  }
	  
	
	for (int ii=0; ii<nReactionProducts; ii++){
	  double vx,vy,vz,mass;
	  mass = ReactionProductMassTable[ReactionChannel*nMaxReactionProducts+ii];
	  vx = ReactionProductVelocity[3*ii+0];
	  vy = ReactionProductVelocity[3*ii+1];
	  vz = ReactionProductVelocity[3*ii+2];
	  mx += mass*vx;
	  my += mass*vy;
	  mz += mass*vz;
	  energy +=0.5*mass*(vx*vx+vy*vy+vz*vz);
	  printf("iProduct:%d,mass:%e,v:%e,%e,%e\n",ii,mass,vx,vy,vz);
	}
	
	printf("iSpec:%d,reactionChannel:%d mx:%e,%e,%e,total energy:%e, eV:%e, j:%d\n", iSpec, ReactionChannel, mx,my,mz, energy/eV2J,eV,j);
	}
      }
      
      printf("iSpec:%d end\n", iSpec);
    }
    
  };



    //calculate the fraction of product reaction output
  inline double GetSpeciesReactionYield(int ProductSpec, int ParentSpec,double ElectronTemeprature) {
    double res;

    switch (ParentSpec) {
    case _H2O_SPEC_ :
      res=H2O::GetSpeciesReactionYield(ProductSpec,ElectronTemeprature);
      break;
    case _O2_SPEC_:
      res=O2::GetSpeciesReactionYield(ProductSpec,ElectronTemeprature);
      break;
    case _H2_SPEC_:
      res=H2::GetSpeciesReactionYield(ProductSpec,ElectronTemeprature);
      break;
    case _H_SPEC_:
      res=H::GetSpeciesReactionYield(ProductSpec,ElectronTemeprature);
      break;
    case _O_SPEC_:
      res=O::GetSpeciesReactionYield(ProductSpec,ElectronTemeprature);
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
    case _H2O_SPEC_: case _O2_SPEC_: case _H2_SPEC_: case _H_SPEC_: case _O_SPEC_:
      res=true;
      break;
    default:
      res=false;
    }

    return res;
  }

  inline bool hasDaughterSpec(int parentSpec, int daughterSpec, double ElectronTemeprature) {
    bool res= false;
    int * ProdTable=NULL;
    int nMaxProducts=0, nChannels=0;
    double * minTemp;
    switch (parentSpec) {
    case _H2O_SPEC_ :
      ProdTable=&H2O::Burger2010SSR::ReactionChannelProducts[0][0];
      nMaxProducts=H2O::Burger2010SSR::nMaxReactionProducts;
      nChannels = H2O::Burger2010SSR::nReactionChannels;
      minTemp = H2O::Burger2010SSR::minTemp;
      break;
    case _O2_SPEC_:
      ProdTable=&O2::Burger2010SSR::ReactionChannelProducts[0][0];
      nMaxProducts=O2::Burger2010SSR::nMaxReactionProducts;
      nChannels = O2::Burger2010SSR::nReactionChannels;
      minTemp = O2::Burger2010SSR::minTemp;
      break;
    case _H2_SPEC_:
      ProdTable=&H2::Burger2010SSR::ReactionChannelProducts[0][0];
      nMaxProducts=H2::Burger2010SSR::nMaxReactionProducts;
      nChannels = H2::Burger2010SSR::nReactionChannels;
      minTemp = H2::Burger2010SSR::minTemp;
      break;
    case _H_SPEC_:
      ProdTable=&H::Burger2010SSR::ReactionChannelProducts[0][0];
      nMaxProducts=H::Burger2010SSR::nMaxReactionProducts;
      nChannels = H::Burger2010SSR::nReactionChannels;
      minTemp = H::Burger2010SSR::minTemp;
      break;
    case _O_SPEC_:
      ProdTable=&O::Burger2010SSR::ReactionChannelProducts[0][0];
      nMaxProducts=O::Burger2010SSR::nMaxReactionProducts;
      nChannels = O::Burger2010SSR::nReactionChannels;
      minTemp = O::Burger2010SSR::minTemp;
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the species is unknown");
    }
   
    for (int iChannel=0; iChannel<nChannels; iChannel++){
      if (ElectronTemeprature>minTemp[iChannel]){
	for (int i=0; i<nMaxProducts; i++){
	  if (ProdTable[iChannel*nMaxProducts+i]==daughterSpec){
	    res=true;
	    break;
	  }
	}
      }
      if (res) break;
    }

    return res;

  }

  


}






#endif
