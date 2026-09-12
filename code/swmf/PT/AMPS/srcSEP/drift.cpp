
#include "sep.h"


/* the formaliaztion of the drift velocity calculation 
Here, I discuss an equation for the particle drift velocity. In Lulu's code, in application to the solar corona, particularly, to the low solar corona, equation for the drift velocity, $\mathbf{V}_d$, was used as follows:
\begin{equation}\label{eq:Rossi}
\mathbf{V}_d=
\frac{pv}{qB}\left\{\frac{1-\mu^2}2\frac{\mathbf{B}\times\nabla B}{B^2}+\mu^2\frac{\mathbf{B}\times\left[\left(\mathbf{B}\cdot\nabla\right)\mathbf{B}\right]}{B^3}+\frac{1-\mu^2}2\frac{\mathbf{B}\left(\mathbf{B}\cdot\nabla\times\mathbf{B}\right)}{B^3}\right\},
\end{equation}
which seems to be taken from \cite{Rossi1970} book. Herewith, $p$, $v$, and $q$ are the particle momentum, velocity, and charge and $\mathbf{B}$ and $B$ are the vector and magnitude of the magnetic field. Eq.~\ref{eq:Rossi} may be simplified using the identity as follows:
\begin{equation}\label{eq:baccab}
\left(\mathbf{B}\cdot\nabla\right)\mathbf{B}=B\nabla B - \mathbf{B}\times\nabla\times\mathbf{B},
\end{equation}
and by introducing the unit vector, $\mathbf{b}=\mathbf{B}/B$, so that:
\begin{equation}\label{eq:Rossi1}
\mathbf{V}_d=
\frac{pv}{qB^2}\left[\frac{1+\mu^2}2\mathbf{b}\times\nabla B+
\mu^2\nabla\times\mathbf{B}+\frac{1-3\mu^2}2\mathbf{b}\left(\mathbf{b}\cdot\nabla\times\mathbf{B}\right)\right].
\end{equation}
*/


int SEP::b_times_grad_absB_offset=-1;
int SEP::CurlB_offset=-1;
int SEP::b_b_Curl_B_offset=-1;

