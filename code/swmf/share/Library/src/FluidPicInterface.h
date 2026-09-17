/*******************************************************
FluidPicInterface class handels all interaction between
a fluid model (BATSRUS/SWMF) and the particle models.

Originally writen by Lars Daldorff (daldorff@umich.edu) 15 Jan 2013

********************************************************/
#ifndef FLUIDPICINTERFACE_H
#define FLUIDPICINTERFACE_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <array>
#include <list>
#include "MDArray.h"
#include "ReadParam.h"
#include "Writer.h"

class TimeController{
 private:
  double dtSI;


  // How to use varying timestep for ECSIM? 

  // 1) At the beginning of one iteration, assume E,B fields and the particle 
  // velocity are at t=t_n, and the particle location is at t=t_n+dt_n/2. 
  // The mass matrix SHOULD be calculated with dtn. E,B fields and 
  // the particle velocity HAVE to be updated from t=t_n to t=t_n+dt_n. BUT, the
  // particle location can be updated to a different time besides
  // t = t_n+dt_n/2*3. 

  // 2) Here is the algorithm for one iteration (update E,B,V_p from t = t_n to
  //    t = t_n+dt_n, update X_p from t=t_n+dt_n/2 to t= t_n+dt_n+dt_{n+1}/2):
  // a) dtSICFL0 and dtSICFL1 are the time steps calculated from the 
  //    CLF conditions at t=t_n and t=t_{n-1}, respectively. 
  // b) dt_n = dtSICFL1
  //    dt_{n+1} = dtSICFL0

  double dtSICFL0, dtSICFL1; 

 public:
  double SItime;

  double cflLimit;
  bool useFixedDt;
  double fixedDtSI; 
  double maxDtSI;

  double Si2NoT;
  double No2SiT; 

 public:
  TimeController(){
    SItime = 0;
    useFixedDt = false; 
    cflLimit = 0.4;
    fixedDtSI = 0;
    maxDtSI = 0;

    Si2NoT = 1;
    No2SiT = 1; 

    dtSICFL0 = -1; 
    dtSICFL1 = -1;
    dtSI = -1;       
  }

  double getDt()const{
    return getSIDt()*Si2NoT; 
  }

  double getSIDt()const{
    return dtSI; 
  }

  double getNextDt()const{
    return (useFixedDt? fixedDtSI:dtSICFL0)*Si2NoT;
  } 

  double getPartLocDt()const{
    return (useFixedDt? fixedDtSI:(dtSICFL1 + dtSICFL0)*0.5)*Si2NoT;
  }

  void calSIDt() {    
    dtSI = useFixedDt? fixedDtSI:dtSICFL1;
  }

  void initDt(double normDt){
    dtSI = normDt*No2SiT; 
    dtSICFL0 = dtSI; 
    dtSICFL1 = dtSI; 
  }
  
  void setSIDt(double dtSIIn){
    dtSI = dtSIIn; 
  }

  void setmaxDtSI(double dtIn){
    maxDtSI = dtIn; 

    dtSICFL1 = dtSICFL0;
    dtSICFL0 = maxDtSI*cflLimit; 
    if(dtSICFL1 <= 0){
      dtSICFL1 = dtSICFL0;
    }

  }

  void updateSItime(){
    SItime +=getSIDt();
  }
};


class FluidPicInterface {
protected:
  static const int iErr = 11;

  bool doCoupleAMPS;

  int nCellPerPatch; 
  
  int nDim; // number of dimentions

  // Min and Max of the physical domain in normalized PIC units.
  double phyMin_D[3], phyMax_D[3];

  // The length of the computational domain in normalized PIC units, including
  // the ghost cell layers.
  double lenGst_D[3];

  // Cell Size
  double dx_D[3];

  // Rotation matrix.
  double R_DD[3][3];

  bool doRotate;

  // Number of cells/nodes in each direction, including the ghost cell layers.
  int nCellGst_D[3], nNodeGst_D[3];

  double SItime; // time in SI units

  int nBlock;

  // Number of variables passing between MHD and PIC.
  int nVarFluid;

  // Number of fluid at the MHD side. One 'fluid' has its own density,
  // velocity and pressure. Electron can be one fluid.
  int nFluid;

  // Number of ion fluid at the MHD side.
  int nIonFluid;

  // Number of species at the MHD side. One 'species' only has its own density.
  int nSpecies;

  // Total number of ion/electron species exit in the fluid code.
  int nIon;

  int nVarCoupling;

  bool useMultiSpecies, useMultiFluid, useElectronFluid;

  // storage for starting/ending physical (not include ghost cell)
  // cell indexes of this processor
  int StartIdx_D[3], EndIdx_D[3];

  static const int NG = 1; // number of ghost cell

  bool useAnisoP; // Use anisotripic pressure

  bool useMhdPe;

  double rPlanetSi;

  double dt;

  double tUnitPic;    // conversenfactor to time used in IPIC3D
  double invtUnitPic; // 1/tUnitPic
  double *Si2No_V;    // array storing unit conversion factors
  double *No2Si_V;    // array storing inverse unit conversion factors
  double Si2NoM, Si2NoV, Si2NoRho, Si2NoB, Si2NoP, Si2NoJ, Si2NoL, Si2NoE;
  double No2SiV, No2SiL;
  double MhdNo2SiL; // Length in BATSRUS normalized unit -> Si
  double Lnorm, Unorm, Mnorm,
      Qnorm; // normalization units for length, velocity, mass and charge
             // Normalized q/m ==1 for proton in CGS units

  double ScalingFactor;

  //-------------------------------------------------------------------
  // nSIn is the number of species exists at the MHD side. PIC may use
  // two or more species to represent one MHD species. nS is the nuber of the
  // PIC species.
  long nS;         // number of particle species
  int nSIn;        // number of particle species before splitting.
  double *MoMi_S;  // masses for the particles species
  double *QoQi_S;  // charge for each particle species
  double *MoMi0_S; // masses for the particles species before splitting
  double *QoQi0_S; // charge for each particle species before splitting
  //-------------------------------------------------------------------

  double PeRatio; // temperature ratio for electrons: PeRatio = Pe/Ptotal
  double SumMass; // Sum of masses of each particle species
  int nOverlap;   // Number of grid point from boundary where we have MHD+PIC
                  // solution
  int nOverlapP;  // Nomber of overlap cells for redistrebute particles
  int nIsotropic, nCharge; // Intrpolation region for curents/pressure
                           // (nIsotropoc) and charge
  // nCharge. <0 : do nothing; ==0 only ghost region; >0 interpolate inside
  // domain

  unsigned long iSyncStep; // Iterator for sync with fluid
  long nSync;              //

  int myrank; // this process mpi rank
  int nProcs;

  bool isFirstTime;

  int iRegion;
  std::string sRegion;

  // Do not include ghost cells.
  int nxcLocal, nycLocal, nzcLocal;

  // Nodes, include ghost cells.
  int nxnLG, nynLG, nznLG;

  // The range of the computtational domain on this processor.
  double xStart, xEnd, yStart, yEnd, zStart, zEnd;
  double *xStart_I, *xEnd_I, *yStart_I, *yEnd_I, *zStart_I, *zEnd_I;

  // Unless it is the first step to initilize PC, otherwise, only boundary
  // information is needed from GM.
  bool doNeedBCOnly;

  int iCycle;

public:
  std::list<Writer> writer_I;

  ReadParam readParam;

  TimeController timeCtr; 

protected:
  static const int x_ = 0, y_ = 1, z_ = 2;

  bool doSubCycling;

