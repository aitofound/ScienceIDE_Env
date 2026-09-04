#include "FluidPicInterface.h"

using std::cout;
using std::endl;
using std::string;
using std::stringstream;
using std::vector;
using std::array;

/** constructor */
FluidPicInterface::FluidPicInterface() {

  SItime = 0.0;

  for (int iDim = 0; iDim < 3; iDim++) {
    StartIdx_D[iDim] = -1;
    EndIdx_D[iDim] = -1;
  }
  dt = 0.0;

  // form normalizationof length C/Wp 100;  //[m] ->[cm]
  nSync = 1;
  iSyncStep = 1; // first sync time, 0 is at init

  // For now
  Lnorm = 1.0;
  Mnorm = 1.0;
  Unorm = 1.0;

  nBlock = 1;

  isFirstTime = true;
  doNeedBCOnly = false;

  nPlotFile = 0;

  xStart_I = xEnd_I = yStart_I = yEnd_I = zStart_I = zEnd_I = nullptr; 
  
  doCoupleAMPS = false;

  doSplitSpecies = false;
}
//-------------------------------------------------------------------------

/** destructor */
FluidPicInterface::~FluidPicInterface() {
  delete[] MoMi_S;
  delete[] QoQi_S;

  delete[] MoMi0_S;
  delete[] QoQi0_S;

  delete[] iRho_I;
  delete[] iRhoUx_I;
  delete[] iRhoUy_I;
  delete[] iRhoUz_I;
  delete[] iUx_I;
  delete[] iUy_I;
  delete[] iUz_I;
  delete[] iPpar_I;
  delete[] iP_I;

  if(xStart_I != nullptr){
    delete[] xStart_I;
    delete[] xEnd_I;
    delete[] yStart_I;
    delete[] yEnd_I;
    delete[] zStart_I;
    delete[] zEnd_I;
  }

  delete[] Si2No_V;
  delete[] No2Si_V;

  if (nPlotFile > 0) {
    delete[] dnOutput_I;
    delete[] dtOutput_I;
    delete[] plotDx_I;
    delete[] plotString_I;
    delete[] plotVar_I;
  }

  if (doSplitSpecies)
    delete[] iSPic2Mhd_I;
}
//-------------------------------------------------------------------------

void FluidPicInterface::InitData() {

  // normalization variables
  double RHOnorm, Bnorm, Jnorm, Pnorm;

  RHOnorm = Mnorm / (Lnorm * Lnorm * Lnorm);

  /* How to calculate Bnorm?
     1. Method 1
      In CGS unit, we have:
      1) [B] = [E]
      2) div(E) ~ rhoq, where rhoq is the charge density -> [E] = [rhoq]*[L]
      3) moment equation: d(rho*u)/dt ~ rhoq*u x B/c     ->
                         [RHO]*[U]/[T] = [rhoq]*[B]
        From the three equations above, we obtain: [B]=[E]=sqrt[RHO]*[U]

    2. Method 2
      In CGS, [P] = [RHO]*[U]*[U] = [B]*[B] -> [B]=sqrt[RHO]*[U]
  */
  Bnorm = sqrt(RHOnorm) * Unorm;

  // CGS Gauss's law: div(E) ~ rhoq -> [B] = [E] = [rhoq]*[L] = [Q]/[L]^2
  // -> [Q] = sqrt[RHO]*[U]*[L]^2 = sqrt[M*L]*[U].
  Qnorm = sqrt(Mnorm * Lnorm) * Unorm;
  Pnorm = RHOnorm * Unorm * Unorm;
  Jnorm = Qnorm * Unorm / (Lnorm * Lnorm * Lnorm);

  if (myrank == 0) {
    cout.precision(15);
    cout << "============= Normalization factors =============" << endl;
    cout << "Mnorm   = " << Mnorm << endl;
    cout << "Unorm   = " << Unorm << endl;
    cout << "Qnorm   = " << Qnorm << endl;
    cout << "Lnorm   = " << Lnorm << endl;
    cout << "RhoNorm = " << RHOnorm << endl;
    cout << "Bnorm   = " << Bnorm << endl;
    cout << "Pnorm   = " << Pnorm << endl;
    cout << "Jnorm   = " << Jnorm << endl;
    // cout<<"========================================="<<endl;
  }

  // SI -> CGS conversion
  Si2NoRho = 0.001; // [kg/m^3] -> [g/cm^3] and get numnber desity
  Si2NoV = 100.0;   // [m/s] -> [cm/s]
  Si2NoB = 1.0e4;   // [Tesla] -> [gauss]
  Si2NoP = 10.0;    // [Pa] -> [Ba]
  Si2NoL = 100;     // [m] ->[cm]

  /*
     1) SI: mu0*J_SI = curl(B_SI) with unit T/m
     2) CGS: 4pi/c_CGS*J_CGS = curl(B_CGS) with unit G/cm
     3) 1 T/m = 100 G/cm
     From 1) to 3) -> 100*mu0*J_SI = 4*pi/c_CGS*J_CGS ->
     J_CGS = 100*[ 4pi*10^(-7) ]/(4pi)*c_CGS = 10^(-5)*c_CGS,
     and c_CGS = Unorm.
  */
  Si2NoJ = 1.0e-5 * Unorm;

  /*
    1) E_SI = -U_SI x B_SI with unit m/s*T = 1e6 cm/s*G
    2) E_CGS*C_CGS = - U_CGS x B_CGS with unit cm/s*G
    -> E_CGS*C_CGS = 1e6*E_SI
    -> E_CGS = 1e6*E_SI/C_CGS = 1e6*E_SI/Unorm.
   */
  Si2NoE = 1e6 / Unorm;

  // Normalization: CGS -> non dimensional cgs
  Si2NoRho /= RHOnorm;
  Si2NoV /= Unorm;
  Si2NoB /= Bnorm;
  Si2NoP /= Pnorm;
  Si2NoJ /= Jnorm;
  Si2NoL /= Lnorm;
  Si2NoE /= Bnorm;

  Si2NoM = Si2NoRho * Si2NoV;

  No2SiV = 1. / Si2NoV;
  No2SiL = 1. / Si2NoL;

  Si2No_V[iBx] = Si2NoB;
  Si2No_V[iBy] = Si2NoB;
  Si2No_V[iBz] = Si2NoB;

  Si2No_V[iJx] = Si2NoJ;
  Si2No_V[iJy] = Si2NoJ;
  Si2No_V[iJz] = Si2NoJ;

  Si2No_V[iEx] = Si2NoE;
  Si2No_V[iEy] = Si2NoE;
  Si2No_V[iEz] = Si2NoE;

  if (useMhdPe)
    Si2No_V[iPe] = Si2NoP;
  if (useMultiSpecies)
    Si2No_V[iRhoTotal] = Si2NoRho;

  int iMax;
  iMax = nFluid;
  if (useMultiSpecies)
    iMax = nIon;

  for (int i = 0; i < iMax; ++i) {
    Si2No_V[iRho_I[i]] = Si2NoRho;
    Si2No_V[iRhoUx_I[i]] = Si2NoM;
    Si2No_V[iRhoUy_I[i]] = Si2NoM;
    Si2No_V[iRhoUz_I[i]] = Si2NoM;
    Si2No_V[iP_I[i]] = Si2NoP;
    if (useAnisoP)
      Si2No_V[iPpar_I[i]] = Si2NoP;
  }

  // Get back to SI units
  for (int iVar = 0; iVar < nVarCoupling; iVar++)
    No2Si_V[iVar] = 1.0 / Si2No_V[iVar];

  timeCtr.Si2NoT = getSi2NoT();
  timeCtr.No2SiT = getNo2SiT();
}
//-------------------------------------------------------------------------

void FluidPicInterface::ReNormLength() {
  // Normalization
  for (int i = 0; i < 3; i++) {
    dx_D[i] *= Si2NoL;
    lenGst_D[i] *= Si2NoL;
    phyMin_D[i] *= Si2NoL;
    phyMax_D[i] *= Si2NoL;
    if(i>=nDim){
      // If MHD is 2D.
      phyMin_D[i] = 0;
      phyMax_D[i] = dx_D[i];
    }
  }
}
//-------------------------------------------------------------------------

/** Convert to internal units in IPIC3D */
void FluidPicInterface::ReNormVariables() {
  for (int iBlock = 0; iBlock < nBlock; iBlock++)
    for (int i = 0; i < nxnLG; i++)
      for (int j = 0; j < nynLG; j++)
        for (int k = 0; k < nznLG; k++)
          if (doGetFromGM(i, j, k)) {
            for (int iVar = 0; iVar <= iJz; iVar++)
              State_BGV(iBlock, i, j, k, iVar) *= Si2No_V[iVar];
          }
}
//-------------------------------------------------------------------------