void SEP::InitDriftVelData() {
  int i,j,k; 

  auto GetB = [&] (double *B,PIC::InterpolationRoutines::CellCentered::cStencil& Stencil) {
    int idim;

    for (idim=0;idim<3;idim++) B[idim]=0.0;
    
    for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
      double *ptr=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset); 

      for (idim=0;idim<3;idim++) B[idim]+=Stencil.Weight[iStencil]*ptr[idim];
    }
  }; 

  auto GetBackgroundMagneticField = [&] (double *B,PIC::InterpolationRoutines::CellCentered::cStencil& Stencil) {
    int idim;
    double t[3];

    for (idim=0;idim<3;idim++) B[idim]=0.0;

    for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
      PIC::CPLR::SWMF::GetBackgroundMagneticField(t,Stencil.cell[iStencil]);

      for (idim=0;idim<3;idim++) B[idim]+=Stencil.Weight[iStencil]*t[idim];
    }
  };    

  auto ProcessCell = [&] (PIC::Mesh::cDataCenterNode *cell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode ) {
    double xcell[3],x_test[3];
    double Bdx[2][3],Bdy[2][3],Bdz[2][3]; //[i][idim] 
    int LocalCellNumber,idim,i,j,k;
    double delta;
    PIC::InterpolationRoutines::CellCentered::cStencil MagneticFieldStencil;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node_test;

    cell->GetX(xcell);

    delta=(startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_;
    delta=min(delta,startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_;
    delta=min(delta,startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_;

    //get the magnetic field mesh
    double AbsB,b[3];

    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xcell,startNode,MagneticFieldStencil);    
    GetBackgroundMagneticField(b,MagneticFieldStencil);
    AbsB=Vector3D::Normalize(b);

    for (i=0;i<2;i++) {
      memcpy(x_test,xcell,3*sizeof(double));
      x_test[0]+=(i==1) ? 0.5*delta : -0.5*delta;
      node_test=PIC::Mesh::mesh->findTreeNode(x_test,startNode);
 
      if (node_test->IsUsedInCalculationFlag==false) {
        memcpy(x_test,xcell,3*sizeof(double));
        node_test=startNode;
      }

      PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x_test,node_test,MagneticFieldStencil);
      GetBackgroundMagneticField(&Bdx[i][0],MagneticFieldStencil);

      memcpy(x_test,xcell,3*sizeof(double));
      x_test[1]+=(i==1) ? 0.5*delta : -0.5*delta;
      node_test=PIC::Mesh::mesh->findTreeNode(x_test,startNode);

      if (node_test->IsUsedInCalculationFlag==false) {
        memcpy(x_test,xcell,3*sizeof(double));
        node_test=startNode;
      }

      PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x_test,node_test,MagneticFieldStencil);
      GetBackgroundMagneticField(&Bdy[i][0],MagneticFieldStencil);

      memcpy(x_test,xcell,3*sizeof(double));
      x_test[2]+=(i==1) ? 0.5*delta : -0.5*delta;
      node_test=PIC::Mesh::mesh->findTreeNode(x_test,startNode);

      if (node_test->IsUsedInCalculationFlag==false) {
        memcpy(x_test,xcell,3*sizeof(double));
        node_test=startNode;
      }

      PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x_test,node_test,MagneticFieldStencil);
      GetBackgroundMagneticField(&Bdz[i][0],MagneticFieldStencil);
    }

    //magnetic field in the cell cented
    double B[3];
    double grad_absB[3],b_times_grad_absB[3],CurlB[3];
    double b_b_Curl_B[3],b_Curl_B;

    double AbsB2=AbsB*AbsB;

    grad_absB[0]=(Vector3D::Length(Bdx[1])-Vector3D::Length(Bdx[0]))/(2.0*delta); 
    grad_absB[1]=(Vector3D::Length(Bdy[1])-Vector3D::Length(Bdy[0]))/(2.0*delta);
    grad_absB[2]=(Vector3D::Length(Bdz[1])-Vector3D::Length(Bdz[0]))/(2.0*delta);

    Vector3D::CrossProduct(b_times_grad_absB,b,grad_absB);

    CurlB[0]=(Bdy[1][2]-Bdy[0][2])/delta-(Bdz[1][1]-Bdz[0][1])/delta;
    CurlB[1]=(Bdz[1][0]-Bdz[0][0])/delta-(Bdx[1][2]-Bdx[0][2])/delta;
    CurlB[2]=(Bdx[1][1]-Bdx[0][1])/delta-(Bdy[1][0]-Bdy[0][0])/delta; 

    b_Curl_B=Vector3D::DotProduct(b,CurlB);

    for (int i=0;i<3;i++) b_b_Curl_B[i]=b[i]*b_Curl_B;

    //save the vectors in the cell cented state vector 
    char *CellStatevector=cell->GetAssociatedDataBufferPointer(); 

    double *b_times_grad_absB_ptr=(double*)(CellStatevector+b_times_grad_absB_offset);
    double *CurlB_ptr=(double*)(CellStatevector+CurlB_offset); 
    double *b_b_Curl_B_ptr=(double*)(CellStatevector+b_b_Curl_B_offset);  

    if (AbsB==0.0) AbsB2=1.0,AbsB=1.0;

    for (int i=0;i<3;i++) {
      b_times_grad_absB_ptr[i]=b_times_grad_absB[i]/AbsB2;
      CurlB_ptr[i]=CurlB[i]/AbsB2;
      b_b_Curl_B[i]=b_b_Curl_B[i]/AbsB2;
    }
  };


  for (int iNode=0;iNode<PIC::DomainBlockDecomposition::nLocalBlocks;iNode++) {
    PIC::Mesh::cDataCenterNode *cell;
    int LocalCellNumber;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::DomainBlockDecomposition::BlockTable[iNode];

    if (node->block!=NULL) {
      for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        for (j=0;j<_BLOCK_CELLS_Y_;j++)  {
          for (i=0;i<_BLOCK_CELLS_X_;i++) {
            LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
            cell=node->block->GetCenterNode(LocalCellNumber);

            ProcessCell(cell,node);
          }
        }
      }
    }
  }
}

int SEP::RequestStaticCellData(int offset) {
  int size=0;

  b_times_grad_absB_offset=offset+size;
  size+=3*sizeof(double);

  CurlB_offset=offset+size; 
  size+=3*sizeof(double);

  b_b_Curl_B_offset=offset+size; 
  size+=3*sizeof(double);

  SEP::ParticleSource::ShockWave::ShockStateFlag_offset=offset+size; 
  size+=sizeof(double);

  return size;
}