  // Variables for IDL format output.
  static const int nDimMax = 3;
  int nPlotFile;
  int *dnOutput_I;
  double *dtOutput_I, *plotDx_I;
  // The second dimension: xmin, xmax, ymin, ymax, zmin, zmax.
  // double **plotRangeMin_ID, **plotRangeMax_ID;
  MDArray<double> plotRangeMin_ID, plotRangeMax_ID;
  std::string *plotString_I;
  std::string *plotVar_I;
  bool doSaveBinary;
  double drSat; // A particle within drSat*dx from a satellite point will be
                // wrote out.

  std::vector<std::vector<std::array<double, 4> > > satInfo_III;

  // Simulation start time.
  int iYear, iMonth, iDay, iHour, iMinute, iSecond;

  // If the maximum thermal velocity of one node exceeds uthLimit, which is in
  // normalized PIC unit, then save the output and stop runing.
  double uthLimit; //

  // 1) If useSWMFDt is true, use the dt given by coupling frequency.
  // 2) If useSWMFDt is false and useFixedDt is true, use fixedDt, which is set
  // with
  //    command #TIMESTEP.
  // 3) If both useSWMFDt and useFixedDt are false, calculate dt using the
  // 'CFL' condition.
  bool useFixedDt;
  double fixedDt; // In SI unit

  bool isPeriodicX, isPeriodicY,
      isPeriodicZ; // Use periodic BC in one direction?

  // Variables for test setup.
  bool doTestEMWave;
  double waveVec_D[3], phase0, amplE_D[3];

  // The same particle species (such as the electrons) can be split into two
  // or more 'species' in the PIC side based on some creteria, such as the
  // polarity of the magnetic field. This feature can be used to distinguish
  // the particles from different sources.
  bool doSplitSpecies;
  std::string splitType;
  int *iSPic2Mhd_I;

public:
  MDArray<double> Bc_BGD; // cell centered B

  // double ****State_BGV; // node centered state variables
  MDArray<double> State_BGV;

  // The min/max location of blocks. Do not include ghost cells.
  MDArray<double> BlockMin_BD, BlockMax_BD, CellSize_BD;

  // The ghost cell number in each direction in each side.
  int nG_D[nDimMax];

  // These variables are also used in PSKOutput.h
  int *iRho_I, *iRhoUx_I, *iRhoUy_I, *iRhoUz_I, iBx, iBy, iBz, iEx, iEy, iEz,
      iPe, *iPpar_I, *iP_I, iJx, iJy, iJz, *iUx_I, *iUy_I, *iUz_I, iRhoTotal;

  int nBCLayer;
  bool useRandomPerCell;
  bool doUseOldRestart;
  std::string testFuncs;
  int iTest, jTest, kTest;

  int nPartGhost;

  // If useUniformPart is true, then assign the particle position uniformly.
  bool useUniformPart;

  // Change smooth coefficient near the boundary.
  // Parameters 'SmoothNiter' and 'Smooth' are declared in Colective.h
  bool doSmoothAll; // Smooth jh and rhoh?.
  double innerSmoothFactor, boundarySmoothFactor;
  double nBoundarySmooth;

  // At most 10 vectors are supported during the coupling.
  static const int nVecMax = 10;
  int vecIdxStart_I[nVecMax], nVec;

public:
  /** constructor */
  FluidPicInterface();

  /** destructor */
  ~FluidPicInterface();

  void InitData();

  void ReNormLength();

  // Convert State_BGV to normalized PIC units.
  void ReNormVariables();

  void Moment2Velocity();

  void ReadFromGMinit(const int *const paramint,
                      const double *const ParamRealRegion,
                      const double *const ParamRealComm,
                      const std::stringstream * const ss);

  void fixPARAM(double *&qom, int *&npcelx, int *&npcely, int *&npcelz,
                int *ns);

  void checkParam();

  /** Get nomal and pendicular vector to magnetic field */
  void MagneticBaseVectors(const double Bx, const double By, const double Bz,
                           MDArray<double> &norm_DD) const;

  void mhd_to_Pic_Vec(double const *vecIn_D, double *vecOut_D,
                      bool isZeroOrigin = false) const;
  void pic_to_Mhd_Vec(double const *vecIn_D, double *vecOut_D,
                      bool isZeroOrigin = false) const;
  std::string addPlasmaVar(std::string varString, int is) const;
  std::string expandVariable(std::string inVars) const;
  double getSmoothFactor(int i, int j, int k) const;
  /** day of year **/
  int doy(int yr, int mo, int dy);
  /** Convert real time into simulation time in second. **/
  double convert_time(int yr, int mo, int dy, int hr, int mn, int sc,
                      double msc);
  int second_to_clock_time(int second);
  void read_satellite_file(std::string filename);
  void PrintFluidPicInterface();
  void setStateVar(double *state_I, int *iPoint_I);
  void GetGridPnt(double *Pos_I);
  void get_region_points(bool doCount, bool doGetPos, bool doSetData,
                         int &nPoint, double *pos_I, double *state_I,
                         int *iPoint_I);
  void GetNgridPnt(int &nPoint);
  bool doGetFromGM(int i, int j, int k);

  void writers_init();
  void writers_write(double timeNow, int iCycle, bool doForceOutput,
                     FuncFindPointList find_output_list, FuncGetField get_var);

  void set_doCoupleAMPS(bool in) { doCoupleAMPS = in; }

  void set_myrank(int i) { myrank = i; }
  void set_nProcs(int i) { nProcs = i; }
  int get_nProcs() const { return nProcs; }

  int get_nCellPerPatch()const{return nCellPerPatch;}
  
  /** get start index in for the total domain */
  int getGlobalStartIndex(int dir) { return (StartIdx_D[dir]); }

  /** get number of cell in overlap region for field solver, used by fields */
  int getnOverlap() { return (nOverlap); }
  void setnOverlap(double param) { nOverlap = param; }

  /** get number of cell in overlap region for quintities relativ for paricle
   * initialization  */
  int getnOverlapP() { return (nOverlapP); }
  void setnOverlapP(double param) { nOverlapP = param; }

  /** get number of cell from the boundary where we push the solution closer to
     a fluid solution,
      used by the Jxyz and P tensor */
  int getnIsotropic() const { return (nIsotropic); }
  void setnIsotropic(double param) { nIsotropic = param; }

  /** get number of cell from the boundary where we push the solution closer to
     a fluid solution,
      used by charge density */
  int getnCharge() const { return (nCharge); }
  void setnCharge(double param) { nCharge = param; }

  /** Number of steps between syncrinosation of the fluid solution */
  int getnSync() { return (nSync); }
  void setnSync(int SyncDn) { nSync = SyncDn; }

  /** Use anisotropisc pressure when seting upt the particle distribution */
  bool getUseAnisoP() const { return (useAnisoP); }
  // void setAnisoP(bool useAnisoPIn){useAnisoP = useAnisoPIn;}

  /** Whether electron pressure is passed between PIC and BATSRUS */
  bool getUseMhdPe() const { return (useMhdPe); }

  bool getUseMultiFluid() const { return (useMultiFluid); }
  bool getUseMultiSpecies() const { return (useMultiSpecies); }
  bool get_useElectronFluid() const { return useElectronFluid; }

  /** Get convertion factor to from IPIC3D internal units */
  inline double getNo2Si_V(int idx) const { return (No2Si_V[idx]); }

  /** Get convertion factor to from IPIC3D internal units */
  inline double getSi2No_V(int idx) const { return (Si2No_V[idx]); }

  /** Get time convertion units to internal IPIC3D units */
  double gettUnitPic() { return (tUnitPic); }

  /** Get time convertion units from internal IPIC3D units */
  double getinvtUnitPic() {
    return (invtUnitPic);
  };

  /** Get time convertion units from internal IPIC3D units */
  void setSItime(double time) {
    //SItime = time;
    timeCtr.SItime = time; 
  };

