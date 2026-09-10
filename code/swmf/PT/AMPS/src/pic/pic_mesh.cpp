//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//====================================================
//$Id: pic_mesh.cpp,v 1.39 2018/06/07 18:28:19 vtenishe Exp $
//====================================================
//the function for particle's data sampling

#include "pic.h"

namespace PIC {
namespace Mesh {
//basic macroscopic parameters sampled in the simulation
cDatumTimed DatumParticleWeight(1,"\"Particle Weight\"",false);
cDatumTimed DatumParticleNumber(1,"\"Particle Number\"",true);
cDatumTimed DatumNumberDensity(1,"\"Number Density[1/m^3]\"",true);

cDatumWeighted DatumParticleSpeed(1,"\"|V| [m/s]\"",true);

cDatumWeighted DatumParticleVelocity(3, "\"Vx [m/s]\", \"Vy [m/s]\", \"Vz [m/s]\"", true);
cDatumWeighted DatumParticleVelocity2(3,"\"Vx^2 [(m/s)^2]\", \"Vy^2 [(m/s)^2]\", \"Vz^2 [(m/s)^2]\"", false);
cDatumWeighted DatumParticleVelocity2Tensor(3,"\"VxVy [(m/s)^2]\", \"VyVz [(m/s)^2]\", \"VzVx [(m/s)^2]\"", false);
//-------------------------------------------------------------------------
// IMPORTANT: some data may be oncluded only for certain species!!!!
//if(GetSpecieType(DataSetNumber)==_PIC_SPECIE_TYPE__GAS_)
//-------------------------------------------------------------------------
cDatumDerived DatumTranslationalTemperature(1, "\"Translational Temperature [K]\"", true);
cDatumDerived DatumParallelTranslationalTemperature(1, "\"Parallel Translational Temperature [K]\"", true);
cDatumDerived DatumTangentialTranslationalTemperature(1, "\"Tangential Translational Temperature [K]\"", true);

cDatumWeighted DatumParallelTantentialTemperatureSample_Velocity(3,"\"Vpar [m/s]\", \"Vt0 [m/s]\", \"Vt1 [m/s]\"",false);
cDatumWeighted DatumParallelTantentialTemperatureSample_Velocity2(3,"\"Vpar^2 [(m/s)^2]\", \"Vt0^2 [(m/s)^2]\", \"Vt1^2 [(m/s)^2]\"",false);

//Datum table to be used on GPU
_TARGET_DEVICE_ _CUDA_MANAGED_ cDatumTableGPU *DatumTableGPU=NULL;

// vector of active sampling data
vector<PIC::Datum::cDatumSampled*> DataSampledCenterNodeActive;
// vector of active derived data
vector<cDatumDerived*> DataDerivedCenterNodeActive;
}
}

//init the block's global data
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::Mesh::cDataBlockAMR_static_data::LocalTimeStepOffset=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::Mesh::cDataBlockAMR_static_data::LocalParticleWeightOffset=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::Mesh::cDataBlockAMR_static_data::totalAssociatedDataLength=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ bool PIC::Mesh::cDataBlockAMR_static_data::InternalDataInitFlag=false;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::Mesh::cDataBlockAMR_static_data::UserAssociatedDataOffset=0;

//init the cells' global data
int _TARGET_DEVICE_ _CUDA_MANAGED_ PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength=0;
int _TARGET_DEVICE_ _CUDA_MANAGED_ PIC::Mesh::cDataCenterNode_static_data::LocalParticleVolumeInjectionRateOffset=0;

//init the cell corner's global data
int _TARGET_DEVICE_ _CUDA_MANAGED_ PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength=0;

//in case OpenMP is used: tempParticleMovingListTableThreadOffset is the offset in the associatedDataPointer vector to the position when the temporary particle list begins
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::Mesh::cDataBlockAMR_static_data::tempTempParticleMovingListMultiThreadTableOffset=-1;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::Mesh::cDataBlockAMR_static_data::tempTempParticleMovingListMultiThreadTableLength=0;

_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::Mesh::cDataBlockAMR_static_data::LoadBalancingMeasureOffset=0;

//the offsets to the sampled data stored in 'center nodes'
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::Mesh::completedCellSampleDataPointerOffset=0,PIC::Mesh::collectingCellSampleDataPointerOffset=0;
int PIC::Mesh::sampleSetDataLength=0;

//domain block decomposition used in OpenMP loops
_TARGET_DEVICE_ _CUDA_MANAGED_ unsigned int  PIC::DomainBlockDecomposition::nLocalBlocks=0;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::DomainBlockDecomposition::LastMeshModificationID=-1;
_TARGET_DEVICE_ _CUDA_MANAGED_ cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> _TARGET_DEVICE_ **PIC::DomainBlockDecomposition::BlockTable=NULL;

//the mesh parameters
double PIC::Mesh::xmin[3]={0.0,0.0,0.0},PIC::Mesh::xmax[3]={0.0,0.0,0.0};
PIC::Mesh::fLocalMeshResolution PIC::Mesh::LocalMeshResolution=NULL;

_TARGET_DEVICE_ _CUDA_MANAGED_ cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR> *PIC::Mesh::mesh=NULL;
_TARGET_DEVICE_ _CUDA_MANAGED_ cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR> *PIC::Mesh::MeshTable=NULL;
_TARGET_DEVICE_ _CUDA_MANAGED_ int PIC::Mesh::MeshTableLength=0;

_TARGET_DEVICE_ cAmpsMesh<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR> *PIC::Mesh::GPU::mesh=NULL;


//the user defined functions for output of the 'ceneter node' data into a data file
vector<PIC::Mesh::fPrintVariableListCenterNode> PIC::Mesh::PrintVariableListCenterNode;
vector<PIC::Mesh::fPrintDataCenterNode> PIC::Mesh::PrintDataCenterNode;
vector<PIC::Mesh::fInterpolateCenterNode> PIC::Mesh::InterpolateCenterNode;

//the user defined fucntion for output of the 'corener' data into a file
vector<PIC::Mesh::fPrintVariableListCornerNode> PIC::Mesh::PrintVariableListCornerNode;
vector<PIC::Mesh::fPrintDataCornerNode> PIC::Mesh::PrintDataCornerNode;

void PIC::Mesh::AddVaraibleListFunction(fPrintVariableListCenterNode f) {
  PrintVariableListCenterNode.push_back(f);
} 



void PIC::Mesh::SetCellSamplingDataRequest() {
  exit(__LINE__,__FILE__,"not implemented yet");
}

//print the corner node data
void PIC::Mesh::cDataCornerNode::PrintVariableList(FILE* fout,int DataSetNumber) {
  //print the user defind 'corner node' data
  vector<fPrintVariableListCornerNode>::iterator fptr;
  for (fptr=PrintVariableListCornerNode.begin();fptr!=PrintVariableListCornerNode.end();fptr++) (*fptr)(fout,DataSetNumber);
}

void PIC::Mesh::cDataCornerNode::PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CornerNodeThread) {
  //print the user defind 'center node' data
  vector<fPrintDataCornerNode>::iterator fptr;

  for (fptr=PrintDataCornerNode.begin();fptr!=PrintDataCornerNode.end();fptr++) (*fptr)(fout,DataSetNumber,pipe,CornerNodeThread,this);

}

//print the center node data
void PIC::Mesh::cDataCenterNode::PrintVariableList(FILE* fout,int DataSetNumber) {
  // printe sampled data names
  vector<PIC::Datum::cDatumSampled*>::iterator ptrDatumSampled;

  for (ptrDatumSampled = DataSampledCenterNodeActive.begin(); ptrDatumSampled!= DataSampledCenterNodeActive.end(); ptrDatumSampled++) {
    if ((*ptrDatumSampled)->doPrint==true) (*ptrDatumSampled)->PrintName(fout);
  }

  // print derived data names
  vector<cDatumDerived*>::iterator ptrDatumDerived;

  for(ptrDatumDerived = DataDerivedCenterNodeActive.begin(); ptrDatumDerived!= DataDerivedCenterNodeActive.end(); ptrDatumDerived++) {
    if ((*ptrDatumDerived)->doPrint==true) (*ptrDatumDerived)->PrintName(fout);
  }

  //print the user defind 'center node' data
  vector<fPrintVariableListCenterNode>::iterator fptr;
  for (fptr=PrintVariableListCenterNode.begin();fptr!=PrintVariableListCenterNode.end();fptr++) (*fptr)(fout,DataSetNumber);

  //if drift velocity is output -> print the variable name here
  if (_PIC_OUTPUT__DRIFT_VELOCITY__MODE_==_PIC_MODE_ON_) fprintf(fout, ", \"vxDrift\", \"vyDrift\", \"vzDrift\"");

  //print varialbes sampled by the user defined sampling procedures
  if (PIC::IndividualModelSampling::PrintVariableList.size()!=0) {
    for (unsigned int i=0;i<PIC::IndividualModelSampling::PrintVariableList.size();i++) PIC::IndividualModelSampling::PrintVariableList[i](fout,DataSetNumber);
  }

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
  // IMPORTANT ABOUT PLACEMENT OF THE EXTRA SI COLUMNS:
  //
  // The existing Tecplot output path prints data in the following order:
  //   1. built-in sampled center-node data (OutputData[])
  //   2. user-defined center-node data (PrintDataCenterNode callbacks)
  //   3. optional drift velocity
  //   4. optional IndividualModelSampling data
  //
  // Therefore the matching variable list must use the exact same order.  The
  // safest place for any *new* derived output columns is at the very end, after
  // all pre-existing variables, so that none of the legacy columns shift their
  // header/data alignment.  This is especially important for externally added
  // fields such as magnetic field output (e.g., "Bx (center node)").
  //
  // Append all SI-converted particle quantities here, in the exact same order
  // in which their numeric values are appended at the end of PrintData().
  fprintf(fout, ", \"Number Density[SI]\"");
  fprintf(fout, ", \"Particle Speed[SI, m/s]\"");
  fprintf(fout, ", \"Particle Velocity X[SI, m/s]\", \"Particle Velocity Y[SI, m/s]\", \"Particle Velocity Z[SI, m/s]\"");
#endif
}