void FluidPicInterface::Moment2Velocity() {
  int i, j, k;
  double Rhot;

  for (int iBlock = 0; iBlock < nBlock; iBlock++)
    for (i = 0; i < nxnLG; i++)
      for (j = 0; j < nynLG; j++)
        for (k = 0; k < nznLG; k++) {
          if (doGetFromGM(i, j, k)) {
            if (useMultiSpecies) {
              Rhot = 0;
              for (int iIon = 0; iIon < nIon; ++iIon) {
                // Rho = sum(Rhoi) + Rhoe;
                Rhot += State_BGV(iBlock, i, j, k, iRho_I[iIon]) *
                        (1 + MoMi0_S[0] / MoMi0_S[iIon + 1]);
              } // iIon

              State_BGV(iBlock, i, j, k, iUx_I[0]) /= Rhot;
              State_BGV(iBlock, i, j, k, iUy_I[0]) /= Rhot;
              State_BGV(iBlock, i, j, k, iUz_I[0]) /= Rhot;
            } else {
              for (int iFluid = 0; iFluid < nFluid; ++iFluid) {
                State_BGV(iBlock, i, j, k, iUx_I[iFluid]) /=
                    State_BGV(iBlock, i, j, k, iRho_I[iFluid]);
                State_BGV(iBlock, i, j, k, iUy_I[iFluid]) /=
                    State_BGV(iBlock, i, j, k, iRho_I[iFluid]);
                State_BGV(iBlock, i, j, k, iUz_I[iFluid]) /=
                    State_BGV(iBlock, i, j, k, iRho_I[iFluid]);
              } // iFluid
            }   // else
          }
        }
}
//-------------------------------------------------------------------------

/** Get nomal and pendicular vector to magnetic field */
void FluidPicInterface::MagneticBaseVectors(const double Bx, const double By,
                                            const double Bz,
                                            MDArray<double> &norm_DD) const {
  double inv;
  int Norm_, Perp1_, Perp2_, X_, Y_, Z_;
  Norm_ = 0;
  Perp1_ = 1;
  Perp2_ = 2;
  X_ = 0;
  Y_ = 1;
  Z_ = 2;

  inv = 1.0 / sqrt(Bx * Bx + By * By + Bz * Bz);
  norm_DD(Norm_, X_) = Bx * inv;
  norm_DD(Norm_, Y_) = By * inv;
  norm_DD(Norm_, Z_) = Bz * inv;

  if (norm_DD(Norm_, Z_) < 0.5) {
    norm_DD(Perp1_, X_) = norm_DD(Norm_, Y_);
    norm_DD(Perp1_, Y_) = -norm_DD(Norm_, X_);
    norm_DD(Perp1_, Z_) = 0.0;
    norm_DD(Perp2_, X_) = norm_DD(Norm_, Z_) * norm_DD(Norm_, X_);
    norm_DD(Perp2_, Y_) = norm_DD(Norm_, Z_) * norm_DD(Norm_, Y_);
    norm_DD(Perp2_, Z_) =
        -pow(norm_DD(Norm_, X_), 2) - pow(norm_DD(Norm_, Y_), 2);
  } else {
    norm_DD(Perp1_, X_) = 0.0;
    norm_DD(Perp1_, Y_) = norm_DD(Norm_, Z_);
    norm_DD(Perp1_, Z_) = -norm_DD(Norm_, Y_);
    norm_DD(Perp2_, X_) =
        -pow(norm_DD(Norm_, Y_), 2) - pow(norm_DD(Norm_, Z_), 2);
    norm_DD(Perp2_, Y_) = norm_DD(Norm_, Y_) * norm_DD(Norm_, X_);
    norm_DD(Perp2_, Z_) = norm_DD(Norm_, Z_) * norm_DD(Norm_, X_);
  }

  inv = 1.0 / sqrt(norm_DD(Perp1_, X_) * norm_DD(Perp1_, X_) +
                   norm_DD(Perp1_, Y_) * norm_DD(Perp1_, Y_) +
                   norm_DD(Perp1_, Z_) * norm_DD(Perp1_, Z_));
  norm_DD(Perp1_, X_) *= inv;
  norm_DD(Perp1_, Y_) *= inv;
  norm_DD(Perp1_, Z_) *= inv;

  inv = 1.0 / sqrt(norm_DD(Perp2_, X_) * norm_DD(Perp2_, X_) +
                   norm_DD(Perp2_, Y_) * norm_DD(Perp2_, Y_) +
                   norm_DD(Perp2_, Z_) * norm_DD(Perp2_, Z_));
  norm_DD(Perp2_, X_) *= inv;
  norm_DD(Perp2_, Y_) *= inv;
  norm_DD(Perp2_, Z_) *= inv;
}
//-------------------------------------------------------------------------

/** Get the Electic field as from the fluid description */
void FluidPicInterface::setFluidFieldsNode(double *Ex, double *Ey, double *Ez,
                                           double *Bx, double *By, double *Bz,
                                           const int i, const int j,
                                           const int k) {
  // This function is used by IPIC3D only.
  const int iBlock = 0;
  if (doGetFromGM(i, j, k)) {
    (*Ex) = getEx(iBlock, i, j, k);
    (*Ey) = getEy(iBlock, i, j, k);
    (*Ez) = getEz(iBlock, i, j, k);

    (*Bx) = getBx(iBlock, i, j, k);
    (*By) = getBy(iBlock, i, j, k);
    (*Bz) = getBz(iBlock, i, j, k);
  }
}

