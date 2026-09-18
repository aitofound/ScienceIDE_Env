#include "sep.h"

void SEP::Sampling::Energy::Output(int cnt) {
  namespace FL=PIC::FieldLine;
  double de,norm,t,max_val;
  int iLine,iE,iR;

  //gather all sampled data 
  REnergySamplingTable.reduce(0,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread!=0) return; 

  REnergySamplingTable.find_nan();


  //normalize the energy distribution 
  for (iLine=0;iLine<FL::nFieldLineMax;iLine++) if (FL::FieldLinesAll[iLine].IsInitialized()==true)  for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
    norm=0.0,max_val=0.0;
 
    for (iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++) {
      de=SEP::Sampling::PitchAngle::emin*exp((iE+1)*SEP::Sampling::PitchAngle::dLogE)-SEP::Sampling::PitchAngle::emin*exp(iE*SEP::Sampling::PitchAngle::dLogE);
      norm+=REnergySamplingTable(iE,iR,iLine)*de;
    }

    if (norm>0.0) {
      for (iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++) {
        REnergySamplingTable(iE,iR,iLine)/=norm;

        if ((t=REnergySamplingTable(iE,iR,iLine))>max_val) max_val=t;
      }

      for (iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++) {
        REnergySamplingTable(iE,iR,iLine)/=max_val;
      }
    }
  }

  REnergySamplingTable.find_nan();

  //output the energy distribution in a file 
  FILE *fout;
  char fname[200];

  sprintf(fname,"mkdir -p %s/EnergyRSample",PIC::OutputDataFileDirectory);
  system(fname);

  sprintf(fname,"%s/EnergyRSample/cnt=%i.dat",PIC::OutputDataFileDirectory,cnt);
  fout=fopen(fname,"w");

  fprintf(fout,"VARIABLES=\"E [Mev]\"");  

   for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
      if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
        for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
          fprintf(fout,", \"F=%i (%.2e-%.2e)AU\"",iLine,
            iR*SEP::Sampling::PitchAngle::dR/_AU_,(iR+1)*SEP::Sampling::PitchAngle::dR/_AU_);
	}
      }
   }

   fprintf(fout,"\n");

   //output the data 
   for (iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++) {
     fprintf(fout," %.2e",SEP::Sampling::PitchAngle::emin*exp(iE*SEP::Sampling::PitchAngle::dLogE)*J2MeV);

     for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
       if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
         for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
           fprintf(fout," %.2e",REnergySamplingTable(iE,iR,iLine));
 	 }
       }
     }

     fprintf(fout,"\n");
   }

   fclose(fout);
}

void SEP::Sampling::RadialDisplacement::OutputDisplacementSamplingTable(int cnt) {
  namespace FL=PIC::FieldLine;
  double dD,norm,t,max_val;
  int iLine,iR,iD;

  //gather all sampled data
  DisplacementSamplingTable.reduce(0,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread!=0) return; 
  DisplacementSamplingTable.find_nan();

  //normalize the energy distribution
  for (iLine=0;iLine<FL::nFieldLineMax;iLine++) if (FL::FieldLinesAll[iLine].IsInitialized()==true)  for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
    norm=0.0,max_val=0.0;

    for (iD=0;iD<SEP::Sampling::RadialDisplacement::nSampleIntervals;iD++) {
      dD=exp((iD+1)*Sampling::RadialDisplacement::dLogDisplacement)-exp(iD*Sampling::RadialDisplacement::dLogDisplacement);

      norm+=DisplacementSamplingTable(iD,iR,iLine)*dD;
    }


    if (norm>0.0) {
      for (iD=0;iD<SEP::Sampling::RadialDisplacement::nSampleIntervals;iD++) {
        DisplacementSamplingTable(iD,iR,iLine)/=norm;

       if ((t=DisplacementSamplingTable(iD,iR,iLine))>max_val) max_val=t;
      }

      for (iD=0;iD<SEP::Sampling::RadialDisplacement::nSampleIntervals;iD++) {
        DisplacementSamplingTable(iD,iR,iLine)/=max_val;
      }
    }
  }


  //output the energy distribution in a file
  FILE *fout;
  char fname[200];

  sprintf(fname,"mkdir -p %s/DisplacementRSample",PIC::OutputDataFileDirectory);
  system(fname);

  sprintf(fname,"%s/DisplacementRSample/cnt=%i.dat",PIC::OutputDataFileDirectory,cnt);
  fout=fopen(fname,"w");

  fprintf(fout,"VARIABLES=\"Particle Radial Distance [m]\"");

   for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
      if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
        for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
          fprintf(fout,", \"F=%i (%.2e-%.2e)AU\"",iLine,
            iR*SEP::Sampling::PitchAngle::dR/_AU_,(iR+1)*SEP::Sampling::PitchAngle::dR/_AU_);
        }
      }
   }

   fprintf(fout,"\n");

   //output the data
   for (iD=0;iD<SEP::Sampling::RadialDisplacement::nSampleIntervals;iD++) {
     fprintf(fout," %.2e",exp(iD*Sampling::RadialDisplacement::dLogDisplacement));

     for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
       if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
         for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
           fprintf(fout," %.2e",DisplacementSamplingTable(iD,iR,iLine));
         }
       }
     }

     fprintf(fout,"\n");
   }

   fclose(fout);
}