void PIC::Mesh::cDataCenterNode::PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread) {
  static int  nOutput=0;
  static bool IsFirstCall=true;
  static double* OutputData;

  static vector<cDatumTimed*>    DataTimedPrint;
  static vector<cDatumWeighted*> DataWeightedPrint;
  static vector<cDatumDerived*>  DataDerivedPrint;

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
  // Indices of selected built-in sampled quantities inside the flattened
  // OutputData[] array that stores the already-sampled center-node quantities.
  //
  // Why store indices instead of re-sampling later?
  //   1. It guarantees that the SI conversion uses the exact same normalized
  //      values that are already being printed to Tecplot in the legacy columns.
  //   2. It avoids any risk of recomputing a different quantity or using a
  //      different accumulation path.
  //   3. It keeps the new SI columns purely as post-processing views of the
  //      existing output, with zero effect on the original sampled fields.
  //
  // Assumptions made explicit here:
  //   * DatumNumberDensity has length 1.
  //   * DatumParticleSpeed has length 1.
  //   * DatumParticleVelocity has length 3 and stores components in the order
  //     X, Y, Z.
  static int iOutputNumberDensity = -1;
  static int iOutputParticleSpeed = -1;
  static int iOutputParticleVelocity = -1;
#endif

  static unsigned long int nCallCounter=0;
  ++nCallCounter;

  // find size of message at the first call
  if (IsFirstCall==true) {
    // include sampled data
    vector<PIC::Datum::cDatumSampled*>::iterator itrDatumSampled;
    cDatumTimed*    ptrDatumTimed;
    cDatumWeighted* ptrDatumWeighted;

    for(itrDatumSampled = DataSampledCenterNodeActive.begin();itrDatumSampled!= DataSampledCenterNodeActive.end();itrDatumSampled++) {
      if ((*itrDatumSampled)->doPrint==true) {
#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
        // Record the locations of the already-sampled quantities in the packed
        // output vector *before* nOutput is incremented.
        //
        // These indices are later used to append SI-converted columns that are
        // guaranteed to correspond exactly to the legacy printed values.
        if ((*itrDatumSampled)==&DatumNumberDensity) iOutputNumberDensity=nOutput;
        if ((*itrDatumSampled)==&DatumParticleSpeed) iOutputParticleSpeed=nOutput;
        if ((*itrDatumSampled)==&DatumParticleVelocity) iOutputParticleVelocity=nOutput;
#endif
        nOutput+=(*itrDatumSampled)->length;

        if((*itrDatumSampled)->type == PIC::Datum::cDatumSampled::Timed_) {
          ptrDatumTimed = static_cast<cDatumTimed*> ((*itrDatumSampled));
          DataTimedPrint.push_back(ptrDatumTimed);
        }
        else {
          ptrDatumWeighted = static_cast<cDatumWeighted*> ((*itrDatumSampled));
          DataWeightedPrint.push_back(ptrDatumWeighted);
        }
      }
    }

    // include derived data
    vector<cDatumDerived*>::iterator itrDatumDerived;

    for(itrDatumDerived = DataDerivedCenterNodeActive.begin();itrDatumDerived!= DataDerivedCenterNodeActive.end(); itrDatumDerived++) if ((*itrDatumDerived)->doPrint) {
      nOutput+=(*itrDatumDerived)->length;
      DataDerivedPrint.push_back((*itrDatumDerived));
    }

    // allocate memory for output
    OutputData = new double[nOutput];

    // mark exit from the first (initializing) call
    IsFirstCall=false;
  }

  bool gather_print_data=false;

  if (pipe==NULL) gather_print_data=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_print_data=true;

  if (gather_print_data==true) { // (pipe->ThisThread==CenterNodeThread) {
    // compose a message
    int iOutput=0;
    // timed data values
    vector<cDatumTimed*>::iterator itrDatumTimed;
    vector<cDatumWeighted*>::iterator itrDatumWeighted;
    vector<cDatumDerived*>::iterator itrDatumDerived;

    for (itrDatumTimed = DataTimedPrint.begin();itrDatumTimed!= DataTimedPrint.end(); itrDatumTimed++) {
      GetDatumAverage(*(*itrDatumTimed),&OutputData[iOutput], DataSetNumber);
      iOutput += (*itrDatumTimed)->length;
    }

    for(itrDatumWeighted = DataWeightedPrint.begin();itrDatumWeighted!= DataWeightedPrint.end(); itrDatumWeighted++) {
      GetDatumAverage(*(*itrDatumWeighted),&OutputData[iOutput],DataSetNumber);
      iOutput += (*itrDatumWeighted)->length;
    }

    for(itrDatumDerived = DataDerivedPrint.begin();itrDatumDerived!= DataDerivedPrint.end(); itrDatumDerived++) {
      GetDatumAverage(*(*itrDatumDerived),&OutputData[iOutput], DataSetNumber);
      iOutput += (*itrDatumDerived)->length;
    }
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) {
    //print values to the output file
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv((char*)OutputData,nOutput*sizeof(double),CenterNodeThread);

    for(int iOutput=0; iOutput<nOutput; iOutput++) fprintf(fout, "%e ", OutputData[iOutput]);
  }
  else pipe->send((char*)OutputData,nOutput*sizeof(double));

  //print the user defind 'center node' data
  vector<fPrintDataCenterNode>::iterator fptr;

  for (fptr=PrintDataCenterNode.begin();fptr!=PrintDataCenterNode.end();fptr++) (*fptr)(fout,DataSetNumber,pipe,CenterNodeThread,this);

  //if drift velocity is output -> print the variable name here
  if (_PIC_OUTPUT__DRIFT_VELOCITY__MODE_==_PIC_MODE_ON_) {
    double vDrift[3];

    bool gather_print_data=false;

    if (pipe==NULL) gather_print_data=true;
    else if (pipe->ThisThread==CenterNodeThread) gather_print_data=true;

    if (gather_print_data==true) { //(pipe->ThisThread==CenterNodeThread) {
      //calculate the drift velocity
      double BulkVelocity[3],ParticleMass,ParticleCharge;

      ParticleMass=PIC::MolecularData::GetMass(DataSetNumber);
      ParticleCharge=PIC::MolecularData::GetElectricCharge(DataSetNumber);
      GetBulkVelocity(BulkVelocity,DataSetNumber);

      //set up the points of the interpolation. move the point inside domain if on the boundary
      double xTest[3];

      for (int idim=0;idim<3;idim++) {
        xTest[idim]=x[idim];
        if (xTest[idim]==PIC::Mesh::mesh->xGlobalMax[idim]) xTest[idim]-=1.0E-10*(PIC::Mesh::mesh->xGlobalMax[idim]-PIC::Mesh::mesh->xGlobalMin[idim]);
      }

      PIC::CPLR::InitInterpolationStencil(xTest,NULL);
      PIC::CPLR::GetDriftVelocity(vDrift,BulkVelocity,ParticleMass,ParticleCharge);
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv((char*)vDrift,3*sizeof(double),CenterNodeThread);

      fprintf(fout," %e %e %e ",vDrift[0],vDrift[1],vDrift[2]);
    }
    else pipe->send((char*)vDrift,3*sizeof(double));
  }

  //print data sampled by the user defined sampling functions
  if (PIC::IndividualModelSampling::PrintSampledData.size()!=0) {
    for (unsigned int i=0;i<PIC::IndividualModelSampling::PrintSampledData.size();i++) PIC::IndividualModelSampling::PrintSampledData[i](fout,DataSetNumber,pipe,CenterNodeThread,this);
  }

#if _CONVERT_OUTPUT_SI_UNITS_ == _PIC_MODE_ON_
  // Append the SI-converted quantities *after* all pre-existing output.  This
  // preserves the byte-for-byte order of every legacy Tecplot column and avoids
  // shifting later quantities such as center-node magnetic field.
  //
  // The conversion is intentionally based on the already prepared OutputData[]
  // values for the corresponding built-in sampled quantities.  That ensures the
  // SI columns are simply converted views of the exact normalized values that
  // AMPS is already writing to the legacy columns.
  double NumberDensitySI = 0.0;
  double ParticleSpeedSI = 0.0;
  double ParticleVelocitySI[3] = {0.0,0.0,0.0};

  bool gather_print_data_si=false;

  if (pipe==NULL) gather_print_data_si=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_print_data_si=true;

  if (gather_print_data_si==true) {
    const double SpeciesMassSI = PIC::MolecularData::GetMass(DataSetNumber);

    // -------- Number density -------------------------------------------------
    // Convert normalized number density -> SI [1/m^3] using the unit
    // normalization helpers from src/pic/units.  This call uses the global
    // normalization factors already initialized by the units subsystem and the
    // mass of the currently printed species.
    if ((iOutputNumberDensity>=0) && (iOutputNumberDensity<nOutput)) {
      const double NumberDensitySolver = OutputData[iOutputNumberDensity];

      #if DIM == 3
      NumberDensitySI = NumberDensitySolver /
        (PIC::Units::Factors.No2SiL*PIC::Units::Factors.No2SiL*PIC::Units::Factors.No2SiL);
      #elif DIM == 2
      NumberDensitySI = NumberDensitySolver /
        (PIC::Units::Factors.No2SiL*PIC::Units::Factors.No2SiL);
      #elif DIM == 1
      NumberDensitySI = NumberDensitySolver /
        (PIC::Units::Factors.No2SiL);
      #endif
    }

    // -------- Particle speed -------------------------------------------------
    // Speed is a scalar magnitude sampled by AMPS.  Convert that scalar from
    // normalized units to SI [m/s] using the existing velocity scale factor.
    if ((iOutputParticleSpeed>=0) && (iOutputParticleSpeed<nOutput)) {
      const double ParticleSpeedNormalized = OutputData[iOutputParticleSpeed];
      ParticleSpeedSI = picunits::no2si_v(ParticleSpeedNormalized,PIC::Units::Factors);
    }

    // -------- Particle velocity vector --------------------------------------
    // Velocity is a 3-component sampled quantity.  Convert each component using
    // the same normalized-velocity -> SI [m/s] mapping that is used for any
    // other velocity-like vector in src/pic/units.
    if ((iOutputParticleVelocity>=0) && ((iOutputParticleVelocity+2)<nOutput)) {
      picunits::no2si_v3(&OutputData[iOutputParticleVelocity],ParticleVelocitySI,PIC::Units::Factors);
    }
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) {
      pipe->recv((char*)&NumberDensitySI,sizeof(NumberDensitySI),CenterNodeThread);
      pipe->recv((char*)&ParticleSpeedSI,sizeof(ParticleSpeedSI),CenterNodeThread);
      pipe->recv((char*)ParticleVelocitySI,3*sizeof(double),CenterNodeThread);
    }

    fprintf(fout," %e ",NumberDensitySI);
    fprintf(fout," %e ",ParticleSpeedSI);
    fprintf(fout," %e %e %e ",ParticleVelocitySI[0],ParticleVelocitySI[1],ParticleVelocitySI[2]);
  }
  else {
    pipe->send((char*)&NumberDensitySI,sizeof(NumberDensitySI));
    pipe->send((char*)&ParticleSpeedSI,sizeof(ParticleSpeedSI));
    pipe->send((char*)ParticleVelocitySI,3*sizeof(double));
  }
#endif

}

