c     CORRECTED VERSION OF 11/23/2021.
c     THE CORRECTION IS IN THE PXCP TERM BELOW: IT USES ZSM*DCPXZ.
c
C
C  RETURNS COMPONENTS OF THE EXTERNAL MAGNETIC FIELD VECTOR (I.E., DUE TO ONLY MAGNETOSPHERIC CURRENTS, 
C  WITHOUT CONTRIBUTION FROM THE EARTH'S SOURCES), ACCORDING TO THE DATA-BASED RBF-MODEL DRIVEN BY 
C  INTERPLANETARY AND GROUND-BASED OBSERVABLES.
C
C  VERSION OF 10/19/2016, BASED ON FITTING THE MODEL TO A DATA SET WITH 732 746 RECORDS
C
C  REFERENCES: (1) Tsyganenko, N. A., and V. A. Andreeva (2016), "An empirical RBF model of the magnetosphere
C                  parameterized by interplanetary and ground-based drivers", v.121, doi:10.1002/2016JA023217,
c                  accepted by JGRA, 10/17/2016.
C
C              (2) Andreeva, V. A., and N. A. Tsyganenko (2016), "Reconstructing the magnetosphere from data 
c                  using radial basis functions, JGRA Space Physics, v.121, 2249-2263, doi:10.1002/2015JA022242.
C
C  INPUT PARAMETERS:
C
C  IOPT       A DUMMY PARAMETER, INCLUDED TO MAKE THE SUBROUTINE COMPATIBLE WITH THE TRACING SOFTWARE 
C             PACKAGE (GEOPACK-08). IOPT DOES NOT AFFECT THE OUTPUT FIELD AND MUST BE SET AT ANY INTEGER VALUE
C
C  PARMOD     A 10-element array, with its first 4 elements to be specified as follows:
c
C  PARMOD(1) =  PDYN    - solar wind dynamic pressure in nPa
c
C  PARMOD(2) = <SymHc>  - corrected SymH index, to be calculated as <SymHc> = <0.8*SymH-13*sqrt(PDYN)>
C                         where the angular brackets denote the sliding 30-min average, centered on the
C                         current time moment
c
C  PARMOD(3) = <XIND>   - solar-wind-magnetosphere coupling index, based on Newell et al. [2007] function,
C                         averaged over the previous 30-min trailing interval, see also the documentation 
C                         file TA16_Model_description.pdf for further details. Typical values of <XIND> 
C                         lie between 0 (quiet) and 2 (strongly disturbed)
c
C  PARMOD(4) = <IMF BY> - azimuthal IMF By, in GSW (or GSM) coordinate system, in nanoTeslas, averaged over 
c                         the previous 30-min trailing interval
c
C  PS                   - geodipole tilt angle (in radians)
c
C  X, Y, Z              - Cartesian GSW (or GSM) position (in Earth's radii, Re=6371.2km)
C
C------------------------------------------------------------------------------------------------------
C  ATTENTION: THE MODEL IS VALID ONLY UP TO Xgsw=-15 Re and should NOT be used tailward of that distance
C------------------------------------------------------------------------------------------------------
c
C  OUTPUT:  BX, BY, BZ   - GSW (or GSM) components of the model field (nanoTesla)
C
C  CODED BY: N. A. TSYGANENKO AND V. A. ANDREEVA, version of Oct. 19, 2016
C
C------------------------------------------------------------------------------------------
      RECURSIVE SUBROUTINE RBF_MODEL_2016
     * (IOPT,PARMOD,PS,X,Y,Z,BX,BY,BZ)
C
C  THREAD-SAFE LEGACY ENTRY POINT WITH PHASE-SEPARATION CONTRACT
C
C  The original routine lazily filled SAVE arrays A, XX, YY, ZZ,
C  and related grid data inside the field call.  That created races
C  during initialization and made calls depend on mutable IOP state.
C
C  During evaluation this routine never modifies model data.  The fast
C  path is thread-safe only under a phase-separation contract.  Build
C  the cache once, in one thread, before worker threads are created.
C  Do not call TA16_INITIALIZE or TA16_SET_COEFF_FILE during field
C  evaluation.  Thread creation or corresponding OpenMP synchronization
C  must publish initialized data to workers.  TA16_READY is not atomic.
C
C  Without explicit initialization, this routine uses invocation-local
C  arrays through RBF_MODEL_2016_FILE.  That fallback is fully reentrant
C  if no thread concurrently changes the process-wide coefficient path.
C
      IMPLICIT REAL*8 (A-H,O-Z)
      DIMENSION PARMOD(10)
      CHARACTER*256 TA16_COEFF_FILE,COEFF_FILE_LOCAL
      COMMON /TA16_COEFF/ TA16_COEFF_FILE
      SAVE /TA16_COEFF/
      DIMENSION A_CACHE(23328)
      DIMENSION XX_CACHE(1296),YY_CACHE(1296),ZZ_CACHE(1296)
      DIMENSION ST_CACHE(1296),RHO_CACHE(1296)
      DIMENSION ZSP_CACHE(1296),ZCP_CACHE(1296),RHBR_CACHE(1296)
      COMMON /TA16_CACHE_D/ A_CACHE,XX_CACHE,YY_CACHE,ZZ_CACHE,
     * ST_CACHE,RHO_CACHE,ZSP_CACHE,ZCP_CACHE,RHBR_CACHE
      SAVE /TA16_CACHE_D/
      INTEGER TA16_READY
      COMMON /TA16_CACHE_I/ TA16_READY
      SAVE /TA16_CACHE_I/
C
      IF(TA16_READY.EQ.1) THEN
         CALL RBF_MODEL_2016_EVAL(IOPT,PARMOD,PS,X,Y,Z,BX,BY,BZ,
     *    A_CACHE,XX_CACHE,YY_CACHE,ZZ_CACHE,ST_CACHE,RHO_CACHE,
     *    ZSP_CACHE,ZCP_CACHE,RHBR_CACHE)
      ELSE
C
C  No shared cache exists.  Copy the configured path and use local
C  arrays.  Concurrent calls obtain independent NEWUNIT values.
         COEFF_FILE_LOCAL=TA16_COEFF_FILE
         CALL RBF_MODEL_2016_FILE(IOPT,PARMOD,PS,X,Y,Z,BX,BY,BZ,
     *    COEFF_FILE_LOCAL)
      ENDIF
      RETURN
      END
C
C============================================================================
      RECURSIVE SUBROUTINE RBF_MODEL_2016_FILE
     * (IOPT,PARMOD,PS,X,Y,Z,BX,BY,BZ,COEFF_FILE)
C
C  Fully reentrant entry point with an explicit coefficient-file
C  argument.  All coefficient and grid arrays are invocation-local.
C  Concurrent calls may use different coefficient files.
C============================================================================
      IMPLICIT REAL*8 (A-H,O-Z)
      CHARACTER*(*) COEFF_FILE
      DIMENSION PARMOD(10),A(23328)
      DIMENSION XX(1296),YY(1296),ZZ(1296),ST(1296)
      DIMENSION RHO(1296),ZSP(1296),ZCP(1296),RHBR(1296)
C
      CALL TA16_LOAD_DATA(COEFF_FILE,A,XX,YY,ZZ,ST,RHO,ZSP,ZCP,RHBR)
      CALL RBF_MODEL_2016_EVAL(IOPT,PARMOD,PS,X,Y,Z,BX,BY,BZ,
     * A,XX,YY,ZZ,ST,RHO,ZSP,ZCP,RHBR)
      RETURN
      END
C
C============================================================================
      RECURSIVE SUBROUTINE TA16_LOAD_DATA
     * (COEFF_FILE,A,XX,YY,ZZ,ST,RHO,ZSP,ZCP,RHBR)
C
C  Load coefficients and construct the RBF grid in arrays owned by the caller.
C  There is no SAVE state and no fixed shared I/O unit in this routine.
C============================================================================
      IMPLICIT REAL*8 (A-H,O-Z)
      CHARACTER*(*) COEFF_FILE
      DIMENSION A(23328),XX(1296),YY(1296),ZZ(1296),ST(1296)
      DIMENSION RHO(1296),ZSP(1296),ZCP(1296),RHBR(1296)
C
C  These DATA-initialized quantities are immutable model constants.  Static
C  read-only storage is safe for concurrent access.  In particular, unlike
C  the original code, A10, A11, A13, and A14 are never overwritten.
      DATA PI/3.14159265359D0/
      DATA A0,A1,A2,A3,A4,A5,A6,A7,A8,A9,A10,A11,A12,A13,A14,A15,A16,
     * A17,A18,A19,A20,A21/12.544D0,-0.194D0,0.305D0,0.0573D0,2.178D0,
     * 0.0571D0,-0.999D0,16.473D0,0.00152D0,0.382D0,0.0431D0,
     * -0.00763D0,-0.210D0,0.0405D0,-4.430D0,-0.636D0,-2.600D0,
     * 0.832D0,-5.328D0,1.103D0,-0.907D0,1.450D0/
C
C  Read coefficients into the supplied coefficient array.  NEWUNIT assigns a
C  distinct Fortran unit to each simultaneous call, avoiding the original
C  shared UNIT=777 race.  STATUS='OLD' and ACTION='READ' also document that
C  the coefficient file is never modified by the model.
      OPEN(NEWUNIT=IUNIT,FILE=COEFF_FILE,STATUS='OLD',
     * ACTION='READ')
      READ(IUNIT,100) A
 100  FORMAT(G15.6)
      CLOSE(IUNIT)
C
C----------------------------------------------------------------------------
C  CREATE THE DETERMINISTIC 3-D KURIHARA GRID OF RBF CENTERS.
C
C  This routine has no lazy cache.  The caller selects either local arrays or
C  explicitly initialized shared arrays that remain read-only during field
C  evaluation.
C----------------------------------------------------------------------------
      KLAT=8
      RLOW_GRID=3.3D0
      RHIGH_GRID=16.D0
      XGSW_LIM_DATA=-20.D0
C
      RH=8.D0
      ALPHA=3.D0
      NLAT=KLAT+1
      DLAT_DEG=90.D0/(NLAT-0.5D0)
C
      L=0
      R=RLOW_GRID
      PD_TR=0.5D0
      PM=0.D0
      BZIMF_TR=0.D0
      PSI=0.D0
C
C  The simplified Lin et al. boundary used by the original initialization
C  sets these four fitted terms to zero.  Use local effective values rather
C  than mutating the DATA-initialized constants A10, A11, A13, and A14.
      A10E=0.D0
      A11E=0.D0
      A13E=0.D0
      A14E=0.D0
C
      DO 911 J=1,100
      DO 912 I=1,NLAT
         XCOLATD=DLAT_DEG*(DFLOAT(I)-1.D0)
         NLON=4*(I-1)
         IF(I.NE.1) THEN
            DLON_DEG=360.D0/NLON
         ELSE
            NLON=1
            DLON_DEG=0.D0
         ENDIF
C
         DO 912 K=1,NLON
            XLOND=(K-1)*DLON_DEG
            XCOLAT=XCOLATD*0.01745329252D0
            XLON=XLOND*0.01745329252D0
C
            XXXX=R*DSIN(XCOLAT)*DCOS(XLON)
            YYYY=R*DSIN(XCOLAT)*DSIN(XLON)
            ZZZZ=R*DCOS(XCOLAT)
C
C  Calculate the distance from the RBF node to the simplified Lin et al.
C  [2010] magnetopause.  Nodes outside the boundary are not retained.
            EN=A21
            ES=A21
            THETAN=A19+A20*PSI
            THETAS=A19-A20*PSI
            CTN=DCOS(THETAN)
            CTS=DCOS(THETAS)
            STN=DSIN(THETAN)
            STS=DSIN(THETAS)
C
            RHO2=YYYY**2+ZZZZ**2
            R1=DSQRT(XXXX**2+RHO2)
            RHO1=DSQRT(RHO2)
C
            CTT=XXXX/R1
            STT=DSQRT(YYYY**2+ZZZZ**2)/R1
            T=DATAN2(STT,CTT)
            IF(RHO1.GT.1.D-5) THEN
               SP=ZZZZ/RHO1
               CP=YYYY/RHO1
            ELSE
               IF(XXXX.GT.0.D0) THEN
                  SP=0.D0
                  CP=1.D0
               ELSE
                  RM=1000.D0
                  RETURN
               ENDIF
            ENDIF
C
            PSIN=DACOS(CTT*CTN+STT*STN*SP)
            PSIS=DACOS(CTT*CTS-STT*STS*SP)
            DN=A16+(A17+A18*PSI)*PSI
            DS=A16-(A17-A18*PSI)*PSI
            CN=A14E*(PD_TR+PM)**A15
            CS=CN
C
            B0=A6+A7*(DEXP(A8*BZIMF_TR)-1.D0)/
     *       (DEXP(A9*BZIMF_TR)+1.D0)
            B1=A10E
            B2=A11E+A12*PSI
            B3=A13E
C
            F1=(DSQRT(0.5D0*(1.D0+CTT))+
     *       A5*2.D0*STT*CTT*(1.D0-DEXP(-T)))**
     *       (B0+B1*CP+B2*SP+B3*SP**2)
            R0=A0*(PD_TR+PM)**A1*
     *       (1.D0+A2*(DEXP(A3*BZIMF_TR)-1.D0)/
     *       (DEXP(A4*BZIMF_TR)+1.D0))
            RM=R0*F1+CN*DEXP(DN*PSIN**EN)+
     *       CS*DEXP(DS*PSIS**ES)
C
            IF(R.GT.RM) GOTO 912
            L=L+1
            XX(L)=XXXX
            YY(L)=YYYY
            ZZ(L)=ZZZZ
            ST(L)=DSIN(XCOLAT)
            RHO(L)=R*ST(L)
            ZSP(L)=ZZ(L)*DSIN(XLON)
            ZCP(L)=ZZ(L)*DCOS(XLON)
            RHBR(L)=RH/R*
     *       (1.D0-(1.D0+(R/RH)**ALPHA)**(1.D0/ALPHA))
 912     CONTINUE
C
         RLAST=R
         R=R*(NLAT-0.5D0+PI/4.D0)/
     *    (NLAT-0.5D0-PI/4.D0)
         IF(R.GT.RHIGH_GRID) GOTO 913
 911  CONTINUE
 913  CONTINUE
      RETURN
      END
C
C============================================================================
      RECURSIVE SUBROUTINE RBF_MODEL_2016_EVAL
     * (IOPT,PARMOD,PS,X,Y,Z,BX,BY,BZ,A,XX,YY,ZZ,ST,RHO,
     * ZSP,ZCP,RHBR)
C
C  Pure field-evaluation path with respect to shared model state: all arrays
C  are supplied by the caller and are read only.  RECURSIVE gives each call
C  independent scalar temporaries without -frecursive or -fautomatic.
C============================================================================
      IMPLICIT REAL*8 (A-H,O-Z)
      DIMENSION PARMOD(10),A(23328)
      DIMENSION XX(1296),YY(1296),ZZ(1296),ST(1296)
      DIMENSION RHO(1296),ZSP(1296),ZCP(1296),RHBR(1296)
      DATA D/4.D0/
C
C-------------------- START CALCULATING THE MODEL FIELD ---------------------
      XSM=X*DCOS(PS)-Z*DSIN(PS)
      YSM=Y
      ZSM=Z*DCOS(PS)+X*DSIN(PS)
C
      PDYN=PARMOD(1)
      SYMH=PARMOD(2)
      XIND=PARMOD(3)
      BYIMF=PARMOD(4)
C
      FPD=DSQRT(PDYN/2.D0)-1.D0
      SYMH=SYMH/50.D0
      CPS=DCOS(PS)
      SPS=DSIN(PS)
      TPS=SPS/CPS
C
      BXSM=0.D0
      BYSM=0.D0
      BZSM=0.D0
C
      DO 1 I=1,1296
         XP=XX(I)
         YP=YY(I)
         ZP=ZZ(I)
         XM=XP
         YM=YP
         ZM=-ZP
C
         DELTA_ZR=RHBR(I)*TPS
         DTHETA=-DASIN(DELTA_ZR)*ST(I)
         SDT=DSIN(DTHETA)
         CDTM1=DCOS(DTHETA)-1.D0
         DXP=XP*CDTM1+SDT*ZCP(I)
         DYP=YP*CDTM1+SDT*ZSP(I)
         DZP=ZP*CDTM1-RHO(I)*SDT
         DXM=XM*CDTM1-SDT*ZCP(I)
         DYM=YM*CDTM1-SDT*ZSP(I)
         DZM=ZM*CDTM1-RHO(I)*SDT
C
         CP=DSQRT((XSM-XP-DXP)**2+(YSM-YP-DYP)**2+
     *    (ZSM-ZP-DZP)**2+D**2)
         CM=DSQRT((XSM-XM-DXM)**2+(YSM-YM-DYM)**2+
     *    (ZSM-ZM-DZM)**2+D**2)
         DCPX=(XSM-XP-DXP)/CP
         DCMX=(XSM-XM-DXM)/CM
         DCPY=(YSM-YP-DYP)/CP
         DCMY=(YSM-YM-DYM)/CM
         DCPZ=(ZSM-ZP-DZP)/CP
         DCMZ=(ZSM-ZM-DZM)/CM
C
         DCPX2=1.D0/CP-DCPX**2/CP
         DCMX2=1.D0/CM-DCMX**2/CM
         DCPY2=1.D0/CP-DCPY**2/CP
         DCMY2=1.D0/CM-DCMY**2/CM
         DCPZ2=1.D0/CP-DCPZ**2/CP
         DCMZ2=1.D0/CM-DCMZ**2/CM
         DCPXY=-DCPX*DCPY/CP
         DCMXY=-DCMX*DCMY/CM
         DCPXZ=-DCPX*DCPZ/CP
         DCMXZ=-DCMX*DCMZ/CM
         DCPYZ=-DCPY*DCPZ/CP
         DCMYZ=-DCMY*DCMZ/CM
C
         TXCP=ZSM*DCPY-YSM*DCPZ
         TYCP=XSM*DCPZ-ZSM*DCPX
         TZCP=YSM*DCPX-XSM*DCPY
         TXCM=ZSM*DCMY-YSM*DCMZ
         TYCM=XSM*DCMZ-ZSM*DCMX
         TZCM=YSM*DCMX-XSM*DCMY
C
C  CORRECTION OF 11/23/2021 IS RETAINED: THE PXCP TERM USES ZSM.
         PXCP=2.D0*DCPX-XSM*(DCPY2+DCPZ2)+
     *    YSM*DCPXY+ZSM*DCPXZ
         PYCP=2.D0*DCPY-YSM*(DCPX2+DCPZ2)+
     *    ZSM*DCPYZ+XSM*DCPXY
         PZCP=2.D0*DCPZ-ZSM*(DCPX2+DCPY2)+
     *    XSM*DCPXZ+YSM*DCPYZ
         PXCM=2.D0*DCMX-XSM*(DCMY2+DCMZ2)+
     *    YSM*DCMXY+ZSM*DCMXZ
         PYCM=2.D0*DCMY-YSM*(DCMX2+DCMZ2)+
     *    ZSM*DCMYZ+XSM*DCMXY
         PZCM=2.D0*DCMZ-ZSM*(DCMX2+DCMY2)+
     *    XSM*DCMXZ+YSM*DCMYZ
C
         CTX=CPS*(TXCP+TXCM)
         CTY=CPS*(TYCP+TYCM)
         CTZ=CPS*(TZCP+TZCM)
         STX=SPS*(TXCP-TXCM)
         STY=SPS*(TYCP-TYCM)
         STZ=SPS*(TZCP-TZCM)
         CPX=CPS*(PXCP-PXCM)
         CPY=CPS*(PYCP-PYCM)
         CPZ=CPS*(PZCP-PZCM)
         SPX=SPS*(PXCP+PXCM)
         SPY=SPS*(PYCP+PYCM)
         SPZ=SPS*(PZCP+PZCM)
C
         ACT=A(I)+A(I+5184)*FPD+A(I+10368)*SYMH+
     *    A(I+15552)*XIND
         AST=A(I+1296)+A(I+6480)*FPD+A(I+11664)*SYMH+
     *    A(I+16848)*XIND
         AT=A(I+20736)*BYIMF
         ACP=A(I+2592)+A(I+7776)*FPD+A(I+12960)*SYMH+
     *    A(I+18144)*XIND
         ASP=A(I+3888)+A(I+9072)*FPD+A(I+14256)*SYMH+
     *    A(I+19440)*XIND
         AP=A(I+22032)*BYIMF
C
         BXSM=BXSM+CTX*ACT+STX*AST+(TXCP-TXCM)*AT+
     *    CPX*ACP+SPX*ASP+(PXCP+PXCM)*AP
         BYSM=BYSM+CTY*ACT+STY*AST+(TYCP-TYCM)*AT+
     *    CPY*ACP+SPY*ASP+(PYCP+PYCM)*AP
         BZSM=BZSM+CTZ*ACT+STZ*AST+(TZCP-TZCM)*AT+
     *    CPZ*ACP+SPZ*ASP+(PZCP+PZCM)*AP
  1   CONTINUE
C
C  Convert the magnetic field vector from SM back to GSM/GSW.
      BX=BXSM*CPS+BZSM*SPS
      BY=BYSM
      BZ=BZSM*CPS-BXSM*SPS
      RETURN
      END
C
C============================================================================
      RECURSIVE SUBROUTINE TA16_INITIALIZE
C
C  Performance-oriented explicit initialization.  Call exactly once,
C  from one thread, before creating workers.  Never call while a thread
C  is evaluating RBF_MODEL_2016.  TA16_READY is not atomic.  Visibility
C  relies on synchronization at thread creation or at parallel entry.
C  After publication, cache arrays are read only during evaluation.
C============================================================================
      IMPLICIT REAL*8 (A-H,O-Z)
      CHARACTER*256 TA16_COEFF_FILE
      COMMON /TA16_COEFF/ TA16_COEFF_FILE
      SAVE /TA16_COEFF/
      DIMENSION A_CACHE(23328)
      DIMENSION XX_CACHE(1296),YY_CACHE(1296),ZZ_CACHE(1296)
      DIMENSION ST_CACHE(1296),RHO_CACHE(1296)
      DIMENSION ZSP_CACHE(1296),ZCP_CACHE(1296),RHBR_CACHE(1296)
      COMMON /TA16_CACHE_D/ A_CACHE,XX_CACHE,YY_CACHE,ZZ_CACHE,
     * ST_CACHE,RHO_CACHE,ZSP_CACHE,ZCP_CACHE,RHBR_CACHE
      SAVE /TA16_CACHE_D/
      INTEGER TA16_READY
      COMMON /TA16_CACHE_I/ TA16_READY
      SAVE /TA16_CACHE_I/
C
      TA16_READY=0
      CALL TA16_LOAD_DATA(TA16_COEFF_FILE,A_CACHE,XX_CACHE,
     * YY_CACHE,ZZ_CACHE,ST_CACHE,RHO_CACHE,ZSP_CACHE,ZCP_CACHE,
     * RHBR_CACHE)
      TA16_READY=1
      RETURN
      END
C
C============================================================================
      RECURSIVE SUBROUTINE TA16_SET_COEFF_FILE(FNAME)
C
C  Set the process-wide coefficient path and initialize the cache.
C  Call before concurrent field evaluations, never while workers are
C  evaluating RBF_MODEL_2016.
C============================================================================
      IMPLICIT REAL*8 (A-H,O-Z)
      CHARACTER*(*) FNAME
      CHARACTER*256 TA16_COEFF_FILE
      COMMON /TA16_COEFF/ TA16_COEFF_FILE
      SAVE /TA16_COEFF/
      TA16_COEFF_FILE=FNAME
      CALL TA16_INITIALIZE
      RETURN
      END
C
C============================================================================
      BLOCK DATA TA16_DEFAULTS
C
C  Initialize configuration metadata only.  Coefficients and grid arrays are
C  loaded explicitly by TA16_INITIALIZE/TA16_SET_COEFF_FILE, or locally by the
C  safe fallback path in RBF_MODEL_2016.
C============================================================================
      CHARACTER*256 TA16_COEFF_FILE
      COMMON /TA16_COEFF/ TA16_COEFF_FILE
      INTEGER TA16_READY
      COMMON /TA16_CACHE_I/ TA16_READY
      DATA TA16_COEFF_FILE /'TA16_RBF.par'/
      DATA TA16_READY /0/
      END