void SEP::GetDriftVelocity(double *v_drift,double *x,double v_parallel,double v_perp,double ElectricCharge,double mass,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node) {
  PIC::InterpolationRoutines::CellCentered::cStencil Stencil;

  PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,Node,Stencil);
  SEP::GetDriftVelocity(v_drift,x,v_parallel,v_perp,ElectricCharge,mass,Node,Stencil);
}


void SEP::GetDriftVelocity(double *v_drift,double *x,double v_parallel,double v_perp,double ElectricCharge,double mass,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node,PIC::InterpolationRoutines::CellCentered::cStencil& Stencil) {
  double mu,mu2,t,Speed,p,t1,t2,t3;
  int idim;

  if (_SEP_MOVER_DRIFT_==_PIC_MODE_OFF_) {
    for (idim=0;idim<3;idim++) v_drift[idim]=0.0;
    return;
  }

  Speed=sqrt(v_parallel*v_parallel+v_perp*v_perp);
  mu=v_parallel/Speed;   
  mu2=mu*mu;

  switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
  case _PIC_MODE_OFF_:
    p=Speed*mass;
    break;
  case _PIC_MODE_ON_:
    p=Relativistic::Speed2Momentum(Speed,mass);
    break;
  }

  t=p*Speed/ElectricCharge;

  t1=0.5*(1.0+mu2)*t;
  t2=mu2*t;
  t3=0.5*(1.0-3.0*mu2)*t; 
  
  for (idim=0;idim<3;idim++) v_drift[idim]=0.0;  

  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    char *CellStateVector=Stencil.cell[iStencil]->GetAssociatedDataBufferPointer(); 

    double *b_times_grad_absB=(double*)(CellStateVector+b_times_grad_absB_offset);
    double *CurlB=(double*)(CellStateVector+CurlB_offset);
    double *b_b_Curl_B=(double*)(CellStateVector+b_b_Curl_B_offset);

    for (idim=0;idim<3;idim++) {
      v_drift[idim]+=Stencil.Weight[iStencil]*(t1*b_times_grad_absB[idim]+t2*CurlB[idim]+t3*b_b_Curl_B[idim]);
    } 
  }
}


  


