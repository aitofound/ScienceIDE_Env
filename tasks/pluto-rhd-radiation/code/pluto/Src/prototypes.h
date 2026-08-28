/* *********************************************************************
   PLUTO function prototypes
   ********************************************************************* */

int    AdvanceStep(Data *, timeStep *, Grid *);
void   AdvectFlux (const Sweep *, int, int, Grid *);
void   AMR_StoreFlux (double **, double **, int, int, int, int, int, Grid *);
void   Analysis (const Data *, Grid *);
char     *Array1D (int, size_t);
char    **Array2D (int, int, size_t);
char   ***Array3D (int, int, int, size_t);
char  ****Array4D (int, int, int, int, size_t);
char  ***ArrayBox(long int, long int, long int, long int, long int, long int, size_t);
double ***ArrayBoxMap (int, int, int, int, int, int, double *);
double ***ArrayMap (int, int, int, double *);
uint16_t ***ArrayUint16_tMap (int, int, int, uint16_t *);
void   ArrayReconstruct(double ***, uint16_t *, int, int, int, int,
                        double *, double *, int, Grid *);

void ShowMemoryInfo();
void FreeAll();

double BodyForcePotential(double, double, double);
void   BodyForceVector(double *, double *, double, double, double);
void   Boundary    (const Data *, int, Grid *);
void   BoundaryConservative (const Data *, Grid *);

void   ChangeOutputVar (void);
void   CharTracingStep(const Sweep *, int, int, Grid *);
int    CheckData (Data_Arr, Data_Arr, Data_Arr, int, RBox *, const char *);

void   CheckPrimStates (double **, double **, double **, int, int);
int    CheckNaN   (double **, int, int, const char *);

void   ComputeUserVar (const Data *, Grid *);
float  ***Convert_dbl2flt (double ***, double, int);
int    ConsToPrimLoc (double *, double *, uint16_t *);
int    ConsToPrim3D(Data_Arr, Data_Arr, uint16_t ***, RBox *, Grid *);
void   CreateImage (char *);
void   ComputeEntropy (const Data *, Grid *);

void   EntropySwitch(const Data *, Grid *);
#ifdef CHOMBO
void error (const char *fmt, ...);  /* Used to quit pluto only (flush buffer) */
#endif

void FailSafe (Data *, timeStep *, Grid *);

int   FileClose  (FILE *, int);
int   FileDelete (char *);
FILE  *FileOpen  (char *, int, char *);
void  FileReadData  (void *, size_t, int, FILE *, int, int);
void  FileWriteData (void *, size_t, int, FILE *, int);
void  FileWriteHeader(char *buffer, char fname[], int mode);
void  FileWriteArray(void *, long int, long int, size_t, char *);
void  FreeArray1D (void *);
void  FreeArray2D (void **);
void  FreeArray3D (void ***);
void  FreeArray4D (void ****);
void  FreeArrayBox(double ***, long, long, long);
void  FreeArrayBoxMap (double ***, int, int, int, int, int, int);
void  FreeArrayMap (double ***);
void  FreeArrayUint16_tMap(uint16_t ***);
#ifdef FINITE_DIFFERENCE 
 Riemann_Solver FD_Flux;
 void FD_GetMaxEigenvalues (const Data *d, Sweep *sweep, Grid *grid);
#endif
Reconstruct MP5_Reconstruct, PPM_Reconstruct, LIMO3_Reconstruct,
            WENOZ_Reconstruct, WENO3_Reconstruct;

void FindShock (const Data *, Grid *);
int  FlagCheck  (uint16_t ***, int);
void FlagShock (const Data *, Grid *);
void FlagShow (Data *, uint16_t);

void Flatten (const Sweep *, int, int, Grid *);
void FluidInterfaceBoundary(const Sweep *, int, int, Grid *);
void FreeGrid (Grid *);

void     GetAreaFlux (const Sweep *, double **, double **, int, int, Grid *);
void     GetCGSUnits (double *);
Image   *GetImage (char *);
double  *GetInverse_dl (const Grid *);
int      GetNghost (void);
void     GetNeighbourRanks (Grid *, int **);
void     GetOutputFrequency(Output *, const char *);
int      GetOutputVarNames(int, char *var_names[NVAR]);
double  ***GetUserVar (char *);
void   GnuplotSetting(Runtime *, Grid *);

