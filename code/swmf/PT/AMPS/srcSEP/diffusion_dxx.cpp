
#include <cmath>
#include <ctgmath>

#include "sep.h"
#include "quadrature.h"

//static variables from c_D_x_x
//template<class T> double SEP::Diffusion::cD_x_x<T>::speed;

/*template<class T>
double SEP::Diffusion::cD_x_x<T>::p=0.0;

template<class T>
double SEP::Diffusion::cD_x_x<T>::W[2]={0.0,0.0};

template<class T>
double SEP::Diffusion::cD_x_x<T>::AbsB=0.0;

template<class T>
double SEP::Diffusion::cD_x_x<T>::xLocation[3]={0.0,0.0,0.0};

template<class T>
double SEP::Diffusion::cD_x_x<T>::vAlfven=0.0;

template<class T>
double SEP::Diffusion::cD_x_x<T>::B[3]={0.0,0.0,0.0};
    
template<class T>
PIC::FieldLine::cFieldLineSegment*  SEP::Diffusion::cD_x_x<T>::Segment=NULL;*/

namespace DxxInternalNumerics {
  thread_local double v;
  thread_local int spec;
  thread_local double FieldLineCoord;
  thread_local PIC::FieldLine::cFieldLineSegment *Segment;

  double Integrant(double *mu) {
    double D,dD_dmu,vParallel,vNorm;
    double t=1.0-mu[0]*mu[0];

    vParallel=v*mu[0];
    vNorm=v*sqrt(t); 

    SEP::Diffusion::GetPitchAngleDiffusionCoefficient(D,dD_dmu,mu[0],vParallel,vNorm,spec,FieldLineCoord,Segment);

    if (D==0.0) {
      //for debugging: catch the issue in the debugger by pacing a breat point in calculation of the D_mu_mu
      SEP::Diffusion::GetPitchAngleDiffusionCoefficient(D,dD_dmu,mu[0],vParallel,vNorm,spec,FieldLineCoord,Segment);
    }

    return t*t/D;
  }
} 

void SEP::Diffusion::GetDxx(double& D,double &dDxx_dx,double v,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
  namespace FL = PIC::FieldLine;
  double s,ds,D1,D0;

  DxxInternalNumerics::v=v;
  DxxInternalNumerics::spec=spec;
  DxxInternalNumerics::FieldLineCoord=FieldLineCoord;
  DxxInternalNumerics::Segment=Segment;

  //as Dxx depends only on |\mu|, the integration limit is chabnged fomr (-1,1) to (0+something very small to exclude 0 from integraiont), 1);
  double xmin[]={0.00001};
  double xmax[]={1.0};

  if (v<1.0E6) {     
    D=2.0*v*v/8.0*Quadrature::Gauss::Cube::GaussLegendre(1,4,DxxInternalNumerics::Integrant,xmin,xmax);
  } 
  else if (v<1.0E7) {
    D=2.0*v*v/8.0*Quadrature::Gauss::Cube::GaussLegendre(1,4,DxxInternalNumerics::Integrant,xmin,xmax);
  }
  else {
    D=2.0*v*v/8.0*Quadrature::Gauss::Cube::GaussLegendre(1,6,DxxInternalNumerics::Integrant,xmin,xmax);  
  }

  double DTEST,DTEST1,DDTEST1;

  {
  FL::cFieldLineVertex* VertexBegin=Segment->GetBegin();
  FL::cFieldLineVertex* VertexEnd=Segment->GetEnd();
  double *x0,*x1;
  double w0,w1;

    x0=VertexBegin->GetX();
    x1=VertexEnd->GetX();

      w1=fmod(FieldLineCoord,1);
  w0=1.0-w1; 


  double XTEST[3];

  for (int idim=0;idim<3;idim++) {
    double t;

    t=w0*x0[idim]+w1*x1[idim];
    XTEST[idim]=t;

  }

  DTEST = DLT::calculate_Dxx(Vector3D::Length(XTEST),v);

  QLT::calculateAtHeliocentricDistance(DTEST,DDTEST1,Vector3D::Length(XTEST),v);

  }

  ds=Segment->GetLength();

  DxxInternalNumerics::FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,ds);
  DxxInternalNumerics::Segment=FL::FieldLinesAll[iFieldLine].GetSegment(DxxInternalNumerics::FieldLineCoord);

  if (DxxInternalNumerics::FieldLineCoord<0.0) {
    dDxx_dx=0.0;
    return;
  }
  else {
    D1=2.0*v*v/8.0*Quadrature::Gauss::Cube::GaussLegendre(1,4,DxxInternalNumerics::Integrant,xmin,xmax);
  }

  DxxInternalNumerics::FieldLineCoord=FL::FieldLinesAll[iFieldLine].move(FieldLineCoord,-ds);
  DxxInternalNumerics::Segment=FL::FieldLinesAll[iFieldLine].GetSegment(DxxInternalNumerics::FieldLineCoord);

  if (DxxInternalNumerics::FieldLineCoord<0.0) {
    dDxx_dx=0.0;
    return;
  }
  else {
    D0=2.0*v*v/8.0*Quadrature::Gauss::Cube::GaussLegendre(1,4,DxxInternalNumerics::Integrant,xmin,xmax);
  }

  dDxx_dx=(D1-D0)/(2.0*ds);
} 

//====================================================================================================
//calcualte particle's mean free path
double SEP::Diffusion::GetMeanFreePath(double v,int spec,double FieldLineCoord,PIC::FieldLine::cFieldLineSegment *Segment,int iFieldLine) {
  double D,dDxx_dx; 

  GetDxx(D,dDxx_dx,v,spec,FieldLineCoord,Segment,iFieldLine);
  return 3.0*D/v;
}