// Data recived from SWMF coupler
void FluidPicInterface::ReadFromGMinit(const int *const paramint,
                                       const double *const ParamRealRegion,
                                       const double *const ParamRealComm,
                                       const stringstream * const ss) {

  nDim = paramint[0];
  nVarFluid = paramint[2];
  nFluid = paramint[3];
  nSpecies = paramint[4];

  // c++ index starts from 0. So, minus 1.
  iPe = paramint[5] - 1;
  iBx = paramint[6] - 1;
  iBy = iBx + 1;
  iBz = iBy + 1;

  iEx = paramint[7] - 1;
  iEy = iEx + 1;
  iEz = iEy + 1;

  nCellPerPatch = paramint[8];

  useElectronFluid = iEx > 1;

  if (useElectronFluid) {
    nIonFluid = -1; // Do not distinguish between electrons and ions.
    nIon = -1;
    nS = nFluid;
  } else {
    nIonFluid = nFluid;
    nIon = nFluid + nSpecies - 1; // Assuming one electron species.
    nS = nIon + 1;                // + electron
  }

  nSIn = nS;
  if (doSplitSpecies) {
    if (splitType == "Bx" || splitType == "By" || splitType == "Bz") {
      nS *= 2;
      iSPic2Mhd_I = new int[nS];
      for (int i = 0; i < nS; i++) {
        iSPic2Mhd_I[i] = floor((i + 0.5) / 2.0);
      }
    } else if (splitType == "ElectronOnly") {
      // Assume there are two ion species/fluids in MHD
      if (nS != 3) {
        cout << "'ElectronOnly' type splitting is not supported for nSpecies "
                "= " << nSpecies << " nFluid = " << nFluid << " nS = " << nS
             << endl;
        abort();
      }
      nS += 1;
      iSPic2Mhd_I = new int[nS];
      iSPic2Mhd_I[0] = 0;
      iSPic2Mhd_I[1] = 0;
      for (int i = 2; i < nS; i++) {
        iSPic2Mhd_I[i] = i - 1;
      }

    } else {
      cout << " splitType = " << splitType << " is not found!" << endl;
      abort();
    }
  }

  useMultiFluid = nIonFluid > 1;
  useMultiSpecies = nSpecies > 1;

  nVarCoupling = nVarFluid + 3; // nVarFluid + (Jx, Jy, Jz)

  iRho_I = new int[nS];
  iRhoUx_I = new int[nS];
  iRhoUy_I = new int[nS];
  iRhoUz_I = new int[nS];
  iUx_I = new int[nS];
  iUy_I = new int[nS];
  iUz_I = new int[nS];
  iPpar_I = new int[nS];
  iP_I = new int[nS];

  int n = 9;
  if (useMultiSpecies) {
    // MultiSpecies. Densities of each species are known. Total velocity
    // and total pressure are known.
    iRhoTotal = paramint[n++] - 1;
    iRho_I[0] = iRhoTotal + 1;
    iRhoUx_I[0] = paramint[n++] - 1;
    iUx_I[0] = iRhoUx_I[0];
    iRhoUy_I[0] = iRhoUx_I[0] + 1;
    iUy_I[0] = iRhoUy_I[0];
    iRhoUz_I[0] = iRhoUx_I[0] + 2;
    iUz_I[0] = iRhoUz_I[0];
    iPpar_I[0] = paramint[n++] - 1;
    iP_I[0] = paramint[n++] - 1;

    for (int iIon = 1; iIon < nIon; ++iIon) {
      iRho_I[iIon] = iRho_I[0] + iIon;
      iRhoUx_I[iIon] = iRhoUx_I[0];
      iUx_I[iIon] = iUx_I[0];
      iRhoUy_I[iIon] = iRhoUy_I[0];
      iUy_I[iIon] = iUy_I[0];
      iRhoUz_I[iIon] = iRhoUz_I[0];
      iUz_I[iIon] = iUz_I[0];
      iPpar_I[iIon] = iPpar_I[0];
      iP_I[iIon] = iP_I[0];
    }
  } else {
    // Not multi-species
    for (int iFluid = 0; iFluid < nFluid; ++iFluid)
      iRho_I[iFluid] = paramint[n++] - 1;
    for (int iFluid = 0; iFluid < nFluid; ++iFluid) {
      iRhoUx_I[iFluid] = paramint[n++] - 1;
      iUx_I[iFluid] = iRhoUx_I[iFluid];
      iRhoUy_I[iFluid] = iRhoUx_I[iFluid] + 1;
      iUy_I[iFluid] = iRhoUy_I[iFluid];
      iRhoUz_I[iFluid] = iRhoUx_I[iFluid] + 2;
      iUz_I[iFluid] = iRhoUz_I[iFluid];
    }

    for (int iFluid = 0; iFluid < nFluid; ++iFluid)
      iPpar_I[iFluid] = paramint[n++] - 1;
    for (int iFluid = 0; iFluid < nFluid; ++iFluid)
      iP_I[iFluid] = paramint[n++] - 1;
  }

  nVec = nFluid + 1;
  if (useElectronFluid)
    nVec++; // + E field.
  if (nVec > nVecMax) {
    if (myrank == 0)
      cout << "Error: nVec > nVecMax!!!!" << endl;
    // MPI_Abort(MPI_COMM_MYSIM, iErr);
  }
  for (int iVec = 0; iVec < nFluid; iVec++)
    vecIdxStart_I[iVec] = iRhoUx_I[iVec];
  vecIdxStart_I[nFluid] = iBx;
  if (useElectronFluid)
    vecIdxStart_I[nFluid + 1] = iEx;

  // See GM/BATSRUS/src/ModExtraVariables.f90.
  useAnisoP = iPpar_I[0] != 0;
  useMhdPe = iPe != 0;

  iJx = nVarFluid;
  iJy = iJx + 1;
  iJz = iJx + 2;

  Si2No_V = new double[nVarCoupling];
  No2Si_V = new double[nVarCoupling];
  for (int i = 0; i < nVarCoupling; i++)
    Si2No_V[i] = 1;

  n = 0;
  for (int i = 0; i < 3; i++) {
    phyMin_D[i] = ParamRealRegion[n++]; // Lmin
    phyMax_D[i] = phyMin_D[i] + ParamRealRegion[n++];
    dx_D[i] = ParamRealRegion[n++]; // dx
    lenGst_D[i] = phyMax_D[i] - phyMin_D[i];
  }
  for(int i = 0; i < nDim; i++){
    lenGst_D[i] += 2*dx_D[i];
  }

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      R_DD[i][j] = ParamRealRegion[n++];
    }
  }

  // Normalization parameters.
  Lnorm = ParamRealRegion[n++];
  Unorm = ParamRealRegion[n++];
  Mnorm = ParamRealRegion[n++];

  ScalingFactor = ParamRealRegion[n++];

  doRotate = false;
  double csmall = 1e-7;
  for (int i = 0; i < nDim; i++)
    if (fabs(R_DD[i][i] - 1) > csmall) {
      doRotate = true;
    }

  for (int i = 0; i < 3; i++)
    nNodeGst_D[i] = (int)(lenGst_D[i] / dx_D[i] + 0.5);

  // Add 1 as we count the nodes not cells
  for (int i = 0; i < nDim; i++)
    nNodeGst_D[i] += 1;

  QoQi_S = new double[nS];
  MoMi_S = new double[nS];

  /** Do not change the order of the following lines. */
  n = 0;
  if (useElectronFluid) {
    for (int i = 0; i < nSIn; ++i) {
      QoQi_S[i] = ParamRealComm[n++];
      MoMi_S[i] = ParamRealComm[n++];
    }
  } else {
    QoQi_S[0] = -1.0;
    for (int i = 1; i < nSIn; ++i) {
      QoQi_S[i] = ParamRealComm[n++];
      MoMi_S[i] = ParamRealComm[n++];
    }
  }

  // Electron pressure ratio: Pe/Ptotal
  PeRatio = ParamRealComm[n++];

  rPlanetSi = ParamRealComm[n++];
  MhdNo2SiL = ParamRealComm[n++];
  /** Do not change the order of above lines. */

  // Normalization units converted [SI] -> [cgs]
  if (Lnorm > 0) {
    Lnorm *= 100.0;
  } else {
    Lnorm = 1.0;
  }
  if (Unorm > 0) {
    Unorm *= 100.0;
  } else {
    Unorm = 1.0;
  }
  if (Mnorm > 0) {
    Mnorm *= 1000.0;
  } else {
    Mnorm = 1.0;
  }
  
  InitData();
  ReNormLength();
  checkParam();
}

/** Check the parameters passed or calculated from BATSRUS*/
void FluidPicInterface::checkParam() {

  // #ifndef COUPLEAMPS
  //   assert_ge(lenGst_D[0], 0.0);
  //   assert_ge(lenGst_D[1], 0.0);
  //   assert_ge(lenGst_D[2], 0.0);

  //   assert_ge(dx_D[0], 0.0);
  //   assert_ge(dx_D[1], 0.0);
  //   assert_ge(dx_D[2], 0.0);
  // #endif

  if (useMultiFluid && !useMhdPe) {
    cout << " Use multi-fluid but do not use electron pressure. This case is "
            "not supported so far!!!" << endl;
    abort();
  }
}

bool FluidPicInterface::doGetFromGM(int i, int j, int k) {
  if (doCoupleAMPS)
    return true;

  // 1) The first step initialize from GM: all nodes need information from GM;
  // 2) During updates, only boundary nodes needs to be overwritten by GM.
  // Input: i, j, k are the local indexes.
  bool doGetGM;
  int minDn;
  if (doNeedBCOnly) {
    doGetGM = false;
    int ig = StartIdx_D[0] + i - 1;
    int nxg = getFluidNxc() + 3;
    int jg = StartIdx_D[1] + j - 1;
    int nyg = getFluidNyc() + 3;
    int kg = StartIdx_D[2] + k - 1;
    int nzg = getFluidNzc() + 3;
    minDn = std::min(ig, nxg - ig - 1);
    if (nDim > 1)
      minDn = std::min(minDn, std::min(jg, nyg - jg - 1));
    if (nDim > 2)
      minDn = std::min(minDn, std::min(kg, nzg - kg - 1));
    if (minDn < nBCLayer)
      doGetGM = true;
  } else {
    doGetGM = true;
  }
  return doGetGM;
}

/** get the number of cordinate points used by the simulation. recived from GM
 */
void FluidPicInterface::GetNgridPnt(int &nPoint) {
  double *Pos_I = nullptr, *state_I = nullptr;
  int *iPoint_I = nullptr;
  bool doCount, doGetPos, doSetData;

  doCount = true;
  doGetPos = false;
  doSetData = false;
  get_region_points(doCount, doGetPos, doSetData, nPoint, Pos_I, state_I,
                    iPoint_I);
}