void PIC::Mesh::cDataCenterNode::Interpolate(cDataCenterNode** InterpolationList,double *InterpolationCoefficients,int nInterpolationCoefficients) {
  int s;

  if (associatedDataPointer==NULL) exit(__LINE__,__FILE__,"Error: The associated data buffer is not initialized");

  for (s=0;s<PIC::nTotalSpecies;s++) {
    // interpolate all sampled data
    vector<PIC::Datum::cDatumSampled*>::iterator ptrDatum;
    PIC::Datum::cDatumSampled Datum;

    for(ptrDatum = DataSampledCenterNodeActive.begin();ptrDatum!= DataSampledCenterNodeActive.end(); ptrDatum++) {
      Datum=**ptrDatum;
      InterpolateDatum(Datum,InterpolationList,InterpolationCoefficients,nInterpolationCoefficients, s);
    }

    //temeprature is exeption: it needs avaraging of the mean velocity and mean squate of velocity are not
    //interpolated using the interpolation weights but are averaged
    double TotalParticleWeight=0.0,v[3]={0.0,0.0,0.0},v2[3]={0.0,0.0,0.0},vtemp[3],v2temp[3],w;
    double v2Tensor[3]={0,0,0}, v2TensorTemp[3];

    double vParallelTangentialTemperatureSample[3]={0.0,0.0,0.0},v2ParallelTangentialTemperatureSample[3]={0.0,0.0,0.0};
    int iStencil,idim;

    for (iStencil=0;iStencil<nInterpolationCoefficients;iStencil++) {
      w=InterpolationList[iStencil]->GetDatumCumulative(DatumParticleWeight,s);
      TotalParticleWeight+=w;

      InterpolationList[iStencil]->GetDatumCumulative(DatumParticleVelocity, vtemp, s);
      InterpolationList[iStencil]->GetDatumCumulative(DatumParticleVelocity2,v2temp,s);

      if  (_PIC_SAMPLE__VELOCITY_TENSOR_MODE_==_PIC_MODE_ON_) {
        InterpolationList[iStencil]->GetDatumCumulative(DatumParticleVelocity2Tensor,v2TensorTemp,s);
      }

      for (idim=0;idim<3;idim++) v[idim]+=vtemp[idim],v2[idim]+=v2temp[idim];

      if  (_PIC_SAMPLE__VELOCITY_TENSOR_MODE_==_PIC_MODE_ON_) {
        for (idim=0;idim<3;idim++) v2Tensor[idim]+=v2TensorTemp[idim];
      }

      if ((w>0.0)&&(_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE_!= _PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE__OFF_)) {
        InterpolationList[iStencil]->GetDatumCumulative(DatumParallelTantentialTemperatureSample_Velocity,vtemp,s);
        InterpolationList[iStencil]->GetDatumCumulative(DatumParallelTantentialTemperatureSample_Velocity2,v2temp,s);

        for (idim=0;idim<3;idim++) {
          vParallelTangentialTemperatureSample[idim]+=pow(vtemp[idim]/w,2)*w;  //get mean of <v>^2 over the satencil
          v2ParallelTangentialTemperatureSample[idim]+=v2temp[idim];           // get mean <v^2> over the stencil
        }
      }
    }

    //weight the averaged v and v2 to the interpolated value of the weight so the calculation of the temperature is consistent
    if (TotalParticleWeight>0.0) {
      double c;

      c=GetDatumCumulative(DatumParticleWeight,s)/TotalParticleWeight;
      for (idim=0;idim<3;idim++) v[idim]*=c,v2[idim]*=c;

      SetDatum(&DatumParticleVelocity,v,s);
      SetDatum(&DatumParticleVelocity2,v2,s);

      if  (_PIC_SAMPLE__VELOCITY_TENSOR_MODE_==_PIC_MODE_ON_) {
        for (idim=0;idim<3;idim++) v2Tensor[idim]*=c;
        SetDatum(&DatumParticleVelocity2Tensor,v2Tensor,s);
      }

      if (_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE_!= _PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE__OFF_) {
        for (idim=0;idim<3;idim++) {
          vParallelTangentialTemperatureSample[idim]=sqrt(vParallelTangentialTemperatureSample[idim]/TotalParticleWeight)*GetDatumCumulative(DatumParticleWeight,s);  //convert back from < <v>^2 > -> <v>
          v2ParallelTangentialTemperatureSample[idim]*=c;
        }

        SetDatum(&DatumParallelTantentialTemperatureSample_Velocity,vParallelTangentialTemperatureSample,s);
        SetDatum(&DatumParallelTantentialTemperatureSample_Velocity2,v2ParallelTangentialTemperatureSample,s);
      }
    }
    else {
      //init the approprate locations in the state vector with zeros
      double v3temp[3]={0.0,0.0,0.0};

      SetDatum(&DatumParticleVelocity,v3temp,s);
      SetDatum(&DatumParticleVelocity2,v3temp,s);

      if  (_PIC_SAMPLE__VELOCITY_TENSOR_MODE_==_PIC_MODE_ON_) {
        SetDatum(&DatumParticleVelocity2Tensor,v3temp,s);
      }

      if (_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE_!= _PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE__OFF_) {
        SetDatum(&DatumParallelTantentialTemperatureSample_Velocity,v3temp,s);
        SetDatum(&DatumParallelTantentialTemperatureSample_Velocity2,v3temp,s);
      }
    }

  }

  //print the user defind 'center node' data
  vector<fInterpolateCenterNode>::iterator fptr;

  for (fptr=InterpolateCenterNode.begin();fptr!=InterpolateCenterNode.end();fptr++) (*fptr)(InterpolationList,InterpolationCoefficients,nInterpolationCoefficients,this);

  //interpolate data sampled by user defiend sampling procedures
  if (PIC::IndividualModelSampling::InterpolateCenterNodeData.size()!=0) {
    for (unsigned int ifunc=0;ifunc<PIC::IndividualModelSampling::PrintVariableList.size();ifunc++) PIC::IndividualModelSampling::InterpolateCenterNodeData[ifunc](InterpolationList,InterpolationCoefficients,nInterpolationCoefficients,this);
  }
}

/*
_TARGET_GLOBAL_ 
void PIC::Mesh::AllocateMesh() {
  if (GPU::mesh==NULL) {
    //allocate mesh
    #if DIM == 3
    GPU::mesh=new cMeshAMR3d<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>[1];
    #elif DIM == 2
    GPU::mesh=new cMeshAMR2d<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>[1];
    #else
    GPU::mesh=new cMeshAMR1d<PIC::Mesh::cDataCornerNode,PIC::Mesh::cDataCenterNode,PIC::Mesh::cDataBlockAMR>[1];
    #endif
  }
}
 */

void PIC::Mesh::initCellSamplingDataBuffer() {
  //  if (cDataBlockAMR::totalAssociatedDataLength!=0) exit(__LINE__,__FILE__,"Error: reinitialization of the blocks associated data offsets");

  //local time step
#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_LOCAL_TIME_STEP_
  if (PIC::ThisThread==0) cout << "$PREFIX:Time step mode: specie dependent local time step" << endl;
  cDataBlockAMR_static_data::LocalTimeStepOffset=cDataBlockAMR::RequestInternalBlockData(sizeof(double)*PIC::nTotalSpecies);
#elif _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  //do nothing for _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
#elif _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_
  //do nothing for _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_
#else
  exit(__LINE__,__FILE__,"not implemented");
#endif

  //local particle weight
#if _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_LOCAL_PARTICLE_WEIGHT_
  if (PIC::ThisThread==0) cout << "$PREFIX:Particle weight mode: specie dependent local weight" << endl;
  cDataBlockAMR_static_data::LocalParticleWeightOffset=cDataBlockAMR::RequestInternalBlockData(sizeof(double)*PIC::nTotalSpecies);
#elif _SIMULATION_PARTICLE_WEIGHT_MODE_ ==_SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
  //do nothing for _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
#elif _SIMULATION_PARTICLE_WEIGHT_MODE_ ==_SINGLE_GLOBAL_PARTICLE_WEIGHT_
  //do nothing for _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SINGLE_GLOBAL_PARTICLE_WEIGHT_
#else
  exit(__LINE__,__FILE__,"not implemented");
#endif

  //set up the offsets for 'center node' sampled data
  long int offset=0;

  if (_PIC_SAMPLING_MODE_==_PIC_MODE_ON_) {
    DatumParticleWeight.activate(offset, &DataSampledCenterNodeActive);
    DatumNumberDensity.activate(offset, &DataSampledCenterNodeActive);
    DatumParticleNumber.activate(offset, &DataSampledCenterNodeActive);
    DatumParticleVelocity.activate(offset, &DataSampledCenterNodeActive);
    DatumParticleVelocity2.activate(offset, &DataSampledCenterNodeActive);

    if (_PIC_SAMPLE__VELOCITY_TENSOR_MODE_==_PIC_MODE_ON_) {
      DatumParticleVelocity2Tensor.activate(offset, &DataSampledCenterNodeActive);
    }

    DatumParticleSpeed.activate(offset, &DataSampledCenterNodeActive);
    DatumTranslationalTemperature.activate(&cDataCenterNode::GetTranslationalTemperature, &DataDerivedCenterNodeActive);

    //sampling the 'parallel' and 'tangential' kinetic temperatures
    if (_PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE_ != _PIC_SAMPLE__PARALLEL_TANGENTIAL_TEMPERATURE__MODE__OFF_) {
      DatumParallelTantentialTemperatureSample_Velocity.activate(offset, &DataSampledCenterNodeActive);
      DatumParallelTantentialTemperatureSample_Velocity2.activate(offset,&DataSampledCenterNodeActive);
      DatumParallelTranslationalTemperature.activate(&cDataCenterNode::GetParallelTranslationalTemperature, &DataDerivedCenterNodeActive);
      DatumTangentialTranslationalTemperature.activate(&cDataCenterNode::GetTangentialTranslationalTemperature, &DataDerivedCenterNodeActive);
    }

    //check if user defined sampling data is requested
    if (PIC::IndividualModelSampling::DataSampledList.size()!=0) {
      for (unsigned int i=0;i<PIC::IndividualModelSampling::DataSampledList.size();i++) PIC::IndividualModelSampling::DataSampledList[i]->activate(offset, &DataSampledCenterNodeActive);
    }

    if (PIC::IndividualModelSampling::RequestSamplingData.size()!=0) {
      for (unsigned int i=0;i<PIC::IndividualModelSampling::RequestSamplingData.size();i++) offset+=PIC::IndividualModelSampling::RequestSamplingData[i](offset);
    }
  }

  PIC::Mesh::sampleSetDataLength=offset;
  PIC::Mesh::completedCellSampleDataPointerOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;

  if ((PIC::SamplingMode!=_DISABLED_SAMPLING_MODE_)&&(_PIC_STORE_PREVIOUS_CYCLE_SAMPLE_MODE_==_PIC_MODE_ON_)) {
    PIC::Mesh::collectingCellSampleDataPointerOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+PIC::Mesh::sampleSetDataLength;
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=2*PIC::Mesh::sampleSetDataLength;
  }
  else {
    PIC::Mesh::collectingCellSampleDataPointerOffset=PIC::Mesh::completedCellSampleDataPointerOffset;
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=PIC::Mesh::sampleSetDataLength;
  }


  //the volume partilce injection: save the volume particle injection rate
  if (_PIC_VOLUME_PARTICLE_INJECTION_MODE_ == _PIC_VOLUME_PARTICLE_INJECTION_MODE__ON_) {
    PIC::Mesh::cDataCenterNode_static_data::LocalParticleVolumeInjectionRateOffset=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength;
    PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=sizeof(double)*PIC::nTotalSpecies;
  }

  //allocate the model requested static (not sampling) cell data
  if (PIC::IndividualModelSampling::RequestStaticCellData.size()!=0) {
    for (unsigned int i=0;i<PIC::IndividualModelSampling::RequestStaticCellData.size();i++) {
      PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength+=PIC::IndividualModelSampling::RequestStaticCellData[i](PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength);
    }
  }

  //allocate the model requested static (not sampling) cell's corner data
  if (PIC::IndividualModelSampling::RequestStaticCellCornerData->size()!=0) {
    for (unsigned int i=0;i<PIC::IndividualModelSampling::RequestStaticCellCornerData->size();i++) {
      PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength+=(*PIC::IndividualModelSampling::RequestStaticCellCornerData)[i](PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength);
    }
  }
}


//flush and switch the sampling buffers in 'center' nodes
void PIC::Mesh::flushCompletedSamplingBuffer(cDataCenterNode* node) {
  int i,length=PIC::Mesh::sampleSetDataLength/sizeof(double);
  double *ptr;

  for (i=0,ptr=(double*)(node->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset);i<length;i++,ptr++) *ptr=0.0;
}

void PIC::Mesh::flushCollectingSamplingBuffer(cDataCenterNode* node) {
  int i,length=PIC::Mesh::sampleSetDataLength/sizeof(double);
  double *ptr;

  for (i=0,ptr=(double*)(node->GetAssociatedDataBufferPointer()+PIC::Mesh::collectingCellSampleDataPointerOffset);i<length;i++,ptr++) *ptr=0.0;
}

void PIC::Mesh::switchSamplingBuffers() {
  int tempOffset;

  //exchange the CellSampleData offsets
  tempOffset=PIC::Mesh::completedCellSampleDataPointerOffset;
  PIC::Mesh::completedCellSampleDataPointerOffset=PIC::Mesh::collectingCellSampleDataPointerOffset;
  PIC::Mesh::collectingCellSampleDataPointerOffset=tempOffset;

  //switch the offsets for the internal spherical surfaces installed into the mesh
  PIC::BC::InternalBoundary::Sphere::switchSamplingBuffers();

}

//==============================================================
//init set and set up the computational mesh
void PIC::Mesh::Init(double* xMin,double* xMax,fLocalMeshResolution ResolutionFunction) {

  for (int idim=0;idim<DIM;idim++) xmin[idim]=xMin[idim],xmax[idim]=xMax[idim];

  LocalMeshResolution=ResolutionFunction;
  mesh->init(xMin,xMax,LocalMeshResolution);
}