  /** Get time convertion units from internal IPIC3D units */
  double getSItime() {
    //return (SItime);
    return timeCtr.SItime;
  };


  /** set SI dt */
  void setSIDt(double SIDt) {
    timeCtr.setSIDt(SIDt);
    return;
  }

  double calSIDt() {
    return timeCtr.getSIDt();    
  }

  void updateSItime() {
    timeCtr.updateSItime();
    if (myrank == 0) {
      std::cout << "SItime = " << timeCtr.SItime << " dt (s) = " << timeCtr.getSIDt()
                << " , normalized dt = " << timeCtr.getDt()
                << std::endl;
    }
  }

  /** get fluid time step in IPIC3D units */
  double getFluidDt() const {
    return timeCtr.getDt();
  }

  // The begining 'physical' point of this IPIC region. Assume there is one
  // layer PIC ghost cell.
  double getphyMin(int i) const { return phyMin_D[i]; }
  double getphyMax(int i) const { return phyMax_D[i]; }
  double getdx(int i) const { return dx_D[i]; }
  bool getdoRotate() const { return doRotate; }
  double getRDD(int i, int j) const { return R_DD[i][j]; }

  void setiRegion(int i) {
    iRegion = i;
    std::stringstream ss;
    sRegion = ss.str();
  }
  int getiRegion() const { return (iRegion); }
  std::string getsRegion() const { return sRegion; }

  int getnDim() const { return (nDim); }

  int getnVarFluid() const { return (nVarFluid); }
  int get_nVarCoupling() const { return (nVarCoupling); }
  int getnIon() const { return (nIon); }
  int get_nS() const { return nS; }
  int get_nFluid() const { return (nFluid); }

  double getSi2NoL() const { return (Si2NoL); }
  double getSi2NoT() const {return Si2NoL/Si2NoV; }
  double getNo2SiL() const { return (No2SiL); }
  double getNo2SiRho() const { return (1. / Si2NoRho); }
  double getNo2SiV() const { return (1. / Si2NoV); }
  double getNo2SiB() const { return (1. / Si2NoB); }
  double getNo2SiP() const { return (1. / Si2NoP); }
  double getNo2SiJ() const { return (1. / Si2NoJ); }
  double getNo2SiT() const {return Si2NoV/Si2NoL; }


  int getNxcLocal() const { return nxcLocal; }
  int getNycLocal() const { return nycLocal; }
  int getNzcLocal() const { return nzcLocal; }

  bool getdoNeedBCOnly() const {
    return (doNeedBCOnly);
  };
  void setdoNeedBCOnly(bool doBCOnly) {
    doNeedBCOnly = doBCOnly;
  };
  void setCycle(int iCycleIn) {
    iCycle = iCycleIn;
  };
  int getCycle() const {
    return iCycle;
  };
  bool getUseRandomPerCell() const {
    return useRandomPerCell;
  };
  std::string getTestFunc() const {
    return testFuncs;
  };
  int getiTest() const {
    return iTest;
  };
  int getjTest() const {
    return jTest;
  };
  int getkTest() const {
    return kTest;
  };

  // IDL format output.
  int getnPlotFile() const {
    return nPlotFile;
  };
  int getdnOutput(int i) const {
    return dnOutput_I[i];
  };
  double getdtOutput(int i) const {
    return dtOutput_I[i];
  };
  std::string getplotString(int i) const {
    return plotString_I[i];
  };
  double getplotDx(int i) const {
    return plotDx_I[i] > 0 ? plotDx_I[i] : 1;
  };
  std::string getplotVar(int i) const {
    return plotVar_I[i];
  };
  double getplotRangeMin(int iPlot, int i) const {
    // plotRangeMin_ID is only set from PARAM.in for 'cut'!!!!
    return plotRangeMin_ID(iPlot, i);
  };
  double getplotRangeMax(int iPlot, int i) const {
    // plotRangeMin_ID is only set from PARAM.in for 'cut'!!!!
    return plotRangeMax_ID(iPlot, i);
  };

  bool getdoSaveBinary() const {
    return doSaveBinary;
  };

  void set_doSaveBinary(bool in) {
    doSaveBinary = in; 
  };
  
  double getSatRadius() const {
    return drSat * dx_D[0];
  };
  /* void setmaxDt(double dt) { */
  /*   maxDt = dt / (Si2NoL / Si2NoV); */
  /* }; */
  void setxStart(double v) {
    xStart = v;
    const int iBlock = 0;
    BlockMin_BD(iBlock, x_) = v;
  };
  void setxEnd(double v) {
    xEnd = v;
    const int iBlock = 0;
    BlockMax_BD(iBlock, x_) = v;
  };
  void setyStart(double v) {
    yStart = v;
    const int iBlock = 0;
    BlockMin_BD(iBlock, y_) = v;
  };
  void setyEnd(double v) {
    yEnd = v;
    const int iBlock = 0;
    BlockMax_BD(iBlock, y_) = v;
  };
  void setzStart(double v) {
    zStart = v;
    const int iBlock = 0;
    BlockMin_BD(iBlock, z_) = v;
  };
  void setzEnd(double v) {
    zEnd = v;
    const int iBlock = 0;
    BlockMax_BD(iBlock, z_) = v;
  };
  int getnPartGhost() const {
    return nPartGhost;
  };
  bool getdoSmoothAll() const {
    return doSmoothAll;
  };
  double getMiSpecies(int i) const {
    return MoMi_S[i];
  };
  double getQiSpecies(int i) const {
    return QoQi_S[i];
  };

  double get_qom(int is) const { return QoQi_S[is] / MoMi_S[is]; }

  double getcLightSI() const {
    return Unorm / 100; /*Unorm is in cgs unit*/
  };

  bool getdoTestEMWave() const {
    return doTestEMWave;
  };
  double getwaveVec_D(int i) const {
    return waveVec_D[i] / Si2NoL;
  };
  double getphase0deg() const {
    return phase0;
  };
  double getamplE_D(int i) const {
    return amplE_D[i];
  };

  // Replace these functions. --Yuxi
  /** return min X for fluid grid without ghostcell */
  inline double getFluidStartX() const { return phyMin_D[x_]; }
  /** return min Y for fluid grid without ghostcell */
  inline double getFluidStartY() const { return phyMin_D[y_]; }
  /** return min Z for fluid grid without ghostcell */
  inline double getFluidStartZ() const { return phyMin_D[z_]; }

  // return planet radius in SI unit.
  inline double getrPlanet() const { return (rPlanetSi); }
  // return MhdNo2SiL
  inline double getMhdNo2SiL() const { return (MhdNo2SiL); }
  // BATSRUS normalized unit -> PIC normalized unit;
  inline double getMhdNo2NoL() const { return (MhdNo2SiL * Si2NoL); }

  bool getdoSubCycling() const { return doSubCycling; }

  double get_uthLimit() const { return uthLimit; }

  bool get_useUniformPart() const { return useUniformPart; }

  bool get_doSplitSpecies() const { return doSplitSpecies; }

  int get_iSPic2Mhd_I(int i) const {
    if (doSplitSpecies) {
      return iSPic2Mhd_I[i];
    } else {
      return i;
    }
  }
  std::string get_splitType() const { return splitType; }

  void set_State_BGV(int nBlockIn, int nx, int ny, int nz, double *state_I,
                     int *iPoint_I) {
    nBlock = nBlockIn;
    nxnLG = nx;
    nynLG = ny;
    nznLG = nz;
    setStateVar(state_I, iPoint_I);
  }

  /** Get the Electic field as from the fluid description */
  void setFluidFieldsNode(double *Ex, double *Ey, double *Ez, double *Bx,
                          double *By, double *Bz, const int i, const int j,
                          const int k);