/*
//=============================================================================
//Focused transport mover He-2019-AJL 
// a better implementation is in mover.cpp
int SEP::ParticleMover_HE_2019_AJL(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) { 
  PIC::ParticleBuffer::byte *ParticleData;
  int spec;

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  spec=PIC::ParticleBuffer::GetI(ParticleData);

auto GetCoefficients = [&] (double *x,double& dVsw_xdx,double& dVsw_ydy,double& dVsw_zdz,double& dAbsBdz,double& absB,double* b,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node,double l,double* Vsw) { 
  PIC::InterpolationRoutines::CellCentered::cStencil Stencil;
  int idim; 
  double B[3],ex[3],ey[3];

  PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,Node,Stencil);

  for (idim=0;idim<3;idim++) B[idim]=0.0,Vsw[idim]=0.0;

  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    double *ptr_b=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);
    double *ptr_v=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset);

    for (idim=0;idim<3;idim++) {
      B[idim]+=Stencil.Weight[iStencil]*ptr_b[idim];
      Vsw[idim]+=Stencil.Weight[iStencil]*ptr_v[idim];
    }
  }

  memcpy(b,B,3*sizeof(double));
  absB=Vector3D::Normalize(b);

  Vector3D::GetNormFrame(ex,ey,b); 

  double B_zplus=0.0,B_zminus=0.0,Vz_zplus=0.0,Vz_zminus=0.0;
  double Vx_xplus=0.0,Vx_xminus=0.0,Vy_yplus=0.0,Vy_yminus=0.0;
  double xtest[3],t[3]={0.0,0.0,0.0};
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* tNode;
  double dz=0.0,dy=0.0,dx=0.0;
  bool NodeAvailabilityFlag;

  ///z+
  for (idim=0;idim<3;idim++) xtest[idim]=x[idim]+l*b[idim];

  tNode=PIC::Mesh::mesh->findTreeNode(xtest,Node); 

  if (tNode==NULL) NodeAvailabilityFlag=false;
  else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

  if (NodeAvailabilityFlag==true) {
    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
    dz+=l;
  }
  else {
    PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(x,Node,Stencil);
  }
   
  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    double *ptr=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);

    for (idim=0;idim<3;idim++) t[idim]+=Stencil.Weight[iStencil]*ptr[idim];

    Vz_zplus+=Stencil.Weight[iStencil]*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset))[2]; 
  }
   
  B_zplus=sqrt(t[0]*t[0]+t[1]*t[1]+t[2]*t[2]); 
 
  //z-
  for (idim=0;idim<3;idim++) xtest[idim]=x[idim]-l*b[idim],t[idim]=0.0;

  tNode=PIC::Mesh::mesh->findTreeNode(xtest,Node);

  if (tNode==NULL) NodeAvailabilityFlag=false;
  else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

  if (NodeAvailabilityFlag==true) {
    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
    dz+=l;
  }
  else {
    PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(x,Node,Stencil);
  }

  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    double *ptr=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);

    for (idim=0;idim<3;idim++) t[idim]+=Stencil.Weight[iStencil]*ptr[idim];

    Vz_zminus+=Stencil.Weight[iStencil]*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset))[2];
  }

  B_zminus=sqrt(t[0]*t[0]+t[1]*t[1]+t[2]*t[2]);

  //y+
  for (idim=0;idim<3;idim++) xtest[idim]=x[idim]+l*ey[idim]; 

  tNode=PIC::Mesh::mesh->findTreeNode(xtest,Node);

  if (tNode==NULL) NodeAvailabilityFlag=false;
  else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

  if (NodeAvailabilityFlag==true) {
    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
    dy+=l;
  }
  else {
    PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(x,Node,Stencil);
  }


  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    Vy_yplus+=Stencil.Weight[iStencil]*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset))[1];
  }

  //y-
  for (idim=0;idim<3;idim++) xtest[idim]=x[idim]-l*ey[idim];

  tNode=PIC::Mesh::mesh->findTreeNode(xtest,Node);

  if (tNode==NULL) NodeAvailabilityFlag=false;
  else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

  if (NodeAvailabilityFlag==true) {
    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
    dy+=l;
  }
  else {
    PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(x,Node,Stencil);
  }

  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    Vy_yminus+=Stencil.Weight[iStencil]*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset))[1];
  }

  //x+
  for (idim=0;idim<3;idim++) xtest[idim]=x[idim]+l*ex[idim];

  tNode=PIC::Mesh::mesh->findTreeNode(xtest,Node);

  if (tNode==NULL) NodeAvailabilityFlag=false;
  else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

  if (NodeAvailabilityFlag==true) {
    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
    dx+=l;
  }
  else {
    PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(x,Node,Stencil);
  }

  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    Vx_xplus+=Stencil.Weight[iStencil]*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset))[0];
  }

  //x-
  for (idim=0;idim<3;idim++) xtest[idim]=x[idim]-l*ex[idim];

  tNode=PIC::Mesh::mesh->findTreeNode(xtest,Node);

  if (tNode==NULL) NodeAvailabilityFlag=false;
  else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

  if (NodeAvailabilityFlag==true) {
    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
    dx+=l;
  }
  else {
    PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(x,Node,Stencil);
  }

  for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
    Vx_xminus+=Stencil.Weight[iStencil]*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset))[0];
  }

  dVsw_xdx=(dx>0.0) ? (Vx_xplus-Vx_xminus)/dx : 0.0; 
  dVsw_ydy=(dy>0.0) ? (Vy_yplus-Vy_yminus)/dy : 0.0;
  dVsw_zdz=(dz>0.0) ? (Vz_zplus-Vz_zminus)/dz : 0.0;

  dAbsBdz=(dz>0.0) ? (B_zplus-B_zminus)/dz : 0.0; 
};  

  //first half step
  int idim;
  double p_init,p_middle,p_final,mu_init,mu_middle,mu_final,mu2;

  double v,x_middle[3],x_init[3],x_final[3];
  double l,dVsw_xdx,dVsw_ydy,dVsw_zdz,dAbsBdz,absB,b[3],Vsw[3];
  double mass=PIC::MolecularData::GetMass(spec);

  l=min((startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_,min((startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_,(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_));   

  PIC::ParticleBuffer::GetX(x_init,ParticleData);
  p_init=*((double*)(ParticleData+SEP::Offset::Momentum));
  mu_init=*((double*)(ParticleData+SEP::Offset::CosPitchAngle));

 
  GetCoefficients(x_init,dVsw_xdx,dVsw_ydy,dVsw_zdz,dAbsBdz,absB,b,startNode,l,Vsw);
 
  mu2=mu_init*mu_init;

  switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
  case _PIC_MODE_OFF_:
    v=p_init/mass;
    break;
  case _PIC_MODE_ON_:
     if (fabs(mu_init)>1.0E-10) {
       double p_tot,speed;

       p_tot=fabs(p_init/mu_init);
       speed=Relativistic::Momentum2Speed(p_tot,mass);
       v=speed*mu_init;
     }
     else v=0.0; 

    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is not found");
  }
     


  p_middle=p_init-0.5*dtTotal*p_init*(0.5*(1.0-mu2)*(dVsw_xdx+dVsw_ydy)+mu2*dVsw_zdz); 
  mu_middle=mu_init+0.5*dtTotal*0.5*(1-mu2)*(-v/absB*dAbsBdz+mu_init*(dVsw_xdx+dVsw_ydy-2.0*dVsw_zdz));

  if (mu_middle>1.0) mu_middle=1.0;
  else if (mu_middle<-1.0) mu_middle=-1.0; 

  for (idim=0;idim<3;idim++) x_middle[idim]=x_init[idim]+0.5*dtTotal*v*b[idim];

  //second half
  startNode=PIC::Mesh::mesh->findTreeNode(x_middle,startNode);

  bool trajectory_teminated=false;

  if ((startNode==NULL)||(x_middle[0]*x_middle[0]+x_middle[1]*x_middle[1]+x_middle[2]*x_middle[2]<_SUN__RADIUS_*_SUN__RADIUS_)) trajectory_teminated=true;
  else if (startNode->IsUsedInCalculationFlag==false) trajectory_teminated=true;

if (trajectory_teminated==true) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
}

  GetCoefficients(x_middle,dVsw_xdx,dVsw_ydy,dVsw_zdz,dAbsBdz,absB,b,startNode,l,Vsw);


  switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
  case _PIC_MODE_OFF_:
    v=p_middle/mass;
    break;
  case _PIC_MODE_ON_:
     if (fabs(mu_init)>1.0E-10) {
       double p_tot,speed;

       p_tot=fabs(p_middle/mu_middle);
       speed=Relativistic::Momentum2Speed(p_tot,mass);
       v=speed*mu_middle;
     }
     else v=0.0;

    break;
  default:
    exit(__LINE__,__FILE__,"Error: the option is not found");
  }



  mu2=mu_middle*mu_middle;
  p_final=p_init-dtTotal*p_middle*(0.5*(1.0-mu2)*(dVsw_xdx+dVsw_ydy)+mu2*dVsw_zdz);
  mu_final=mu_init+dtTotal*0.5*(1-mu2)*(-v/absB*dAbsBdz+mu_middle*(dVsw_xdx+dVsw_ydy-2.0*dVsw_zdz));

  if (mu_final>1.0) mu_final=1.0;
  else if (mu_final<-1.0) mu_final=-1.0;

  

  for (idim=0;idim<3;idim++) x_final[idim]=x_init[idim]+dtTotal*v*b[idim];

  startNode=PIC::Mesh::mesh->findTreeNode(x_final,startNode);

  if ((startNode==NULL)||(x_final[0]*x_final[0]+x_final[1]*x_final[1]+x_final[2]*x_final[2]<_SUN__RADIUS_*_SUN__RADIUS_)) trajectory_teminated=true;
  else if (startNode->IsUsedInCalculationFlag==false) trajectory_teminated=true;

if (trajectory_teminated==true) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
}




  PIC::ParticleBuffer::SetX(x_final,ParticleData); 
  *((double*)(ParticleData+SEP::Offset::Momentum))=p_final;
  *((double*)(ParticleData+SEP::Offset::CosPitchAngle))=mu_final; 


  PIC::Mesh::cDataBlockAMR *block;
  int i,j,k;

  if (PIC::Mesh::mesh->FindCellIndex(x_final,i,j,k,startNode,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  if ((block=startNode->block)==NULL) {
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }


  #if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;
  #else 
    exit(__LINE__,__FILE__,"Error: the option is not defined");
  #endif


  return _PARTICLE_MOTION_FINISHED_;
}
*/

