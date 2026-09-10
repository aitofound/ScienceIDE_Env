//$Id$
//the LB model of the internal degrees of freedom

#include "pic.h"

//the total number of the vibrational and rotational modes
//int *PIC::IDF::nTotalVibtationalModes;
//int *PIC::IDF::nTotalRotationalModes;
//double **PIC::IDF::CharacteristicVibrationalTemperature;

//the offset of the rotation and vibration energy in the particle's data state vector
int PIC::IDF::LB::_ROTATIONAL_ENERGY_OFFSET_=-1,PIC::IDF::LB::_VIBRATIONAL_ENERGY_OFFSET_=-1;
int PIC::IDF::_TOTAL_SAMPLE_PARTICLE_WEIGHT_SAMPLE_DATA_OFFSET_=-1;
int PIC::IDF::_ROTATIONAL_ENERGY_SAMPLE_DATA_OFFSET_=-1;
int PIC::IDF::_VIBRATIONAL_ENERGY_SAMPLE_DATA_OFFSET_[PIC::nTotalSpecies];
//double *PIC::IDF::RotationZnumber;


//init the particle's vibrational energy
void PIC::IDF::LB::InitVibTemp(double VibTemp,PIC::ParticleBuffer::byte *ParticleDataStart) {
  int s,nmode;
  double ThetaVib;

  s=PIC::ParticleBuffer::GetI(ParticleDataStart);

  if ((nTotalVibtationalModes[s]==0)||(VibTemp<1.0E-8)) {
    for (nmode=0;nmode<PIC::IDF::nTotalVibtationalModes[s];nmode++) SetVibE(0.0,nmode,ParticleDataStart);
    return;
  }

  for (nmode=0;nmode<nTotalVibtationalModes[s];nmode++) {
    ThetaVib=PIC::IDF::CharacteristicVibrationalTemperature[nmode+s*PIC::IDF::nSpeciesMaxVibrationalModes];

    if (VibTemp<0.0001*ThetaVib) {
      SetVibE(0.0,nmode,ParticleDataStart);
      continue;
    }

    double c,p,EtaVib,Evib,dE,EvibMax;
    double F,Fmax;

    //Bird, Eq 5.53
    c=ThetaVib/VibTemp;
    EtaVib=2.0*c/(exp(c)-1.0);

    //Function to sample from the distribution E^a * exp(-E / (kT))
    double a=EtaVib/2.0-1.0;
    double alpha=a+1;
    double beta=1.0/(Kbol*VibTemp);

    Evib=Vector3D::Distribution::Gamma(alpha, beta);
    SetVibE(Evib,nmode,ParticleDataStart);
  }
}

void PIC::IDF::LB::InitRotTemp(double RotTemp,PIC::ParticleBuffer::byte *ParticleDataStart) {
  int s;
  double RotDF,Er,p;

  s=PIC::ParticleBuffer::GetI(ParticleDataStart);
  RotDF=nTotalRotationalModes[s];

  if (RotDF<=1.0) {
    SetRotE(0.0,ParticleDataStart);
    return;
  }

  /*
    if (RotDF==2.0) Er=-log(rnd());
  else {
    //Bird: Eq. 11.23
    do {
      Er=10.0*rnd();
      p=pow(Er/(RotDF/2.0-1.0),RotDF/2.0-1.0)*exp(-Er+RotDF/2.0-1.0);
    }
    while (rnd()>p);
  }

  Er*=Kbol*RotTemp;
  SetRotE(Er,ParticleDataStart);

  return;
*/

  if (RotDF==2.0) Er=-Kbol*RotTemp*log(rnd());
  else {
    //Bird: Eq. 11.23
    double a=RotDF/2.0-1.0;
    double alpha=a+1;
    double beta=1.0/(Kbol*RotTemp);

    Er=Vector3D::Distribution::Gamma(alpha, beta);
  }

  SetRotE(Er,ParticleDataStart);
}