  void CalcFluidState(const double *dataPIC_I, double *dataFluid_I) const;

  //-----------long Inline functions or template functions.-------

  /** Convert from local index to global index. Only correct for State_BGV
   because of the ignored
   dimension. */
  inline void getGlobalIndex(const int il, const int jl, const int kl, int *ig,
                             int *jg, int *kg) const {
    *ig = StartIdx_D[0] + il - 1;
    if (nNodeGst_D[1] == 1)
      *jg = 0;
    else
      *jg = StartIdx_D[1] + jl - 1;
    if (nNodeGst_D[2] == 1)
      *kg = 0;
    else
      *kg = StartIdx_D[2] + kl - 1;
  }

  inline void getInterpolatedValue(const int iBlock, const int i, int j, int k,
                                   double *Var, const int iVar) const {
    if (nNodeGst_D[1] == 1)
      j = 0;
    if (nNodeGst_D[2] == 1)
      k = 0;
    *Var = State_BGV(iBlock, i, j, k, iVar);
  }

  /** Second order interpolation  for a given position */
  inline void getInterpolatedValue(const int iBlock, const double x,
                                   const double y, const double z, double *Var,
                                   int iVar) const {
    int i1 = 0, j1 = 0, k1 = 0, i2 = 0, j2 = 0, k2 = 0;
    double dx1 = 0.5, dx2 = 0.5, dy1 = 0.5, dy2 = 0.5, dz1 = 0.5, dz2 = 0.5;

    // Get the index of the for the nodes surounding the cell

    dx1 = (x - BlockMin_BD(iBlock, x_)) / CellSize_BD(iBlock, x_) + nG_D[x_];
    i1 = floor(dx1);
    i2 = i1 + 1;
    dx1 -= i1;
    dx2 = 1 - dx1;
    if (nNodeGst_D[1] > 1) {
      dy1 = (y - BlockMin_BD(iBlock, y_)) / CellSize_BD(iBlock, y_) + nG_D[y_];
      j1 = floor(dy1);
      j2 = j1 + 1;
      dy1 -= j1;
      dy2 = 1 - dy1;
    }

    if (nNodeGst_D[2] > 1) {
      dz1 = (z - BlockMin_BD(iBlock, z_)) / CellSize_BD(iBlock, z_) + nG_D[z_];
      k1 = floor(dz1);
      k2 = k1 + 1;
      dz1 -= k1;
      dz2 = 1 - dz1;
    }

    *Var = dz2 * (dy2 * (dx2 * State_BGV(iBlock, i1, j1, k1, iVar) +
                         dx1 * State_BGV(iBlock, i2, j1, k1, iVar)) +
                  dy1 * (dx2 * State_BGV(iBlock, i1, j2, k1, iVar) +
                         dx1 * State_BGV(iBlock, i2, j2, k1, iVar))) +
           dz1 * (dy2 * (dx2 * State_BGV(iBlock, i1, j1, k2, iVar) +
                         dx1 * State_BGV(iBlock, i2, j1, k2, iVar)) +
                  dy1 * (dx2 * State_BGV(iBlock, i1, j2, k2, iVar) +
                         dx1 * State_BGV(iBlock, i2, j2, k2, iVar)));
  }

  /** Find out if we will sync with fluid solution at this time/cycle */
  inline unsigned long doSyncWithFluid(unsigned long cycle) {
    if (cycle == iSyncStep * nSync) {
      iSyncStep++;
      return ((iSyncStep - 1));
    } else
      return (0);
  }

  /** nNodeGst_D includes 1 guard/ghost cell layer... */
  inline double getFluidNxc() const { return (nNodeGst_D[0] - 3 * NG); }

  /** nNodeGst_D includes 1 guard/ghost cell layer... */
  inline double getFluidNyc() const {
    if (nNodeGst_D[1] > 3 * NG)
      return (nNodeGst_D[1] - 3 * NG);
    else
      return (1);
  }

  /** nNodeGst_D includes 1 guard/ghost cell layer... */
  inline double getFluidNzc() const {
    if (nNodeGst_D[2] > 3 * NG)
      return (nNodeGst_D[2] - 3 * NG);
    else
      return (1);
  }

  /** nNodeGst_D includes 1 guard/ghost cell layer... */
  inline double getFluidNxn() { return (nNodeGst_D[0] - 2 * NG); }

  /** nNodeGst_D includes 1 guard/ghost cell layer... */
  inline double getFluidNyn() {
    if (nNodeGst_D[1] > 3 * NG)
      return (nNodeGst_D[1] - 2 * NG);
    else
      return (1);
  }

  /** nNodeGst_D includes 1 guard/ghost cell layer... */
  inline double getFluidNzn() {
    if (nNodeGst_D[2] > 3 * NG)
      return (nNodeGst_D[2] - 2 * NG);
    else
      return (1);
  }

  /** Get physical dimetntions to the simulation domain, without ghost cells */
  inline double getFluidLx() { return (lenGst_D[0] - NG * 2 * dx_D[0]); }

  /** Get physical dimetntions to the simulation domain, without ghost cells */
  inline double getFluidLy() {
    if (nNodeGst_D[1] > 3 * NG)
      return (lenGst_D[1] - NG * 2 * dx_D[1]);
    else
      return (lenGst_D[1]);
  }

  /** Get physical dimetntions to the simulation domain, without ghost cells */
  inline double getFluidLz() {
    if (nNodeGst_D[2] > 3 * NG)
      return (lenGst_D[2] - NG * 2 * dx_D[2]);
    else
      return (lenGst_D[2]);
  }

  /** place holder for all variables like charge density that we know is 0.0 */
  template <typename Type>
  inline double getFluidZero(const Type x, const Type y, const Type z,
                             const int is) const {
    return (0.0);
  }