void FluidPicInterface::get_region_points(bool doCount, bool doGetPos,
                                          bool doSetData, int &nPoint,
                                          double *pos_I, double *state_I,
                                          int *iPoint_I) {
  int i, j, k, iMin, iMax, jMin, jMax, kMin, kMax;
  double pic_D[3], mhd_D[3];

  iMin = 0;
  iMax = 1;
  jMin = 0;
  jMax = 1;
  kMin = 0;
  kMax = 1;
  iMax = nxnLG;
  if (nDim >= 2)
    jMax = nynLG;
  if (nDim == 3)
    kMax = nznLG;

  int n = 0, ii = 0;
  nPoint = 0;
  for (int iBlock = 0; iBlock < nBlock; iBlock++)
    for (i = iMin; i < iMax; i++)
      for (j = jMin; j < jMax; j++)
        for (k = kMin; k < kMax; k++) {
          if (doGetFromGM(i, j, k)) {

            if (doCount)
              nPoint += nDim;

            if (doGetPos) {
              pic_D[x_] = (i + StartIdx_D[x_] - 2) * dx_D[x_];
              pic_D[y_] = (j + StartIdx_D[y_] - 2) * dx_D[y_];
              pic_D[z_] = (k + StartIdx_D[z_] - 2) * dx_D[z_];
              pic_to_Mhd_Vec(pic_D, mhd_D);
              for (int iDim = 0; iDim < nDim; iDim++) {
                pos_I[n++] = mhd_D[iDim] * No2SiL;
              }

            } // doGetPos

            if (doSetData) {
              for (int iVar = 0; iVar < nVarFluid; iVar++) {
                int idx;
                idx = iVar + nVarFluid * (iPoint_I[ii] - 1);
                State_BGV(iBlock, i, j, k, iVar) = state_I[idx];		
              }
              ii++;

              // Convert vectors from MHD coordinates to PIC
              // coordinates----------
              for (int iVec = 0; iVec < nVec; iVec++) {
                int idx0, idx1;
                idx0 = vecIdxStart_I[iVec];
                idx1 = idx0 + nDim;
                for (int iVar = idx0; iVar < idx1; iVar++)
                  mhd_D[iVar - idx0] = State_BGV(iBlock, i, j, k, iVar);
                mhd_to_Pic_Vec(mhd_D, pic_D, true);
                for (int iVar = idx0; iVar < idx1; iVar++) {
                  State_BGV(iBlock, i, j, k, iVar) = pic_D[iVar - idx0];
                }
              }
              //------------------------------------------------------------------

            } // doSetData
          }
        } // i j k

  if (doSetData && nDim < 3) {
    // Fill in the data in the ignored dimension.
    if (nDim == 1) {
      int j0 = 0;
      for (int iBlock = 0; iBlock < nBlock; iBlock++)
        for (i = iMin; i < iMax; i++)
          for (j = 1; j < nynLG; j++)
            for (k = kMin; k < kMax; k++)
              for (int iVar = 0; iVar < nVarFluid; iVar++) {
                State_BGV(iBlock, i, j, k, iVar) =
                    State_BGV(iBlock, i, j0, k, iVar);
              }
    }

    int k0 = 0;
    for (int iBlock = 0; iBlock < nBlock; iBlock++)
      for (i = iMin; i < iMax; i++)
        for (j = iMin; j < nynLG; j++)
          for (k = 1; k < nznLG; k++)
            for (int iVar = 0; iVar < nVarFluid; iVar++) {
              State_BGV(iBlock, i, j, k, iVar) =
                  State_BGV(iBlock, i, j, k0, iVar);
            }
  }
}

/** Get index of node points recived from GM */
void FluidPicInterface::GetGridPnt(double *Pos_I) {
  double *state_I = nullptr;
  int *iPoint_I = nullptr, nPoint;
  bool doCount, doGetPos, doSetData;

  doCount = false;
  doGetPos = true;
  doSetData = false;
  get_region_points(doCount, doGetPos, doSetData, nPoint, Pos_I, state_I,
                    iPoint_I);
}