void PIC::Mesh::buildMesh() {
  mesh->buildMesh();
}


//pack block data for the data syncronization
_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::PackBlockData(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,char* SendDataBuffer) {
  int ibegin=0;
  int BlockUserDataLength=PIC::Mesh::cDataBlockAMR_static_data::totalAssociatedDataLength-PIC::Mesh::cDataBlockAMR_static_data::UserAssociatedDataOffset;

  return PackBlockData_Internal(NodeTable,NodeTableLength,NodeDataLength,NULL,NULL,SendDataBuffer,
      &ibegin,&PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength,1,
      &ibegin,&PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength,1,
      &ibegin,&BlockUserDataLength,1);
}

_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::PackBlockData(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* SendDataBuffer) {
  int ibegin=0;
  int BlockUserDataLength=PIC::Mesh::cDataBlockAMR_static_data::totalAssociatedDataLength-PIC::Mesh::cDataBlockAMR_static_data::UserAssociatedDataOffset;

  return PackBlockData_Internal(NodeTable,NodeTableLength,NodeDataLength,
      BlockCenterNodeMask,BlockCornerNodeMask, 
      SendDataBuffer,
      &ibegin,&PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength,1,
      &ibegin,&PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength,1,
      &ibegin,&BlockUserDataLength,1);
}

_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::PackBlockData_Internal(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,char* SendDataBuffer,
    int* iCornerNodeStateVectorIntervalBegin,int *CornerNodeStateVectorIntervalLength,int nCornerNodeStateVectorIntervals,
    int* iCenterNodeStateVectorIntervalBegin,int *CenterNodeStateVectorIntervalLength,int nCenterNodeStateVectorIntervals,
    int* iBlockUserDataStateVectorIntervalBegin,int *iBlockUserDataStateVectorIntervalLength,int nBlocktateVectorIntervals) {

  return PIC::Mesh::PackBlockData_Internal(NodeTable,NodeTableLength,NodeDataLength,NULL,NULL,SendDataBuffer,
      iCornerNodeStateVectorIntervalBegin,CornerNodeStateVectorIntervalLength,nCornerNodeStateVectorIntervals,
      iCenterNodeStateVectorIntervalBegin,CenterNodeStateVectorIntervalLength,nCenterNodeStateVectorIntervals,
      iBlockUserDataStateVectorIntervalBegin,iBlockUserDataStateVectorIntervalLength,nBlocktateVectorIntervals);
}

_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::PackBlockData_Internal(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned long int* NodeDataLength,
    unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,
    char* SendDataBuffer,
    int* iCornerNodeStateVectorIntervalBegin,int *CornerNodeStateVectorIntervalLength,int nCornerNodeStateVectorIntervals,
    int* iCenterNodeStateVectorIntervalBegin,int *CenterNodeStateVectorIntervalLength,int nCenterNodeStateVectorIntervals,
    int* iBlockUserDataStateVectorIntervalBegin,int *iBlockUserDataStateVectorIntervalLength,int nBlocktateVectorIntervals) {

  using namespace PIC::Mesh::cDataBlockAMR_static_data;

  //  #ifdef __CUDA_ARCH__ 
  //  cAmpsMesh<cDataCornerNode,cDataCenterNode,cDataBlockAMR>  *mesh=GPU::mesh;
  //  #else
  //  cAmpsMesh<cDataCornerNode,cDataCenterNode,cDataBlockAMR>  *mesh=CPU::mesh;
  //  #endif


  int SendBufferIndex=0;

  int CenterNodeSendMaskLength=BlockElementSendMask::CenterNode::GetSize();
  int CornerNodeSendMaskLength=BlockElementSendMask::CornerNode::GetSize();

  for (int iNode=0;iNode<NodeTableLength;iNode++) if (NodeTable[iNode]->block!=NULL)  {
    int iDataInterval,iCell,jCell,kCell;
    long int LocalCellNumber;
    PIC::Mesh::cDataCenterNode *CenterNode=NULL;
    PIC::Mesh::cDataCornerNode *CornerNode=NULL;
    int BeginSendBufferIndex=SendBufferIndex;

    unsigned char* CurrentBlockCenterNodeMask=(BlockCenterNodeMask!=NULL) ? BlockCenterNodeMask+iNode*CenterNodeSendMaskLength : NULL;
    unsigned char* CurrentBlockCornerNodeMask=(BlockCornerNodeMask!=NULL) ? BlockCornerNodeMask+iNode*CornerNodeSendMaskLength : NULL;

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node=NodeTable[iNode];
    cDataBlockAMR* block=Node->block;
    int iCellMax,jCellMax,kCellMax;

    switch (DIM) {
    case 3:
      iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=_BLOCK_CELLS_Z_;
      break;
    case 2:
      iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=1;
      break;
    case 1:
      iCellMax=_BLOCK_CELLS_X_,jCellMax=1,kCellMax=1;
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the value of the parameter is not recognized");
      break;
    }

    //send the center node associated data
    if (nCenterNodeStateVectorIntervals!=0) for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
      LocalCellNumber=_getCenterNodeLocalNumber(iCell,jCell,kCell);
      CenterNode=block->GetCenterNode(LocalCellNumber);

      if (BlockCenterNodeMask!=NULL) {
        if (BlockElementSendMask::CenterNode::Test(iCell,jCell,kCell,CurrentBlockCenterNodeMask)==false) {
          continue;
        }
      }

      if (SendDataBuffer!=NULL) {
        for (iDataInterval=0;iDataInterval<nCenterNodeStateVectorIntervals;iDataInterval++) {
          memcpy(SendDataBuffer+SendBufferIndex,CenterNode->associatedDataPointer+iCenterNodeStateVectorIntervalBegin[iDataInterval],CenterNodeStateVectorIntervalLength[iDataInterval]);
          SendBufferIndex+=CenterNodeStateVectorIntervalLength[iDataInterval];
        }

        memcpy(SendDataBuffer+SendBufferIndex,&CenterNode->Measure,sizeof(double));
        SendBufferIndex+=sizeof(double);
      }
      else {
        for (iDataInterval=0;iDataInterval<nCenterNodeStateVectorIntervals;iDataInterval++) {
          SendBufferIndex+=CenterNodeStateVectorIntervalLength[iDataInterval];
        }

        SendBufferIndex+=sizeof(double);
      }
    }

    //send the corner node associated data
    //in a block corners with indecies from 0 to 'iCellMax-1' are considered belongs to the block. The corner with index 'iCellMax' is considered belongs to the next block
    if (BlockCornerNodeMask!=NULL) {
      if (nCornerNodeStateVectorIntervals!=0) for (kCell=0;kCell<kCellMax+1;kCell++) for (jCell=0;jCell<jCellMax+1;jCell++) for (iCell=0;iCell<iCellMax+1;iCell++) {
        int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
        CornerNode=block->GetCornerNode(nd);

        if (BlockElementSendMask::CornerNode::Test(iCell,jCell,kCell,CurrentBlockCornerNodeMask)==false) {
          continue;
        }

        if (SendDataBuffer!=NULL) {
          for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
            memcpy(SendDataBuffer+SendBufferIndex,CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],CornerNodeStateVectorIntervalLength[iDataInterval]);
            SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
          }
        }
        else {
          for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
            SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
          }
        }
      }
    }
    else {
      //send the 'internal corners'
      if (nCornerNodeStateVectorIntervals!=0) for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
        int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
        CornerNode=block->GetCornerNode(nd);

        if (SendDataBuffer!=NULL) {
          for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
            memcpy(SendDataBuffer+SendBufferIndex,CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],CornerNodeStateVectorIntervalLength[iDataInterval]);
            SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
          }
        }
        else {
          for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
            SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
          }
        }
      }

      //send 'corners' from the 'right' boundary of the block
      int iface,iFaceTable[3]={1,3,5};
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *NeibNode,*ThisNode=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*) Node;

      if (nCornerNodeStateVectorIntervals!=0) for (int i=0;i<3;i++) {
        iface=iFaceTable[i];

        bool flag=false;

        if (ThisNode!=NULL) if ((NeibNode=ThisNode->GetNeibFace(iface,0,0,mesh))!=NULL) if (NeibNode->RefinmentLevel<ThisNode->RefinmentLevel) flag=true;

        if (flag==true) {
          //the current block has more points than the neibour -> need to send the point that exist in the current block but not exist in the neib block

          switch (iface) {
          case 1:
            //the plane normal to 'x' and at the maximum 'x'
            for (iCell=iCellMax,kCell=1;kCell<kCellMax+1;kCell+=2) for (jCell=1;jCell<jCellMax+1;jCell+=2) {
              int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
              CornerNode=block->GetCornerNode(nd);

              if (SendDataBuffer!=NULL) {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  memcpy(SendDataBuffer+SendBufferIndex,CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],CornerNodeStateVectorIntervalLength[iDataInterval]);
                  SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
              else {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
            }
            break;

          case 3:
            //the plane is normal to the 'y' direction, and is at maximum 'y'
            for (jCell=jCellMax,kCell=1;kCell<kCellMax+1;kCell+=2) for (iCell=1;iCell<iCellMax+1;iCell+=2) {
              int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
              CornerNode=block->GetCornerNode(nd);

              if (SendDataBuffer!=NULL) {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  memcpy(SendDataBuffer+SendBufferIndex,CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],CornerNodeStateVectorIntervalLength[iDataInterval]);
                  SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
              else {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
            }
            break;

          case 5:
            //the plane normal to 'z' and at the maximum 'z'
            for (kCell=kCellMax,jCell=0;jCell<jCellMax;jCell+=2) for (iCell=0;iCell<iCellMax;iCell+=2) {
              int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
              CornerNode=block->GetCornerNode(nd);

              if (SendDataBuffer!=NULL) {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  memcpy(SendDataBuffer+SendBufferIndex,CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],CornerNodeStateVectorIntervalLength[iDataInterval]);
                  SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
              else {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  SendBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
            }
            break;
          }
        }

      }
    }

    if (SendDataBuffer!=NULL) {
      for (iDataInterval=0;iDataInterval<nBlocktateVectorIntervals;iDataInterval++) {
        memcpy(SendDataBuffer+SendBufferIndex,block->associatedDataPointer+UserAssociatedDataOffset+iBlockUserDataStateVectorIntervalBegin[iDataInterval],iBlockUserDataStateVectorIntervalLength[iDataInterval]);
        SendBufferIndex+=iBlockUserDataStateVectorIntervalLength[iDataInterval];
      }
    }
    else {
      for (iDataInterval=0;iDataInterval<nBlocktateVectorIntervals;iDataInterval++) {
        SendBufferIndex+=iBlockUserDataStateVectorIntervalLength[iDataInterval];
      }
    }

    if (NodeDataLength!=NULL) NodeDataLength[iNode]=SendBufferIndex-BeginSendBufferIndex;
  }

  return SendBufferIndex;
}

//unpack data for the data syncronization
_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::UnpackBlockData(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,char* RecvDataBuffer) {
  int ibegin=0;
  int BlockUserDataLength=PIC::Mesh::cDataBlockAMR_static_data::totalAssociatedDataLength-PIC::Mesh::cDataBlockAMR_static_data::UserAssociatedDataOffset;

  return UnpackBlockData_Internal(NodeTable,NodeTableLength,RecvDataBuffer,
      &ibegin,&PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength,1,
      &ibegin,&PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength,1,
      &ibegin,&BlockUserDataLength,1);
}

_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::UnpackBlockData(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,char* RecvDataBuffer) {
  int ibegin=0;
  int BlockUserDataLength=PIC::Mesh::cDataBlockAMR_static_data::totalAssociatedDataLength-PIC::Mesh::cDataBlockAMR_static_data::UserAssociatedDataOffset;

  return UnpackBlockData_Internal(NodeTable,NodeTableLength,
      BlockCenterNodeMask,BlockCornerNodeMask,
      RecvDataBuffer,
      &ibegin,&PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength,1,
      &ibegin,&PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength,1,
      &ibegin,&BlockUserDataLength,1);
}