double PIC::IDF::LB::GetCellRotTemp(int s,PIC::Mesh::cDataCenterNode* cell) {
  double RotDF,Erot,w,wtot=0.0,res=0.0;
  int sStart,sStop;

  if (s==-1) sStart=0,sStop=PIC::nTotalSpecies;
  else sStart=s,sStop=s+1;

  for (s=sStart;s<sStop;s++) {
    RotDF=nTotalRotationalModes[s];
    Erot=GetCellMeanRotE(s,cell);
    w=cell->GetNumberDensity(s);

    if ((w<1.0E-8)||(Erot<1.0E-300)||(RotDF<1.0E-10)) continue;

    res+=w*2.0/(RotDF*Kbol)*Erot;
    wtot+=w;
  }

  if (wtot>0.0) res/=wtot;
  return res;
}

double PIC::IDF::LB::GetCellVibTemp(int s,PIC::Mesh::cDataCenterNode* cell) {
  return GetCellVibTemp(-1,s,cell);
}

double PIC::IDF::LB::GetCellVibTemp(int nmode,int s,PIC::Mesh::cDataCenterNode* cell) {
  int nmodeStart,nmodeStop,sStart,sStop;
  double ThetaVib,Ev,EtaVib,EtaVibSumm,TempOfTheMode,VibTempSumm,Tvib;
  double w,wtot=0.0,res=0.0;

  if (s==-1) nmode=-1,sStart=0,sStop=PIC::nTotalSpecies;
  else sStart=s,sStop=s+1;

  for (s=sStart;s<sStop;s++) {
    EtaVibSumm=0.0,VibTempSumm=0.0;

    if (nmode==-1) nmodeStart=0,nmodeStop=PIC::IDF::nTotalVibtationalModes[s];
    else nmodeStart=nmode,nmodeStop=nmode+1;

    for(nmode=nmodeStart;nmode<nmodeStop;nmode++) {
      ThetaVib=PIC::IDF::CharacteristicVibrationalTemperature[nmode+s*PIC::IDF::nSpeciesMaxVibrationalModes];

  //Get effective temperature of the mode: Bird. Eq 11.26
  //Get effective number of vibrational degrees of freedom for the mode: Bird. Eq 11.28, 5.53
      Ev=GetCellMeanVibE(nmode,s,cell);

      double a;
      a=Ev/(Kbol*ThetaVib);
      if (a>1.0E-300) {
        TempOfTheMode=ThetaVib/log(1.0+1.0/a);
        EtaVib=2.0*Ev/(Kbol*TempOfTheMode);
      }
      else {
        TempOfTheMode=0.0;
        EtaVib=0.0;
      }

      EtaVibSumm+=EtaVib;
      VibTempSumm+=EtaVib*TempOfTheMode;
    }

    Tvib=(EtaVibSumm>1.0E-300) ? VibTempSumm/EtaVibSumm : 0.0;

    w=cell->GetNumberDensity(s);
    res+=w*Tvib;
    wtot+=w;
  }

  if (wtot>0.0) res/=wtot;
  return res;
}

double PIC::IDF::LB::GetCellMeanRotE(int s,PIC::Mesh::cDataCenterNode* cell) {
  double wtot=0.0,res=0.0;
  int sStart,sStop;

  if (s==-1) sStart=0,sStop=PIC::nTotalSpecies;
  else sStart=s,sStop=s+1;

  for (s=sStart;s<sStop;s++) {
    wtot+=(*(s+(double*)(cell->associatedDataPointer+PIC::Mesh::completedCellSampleDataPointerOffset+_TOTAL_SAMPLE_PARTICLE_WEIGHT_SAMPLE_DATA_OFFSET_)));
    res+=(*(s+(double*)(cell->associatedDataPointer+PIC::Mesh::completedCellSampleDataPointerOffset+_ROTATIONAL_ENERGY_SAMPLE_DATA_OFFSET_)));
  }

  if (wtot>0.0) res/=wtot;
  return res;
}