void   HancockStep    (const Sweep *, int, int, Grid *);

int Integrate (Data *, timeStep *, Grid *);

double Length_1 (int, int, int, Grid *);
double Length_2 (int, int, int, Grid *);
double Length_3 (int, int, int, Grid *);
int    LocateIndex(double *, int, int, double);
void   LogFileClose(void);
void   LogFileFlush(void);
void   LogFileOpen (char *, char *);

char  *IndentString();
void   Init (double *, double, double, double);
void   InitDomain (Data *, Grid *);
void   Initialize(Data *, Runtime *, Grid *, cmdLine *);
void   InternalBoundaryReset (const Sweep *, timeStep *, int, int, Grid *);
void   InputDataClose(int);
void   InputDataGridSize (int, int *);
double InputDataInterpolate (int, double, double, double);
int    InputDataOpen(char *, char *, char *, long int, int);
void   InputDataReadSlice(int, int);
int    IsLittleEndian (void);

double MP5_States(double *, int, int);
double WENOZ_States(double *, int, int);
double WENO3_States(double *, int, int, double);

void   StaggeredRemap (Data_Arr, Grid *);
double StaggeredRemap_RKL(double ***, double ***, Data_Arr, RBox *, double, Grid *);
double StaggeredRemap_RHS(double ***, double ***, double ***, RBox *, Grid *);
void   StaggeredRemapBoundary(double ***phi, Data_Arr Bs, RBox *, Grid *grid);

void   MakeState (Sweep *);
double MeanMolecularWeight(double *);
double Median (double a, double b, double c);

void   OutflowBoundary(double ***, RBox *, int);
void   OutputLogPre  (Data *, timeStep *, Runtime *, Grid *);
void   OutputLogPost (Data *, timeStep *, Runtime *, Grid *);

void   ParabolicArrays(const Data *, int *, int, Grid *);
void   ParabolicFlux(Data_Arr, Data_Arr, double ***, const Sweep *,
                     double **, int, int, Grid *);
double ParabolicRHS   (const Data *, Data_Arr, RBox *, double **, int, double, Grid *);
void   ParabolicUpdate(const Data *, Data_Arr, RBox *, double **, double, timeStep *, Grid *);
void   ParseCmdLineArgs (int, char *argv[], char *, cmdLine *);
int    ParamFileRead    (char *);
char  *ParamFileGet     (const char *, int );
int    ParamExist       (const char *);
int    ParamFileHasBoth (const char *, const char *);
void   PeriodicBoundary (double ***, RBox *, int);

void   PolarAxisBoundary(const Data *, RBox *, int);

void   PrimToChar (double **, double *, double *); 
void   PrimToCons3D(Data_Arr, Data_Arr, RBox *, Grid *);
void   PrimToConsLoc (double *, double *);

void   PrintColumnLegend(char *legend[], int, FILE *);

void   RBoxCopy (RBox *, Data_Arr, Data_Arr, int, char);
void   RBoxDefine(int, int, int, int, int, int, int, RBox *);
void   RBoxEnlarge(RBox *, int, int, int);
void   RBoxSetDirections(RBox *, int);
void   RBoxShow(RBox *);
void   ReadHDF5 (Output *output, Grid *grid);
void   ReflectiveBoundary(double ***, int, int, RBox *, int);
void   ResetState (const Data *, Sweep *, Grid *);
void   RestartFromFile (const Data *, Runtime *, int, int, Grid *);
void   RestartDump     (Runtime *);
void   RestartGet      (Runtime *, int, int, int);

void   RightHandSide (const Sweep *, timeStep *, int, int, double, Grid *);
void   RightHandSideSource (const Sweep *, timeStep *, int, int, double,
                           double *, Grid *);

#if RING_AVERAGE > 1
void RingAverageCons(Data *, Grid *);
void RingAverageReconstruct (Sweep *, int, int, Grid *);
void RingAverageReconstructNew (double *v, int beg, int end, Grid *grid);

void RingAverageSize(Grid *);
#endif

void   RKC (const Data *d, double, timeStep *, Grid *);
void   RKL (const Data *d, double, timeStep *, Grid *);