void FluidPicInterface::setStateVar(double *state_I, int *iPoint_I) {
  const int x_ = 0, y_ = 1, z_ = 2;
  int i, j, k;

  State_BGV.clear();
  State_BGV.init(nBlock, nxnLG, nynLG, nznLG, nVarCoupling);

  Bc_BGD.clear();
  Bc_BGD.init(nBlock, nxnLG - 1, nynLG - 1, nznLG - 1, 3);

  //-----------------Put data to State_BGV--------------------
  double *Pos_I = nullptr;
  int nPoint;
  bool doCount, doGetPos, doSetData;
  doCount = false;
  doGetPos = false;
  doSetData = true;
  get_region_points(doCount, doGetPos, doSetData, nPoint, Pos_I, state_I,
                    iPoint_I);
  //-----------------------------------------------------------
  for (int iBlock = 0; iBlock < nBlock; iBlock++)
    for (i = 0; i < nxnLG - 1; i++)
      for (j = 0; j < nynLG - 1; j++)
        for (k = 0; k < nznLG - 1; k++) {
          if (doGetFromGM(i, j, k)) {
            Bc_BGD(iBlock, i, j, k, x_) =
                0.125 * (State_BGV(iBlock, i, j, k, iBx) +
                         State_BGV(iBlock, i + 1, j, k, iBx) +
                         State_BGV(iBlock, i, j + 1, k, iBx) +
                         State_BGV(iBlock, i + 1, j + 1, k, iBx) +
                         State_BGV(iBlock, i, j, k + 1, iBx) +
                         State_BGV(iBlock, i + 1, j, k + 1, iBx) +
                         State_BGV(iBlock, i, j + 1, k + 1, iBx) +
                         State_BGV(iBlock, i + 1, j + 1, k + 1, iBx));

            Bc_BGD(iBlock, i, j, k, y_) =
                0.125 * (State_BGV(iBlock, i, j, k, iBy) +
                         State_BGV(iBlock, i + 1, j, k, iBy) +
                         State_BGV(iBlock, i, j + 1, k, iBy) +
                         State_BGV(iBlock, i + 1, j + 1, k, iBy) +
                         State_BGV(iBlock, i, j, k + 1, iBy) +
                         State_BGV(iBlock, i + 1, j, k + 1, iBy) +
                         State_BGV(iBlock, i, j + 1, k + 1, iBy) +
                         State_BGV(iBlock, i + 1, j + 1, k + 1, iBy));

            Bc_BGD(iBlock, i, j, k, z_) =
                0.125 * (State_BGV(iBlock, i, j, k, iBz) +
                         State_BGV(iBlock, i + 1, j, k, iBz) +
                         State_BGV(iBlock, i, j + 1, k, iBz) +
                         State_BGV(iBlock, i + 1, j + 1, k, iBz) +
                         State_BGV(iBlock, i, j, k + 1, iBz) +
                         State_BGV(iBlock, i + 1, j, k + 1, iBz) +
                         State_BGV(iBlock, i, j + 1, k + 1, iBz) +
                         State_BGV(iBlock, i + 1, j + 1, k + 1, iBz));
          }
        }

  // J = curl B/ mu0
  double invdx, invdy, invdz;
  double compZdx, compXdy, compYdz, compXdz, compZdy, compYdx;

  // 1/(dx*mu0)
  invdx = 1.0 / (dx_D[x_] * No2SiL * 4.0 * 3.14159265359 * 1.0e-7);
  invdy = 1.0 / (dx_D[y_] * No2SiL * 4.0 * 3.14159265359 * 1.0e-7);
  invdz = 1.0 / (dx_D[z_] * No2SiL * 4.0 * 3.14159265359 * 1.0e-7);

  for (int iBlock = 0; iBlock < nBlock; iBlock++)
    for (i = 0; i < nxnLG - 2; i++)
      for (j = 0; j < nynLG - 2; j++)
        for (k = 0; k < nznLG - 2; k++) {
          if (doGetFromGM(i, j, k)) {
            compZdy = 0.25 * invdy * (Bc_BGD(iBlock, i, j + 1, k, z_) -
                                      Bc_BGD(iBlock, i, j, k, z_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k, z_) -
                                      Bc_BGD(iBlock, i + 1, j, k, z_) +
                                      Bc_BGD(iBlock, i, j + 1, k + 1, z_) -
                                      Bc_BGD(iBlock, i, j, k + 1, z_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k + 1, z_) -
                                      Bc_BGD(iBlock, i + 1, j, k + 1, z_));

            compXdy = 0.25 * invdy * (Bc_BGD(iBlock, i, j + 1, k, x_) -
                                      Bc_BGD(iBlock, i, j, k, x_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k, x_) -
                                      Bc_BGD(iBlock, i + 1, j, k, x_) +
                                      Bc_BGD(iBlock, i, j + 1, k + 1, x_) -
                                      Bc_BGD(iBlock, i, j, k + 1, x_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k + 1, x_) -
                                      Bc_BGD(iBlock, i + 1, j, k + 1, x_));

            compYdz = 0.25 * invdz * (Bc_BGD(iBlock, i, j, k + 1, y_) -
                                      Bc_BGD(iBlock, i, j, k, y_) +
                                      Bc_BGD(iBlock, i + 1, j, k + 1, y_) -
                                      Bc_BGD(iBlock, i + 1, j, k, y_) +
                                      Bc_BGD(iBlock, i, j + 1, k + 1, y_) -
                                      Bc_BGD(iBlock, i, j + 1, k, y_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k + 1, y_) -
                                      Bc_BGD(iBlock, i + 1, j + 1, k, y_));

            compXdz = 0.25 * invdz * (Bc_BGD(iBlock, i, j, k + 1, x_) -
                                      Bc_BGD(iBlock, i, j, k, x_) +
                                      Bc_BGD(iBlock, i + 1, j, k + 1, x_) -
                                      Bc_BGD(iBlock, i + 1, j, k, x_) +
                                      Bc_BGD(iBlock, i, j + 1, k + 1, x_) -
                                      Bc_BGD(iBlock, i, j + 1, k, x_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k + 1, x_) -
                                      Bc_BGD(iBlock, i + 1, j + 1, k, x_));

            compZdx = 0.25 * invdx * (Bc_BGD(iBlock, i + 1, j, k, z_) -
                                      Bc_BGD(iBlock, i, j, k, z_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k, z_) -
                                      Bc_BGD(iBlock, i, j + 1, k, z_) +
                                      Bc_BGD(iBlock, i + 1, j, k + 1, z_) -
                                      Bc_BGD(iBlock, i, j, k + 1, z_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k + 1, z_) -
                                      Bc_BGD(iBlock, i, j + 1, k + 1, z_));

            compYdx = 0.25 * invdx * (Bc_BGD(iBlock, i + 1, j, k, y_) -
                                      Bc_BGD(iBlock, i, j, k, y_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k, y_) -
                                      Bc_BGD(iBlock, i, j + 1, k, y_) +
                                      Bc_BGD(iBlock, i + 1, j, k + 1, y_) -
                                      Bc_BGD(iBlock, i, j, k + 1, y_) +
                                      Bc_BGD(iBlock, i + 1, j + 1, k + 1, y_) -
                                      Bc_BGD(iBlock, i, j + 1, k + 1, y_));

            State_BGV(iBlock, i + 1, j + 1, k + 1, iJx) = compZdy - compYdz;
            State_BGV(iBlock, i + 1, j + 1, k + 1, iJy) = compXdz - compZdx;
            State_BGV(iBlock, i + 1, j + 1, k + 1, iJz) = compYdx - compXdy;

            if (nDim == 2) {
              for (int iBlock = 0; iBlock < nBlock; iBlock++)
                for (int kk = 0; kk <= nznLG - 1; kk += nznLG - 1)
                  for (int iVar = iJx; iVar <= iJz; iVar++) {
                    State_BGV(iBlock, i + 1, j + 1, kk, iVar) =
                        State_BGV(iBlock, i + 1, j + 1, k + 1, iVar);
                  }
            }
          }
        }

  // Fill in ghost cells. This part can be further simplified. - Yuxi
  if (nDim == 2) {
    k = 0;
    for (int iBlock = 0; iBlock < nBlock; iBlock++)
      for (j = 1; j < nynLG - 1; j++) {
        State_BGV(iBlock, 0, j, k, iJx) = State_BGV(iBlock, 1, j, k, iJx);
        State_BGV(iBlock, 0, j, k, iJy) = State_BGV(iBlock, 1, j, k, iJy);
        State_BGV(iBlock, 0, j, k, iJz) = State_BGV(iBlock, 1, j, k, iJz);

        State_BGV(iBlock, nxnLG - 1, j, k, iJx) =
            State_BGV(iBlock, nxnLG - 2, j, k, iJx);
        State_BGV(iBlock, nxnLG - 1, j, k, iJy) =
            State_BGV(iBlock, nxnLG - 2, j, k, iJy);
        State_BGV(iBlock, nxnLG - 1, j, k, iJz) =
            State_BGV(iBlock, nxnLG - 2, j, k, iJz);
      }

    for (int iBlock = 0; iBlock < nBlock; iBlock++)
      for (i = 0; i < nxnLG; i++) {
        State_BGV(iBlock, i, 0, k, iJx) = State_BGV(iBlock, i, 1, k, iJx);
        State_BGV(iBlock, i, 0, k, iJy) = State_BGV(iBlock, i, 1, k, iJy);
        State_BGV(iBlock, i, 0, k, iJz) = State_BGV(iBlock, i, 1, k, iJz);

        State_BGV(iBlock, i, nynLG - 1, k, iJx) =
            State_BGV(iBlock, i, nynLG - 2, k, iJx);
        State_BGV(iBlock, i, nynLG - 1, k, iJy) =
            State_BGV(iBlock, i, nynLG - 2, k, iJy);
        State_BGV(iBlock, i, nynLG - 1, k, iJz) =
            State_BGV(iBlock, i, nynLG - 2, k, iJz);
      }

  } else if (nDim == 3) {
    for (int iBlock = 0; iBlock < nBlock; iBlock++)
      for (i = 1; i < nxnLG - 1; i++)
        for (j = 1; j < nynLG - 1; j++) {
          State_BGV(iBlock, i, j, 0, iJx) = State_BGV(iBlock, i, j, 1, iJx);
          State_BGV(iBlock, i, j, 0, iJy) = State_BGV(iBlock, i, j, 1, iJy);
          State_BGV(iBlock, i, j, 0, iJz) = State_BGV(iBlock, i, j, 1, iJz);

          State_BGV(iBlock, i, j, nznLG - 1, iJx) =
              State_BGV(iBlock, i, j, nznLG - 2, iJx);
          State_BGV(iBlock, i, j, nznLG - 1, iJy) =
              State_BGV(iBlock, i, j, nznLG - 2, iJy);
          State_BGV(iBlock, i, j, nznLG - 1, iJz) =
              State_BGV(iBlock, i, j, nznLG - 2, iJz);
        }

    for (int iBlock = 0; iBlock < nBlock; iBlock++)
      for (i = 1; i < nxnLG - 1; i++)
        for (k = 0; k < nznLG; k++) {
          State_BGV(iBlock, i, 0, k, iJx) = State_BGV(iBlock, i, 1, k, iJx);
          State_BGV(iBlock, i, 0, k, iJy) = State_BGV(iBlock, i, 1, k, iJy);
          State_BGV(iBlock, i, 0, k, iJz) = State_BGV(iBlock, i, 1, k, iJz);

          State_BGV(iBlock, i, nynLG - 1, k, iJx) =
              State_BGV(iBlock, i, nynLG - 2, k, iJx);
          State_BGV(iBlock, i, nynLG - 1, k, iJy) =
              State_BGV(iBlock, i, nynLG - 2, k, iJy);
          State_BGV(iBlock, i, nynLG - 1, k, iJz) =
              State_BGV(iBlock, i, nynLG - 2, k, iJz);
        }

    for (int iBlock = 0; iBlock < nBlock; iBlock++)
      for (j = 0; j < nynLG; j++)
        for (k = 0; k < nznLG; k++) {
          State_BGV(iBlock, 0, j, k, iJx) = State_BGV(iBlock, 1, j, k, iJx);
          State_BGV(iBlock, 0, j, k, iJy) = State_BGV(iBlock, 1, j, k, iJy);
          State_BGV(iBlock, 0, j, k, iJz) = State_BGV(iBlock, 1, j, k, iJz);

          State_BGV(iBlock, nxnLG - 1, j, k, iJx) =
              State_BGV(iBlock, nxnLG - 2, j, k, iJx);
          State_BGV(iBlock, nxnLG - 1, j, k, iJy) =
              State_BGV(iBlock, nxnLG - 2, j, k, iJy);
          State_BGV(iBlock, nxnLG - 1, j, k, iJz) =
              State_BGV(iBlock, nxnLG - 2, j, k, iJz);
        }
  }

  ReNormVariables();
  Moment2Velocity();
  isFirstTime = false;
}