double PIC::IDF::LB::GetCellMeanVibE(int nmode,int s,PIC::Mesh::cDataCenterNode* cell) {
  double wtot=0.0,res=0.0;
  int sStart,sStop,nmodeStart,nmodeStop;

  if (s==-1) sStart=0,sStop=PIC::nTotalSpecies;
  else sStart=s,sStop=s+1;

  for (s=sStart;s<sStop;s++) {
    wtot+=(*(s+(double*)(cell->associatedDataPointer+PIC::Mesh::completedCellSampleDataPointerOffset+_TOTAL_SAMPLE_PARTICLE_WEIGHT_SAMPLE_DATA_OFFSET_)));

    if (nmode==-1) nmodeStart=0,nmodeStop=PIC::IDF::nTotalVibtationalModes[s];
    else nmodeStart=nmode,nmodeStop=nmode+1;

    for(nmode=nmodeStart;nmode<nmodeStop;nmode++) {
      res+=(*(nmode+(double*)(cell->associatedDataPointer+PIC::Mesh::completedCellSampleDataPointerOffset+_VIBRATIONAL_ENERGY_SAMPLE_DATA_OFFSET_[s])));
    }
  }

  if (wtot>0.0) res/=wtot;
  return res;
}

void PIC::IDF::LB::CalculateRTTransition(PIC::ParticleBuffer::byte *ptr0, PIC::ParticleBuffer::byte *ptr1, 
    double &Ec, bool* ChangeParticlePropertiesFlag, bool &RedistributionEnergyFlag, double TempIndexOmega) {
    
    int s[2];
    s[0]=PIC::ParticleBuffer::GetI(ptr0);
    s[1]=PIC::ParticleBuffer::GetI(ptr1);
    
    double RotDF=PIC::IDF::nTotalRotationalModes[s[0]]+PIC::IDF::nTotalRotationalModes[s[1]];
    
    if (RotDF>0.0) {
        double Erot;

	if (PIC::IDF::nTotalRotationalModes[s[0]]>0) Ec+=GetRotE(ptr0);
        if (PIC::IDF::nTotalRotationalModes[s[1]]>0) Ec+=GetRotE(ptr1); 	

        //divide the energy between rotational and translational degrees of freedom
        PIC::IDF::distribute_energy(Ec,Erot,1.5-TempIndexOmega,RotDF/2.0-1.0,Ec);

        //Distribute rotational energy between the particles  
        double Erot0,Erot1;
        int RotDF0=PIC::IDF::nTotalRotationalModes[s[0]];
        int RotDF1=PIC::IDF::nTotalRotationalModes[s[1]];
 
        if (RotDF0==0) {
            Erot0=0.0,Erot1=Erot;
        }
        else if (RotDF1==0) {
            Erot0=Erot,Erot1=0.0;
        }
        else {
            PIC::IDF::distribute_energy(Erot0,Erot1,RotDF0/2.0-1.0,RotDF1/2.0-1.0,Erot);
        }
      
        if (ChangeParticlePropertiesFlag[0]==true) SetRotE(Erot0,ptr0);
        if (ChangeParticlePropertiesFlag[1]==true) SetRotE(Erot1,ptr1);

        RedistributionEnergyFlag=true;
    }
}

