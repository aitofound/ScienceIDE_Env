#include "sep.h"
#include "amps2swmf.h"

void SEP::FieldLine::OutputBackgroundData(char* fname, int iFieldLine) {
  namespace FL = PIC::FieldLine;
  FILE *fout=fopen(fname,"w");
  double s=0.0,*x0,*x1,FieldLineCoord=0.0;
  FL::cFieldLineSegment *Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();

  const int nEnergyLevels=3;
  double EnergyLevelTable[nEnergyLevels]={10.0*MeV2J,100.0*MeV2J,300.0*MeV2J};

  fprintf(fout,"VARIABLES=\"S[AU]\", \"Heliocentric Distance [AU]\", \"AbsB (Parker) [T]\"");

  if (_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_) {
    fprintf(fout,", \"AbsB [T]\", \"Number Density\", \"Speed\", \"Temp\", \"Pressure\", \"w+\", \"w-\"");
  }

  //output mean free path 
  for (int i=0;i<nEnergyLevels;i++) { 
    fprintf(fout,", \"Lambda(QLT,%.2e Mev)\",  \"Lambda(QLT1,Parker,%.2e Mev)\" ",EnergyLevelTable[i]/MeV2J,EnergyLevelTable[i]/MeV2J);

    if (_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__SWMF_) fprintf(fout,", \"Lambda(QLT1,SWMF,%.2e Mev)\" ",EnergyLevelTable[i]/MeV2J);       

    fprintf(fout,", \"Lambda(Tenishev2005AIAA,%.2e Mev)\",  \"Lambda(Chen2024AA,%.2e Mev)\" ",EnergyLevelTable[i]/MeV2J,EnergyLevelTable[i]/MeV2J);
    fprintf(fout,", \"Lambda(Jokopii1966AJ,Fraction,%.2e Mev)\", \"Lambda(Jokopii1966AJ,AWSOM,%.2e Mev)\" ",EnergyLevelTable[i]/MeV2J,EnergyLevelTable[i]/MeV2J); 
  }

  //output Dxx
  for (int i=0;i<nEnergyLevels;i++) {
    fprintf(fout,", \"Dxx(QLT,%.2e Mev)\",  \"Dxx(QLT1,Parker,%.2e Mev)\" ",EnergyLevelTable[i]/MeV2J,EnergyLevelTable[i]/MeV2J);

    if (_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__SWMF_) fprintf(fout,", \"Dxx(QLT1,SWMF,%.2e Mev)\" ",EnergyLevelTable[i]/MeV2J);

    fprintf(fout,", \"Dxx(Tenishev2005AIAA,%.2e Mev)\",  \"Dxx(Chen2024AA,%.2e Mev)\" ",EnergyLevelTable[i]/MeV2J,EnergyLevelTable[i]/MeV2J);
    fprintf(fout,", \"Dxx(Jokopii1966AJ,Fraction,%.2e Mev)\", \"Dxx(Jokopii1966AJ,AWSOM,%.2e Mev)\" ",EnergyLevelTable[i]/MeV2J,EnergyLevelTable[i]/MeV2J);
  } 


  fprintf(fout,"\n");

  auto GetMeanFreePathVector = [&] (double r,double e,double Speed,double FieldLineCoord,FL::cFieldLineSegment *Segment,vector<double>& MeanFreePath) {
    double dxx,AbsB;

    MeanFreePath.push_back(QLT::calculateMeanFreePath(r,Speed));

    AbsB=SEP::ParkerSpiral::GetAbsB(r); 
    MeanFreePath.push_back(QLT1::calculateMeanFreePath(r,Speed,AbsB));

    if (_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__SWMF_) {
       AbsB=SEP::FieldLineData::GetAbsB(FieldLineCoord,Segment,iFieldLine);
       MeanFreePath.push_back(QLT1::calculateMeanFreePath(r,Speed,AbsB));
    }

    MeanFreePath.push_back(SEP::Scattering::Tenishev2005AIAA::lambda0*
      pow(e/GeV2J,SEP::Scattering::Tenishev2005AIAA::alpha)*
      pow(r/_AU_,SEP::Scattering::Tenishev2005AIAA::beta));

    dxx=SEP::Diffusion::Chen2024AA::GetDxx(r,e);
    MeanFreePath.push_back(3.0*dxx/Speed); //Eq, 15, Liu-2024-arXiv; 

    //Jokopii1966AJ:
    auto fptr=SEP::Diffusion::GetPitchAngleDiffusionCoefficient;
    auto mode=SEP::Diffusion::Jokopii1966AJ::Mode;
    double dDxx_dx; 

    SEP::Diffusion::GetPitchAngleDiffusionCoefficient=SEP::Diffusion::Jokopii1966AJ::GetPitchAngleDiffusionCoefficient;

    SEP::Diffusion::Jokopii1966AJ::Mode=SEP::Diffusion::Jokopii1966AJ::_fraction;
    SEP::Diffusion::GetDxx(dxx,dDxx_dx,Speed,0,FieldLineCoord,Segment,iFieldLine);
    MeanFreePath.push_back(3.0*dxx/Speed);

    SEP::Diffusion::Jokopii1966AJ::Mode=SEP::Diffusion::Jokopii1966AJ::_awsom;
    SEP::Diffusion::GetDxx(dxx,dDxx_dx,Speed,0,FieldLineCoord,Segment,iFieldLine);
    MeanFreePath.push_back(3.0*dxx/Speed); 

    SEP::Diffusion::Jokopii1966AJ::Mode=mode;
    SEP::Diffusion::GetPitchAngleDiffusionCoefficient=fptr;
  }; 

  auto GetDxxVector = [&] (double r,double e,double Speed,double FieldLineCoord,FL::cFieldLineSegment *Segment,vector<double>& Dxx) {
    double dxx,AbsB;

    Dxx.push_back(QLT::calculateMeanFreePath(r,Speed)*Speed/3.0);

    AbsB=SEP::ParkerSpiral::GetAbsB(r); 
    Dxx.push_back(QLT1::calculateMeanFreePath(r,Speed,AbsB)*Speed/3.0);

    if (_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__SWMF_) {
       AbsB=SEP::FieldLineData::GetAbsB(FieldLineCoord,Segment,iFieldLine);
       Dxx.push_back(QLT1::calculateMeanFreePath(r,Speed,AbsB)*Speed/3.0);
    }

    Dxx.push_back(SEP::Scattering::Tenishev2005AIAA::lambda0*
      pow(e/GeV2J,SEP::Scattering::Tenishev2005AIAA::alpha)*
      pow(r/_AU_,SEP::Scattering::Tenishev2005AIAA::beta)*Speed/3.0);

    Dxx.push_back(SEP::Diffusion::Chen2024AA::GetDxx(r,e));

    //Jokopii1966AJ:
    auto fptr=SEP::Diffusion::GetPitchAngleDiffusionCoefficient;
    auto mode=SEP::Diffusion::Jokopii1966AJ::Mode;
    double dDxx_dx;

    SEP::Diffusion::GetPitchAngleDiffusionCoefficient=SEP::Diffusion::Jokopii1966AJ::GetPitchAngleDiffusionCoefficient;

    SEP::Diffusion::Jokopii1966AJ::Mode=SEP::Diffusion::Jokopii1966AJ::_fraction;
    SEP::Diffusion::GetDxx(dxx,dDxx_dx,Speed,0,FieldLineCoord,Segment,iFieldLine);
    Dxx.push_back(dxx);

    SEP::Diffusion::Jokopii1966AJ::Mode=SEP::Diffusion::Jokopii1966AJ::_awsom;
    SEP::Diffusion::GetDxx(dxx,dDxx_dx,Speed,0,FieldLineCoord,Segment,iFieldLine);
    Dxx.push_back(dxx); 

    SEP::Diffusion::Jokopii1966AJ::Mode=mode;
    SEP::Diffusion::GetPitchAngleDiffusionCoefficient=fptr;
  };

  auto OutputVertex = [&] (double s,double r,FL::cFieldLineVertex* v,double FieldLineCoord,FL::cFieldLineSegment *Segment) {
    double AbsB_Parker;

    AbsB_Parker=SEP::ParkerSpiral::GetAbsB(r); 
    fprintf(fout,"%e %e %e ",s/_AU_,r/_AU_,AbsB_Parker);

    if (_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_) {
      double pv[3],n,t,p,*W,AbsB;

      AbsB=Vector3D::Length(v->GetMagneticField());
      v->GetPlasmaVelocity(pv);
      v->GetPlasmaDensity(n);
      v->GetPlasmaTemperature(t);
      v->GetPlasmaPressure(p);
      W=v->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);

      fprintf(fout,"%e %e %e %e %e %e %e ", AbsB,n,Vector3D::Length(pv),t,p,W[0],W[1]);
    }

    double speed;
    vector<double> MeanFreePath,Dxx;

    for (int i=0;i<nEnergyLevels;i++) {
      speed=Relativistic::E2Speed(EnergyLevelTable[i],_H__MASS_); 
      GetMeanFreePathVector(r,EnergyLevelTable[i],speed,FieldLineCoord,Segment,MeanFreePath);
      GetDxxVector(r,EnergyLevelTable[i],speed,FieldLineCoord,Segment,Dxx);
    }  

    for (auto t : MeanFreePath) fprintf(fout,"%e ",t);
    for (auto t : Dxx) fprintf(fout,"%e ",t); 

    fprintf(fout,"\n");
  };  

  x0=Segment->GetBegin()->GetX(); 

  for (;Segment!=NULL;Segment=Segment->GetNext()) {
    x1=Segment->GetBegin()->GetX();
    s+=Vector3D::Distance(x0,x1);
    x0=x1;

    OutputVertex(s,Vector3D::Length(x1),Segment->GetBegin(),FieldLineCoord,Segment);
    FieldLineCoord+=1;

    if (Segment->GetNext()==NULL) {
       x1=Segment->GetEnd()->GetX();
       s+=Vector3D::Distance(x0,x1);
       OutputVertex(s,Vector3D::Length(x1),Segment->GetEnd(),FieldLineCoord,Segment);
    }
  }

  fclose(fout);
}