_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::UnpackBlockData_Internal(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,char* RecvDataBuffer,
    int* iCornerNodeStateVectorIntervalBegin,int *CornerNodeStateVectorIntervalLength,int nCornerNodeStateVectorIntervals,
    int* iCenterNodeStateVectorIntervalBegin,int *CenterNodeStateVectorIntervalLength,int nCenterNodeStateVectorIntervals,
    int* iBlockUserDataStateVectorIntervalBegin,int *iBlockUserDataStateVectorIntervalLength,int nBlocktateVectorIntervals) {
  return UnpackBlockData_Internal(NodeTable, NodeTableLength,NULL,NULL,RecvDataBuffer,
      iCornerNodeStateVectorIntervalBegin,CornerNodeStateVectorIntervalLength,nCornerNodeStateVectorIntervals,
      iCenterNodeStateVectorIntervalBegin,CenterNodeStateVectorIntervalLength,nCenterNodeStateVectorIntervals,
      iBlockUserDataStateVectorIntervalBegin,iBlockUserDataStateVectorIntervalLength,nBlocktateVectorIntervals);
}

_TARGET_DEVICE_ _TARGET_HOST_
int PIC::Mesh::UnpackBlockData_Internal(cTreeNodeAMR<cDataBlockAMR>** NodeTable,int NodeTableLength,
    unsigned char* BlockCenterNodeMask,unsigned char* BlockCornerNodeMask,
    char* RecvDataBuffer,
    int* iCornerNodeStateVectorIntervalBegin,int *CornerNodeStateVectorIntervalLength,int nCornerNodeStateVectorIntervals,
    int* iCenterNodeStateVectorIntervalBegin,int *CenterNodeStateVectorIntervalLength,int nCenterNodeStateVectorIntervals,
    int* iBlockUserDataStateVectorIntervalBegin,int *iBlockUserDataStateVectorIntervalLength,int nBlocktateVectorIntervals) {

  using namespace PIC::Mesh::cDataBlockAMR_static_data;
  int RecvDataBufferIndex=0;

  int CenterNodeSendMaskLength=BlockElementSendMask::CenterNode::GetSize();
  int CornerNodeSendMaskLength=BlockElementSendMask::CornerNode::GetSize();

  //  #ifdef __CUDA_ARCH__ 
  //  cAmpsMesh<cDataCornerNode,cDataCenterNode,cDataBlockAMR>  *mesh=GPU::mesh;
  //  #else
  //  cAmpsMesh<cDataCornerNode,cDataCenterNode,cDataBlockAMR>  *mesh=CPU::mesh;
  //  #endif


  for (int iNode=0;iNode<NodeTableLength;iNode++) if (NodeTable[iNode]->block!=NULL) {
    int iCell,jCell,kCell,iDataInterval;
    long int LocalCellNumber;
    PIC::Mesh::cDataCenterNode *CenterNode=NULL;
    PIC::Mesh::cDataCornerNode *CornerNode=NULL;

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node=NodeTable[iNode];
    PIC::Mesh::cDataBlockAMR *block=Node->block;

    unsigned char* CurrentBlockCenterNodeMask=(BlockCenterNodeMask!=NULL) ? BlockCenterNodeMask+iNode*CenterNodeSendMaskLength : NULL;
    unsigned char* CurrentBlockCornerNodeMask=(BlockCornerNodeMask!=NULL) ? BlockCornerNodeMask+iNode*CornerNodeSendMaskLength : NULL;
    int iCellMax,jCellMax,kCellMax;

    switch (DIM) {
    case 3:
      iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=_BLOCK_CELLS_Z_;
      break;
    case 2:
      iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=1;
      break;
    case 1:
      iCellMax=_BLOCK_CELLS_X_,jCellMax=1,kCellMax=1;
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the value of the parameter is not recognized");
      break;
    }

    //recieve the center node associeated data
    if (nCenterNodeStateVectorIntervals!=0) for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
      LocalCellNumber=_getCenterNodeLocalNumber(iCell,jCell,kCell);
      CenterNode=block->GetCenterNode(LocalCellNumber);

      if (BlockCenterNodeMask!=NULL) {
        if (BlockElementSendMask::CenterNode::Test(iCell,jCell,kCell,CurrentBlockCenterNodeMask)==false) {
          continue;
        }
      }

      if (RecvDataBuffer!=NULL) {
        for (iDataInterval=0;iDataInterval<nCenterNodeStateVectorIntervals;iDataInterval++) {
          memcpy(CenterNode->associatedDataPointer+iCenterNodeStateVectorIntervalBegin[iDataInterval],RecvDataBuffer+RecvDataBufferIndex,CenterNodeStateVectorIntervalLength[iDataInterval]);
          RecvDataBufferIndex+=CenterNodeStateVectorIntervalLength[iDataInterval];
        }

        memcpy(&CenterNode->Measure,RecvDataBuffer+RecvDataBufferIndex,sizeof(double));
        RecvDataBufferIndex+=sizeof(double);
      }
      else {
        for (iDataInterval=0;iDataInterval<nCenterNodeStateVectorIntervals;iDataInterval++) {
          RecvDataBufferIndex+=CenterNodeStateVectorIntervalLength[iDataInterval];
        }

        RecvDataBufferIndex+=sizeof(double);
      }
    }


    if (BlockCornerNodeMask!=NULL) {
      if (nCornerNodeStateVectorIntervals!=0) for (kCell=0;kCell<kCellMax+1;kCell++) for (jCell=0;jCell<jCellMax+1;jCell++) for (iCell=0;iCell<iCellMax+1;iCell++) {
        int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
        CornerNode=block->GetCornerNode(nd);

        if (BlockElementSendMask::CornerNode::Test(iCell,jCell,kCell,CurrentBlockCornerNodeMask)==false) {
          continue;
        }

        if (RecvDataBuffer!=NULL) {
          for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
            memcpy(CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],RecvDataBuffer+RecvDataBufferIndex,CornerNodeStateVectorIntervalLength[iDataInterval]);
            RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
          }
        }
        else {
          for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
            RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
          }
        }
      }
    }
    else {
      //recieve the 'internal' corner node associated data
      if (nCornerNodeStateVectorIntervals!=0) for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
        int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
        CornerNode=block->GetCornerNode(nd);

        if (RecvDataBuffer!=NULL) {
          for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
            memcpy(CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],RecvDataBuffer+RecvDataBufferIndex,CornerNodeStateVectorIntervalLength[iDataInterval]);
            RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
          }
        }
        else {
          for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
            RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
          }
        }
      }

      //recv 'corners' from the 'right' boundary of the block
      int iface,iFaceTable[3]={1,3,5};
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *NeibNode,*ThisNode=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*) Node;

      if (nCornerNodeStateVectorIntervals!=0) for (int i=0;i<3;i++) {
        iface=iFaceTable[i];

        if ((NeibNode=ThisNode->GetNeibFace(iface,0,0,mesh))!=NULL) if (NeibNode->RefinmentLevel<ThisNode->RefinmentLevel) {
          //the current block has more points than the neibour -> need to send the point that exist in the current block but not exist in the neib block

          switch (iface) {
          case 1:
            //the plane normal to 'x' and at the maximum 'x'
            for (iCell=iCellMax,kCell=1;kCell<kCellMax+1;kCell+=2) for (jCell=1;jCell<jCellMax+1;jCell+=2) {
              int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
              CornerNode=block->GetCornerNode(nd);

              if (RecvDataBuffer!=NULL) {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  memcpy(CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],RecvDataBuffer+RecvDataBufferIndex,CornerNodeStateVectorIntervalLength[iDataInterval]);
                  RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
              else {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }

            }
            break;

          case 3:
            //the plane is normal to the 'y' direction, and is at maximum 'y'
            for (jCell=jCellMax,kCell=1;kCell<kCellMax+1;kCell+=2) for (iCell=1;iCell<iCellMax+1;iCell+=2) {
              int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
              CornerNode=block->GetCornerNode(nd);

              if (RecvDataBuffer!=NULL) {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  memcpy(CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],RecvDataBuffer+RecvDataBufferIndex,CornerNodeStateVectorIntervalLength[iDataInterval]);
                  RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
              else {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }

            }
            break;

          case 5:
            //the plane normal to 'z' and at the maximum 'z'
            for (kCell=kCellMax,jCell=0;jCell<jCellMax;jCell+=2) for (iCell=0;iCell<iCellMax;iCell+=2) {
              int nd=_getCornerNodeLocalNumber(iCell,jCell,kCell);
              CornerNode=block->GetCornerNode(nd);

              if (RecvDataBuffer!=NULL) {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  memcpy(CornerNode->associatedDataPointer+iCornerNodeStateVectorIntervalBegin[iDataInterval],RecvDataBuffer+RecvDataBufferIndex,CornerNodeStateVectorIntervalLength[iDataInterval]);
                  RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
              else {
                for (iDataInterval=0;iDataInterval<nCornerNodeStateVectorIntervals;iDataInterval++) {
                  RecvDataBufferIndex+=CornerNodeStateVectorIntervalLength[iDataInterval];
                }
              }
            }
            break;
          }
        }

      }
    }

    if (RecvDataBuffer!=NULL) {
      for (iDataInterval=0;iDataInterval<nBlocktateVectorIntervals;iDataInterval++) {
        memcpy(block->associatedDataPointer+UserAssociatedDataOffset+iBlockUserDataStateVectorIntervalBegin[iDataInterval],RecvDataBuffer+RecvDataBufferIndex,iBlockUserDataStateVectorIntervalLength[iDataInterval]);
        RecvDataBufferIndex+=iBlockUserDataStateVectorIntervalLength[iDataInterval];
      }
    }
    else {
      for (iDataInterval=0;iDataInterval<nBlocktateVectorIntervals;iDataInterval++) {
        RecvDataBufferIndex+=iBlockUserDataStateVectorIntervalLength[iDataInterval];
      }
    }
  }


  return RecvDataBufferIndex;
}