void PIC::IDF::LB::CalculateVTTransition(PIC::ParticleBuffer::byte *ptr0, PIC::ParticleBuffer::byte *ptr1, 
    double &Ec, bool* ChangeParticlePropertiesFlag, bool &RedistributionEnergyFlag, double TempIndexOmega) {
    
    int s[2];
    s[0]=PIC::ParticleBuffer::GetI(ptr0);
    s[1]=PIC::ParticleBuffer::GetI(ptr1);


    exit(__LINE__,__FILE__,"The implimentation is not tests or verified");

    // Get total internal energy (rotational + vibrational)
    double Eint = GetRotE(ptr0) + GetRotE(ptr1);
    for(int i=0; i<2; i++) {
        for(int nmode=0; nmode<nTotalVibtationalModes[s[i]]; nmode++) {
            Eint += GetVibE(nmode, (i==0) ? ptr0 : ptr1);
        }
    }

    // Total energy to redistribute
    double Etot = Ec + Eint;

    // First split translational and total internal energy
    double TotalDF = 1.5-TempIndexOmega;  // Translational DF
    double InternalDF = 0.0;

    // Calculate total internal degrees of freedom
    double RotDF = PIC::IDF::nTotalRotationalModes[s[0]] + PIC::IDF::nTotalRotationalModes[s[1]];
    double VibDF = 0.0;
    for(int i=0; i<2; i++) {
        for(int nmode=0; nmode<nTotalVibtationalModes[s[i]]; nmode++) {
            VibDF += 2.0;  // Each vibrational mode contributes 2 degrees of freedom
        }
    }
    InternalDF = (RotDF/2.0 - 1.0) + VibDF/2.0;

    // Split total energy between translational and internal
    PIC::IDF::distribute_energy(Ec, Eint, TotalDF, InternalDF, Etot);

    // Now split internal energy between rotational and vibrational
    double Erot, Evib;
    if(RotDF > 0.0 && VibDF > 0.0) {
        PIC::IDF::distribute_energy(Erot, Evib, RotDF/2.0-1.0, VibDF/2.0, Eint);
    }
    else if(RotDF > 0.0) {
        Erot = Eint;
        Evib = 0.0;
    }
    else {
        Erot = 0.0;
        Evib = Eint;
    }

    // Distribute rotational energy between particles
    if(RotDF > 0.0) {
        double Erot0, Erot1;
        int RotDF0=PIC::IDF::nTotalRotationalModes[s[0]];
        int RotDF1=PIC::IDF::nTotalRotationalModes[s[1]];
        
        if(RotDF0==0) {
            Erot0=0.0; Erot1=Erot;
        }
        else if(RotDF1==0) {
            Erot0=Erot; Erot1=0.0;
        }
        else {
            PIC::IDF::distribute_energy(Erot0,Erot1,RotDF0/2.0-1.0,RotDF1/2.0-1.0,Erot);
        }
        
        if(ChangeParticlePropertiesFlag[0]) SetRotE(Erot0,ptr0);
        if(ChangeParticlePropertiesFlag[1]) SetRotE(Erot1,ptr1);
    }

    // Distribute vibrational energy between particles
    if(VibDF > 0.0) {
        double Evib0=0.0, Evib1=0.0;
        double VibDF0=2.0*nTotalVibtationalModes[s[0]];
        double VibDF1=2.0*nTotalVibtationalModes[s[1]];
        
        if(VibDF0==0.0) {
            Evib0=0.0; Evib1=Evib;
        }
        else if(VibDF1==0.0) {
            Evib0=Evib; Evib1=0.0;
        }
        else {
            PIC::IDF::distribute_energy(Evib0,Evib1,VibDF0/2.0,VibDF1/2.0,Evib);
        }
        
        // Distribute among modes for each particle
        if(ChangeParticlePropertiesFlag[0] && VibDF0>0.0) {
            double modeEnergy = Evib0/nTotalVibtationalModes[s[0]];
            for(int nmode=0; nmode<nTotalVibtationalModes[s[0]]; nmode++) {
                SetVibE(modeEnergy,nmode,ptr0);
            }
        }
        
        if(ChangeParticlePropertiesFlag[1] && VibDF1>0.0) {
            double modeEnergy = Evib1/nTotalVibtationalModes[s[1]];
            for(int nmode=0; nmode<nTotalVibtationalModes[s[1]]; nmode++) {
                SetVibE(modeEnergy,nmode,ptr1);
            }
        }
    }

    RedistributionEnergyFlag=true;
}

/*
  Summary of the Energy Distribution of a Colliding Pair in a Thermalized Gas

  When two particles are drawn at random from a Maxwell-Boltzmann (MB) distribution
  at temperature \( T \), their relative kinetic energy \( E \) (where
  \( E = \frac{1}{2}\mu g^{2} \) and \(\mu\) is the reduced mass, \(g\) the relative speed)
  follows an MB-like distribution. For randomly chosen pairs (no collision weighting):

    \[
      f_{\text{pair}}(E) \propto E^{1/2} e^{-E/(k_B T)}.
    \]

  However, the energy distribution of pairs that actually collide is different. Collisions
  occur at a rate proportional to \( g \sigma(g) \), where \(\sigma(g)\) is the collision
  cross section. If \(\sigma(g)\) is constant (\(\sigma = \text{const}\)):

    \[
      f_{\text{collision}}(E) \propto E e^{-E/(k_B T)},
    \]

  which differs from the unweighted distribution by an extra factor of \( E^{1/2} \to E \).

  If the cross section depends on relative speed as \(\sigma(g) \propto g^{v}\), then:

    \[
      f_{\text{collision}}(E) \propto E^{1 + \frac{v}{2}} e^{-E/(k_B T)}.
    \]

  Thus, the exponent in the energy distribution depends on the parameter \( v \) characterizing
  the cross section's speed dependence, shifting the relative weighting of low- or high-energy
  collisions accordingly.
*/