void SEP::Sampling::RadialDisplacement::OutputDisplacementEnergySamplingTable(int cnt) {
  namespace FL=PIC::FieldLine;
  double dD,norm,t,max_val;
  int iLine,iR,iD,iE;

  //gather all sampled data
  DisplacementEnergySamplingTable.reduce(0,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread!=0) return; 
  DisplacementEnergySamplingTable.find_nan();

  //normalize the energy distribution
  for (iLine=0;iLine<FL::nFieldLineMax;iLine++) if (FL::FieldLinesAll[iLine].IsInitialized()==true)  for (iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++)  for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
    norm=0.0,max_val=0.0;

    for (iD=0;iD<SEP::Sampling::RadialDisplacement::nSampleIntervals;iD++) {
      dD=exp((iD+1)*Sampling::RadialDisplacement::dLogDisplacement)-exp(iD*Sampling::RadialDisplacement::dLogDisplacement);

      norm+=DisplacementEnergySamplingTable(iD,iE,iR,iLine)*dD;
    }


    if (norm>0.0) {
      for (iD=0;iD<SEP::Sampling::RadialDisplacement::nSampleIntervals;iD++) {
        DisplacementEnergySamplingTable(iD,iE,iR,iLine)/=norm;

        if ((t=DisplacementEnergySamplingTable(iD,iE,iR,iLine))>max_val) max_val=t;
      }


      for (iD=0;iD<SEP::Sampling::RadialDisplacement::nSampleIntervals;iD++) {
        DisplacementEnergySamplingTable(iD,iE,iR,iLine)/=max_val;
      }
    }
  }


  //output the energy distribution in a file
  FILE *fout;
  char fname[200];

  sprintf(fname,"mkdir -p %s/DisplacementEvergyRSample",PIC::OutputDataFileDirectory);
  system(fname);

  sprintf(fname,"%s/DisplacementEvergyRSample/cnt=%i.dat",PIC::OutputDataFileDirectory,cnt);
  fout=fopen(fname,"w");

  fprintf(fout,"VARIABLES=\"Particle Radial Distance [m]\"");

   for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
      if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
        for (iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++) {
          for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
            fprintf(fout,", \"F=%i (%.2e-%.2e)Mev (%.2e-%.2e)AU\"",iLine,
              SEP::Sampling::PitchAngle::emin*exp(iE*SEP::Sampling::PitchAngle::dLogE)*J2MeV,
              SEP::Sampling::PitchAngle::emin*exp((iE+1)*SEP::Sampling::PitchAngle::dLogE)*J2MeV,
              iR*SEP::Sampling::PitchAngle::dR/_AU_,(iR+1)*SEP::Sampling::PitchAngle::dR/_AU_);
	  }
        }
      }
   }

   fprintf(fout,"\n");

   //output the data
   for (iD=0;iD<SEP::Sampling::RadialDisplacement::nSampleIntervals;iD++) {
     fprintf(fout," %.2e",exp(iD*Sampling::RadialDisplacement::dLogDisplacement));

     for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
       if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
         for (iE=0;iE<SEP::Sampling::PitchAngle::nEnergySamplingIntervals;iE++) {
           for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
             fprintf(fout," %.2e",DisplacementEnergySamplingTable(iD,iE,iR,iLine));
	   }
         }
       }
     }

     fprintf(fout,"\n");
   }

   fclose(fout);
}