int SEP::ParticleMover_BOROVIKOV_2019_ARXIV(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  struct cGetCoefficientManager {
    double Dln_B_Dt,dln_B_ds,Dln_rho_B_Dt,b_Du_Dt;
    double* x0;
    double l,t,b[3];
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node;
  }; 

  double *sphereX0,InnerBoundaryRadius;
  SEP::InnerBoundary->GetSphereGeometricalParameters(sphereX0,InnerBoundaryRadius);

  auto GetCoefficients = [&] (cGetCoefficientManager* mng,double time_offset) {
    PIC::InterpolationRoutines::CellCentered::cStencil Stencil;
    int idim;
    double B[3]={0.0,0.0,0.0},Vsw[3]={0.0,0.0,0.0};

    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(mng->x0,mng->Node,Stencil);

    for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
      double *ptr_b=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);
      double *ptr_v=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset);

      for (idim=0;idim<3;idim++) {
        B[idim]+=Stencil.Weight[iStencil]*ptr_b[idim];
        Vsw[idim]+=Stencil.Weight[iStencil]*ptr_v[idim];
      }
    }

    memcpy(mng->b,B,3*sizeof(double));
    Vector3D::Normalize(mng->b);

    //d/ds 
    //s+
    double ds=0.0,xtest[3],tB[3],B_splus,B_sminus;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* tNode;
    bool NodeAvailabilityFlag; 

    for (idim=0;idim<3;idim++) xtest[idim]=mng->x0[idim]+mng->l*mng->b[idim],tB[idim]=0.0;

    tNode=PIC::Mesh::mesh->findTreeNode(xtest,mng->Node);

    if (tNode==NULL) NodeAvailabilityFlag=false;
    else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

    if ((NodeAvailabilityFlag==true)&&(xtest[0]*xtest[0]+xtest[1]*xtest[1]+xtest[2]*xtest[2]<InnerBoundaryRadius*InnerBoundaryRadius))  {
      PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
      ds+=mng->l;
    }
    else {
      PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(mng->x0,mng->Node,Stencil);
    }

    for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
      double *ptr=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);

      for (idim=0;idim<3;idim++) tB[idim]+=Stencil.Weight[iStencil]*ptr[idim];
    }

    B_splus=sqrt(tB[0]*tB[0]+tB[1]*tB[1]+tB[2]*tB[2]);

    //s-
    for (idim=0;idim<3;idim++) xtest[idim]=mng->x0[idim]-mng->l*mng->b[idim],tB[idim]=0.0;

    tNode=PIC::Mesh::mesh->findTreeNode(xtest,mng->Node);

    if (tNode==NULL) NodeAvailabilityFlag=false;
    else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

    if ((NodeAvailabilityFlag==true)&&(xtest[0]*xtest[0]+xtest[1]*xtest[1]+xtest[2]*xtest[2]<InnerBoundaryRadius*InnerBoundaryRadius)) {
      PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
      ds+=mng->l;
    }
    else {
      PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(mng->x0,mng->Node,Stencil);
    }

    for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
      double *ptr=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);
      
      for (idim=0;idim<3;idim++) tB[idim]+=Stencil.Weight[iStencil]*ptr[idim];
    }

    B_sminus=sqrt(tB[0]*tB[0]+tB[1]*tB[1]+tB[2]*tB[2]);

    ///Dt+
    double B_tplus,rho_B_tplus,B_tminus,rho_B_tminus,rho_tplus=0.0,rho_tminus=0.0,Vsw_tplus[3]={0.0,0.0,0.0},Vsw_tminus[3]={0.0,0.0,0.0}; 

    for (idim=0;idim<3;idim++) xtest[idim]=mng->x0[idim]+mng->t*Vsw[idim],tB[idim]=0.0;

    tNode=PIC::Mesh::mesh->findTreeNode(xtest,mng->Node);

    if (tNode==NULL) NodeAvailabilityFlag=false;
    else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

    if ((NodeAvailabilityFlag==true)&&(xtest[0]*xtest[0]+xtest[1]*xtest[1]+xtest[2]*xtest[2]<InnerBoundaryRadius*InnerBoundaryRadius)) {
      PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
    }
    else {
      PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(mng->x0,mng->Node,Stencil);
    }

    for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
      double *ptr_b=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);
      double *ptr_v=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset);

      for (idim=0;idim<3;idim++) {
        tB[idim]+=Stencil.Weight[iStencil]*ptr_b[idim];
        Vsw_tplus[idim]+=Stencil.Weight[iStencil]*ptr_v[idim];
      }

      rho_tplus+=Stencil.Weight[iStencil]*(*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::PlasmaNumberDensityOffset)));
    }

    B_tplus=sqrt(tB[0]*tB[0]+tB[1]*tB[1]+tB[2]*tB[2]);
    

    ///Dt-
    for (idim=0;idim<3;idim++) xtest[idim]=mng->x0[idim]-mng->t*Vsw[idim],tB[idim]=0.0;

    tNode=PIC::Mesh::mesh->findTreeNode(xtest,mng->Node);

    if (tNode==NULL) NodeAvailabilityFlag=false;
    else NodeAvailabilityFlag=(tNode->IsUsedInCalculationFlag==true) ? true : false;

    if ((NodeAvailabilityFlag==true)&&(xtest[0]*xtest[0]+xtest[1]*xtest[1]+xtest[2]*xtest[2]<InnerBoundaryRadius*InnerBoundaryRadius)) {
      PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xtest,tNode,Stencil);
    }
    else {
      PIC::InterpolationRoutines::CellCentered::Constant::InitStencil(mng->x0,mng->Node,Stencil);
    }

    for (int iStencil=0;iStencil<Stencil.Length;iStencil++) {
      double *ptr_b=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);
      double *ptr_v=(double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::BulkVelocityOffset);

      for (idim=0;idim<3;idim++) {
        tB[idim]+=Stencil.Weight[iStencil]*ptr_b[idim];
        Vsw_tminus[idim]+=Stencil.Weight[iStencil]*ptr_v[idim];
      }

      rho_tminus+=Stencil.Weight[iStencil]*(*((double*)(Stencil.cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::PlasmaNumberDensityOffset)));
    }

    B_tminus=sqrt(tB[0]*tB[0]+tB[1]*tB[1]+tB[2]*tB[2]);


    if ((B_tplus>0.0)&&(B_tminus>0.0)&&(B_splus>0.0)&&(B_sminus>0.0)) {
      mng->Dln_B_Dt=log(B_tplus/B_tminus)/(2.0*mng->t);
      mng->dln_B_ds=(ds>0.0) ? log(B_splus/B_sminus)/ds : 0.0;

      if ((rho_tplus>0.0)&&(rho_tminus>0.0)) {
        mng->Dln_rho_B_Dt=log((rho_tplus/B_tplus)/(rho_tminus/B_tminus))/(2.0*mng->t),
        mng->b_Du_Dt=(mng->b[0]*(Vsw_tplus[0]-Vsw_tminus[0])+mng->b[1]*(Vsw_tplus[1]-Vsw_tminus[1])+mng->b[2]*(Vsw_tplus[2]-Vsw_tminus[2]))/(2.0*mng->t);
      }
      else {
        mng->Dln_rho_B_Dt=0.0;
        mng->b_Du_Dt=0.0;
      }
    }
    else {
      mng->Dln_B_Dt=0.0;
      mng->dln_B_ds=0.0;

      mng->b_Du_Dt=0.0;
      mng->Dln_rho_B_Dt=0.0; 
    } 
  };



  cGetCoefficientManager mng;

  mng.l=min((startNode->xmax[0]-startNode->xmin[0])/_BLOCK_CELLS_X_,min((startNode->xmax[1]-startNode->xmin[1])/_BLOCK_CELLS_Y_,(startNode->xmax[2]-startNode->xmin[2])/_BLOCK_CELLS_Z_));
  mng.t=0.25*dtTotal;

  //first half step
  double v_par_init,v_par_middle;
  double p_par_init,p_par_middle,p_par_final,p_norm_init,p_norm_middle,p_norm_final; 
  double x_init[3],x_middle[3],x_final[3];
  double p,m_i,mass,speed;
  int idim,spec;
  bool trajectory_teminated=false;

  
  PIC::ParticleBuffer::byte *ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr); 

  spec=PIC::ParticleBuffer::GetI(ParticleData);
  mass=PIC::MolecularData::GetMass(spec); 

  PIC::ParticleBuffer::GetX(x_init,ParticleData);
  p_norm_init=*((double*)(ParticleData+SEP::Offset::p_norm));
  p_par_init=*((double*)(ParticleData+SEP::Offset::p_par));

  p=sqrt(p_par_init*p_par_init+p_norm_init*p_norm_init);
  speed=Relativistic::Momentum2Speed(p,mass);
  m_i=mass*Relativistic::GetGamma(speed);

  v_par_init=speed*p_par_init/p;

  mng.x0=x_init;
  mng.Node=startNode;
  GetCoefficients(&mng,0.0);

  p_norm_middle=p_norm_init+0.25*dtTotal*(mng.Dln_B_Dt+mng.dln_B_ds*v_par_init)*p_norm_init; 
  p_par_middle=p_par_init+0.5*dtTotal*(-p_norm_init*p_norm_init/(2.0*m_i)*mng.dln_B_ds+mng.Dln_rho_B_Dt*p_par_init-m_i*mng.b_Du_Dt);  
  
  for (idim=0;idim<3;idim++) x_middle[idim]=x_init[idim]+0.5*dtTotal*v_par_init*mng.b[idim]; 

  startNode=PIC::Mesh::mesh->findTreeNode(x_middle,startNode);

  if ((startNode==NULL)||(x_middle[0]*x_middle[0]+x_middle[1]*x_middle[1]+x_middle[2]*x_middle[2]<InnerBoundaryRadius*InnerBoundaryRadius)) trajectory_teminated=true;
  else if (startNode->IsUsedInCalculationFlag==false) trajectory_teminated=true;

  if (trajectory_teminated==true) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
  }
  
  //send half step
  p=sqrt(p_par_middle*p_par_middle+p_norm_middle*p_norm_middle);
  speed=Relativistic::Momentum2Speed(p,mass);
  m_i=mass*Relativistic::GetGamma(speed);

  v_par_middle=speed*p_par_middle/p;

  mng.x0=x_middle;
  mng.Node=startNode; 
  GetCoefficients(&mng,0.5*dtTotal);

  p_norm_final=p_norm_init+0.5*dtTotal*(mng.Dln_B_Dt+mng.dln_B_ds*v_par_middle)*p_norm_middle;
  p_par_final=p_par_init+dtTotal*(-p_norm_middle*p_norm_middle/(2.0*m_i)*mng.dln_B_ds+mng.Dln_rho_B_Dt*p_par_middle-m_i*mng.b_Du_Dt);
 

  for (idim=0;idim<3;idim++) x_final[idim]=x_init[idim]+dtTotal*v_par_middle*mng.b[idim];

  startNode=PIC::Mesh::mesh->findTreeNode(x_final,startNode);

  if ((startNode==NULL)||(x_final[0]*x_final[0]+x_final[1]*x_final[1]+x_final[2]*x_final[2]<InnerBoundaryRadius*InnerBoundaryRadius)) trajectory_teminated=true;
  else if (startNode->IsUsedInCalculationFlag==false) trajectory_teminated=true;

  if (trajectory_teminated==true) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_LEFT_THE_DOMAIN_;
  }


  PIC::ParticleBuffer::SetX(x_final,ParticleData);
  *((double*)(ParticleData+SEP::Offset::p_par))=p_par_final;
  *((double*)(ParticleData+SEP::Offset::p_norm))=p_norm_final;


  PIC::Mesh::cDataBlockAMR *block;
  int i,j,k;

  if (PIC::Mesh::mesh->FindCellIndex(x_final,i,j,k,startNode,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  if ((block=startNode->block)==NULL) {
    exit(__LINE__,__FILE__,"Error: the block is empty. Most probably hte tiime step is too long");
  }


  #if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  tempFirstCellParticlePtr=block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;
  #else
    exit(__LINE__,__FILE__,"Error: the option is not defined");
  #endif


  return _PARTICLE_MOTION_FINISHED_;
}