int PIC::Mesh::cDataBlockAMR::sendBoundaryLayerBlockData(CMPI_channel *pipe,void* Node,char *SendDataBuffer) {
  using namespace PIC::Mesh::cDataBlockAMR_static_data;

  int iCell,jCell,kCell;
  long int LocalCellNumber;
  PIC::Mesh::cDataCenterNode *CenterNode=NULL;
  PIC::Mesh::cDataCornerNode *CornerNode=NULL;
  int SendBufferIndex=0;
  int iCellMax,jCellMax,kCellMax;

  switch (DIM) {
  case 3:
    iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=_BLOCK_CELLS_Z_;
    break;
  case 2:
    iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=1;
    break;
  case 1:
    iCellMax=_BLOCK_CELLS_X_,jCellMax=1,kCellMax=1;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the value of the parameter is not recognized");
    break;
  }

  //send the center node associated data
  for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
    LocalCellNumber=getCenterNodeLocalNumber(iCell,jCell,kCell);
    CenterNode=GetCenterNode(LocalCellNumber);

    /*
    pipe->send(CenterNode->associatedDataPointer,cDataCenterNode_static_data::totalAssociatedDataLength);
    pipe->send(CenterNode->Measure);
     */

    if (pipe!=NULL) {
      pipe->send(CenterNode->associatedDataPointer,cDataCenterNode_static_data::totalAssociatedDataLength);
      pipe->send(CenterNode->Measure);
    }
    else if (SendDataBuffer!=NULL) { 
      memcpy(SendDataBuffer+SendBufferIndex,CenterNode->associatedDataPointer,cDataCenterNode_static_data::totalAssociatedDataLength);
      SendBufferIndex+=cDataCenterNode_static_data::totalAssociatedDataLength;

      memcpy(SendDataBuffer+SendBufferIndex,&CenterNode->Measure,sizeof(double));
      SendBufferIndex+=sizeof(double);
    }
    else {
      SendBufferIndex+=cDataCenterNode_static_data::totalAssociatedDataLength+sizeof(double);
    }  

  }

  //send the corner node associated data
  //in a block corners with indecies from 0 to 'iCellMax-1' are considered belongs to the block. The corner with index 'iCellMax' is considered belongs to the next block

  //send the 'internal corners'
  for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
    int nd=getCornerNodeLocalNumber(iCell,jCell,kCell);
    CornerNode=GetCornerNode(nd);

    /*
    pipe->send(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
     */

    if (pipe!=NULL) {
      pipe->send(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
    }
    else if (SendDataBuffer!=NULL) {
      memcpy(SendDataBuffer+SendBufferIndex,CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
      SendBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
    }
    else {
      SendBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
    } 
  }

  //send 'corners' from the 'right' boundary of the block
  int iface,iFaceTable[3]={1,3,5};
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *NeibNode,*ThisNode=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*) Node;

  for (int i=0;i<3;i++) {
    iface=iFaceTable[i];

    bool flag=false; 

    if ((pipe==NULL)&&(SendDataBuffer==NULL)) flag=true;
    if (ThisNode!=NULL) if ((NeibNode=ThisNode->GetNeibFace(iface,0,0,mesh))!=NULL) if (NeibNode->RefinmentLevel<ThisNode->RefinmentLevel) flag=true;

    if (flag==true) {
      //the current block has more points than the neibour -> need to send the point that exist in the current block but not exist in the neib block

      switch (iface) {
      case 1:
        //the plane normal to 'x' and at the maximum 'x'
        for (iCell=iCellMax,kCell=1;kCell<kCellMax+1;kCell+=2) for (jCell=1;jCell<jCellMax+1;jCell+=2) {
          int nd=getCornerNodeLocalNumber(iCell,jCell,kCell);
          CornerNode=GetCornerNode(nd);

          /*
          pipe->send(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
           */

          if (pipe!=NULL) {
            pipe->send(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
          }
          else if (SendDataBuffer!=NULL) {
            memcpy(SendDataBuffer+SendBufferIndex,CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
            SendBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }
          else {
            SendBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }

        }
        break;

      case 3:
        //the plane is normal to the 'y' direction, and is at maximum 'y'
        for (jCell=jCellMax,kCell=1;kCell<kCellMax+1;kCell+=2) for (iCell=1;iCell<iCellMax+1;iCell+=2) {
          int nd=getCornerNodeLocalNumber(iCell,jCell,kCell);
          CornerNode=GetCornerNode(nd);

          /*
          pipe->send(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
           */

          if (pipe!=NULL) {
            pipe->send(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
          }
          else if (SendDataBuffer!=NULL) {
            memcpy(SendDataBuffer+SendBufferIndex,CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
            SendBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }
          else {
            SendBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }

        }
        break;

      case 5:
        //the plane normal to 'z' and at the maximum 'z'
        for (kCell=kCellMax,jCell=0;jCell<jCellMax;jCell+=2) for (iCell=0;iCell<iCellMax;iCell+=2) {
          int nd=getCornerNodeLocalNumber(iCell,jCell,kCell);
          CornerNode=GetCornerNode(nd);

          /*
          pipe->send(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
           */

          if (pipe!=NULL) {
            pipe->send(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
          }
          else if (SendDataBuffer!=NULL) {
            memcpy(SendDataBuffer+SendBufferIndex,CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength);
            SendBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }
          else {
            SendBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }


        }
        break;
      }
    }

  }

  /*
  pipe->send(associatedDataPointer+UserAssociatedDataOffset,totalAssociatedDataLength-UserAssociatedDataOffset);
   */

  if (pipe!=NULL) {
    pipe->send(associatedDataPointer+UserAssociatedDataOffset,totalAssociatedDataLength-UserAssociatedDataOffset);
  }
  else if (SendDataBuffer!=NULL) {
    memcpy(SendDataBuffer+SendBufferIndex,associatedDataPointer+UserAssociatedDataOffset,totalAssociatedDataLength-UserAssociatedDataOffset);
    SendBufferIndex+=totalAssociatedDataLength-UserAssociatedDataOffset;
  }
  else {
    SendBufferIndex+=totalAssociatedDataLength-UserAssociatedDataOffset;
  }

  return SendBufferIndex;
}

void PIC::Mesh::cDataBlockAMR::sendMoveBlockAnotherProcessor(CMPI_channel *pipe,void *Node) {
  int iCell,jCell,kCell;
  long int LocalCellNumber;
  int iCellMax,jCellMax,kCellMax;

  sendBoundaryLayerBlockData(pipe,Node,NULL);

  switch (DIM) {
  case 3:
    iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=_BLOCK_CELLS_Z_;
    break;
  case 2:
    iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=1;
    break;
  case 1:
    iCellMax=_BLOCK_CELLS_X_,jCellMax=1,kCellMax=1;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the value of the parameter is not recognized");
    break;
  }

  //send all blocks' data when the blocks is moved to another processor
  long int Particle,NextParticle;
  char *buffer=new char[PIC::ParticleBuffer::ParticleDataLength];

  const int _CENTRAL_NODE_NUMBER_SIGNAL_=1;
  const int _NEW_PARTICLE_SIGNAL_=       2;
  const int _END_COMMUNICATION_SIGNAL_=  3;

  for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
    Particle=FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)];

    if  (Particle!=-1) {
      LocalCellNumber=PIC::Mesh::mesh->getCenterNodeLocalNumber(iCell,jCell,kCell);
      pipe->send(_CENTRAL_NODE_NUMBER_SIGNAL_);
      pipe->send(LocalCellNumber);

      while (Particle!=-1) {
        PIC::ParticleBuffer::PackParticleData(buffer,Particle);
        pipe->send(_NEW_PARTICLE_SIGNAL_);
        pipe->send(buffer,PIC::ParticleBuffer::ParticleDataLength);

        NextParticle=PIC::ParticleBuffer::GetNext(Particle);
        PIC::ParticleBuffer::DeleteParticle_withoutTrajectoryTermination(Particle);

        Particle=NextParticle;
      }

      FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)]=-1;
    }
  }

  pipe->send(_END_COMMUNICATION_SIGNAL_);
  delete [] buffer;
}

int PIC::Mesh::cDataBlockAMR::recvBoundaryLayerBlockData(CMPI_channel *pipe,int From,void* Node,char *RecvDataBuffer) {
  using namespace PIC::Mesh::cDataBlockAMR_static_data;

  int iCell,jCell,kCell;
  long int LocalCellNumber;
  PIC::Mesh::cDataCenterNode *CenterNode=NULL;
  PIC::Mesh::cDataCornerNode *CornerNode=NULL;
  int RecvDataBufferIndex=0;
  int iCellMax,jCellMax,kCellMax;

  switch (DIM) {
  case 3:
    iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=_BLOCK_CELLS_Z_;
    break;
  case 2:
    iCellMax=_BLOCK_CELLS_X_,jCellMax=_BLOCK_CELLS_Y_,kCellMax=1;
    break;
  case 1:
    iCellMax=_BLOCK_CELLS_X_,jCellMax=1,kCellMax=1;
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the value of the parameter is not recognized");
    break;
  }

  //recieve the center node associeated data
  for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
    LocalCellNumber=getCenterNodeLocalNumber(iCell,jCell,kCell);
    CenterNode=GetCenterNode(LocalCellNumber);

    /*
    pipe->recv(CenterNode->associatedDataPointer,cDataCenterNode_static_data::totalAssociatedDataLength,From);
    pipe->recv(CenterNode->Measure,From);
     */

    if (pipe!=NULL) {
      pipe->recv(CenterNode->associatedDataPointer,cDataCenterNode_static_data::totalAssociatedDataLength,From);
      pipe->recv(CenterNode->Measure,From);
    }
    else if (RecvDataBuffer!=NULL) {
      memcpy(CenterNode->associatedDataPointer,RecvDataBuffer+RecvDataBufferIndex,cDataCenterNode_static_data::totalAssociatedDataLength);
      RecvDataBufferIndex+=cDataCenterNode_static_data::totalAssociatedDataLength;

      memcpy(&CenterNode->Measure,RecvDataBuffer+RecvDataBufferIndex,sizeof(double));
      RecvDataBufferIndex+=sizeof(double);
    }
    else {
      RecvDataBufferIndex+=cDataCenterNode_static_data::totalAssociatedDataLength+sizeof(double);
    }
  }

  //recieve the 'internal' corner node associated data
  for (kCell=0;kCell<kCellMax;kCell++) for (jCell=0;jCell<jCellMax;jCell++) for (iCell=0;iCell<iCellMax;iCell++) {
    int nd=getCornerNodeLocalNumber(iCell,jCell,kCell);
    CornerNode=GetCornerNode(nd);

    /*
    pipe->recv(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength,From);
     */

    if (pipe!=NULL) {
      pipe->recv(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength,From);
    }
    else if (RecvDataBuffer!=NULL) {
      memcpy(CornerNode->associatedDataPointer,RecvDataBuffer+RecvDataBufferIndex,cDataCornerNode_static_data::totalAssociatedDataLength);
      RecvDataBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
    }
    else {
      RecvDataBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
    }
  }

  //recv 'corners' from the 'right' boundary of the block
  int iface,iFaceTable[3]={1,3,5};
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *NeibNode,*ThisNode=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*) Node;

  for (int i=0;i<3;i++) {
    iface=iFaceTable[i];

    if ((NeibNode=ThisNode->GetNeibFace(iface,0,0,PIC::Mesh::mesh))!=NULL) if (NeibNode->RefinmentLevel<ThisNode->RefinmentLevel) {
      //the current block has more points than the neibour -> need to send the point that exist in the current block but not exist in the neib block

      switch (iface) {
      case 1:
        //the plane normal to 'x' and at the maximum 'x'
        for (iCell=iCellMax,kCell=1;kCell<kCellMax+1;kCell+=2) for (jCell=1;jCell<jCellMax+1;jCell+=2) {
          int nd=getCornerNodeLocalNumber(iCell,jCell,kCell);
          CornerNode=GetCornerNode(nd);

          /*
          pipe->recv(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength,From);
           */

          if (pipe!=NULL) {
            pipe->recv(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength,From);
          }
          else if (RecvDataBuffer!=NULL) {
            memcpy(CornerNode->associatedDataPointer,RecvDataBuffer+RecvDataBufferIndex,cDataCornerNode_static_data::totalAssociatedDataLength);
            RecvDataBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }
          else {
            RecvDataBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }

        }
        break;

      case 3:
        //the plane is normal to the 'y' direction, and is at maximum 'y'
        for (jCell=jCellMax,kCell=1;kCell<kCellMax+1;kCell+=2) for (iCell=1;iCell<iCellMax+1;iCell+=2) {
          int nd=getCornerNodeLocalNumber(iCell,jCell,kCell);
          CornerNode=GetCornerNode(nd);

          /*
          pipe->recv(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength,From);
           */

          if (pipe!=NULL) {
            pipe->recv(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength,From);
          }
          else if (RecvDataBuffer!=NULL) {
            memcpy(CornerNode->associatedDataPointer,RecvDataBuffer+RecvDataBufferIndex,cDataCornerNode_static_data::totalAssociatedDataLength);
            RecvDataBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }
          else {
            RecvDataBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }

        }
        break;

      case 5:
        //the plane normal to 'z' and at the maximum 'z'
        for (kCell=kCellMax,jCell=0;jCell<jCellMax;jCell+=2) for (iCell=0;iCell<iCellMax;iCell+=2) {
          int nd=getCornerNodeLocalNumber(iCell,jCell,kCell);
          CornerNode=GetCornerNode(nd);

          /*
          pipe->recv(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength,From);
           */

          if (pipe!=NULL) {
            pipe->recv(CornerNode->associatedDataPointer,cDataCornerNode_static_data::totalAssociatedDataLength,From);
          }
          else if (RecvDataBuffer!=NULL) {
            memcpy(CornerNode->associatedDataPointer,RecvDataBuffer+RecvDataBufferIndex,cDataCornerNode_static_data::totalAssociatedDataLength);
            RecvDataBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }
          else {
            RecvDataBufferIndex+=cDataCornerNode_static_data::totalAssociatedDataLength;
          }



        }
        break;
      }
    }

  }

  /*
  pipe->recv(associatedDataPointer+UserAssociatedDataOffset,totalAssociatedDataLength-UserAssociatedDataOffset,From);
   */

  if (pipe!=NULL) {
    pipe->recv(associatedDataPointer+UserAssociatedDataOffset,totalAssociatedDataLength-UserAssociatedDataOffset,From);
  }
  else if (RecvDataBuffer!=NULL) {
    memcpy(associatedDataPointer+UserAssociatedDataOffset,RecvDataBuffer+RecvDataBufferIndex,totalAssociatedDataLength-UserAssociatedDataOffset);
    RecvDataBufferIndex+=totalAssociatedDataLength-UserAssociatedDataOffset;
  }
  else {
    RecvDataBufferIndex+=totalAssociatedDataLength-UserAssociatedDataOffset;
  }

  return RecvDataBufferIndex;
}