  /** get Pxx from fluid */
  template <typename Type>
  inline double getPICPxx(const Type x, const Type y, const Type z,
                          const int is) const {
    int iBlock = 0;
    return getPICPxx(iBlock, x, y, z, is);
  }
  template <typename Type>
  inline double getPICPxx(const int iBlock, const Type x, const Type y,
                          const Type z, const int is) const {
    double Pxx;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Pxx =
        getFluidPxx(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Pxx;
  }
  template <typename Type>
  inline double getFluidPxx(const int iBlock, const Type x, const Type y,
                            const Type z, const int is) const {
    double Pxx;
    if (useAnisoP) {
      double Bx, By, Bz, Bt2, Ppar, P, Pperp;
      getInterpolatedValue(iBlock, x, y, z, &Bx, iBx);
      getInterpolatedValue(iBlock, x, y, z, &By, iBy);
      getInterpolatedValue(iBlock, x, y, z, &Bz, iBz);
      Bt2 = Bx * Bx + By * By + Bz * Bz;

      Ppar = getFluidPpar(iBlock, x, y, z, is);
      P = getFluidP(iBlock, x, y, z, is);
      Pperp = 0.5 * (3.0 * P - Ppar);

      Pxx = Pperp + (Ppar - Pperp) * Bx * Bx / Bt2;

    } else {
      Pxx = getFluidP(iBlock, x, y, z, is);
    }
    return (QoQi0_S[is] *
            (Pxx / MoMi0_S[is] + getFluidRhoNum(iBlock, x, y, z, is) *
                                     pow(getFluidUx(iBlock, x, y, z, is), 2)));
  }

  /** get Pyy from fluid */
  template <typename Type>
  inline double getPICPyy(const Type x, const Type y, const Type z,
                          const int is) const {
    int iBlock = 0;
    return getPICPyy(iBlock, x, y, z, is);
  }

  template <typename Type>
  inline double getPICPyy(const int iBlock, const Type x, const Type y,
                          const Type z, const int is) const {
    double Pyy;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Pyy =
        getFluidPyy(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Pyy;
  }

  template <typename Type>
  inline double getFluidPyy(const int iBlock, const Type x, const Type y,
                            const Type z, const int is) const {
    double Pyy;
    if (useAnisoP) {
      double Bx, By, Bz, Bt2, Ppar, P, Pperp;
      getInterpolatedValue(iBlock, x, y, z, &Bx, iBx);
      getInterpolatedValue(iBlock, x, y, z, &By, iBy);
      getInterpolatedValue(iBlock, x, y, z, &Bz, iBz);
      Bt2 = Bx * Bx + By * By + Bz * Bz;

      Ppar = getFluidPpar(iBlock, x, y, z, is);
      P = getFluidP(iBlock, x, y, z, is);
      Pperp = 0.5 * (3.0 * P - Ppar);

      Pyy = Pperp + (Ppar - Pperp) * By * By / Bt2;
    } else {
      Pyy = getFluidP(iBlock, x, y, z, is);
    }
    return (QoQi0_S[is] *
            (Pyy / MoMi0_S[is] + getFluidRhoNum(iBlock, x, y, z, is) *
                                     pow(getFluidUy(iBlock, x, y, z, is), 2)));
  }

  /** get Pzz from fluid */
  template <typename Type>
  inline double getPICPzz(const Type x, const Type y, const Type z,
                          const int is) const {
    int iBlock = 0;
    return getPICPzz(iBlock, x, y, z, is);
  }

  template <typename Type>
  inline double getPICPzz(const int iBlock, const Type x, const Type y,
                          const Type z, const int is) const {
    double Pzz;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Pzz =
        getFluidPzz(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Pzz;
  }

  template <typename Type>
  inline double getFluidPzz(const int iBlock, const Type x, const Type y,
                            const Type z, const int is) const {
    double Pzz;
    if (useAnisoP) {
      double Bx, By, Bz, Bt2, Ppar, P, Pperp;
      getInterpolatedValue(iBlock, x, y, z, &Bx, iBx);
      getInterpolatedValue(iBlock, x, y, z, &By, iBy);
      getInterpolatedValue(iBlock, x, y, z, &Bz, iBz);
      Bt2 = Bx * Bx + By * By + Bz * Bz;

      Ppar = getFluidPpar(iBlock, x, y, z, is);
      P = getFluidP(iBlock, x, y, z, is);
      Pperp = 0.5 * (3.0 * P - Ppar);

      Pzz = Pperp + (Ppar - Pperp) * Bz * Bz / Bt2;

    } else {
      Pzz = getFluidP(iBlock, x, y, z, is);
    }

    return (QoQi0_S[is] *
            (Pzz / MoMi0_S[is] + getFluidRhoNum(iBlock, x, y, z, is) *
                                     pow(getFluidUz(iBlock, x, y, z, is), 2)));
  }

  /** get Pxy from fluid */
  template <typename Type>
  inline double getPICPxy(const Type x, const Type y, const Type z,
                          const int is) const {
    int iBlock = 0;
    return getPICPxy(iBlock, x, y, z, is);
  }

  template <typename Type>
  inline double getPICPxy(const int iBlock, const Type x, const Type y,
                          const Type z, const int is) const {
    double Pxy;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Pxy =
        getFluidPxy(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Pxy;
  }

  template <typename Type>
  inline double getFluidPxy(const int iBlock, const Type x, const Type y,
                            const Type z, const int is) const {
    double Pxy;
    if (useAnisoP) {
      double Bx, By, Bz, Bt2, Ppar, P, Pperp;
      getInterpolatedValue(iBlock, x, y, z, &Bx, iBx);
      getInterpolatedValue(iBlock, x, y, z, &By, iBy);
      getInterpolatedValue(iBlock, x, y, z, &Bz, iBz);
      Bt2 = Bx * Bx + By * By + Bz * Bz;

      Ppar = getFluidPpar(iBlock, x, y, z, is);
      P = getFluidP(iBlock, x, y, z, is);
      Pperp = 0.5 * (3.0 * P - Ppar);

      Pxy = (Ppar - Pperp) * Bx * By / Bt2;

    } else {
      Pxy = 0;
    }

    return (QoQi0_S[is] *
            (Pxy / MoMi0_S[is] + getFluidRhoNum(iBlock, x, y, z, is) *
                                     getFluidUx(iBlock, x, y, z, is) *
                                     getFluidUy(iBlock, x, y, z, is)));
  }

  /** get Pxz from fluid */
  template <typename Type>
  inline double getPICPxz(const Type x, const Type y, const Type z,
                          const int is) const {
    int iBlock = 0;
    return getPICPxz(iBlock, x, y, z, is);
  }
  template <typename Type>
  inline double getPICPxz(const int iBlock, const Type x, const Type y,
                          const Type z, const int is) const {
    double Pxz;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Pxz =
        getFluidPxz(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Pxz;
  }
  template <typename Type>
  inline double getFluidPxz(const int iBlock, const Type x, const Type y,
                            const Type z, const int is) const {
    double Pxz;
    if (useAnisoP) {
      double Bx, By, Bz, Bt2, Ppar, P, Pperp;
      getInterpolatedValue(iBlock, x, y, z, &Bx, iBx);
      getInterpolatedValue(iBlock, x, y, z, &By, iBy);
      getInterpolatedValue(iBlock, x, y, z, &Bz, iBz);
      Bt2 = Bx * Bx + By * By + Bz * Bz;

      Ppar = getFluidPpar(iBlock, x, y, z, is);
      P = getFluidP(iBlock, x, y, z, is);
      Pperp = 0.5 * (3.0 * P - Ppar);

      Pxz = (Ppar - Pperp) * Bx * Bz / Bt2;

    } else {
      Pxz = 0;
    }

    return (QoQi0_S[is] *
            (Pxz / MoMi0_S[is] + getFluidRhoNum(iBlock, x, y, z, is) *
                                     getFluidUx(iBlock, x, y, z, is) *
                                     getFluidUz(iBlock, x, y, z, is)));
  }

  /** get Pyz from fluid */
  template <typename Type>
  inline double getPICPyz(const Type x, const Type y, const Type z,
                          const int is) const {
    int iBlock = 0;
    return getPICPyz(iBlock, x, y, z, is);
  }

  template <typename Type>
  inline double getPICPyz(const int iBlock, const Type x, const Type y,
                          const Type z, const int is) const {
    double Pyz;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Pyz =
        getFluidPyz(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Pyz;
  }
  template <typename Type>
  inline double getFluidPyz(const int iBlock, const Type x, const Type y,
                            const Type z, const int is) const {
    double Pyz;
    if (useAnisoP) {
      double Bx, By, Bz, Bt2, Ppar, P, Pperp;
      getInterpolatedValue(iBlock, x, y, z, &Bx, iBx);
      getInterpolatedValue(iBlock, x, y, z, &By, iBy);
      getInterpolatedValue(iBlock, x, y, z, &Bz, iBz);
      Bt2 = Bx * Bx + By * By + Bz * Bz;

      Ppar = getFluidPpar(iBlock, x, y, z, is);
      P = getFluidP(iBlock, x, y, z, is);
      Pperp = 0.5 * (3.0 * P - Ppar);

      Pyz = (Ppar - Pperp) * By * Bz / Bt2;

    } else {
      Pyz = 0;
    }

    return (QoQi0_S[is] *
            (Pyz / MoMi0_S[is] + getFluidRhoNum(iBlock, x, y, z, is) *
                                     getFluidUy(iBlock, x, y, z, is) *
                                     getFluidUz(iBlock, x, y, z, is)));
  }

  /** get Jx from fluid */
  template <typename Type>
  inline double getPICJx(const Type x, const Type y, const Type z,
                         const int is) const {
    int iBlock = 0;
    return getPICJx(iBlock, x, y, z, is);
  }

  template <typename Type>
  inline double getPICJx(const int iBlock, const Type x, const Type y,
                         const Type z, const int is) const {
    double Jx;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Jx = getFluidJx(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Jx;
  }

  template <typename Type>
  inline double getFluidJx(const int iBlock, const Type x, const Type y,
                           const Type z, const int is) const {
    return (QoQi0_S[is] * getFluidU(iBlock, x, y, z, is, iUx_I, iJx) *
            getFluidRhoNum(iBlock, x, y, z, is));
  }

  /** get Jy from fluid */
  template <typename Type>
  inline double getPICJy(const Type x, const Type y, const Type z,
                         const int is) const {
    int iBlock = 0;
    return getPICJy(iBlock, x, y, z, is);
  }

  template <typename Type>
  inline double getPICJy(const int iBlock, const Type x, const Type y,
                         const Type z, const int is) const {
    double Jy;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Jy = getFluidJy(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Jy;
  }
  template <typename Type>
  inline double getFluidJy(const int iBlock, const Type x, const Type y,
                           const Type z, const int is) const {
    return (QoQi0_S[is] * getFluidU(iBlock, x, y, z, is, iUy_I, iJy) *
            getFluidRhoNum(iBlock, x, y, z, is));
  }

  /** get Jz from fluid */
  template <typename Type>
  inline double getPICJz(const Type x, const Type y, const Type z,
                         const int is) const {
    int iBlock = 0;
    return getPICJz(iBlock, x, y, z, is);
  }
  template <typename Type>
  inline double getPICJz(const int iBlock, const Type x, const Type y,
                         const Type z, const int is) const {
    double Jz;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Jz = getFluidJz(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Jz;
  }
  template <typename Type>
  inline double getFluidJz(const int iBlock, const Type x, const Type y,
                           const Type z, const int is) const {
    return (QoQi0_S[is] * getFluidU(iBlock, x, y, z, is, iUz_I, iJz) *
            getFluidRhoNum(iBlock, x, y, z, is));
  }

  /** get bulk velocity Ux from fluid */
  template <typename Type>
  inline double getPICUx(const Type x, const Type y, const Type z,
                         const int is) const {
    int iBlock = 0;
    return getPICUx(iBlock, x, y, z, is);
  }
  template <typename Type>
  inline double getPICUx(const int iBlock, const Type x, const Type y,
                         const Type z, const int is) const {
    double Ux;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Ux = getFluidUx(iBlock, x, y, z, iMHD);
    return Ux;
  }
  template <typename Type>
  inline double getFluidUx(const int iBlock, const Type x, const Type y,
                           const Type z, const int is) const {
    return (getFluidU(iBlock, x, y, z, is, iUx_I, iJx));
  }

  /** get bulk velocity Uy from fluid */
  template <typename Type>
  inline double getPICUy(const Type x, const Type y, const Type z,
                         const int is) const {
    int iBlock = 0;
    return getPICUy(iBlock, x, y, z, is);
  }

  template <typename Type>
  inline double getPICUy(const int iBlock, const Type x, const Type y,
                         const Type z, const int is) const {
    double Uy;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Uy = getFluidUy(iBlock, x, y, z, iMHD);
    return Uy;
  }
  template <typename Type>
  inline double getFluidUy(const int iBlock, const Type x, const Type y,
                           const Type z, const int is) const {
    return (getFluidU(iBlock, x, y, z, is, iUy_I, iJy));
  }

  /** get bulk velocity Uz from fluid */
  template <typename Type>
  inline double getPICUz(const Type x, const Type y, const Type z,
                         const int is) const {
    int iBlock = 0;
    return getPICUz(iBlock, x, y, z, is);
  }

  template <typename Type>
  inline double getPICUz(const int iBlock, const Type x, const Type y,
                         const Type z, const int is) const {
    double Uz;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Uz = getFluidUz(iBlock, x, y, z, iMHD);
    return Uz;
  }
  template <typename Type>
  inline double getFluidUz(const int iBlock, const Type x, const Type y,
                           const Type z, const int is) const {
    return (getFluidU(iBlock, x, y, z, is, iUz_I, iJz));
  }

  /** get bulk velocity U in x, y or z from fluid */
  template <typename Type>
  inline double getFluidU(const int iBlock, const Type x, const Type y,
                          const Type z, const int is, const int *iU_I,
                          const int iJ) const {
    // if(doSplitSpecies && !do_deposit_particle(is, x, y, z)) return 0;
    // Assume qe = -qi;
    double U, J, Rhoit, Qit, Rhot;

    if (useElectronFluid) {
      getInterpolatedValue(iBlock, x, y, z, &U, iU_I[is]);
    } else if (useMultiFluid) {
      if (is == 0) {
        // Electron
        /** Ue = (J - sum(ni*qi*Ui))/(ne*qe)
               = J/(ne*qe) + sum(ni*Ui)/ne */
        double Ui, ni, ne;
        getInterpolatedValue(iBlock, x, y, z, &J, iJ);
        ne = getFluidRhoNum(iBlock, x, y, z, 0);
        U = J / (QoQi0_S[0] * ne);

        for (int iIon = 0; iIon < nIon; ++iIon) {
          Ui = getFluidU(iBlock, x, y, z, iIon + 1, iU_I, iJ);
          ni = getFluidRhoNum(iBlock, x, y, z, iIon + 1);
          U += ni * Ui / ne;
        }
      } else {
        // Ion
        getInterpolatedValue(iBlock, x, y, z, &U, iU_I[is - 1]);
      }
    } else {
      // Single fluid or multi-species.

      // Ui = U_{MHD} - me/qe*J/Rho;
      // where Rho is total density include electrons.
      // Ue = U_{MHD} - Rhoit/Qit*J/Rho;
      // where Rhoit = sum(ni*Mi), Qit = sum(ni*Qi).

      double moq, Numi;
      if (is == 0) {
        Rhoit = 0;
        Qit = 0;
        for (int iIon = 0; iIon < nIon; ++iIon) {
          Numi = getFluidRhoNum(iBlock, x, y, z, iIon + 1);
          Rhoit += Numi * MoMi0_S[iIon + 1];
          Qit += Numi * QoQi0_S[iIon + 1];
        }
        moq = 0;
        if (Qit != 0)
          moq = Rhoit / Qit;
      } else
        moq = MoMi0_S[0] / QoQi0_S[0];

      Rhot = 0;
      for (int is0 = 0; is0 < nSIn; ++is0) {
        Rhot += MoMi0_S[is0] * getFluidRhoNum(iBlock, x, y, z, is0);
      }

      getInterpolatedValue(iBlock, x, y, z, &U, iU_I[0]);
      getInterpolatedValue(iBlock, x, y, z, &J, iJ);

      if (Rhot != 0)
        U -= moq * J / Rhot;
    }
    return (U);
  }

  /** get thermal velocity at location x, y, z for fluid is */
  template <typename Type>
  inline double getPICUth(const Type x, const Type y, const Type z,
                          const int is) const {
    int iBlock = 0;
    return getPICUth(iBlock, x, y, z, is);
  }
  template <typename Type>
  inline double getPICUth(const int iBlock, const Type x, const Type y,
                          const Type z, const int is) const {
    double Uth;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Uth = getFluidUth(iBlock, x, y, z, iMHD);
    return Uth;
  }
  template <typename Type>
  inline double getFluidUth(const int iBlock, const Type x, const Type y,
                            const Type z, const int is) const {
    // if(doSplitSpecies && !do_deposit_particle(is, x, y, z)) return 0;
    double Uth = 0, p, ni;

    p = getFluidP(iBlock, x, y, z, is);
    ni = getFluidRhoNum(iBlock, x, y, z, is);
    if (ni > 0)
      Uth = sqrt(p / (ni * MoMi0_S[is]));
    return (Uth);
  }

  template <typename Type>
  inline void setPICIsoUth(const int iBlock, const Type x, const Type y,
                           const Type z, double *u, double *v, double *w,
                           const double rand1, const double rand2,
                           const double rand3, const double rand4,
                           const int is) const {

    setFluidIsoUth(iBlock, x, y, z, u, v, w, rand1, rand2, rand3, rand4, is);
  }

  template <typename Type>
  inline void setPICIsoUth(const Type x, const Type y, const Type z, double *u,
                           double *v, double *w, const double rand1,
                           const double rand2, const double rand3,
                           const double rand4, const int is) const {
    const int iBlock = 0;
    setFluidIsoUth(iBlock, x, y, z, u, v, w, rand1, rand2, rand3, rand4, is);
  }

  template <typename Type>
  inline void setFluidIsoUth(const int iBlock, const Type x, const Type y,
                             const Type z, double *u, double *v, double *w,
                             const double rand1, const double rand2,
                             const double rand3, const double rand4,
                             const int is) const {
    double harvest, prob, theta, Uth;

    // u = X velocity
    harvest = rand1;
    prob = sqrt(-2.0 * log(1.0 - .999999999 * harvest));
    harvest = rand2;
    theta = 2.0 * M_PI * harvest;
    Uth = getPICUth(iBlock, x, y, z, is);

    (*u) = Uth * prob * cos(theta);
    // v = Y velocity
    (*v) = Uth * prob * sin(theta);
    // w = Z velocity
    harvest = rand3;
    prob = sqrt(-2.0 * log(1.0 - .999999999 * harvest));
    harvest = rand4;
    theta = 2.0 * M_PI * harvest;
    (*w) = Uth * prob * cos(theta);
  }

  /** set thermal velocity in magnetic cordinates for a given position */
  template <typename Type>
  inline void setPICAnisoUth(const int iBlock, const Type x, const Type y,
                             const Type z, double *u, double *v, double *w,
                             const double rand1, const double rand2,
                             const double rand3, const double rand4,
                             const int is) const {
    setFluidAnisoUth(iBlock, x, y, z, u, v, w, rand1, rand2, rand3, rand4, is);
  }

  template <typename Type>
  inline void setPICAnisoUth(const Type x, const Type y, const Type z,
                             double *u, double *v, double *w,
                             const double rand1, const double rand2,
                             const double rand3, const double rand4,
                             const int is) const {
    const int iBlock = 0;
    setFluidAnisoUth(iBlock, x, y, z, u, v, w, rand1, rand2, rand3, rand4, is);
  }

  template <typename Type>
  inline void setFluidAnisoUth(const int iBlock, const Type x, const Type y,
                               const Type z, double *u, double *v, double *w,
                               const double rand1, const double rand2,
                               const double rand3, const double rand4,
                               const int is) const {
    double Bx, By, Bz, P, Ppar, Pperp, Uthperp, Uthpar, Uthperp1, Uthperp2;
    double prob, theta;
    MDArray<double> norm_DD;
    // indexes for the norm_DD matix
    int Norm_, Perp1_, Perp2_, X_, Y_, Z_;

    if (useMultiFluid || useMultiSpecies || doSplitSpecies) {
      std::cout << " setFluidanisoUth has not implemented for "
                   "multifluid/multispecies/doSplitSpecies!!!" << std::endl;
      abort();
    }

    Norm_ = 0;
    Perp1_ = 1;
    Perp2_ = 2;
    X_ = 0;
    Y_ = 1;
    Z_ = 2;

    // Get number density and B at the particle position
    double ni = getFluidRhoNum(iBlock, x, y, z, is);
    getInterpolatedValue(iBlock, x, y, z, &Bx, iBx);
    getInterpolatedValue(iBlock, x, y, z, &By, iBy);
    getInterpolatedValue(iBlock, x, y, z, &Bz, iBz);

    // Get Parallel and perpendicular presure
    Ppar = getFluidPpar(iBlock, x, y, z, is);
    P = getFluidP(iBlock, x, y, z, is);
    Pperp = 0.5 * (3.0 * P - Ppar);

    // Get 3 vertors spaning the vector space
    norm_DD.init(3, 3);
    MagneticBaseVectors(Bx, By, Bz, norm_DD);

    // Get the thermal verlocities
    prob = sqrt(-2.0 * log(1.0 - .999999999 * rand1));
    theta = 2.0 * M_PI * rand2;
    Uthpar = sqrt(Ppar / (MoMi0_S[is] * ni)) * prob * cos(theta);

    prob = sqrt(-2.0 * log(1.0 - .999999999 * rand3));
    theta = 2.0 * M_PI * rand4;
    Uthperp = sqrt(Pperp / (MoMi0_S[is] * ni)) * prob;
    Uthperp1 = Uthperp * cos(theta);
    Uthperp2 = Uthperp * sin(theta);

    // Set particle thermal velocity
    (*u) = Uthpar * norm_DD(Norm_, X_) + Uthperp1 * norm_DD(Perp1_, X_) +
           Uthperp2 * norm_DD(Perp2_, X_);
    (*v) = Uthpar * norm_DD(Norm_, Y_) + Uthperp1 * norm_DD(Perp1_, Y_) +
           Uthperp2 * norm_DD(Perp2_, Y_);
    (*w) = Uthpar * norm_DD(Norm_, Z_) + Uthperp1 * norm_DD(Perp1_, Z_) +
           Uthperp2 * norm_DD(Perp2_, Z_);
  }

  /** get total pressure for species from fluid */
  template <typename Type>
  inline double getPICP(const Type x, const Type y, const Type z,
                        const int is) const {
    int iBlock = 0;
    return getPICP(iBlock, x, y, z, is);
  }
  template <typename Type>
  inline double getPICP(const int iBlock, const Type x, const Type y,
                        const Type z, const int is) const {
    double P;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    P = getFluidP(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return P;
  }
  template <typename Type>
  inline double getFluidP(const int iBlock, const Type x, const Type y,
                          const Type z, const int is) const {
    // if(doSplitSpecies && !do_deposit_particle(is, x, y, z)) return 0;
    double P;

    if (useElectronFluid) {
      getInterpolatedValue(iBlock, x, y, z, &P, iP_I[is]);
    } else if (useMultiFluid) {
      // Multi-fluid.
      if (is == 0)
        getInterpolatedValue(iBlock, x, y, z, &P, iPe); // Electron
      else
        getInterpolatedValue(iBlock, x, y, z, &P, iP_I[is - 1]); // Ion
    } else {
      // Single-fluid and multi-species.
      if (!useMhdPe) {
        getInterpolatedValue(iBlock, x, y, z, &P, iP_I[0]);
        if (is == 0)
          P *= PeRatio;
        else if (is > 0)
          P *= (1 - PeRatio);
      } else {
        if (is == 0)
          getInterpolatedValue(iBlock, x, y, z, &P, iPe); // Electron
        else if (is > 0)
          getInterpolatedValue(iBlock, x, y, z, &P, iP_I[0]); // Ion
      }

      // Split pressure among ions.
      if (useMultiSpecies && is > 0) {
        double Numit;
        Numit = 0; // Number of all ions.
        for (int iIon = 0; iIon < nIon; ++iIon)
          Numit += getFluidRhoNum(iBlock, x, y, z, iIon + 1);

        P *= getFluidRhoNum(iBlock, x, y, z, is) / Numit;
      }
    }
    return (P);
  }

  /** get parallel thermal presure for species from a fluid*/
  template <typename Type>
  inline double getPICPpar(const Type x, const Type y, const Type z,
                           const int is) const {
    int iBlock = 0;
    return getPICPpar(iBlock, x, y, z, is);
  }
  template <typename Type>
  inline double getPICPpar(const int iBlock, const Type x, const Type y,
                           const Type z, const int is) const {
    double Ppar;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    Ppar =
        getFluidPpar(iBlock, x, y, z, iMHD) * RatioPIC2MHD(iBlock, x, y, z, is);
    return Ppar;
  }
  template <typename Type>
  inline double getFluidPpar(const int iBlock, const Type x, const Type y,
                             const Type z, const int is) const {
    // if(doSplitSpecies && !do_deposit_particle(is, x, y, z)) return 0;
    // Need to check whether this function works correctly! -- Yuxi
    double P;
    if (useMultiSpecies || useMultiFluid || doSplitSpecies) {
      std::cout << " getFluidPpar has not implemented for "
                   "multifluid/multispecies/doSplitSpecies!!" << std::endl;
      abort();
    }

    if (useElectronFluid) {
      getInterpolatedValue(iBlock, x, y, z, &P, iPpar_I[is]);
    } else if (useMhdPe) {
      if (is == 0)
        getInterpolatedValue(iBlock, x, y, z, &P, iPe); // Electron
      if (is == 1)
        getInterpolatedValue(iBlock, x, y, z, &P, iPpar_I[0]); // Ion
    } else {
      getInterpolatedValue(iBlock, x, y, z, &P, iPpar_I[0]);
      if (is == 0)
        P *= PeRatio;
      else if (is == 1)
        P *= (1 - PeRatio);
    }

    return (P);
  }

  /** this should return NUMBER DENSITY from fluid */
  template <typename Type>
  inline double getPICRhoNum(const Type x, const Type y, const Type z,
                             const int is) const {
    int iBlock = 0;
    return getPICRhoNum(iBlock, x, y, z, is);
  }
  template <typename Type>
  inline double getPICRhoNum(const int iBlock, const Type x, const Type y,
                             const Type z, const int is) const {
    double RhoNum;
    int iMHD;
    iMHD = is;
    if (doSplitSpecies)
      iMHD = iSPic2Mhd_I[is];
    RhoNum = getFluidRhoNum(iBlock, x, y, z, iMHD) *
             RatioPIC2MHD(iBlock, x, y, z, is);

    return RhoNum;
  }

  template <typename Type>
  inline double getFluidRhoNum(const int iBlock, const Type x, const Type y,
                               const Type z, const int is) const {
    double Rho, NumDens;

    // if(doSplitSpecies && !do_deposit_particle(is, x, y, z)) return 0;

    if (useElectronFluid) {
      getInterpolatedValue(iBlock, x, y, z, &Rho, iRho_I[is]);
      NumDens = Rho / MoMi0_S[is];
    } else if (useMultiFluid || useMultiSpecies) {
      if (is == 0) {
        // Electron
        NumDens = 0;
        for (int iIon = 0; iIon < nIon; ++iIon) {
          getInterpolatedValue(iBlock, x, y, z, &Rho, iRho_I[iIon]);
          NumDens += Rho / MoMi0_S[iIon + 1];
        }
      } else {
        // Ion
        getInterpolatedValue(iBlock, x, y, z, &Rho, iRho_I[is - 1]);
        NumDens = Rho / MoMi0_S[is];
      }
    } else {
      // Electrons and iones have same density, ignoring is
      getInterpolatedValue(iBlock, x, y, z, &Rho, iRho_I[0]);
      NumDens = Rho / SumMass;
    }
    return (NumDens);
  }

  template <typename Type>
  inline double getBx(const int iBlock, const Type x, const Type y,
                      const Type z) const {
    double Bx;
    getInterpolatedValue(iBlock, x, y, z, &Bx, iBx);
    return Bx;
  }

  template <typename Type>
  inline double getBy(const int iBlock, const Type x, const Type y,
                      const Type z) const {
    double By;
    getInterpolatedValue(iBlock, x, y, z, &By, iBy);
    return By;
  }

  template <typename Type>
  inline double getBz(const int iBlock, const Type x, const Type y,
                      const Type z) const {
    double Bz;
    getInterpolatedValue(iBlock, x, y, z, &Bz, iBz);
    return Bz;
  }

  template <typename Type>
  inline double getEx(const int iBlock, const Type x, const Type y,
                      const Type z) const {
    double Ex;
    if (useElectronFluid) {
      getInterpolatedValue(iBlock, x, y, z, &Ex, iEx);
    } else {
      Ex = (getFluidUz(iBlock, x, y, z, 0) * getBy(iBlock, x, y, z) -
            getFluidUy(iBlock, x, y, z, 0) * getBz(iBlock, x, y, z));
    }
    return Ex;
  }

  template <typename Type>
  inline double getEy(const int iBlock, const Type x, const Type y,
                      const Type z) const {
    double Ey;
    if (useElectronFluid) {
      getInterpolatedValue(iBlock, x, y, z, &Ey, iEy);
    } else {
      Ey = (getFluidUx(iBlock, x, y, z, 0) * getBz(iBlock, x, y, z) -
            getFluidUz(iBlock, x, y, z, 0) * getBx(iBlock, x, y, z));
    }
    return Ey;
  }

  template <typename Type>
  inline double getEz(const int iBlock, const Type x, const Type y,
                      const Type z) const {
    double Ez;
    if (useElectronFluid) {
      getInterpolatedValue(iBlock, x, y, z, &Ez, iEz);
    } else {
      Ez = (getFluidUy(iBlock, x, y, z, 0) * getBx(iBlock, x, y, z) -
            getFluidUx(iBlock, x, y, z, 0) * getBy(iBlock, x, y, z));
    }
    return Ez;
  }

  template <typename Type>
  inline double RatioPIC2MHD(const int iBlock, const Type x, const Type y,
                             const Type z, const int is) const {
    double Ratio;
    if (doSplitSpecies) {
      if (splitType == "Bx" || splitType == "By" || splitType == "Bz") {
        int iVar;
        if (splitType == "Bx")
          iVar = iBx;
        if (splitType == "By")
          iVar = iBy;
        if (splitType == "Bz")
          iVar = iBz;
        double var;
        getInterpolatedValue(iBlock, x, y, z, &var, iVar);

        // var >0, deposit to the even species
        // var <=0, deposit to the odd species
        Ratio = 0;
        if ((var > 0 && is % 2 == 0) || (var <= 0 && is % 2 == 1))
          Ratio = 1;
      } else if (splitType == "ElectronOnly") {
        Ratio = 1;
        if (is == 0 || is == 1) {
          // Assume there are one electron and two ion species before splitting.
          double Ion1, Ion2, IonTot;
          Ion1 = getFluidRhoNum(iBlock, x, y, z, 1); // ion species 1
          Ion2 = getFluidRhoNum(iBlock, x, y, z, 2); // ion species 1
          IonTot = Ion1 + Ion2;
          Ratio = 0;
          if (IonTot > 0 && is == 0)
            Ratio = Ion1 / IonTot;
          if (IonTot > 0 && is == 1)
            Ratio = Ion2 / IonTot;
        }
      }
    } else {
      Ratio = 1;
    }
    return Ratio;
  }

}; // End of class FluidPicInterface declaration.
//----------------------------------------------------------------------

double weightedValue(double ****V, const int ix, const int iy, const int iz,
                     const int is, const double w000, const double w001,
                     const double w010, const double w011, const double w100,
                     const double w101, const double w110, const double w111);

double weightedValue(double ***V, const int ix, const int iy, const int iz,
                     const double w000, const double w001, const double w010,
                     const double w011, const double w100, const double w101,
                     const double w110, const double w111);
#endif
