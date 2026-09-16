//functions describing injection at shock

#include "sep.h"

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
#include "amps2swmf.h" 
#endif

int SEP::ParticleSource::ShockWave::ShockStateFlag_offset=-1;
double SEP::ParticleSource::ShockWave::MaxLimitCompressionRatio=3.0;

int SEP::FieldLine::MagneticTubeRadiusMode=SEP::FieldLine::MagneticTubeRadiusModeR2;

//condition for presence of a shock in a given cell
bool SEP::ParticleSource::ShockWave::IsShock(PIC::Mesh::cDataCenterNode *CenterNode) {
  double density_current,density_last;
  char *SamplingBuffer; 
  bool flag=false;
  
  const double min_ratio=1.2;

  SamplingBuffer=CenterNode->GetAssociatedDataBufferPointer();

  density_current=*((double*)(SamplingBuffer+PIC::CPLR::SWMF::PlasmaNumberDensityOffset));
  density_last=*((double*)(SamplingBuffer+PIC::CPLR::SWMF::PlasmaNumberDensityOffset_last)); 

  if ((density_last>0.0)&&(density_current>0.0)) {
    flag=(density_current/density_last>min_ratio) ? true : false;
  }

  #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  flag=false;

  if ((PIC::CPLR::SWMF::PlasmaDivUdXOffset>0)&&(AMPS2SWMF::DivUdXShockLocationThrehold>0.0)) {
    flag=(fabs(*((double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::PlasmaDivUdXOffset)))>AMPS2SWMF::DivUdXShockLocationThrehold); 
  }
  #endif


  if (flag==true) {
    *((double*)(SamplingBuffer+ShockStateFlag_offset))=1.0;
  }
  else {
    *((double*)(SamplingBuffer+ShockStateFlag_offset))=0.0;
  }

  return flag;
} 

void SEP::ParticleSource::ShockWave::Output::Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode) {
  double *ptr_flag=(double*)(CenterNode->GetAssociatedDataBufferPointer()+SEP::ParticleSource::ShockWave::ShockStateFlag_offset);

  *ptr_flag=0.0;

  for (int i=0;i<nInterpolationCoeficients;i++) { 
    bool flag=SEP::ParticleSource::ShockWave::IsShock(InterpolationList[i]);

    if (flag==true) {
      *ptr_flag=1.0;
      break;
    }
  }
} 

void SEP::ParticleSource::ShockWave::Output::PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode) {
  double t;

  bool gather_print_data=false;

  if (pipe==NULL) gather_print_data=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_print_data=true;

  if (gather_print_data==true) {
    t=*((double*)(CenterNode->GetAssociatedDataBufferPointer()+SEP::ParticleSource::ShockWave::ShockStateFlag_offset));
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

    fprintf(fout," %e ",t);
  }
  else {
    pipe->send(t);
  }
}


void SEP::ParticleSource::ShockWave::Output::PrintVariableList(FILE* fout,int DataSetNumber) {
  fprintf(fout,", \"Is shock\"");
}

void SEP::ParticleSource::PopulateAllFieldLines() {
  namespace FL = PIC::FieldLine;
  int iFieldLine;

  for (iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    PopulateFieldLine(iFieldLine);
  } 
}

void SEP::ParticleSource::PopulateFieldLine(int iFieldLine) { 
  namespace FL = PIC::FieldLine;
  int iSegment; 
  FL::cFieldLineSegment* Segment;


  for (iSegment=0,Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment(); iSegment<FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber(); iSegment++,Segment=Segment->GetNext()) {
    double t_sw_end,t_sw_begin,v_sw_end[3],v_sw_begin[3],n_sw_end,n_sw_begin,v[3],NumberDensity,Temperature,Volume;
    auto VertexBegin=Segment->GetBegin();
    auto VertexEnd=Segment->GetEnd();
  
    VertexBegin->GetDatum(FL::DatumAtVertexPlasmaTemperature,&t_sw_begin);
    VertexBegin->GetDatum(FL::DatumAtVertexPlasmaDensity,&n_sw_begin);
    VertexBegin->GetPlasmaVelocity(v_sw_begin);

    VertexEnd->GetDatum(FL::DatumAtVertexPlasmaTemperature,&t_sw_end);
    VertexEnd->GetDatum(FL::DatumAtVertexPlasmaDensity,&n_sw_end);
    VertexEnd->GetPlasmaVelocity(v_sw_end);

    NumberDensity=0.5*(n_sw_begin+n_sw_end);
    Temperature=0.5*(t_sw_begin+t_sw_end);

    Volume=SEP::FieldLine::GetSegmentVolume(Segment,iFieldLine);  

    for (int idim=0;idim<3;idim++) v[idim]=0.5*(v_sw_begin[idim]+v_sw_end[idim]);

    PIC::FieldLine::PopulateSegment(_H_PLUS_SPEC_,NumberDensity,Temperature,v,Volume,iSegment,iFieldLine,200);
 }
}

//===================================================================================
//calcualte volume associated with a segment of the field line 
double SEP::FieldLine::MagneticTubeRadius(double *x,int iFieldLine) {
  namespace FL = PIC::FieldLine;
  double *x0,res;

  //1. the radius of the magnetic tube as the first vertex from the beginnig of the filed line is ONE
  //2. the radius increases as R^2 
  x0=FL::FieldLinesAll[iFieldLine].GetFirstSegment()->GetBegin()->GetX(); 

  switch (MagneticTubeRadiusMode) {
  case MagneticTubeRadiusModeConst:
    res=Vector3D::Length(x0);
    break;
  default: 
    res=Vector3D::DotProduct(x,x)/Vector3D::DotProduct(x0,x0);
  }

  return res;
}

double SEP::FieldLine::MagneticTubeRadius(PIC::FieldLine::cFieldLineVertex* Vertex,int iFieldLine) {
  return MagneticTubeRadius(Vertex->GetX(),iFieldLine);
}

double SEP::FieldLine::GetSegmentVolume(PIC::FieldLine::cFieldLineSegment* Segment,int iFieldLine) {
  double r0,r1;

  r0=MagneticTubeRadius(Segment->GetBegin(),iFieldLine);
  r1=MagneticTubeRadius(Segment->GetEnd(),iFieldLine);

  return Pi*(r0*r0+r0*r1+r1*r1)*Segment->GetLength()/3.0;
}
 