//recieve all blocks' data when the blocks is moved to another processo
void PIC::Mesh::cDataBlockAMR::recvMoveBlockAnotherProcessor(CMPI_channel *pipe,int From,void *Node) {
  long int LocalCellNumber=-1;
  int i=-10,j=-10,k=-10;

  recvBoundaryLayerBlockData(pipe,From,Node,NULL);

  long int Particle;
  char buffer[PIC::ParticleBuffer::ParticleDataLength];

  int Signal;
  const int _CENTRAL_NODE_NUMBER_SIGNAL_=1;
  const int _NEW_PARTICLE_SIGNAL_=       2;
  const int _END_COMMUNICATION_SIGNAL_=  3;

  pipe->recv(Signal,From);

  while (Signal!=_END_COMMUNICATION_SIGNAL_) {
    switch (Signal) {
    case _CENTRAL_NODE_NUMBER_SIGNAL_ :
      pipe->recv(LocalCellNumber,From);
      PIC::Mesh::mesh->convertCenterNodeLocalNumber2LocalCoordinates(LocalCellNumber,i,j,k);
      break;
    case _NEW_PARTICLE_SIGNAL_ :
      pipe->recv(buffer,PIC::ParticleBuffer::ParticleDataLength,From);
      Particle=PIC::ParticleBuffer::GetNewParticle(FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],true);
      PIC::ParticleBuffer::UnPackParticleData(buffer,Particle);
      break;
    default :
      exit(__LINE__,__FILE__,"Error: unknown option");
    }

    pipe->recv(Signal,From);
  }
}

//===============================================================================================================
//update the domain decomposition table
void PIC::DomainBlockDecomposition::UpdateBlockTable() {
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;

  if (LastMeshModificationID==PIC::Mesh::mesh->nMeshModificationCounter) return; //no modification of the mesh is made

  //calculate the new number of the blocks
  for (nLocalBlocks=0,node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) if (node->IsUsedInCalculationFlag==true) {
    nLocalBlocks++;
  }

  //deallocate and allocat the block pointe buffer
  //  if (BlockTable!=NULL) delete [] BlockTable;
  //  BlockTable=new cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* [nLocalBlocks];

  if (BlockTable!=NULL) amps_free_managed(BlockTable);
  amps_malloc_managed<cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*>(BlockTable,nLocalBlocks);

  //populate the block pointer buffer
  for (nLocalBlocks=0,node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) if (node->IsUsedInCalculationFlag==true) {
    BlockTable[nLocalBlocks++]=node;
  }

  //update the domain decomposition table ID
  LastMeshModificationID=PIC::Mesh::mesh->nMeshModificationCounter;

  //rebuld the matrix of the linear equatuion solver that has a 'global' matrix distributed over multiple MPI processes
  for (auto f : PIC::LinearSolverTable) f->RebuildMatrix();
}


//===============================================================================================================
//get the interpolation stencil for visualization of the model results (used only when the linear interpolation routine is set)
int PIC::Mesh::GetCenterNodesInterpolationCoefficients(double *x,double *CoefficientsList,PIC::Mesh::cDataCenterNode **InterpolationStencil,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode,int nMaxCoefficients) {
  int iCell,cnt=0;
  double SumWeight=0.0;

  //  if (_PIC_COUPLER__INTERPOLATION_MODE_ != _PIC_COUPLER__INTERPOLATION_MODE__CELL_CENTERED_LINEAR_) {
  //    exit(__LINE__,__FILE__,"Error: the function should be used only when the linear interpolation routine is set");
  //  }

  //construct the interpolation stencil
#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  int ThreadOpenMP=omp_get_thread_num();
#else
  int ThreadOpenMP=0;
#endif

  PIC::CPLR::InitInterpolationStencil(x,startNode);

  //if the length of the coefficient list is not enough -> exist with an error message
  if (nMaxCoefficients<PIC::InterpolationRoutines::CellCentered::StencilTable[ThreadOpenMP].Length) {
    exit(__LINE__,__FILE__,"The length of the interpolation stencil is too short");
    return -1;
  }

  for (iCell=0;iCell<PIC::InterpolationRoutines::CellCentered::StencilTable[ThreadOpenMP].Length;iCell++) {
    if (PIC::InterpolationRoutines::CellCentered::StencilTable[ThreadOpenMP].cell[iCell]->Measure>0.0) {
      CoefficientsList[cnt]=PIC::InterpolationRoutines::CellCentered::StencilTable[ThreadOpenMP].Weight[iCell];
      InterpolationStencil[cnt]=PIC::InterpolationRoutines::CellCentered::StencilTable[ThreadOpenMP].cell[iCell];

      SumWeight+=CoefficientsList[cnt];
      cnt++;
    }
  }

  if (cnt!=0) for (int ii=0;ii<cnt;ii++) CoefficientsList[ii]/=SumWeight;

  return cnt;
}



//===============================================================================================================
//cell dat aaccess routines
/*
void PIC::Mesh::cDataCenterNode::SampleDatum(Datum::cDatumSampled Datum, double* In, int spec, double weight) {
  for(int i=0; i<Datum.length; i++) {
 *(i + Datum.length * spec + (double*)(associatedDataPointer + collectingCellSampleDataPointerOffset+Datum.offset))+= In[i] * weight;
  }
}

void PIC::Mesh::cDataCenterNode::SampleDatum(Datum::cDatumSampled Datum, double In, int spec,  double weight) {
 *(spec + (double*)(associatedDataPointer + collectingCellSampleDataPointerOffset+Datum.offset))+= In * weight;
}

//.......................................................................
void PIC::Mesh::cDataCenterNode::SetDatum(Datum::cDatumSampled Datum, double* In, int spec) {
  for(int i=0; i<Datum.length; i++) *(i + Datum.length * spec + (double*)(associatedDataPointer + completedCellSampleDataPointerOffset+Datum.offset)) = In[i];
}
 */

//get accumulated data
//.......................................................................
void PIC::Mesh::cDataCenterNode::GetDatumCumulative(Datum::cDatumSampled Datum, double* Out, int spec) {
  if (Datum.offset>=0) for (int i=0; i<Datum.length; i++) Out[i] = *(i + Datum.length * spec + (double*)(associatedDataPointer + completedCellSampleDataPointerOffset+Datum.offset));
}

double PIC::Mesh::cDataCenterNode::GetDatumCumulative(Datum::cDatumSampled Datum, int spec) {
  return (Datum.offset>=0) ? *(spec + (double*)(associatedDataPointer + completedCellSampleDataPointerOffset+Datum.offset)) : 0.0;
}

//get data averaged over time
void PIC::Mesh::cDataCenterNode::GetDatumAverage(cDatumTimed Datum, double* Out, int spec) {
  if (Datum.offset>=0) {
    if (PIC::LastSampleLength > 0) for (int i=0; i<Datum.length; i++) {
      Out[i] = *(i + Datum.length * spec + (double*)(associatedDataPointer + completedCellSampleDataPointerOffset+Datum.offset)) / PIC::LastSampleLength;
    }
    else for (int i=0; i<Datum.length; i++) Out[i] = 0.0;
  }
}

double PIC::Mesh::cDataCenterNode::GetDatumAverage(cDatumTimed Datum, int spec) {
  return ((PIC::LastSampleLength>0)&&(Datum.offset>=0)) ?  *(spec + (double*)(associatedDataPointer + completedCellSampleDataPointerOffset+Datum.offset)) / PIC::LastSampleLength : 0.0;
}

//get data averaged over sampled weight
void PIC::Mesh::cDataCenterNode::GetDatumAverage(cDatumWeighted Datum, double* Out, int spec) {
  double TotalWeight=0.0;

  if (Datum.offset>=0) {
    GetDatumCumulative(DatumParticleWeight, &TotalWeight, spec);

    if (TotalWeight > 0) {
      for(int i=0; i<Datum.length; i++) Out[i] = *(i + Datum.length * spec + (double*)(associatedDataPointer + completedCellSampleDataPointerOffset+Datum.offset)) / TotalWeight;
    }
    else for(int i=0; i<Datum.length; i++) Out[i] = 0.0;
  }
}

double PIC::Mesh::cDataCenterNode::GetDatumAverage(cDatumWeighted Datum, int spec) {
  double TotalWeight=0.0;

  GetDatumCumulative(DatumParticleWeight, &TotalWeight, spec);
  return ((TotalWeight>0)&&(Datum.offset>=0)) ? *(spec + (double*)(associatedDataPointer + completedCellSampleDataPointerOffset+Datum.offset)) / TotalWeight : 0.0;
}

//get average for derived data
void PIC::Mesh::cDataCenterNode::GetDatumAverage(cDatumDerived Datum, double* Out, int spec) {
  if (Datum.is_active()==true) (this->*Datum.GetAverage)(Out,spec);
}
//-----------------------------------------------------------------------

//backward compatible access
//-----------------------------------------------------------------------
double PIC::Mesh::cDataCenterNode::GetNumberDensity(int spec) {
  return GetDatumAverage(DatumNumberDensity, spec);
}

void PIC::Mesh::cDataCenterNode::GetBulkVelocity(double* vOut, int spec) {
  GetDatumAverage(DatumParticleVelocity, vOut, spec);
}

double PIC::Mesh::cDataCenterNode::GetMeanParticleSpeed(int spec) {
  return GetDatumAverage(DatumParticleSpeed, spec);
}

double PIC::Mesh::cDataCenterNode::GetCompleteSampleCellParticleWeight(int spec) {
  return GetDatumAverage(DatumParticleWeight, spec);
}
//-----------------------------------------------------------------------

// data interpolation
//-----------------------------------------------------------------------
void PIC::Mesh::cDataCenterNode::InterpolateDatum(Datum::cDatumSampled Datum, cDataCenterNode** InterpolationList,double *InterpolationCoefficients,int nInterpolationCoefficients, int spec) {
  // container for the interpolated value; set it to be zero
  if (Datum.offset>=0) {
    double value[Datum.length], interpolated[Datum.length];

    for (int i=0; i<Datum.length; i++) {
      value[i]=0.0; interpolated[i]=0.0;
    }

    // interpolation loop
    for (int i=0; i<nInterpolationCoefficients; i++) {
      InterpolationList[i]->GetDatumCumulative(Datum, value, spec);

      for(int j=0; j<Datum.length; j++) interpolated[j] += InterpolationCoefficients[i] * value[j];
    }

    SetDatum(&Datum, interpolated, spec);

    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_)
      if (_PIC_DEBUGGER_MODE__CHECK_FINITE_NUMBER_ == _PIC_DEBUGGER_MODE_ON_) {
        for(int i=0; i<Datum.length; i++) {
          if (isfinite(interpolated[i])==false) {
            exit(__LINE__,__FILE__,"Error: Floating Point Exception");
          }
        }
      }
  }
}


