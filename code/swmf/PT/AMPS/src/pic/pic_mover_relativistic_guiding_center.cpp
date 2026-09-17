//$Id: pic_mover_relativistic_boris.cpp,v 1.11 2018/04/10 05:52:32 vtenishe Exp $
//relativistic particle trajectory integration

/*
 * pic_mover_relativistic_boris.cpp
 *
 *  Created on: Sep 25, 2016
 *      Author: vtenishe
 */

#include "pic.h"
#include "Exosphere.dfn"
#include "Exosphere.h"

void PIC::Mover::Relativistic::GuidingCenter::InitiateMagneticMoment(int spec,double *x, double *v,long int ptr, void *node) {
  PIC::Mover::Relativistic::GuidingCenter::InitiateMagneticMoment(spec,x,v,PIC::ParticleBuffer::GetParticleDataPointer(ptr),node);
}

void PIC::Mover::Relativistic::GuidingCenter::InitiateMagneticMoment(int spec,double *x, double *v,PIC::ParticleBuffer::byte *ParticleData, void *node) {
  //following the scheme described in Ropperda et al, https://doi.org/10.3847/1538-4365/aab114.
  //---------------------------------------------------------------
  
  // get the magnetic field
  double B[3]={0.0,0.0,0.0}, AbsB=0.0;
  double E[3]={0.0,0.0,0.0}, AbsE=0.0;

  //printf("GuidingCenter::InitiateMagneticMoment called\n");

  //the function can be executed only the the offset for the particle magnetic moment is defined
  if (_PIC_PARTICLE_DATA__MAGNETIC_MOMENT_OFFSET_!=-1) {
    // get the magnetic field
      switch (_PIC_COUPLER_MODE_) {
      case _PIC_COUPLER_MODE__OFF_ :
        exit(__LINE__,__FILE__,"not implemented");
	
      default:
        PIC::CPLR::InitInterpolationStencil(x,(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)node);
        PIC::CPLR::GetBackgroundMagneticField(B);
	PIC::CPLR::GetBackgroundElectricField(E);
      }

    AbsB=Vector3D::Length(B)+1E-15;
    AbsE=Vector3D::Length(E)+1E-15;
    //compute drifting velocity vE
    double vE[3],vE_norm=Vector3D::Length(vE);
    double v_norm = Vector3D::Length(v);
    Vector3D::CrossProduct(vE, E, B);
    if (AbsB>0.0){
      for (int idim=0; idim<3; idim++) {
	vE[idim] /= AbsB*AbsB;
      }
    }

    
    double kappa, gamma, gamma_star, c2 = SpeedOfLight*SpeedOfLight;
    kappa = 1/sqrt(1-vE_norm*vE_norm/(c2)); 
    gamma = 1/sqrt(1-v_norm*v_norm/(c2));
    // compute magnetic moment
    double v_par, v2, gamma2, m0, mu=0.0;
    double B_star[3],v_star[3];
    gamma_star = gamma/kappa;// gamma in the frame of vE where E field vanishes
    //B_star = kappa*(vec(B)-1/c^2*vec(vE) X vec(E))-(kappa-1)(B dot vE)vec(vE)/(vEnorm)^2
    double vE_cross_E[3], B_dot_vE=Vector3D::DotProduct(B, vE);
    
    Vector3D::CrossProduct(vE_cross_E, vE, E);
    for (int idim=0; idim<3; idim++){
      B_star[idim] = kappa*(B[idim]-vE_cross_E[idim]/c2);
      if (vE_norm>0)  B_star[idim] -= (kappa-1)*B_dot_vE*vE[idim]/(vE_norm*vE_norm);
      v_star[idim] = v[idim]-vE[idim];
    }
    double Bstar_norm= Vector3D::Length(B_star);
    /*
    printf("init mag mom gamma_star:%e, Bstar_norm:%e,kappa:%e, gamma:%e,B:%e,%e,%e\n", gamma_star, 
    Bstar_norm,kappa,gamma,B[0],B[1],B[2]);
    */

    if (Bstar_norm > 0.0) {
      double vstar_par;
      double vstar_norm = Vector3D::Length(v_star);
     
      vstar_par = Vector3D::DotProduct(v_star,B_star)/Bstar_norm; 

      m0     = PIC::MolecularData::GetMass(spec);
      mu     = 0.5 * (gamma_star*gamma_star) * m0 * (vstar_norm*vstar_norm-vstar_par*vstar_par)/Bstar_norm;
      //printf("init mag mom gamma_star:%e, vstar_norm:%e, vstar_par:%e, Bstar_norm:%e, mu:%e\n", gamma_star, vstar_norm,
      //vstar_par, Bstar_norm,mu);
    }

    PIC::ParticleBuffer::SetMagneticMoment(mu, ParticleData);
  }
}