void SEP::Sampling::LarmorRadius::Output(int cnt) {
  namespace FL=PIC::FieldLine;
  double dL,norm,t,max_val;
  int iLine,iR,iD,iL;

  //gather all sampled data
  SamplingTable.reduce(0,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread!=0) return; 
  SamplingTable.find_nan();

  //normalize the energy distribution
  for (iLine=0;iLine<FL::nFieldLineMax;iLine++) if (FL::FieldLinesAll[iLine].IsInitialized()==true)   for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
    norm=0.0,max_val=0.0;

    for (iL=0;iL<nSampleIntervals;iL++) {
      dL=exp((iL+1)*dLog)-exp(iL*dLog);

      norm+=SamplingTable(iL,iR,iLine)*dL;
    }


    if (norm>0.0) {
      for (iL=0;iL<nSampleIntervals;iL++) {
        SamplingTable(iL,iR,iLine)/=norm;

        if ((t=SamplingTable(iL,iR,iLine))>max_val) max_val=t;
      }


      for (iL=0;iL<nSampleIntervals;iL++) {
        SamplingTable(iL,iR,iLine)/=max_val;
      }
    }
  }


  //output the distribution in a file
  FILE *fout;
  char fname[200];

  sprintf(fname,"mkdir -p %s/LarmorRadius",PIC::OutputDataFileDirectory);
  system(fname);

  sprintf(fname,"%s/LarmorRadius/cnt=%i.dat",PIC::OutputDataFileDirectory,cnt);
  fout=fopen(fname,"w");

  fprintf(fout,"VARIABLES=\"LarmorRadius [m]\"");

   for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
      if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
        for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
          fprintf(fout,", \"F=%i (%.2e-%.2e)AU\"",iLine,
            iR*SEP::Sampling::PitchAngle::dR/_AU_,(iR+1)*SEP::Sampling::PitchAngle::dR/_AU_);
        }
      }
   }

   fprintf(fout,"\n");

   //output the data
   for (iL=0;iL<nSampleIntervals;iL++) {
     fprintf(fout," %.2e",exp(iL*dLog));

     for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
       if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
         for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
           fprintf(fout," %.2e",SamplingTable(iL,iR,iLine));
         }
       }
     }

     fprintf(fout,"\n");
   }

   fclose(fout);
}



void SEP::Sampling::MeanFreePath::Output(int cnt) {
  namespace FL=PIC::FieldLine;
  double d,norm,t,max_val;
  int iLine,iR,i;

  //gather all sampled data
  SamplingTable.reduce(0,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread!=0) return;
  SamplingTable.find_nan();

  //normalize the energy distribution
  for (iLine=0;iLine<FL::nFieldLineMax;iLine++) if (FL::FieldLinesAll[iLine].IsInitialized()==true)   for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
    norm=0.0,max_val=0.0;

    for (i=0;i<nSampleIntervals;i++) {
      d=MinSampledMeanFreePath*(exp((i+1)*dLogMeanFreePath)-exp(i*dLogMeanFreePath));

      norm+=SamplingTable(i,iR,iLine)*d;
    }


    if (norm>0.0) {
      for (i=0;i<nSampleIntervals;i++) {
        SamplingTable(i,iR,iLine)/=norm;

        if ((t=SamplingTable(i,iR,iLine))>max_val) max_val=t;
      }


      for (i=0;i<nSampleIntervals;i++) {
        SamplingTable(i,iR,iLine)/=max_val;
      }
    }
  }

  //output the distribution in a file
  FILE *fout;
  char fname[200];

  sprintf(fname,"mkdir -p %s/MeanFreePath",PIC::OutputDataFileDirectory);
  system(fname);

  sprintf(fname,"%s/MeanFreePath/cnt=%i.dat",PIC::OutputDataFileDirectory,cnt);
  fout=fopen(fname,"w");

  fprintf(fout,"VARIABLES=\"MeanFreePath [AU]\"");

   for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
      if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
        for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
          fprintf(fout,", \"F=%i (%.2e-%.2e)AU\"",iLine,
            iR*SEP::Sampling::PitchAngle::dR/_AU_,(iR+1)*SEP::Sampling::PitchAngle::dR/_AU_);
        }
      }
   }

   fprintf(fout,"\n");

   //output the data
   for (i=0;i<nSampleIntervals;i++) {
     fprintf(fout," %.2e",MinSampledMeanFreePath*exp(i*dLogMeanFreePath)/_AU_);

     for (iLine=0;iLine<FL::nFieldLineMax;iLine++) {
       if (FL::FieldLinesAll[iLine].IsInitialized()==true)  {
         for (iR=0;iR<SEP::Sampling::PitchAngle::nRadiusIntervals;iR++) {
           fprintf(fout," %.2e",SamplingTable(i,iR,iLine));
         }
       }
     }

     fprintf(fout,"\n");
   }

   fclose(fout);
}