//-----------------------------------------------------------------------
//get signature of the AMR tree
unsigned int PIC::Mesh::GetMeshTreeSignature(void *startNodeIn,int nline,const char* fname) {
  static CRC32 Signature;
  static CRC32 CutFaceNumberSignature;
  static CRC32 NeibCutFaceListDescriptorList;
  static int BottomNodeCounter=0;

  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *)startNodeIn;

  if ((startNode==PIC::Mesh::mesh->rootTree)||(startNode==NULL)) {
    Signature.clear();
    CutFaceNumberSignature.clear();
    NeibCutFaceListDescriptorList.clear();
    BottomNodeCounter=0;

    if (startNode==NULL) {
      if (PIC::Mesh::mesh->rootTree==NULL) return 0;
      else startNode=PIC::Mesh::mesh->rootTree;
    }
  }

  if (startNode->lastBranchFlag()!=_BOTTOM_BRANCH_TREE_) {
    for (int i=0;i<(1<<DIM);i++) if (startNode->downNode[i]!=NULL) {
      Signature.add(i);
      GetMeshTreeSignature(startNode->downNode[i],nline,fname);
    }
  }
  else {
    BottomNodeCounter++;

    //add the list of the cut faces accessable directely from the block
    if (startNode->FirstTriangleCutFace!=NULL) {
      int cnt=0;

      for (CutCell::cTriangleFaceDescriptor* tr=startNode->FirstTriangleCutFace;tr!=NULL;tr=tr->next) cnt++;

      CutFaceNumberSignature.add(BottomNodeCounter);
      CutFaceNumberSignature.add(cnt);
    }

    //add the list of the cut-faces accessable throught the block neibours
#if _USER_DEFINED_INTERNAL_BOUNDARY_NASTRAN_SURFACE_MODE_ == _USER_DEFINED_INTERNAL_BOUNDARY_NASTRAN_SURFACE_MODE_ON_
    if (startNode->neibCutFaceListDescriptorList!=NULL) {
      int cnt=0;

      for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>::cCutFaceListDescriptor *t=startNode->neibCutFaceListDescriptorList;t!=NULL;t=t->next) cnt++;

      NeibCutFaceListDescriptorList.add(BottomNodeCounter);
      NeibCutFaceListDescriptorList.add(cnt);
    }
#endif
  }

  //output calculated signatures
  if (startNode==PIC::Mesh::mesh->rootTree) {
    char msg[300];

    sprintf(msg,"Mesh AMR Tree Signature (line=%i, file=%s)",nline,fname);
    Signature.PrintChecksum(msg);

    sprintf(msg,"Mesh AMR Tree Cut-Face Signature (line=%i, file=%s)",nline,fname);
    CutFaceNumberSignature.PrintChecksum(msg);

    sprintf(msg,"Mesh AMR Tree Neib Cut Face Descriptor List Signature (line=%i, file=%s)",nline,fname);
    NeibCutFaceListDescriptorList.PrintChecksum(msg);
  }

  return Signature.checksum();
}

//=====================================================================================
//reset values of the 'corner' node associated data vector
void PIC::Mesh::SetCornerNodeAssociatedDataValue(void *DataBuffer,int DataBufferLength,int DataBufferOffset) {
  int i,j,k;

  for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];

    PIC::Mesh::cDataBlockAMR *block=node->block;

    if (block!=NULL) for (i=0;i<_BLOCK_CELLS_X_+1;i++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (k=0;k<_BLOCK_CELLS_Z_+1;k++) {
      PIC::Mesh::cDataCornerNode *CornerNode=block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));

      if (CornerNode!=NULL) memcpy(CornerNode->GetAssociatedDataBufferPointer()+DataBufferOffset,DataBuffer,DataBufferLength);
    }
  }

  for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) for (auto node=PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];node!=NULL;node=node->nextNodeThisThread) {
    PIC::Mesh::cDataBlockAMR *block=node->block;

    if (block!=NULL) for (i=0;i<_BLOCK_CELLS_X_+1;i++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (k=0;k<_BLOCK_CELLS_Z_+1;k++) {
      PIC::Mesh::cDataCornerNode *CornerNode=block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));

      if (CornerNode!=NULL) memcpy(CornerNode->GetAssociatedDataBufferPointer()+DataBufferOffset,DataBuffer,DataBufferLength);
    }
  }
}

void PIC::Mesh::SetCornerNodeAssociatedDataValue(double NewValue,int ResetElementNumber,int DataBufferOffset) {
  double DataBuffer[ResetElementNumber];

  //init the data buffer
  for (int i=0;i<ResetElementNumber;i++) DataBuffer[i]=NewValue;

  //reset the associated data vector
  SetCornerNodeAssociatedDataValue(DataBuffer,ResetElementNumber*sizeof(double),DataBufferOffset);
}


//=====================================================================================
//reset values of the 'center' node associated data vector
void PIC::Mesh::SetCenterNodeAssociatedDataValue(void *DataBuffer,int DataBufferLength,int DataBufferOffset) {
  int i,j,k;

  for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];

    PIC::Mesh::cDataBlockAMR *block=node->block;

    if (block!=NULL) for (i=-1;i<_BLOCK_CELLS_X_+1;i++) for (j=-1;j<_BLOCK_CELLS_Y_+1;j++) for (k=-1;k<_BLOCK_CELLS_Z_+1;k++) {
      PIC::Mesh::cDataCenterNode *CenterNode=block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));

      if (CenterNode!=NULL) memcpy(CenterNode->GetAssociatedDataBufferPointer()+DataBufferOffset,DataBuffer,DataBufferLength);
    }
  }

  for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) for (auto node=PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];node!=NULL;node=node->nextNodeThisThread) {
    PIC::Mesh::cDataBlockAMR *block=node->block;

    if (block!=NULL) for (i=-1;i<_BLOCK_CELLS_X_+1;i++) for (j=-1;j<_BLOCK_CELLS_Y_+1;j++) for (k=-1;k<_BLOCK_CELLS_Z_+1;k++) {
      PIC::Mesh::cDataCenterNode *CenterNode=block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));

      if (CenterNode!=NULL) memcpy(CenterNode->GetAssociatedDataBufferPointer()+DataBufferOffset,DataBuffer,DataBufferLength);
    }
  }
}

void PIC::Mesh::SetCenterNodeAssociatedDataValue(double NewValue,int ResetElementNumber,int DataBufferOffset) {
  double DataBuffer[ResetElementNumber];

  //init the data buffer
  for (int i=0;i<ResetElementNumber;i++) DataBuffer[i]=NewValue;

  //reset the associated data vector
  SetCenterNodeAssociatedDataValue(DataBuffer,ResetElementNumber*sizeof(double),DataBufferOffset);
}


//====================================================
//return the total number of allocated cells in the entire domain
int PIC::Mesh::GetAllocatedCellTotalNumber() {
  std::function<int(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)> GetAllocatedCellNumberInBlock;
  int res=0;

  GetAllocatedCellNumberInBlock = [&] (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) -> int {
    int res=0;

    if (startNode->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
      PIC::Mesh::cDataBlockAMR *block;
      PIC::Mesh::cDataCenterNode *cell;

      if (((block=startNode->block)!=NULL) && (startNode->Thread==PIC::ThisThread)) {
        for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
          for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
            for (int i=0;i<_BLOCK_CELLS_X_;i++) {
              cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
              if (cell->GetAssociatedDataBufferPointer()!=NULL) res++;
            }
          }
        }
      }
    }
    else {
      int iDownNode;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *downNode;

      for (iDownNode=0;iDownNode<(1<<DIM);iDownNode++) if ((downNode=startNode->downNode[iDownNode])!=NULL) {
        res+=GetAllocatedCellNumberInBlock(downNode);
      }
    }


    if (startNode==PIC::Mesh::mesh->rootTree) {
      //sum the number of cells across all subdomains
      int t;

      MPI_Allreduce(&res,&t,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
      res=t;
    }

    return res;
  };

  return GetAllocatedCellNumberInBlock(mesh->rootTree);
}


//==========================================================================================
//copy mesh from the host to the device 
#if _CUDA_MODE_ == _ON_
void PIC::Mesh::GPU::CopyMeshHost2Device() {
  //gather the tree information on the host 

  vector<list<PIC::Mesh::GPU::cNodeData> > TreeStructureTable;
  list<PIC::Mesh::GPU::cNodeData> TreeLevelList;
  list<PIC::Mesh::GPU::cNodeData>::iterator it;
  cNodeData new_entry;
  int i,j,k;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *Node,*DownNode;

  bool next_tree_level_exists=true;
  int iCurrentTreeLevel=0; 

  //prepare the tree structure
  if (PIC::Mesh::mesh->rootTree!=NULL) {
    new_entry.clear();

    new_entry.SplitFlag=true;
    new_entry.AllocatedFlag=false;
    PIC::Mesh::mesh->GetAMRnodeID(new_entry.NodeId,PIC::Mesh::mesh->rootTree); 

    TreeLevelList.push_back(new_entry);
    TreeStructureTable.push_back(TreeLevelList);  

    while (next_tree_level_exists==true) {
      next_tree_level_exists=false;
      TreeLevelList.clear(); 

      for (it=TreeStructureTable[iCurrentTreeLevel].begin();it!=TreeStructureTable[iCurrentTreeLevel].end();it++) {
        Node=PIC::Mesh::mesh->findAMRnodeWithID(it->NodeId);  

        for (i=0;i<(1<<DIM);i++) if ((DownNode=Node->downNode[i])!=NULL) {
          //the next level exists
          next_tree_level_exists=true;
          it->SplitFlag=true;

          //add the new node in the list
          new_entry.clear();

          PIC::Mesh::mesh->GetAMRnodeID(new_entry.NodeId,DownNode);
          if (DownNode->block!=NULL) new_entry.AllocatedFlag=true;

          TreeLevelList.push_back(new_entry);
        }
      }

      TreeStructureTable.push_back(TreeLevelList);
      iCurrentTreeLevel++;
    }
  }

  //create the tree structure on the device 
  for (int iLevel=0;iLevel<iCurrentTreeLevel;iLevel++) {
    int i,nNodes;
    PIC::Mesh::GPU::cNodeData *buffer,*buffer_dev;

    nNodes=TreeStructureTable[iLevel].size();
    buffer=new cNodeData [nNodes];
    amps_new_device(buffer_dev,nNodes);

    for (i=0,it=TreeStructureTable[iLevel].begin();it!=TreeStructureTable[iLevel].end();i++,it++) {
      buffer[i]=*it;
    }

    cudaMemcpy(buffer_dev,buffer,nNodes*sizeof(PIC::Mesh::GPU::cNodeData),cudaMemcpyHostToDevice);

    auto SplitDeviceMeshBlocks = [=] _TARGET_DEVICE_ (PIC::Mesh::GPU::cNodeData *buffer, int nNodes) {
      int i;
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *Node;

      for (i=0;i<nNodes;i++) {
        Node=PIC::Mesh::GPU::mesh->findAMRnodeWithID(buffer[i].NodeId);

        if (buffer[i].SplitFlag==true) {
          PIC::Mesh::GPU::mesh->splitTreeNode(Node);

          if (buffer[i].AllocatedFlag==true) {
            PIC::Mesh::GPU::mesh->AllocateBlock(Node);
          }
        }
      } 
    };  

    kernel_2<<<1,1>>>(SplitDeviceMeshBlocks,buffer,nNodes);
    cudaDeviceSynchronize();

    delete [] buffer;
    amps_free_device(buffer_dev); 
  }
}
#endif