/** print info for coupling */
void FluidPicInterface::PrintFluidPicInterface() {

  if (myrank == 0) {
    cout << "nS = " << nS  << " Sum all particle masses = " << SumMass << endl;
    cout << "useMultiFluid   = " << (useMultiFluid? "T":"F");
    if(useMultiFluid) cout<< " nFluid = " << nFluid << endl;
    cout << "useMultiSpecies = " << (useMultiSpecies? "T":"F");
    if(useMultiSpecies) cout<< " nSpecies =" << nSpecies << endl;
    cout << "useElectronFluid = " << (useElectronFluid? "T":"F") << endl;
    for (int is = 0; is < nS; is++) {
      cout << "Q/Qi[" << is << "] = " << QoQi_S[is] << endl;
      cout << "M/Mi[" << is << "] = " << MoMi_S[is] << endl;
    }
    if (!useMhdPe)
      cout << "Pe/Ptotal = " << PeRatio << endl;
    cout << "========== Unit conversion factors ==============" << endl;
    cout << " Si2NoRho = " << Si2NoRho << endl;
    cout << " Si2NoV   = " << Si2NoV << endl;
    cout << " Si2NoB   = " << Si2NoB << endl;
    cout << " Si2NoE   = " << Si2NoE << endl;
    cout << " Si2NoP   = " << Si2NoP << endl;
    cout << " Si2NoJ   = " << Si2NoJ << endl;
    cout << " Si2NoL   = " << Si2NoL << endl;
    cout << "===================================================" << endl;
  }
}

// Read Satellite files.
void FluidPicInterface::read_satellite_file(string filename) {
  std::ifstream file;
  string line;
  vector<array<double, 4> > satInfo_II;
  file.open(filename.c_str());
  while (getline(file, line)) {
    if (line.substr(0, 6) == "#START")
      break;
  }

  int yr, mo, dy, hr, mn, sc;
  double msc;
  array<double, 4> data;
  // A better way??
  while (file >> yr &&      // yr
         file >> mo &&      // Mo
         file >> dy &&      // Dy
         file >> hr &&      // Hr
         file >> mn &&      // Mn
         file >> sc &&      // Sc
         file >> msc &&     // Msc
         file >> data[1] && // X
         file >> data[2] && // Y
         file >> data[3]    // Z
         ) {
    data[0] = convert_time(yr, mo, dy, hr, mn, sc, msc);

    // Assume the location of satellite is in normalized BATSRUS unit.It
    // is usually planet radius.
    data[1] = data[1] * getMhdNo2NoL() - getFluidStartX();
    data[2] = data[2] * getMhdNo2NoL() - getFluidStartY();
    data[3] = data[3] * getMhdNo2NoL() - getFluidStartZ();

    satInfo_II.push_back(data);
  }

  int nline;
  nline = satInfo_II.size();
  for (int i = 0; i < nline; i++) {
    cout.precision(10);
  }
  file.close();
  satInfo_III.push_back(satInfo_II);
}

int FluidPicInterface::second_to_clock_time(int second) {
  int iHr, iMn, iSc, time;
  iHr = floor(second / 3600);
  second -= iHr * 3600;
  iMn = floor(second / 60);
  iSc = second - iMn * 60;

  time = iSc + 100 * iMn + 10000 * iHr;
  return time;
}

/** Convert real time into simulation time in second. **/
double FluidPicInterface::convert_time(int yr, int mo, int dy, int hr, int mn,
                                       int sc, double msc) {
  double time;
  double doyStart, doyNow;
  doyStart = doy(iYear, iMonth, iDay);
  doyNow = doy(yr, mo, dy);

  time =
      (((doyNow - doyStart) * 24 + (hr - iHour)) * 60 + (mn - iMinute)) * 60 +
      sc - iSecond + msc / 1000.0;

  bool doTestMe;
  doTestMe = false;
  if (doTestMe) {
    cout << " yr= " << yr << " mo= " << mo << " dy= " << dy << " hr= " << hr
         << " mn= " << mn << " sc= " << sc << " msc=" << msc
         << " iyr= " << iYear << " imo= " << iMonth << " idy= " << iDay
         << " ihr= " << iHour << " imn= " << iMinute << " isc= " << iSecond
         << " time= " << time << endl;
  }
  return time;
}

