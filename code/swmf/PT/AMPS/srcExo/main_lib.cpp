
#include "pic.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <time.h>

#include <sys/time.h>
#include <sys/resource.h>

//forward scattering cross section
#include "ForwardScatteringCrossSection.h"


//the particle class
#include "rnd.h"
#include "pic.h"
#include "Mars.h"
#include "MTGCM.h"

int Exoplanet::LossProcesses::ThermalParticleReleasingProcessor(double *xInit,double *xFinal,double *vFinal,long int ptr,int &spec,PIC::ParticleBuffer::byte *ParticleData, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  int *ReactionProductsList,nReactionProducts;
  double *ReactionProductVelocity;
  int ReactionChannel;
  bool PhotolyticReactionRoute;


  //init the reaction tables
  static bool initflag=false;
  static double TotalProductYeld_PhotolyticReaction[PIC::nTotalSpecies*PIC::nTotalSpecies];
  static double TotalProductYeld_ElectronImpact[PIC::nTotalSpecies*PIC::nTotalSpecies];

  if (initflag==false) {
    int iParent,iProduct;

    initflag=true;

    for (iParent=0;iParent<PIC::nTotalSpecies;iParent++) for (iProduct=0;iProduct<PIC::nTotalSpecies;iProduct++) {
      TotalProductYeld_PhotolyticReaction[iProduct+iParent*PIC::nTotalSpecies]=0.0;
      TotalProductYeld_ElectronImpact[iProduct+iParent*PIC::nTotalSpecies]=0.0;

      if (PhotolyticReactions::ModelAvailable(iParent)==true) {
        TotalProductYeld_PhotolyticReaction[iProduct+iParent*PIC::nTotalSpecies]=PhotolyticReactions::GetSpeciesReactionYield(iProduct,iParent);
      }
    }
  }

  //determine the type of the reaction
  // PhotolyticReactionRoute=(rnd()<PhotolyticReactionRate/(PhotolyticReactionRate+ElectronImpactRate)) ? true : false;
  PhotolyticReactionRoute=true;

  //inject the products of the reaction
  double ParentTimeStep,ParentParticleWeight;

#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
  ParentParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
#else
  ParentParticleWeight=0.0;
  exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  ParentTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
#elif _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
  ParentTimeStep=node->block->GetLocalTimeStep(spec);
#else
  ParentTimeStep=0.0;
  exit(__LINE__,__FILE__,"Error: the time step node is not defined");
#endif


  //account for the parent particle correction factor
  ParentParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

  //the particle buffer used to set-up the new particle data
  char tempParticleData[PIC::ParticleBuffer::ParticleDataLength];
  PIC::ParticleBuffer::SetParticleAllocated((PIC::ParticleBuffer::byte*)tempParticleData);

  //copy the state of the initial parent particle into the new-daugher particle (just in case....)
  PIC::ParticleBuffer::CloneParticle((PIC::ParticleBuffer::byte*)tempParticleData,ParticleData);

  for (int specProduct=0;specProduct<PIC::nTotalSpecies;specProduct++) {
    double ProductTimeStep,ProductParticleWeight;
    double ModelParticleInjectionRate,TimeCounter=0.0,TimeIncrement,ProductWeightCorrection=1.0/NumericalLossRateIncrease;
    int iProduct;
    long int newParticle;
    PIC::ParticleBuffer::byte *newParticleData;


#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
     ProductParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[specProduct];
#else
     ProductParticleWeight=0.0;
     exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
    !!! ProductTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[specProduct]; //local time step 
#elif _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
     ProductTimeStep=node->block->GetLocalTimeStep(specProduct);
#else
     ProductTimeStep=0.0;
     exit(__LINE__,__FILE__,"Error: the time step node is not defined");
#endif

//1) calcualtion of injection rate//
    !!! ModelParticleInjectionRate=/* expression for particle flux through surface of sphere; just hardwire for the first step*/ ParentParticleWeight/ParentTimeStep/ProductParticleWeight*((PhotolyticReactionRoute==true) ? TotalProductYeld_PhotolyticReaction[specProduct+spec*PIC::nTotalSpecies] : TotalProductYeld_ElectronImpact[specProduct+spec*PIC::nTotalSpecies]);

     //inject the product particles
     if (ModelParticleInjectionRate>0.0) {
       TimeIncrement=-log(rnd())/ModelParticleInjectionRate *rnd(); //<- *rnd() is to account for the injection of the first particle in the curent interaction
//2) generate particle inside
      !!! while (TimeCounter+TimeIncrement<ProductTimeStep) {
         TimeCounter+=TimeIncrement; //total time 
         TimeIncrement=-log(rnd())/ModelParticleInjectionRate;

         //generate model particle with spec=specProduct
         bool flag=false;

         do {
           //generate a reaction channel
           if (PhotolyticReactionRoute==true) {
             PhotolyticReactions::GenerateReactionProducts(spec,ReactionChannel,nReactionProducts,ReactionProductsList,ReactionProductVelocity);
           }
           else {
	     /*             if (rnd()<Europa::ElectronModel::HotElectronFraction) ElectronImpact::GenerateReactionProducts(spec,Europa::ElectronModel::HotElectronTemperature,ReactionChannel,nReactionProducts,ReactionProductsList,ReactionProductVelocity);
			    else ElectronImpact::GenerateReactionProducts(spec,Europa::ElectronModel::ThermalElectronTemperature,ReactionChannel,nReactionProducts,ReactionProductsList,ReactionProductVelocity);*/
	   }

           //check whether the products contain species with spec=specProduct
           for (iProduct=0;iProduct<nReactionProducts;iProduct++) if (ReactionProductsList[iProduct]==specProduct) {
             flag=true;
             break;
           }
         }
         while (flag==false);


         //determine the velocity of the product species
         double ProductParticleVelocity[3];

         for (int idim=0;idim<3;idim++) ProductParticleVelocity[idim]=vFinal[idim]+ReactionProductVelocity[idim+3*iProduct];
      
          //determine location injection of particle with variable x uniformly
          // 1. Generate three random numbers x, y, z using Gaussian distribution
          // 2. Multiply each number by 1/sqrt(x^2+y^2+z^2) (a.k.a. Normalization); allows to handle if x=y=z=0
          // 3. Multiply each number by the radius of my sphere
           
         	double x[3]={0.0,0.0,0.0];
          double theta_ran, phi_ran, R_sonicpt;

          R_sonicpt = 180000; //just say 180km for debug purpose

         	theta_ran = -log(rnd())*2*Pi;
          phi_ran = asin(-1+2*(-log(rnd())));

          x[0]=R_sonicpt*cos(phi_ran)*cos(theta_ran);
          x[1]=R_sonicpt*cos(phi_ran)*sin(theta_ran);
          x[2]=R_sonicpt*sin(phi_ran);

      //copied example nothing related to this
      ////generate positions of the background particle in the cell
		  // xmin=node->xmin;
		  // xmax=node->xmax;
		  // xMiddle=cell->GetX();

		  // x[0]=xMiddle[0]+(xmax[0]-xmin[0])/_BLOCK_CELLS_X_*(rnd()-0.5);
		  // x[1]=xMiddle[1]+(xmax[1]-xmin[1])/_BLOCK_CELLS_Y_*(rnd()-0.5);
		  // x[2]=xMiddle[2]+(xmax[2]-xmin[2])/_BLOCK_CELLS_Z_*(rnd()-0.5);
      //copied example nothing related to this


      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode=NULL;  
      startNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
      if (startNode->Thread==PIC::Mesh::mesh->ThisThread) {
      
         //generate a particle ~ line#1724
         !!!PIC::ParticleBuffer::SetX(xFinal,(PIC::ParticleBuffer::byte*)tempParticleData); //particle buffer
         //I should calculate velocity of particle; maxwellian equation; flux maxwellian : normal vel * exponent
         PIC::Distribution::InjectMaxwellianDistribution(v,vbulk,SurfaceTemperature,ExternalNormal,spec); -> change variables

         PIC::ParticleBuffer::SetV(ProductParticleVelocity,(PIC::ParticleBuffer::byte*)tempParticleData); //
         PIC::ParticleBuffer::SetI(specProduct,(PIC::ParticleBuffer::byte*)tempParticleData);

         #if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
         PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ProductWeightCorrection,(PIC::ParticleBuffer::byte*)tempParticleData);
         #endif

         //apply condition of tracking the particle
         #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
         PIC::ParticleTracker::InitParticleID(tempParticleData);
         PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(xInit,xFinal,spec,tempParticleData,(void*)node);
         #endif


         //get and injection into the system the new model particle
         newParticle=PIC::ParticleBuffer::GetNewParticle();
         newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
         memcpy((void*)newParticleData,(void*)tempParticleData,PIC::ParticleBuffer::ParticleDataLength);

         _PIC_PARTICLE_MOVER__MOVE_PARTICLE_BOUNDARY_INJECTION_(newParticle,ProductTimeStep-TimeCounter,node,true);
       }
       }
     }

     }


  return (rnd()<1.0/NumericalLossRateIncrease) ? _PHOTOLYTIC_REACTIONS_PARTICLE_REMOVED_ : _PHOTOLYTIC_REACTIONS_NO_TRANSPHORMATION_;
  }