void PIC::IDF::LB::RedistributeEnergy(PIC::ParticleBuffer::byte *ptr0,PIC::ParticleBuffer::byte *ptr1,
    double& vrel,bool* ChangeParticlePropertiesFlag,PIC::Mesh::cDataCenterNode* cell) {
    
    int s[2];
    double TempIndexOmega,Ec,m[2],mr;
    bool RedistributionEnergyFlag=false;

    s[0]=PIC::ParticleBuffer::GetI(ptr0);
    s[1]=PIC::ParticleBuffer::GetI(ptr1);
    
    TempIndexOmega=GetTempIndex(s[0],s[1]);

    m[0]=PIC::MolecularData::GetMass(s[0]);
    m[1]=PIC::MolecularData::GetMass(s[1]);
    mr=m[0]*m[1]/(m[0]+m[1]);

    Ec=0.5*mr*vrel*vrel;

    // Set collision numbers
    const double Zrot=5.0;
    const double Zvib=0.5;
    
    // Calculate total relaxation probability: 1/Z = 1/Zrot + 1/Zvib
    double totalRelaxProb = 0.0;
    
    if (_PIC_INTERNAL_DEGREES_OF_FREEDOM__RT_RELAXATION_MODE_ == _PIC_MODE_ON_) totalRelaxProb += 1.0/Zrot;
    if (_PIC_INTERNAL_DEGREES_OF_FREEDOM__VT_RELAXATION_MODE_ == _PIC_MODE_ON_) totalRelaxProb += 1.0/Zvib;
    
    if (rnd() < totalRelaxProb) {
        //redistribution of the internal energy happened => decide via which channel
        if (_PIC_INTERNAL_DEGREES_OF_FREEDOM__VT_RELAXATION_MODE_ == _PIC_MODE_OFF_) {
            //only RT is available 
            CalculateRTTransition(ptr0, ptr1, Ec, ChangeParticlePropertiesFlag, RedistributionEnergyFlag, TempIndexOmega);
        }
        else if (_PIC_INTERNAL_DEGREES_OF_FREEDOM__RT_RELAXATION_MODE_ == _PIC_MODE_OFF_) {
            //only VT is available 
            CalculateVTTransition(ptr0, ptr1, Ec, ChangeParticlePropertiesFlag, RedistributionEnergyFlag, TempIndexOmega);
        }
        else {
            // Both modes are available
            // Probability of RT given relaxation = (1/Zrot)/(1/Zrot + 1/Zvib)
            double probRTGivenRelax = (1.0/Zrot)/totalRelaxProb;
            
            if (rnd() < probRTGivenRelax) {
                CalculateRTTransition(ptr0, ptr1, Ec, ChangeParticlePropertiesFlag, RedistributionEnergyFlag, TempIndexOmega);
            }
            else {
                CalculateVTTransition(ptr0, ptr1, Ec, ChangeParticlePropertiesFlag, RedistributionEnergyFlag, TempIndexOmega);
            }
        }
    }

    if (RedistributionEnergyFlag==true) vrel=sqrt(2.0*Ec/mr);
}

//request the data for the model
int PIC::IDF::LB::RequestSamplingData(int offset) {
  int SampleDataLength=0;

  _TOTAL_SAMPLE_PARTICLE_WEIGHT_SAMPLE_DATA_OFFSET_=offset+SampleDataLength;
  SampleDataLength+=PIC::nTotalSpecies*sizeof(double);

  _ROTATIONAL_ENERGY_SAMPLE_DATA_OFFSET_=offset+SampleDataLength;
  SampleDataLength+=PIC::nTotalSpecies*sizeof(double);

  for (int s=0;s<PIC::nTotalSpecies;s++) {
    _VIBRATIONAL_ENERGY_SAMPLE_DATA_OFFSET_[s]=offset+SampleDataLength;
    SampleDataLength+=nTotalVibtationalModes[s]*sizeof(double);
  }

  return SampleDataLength;
}