/** day of year **/
int FluidPicInterface::doy(int yr, int mo, int dy) {
  int nDayInMonth_I[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  int doy;

  if (yr % 4 == 0)
    nDayInMonth_I[1]++;

  doy = 0;
  for (int i = 0; i < mo - 1; i++)
    doy += nDayInMonth_I[i];
  doy += dy;

  return doy;
}

/** electron mass given by IPIC3D params while ion mass comes form BATSRUS */
void FluidPicInterface::fixPARAM(double *&qom, int *&npcelx, int *&npcely,
                                 int *&npcelz, int *ns) {

  double qom_el;
  int npx, npy, npz;

  // tmp store old values
  qom_el = qom[0];
  npx = npcelx[0];
  npy = npcely[0];
  npz = npcelz[0];

  delete[] qom;
  delete[] npcelx;
  delete[] npcely;
  delete[] npcelz;

  *ns = nS;
  qom = new double[nS];
  npcelx = new int[nS];
  npcely = new int[nS];
  npcelz = new int[nS];

  if (!useElectronFluid) {
    MoMi_S[0] = QoQi_S[0] / qom_el;
  }

  SumMass = 0.0;
  for (int is = 0; is < nSIn; is++)
    SumMass += MoMi_S[is];

  // Fix the values of MoMi_S and QoQi_S;
  MoMi0_S = new double[nSIn];
  QoQi0_S = new double[nSIn];
  for (int i = 0; i < nSIn; i++) {
    MoMi0_S[i] = MoMi_S[i];
    QoQi0_S[i] = QoQi_S[i];
  }

  if (doSplitSpecies) {
    int idx;
    for (int i = 0; i < nS; i++) {
      idx = iSPic2Mhd_I[i];
      QoQi_S[i] = QoQi0_S[idx];
      MoMi_S[i] = MoMi0_S[idx];
    }
  }

  // iones
  for (int is = 0; is < nS; is++) {
    qom[is] = QoQi_S[is] / MoMi_S[is];
    npcelx[is] = npx;
    npcely[is] = npy;
    npcelz[is] = npz;
  }

  if (myrank == 0) {
    cout << "============= fixPARAM =============" << endl;
    for (int is = 0; is < nS; is++) {
      cout << "qom[" << is << "]  =  " << qom[is] << endl;
      cout << "npcelx[" << is << "] = " << npcelx[is] << endl;
      cout << "npcely[" << is << "] = " << npcely[is] << endl;
      cout << "npcelz[" << is << "] = " << npcelz[is] << endl;
      cout << "Q/Qi[" << is << "] = " << QoQi_S[is] << endl;
      cout << "M/Mi[" << is << "] = " << MoMi_S[is] << endl;
    }
    if (!useMhdPe && !useElectronFluid)
      cout << "Pe/Ptotal = " << PeRatio << endl;
    cout << "===================================" << endl;
  }
}

double FluidPicInterface::getSmoothFactor(int i, int j, int k) const {
  double smooth;
  int iMin, jMin, kMin, idxMin;

  int ig, jg, kg;
  getGlobalIndex(i, j, k, &ig, &jg, &kg);

  // The edge for PIC domain is node with index 1. It is different from
  // standalone PIC
  iMin = std::min(ig - 1, nNodeGst_D[0] - ig - 2);
  jMin = std::min(jg - 1, nNodeGst_D[1] - jg - 2);
  kMin = std::min(kg - 1, nNodeGst_D[2] - kg - 2);
  idxMin = std::min(iMin, jMin);

  if (nDim > 2)
    idxMin = std::min(idxMin, kMin);

  double ix;
  if (idxMin < nBoundarySmooth) {
    ix = (nBoundarySmooth - idxMin) / nBoundarySmooth;
    smooth = ix * boundarySmoothFactor + (1 - ix) * innerSmoothFactor;
  } else {
    smooth = innerSmoothFactor;
  }

  if (smooth > 1)
    smooth = 1;
  return smooth;
}

string FluidPicInterface::expandVariable(string inVars) const {
  // Expand the plot variables inside { };
  string::size_type pos1, pos2;
  string var0;

  pos1 = inVars.find_first_of("{");
  while (pos1 != string::npos) {
    pos2 = inVars.find_first_of("}");
    if (pos2 == string::npos) {
      cout << "Variables should be inside { }: " << inVars << endl;
      abort();
    }

    var0 = inVars.substr(pos1 + 1, pos2 - pos1 - 1);
    inVars.erase(pos1, pos2 - pos1 + 1);
    if (var0 == "fluid") {
      inVars += " rhoS0 rhoS1 Bx By Bz Ex Ey Ez uxS0 uyS0 uzS0 uxS1 uyS1 "
                "uzS1 pS0 pS1 pXXS0 pYYS0 pZZS0 pXYS0 pXZS0 pYZS0 pXXS1 "
                "pYYS1 pZZS1 pXYS1 pXZS1 pYZS1";
      for (int is = 2; is < nS; is++) {
        inVars += addPlasmaVar(
            "rhoS uxS uyS uzS pS pXXS pYYS pZZS pXYS pXZS pYZS", is);
      }
    } else if (var0 == "all") {
      inVars += " qS0 qS1 Bx By Bz Ex Ey Ez kXXS0 kYYS0 kZZS0 kXYS0 kXZS0 "
                "kYZS0 kXXS1 kYYS1 kZZS1 kXYS1 kXZS1 kYZS1 jxS0 jyS0 jzS0 "
                "jxS1 jyS1 jzS1";
      for (int is = 2; is < nS; is++) {
        inVars +=
            addPlasmaVar("rhoS jxS jyS jzS kXXS kYYS kZZS kXYS kXZS kYZS", is);
      }
    }
    pos1 = inVars.find_first_of("{");
  }
  return inVars;
}

string FluidPicInterface::addPlasmaVar(string varString, int is) const {
  string::size_type pos1;
  stringstream ss;
  ss << is;
  string iString = ss.str();
  varString.insert(0, " ");

  pos1 = varString.find_first_of("S");
  while (pos1 != string::npos) {
    varString.insert(pos1 + 1, iString);
    pos1 = varString.find_first_of("S", pos1 + 1);
  }

  return varString;
}

void FluidPicInterface::pic_to_Mhd_Vec(double const *vecIn_D, double *vecOut_D,
                                       bool isZeroOrigin) const {
  /** 1) Change a vector in coupling PIC coordinates to MHD coordinates.
      If not isZeroOrigin, then shifting the origin of the coupling
      PIC coordinates to INxRange_I.
   **/

  for (int iDim = 0; iDim < nDim; iDim++) {
    vecOut_D[iDim] = 0;
    for (int jDim = 0; jDim < nDim; jDim++) {
      vecOut_D[iDim] += R_DD[iDim][jDim] * vecIn_D[jDim];
    }
    if (!isZeroOrigin)
      vecOut_D[iDim] += phyMin_D[iDim];
  }
}

void FluidPicInterface::mhd_to_Pic_Vec(double const *vecIn_D, double *vecOut_D,
                                       bool isZeroOrigin) const {
  /** 1) Change a vector in MHD coordinates to coupling PIC coordinates.
      If not isZeroOrigin, then shifting the origin of the coupling
      PIC coordinates to phyMin_D.
  **/

  double vec_D[3];
  if (!isZeroOrigin) {
    for (int iDim = 0; iDim < nDim; iDim++)
      vec_D[iDim] = vecIn_D[iDim] - phyMin_D[iDim];
  } else {
    for (int iDim = 0; iDim < nDim; iDim++)
      vec_D[iDim] = vecIn_D[iDim];
  }

  for (int iDim = 0; iDim < nDim; iDim++) {
    vecOut_D[iDim] = 0;
    for (int jDim = 0; jDim < nDim; jDim++) {
      vecOut_D[iDim] += R_DD[jDim][iDim] * vec_D[jDim];
    }
  } // iDim
}

void FluidPicInterface::CalcFluidState(const double *dataPIC_I,
                                       double *data_I) const {
  /* Input: dataPIC_I
     Output: data_I
     Function:
     dataPIC_I contains the information collected from EACH PIC species,
     and the E and B field. dataPIC_I have the normalized PIC units, and
     the vectors are in PIC coordinates. This function collects the
     MHD values, data_I, from dataPIC_I. data_I is in SI units and the
     vectors are in MHD coordinates.
   */

  double Rhoi, Pi;
  double Rhoe;
  double Rho;
  double PeXX, PeYY, PeZZ, PeXY, PeXZ, PeYZ, PiXX, PiYY, PiZZ, PiXY, PiXZ, PiYZ;
  double PtXX, PtYY, PtZZ, PtXY, PtXZ, PtYZ, BX, BY, BZ;
  double PitXX, PitYY, PitZZ, PitXY, PitXZ, PitYZ;
  double Mx, My, Mz;    // Total momentum.
  double Mix, Miy, Miz; // Momentum of i species.

  // (rho + 3*Moment + 6*p) = 10 variables per PIC species.
  const int nVarSpecies = 10;
  // For example, the rho index for PIC species iSpecies is
  // iRhoPIC+iSpecies*nVarSpecies
  const int iRhoPIC = 0, iMxPIC = 1, iMyPIC = 2, iMzPIC = 3, iPxxPIC = 4,
            iPyyPIC = 5, iPzzPIC = 6, iPxyPIC = 7, iPxzPIC = 8, iPyzPIC = 9;
  int iBxPIC = nS * nVarSpecies, iByPIC = iBxPIC + 1, iBzPIC = iByPIC + 1,
      iExPIC = iBzPIC + 1, iEyPIC = iExPIC + 1, iEzPIC = iEyPIC + 1;

  for (int i = 0; i < nVarFluid; i++) {
    // The variable for hyperbolic clean, is not known by iPIC3D,
    // but it is needed to be passed back. So data_I need to be
    // initilized.
    data_I[i] = 0;
  }

  BX = dataPIC_I[iBxPIC];
  BY = dataPIC_I[iByPIC];
  BZ = dataPIC_I[iBzPIC];

  data_I[iBx] = BX;
  data_I[iBy] = BY;
  data_I[iBz] = BZ;

  if (useElectronFluid) {

    if (doSplitSpecies) {
      cout << " Species splitting for 5/6 moments has not been "
              "implemented!" << endl;
      abort();
    }

    data_I[iEx] = dataPIC_I[iExPIC];
    data_I[iEy] = dataPIC_I[iEyPIC];
    data_I[iEz] = dataPIC_I[iEzPIC];

    for (int iFluid = 0; iFluid < nFluid; ++iFluid) {
      Rhoi = dataPIC_I[iRhoPIC + iFluid * nVarSpecies];

      Mix = dataPIC_I[iMxPIC + iFluid * nVarSpecies];
      Miy = dataPIC_I[iMyPIC + iFluid * nVarSpecies];
      Miz = dataPIC_I[iMzPIC + iFluid * nVarSpecies];

      PiXX = dataPIC_I[iPxxPIC + iFluid * nVarSpecies];
      PiYY = dataPIC_I[iPyyPIC + iFluid * nVarSpecies];
      PiZZ = dataPIC_I[iPzzPIC + iFluid * nVarSpecies];
      Pi = (PiXX + PiYY + PiZZ) / 3.0;
      PiXY = dataPIC_I[iPxyPIC + iFluid * nVarSpecies];
      PiXZ = dataPIC_I[iPxzPIC + iFluid * nVarSpecies];
      PiYZ = dataPIC_I[iPyzPIC + iFluid * nVarSpecies];

      data_I[iRho_I[iFluid]] = Rhoi;

      data_I[iRhoUx_I[iFluid]] = Mix;
      data_I[iRhoUy_I[iFluid]] = Miy;
      data_I[iRhoUz_I[iFluid]] = Miz;

      data_I[iP_I[iFluid]] = Pi;
      if (useAnisoP) {
        data_I[iPpar_I[iFluid]] =
            (BX * PiXX * BX + BY * PiYY * BY + BZ * PiZZ * BZ +
             2.0 * BX * PiXY * BY + 2.0 * BX * PiXZ * BZ +
             2.0 * BY * PiYZ * BZ) /
            (BX * BX + BY * BY + BZ * BZ + 1e-40);
      }
    }
  } else {

    Rhoe = 0;
    PeXX = 0;
    PeYY = 0;
    PeZZ = 0;
    PeXY = 0;
    PeXZ = 0;
    PeYZ = 0;
    Mx = 0;
    My = 0;
    Mz = 0;
    for (int iSpecies = 0; iSpecies < nS; iSpecies++) {
      int iMHD = iSpecies;
      if (doSplitSpecies)
        iMHD = get_iSPic2Mhd_I(iSpecies);
      if (iMHD == 0) {
        // Electron

        Rhoe += dataPIC_I[iRhoPIC + iSpecies * nVarSpecies];
        Mx += dataPIC_I[iMxPIC + iSpecies * nVarSpecies];
        My += dataPIC_I[iMyPIC + iSpecies * nVarSpecies];
        Mz += dataPIC_I[iMzPIC + iSpecies * nVarSpecies];

        PeXX += dataPIC_I[iPxxPIC + iSpecies * nVarSpecies];
        PeYY += dataPIC_I[iPyyPIC + iSpecies * nVarSpecies];
        PeZZ += dataPIC_I[iPzzPIC + iSpecies * nVarSpecies];

        PeXY += dataPIC_I[iPxyPIC + iSpecies * nVarSpecies];
        PeXZ += dataPIC_I[iPxzPIC + iSpecies * nVarSpecies];
        PeYZ += dataPIC_I[iPyzPIC + iSpecies * nVarSpecies];
      }
    } // iSpecies

    if (useMhdPe)
      data_I[iPe] = (PeXX + PeYY + PeZZ) / 3.0;

    Rho = Rhoe;
    PtXX = PeXX;
    PtYY = PeYY;
    PtZZ = PeZZ;
    PtXY = PeXY;
    PtXZ = PeXZ;
    PtYZ = PeYZ;

    for (int iSpecies = 0; iSpecies < nS; ++iSpecies) {
      int iIon;
      iIon = get_iSPic2Mhd_I(iSpecies) - 1; // The first species is electron;

      if (iIon >= 0) {
        Rhoi = dataPIC_I[iRhoPIC + iSpecies * nVarSpecies];
        Mix = dataPIC_I[iMxPIC + iSpecies * nVarSpecies];
        Miy = dataPIC_I[iMyPIC + iSpecies * nVarSpecies];
        Miz = dataPIC_I[iMzPIC + iSpecies * nVarSpecies];

        PiXX = dataPIC_I[iPxxPIC + iSpecies * nVarSpecies];
        PiYY = dataPIC_I[iPyyPIC + iSpecies * nVarSpecies];
        PiZZ = dataPIC_I[iPzzPIC + iSpecies * nVarSpecies];
        PiXY = dataPIC_I[iPxyPIC + iSpecies * nVarSpecies];
        PiXZ = dataPIC_I[iPxzPIC + iSpecies * nVarSpecies];
        PiYZ = dataPIC_I[iPyzPIC + iSpecies * nVarSpecies];

        // Sum to total density/pressure.
        Rho += Rhoi;
        Mx += Mix;
        My += Miy;
        Mz += Miz;

        PtXX += PiXX;
        PtYY += PiYY;
        PtZZ += PiZZ;
        PtXY += PiXY;
        PtXZ += PiXZ;
        PtYZ += PiYZ;

        // Density
        if (useMultiFluid || useMultiSpecies) {
          data_I[iRho_I[iIon]] += Rhoi;
        }

        // Pressure.
        if (useMultiFluid) {
          // ONLY works for iso pressure so far!!!!!
          data_I[iP_I[iIon]] += (PiXX + PiYY + PiZZ) / 3;
          if (useAnisoP) {
            cout << "Multi-fluid model can not work with aniso pressure "
                    "now!!" << endl;
            abort();
          }
        }

        // Momentum.
        if (useMultiFluid) {
          data_I[iRhoUx_I[iIon]] += Mix;
          data_I[iRhoUy_I[iIon]] += Miy;
          data_I[iRhoUz_I[iIon]] += Miz;
        }
      } // if(iIon > 0)
    }   // iSpecies

    if (!(useMultiFluid || useMultiSpecies)) {
      int iIon = 0;
      // Only one ion species.Rho = Rhoi + Rhoe
      data_I[iRho_I[iIon]] = Rho;
    }

    // Do not includes electron density. The total density passed
    // in MHD side is useless, so it doesnot matter whether Rho include
    // electron or not. -- Yuxi
    if (useMultiSpecies)
      data_I[iRhoTotal] = Rho - Rhoe;

    // Momentum
    if (!useMultiFluid && !useElectronFluid) {
      // Include electron momentum.
      data_I[iRhoUx_I[0]] = Mx;
      data_I[iRhoUy_I[0]] = My;
      data_I[iRhoUz_I[0]] = Mz;
    }

    // Sum of ion pressure.
    PitXX = PtXX - PeXX;
    PitYY = PtYY - PeYY;
    PitZZ = PtZZ - PeZZ;
    PitXY = PtXY - PeXY;
    PitXZ = PtXZ - PeXZ;
    PitYZ = PtYZ - PeYZ;

    // Pressure
    if (!useMultiFluid && !useElectronFluid) {
      // AnisoP
      if (useAnisoP) {
        if (useMhdPe)
          data_I[iPpar_I[0]] = (BX * PitXX * BX + BY * PitYY * BY +
                                BZ * PitZZ * BZ + 2.0 * BX * PitXY * BY +
                                2.0 * BX * PitXZ * BZ + 2.0 * BY * PitYZ * BZ) /
                               (BX * BX + BY * BY + BZ * BZ + 1e-40);
        else
          data_I[iPpar_I[0]] = (BX * PtXX * BX + BY * PtYY * BY +
                                BZ * PtZZ * BZ + 2.0 * BX * PtXY * BY +
                                2.0 * BX * PtXZ * BZ + 2.0 * BY * PtYZ * BZ) /
                               (BX * BX + BY * BY + BZ * BZ + 1e-40);
      } // useAnisoP

      // Isotropic Pressure.
      if (useMhdPe)
        data_I[iP_I[0]] = (PitXX + PitYY + PitZZ) / 3;
      else
        data_I[iP_I[0]] = (PtXX + PtYY + PtZZ) / 3;
    }
  } // useElectronFluid is true or false

  // Convert to SI units
  for (int iVar = 0; iVar < nVarFluid; ++iVar) {
    data_I[iVar] *= getNo2Si_V(iVar);
  }

  // Convert the vectors from PIC coordinates to MHD coordinates.
  double mhd_D[3], pic_D[3];
  for (int iVec = 0; iVec < nVec; iVec++) {
    int idx0;
    idx0 = vecIdxStart_I[iVec];
    for (int iVar = idx0; iVar < idx0 + nDim; iVar++)
      pic_D[iVar - idx0] = data_I[iVar];
    pic_to_Mhd_Vec(pic_D, mhd_D, true);
    for (int iVar = idx0; iVar < idx0 + nDim; iVar++)
      data_I[iVar] = mhd_D[iVar - idx0];
  } // iVec
}

void FluidPicInterface::writers_init() {

  //------ Scalar parameters.----------
  std::vector<std::string> scalarName_I;
  std::vector<double> scalarVar_I;
  std::string ms = "mS", qs = "qS";
  for (int i = 0; i < nS; ++i) {
    scalarName_I.push_back(ms + std::to_string(i));
    scalarName_I.push_back(qs + std::to_string(i));
    scalarVar_I.push_back(getMiSpecies(i));
    scalarVar_I.push_back(getQiSpecies(i));
  }
  scalarName_I.push_back("cLight");
  scalarVar_I.push_back(getcLightSI());
  scalarName_I.push_back("rPlanet");
  scalarVar_I.push_back(getrPlanet());
  //-------------------------------------

  Writer::set_doSaveBinary(getdoSaveBinary());
  for (Writer &wTmp : writer_I) {
    // Pass information to writers.
    wTmp.set_rank(myrank);
    wTmp.set_nProcs(get_nProcs());
    wTmp.set_nDim(getnDim());
    wTmp.set_iRegion(getiRegion());
    wTmp.set_domainMin_D({ { 0, 0, 0 } });
    wTmp.set_domainMax_D({ { getFluidLx(), getFluidLy(), getFluidLz() } });
    wTmp.set_dx_D({ { dx_D[x_], dx_D[y_], dx_D[z_] } });
    wTmp.set_axisOrigin_D(
        { { getFluidStartX(), getFluidStartY(), getFluidStartZ() } });
    wTmp.set_nSpecies(nS);
    wTmp.set_units(getNo2SiL(), getNo2SiV(), getNo2SiB(), getNo2SiRho(),
                   getNo2SiP(), getNo2SiJ(), getrPlanet());
    wTmp.set_scalarValue_I(scalarVar_I);
    wTmp.set_scalarName_I(scalarName_I);

    //--------------------------------------------------
    wTmp.init();
    //wTmp.print();
  }
}

void FluidPicInterface::writers_write(double timeNow, int iCycle,
                                      bool doForceOutput,
                                      FuncFindPointList find_output_list,
                                      FuncGetField get_var) {
  for (Writer &wTmp : writer_I) {
    wTmp.write(timeNow, iCycle, doForceOutput, find_output_list, get_var);
  }
}

//---------End of FluidPicInterface member functions defination--------------