//advance particle location
int PIC::Mover::Relativistic::GuidingCenter::Mover_FirstOrder(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *newNode=NULL;
  PIC::ParticleBuffer::byte *ParticleData;
  double *xminBlock,*xmaxBlock;
  double mass,QdT_over_twoM,ElectricCharge;
  int idim,i,j,k,spec;
  double vInit[3],xInit[3],xFinal[3],vFinal[3]={0.0,0.0,0.0};
  double var15[15];
  double * b_dot_grad_b, *vE_dot_grad_b, * b_dot_grad_vE,
    *vE_dot_grad_vE, *grad_kappaB;
  double B[3],E[3];
  int code;
  b_dot_grad_b = var15;
  vE_dot_grad_b = var15+ 3;
  b_dot_grad_vE = var15+ 6;
  vE_dot_grad_vE = var15+ 9;
  grad_kappaB =var15+12;

  //printf("ptr:%d,gca called\n", ptr);

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetV(vInit,ParticleData);
  PIC::ParticleBuffer::GetX(xInit,ParticleData);
  spec=PIC::ParticleBuffer::GetI(ParticleData);
  
  ElectricCharge=PIC::MolecularData::GetElectricCharge(spec);
  mass=PIC::MolecularData::GetMass(spec);


  double vNorm=sqrt(vInit[0]*vInit[0]+ vInit[1]*vInit[1]+vInit[2]*vInit[2]);
  double c2 = SpeedOfLight*SpeedOfLight;
  double lfac = 1/sqrt(1.0 - vNorm*vNorm/c2);

  PIC::CPLR::InitInterpolationStencil(xInit,(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)startNode);
  PIC::CPLR::GetBackgroundMagneticField(B);
  PIC::CPLR::GetBackgroundElectricField(E);
  PIC::CPLR::GetVarForRelativisticGCA(var15);
  //printf("test gca ptr:%d, B:%e,%e,%e, E:%e,%e,%e\n", ptr, B[0],B[1],B[2], E[0],E[1],E[2]);
  double bNorm = sqrt(B[0]*B[0]+B[1]*B[1]+B[2]*B[2]);
  double bHat[3]={0.0,0.0,0.0};//unit vector of B field
  double ePar=0.0;//Parallel E field in the B direction
  if (bNorm>0.0){
    for (int idim=0; idim<3; idim++) {
      bHat[idim] = B[idim]/bNorm;
      ePar += E[idim]*bHat[idim];
    }
  }
  
  double vPar= Vector3D::DotProduct(vInit, bHat);//vParallel
  double vPerp= sqrt(vNorm*vNorm - vPar*vPar);//vPerpedicular
  double uPar = lfac*vPar;//parallel momentum divided by m0
  //double Mr;// the conserved magnetic moment 
  double Mr  = PIC::ParticleBuffer::GetMagneticMoment(ptr);
  //use Mr to approximate Mr_star
  //Mr = mass * (vPerp *vPerp) *(lfac *lfac) / (2.0 * bNorm); 
  /*
  if (isnan(uPar)){
    printf("ptr:%d, magnetic mom:%e, vParOld:%e, vPerpOld:%e,lfac:%e, uParOld:%e\n", ptr, Mr, vPar, vPerp,lfac,uPar);
  }
  */
  double vE[3], vENorm=0.0;
  Vector3D::CrossProduct(vE, E, bHat);
  if (bNorm>0.0){
    for (int idim=0; idim<3; idim++) {
      vE[idim] = vE[idim] / bNorm;
      vENorm += vE[idim]*vE[idim];
    }
  }

  vENorm = sqrt(vENorm);
  double kappa, gamma;
  kappa = 1/sqrt(1-vENorm*vENorm/c2);
  gamma = sqrt(1.0+(uPar*uPar+2.0*Mr*bNorm/mass)/c2)*kappa;

  //printf("kappaOld:%e, gammaOld:%e\n", kappa, gamma);

  double utmp1[3]={0.0,0.0,0.0}, utmp2[3], utmp3[3];
  //dydt derivatives
  double temp = bNorm/(kappa*kappa);
  if (bNorm > 0.0){
    for (int idim=0; idim<3; idim++) {
      utmp1[idim] = bHat[idim]/temp;
    }
  }
  
  for (int idim=0; idim<3; idim++) {
    utmp2[idim] = Mr/(gamma*ElectricCharge)*grad_kappaB[idim] 
      + mass/ElectricCharge* (uPar*uPar/gamma*b_dot_grad_b[idim] + uPar*vE_dot_grad_b[idim] 
	      + uPar*b_dot_grad_vE[idim] + gamma*vE_dot_grad_vE[idim]);
  }

  //relatisitic part
  for (int idim=0; idim<3; idim++) {
    utmp2[idim] = utmp2[idim] + uPar*ePar/(gamma)*vE[idim];
  }

  //call cross(utmp1,utmp2,utmp3)
  //guiding center velocity
  double u[3];
  Vector3D::CrossProduct(utmp3,utmp1,utmp2);
  for (int idim=0; idim<3; idim++) {
    u[idim] = vE[idim] + utmp3[idim]; //velocity of guiding center
    u[idim] += uPar/gamma * bHat[idim];
   }


  /*
    dydt(1:ndir) = ( u(1:ndir) + upar/gamma * bhat(1:ndir) )
    dydt(ndir+1) = q/m*epar - Mr/(m*gamma) * sum(bhat(:)*gradkappaB(:)) &
                   + sum(vE(:)*(upar*bdotgradb(:)+gamma*vEdotgradb(:)))
    dydt(ndir+2) = 0.0d0 ! magnetic moment is conserved
  */
  //particle ve
  
  //upar at next time step
  double dupardt;
  dupardt  = ElectricCharge/mass*ePar;
  temp = Mr/(mass*gamma);
  for (int idim=0; idim<3; idim++) {
    dupardt +=  - temp*bHat[idim]*grad_kappaB[idim]+
      vE[idim]*(uPar*b_dot_grad_b[idim]+gamma*vE_dot_grad_b[idim]);
    xFinal[idim] =xInit[idim]+dtTotal*u[idim]; //guiding center loc at next step
  }
   

  uPar += dupardt*dtTotal;

  /*
  if (isnan(uPar)){
    printf("grad_kappaB[idim]:%e\n",grad_kappaB[0]);
    printf("dupardt:%e, ElectricCharge/mass*ePar:%e, temp*bHat[idim]*grad_kappaB[idim]:%e", dupardt, ElectricCharge/mass*ePar, temp*bHat[0]*grad_kappaB[0]);
    printf("test gca ptr:%d, dtTotal:%e,xInit:%e,%e,%e, u:%e,%e,%e\n", ptr, dtTotal,xInit[0],xInit[1],xInit[2], u[0],u[1],u[2]);
  }
  */


  #if  _TARGET_ID_(_TARGET_) != _TARGET_NONE__ID_ && _INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_ON_ 
  double rFinal = sqrt(xFinal[0]*xFinal[0]+xFinal[1]*xFinal[1]+xFinal[2]*xFinal[2]);
  if (rFinal<_RADIUS_(_TARGET_))  { 
    static cInternalSphericalData_UserDefined::fParticleSphereInteraction ParticleSphereInteraction=
      ((cInternalSphericalData*)(PIC::Mesh::mesh->InternalBoundaryList.front().BoundaryElement))->ParticleSphereInteraction;
    static void* BoundaryElement=PIC::Mesh::mesh->InternalBoundaryList.front().BoundaryElement;
    
    code= ParticleSphereInteraction(spec,ptr,xInit,vInit,dtTotal,(void*)startNode,BoundaryElement);
    
    
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
  }
  #endif
  
  newNode=PIC::Mesh::mesh->findTreeNode(xFinal,startNode);

  if (newNode==NULL){
    	//intersects with outer bounary
#if _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__DELETE_
	  //printf("delete test2 out\n");
	  //printf("nDeleteExternal:%d, x_test:%e,%e,%e\n", nDeleteExternal, xInit[0],xInit[1],xInit[2]);
	   
	PIC::ParticleBuffer::DeleteParticle(ptr);
	return _PARTICLE_LEFT_THE_DOMAIN_;
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_
   // code=ProcessOutsideDomainParticles(ptr,xInit,vInit,nIntersectionFace,startNode);
    //xInit,vInit, startNode may change inside the user defined function
	
	
  exit(__LINE__,__FILE__,"Error: branch _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__USER_FUNCTION_ is not implemented");

#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__PERIODIC_CONDITION_
    exit(__LINE__,__FILE__,"Error: not implemented");
#elif _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE_ == _PIC_PARTICLE_DOMAIN_BOUNDARY_INTERSECTION_PROCESSING_MODE__SPECULAR_REFLECTION_
    //reflect the particle back into the domain
    {
      double c=0.0;
      for (int idim=0;idim<3;idim++) c+=ExternalBoundaryFaceTable[nIntersectionFace].norm[idim]*vInit[idim];
      for (int idim=0;idim<3;idim++) vInit[idim]-=2.0*c*ExternalBoundaryFaceTable[nIntersectionFace].norm[idim];
      //startNode stays the same
      //xInit stays the same
    }

    code=_PARTICLE_REJECTED_ON_THE_FACE_;
#else
    exit(__LINE__,__FILE__,"Error: the option is unknown");
#endif

  }else{
    
    PIC::CPLR::InitInterpolationStencil(xFinal,(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)newNode);
    PIC::CPLR::GetBackgroundMagneticField(B);
    PIC::CPLR::GetBackgroundElectricField(E);
    bNorm = sqrt(B[0]*B[0]+B[1]*B[1]+B[2]*B[2]);
    if (bNorm>0.0){
      for (int idim=0; idim<3; idim++)  bHat[idim] = B[idim]/bNorm;
    }
    
    Vector3D::CrossProduct(vE, E, bHat);
    vENorm =0.0;
    
    if (bNorm>0.0){
      for (int idim=0; idim<3; idim++) {
	vE[idim] = vE[idim] / bNorm;
	vENorm += vE[idim]*vE[idim];
      }
    }
    
    vENorm = sqrt(vENorm);
    kappa = 1/sqrt(1-vENorm*vENorm/c2);
    gamma = sqrt(1.0+(uPar*uPar+2.0*Mr*bNorm/mass)/c2)*kappa;//gamma at next time step
    vPar = uPar/gamma;
    //if(isnan(vPar)) printf("vENorm:%e, kappa:%e, gammaNew:%e, uParNew:%e\n", vENorm, kappa,gamma, uPar);
    double res = (1-1/(gamma*gamma))*c2-vPar*vPar;
    if (bNorm==0 && res<0) res=0.0;
    if (bNorm>0 && res<0) {
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }
    //vPerp = sqrt((1-1/(gamma*gamma))*c2-vPar*vPar);
    vPerp = sqrt(res);
    //if (isnan(vPerp)) printf("vParNew:%e, vPerpNew:%e\n", vPar, vPerp);
    double e0[3]={1.0,0.0,0.0}, e1[3]={0.0,1.0,0.0};
    double ePerp[3]={1.0,0.0,0.0}; //direction perp to bhat, init in case b is 0
    //if bhat parallel to e0, let vPerp be in the direction of e1 X bhat.
    // if not, vPerp in the direction of e0 X bhat;
    double diff = (e0[0]-bHat[0])*(e0[0]-bHat[0])+(e0[1]-bHat[1])*(e0[1]-bHat[1])+
      (e0[2]-bHat[2])*(e0[2]-bHat[2]);
    
    if (diff > 0.0){
      Vector3D::CrossProduct(ePerp,e0 , bHat);  
    }else{
      Vector3D::CrossProduct(ePerp,e1 , bHat);      
    }
    
    for (int idim=0; idim<3; idim++) {
      vFinal[idim] += vPerp*ePerp[idim]+vPar*bHat[idim];    
    }
    
  }

    //save the trajectory point
if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) { 
  PIC::ParticleTracker::RecordTrajectoryPoint(xFinal,vFinal,spec,ParticleData,(void*)newNode);

if (_PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_) { 
  PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(xFinal,vFinal,spec,ParticleData,(void*)newNode);
}
}

  //finish the trajectory integration procedure
  PIC::Mesh::cDataBlockAMR *block;
  
  if (PIC::Mesh::mesh->FindCellIndex(xFinal,i,j,k,newNode,false)==-1) {
    printf("test 2 xFinal:%e,%e,%e, newNode->xmin:%e,%e,%e, newNode->xmax:%e,%e,%e,ptr:%ld\n",xFinal[0],xFinal[1],xFinal[2],newNode->xmin[0],newNode->xmin[1],
	   newNode->xmin[2], newNode->xmax[0],newNode->xmax[1],newNode->xmax[2],ptr);
    exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");
  }
  if ((block=newNode->block)==NULL) {
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  long int tempFirstCellParticle=atomic_exchange(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),ptr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable* ThreadTempParticleMovingData=block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

  PIC::ParticleBuffer::SetNext(ThreadTempParticleMovingData->first,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (ThreadTempParticleMovingData->last==-1) ThreadTempParticleMovingData->last=ptr;
  if (ThreadTempParticleMovingData->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTempParticleMovingData->first);
  ThreadTempParticleMovingData->first=ptr;
#else
#error The option is unknown
#endif



  PIC::ParticleBuffer::SetV(vFinal,ParticleData);
  PIC::ParticleBuffer::SetX(xFinal,ParticleData);
  /*
  printf("test gca ptr:%d, xInit:%e,%e,%e, vInit:%e,%e,%e\n", ptr,xInit[0],xInit[1],xInit[2], vInit[0],vInit[1],vInit[2]);
  printf("test gca ptr:%d, xFinal:%e,%e,%e,vFinal:%e,%e,%e\n", ptr,xFinal[0],xFinal[1],xFinal[2], vFinal[0],vFinal[1],vFinal[2]);
  */
  return _PARTICLE_MOTION_FINISHED_;


}