void PIC::IDF::LB::Init() {
  //request the additional particle data
  long int offset;
  int DataLength;

  DataLength=(1+nSpeciesMaxVibrationalModes)*sizeof(double);
  PIC::ParticleBuffer::RequestDataStorage(offset,DataLength);

  _ROTATIONAL_ENERGY_OFFSET_=offset;
  _VIBRATIONAL_ENERGY_OFFSET_=offset+sizeof(double);

  //request sampling data
  PIC::IndividualModelSampling::RequestSamplingData.push_back(RequestSamplingData);

  //print out of the otuput file
  PIC::Mesh::PrintVariableListCenterNode.push_back(PrintVariableList);
  PIC::Mesh::PrintDataCenterNode.push_back(PrintData);
  PIC::Mesh::InterpolateCenterNode.push_back(Interpolate);

  //check consistency of the model setting
  for (int s=0;s<PIC::nTotalSpecies;s++) {
    if ( _PIC_INTERNAL_DEGREES_OF_FREEDOM__VT_RELAXATION_MODE_==_PIC_MODE_ON_) {
      for (int nmode=0;nmode<nTotalVibtationalModes[s];nmode++) {
        if (CharacteristicVibrationalTemperature[nmode+s*PIC::IDF::nSpeciesMaxVibrationalModes]<=0.0) {
          exit(__LINE__,__FILE__,"error: CharacteristicVibrationalTemperature must be defined -- check the input file");
        }
      }
    }

    if (_PIC_INTERNAL_DEGREES_OF_FREEDOM__RT_RELAXATION_MODE_==_PIC_MODE_ON_) {
      if (nTotalRotationalModes[s]!=0) {
        if (RotationZnumber[s]==0.0) exit(__LINE__,__FILE__,"error: RotationZnumber must be defined -- check the input file");
      }
    }
  }
}

//print the model data
void PIC::IDF::LB::PrintVariableList(FILE* fout,int DataSetNumber) {
  fprintf(fout,", \"Trot\", \"Tvib\"");
}

void PIC::IDF::LB::PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode) {
  double Trot,Tvib;

  bool gather_output_data=false;

  if (pipe==NULL) gather_output_data=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_output_data=true;


  if (gather_output_data==true) { //if (pipe->ThisThread==CenterNodeThread) {
    Trot=GetCellRotTemp(DataSetNumber,CenterNode);
    Tvib=GetCellVibTemp(DataSetNumber,CenterNode);
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) {
      pipe->recv(Trot,CenterNodeThread);
      pipe->recv(Tvib,CenterNodeThread);
    }

    fprintf(fout," %e  %e ",Trot,Tvib);
  }
  else {
    pipe->send(Trot);
    pipe->send(Tvib);
  }
}

void PIC::IDF::LB::Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode) {
  int i,s,nmode;
  double c;
  char *CenterNodeSampleData,*InterpolationSampleData;


  CenterNodeSampleData=CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset;

  for (i=0;i<nInterpolationCoeficients;i++) {
    c=InterpolationCoeficients[i];
    InterpolationSampleData=InterpolationList[i]->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset;

    for (s=0;s<PIC::nTotalSpecies;s++) {
      *(s+(double*)(CenterNodeSampleData+_TOTAL_SAMPLE_PARTICLE_WEIGHT_SAMPLE_DATA_OFFSET_))+=c*(*(s+(double*)(InterpolationSampleData+_TOTAL_SAMPLE_PARTICLE_WEIGHT_SAMPLE_DATA_OFFSET_)));
      *(s+(double*)(CenterNodeSampleData+_ROTATIONAL_ENERGY_SAMPLE_DATA_OFFSET_))+=c*(*(s+(double*)(InterpolationSampleData+_ROTATIONAL_ENERGY_SAMPLE_DATA_OFFSET_)));

      for (nmode=0;nmode<nTotalVibtationalModes[s];nmode++) {
        *(nmode+(double*)(CenterNodeSampleData+_VIBRATIONAL_ENERGY_SAMPLE_DATA_OFFSET_[s]))+=c*(*(nmode+(double*)(InterpolationSampleData+_VIBRATIONAL_ENERGY_SAMPLE_DATA_OFFSET_[s])));
      }
    }
  }
}