Runtime *RuntimeGet(void);
int      RuntimeSetup  (Runtime *, cmdLine *, char *);
void     RuntimeSet(Runtime *runtime);

void   SetColorMap (unsigned char *, unsigned char *, unsigned char *, char *);
void   SetDefaultVarNames(Output *);
//void   SetGrid (Runtime *, Grid *);
void   SetGeometry (Grid *);
void   SetGrid (Runtime *, int *, Grid *);

void   SetJetDomain   (const Data *, int, int, Grid *);
void   SetOutput (Data *d, Runtime *input);
void   SetVectorIndices (int);
int    SetOutputVar (char *, int, int);
Riemann_Solver *SetSolver (const char *);
void   Show (double **, int);
void   ShowConfig(int, char *a[], char *);
void    ShowData (Data_Arr, Data_Arr, Data_Arr, RBox *, const char *);
void   ShowMatrix(double **, int, double);
void   ShowState (double *, int);
void   ShowVector (double *, int);
void   ShowUnits (void);
void   Sources (Data *, Data_Arr, double, Grid *);

void   SplitSource (Data *, double, timeStep *, Grid *);
void   Startup    (Data *, Grid *);
void   States     (const Sweep *, int, int, Grid *);
void   StateStructAllocate (State *);
void   StoreAMRFlux (double **, double **, int, int, int, int, int, Grid *);
int    StringArrayIndex (char *str_arr[], int, char *);
void   STS (const Data *d, double, timeStep *, Grid *);
void   SymmetryCheck (Data_Arr, int, RBox *);
void   SwapEndian (void *, const int); 


void UnsetJetDomain (const Data *, int, Grid *);
void UpdateStage(Data *, Data_Arr, Data_Arr, double **, double, timeStep *, Grid *);
void UserDefBoundary (const Data *, RBox *, int,  Grid *); 

const char *VarName (int nv, int mode);
void VectorPotentialDiff (double *, Data *, int, int, int, Grid *);

void  Where (int, Grid *);
void  WriteAsciiFile (char *, double *, int);
void  WriteData (const Data *, Output *, Grid *);
void  WriteHDF5        (Output *output, Grid *grid);
void  WriteVTK_Header (FILE *, Grid *);
void  WriteVTK_Vector (FILE *, Data_Arr, double, char *, Grid *);
void  WriteVTK_Scalar (FILE *, double ***, double, char *, Grid *);
void  WriteVTKProcFile (double ***, int, int, int, char *);
void  WriteTabArray (Output *, char *, Grid *);
void  WritePPM (double ***, char *, char *, Grid *);
void  WritePNG (double ***, char *, char *, Grid *);

#define ARRAY_1D(nx,type)          (type    *)Array1D(nx,sizeof(type))
#define ARRAY_2D(nx,ny,type)       (type   **)Array2D(nx,ny,sizeof(type))
#define ARRAY_3D(nx,ny,nz,type)    (type  ***)Array3D(nx,ny,nz,sizeof(type))
#define ARRAY_4D(nx,ny,nz,nv,type) (type ****)Array4D(nx,ny,nz,nv,sizeof(type))
#define ARRAY_BOX(i0,i1, j0,j1, k0,k1,type)  \
        (type ***)ArrayBox(i0,i1,j0,j1,k0,k1,sizeof(type))
/*
#define ARRAY_BOX(i0,i1, j0,j1, k0,k1,type)  \
         ArrayBox_Old(i0,i1,j0,j1,k0,k1)
*/
/* ---------------------------------------------------------------------
            Prototyping for standard output/debugging
   --------------------------------------------------------------------- */

void print (const char *fmt, ...);
void printLog  (const char *fmt, ...);
void Trace (double);

/* ----------------------------------------------
           functions in tools.c 
   ---------------------------------------------- */

void PlutoError (int, char *);

#if UPDATE_VECTOR_POTENTIAL == YES
 void VectorPotentialUpdate (const Data *d, const void *vp, 
                             const Sweep *sweep, const Grid *grid);
#endif

/* --------------- EOS ------------------------- */

void Enthalpy    (double **, double *, int, int);
void Entropy     (double **, double *, int, int);
void SoundSpeed2 (const State *, int, int, int, Grid *);

