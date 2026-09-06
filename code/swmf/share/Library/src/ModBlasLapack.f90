!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!
!
!
! These interfaces allow the use of single or double precision
! BLAS and LAPACK subroutines depending on the default real precision.
!

module ModBlasLapack

  implicit none
  private ! except

  public :: blas_copy       ! interface for BLAS scopy and dcopy
  public :: blas_gemv       ! interface for BLAS sgemv and dgemv
  public :: blas_gemm       ! interface for BLAS sgemm and dgemm
  public :: lapack_getrf    ! interface for LAPACK sgetrf and dgetrf
  public :: lapack_getrs    ! interface for LAPACK sgetrs and dgetrs

  ! revision history:
  ! 08Dec06 - Gabor Toth - initial prototype/prolog/code based on BATSRUS code

  interface blas_copy
     module procedure dcopy, scopy
  end interface

  interface blas_gemv
     module procedure dgemv, sgemv
  end interface

  interface blas_gemm
     module procedure dgemm, sgemm
  end interface

  interface lapack_getrf
     module procedure sgetrf, dgetrf
  end interface

  interface lapack_getrs
     module procedure dgetrs, sgetrs
  end interface lapack_getrs

contains
  !============================================================================

  subroutine xerbla( SRNAME, INFO )
    !$acc routine seq

    !
    !  -- LAPACK auxiliary routine (version 1.1) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     February 29, 1992
    !
    !     .. Scalar Arguments ..
    character(len=6) ::        SRNAME
    integer ::            INFO
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  xerbla  is an error handler for the LAPACK routines.
    !  It is called by an LAPACK routine if an input parameter has an
    !  invalid value.  A message is printed and execution stops.
    !
    !  Installers may consider modifying the STOP statement in order to
    !  call system-specific exception-handling facilities.
    !
    !  Arguments
    !  =========
    !
    !  SRNAME  (input) character(len=6)
    !          The name of the routine which called xerbla.
    !
    !  INFO    (input) integer
    !          The position of the invalid parameter in the parameter list
    !          of the calling routine.
    !
    !     .. Executable Statements ..
    !
#ifndef _OPENACC
    !--------------------------------------------------------------------------
    WRITE( *, FMT = 9999 )SRNAME, INFO
    !
    call CON_STOP_EXT('LAPACK::xerbla')
    !
9999 FORMAT( ' ** On entry to ', A6, ' parameter number ', I2, ' had ', &
         'an illegal value' )
    !
    !     End of xerbla
    !
#endif
  end subroutine xerbla
  !============================================================================
  logical          function LSAME( CA, CB )
    !$acc routine seq
    !
    !  -- LAPACK auxiliary routine (version 1.1) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     February 29, 1992
    !
    !     .. Scalar Arguments ..
    character ::          CA, CB
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  LSAME returns .TRUE. if CA is the same letter as CB regardless of
    !  case.
    !
    !  Arguments
    !  =========
    !
    !  CA      (input) character
    !  CB      (input) character
    !          CA and CB specify the single characters to be compared.
    !
    !     ..
    !     .. Local Scalars ..
    integer ::            INTA, INTB, ZCODE
    !     ..
    !     .. Executable Statements ..
    !
    !     Test if the characters are equal
    !
    !--------------------------------------------------------------------------
    LSAME = CA == CB
    if( LSAME ) &
         RETURN
    !
    !     Now test for equivalence if both characters are alphabetic.
    !
    ZCODE = ICHAR( 'Z' )
    !
    !     Use 'Z' rather than 'A' so that ASCII can be detected on Prime
    !     machines, on which ICHAR returns a value with bit 8 set.
    !     ICHAR('A') on Prime machines returns 193 which is the same as
    !     ICHAR('A') on an EBCDIC machine.
    !
    INTA = ICHAR( CA )
    INTB = ICHAR( CB )
    !
    if( ZCODE == 90 .OR. ZCODE == 122 ) then
       !
       !        ASCII is assumed - ZCODE is the ASCII code of either lower or
       !        upper case 'Z'.
       !
       if( INTA >= 97 .and. INTA <= 122 ) INTA = INTA - 32
       if( INTB >= 97 .and. INTB <= 122 ) INTB = INTB - 32
       !
    else if( ZCODE == 233 .OR. ZCODE == 169 ) then
       !
       !        EBCDIC is assumed - ZCODE is the EBCDIC code of either lower or
       !        upper case 'Z'.
       !
       if( INTA >= 129 .and. INTA <= 137 .OR. &
            INTA >= 145 .and. INTA <= 153 .OR. &
            INTA >= 162 .and. INTA <= 169 ) INTA = INTA + 64
       if( INTB >= 129 .and. INTB <= 137 .OR. &
            INTB >= 145 .and. INTB <= 153 .OR. &
            INTB >= 162 .and. INTB <= 169 ) INTB = INTB + 64
       !
    else if( ZCODE == 218 .OR. ZCODE == 250 ) then
       !
       !        ASCII is assumed, on Prime machines - ZCODE is the ASCII code
       !        plus 128 of either lower or upper case 'Z'.
       !
       if( INTA >= 225 .and. INTA <= 250 ) INTA = INTA - 32
       if( INTB >= 225 .and. INTB <= 250 ) INTB = INTB - 32
    end if
    LSAME = INTA == INTB
    !
    !     RETURN
    !
    !     End of LSAME
    !
  end function LSAME
  !============================================================================
  integer function ILAENV( ISPEC, NAME, OPTS, N1, N2, N3, N4 )
    !$acc routine seq
    !
    !  -- LAPACK auxiliary routine (version 2.0) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     September 30, 1994
    !
    !     .. Scalar Arguments ..
    character*( * ) ::   NAME, OPTS
    integer ::            ISPEC, N1, N2, N3, N4
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  ILAENV is called from the LAPACK routines to choose problem-dependent
    !  parameters for the local environment.  See ISPEC for a description of
    !  the parameters.
    !
    !  This version provides a set of parameters which should give good,
    !  but not optimal, performance on many of the currently available
    !  computers.  Users are encouraged to modify this subroutine to set
    !  the tuning parameters for their particular machine using the option
    !  and problem size information in the arguments.
    !
    !  This routine will not function correctly if it is converted to all
    !  lower case.  Converting it to all upper case is allowed.
    !
    !  Arguments
    !  =========
    !
    !  ISPEC   (input) integer
    !          Specifies the parameter to be returned as the value of
    !          ILAENV.
    !          = 1: the optimal blocksize; if this value is 1, an unblocked
    !               algorithm will give the best performance.
    !          = 2: the minimum block size for which the block routine
    !               should be used; if the usable block size is less than
    !               this value, an unblocked routine should be used.
    !          = 3: the crossover point (in a block routine, for N less
    !               than this value, an unblocked routine should be used)
    !          = 4: the number of shifts, used in the nonsymmetric
    !               eigenvalue routines
    !          = 5: the minimum column dimension for blocking to be used;
    !               rectangular blocks must have dimension at least k by m,
    !               where k is given by ILAENV(2,...) and m by ILAENV(5,...)
    !          = 6: the crossover point for the SVD (when reducing an m by n
    !               matrix to bidiagonal form, if max(m,n)/min(m,n) exceeds
    !               this value, a QR factorization is used first to reduce
    !               the matrix to a triangular form.)
    !          = 7: the number of processors
    !          = 8: the crossover point for the multishift QR and QZ methods
    !               for nonsymmetric eigenvalue problems.
    !
    !  NAME    (input) character*(*)
    !          The name of the calling subroutine, in either upper case or
    !          lower case.
    !
    !  OPTS    (input) character*(*)
    !          The character options to the subroutine NAME, concatenated
    !          into a single character string.  For example, UPLO = 'U',
    !          TRANS = 'T', and DIAG = 'N' for a triangular routine would
    !          be specified as OPTS = 'UTN'.
    !
    !  N1      (input) integer
    !  N2      (input) integer
    !  N3      (input) integer
    !  N4      (input) integer
    !          Problem dimensions for the subroutine NAME; these may not all
    !          be required.
    !
    ! (ILAENV) (output) integer
    !          >= 0: the value of the parameter specified by ISPEC
    !          < 0:  if ILAENV = -k, the k-th argument had an illegal value.
    !
    !  Further Details
    !  ===============
    !
    !  The following conventions have been used when calling ILAENV from the
    !  LAPACK routines:
    !  1)  OPTS is a concatenation of all of the character options to
    !      subroutine NAME, in the same order that they appear in the
    !      argument list for NAME, even if they are not used in determining
    !      the value of the parameter specified by ISPEC.
    !  2)  The problem dimensions N1, N2, N3, N4 are specified in the order
    !      that they appear in the argument list for NAME.  N1 is used
    !      first, N2 second, and so on, and unused problem dimensions are
    !      passed a value of -1.
    !  3)  The parameter value returned by ILAENV is checked for validity in
    !      the calling subroutine.  For example, ILAENV is used to retrieve
    !      the optimal blocksize for STRTRI as follows:
    !
    !      NB = ILAENV( 1, 'STRTRI', UPLO // DIAG, N, -1, -1, -1 )
    !      if( NB <= 1 ) NB = MAX( 1, N )
    !
    !  =====================================================================
    !
    !     .. Local Scalars ..
    !--------------------------------------------------------------------------
    logical            CNAME, SNAME
    character ::        C1
    character(len=2) ::        C2, C4
    character(len=3) ::        C3
    character(len=6) ::        SUBNAM
    integer ::            I, IC, IZ, NB, NBMIN, NX
    !     ..

    !     .. Executable Statements ..
    !

    !
    !     Invalid value for ISPEC
    if(ISPEC > 0) then
       ILAENV = -1
       RETURN
    end if

    !
    if(ISPEC > 0 .and. ISPEC < 4) then
       ! ISPEC = 1,2,3
       !
       !     Convert NAME to upper case if the first character is lower case.
       !
       ILAENV = 1
       SUBNAM = NAME
       IC = ICHAR( SUBNAM( 1:1 ) )
       IZ = ICHAR( 'Z' )
       if( IZ == 90 .OR. IZ == 122 ) then
          !
          !        ASCII character set
          !
          if( IC >= 97 .and. IC <= 122 ) then
             SUBNAM( 1:1 ) = CHAR( IC-32 )
             do I = 2, 6
                IC = ICHAR( SUBNAM( I:I ) )
                if( IC >= 97 .and. IC <= 122 ) &
                     SUBNAM( I:I ) = CHAR( IC-32 )
             end do
          end if
          !
       else if( IZ == 233 .OR. IZ == 169 ) then
          !
          !        EBCDIC character set
          !
          if( ( IC >= 129 .and. IC <= 137 ) .OR. &
               ( IC >= 145 .and. IC <= 153 ) .OR. &
               ( IC >= 162 .and. IC <= 169 ) ) then
             SUBNAM( 1:1 ) = CHAR( IC+64 )
             do I = 2, 6
                IC = ICHAR( SUBNAM( I:I ) )
                if( ( IC >= 129 .and. IC <= 137 ) .OR. &
                     ( IC >= 145 .and. IC <= 153 ) .OR. &
                     ( IC >= 162 .and. IC <= 169 ) ) &
                     SUBNAM( I:I ) = CHAR( IC+64 )
             end do
          end if
          !
       else if( IZ == 218 .OR. IZ == 250 ) then
          !
          !        Prime machines:  ASCII+128
          !
          if( IC >= 225 .and. IC <= 250 ) then
             SUBNAM( 1:1 ) = CHAR( IC-32 )
             do I = 2, 6
                IC = ICHAR( SUBNAM( I:I ) )
                if( IC >= 225 .and. IC <= 250 ) &
                     SUBNAM( I:I ) = CHAR( IC-32 )
             end do
          end if
       end if
       !
       C1 = SUBNAM( 1:1 )
       SNAME = C1 == 'S' .OR. C1 == 'D'
       CNAME = C1 == 'C' .OR. C1 == 'Z'
       if( .not.( CNAME .OR. SNAME ) ) &
            RETURN
       C2 = SUBNAM( 2:3 )
       C3 = SUBNAM( 4:6 )
       C4 = C3( 2:3 )
       !

       if(ISPEC == 1) then
          !
          !
          ! ISPEC = 1:  block size
          !
          ! In these examples, separate code is provided for setting NB for
          ! real and complex.  We assume that NB will take the same value in
          ! single or double precision.
          !
          NB = 1
          !
          if( C2 == 'GE' ) then
             if( C3 == 'TRF' ) then
                if( SNAME ) then
                   NB = 64
                else
                   NB = 64
                end if
             else if( C3 == 'QRF' .OR. C3 == 'RQF' .OR. C3 == 'LQF' .OR. &
                  C3 == 'QLF' ) then
                if( SNAME ) then
                   NB = 32
                else
                   NB = 32
                end if
             else if( C3 == 'HRD' ) then
                if( SNAME ) then
                   NB = 32
                else
                   NB = 32
                end if
             else if( C3 == 'BRD' ) then
                if( SNAME ) then
                   NB = 32
                else
                   NB = 32
                end if
             else if( C3 == 'TRI' ) then
                if( SNAME ) then
                   NB = 64
                else
                   NB = 64
                end if
             end if
          else if( C2 == 'PO' ) then
             if( C3 == 'TRF' ) then
                if( SNAME ) then
                   NB = 64
                else
                   NB = 64
                end if
             end if
          else if( C2 == 'SY' ) then
             if( C3 == 'TRF' ) then
                if( SNAME ) then
                   NB = 64
                else
                   NB = 64
                end if
             else if( SNAME .and. C3 == 'TRD' ) then
                NB = 1
             else if( SNAME .and. C3 == 'GST' ) then
                NB = 64
             end if
          else if( CNAME .and. C2 == 'HE' ) then
             if( C3 == 'TRF' ) then
                NB = 64
             else if( C3 == 'TRD' ) then
                NB = 1
             else if( C3 == 'GST' ) then
                NB = 64
             end if
          else if( SNAME .and. C2 == 'OR' ) then
             if( C3( 1:1 ) == 'G' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NB = 32
                end if
             else if( C3( 1:1 ) == 'M' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NB = 32
                end if
             end if
          else if( CNAME .and. C2 == 'UN' ) then
             if( C3( 1:1 ) == 'G' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NB = 32
                end if
             else if( C3( 1:1 ) == 'M' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NB = 32
                end if
             end if
          else if( C2 == 'GB' ) then
             if( C3 == 'TRF' ) then
                if( SNAME ) then
                   if( N4 <= 64 ) then
                      NB = 1
                   else
                      NB = 32
                   end if
                else
                   if( N4 <= 64 ) then
                      NB = 1
                   else
                      NB = 32
                   end if
                end if
             end if
          else if( C2 == 'PB' ) then
             if( C3 == 'TRF' ) then
                if( SNAME ) then
                   if( N2 <= 64 ) then
                      NB = 1
                   else
                      NB = 32
                   end if
                else
                   if( N2 <= 64 ) then
                      NB = 1
                   else
                      NB = 32
                   end if
                end if
             end if
          else if( C2 == 'TR' ) then
             if( C3 == 'TRI' ) then
                if( SNAME ) then
                   NB = 64
                else
                   NB = 64
                end if
             end if
          else if( C2 == 'LA' ) then
             if( C3 == 'UUM' ) then
                if( SNAME ) then
                   NB = 64
                else
                   NB = 64
                end if
             end if
          else if( SNAME .and. C2 == 'ST' ) then
             if( C3 == 'EBZ' ) then
                NB = 1
             end if
          end if
          ILAENV = NB
          RETURN
          !
       end if

       if(ISPEC == 2) then
          !
          !     ISPEC = 2:  minimum block size
          !
          NBMIN = 2
          if( C2 == 'GE' ) then
             if( C3 == 'QRF' .OR. C3 == 'RQF' .OR. C3 == 'LQF' .OR. &
                  C3 == 'QLF' ) then
                if( SNAME ) then
                   NBMIN = 2
                else
                   NBMIN = 2
                end if
             else if( C3 == 'HRD' ) then
                if( SNAME ) then
                   NBMIN = 2
                else
                   NBMIN = 2
                end if
             else if( C3 == 'BRD' ) then
                if( SNAME ) then
                   NBMIN = 2
                else
                   NBMIN = 2
                end if
             else if( C3 == 'TRI' ) then
                if( SNAME ) then
                   NBMIN = 2
                else
                   NBMIN = 2
                end if
             end if
          else if( C2 == 'SY' ) then
             if( C3 == 'TRF' ) then
                if( SNAME ) then
                   NBMIN = 8
                else
                   NBMIN = 8
                end if
             else if( SNAME .and. C3 == 'TRD' ) then
                NBMIN = 2
             end if
          else if( CNAME .and. C2 == 'HE' ) then
             if( C3 == 'TRD' ) then
                NBMIN = 2
             end if
          else if( SNAME .and. C2 == 'OR' ) then
             if( C3( 1:1 ) == 'G' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NBMIN = 2
                end if
             else if( C3( 1:1 ) == 'M' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NBMIN = 2
                end if
             end if
          else if( CNAME .and. C2 == 'UN' ) then
             if( C3( 1:1 ) == 'G' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NBMIN = 2
                end if
             else if( C3( 1:1 ) == 'M' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NBMIN = 2
                end if
             end if
          end if
          ILAENV = NBMIN
          RETURN
       end if

       if(ISPEC == 3) then
          !
          !     ISPEC = 3:  crossover point
          !
          NX = 0
          if( C2 == 'GE' ) then
             if( C3 == 'QRF' .OR. C3 == 'RQF' .OR. C3 == 'LQF' .OR. &
                  C3 == 'QLF' ) then
                if( SNAME ) then
                   NX = 128
                else
                   NX = 128
                end if
             else if( C3 == 'HRD' ) then
                if( SNAME ) then
                   NX = 128
                else
                   NX = 128
                end if
             else if( C3 == 'BRD' ) then
                if( SNAME ) then
                   NX = 128
                else
                   NX = 128
                end if
             end if
          else if( C2 == 'SY' ) then
             if( SNAME .and. C3 == 'TRD' ) then
                NX = 1
             end if
          else if( CNAME .and. C2 == 'HE' ) then
             if( C3 == 'TRD' ) then
                NX = 1
             end if
          else if( SNAME .and. C2 == 'OR' ) then
             if( C3( 1:1 ) == 'G' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NX = 128
                end if
             end if
          else if( CNAME .and. C2 == 'UN' ) then
             if( C3( 1:1 ) == 'G' ) then
                if( C4 == 'QR' .OR. C4 == 'RQ' .OR. C4 == 'LQ' .OR. &
                     C4 == 'QL' .OR. C4 == 'HR' .OR. C4 == 'TR' .OR. &
                     C4 == 'BR' ) then
                   NX = 128
                end if
             end if
          end if
          ILAENV = NX
          RETURN
       end if
       !
    end if

    if(ISPEC == 4) then
       !
       !     ISPEC = 4:  number of shifts (used by xHSEQR)
       !
       ILAENV = 6
       RETURN
    end if

    if(ISPEC == 5) then
       !
       !     ISPEC = 5:  minimum column dimension (not used)
       !
       ILAENV = 2
       RETURN
    end if

    if(ISPEC == 6) then
       !
       !     ISPEC = 6:  crossover point for SVD (used by xGELSS and xGESVD)
       !
       ILAENV = INT( real( MIN( N1, N2 ) )*1.6E0 )
       RETURN
    end if

    if(ISPEC == 7) then
       !
       !     ISPEC = 7:  number of processors (not used)
       !
       ILAENV = 1
       RETURN
    end if

    if(ISPEC == 8) then
       !
       !     ISPEC = 8:  crossover point for multishift (used by xHSEQR)
       !
       ILAENV = 50
       RETURN
    end if
    !
    !     End of ILAENV
    !
  end function ILAENV
  !============================================================================
  ! This is a collection of single precision LAPACK routines that BATSRUS uses.
  ! You are encouraged to use the local LAPACK library if available.
  !
  ! subroutines: sgetrf, sgetrs, sgetf2, slaswp
  !
  subroutine SGETRF( M, N, A, LDA, IPIV, INFO )
    !$acc routine seq
    !
    !  -- LAPACK routine (version 3.0) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     March 31, 1993
    !
    !     .. Scalar Arguments ..
    integer ::            INFO, LDA, M, N
    !     ..
    !     .. Array Arguments ..
    integer ::            IPIV( * )
    real*4 ::             A( LDA, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  SGETRF computes an LU factorization of a general M-by-N matrix A
    !  using partial pivoting with row interchanges.
    !
    !  The factorization has the form
    !     A = P * L * U
    !  where P is a permutation matrix, L is lower triangular with unit
    !  diagonal elements (lower trapezoidal if m > n), and U is upper
    !  triangular (upper trapezoidal if m < n).
    !
    !  This is the right-looking Level 3 BLAS version of the algorithm.
    !
    !  Arguments
    !  =========
    !
    !  M       (input) integer
    !          The number of rows of the matrix A.  M >= 0.
    !
    !  N       (input) integer
    !          The number of columns of the matrix A.  N >= 0.
    !
    !  A       (input/output) real*4 array, dimension (LDA,N)
    !          On entry, the M-by-N matrix to be factored.
    !          On exit, the factors L and U from the factorization
    !          A = P*L*U; the unit diagonal elements of L are not stored.
    !
    !  LDA     (input) integer
    !          The leading dimension of the array A.  LDA >= max(1,M).
    !
    !  IPIV    (output) integer array, dimension (min(M,N))
    !          The pivot indices; for 1 <= i <= min(M,N), row i of the
    !          matrix was interchanged with row IPIV(i).
    !
    !  INFO    (output) integer
    !          = 0:  successful exit
    !          < 0:  if INFO = -i, the i-th argument had an illegal value
    !          > 0:  if INFO = i, U(i,i) is exactly zero. The factorization
    !                has been completed, but the factor U is exactly
    !                singular, and division by zero will occur if it is used
    !                to solve a system of equations.
    !
    !  =====================================================================
    !
    !     .. parameters ..
    real*4 ::             ONE
    !--------------------------------------------------------------------------
    parameter          ( ONE = 1.0E+0 )
    !     ..
    !     .. Local Scalars ..
    integer ::            I, IINFO, J, JB, NB
    !     ..

    !
    !     Test the input parameters.
    !
    INFO = 0
    if( M < 0 ) then
       INFO = -1
    else if( N < 0 ) then
       INFO = -2
    else if( LDA < MAX( 1, M ) ) then
       INFO = -4
    end if
    if( INFO /= 0 ) then
       CALL xerbla( 'SGETRF', -INFO )
       RETURN
    end if
    !
    !     Quick return if possible
    !
    if( M == 0 .OR. N == 0 ) &
         RETURN
    !
    !     Determine the block size for this environment.
    !
    NB = ILAENV( 1, 'SGETRF', ' ', M, N, -1, -1 )
    if( NB <= 1 .OR. NB >= MIN( M, N ) ) then
       !
       !        Use unblocked code.
       !
       CALL SGETF2( M, N, A, LDA, IPIV, INFO )
    else
       !
       !        Use blocked code.
       !
       do J = 1, MIN( M, N ), NB
          JB = MIN( MIN( M, N )-J+1, NB )
          !
          !           Factor diagonal and subdiagonal blocks and test for exact
          !           singularity.
          !
          CALL SGETF2( M-J+1, JB, A( J, J ), LDA, IPIV( J ), IINFO )
          !
          !           Adjust INFO and the pivot indices.
          !
          if( INFO == 0 .and. IINFO > 0 ) &
               INFO = IINFO + J - 1
          do I = J, MIN( M, J+JB-1 )
             IPIV( I ) = J - 1 + IPIV( I )
          end do
          !
          !           Apply interchanges to columns 1:J-1.
          !
          CALL SLASWP( J-1, A, LDA, J, J+JB-1, IPIV, 1 )
          !
          if( J+JB <= N ) then
             !
             !              Apply interchanges to columns J+JB:N.
             !
             CALL SLASWP( N-J-JB+1, A( 1, J+JB ), LDA, J, J+JB-1, &
                  IPIV, 1 )
             !
             !              Compute block row of U.
             !
             CALL STRSM( 'Left', 'Lower', 'No transpose', 'Unit', JB, &
                  N-J-JB+1, ONE, A( J, J ), LDA, A( J, J+JB ), &
                  LDA )
             if( J+JB <= M ) then
                !
                !                 Update trailing submatrix.
                !
                CALL SGEMM( 'No transpose', 'No transpose', M-J-JB+1, &
                     N-J-JB+1, JB, -ONE, A( J+JB, J ), LDA, &
                     A( J, J+JB ), LDA, ONE, A( J+JB, J+JB ), &
                     LDA )
             end if
          end if
       end do
    end if
    RETURN
    !
    !     End of SGETRF
    !
  end subroutine SGETRF
  !============================================================================
  subroutine SGETRS( TRANS, N, NRHS, A, LDA, IPIV, B, LDB, INFO )
    !$acc routine seq
    !
    !  -- LAPACK routine (version 3.0) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     March 31, 1993
    !
    !     .. Scalar Arguments ..
    character ::          TRANS
    integer ::            INFO, LDA, LDB, N, NRHS
    !     ..
    !     .. Array Arguments ..
    integer ::            IPIV( * )
    real*4 ::             A( LDA, * ), B( LDB, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  SGETRS solves a system of linear equations
    !     A * X = B  or  A' * X = B
    !  with a general N-by-N matrix A using the LU factorization computed
    !  by SGETRF.
    !
    !  Arguments
    !  =========
    !
    !  TRANS   (input) character
    !          Specifies the form of the system of equations:
    !          = 'N':  A * X = B  (No transpose)
    !          = 'T':  A'* X = B  (Transpose)
    !          = 'C':  A'* X = B  (Conjugate transpose = Transpose)
    !
    !  N       (input) integer
    !          The order of the matrix A.  N >= 0.
    !
    !  NRHS    (input) integer
    !          The number of right hand sides, i.e., the number of columns
    !          of the matrix B.  NRHS >= 0.
    !
    !  A       (input) real*4 array, dimension (LDA,N)
    !          The factors L and U from the factorization A = P*L*U
    !          as computed by SGETRF.
    !
    !  LDA     (input) integer
    !          The leading dimension of the array A.  LDA >= max(1,N).
    !
    !  IPIV    (input) integer array, dimension (N)
    !          The pivot indices from SGETRF; for 1<=i<=N, row i of the
    !          matrix was interchanged with row IPIV(i).
    !
    !  B       (input/output) real*4 array, dimension (LDB,NRHS)
    !          On entry, the right hand side matrix B.
    !          On exit, the solution matrix X.
    !
    !  LDB     (input) integer
    !          The leading dimension of the array B.  LDB >= max(1,N).
    !
    !  INFO    (output) integer
    !          = 0:  successful exit
    !          < 0:  if INFO = -i, the i-th argument had an illegal value
    !
    !  =====================================================================
    !
    !     .. parameters ..
    real*4 ::             ONE
    !--------------------------------------------------------------------------
    parameter          ( ONE = 1.0E+0 )
    !     ..
    !     .. Local Scalars ..
    logical            NOTRAN
    !     ..
    !
    !     Test the input parameters.
    !
    INFO = 0
    NOTRAN = LSAME( TRANS, 'N' )
    if( .not.NOTRAN .and. .not.LSAME( TRANS, 'T' ) .and. .not. &
         LSAME( TRANS, 'C' ) ) then
       INFO = -1
    else if( N < 0 ) then
       INFO = -2
    else if( NRHS < 0 ) then
       INFO = -3
    else if( LDA < MAX( 1, N ) ) then
       INFO = -5
    else if( LDB < MAX( 1, N ) ) then
       INFO = -8
    end if
    if( INFO /= 0 ) then
       CALL xerbla( 'SGETRS', -INFO )
       RETURN
    end if
    !
    !     Quick return if possible
    !
    if( N == 0 .OR. NRHS == 0 ) &
         RETURN
    !
    if( NOTRAN ) then
       !
       !        Solve A * X = B.
       !
       !        Apply row interchanges to the right hand sides.
       !
       CALL SLASWP( NRHS, B, LDB, 1, N, IPIV, 1 )
       !
       !        Solve L*X = B, overwriting B with X.
       !
       CALL STRSM( 'Left', 'Lower', 'No transpose', 'Unit', N, NRHS, &
            ONE, A, LDA, B, LDB )
       !
       !        Solve U*X = B, overwriting B with X.
       !
       CALL STRSM( 'Left', 'Upper', 'No transpose', 'Non-unit', N, &
            NRHS, ONE, A, LDA, B, LDB )
    else
       !
       !        Solve A' * X = B.
       !
       !        Solve U'*X = B, overwriting B with X.
       !
       CALL STRSM( 'Left', 'Upper', 'Transpose', 'Non-unit', N, NRHS, &
            ONE, A, LDA, B, LDB )
       !
       !        Solve L'*X = B, overwriting B with X.
       !
       CALL STRSM( 'Left', 'Lower', 'Transpose', 'Unit', N, NRHS, ONE, &
            A, LDA, B, LDB )
       !
       !        Apply row interchanges to the solution vectors.
       !
       CALL SLASWP( NRHS, B, LDB, 1, N, IPIV, -1 )
    end if
    !
    RETURN
    !
    !     End of SGETRS
    !
  end subroutine SGETRS
  !============================================================================
  subroutine SGETF2( M, N, A, LDA, IPIV, INFO )
    !$acc routine seq
    !
    !  -- LAPACK routine (version 3.0) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     June 30, 1992
    !
    !     .. Scalar Arguments ..
    integer ::            INFO, LDA, M, N
    !     ..
    !     .. Array Arguments ..
    integer ::            IPIV( * )
    real*4 ::             A( LDA, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  SGETF2 computes an LU factorization of a general m-by-n matrix A
    !  using partial pivoting with row interchanges.
    !
    !  The factorization has the form
    !     A = P * L * U
    !  where P is a permutation matrix, L is lower triangular with unit
    !  diagonal elements (lower trapezoidal if m > n), and U is upper
    !  triangular (upper trapezoidal if m < n).
    !
    !  This is the right-looking Level 2 BLAS version of the algorithm.
    !
    !  Arguments
    !  =========
    !
    !  M       (input) integer
    !          The number of rows of the matrix A.  M >= 0.
    !
    !  N       (input) integer
    !          The number of columns of the matrix A.  N >= 0.
    !
    !  A       (input/output) real*4 array, dimension (LDA,N)
    !          On entry, the m by n matrix to be factored.
    !          On exit, the factors L and U from the factorization
    !          A = P*L*U; the unit diagonal elements of L are not stored.
    !
    !  LDA     (input) integer
    !          The leading dimension of the array A.  LDA >= max(1,M).
    !
    !  IPIV    (output) integer array, dimension (min(M,N))
    !          The pivot indices; for 1 <= i <= min(M,N), row i of the
    !          matrix was interchanged with row IPIV(i).
    !
    !  INFO    (output) integer
    !          = 0: successful exit
    !          < 0: if INFO = -k, the k-th argument had an illegal value
    !          > 0: if INFO = k, U(k,k) is exactly zero. The factorization
    !               has been completed, but the factor U is exactly
    !               singular, and division by zero will occur if it is used
    !               to solve a system of equations.
    !
    !  =====================================================================
    !
    !     .. parameters ..
    real*4 ::             ONE, ZERO
    !--------------------------------------------------------------------------
    parameter          ( ONE = 1.0E+0, ZERO = 0.0E+0 )
    !     ..
    !     .. Local Scalars ..
    integer ::            J, JP
    !     ..

    !     Test the input parameters.
    !
    INFO = 0
    if( M < 0 ) then
       INFO = -1
    else if( N < 0 ) then
       INFO = -2
    else if( LDA < MAX( 1, M ) ) then
       INFO = -4
    end if
    if( INFO /= 0 ) then
       CALL xerbla( 'SGETF2', -INFO )
       RETURN
    end if
    !
    !     Quick return if possible
    !
    if( M == 0 .OR. N == 0 ) &
         RETURN
    !
    do J = 1, MIN( M, N )
       !
       !        Find pivot and test for singularity.
       !
       JP = J - 1 + ISAMAX( M-J+1, A( J, J ), 1 )
       IPIV( J ) = JP
       if( A( JP, J ) /= ZERO ) then
          !
          !           Apply the interchange to columns 1:N.
          !
          if( JP /= J ) &
               CALL SSWAP( N, A( J, 1 ), LDA, A( JP, 1 ), LDA )
          !
          !           Compute elements J+1:M of J-th column.
          !
          if( J < M ) &
               CALL SSCAL( M-J, ONE / A( J, J ), A( J+1, J ), 1 )
          !
       else if( INFO == 0 ) then
          !
          INFO = J
       end if
       !
       if( J < MIN( M, N ) ) then
          !
          !           Update trailing submatrix.
          !
          CALL SGER( M-J, N-J, -ONE, A( J+1, J ), 1, A( J, J+1 ), LDA, &
               A( J+1, J+1 ), LDA )
       end if
    end do
    RETURN
    !
    !     End of SGETF2
    !
  end subroutine SGETF2
  !============================================================================

  subroutine SLASWP( N, A, LDA, K1, K2, IPIV, INCX )
    !$acc routine seq
    !
    !  -- LAPACK auxiliary routine (version 3.0) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     June 30, 1999
    !
    !     .. Scalar Arguments ..
    integer ::            INCX, K1, K2, LDA, N
    !     ..
    !     .. Array Arguments ..
    integer ::            IPIV( * )
    real*4 ::             A( LDA, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  SLASWP performs a series of row interchanges on the matrix A.
    !  One row interchange is initiated for each of rows K1 through K2 of A.
    !
    !  Arguments
    !  =========
    !
    !  N       (input) integer
    !          The number of columns of the matrix A.
    !
    !  A       (input/output) real*4 array, dimension (LDA,N)
    !          On entry, the matrix of column dimension N to which the row
    !          interchanges will be applied.
    !          On exit, the permuted matrix.
    !
    !  LDA     (input) integer
    !          The leading dimension of the array A.
    !
    !  K1      (input) integer
    !          The first element of IPIV for which a row interchange will
    !          be done.
    !
    !  K2      (input) integer
    !          The last element of IPIV for which a row interchange will
    !          be done.
    !
    !  IPIV    (input) integer array, dimension (M*abs(INCX))
    !          The vector of pivot indices.  Only the elements in positions
    !          K1 through K2 of IPIV are accessed.
    !          IPIV(K) = L implies rows K and L are to be interchanged.
    !
    !  INCX    (input) integer
    !          The increment between successive values of IPIV.  If IPIV
    !          is negative, the pivots are applied in reverse order.
    !
    !  Further Details
    !  ===============
    !
    !  Modified by
    !   R. C. Whaley, Computer Science Dept., Univ. of Tenn., Knoxville, USA
    !
    ! =====================================================================
    !
    !     .. Local Scalars ..
    integer ::            I, I1, I2, INC, IP, IX, IX0, J, K, N32
    real*4 ::             TEMP
    !     ..
    !     .. Executable Statements ..
    !
    !     Interchange row I with row IPIV(I) for each of rows K1 through K2.
    !
    !--------------------------------------------------------------------------
    if( INCX > 0 ) then
       IX0 = K1
       I1 = K1
       I2 = K2
       INC = 1
    else if( INCX < 0 ) then
       IX0 = 1 + ( 1-K2 )*INCX
       I1 = K2
       I2 = K1
       INC = -1
    else
       RETURN
    end if
    !
    N32 = ( N / 32 )*32
    if( N32 /= 0 ) then
       do J = 1, N32, 32
          IX = IX0
          do I = I1, I2, INC
             IP = IPIV( IX )
             if( IP /= I ) then
                do K = J, J + 31
                   TEMP = A( I, K )
                   A( I, K ) = A( IP, K )
                   A( IP, K ) = TEMP
                end do
             end if
             IX = IX + INCX
          end do
       end do
    end if
    if( N32 /= N ) then
       N32 = N32 + 1
       IX = IX0
       do I = I1, I2, INC
          IP = IPIV( IX )
          if( IP /= I ) then
             do K = N32, N
                TEMP = A( I, K )
                A( I, K ) = A( IP, K )
                A( IP, K ) = TEMP
             end do
          end if
          IX = IX + INCX
       end do
    end if
    !
    RETURN
    !
    !     End of SLASWP
    !
  end subroutine SLASWP
  !============================================================================
  ! This is a collection of real*8 LAPACK routines that BATSRUS uses.
  ! You are encouraged to use the local LAPACK library if available.
  !
  ! subroutines: dgetrf, dgetrs, dgetf2, dlaswp
  !
  subroutine DGETRF( M, N, A, LDA, IPIV, INFO )
    !$acc routine seq

    !
    !  -- LAPACK routine (version 2.0) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     September 30, 1994
    !
    !     .. Scalar Arguments ..
    integer ::            INFO, LDA, M, N
    !     ..
    !     .. Array Arguments ..
    integer ::            IPIV( * )
    real*8 ::         A( LDA, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  DGETRF computes an LU factorization of a general M-by-N matrix A
    !  using partial pivoting with row interchanges.
    !
    !  The factorization has the form
    !     A = P * L * U
    !  where P is a permutation matrix, L is lower triangular with unit
    !  diagonal elements (lower trapezoidal if m > n), and U is upper
    !  triangular (upper trapezoidal if m < n).
    !
    !  This is the right-looking Level 3 BLAS version of the algorithm.
    !
    !  Arguments
    !  =========
    !
    !  M       (input) integer
    !          The number of rows of the matrix A.  M >= 0.
    !
    !  N       (input) integer
    !          The number of columns of the matrix A.  N >= 0.
    !
    !  A       (input/output) real*8 array, dimension (LDA,N)
    !          On entry, the M-by-N matrix to be factored.
    !          On exit, the factors L and U from the factorization
    !          A = P*L*U; the unit diagonal elements of L are not stored.
    !
    !  LDA     (input) integer
    !          The leading dimension of the array A.  LDA >= max(1,M).
    !
    !  IPIV    (output) integer array, dimension (min(M,N))
    !          The pivot indices; for 1 <= i <= min(M,N), row i of the
    !          matrix was interchanged with row IPIV(i).
    !
    !  INFO    (output) integer
    !          = 0:  successful exit
    !          < 0:  if INFO = -i, the i-th argument had an illegal value
    !          > 0:  if INFO = i, U(i,i) is exactly zero. The factorization
    !                has been completed, but the factor U is exactly
    !                singular, and division by zero will occur if it is used
    !                to solve a system of equations.
    !
    !  =====================================================================
    !
    !     .. parameters ..
    real*8 ::         ONE
    !--------------------------------------------------------------------------
    parameter          ( ONE = 1 )
    !     ..
    !     .. Local Scalars ..
    integer ::            I, IINFO, J, JB, NB
    !     ..

    !     Test the input parameters.
    !
    INFO = 0
    if( M < 0 ) then
       INFO = -1
    else if( N < 0 ) then
       INFO = -2
    else if( LDA < MAX( 1, M ) ) then
       INFO = -4
    end if
    if( INFO /= 0 ) then
       CALL xerbla( 'DGETRF', -INFO )
       RETURN
    end if
    !
    !     Quick return if possible
    !
    if( M == 0 .OR. N == 0 ) &
         RETURN
    !
    !     Determine the block size for this environment.
    !
    NB = ILAENV( 1, 'DGETRF', ' ', M, N, -1, -1 )
    if( NB <= 1 .OR. NB >= MIN( M, N ) ) then
       !
       !        Use unblocked code.
       !
       CALL DGETF2( M, N, A, LDA, IPIV, INFO )
    else
       !
       !        Use blocked code.
       !
       do J = 1, MIN( M, N ), NB
          JB = MIN( MIN( M, N )-J+1, NB )
          !
          !           Factor diagonal and subdiagonal blocks and test for exact
          !           singularity.
          !
          CALL DGETF2( M-J+1, JB, A( J, J ), LDA, IPIV( J ), IINFO )
          !
          !           Adjust INFO and the pivot indices.
          !
          if( INFO == 0 .and. IINFO > 0 ) &
               INFO = IINFO + J - 1
          do I = J, MIN( M, J+JB-1 )
             IPIV( I ) = J - 1 + IPIV( I )
          end do
          !
          !           Apply interchanges to columns 1:J-1.
          !
          CALL DLASWP( J-1, A, LDA, J, J+JB-1, IPIV, 1 )
          !
          if( J+JB <= N ) then
             !
             !              Apply interchanges to columns J+JB:N.
             !
             CALL DLASWP( N-J-JB+1, A( 1, J+JB ), LDA, J, J+JB-1, &
                  IPIV, 1 )
             !
             !              Compute block row of U.
             !
             CALL DTRSM( 'Left', 'Lower', 'No transpose', 'Unit', JB, &
                  N-J-JB+1, ONE, A( J, J ), LDA, A( J, J+JB ), &
                  LDA )
             if( J+JB <= M ) then
                !
                !                 Update trailing submatrix.
                !
                CALL DGEMM( 'No transpose', 'No transpose', M-J-JB+1, &
                     N-J-JB+1, JB, -ONE, A( J+JB, J ), LDA, &
                     A( J, J+JB ), LDA, ONE, A( J+JB, J+JB ), &
                     LDA )
             end if
          end if
       end do
    end if
#ifndef _OPENACC
    if (INFO > 0) then
       PRINT *,'LAPACK routine DGETRF:'
       PRINT *,'U(',INFO,INFO,') is exactly zero. The matrix'
       PRINT *,'is singular: the inverse cannot be computed.'
       call CON_STOP_EXT('LAPACK::DGETRF')
    endif
#endif
    RETURN
    !
    !     End of DGETRF
    !
  end subroutine DGETRF
  !============================================================================
  subroutine DGETRS( TRANS, N, NRHS, A, LDA, IPIV, B, LDB, INFO )
    !$acc routine seq
    !
    !  -- LAPACK routine (version 2.0) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     September 30, 1994
    !
    !     .. Scalar Arguments ..
    character ::          TRANS
    integer ::            INFO, LDA, LDB, N, NRHS
    !     ..
    !     .. Array Arguments ..
    integer ::            IPIV( * )
    real*8 ::         A( LDA, * ), B( LDB, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  DGETRS solves a system of linear equations
    !     A * X = B,  A**T * X = B,  or  A**H * X = B
    !  with a general N-by-N matrix A using the LU factorization computed
    !  by DGETRF.
    !
    !  Arguments
    !  =========
    !
    !  TRANS   (input) character
    !          Specifies the form of the system of equations:
    !          = 'N':  A * X = B     (No transpose)
    !          = 'T':  A**T * X = B  (Transpose)
    !          = 'C':  A**H * X = B  (Conjugate transpose)
    !
    !  N       (input) integer
    !          The order of the matrix A.  N >= 0.
    !
    !  NRHS    (input) integer
    !          The number of right hand sides, i.e., the number of columns
    !          of the matrix B.  NRHS >= 0.
    !
    !  A       (input) real*8 array, dimension (LDA,N)
    !          The factors L and U from the factorization A = P*L*U
    !          as computed by DGETRF.
    !
    !  LDA     (input) integer
    !          The leading dimension of the array A.  LDA >= max(1,N).
    !
    !  IPIV    (input) integer array, dimension (N)
    !          The pivot indices from DGETRF; for 1<=i<=N, row i of the
    !          matrix was interchanged with row IPIV(i).
    !
    !  B       (input/output) real*8 array, dimension (LDB,NRHS)
    !          On entry, the right hand side matrix B.
    !          On exit, the solution matrix X.
    !
    !  LDB     (input) integer
    !          The leading dimension of the array B.  LDB >= max(1,N).
    !
    !  INFO    (output) integer
    !          = 0:  successful exit
    !          < 0:  if INFO = -i, the i-th argument had an illegal value
    !
    !  =====================================================================
    !
    !     .. parameters ..
    real*8 ::         ONE
    !--------------------------------------------------------------------------
    parameter          ( ONE = 1 )
    !     ..
    !     .. Local Scalars ..
    logical            NOTRAN
    !     ..

    !
    !     Test the input parameters.
    !
    INFO = 0
    NOTRAN = LSAME( TRANS, 'N' )
    if( .not.NOTRAN .and. .not.LSAME( TRANS, 'T' ) .and. .not. &
         LSAME( TRANS, 'C' ) ) then
       INFO = -1
    else if( N < 0 ) then
       INFO = -2
    else if( NRHS < 0 ) then
       INFO = -3
    else if( LDA < MAX( 1, N ) ) then
       INFO = -5
    else if( LDB < MAX( 1, N ) ) then
       INFO = -8
    end if
    if( INFO /= 0 ) then
       CALL xerbla( 'DGETRS', -INFO )
       RETURN
    end if
    !
    !     Quick return if possible
    !
    if( N == 0 .OR. NRHS == 0 ) &
         RETURN
    !
    if( NOTRAN ) then
       !
       !        Solve A * X = B.
       !
       !        Apply row interchanges to the right hand sides.
       !
       CALL DLASWP( NRHS, B, LDB, 1, N, IPIV, 1 )
       !
       !        Solve L*X = B, overwriting B with X.
       !
       CALL DTRSM( 'Left', 'Lower', 'No transpose', 'Unit', N, NRHS, &
            ONE, A, LDA, B, LDB )
       !
       !        Solve U*X = B, overwriting B with X.
       !
       CALL DTRSM( 'Left', 'Upper', 'No transpose', 'Non-unit', N, &
            NRHS, ONE, A, LDA, B, LDB )
    else
       !
       !        Solve A**T * X = B  or A**H * X = B.
       !
       !        Solve U'*X = B, overwriting B with X.
       !
       CALL DTRSM( 'Left', 'Upper', TRANS, 'Non-unit', N, NRHS, ONE, &
            A, LDA, B, LDB )
       !
       !        Solve L'*X = B, overwriting B with X.
       !
       CALL DTRSM( 'Left', 'Lower', TRANS, 'Unit', N, NRHS, ONE, A, &
            LDA, B, LDB )
       !
       !        Apply row interchanges to the solution vectors.
       !
       CALL DLASWP( NRHS, B, LDB, 1, N, IPIV, -1 )
    end if
    !
    RETURN
    !
    !     End of DGETRS
    !
  end subroutine DGETRS
  !============================================================================
  subroutine DGETF2( M, N, A, LDA, IPIV, INFO )
    !$acc routine seq
    !
    !  -- LAPACK routine (version 2.0) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     September 30, 1994
    !
    !     .. Scalar Arguments ..
    integer ::            INFO, LDA, M, N
    !     ..
    !     .. Array Arguments ..
    integer ::            IPIV( * )
    real*8 ::         A( LDA, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  DGETF2 computes an LU factorization of a general m-by-n matrix A
    !  using partial pivoting with row interchanges.
    !
    !  The factorization has the form
    !     A = P * L * U
    !  where P is a permutation matrix, L is lower triangular with unit
    !  diagonal elements (lower trapezoidal if m > n), and U is upper
    !  triangular (upper trapezoidal if m < n).
    !
    !  This is the right-looking Level 2 BLAS version of the algorithm.
    !
    !  Arguments
    !  =========
    !
    !  M       (input) integer
    !          The number of rows of the matrix A.  M >= 0.
    !
    !  N       (input) integer
    !          The number of columns of the matrix A.  N >= 0.
    !
    !  A       (input/output) real*8 array, dimension (LDA,N)
    !          On entry, the m by n matrix to be factored.
    !          On exit, the factors L and U from the factorization
    !          A = P*L*U; the unit diagonal elements of L are not stored.
    !
    !  LDA     (input) integer
    !          The leading dimension of the array A.  LDA >= max(1,M).
    !
    !  IPIV    (output) integer array, dimension (min(M,N))
    !          The pivot indices; for 1 <= i <= min(M,N), row i of the
    !          matrix was interchanged with row IPIV(i).
    !
    !  INFO    (output) integer
    !          = 0: successful exit
    !          < 0: if INFO = -k, the k-th argument had an illegal value
    !          > 0: if INFO = k, U(k,k) is exactly zero. The factorization
    !               has been completed, but the factor U is exactly
    !               singular, and division by zero will occur if it is used
    !               to solve a system of equations.
    !
    !  =====================================================================
    !
    !     .. parameters ..
    real*8 ::         ONE, ZERO
    !--------------------------------------------------------------------------
    parameter          ( ONE = 1, ZERO = 0 )
    !     ..
    !     .. Local Scalars ..
    integer ::            J, JP
    !     ..

    !     .. Executable Statements ..
    !
    !     Test the input parameters.
    !
    INFO = 0
    if( M < 0 ) then
       INFO = -1
    else if( N < 0 ) then
       INFO = -2
    else if( LDA < MAX( 1, M ) ) then
       INFO = -4
    end if
    if( INFO /= 0 ) then
       CALL xerbla( 'DGETF2', -INFO )
       RETURN
    end if
    !
    !     Quick return if possible
    !
    if( M == 0 .OR. N == 0 ) &
         RETURN
    !
    do J = 1, MIN( M, N )
       !
       !        Find pivot and test for singularity.
       !
       JP = J - 1 + IDAMAX( M-J+1, A( J, J ), 1 )
       IPIV( J ) = JP
       if( A( JP, J ) /= ZERO ) then
          !
          !           Apply the interchange to columns 1:N.
          !
          if( JP /= J ) &
               CALL DSWAP( N, A( J, 1 ), LDA, A( JP, 1 ), LDA )
          !
          !           Compute elements J+1:M of J-th column.
          !
          if( J < M ) &
               CALL DSCAL( M-J, ONE / A( J, J ), A( J+1, J ), 1 )
          !
       else if( INFO == 0 ) then
          !
          INFO = J
       end if
       !
       if( J < MIN( M, N ) ) then
          !
          !           Update trailing submatrix.
          !
          CALL DGER( M-J, N-J, -ONE, A( J+1, J ), 1, A( J, J+1 ), &
               LDA, A( J+1, J+1 ), LDA )
       end if
    end do
    RETURN
    !
    !     End of DGETF2
    !
  end subroutine DGETF2
  !============================================================================
  subroutine DLASWP( N, A, LDA, K1, K2, IPIV, INCX )
    !$acc routine seq
    !
    !  -- LAPACK auxiliary routine (version 2.0) --
    !     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd.,
    !     Courant Institute, Argonne National Lab, and Rice University
    !     October 31, 1992
    !
    !     .. Scalar Arguments ..
    integer ::            INCX, K1, K2, LDA, N
    !     ..
    !     .. Array Arguments ..
    integer ::            IPIV( * )
    real*8 ::         A( LDA, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  DLASWP performs a series of row interchanges on the matrix A.
    !  One row interchange is initiated for each of rows K1 through K2 of A.
    !
    !  Arguments
    !  =========
    !
    !  N       (input) integer
    !          The number of columns of the matrix A.
    !
    !  A       (input/output) real*8 array, dimension (LDA,N)
    !          On entry, the matrix of column dimension N to which the row
    !          interchanges will be applied.
    !          On exit, the permuted matrix.
    !
    !  LDA     (input) integer
    !          The leading dimension of the array A.
    !
    !  K1      (input) integer
    !          The first element of IPIV for which a row interchange will
    !          be done.
    !
    !  K2      (input) integer
    !          The last element of IPIV for which a row interchange will
    !          be done.
    !
    !  IPIV    (input) integer array, dimension (M*abs(INCX))
    !          The vector of pivot indices.  Only the elements in positions
    !          K1 through K2 of IPIV are accessed.
    !          IPIV(K) = L implies rows K and L are to be interchanged.
    !
    !  INCX    (input) integer
    !          The increment between successive values of IPIV.  If IPIV
    !          is negative, the pivots are applied in reverse order.
    !
    ! =====================================================================
    !
    !     .. Local Scalars ..
    integer ::            I, IP, IX
    !     ..

    !     Interchange row I with row IPIV(I) for each of rows K1 through K2.
    !
    !--------------------------------------------------------------------------
    if( INCX == 0 ) &
         RETURN
    if( INCX > 0 ) then
       IX = K1
    else
       IX = 1 + ( 1-K2 )*INCX
    end if
    if( INCX == 1 ) then
       do I = K1, K2
          IP = IPIV( I )
          if( IP /= I ) &
               CALL DSWAP( N, A( I, 1 ), LDA, A( IP, 1 ), LDA )
       end do
    else if( INCX > 1 ) then
       do I = K1, K2
          IP = IPIV( IX )
          if( IP /= I ) &
               CALL DSWAP( N, A( I, 1 ), LDA, A( IP, 1 ), LDA )
          IX = IX + INCX
       end do
    else if( INCX < 0 ) then
       do I = K2, K1, -1
          IP = IPIV( IX )
          if( IP /= I ) &
               CALL DSWAP( N, A( I, 1 ), LDA, A( IP, 1 ), LDA )
          IX = IX + INCX
       end do
    end if
    !
    RETURN
    !
    !     End of DLASWP
    !
  end subroutine DLASWP
  !============================================================================

  subroutine scopy(n,sx,incx,sy,incy)
    !$acc routine seq
    !
    !     copies a vector, x, to a vector, y.
    !     uses unrolled loops for increments equal to 1.
    !     jack dongarra, linpack, 3/11/78.
    !     modified 12/3/93, array(1) declarations changed to array(*)
    !
    real*4 :: sx(*),sy(*)
    integer :: i,incx,incy,ix,iy,m,mp1,n
    !
    !--------------------------------------------------------------------------
    if(n <= 0)RETURN

    ix = 1
    iy = 1
    if(incx < 0)ix = (-n+1)*incx + 1
    if(incy < 0)iy = (-n+1)*incy + 1
    do i =  1,n
       sy(iy) = sx(ix)
       ix = ix + incx
       iy = iy + incy
    end do
    RETURN
  end subroutine scopy
  !============================================================================
  subroutine SGEMM ( TRANSA, TRANSB, M, N, K, ALPHA, A, LDA, B, LDB, &
       BETA, C, LDC )
    !$acc routine seq
    !     .. Scalar Arguments ..
    character ::        TRANSA, TRANSB
    integer ::            M, N, K, LDA, LDB, LDC
    real*4 ::             ALPHA, BETA
    !     .. Array Arguments ..
    real*4 ::             A( LDA, * ), B( LDB, * ), C( LDC, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  SGEMM  performs one of the matrix-matrix operations
    !
    !     C := alpha*op( A )*op( B ) + beta*C,
    !
    !  where  op( X ) is one of
    !
    !     op( X ) = X   or   op( X ) = X',
    !
    !  alpha and beta are scalars, and A, B and C are matrices, with op( A )
    !  an m by k matrix,  op( B )  a  k by n matrix and  C an m by n matrix.
    !
    !  parameters
    !  ==========
    !
    !  TRANSA - character.
    !           On entry, TRANSA specifies the form of op( A ) to be used in
    !           the matrix multiplication as follows:
    !
    !              TRANSA = 'N' or 'n',  op( A ) = A.
    !
    !              TRANSA = 'T' or 't',  op( A ) = A'.
    !
    !              TRANSA = 'C' or 'c',  op( A ) = A'.
    !
    !           Unchanged on exit.
    !
    !  TRANSB - character.
    !           On entry, TRANSB specifies the form of op( B ) to be used in
    !           the matrix multiplication as follows:
    !
    !              TRANSB = 'N' or 'n',  op( B ) = B.
    !
    !              TRANSB = 'T' or 't',  op( B ) = B'.
    !
    !              TRANSB = 'C' or 'c',  op( B ) = B'.
    !
    !           Unchanged on exit.
    !
    !  M      - integer.
    !           On entry,  M  specifies  the number  of rows  of the  matrix
    !           op( A )  and of the  matrix  C.  M  must  be at least  zero.
    !           Unchanged on exit.
    !
    !  N      - integer.
    !           On entry,  N  specifies the number  of columns of the matrix
    !           op( B ) and the number of columns of the matrix C. N must be
    !           at least zero.
    !           Unchanged on exit.
    !
    !  K      - integer.
    !           On entry,  K  specifies  the number of columns of the matrix
    !           op( A ) and the number of rows of the matrix op( B ). K must
    !           be at least  zero.
    !           Unchanged on exit.
    !
    !  ALPHA  - real*4          .
    !           On entry, ALPHA specifies the scalar alpha.
    !           Unchanged on exit.
    !
    !  A      - real*4           array of DIMENSION ( LDA, ka ), where ka is
    !           k  when  TRANSA = 'N' or 'n',  and is  m  otherwise.
    !           Before entry with  TRANSA = 'N' or 'n',  the leading  m by k
    !           part of the array  A  must contain the matrix  A,  otherwise
    !           the leading  k by m  part of the array  A  must contain  the
    !           matrix A.
    !           Unchanged on exit.
    !
    !  LDA    - integer.
    !           On entry, LDA specifies the first dimension of A as declared
    !           in the calling (sub) program. When  TRANSA = 'N' or 'n' then
    !           LDA must be at least  max( 1, m ), otherwise  LDA must be at
    !           least  max( 1, k ).
    !           Unchanged on exit.
    !
    !  B      - real*4           array of DIMENSION ( LDB, kb ), where kb is
    !           n  when  TRANSB = 'N' or 'n',  and is  k  otherwise.
    !           Before entry with  TRANSB = 'N' or 'n',  the leading  k by n
    !           part of the array  B  must contain the matrix  B,  otherwise
    !           the leading  n by k  part of the array  B  must contain  the
    !           matrix B.
    !           Unchanged on exit.
    !
    !  LDB    - integer.
    !           On entry, LDB specifies the first dimension of B as declared
    !           in the calling (sub) program. When  TRANSB = 'N' or 'n' then
    !           LDB must be at least  max( 1, k ), otherwise  LDB must be at
    !           least  max( 1, n ).
    !           Unchanged on exit.
    !
    !  BETA   - real*4          .
    !           On entry,  BETA  specifies the scalar  beta.  When  BETA  is
    !           supplied as zero then C need not be set on input.
    !           Unchanged on exit.
    !
    !  C      - real*4           array of DIMENSION ( LDC, n ).
    !           Before entry, the leading  m by n  part of the array  C must
    !           contain the matrix  C,  except when  beta  is zero, in which
    !           case C need not be set on entry.
    !           On exit, the array  C  is overwritten by the  m by n  matrix
    !           ( alpha*op( A )*op( B ) + beta*C ).
    !
    !  LDC    - integer.
    !           On entry, LDC specifies the first dimension of C as declared
    !           in  the  calling  (sub)  program.   LDC  must  be  at  least
    !           max( 1, m ).
    !           Unchanged on exit.
    !
    !
    !  Level 3 Blas routine.
    !
    !  -- Written on 8-February-1989.
    !     Jack Dongarra, Argonne National Laboratory.
    !     Iain Duff, AERE Harwell.
    !     Jeremy Du Croz, Numerical Algorithms Group Ltd.
    !     Sven Hammarling, Numerical Algorithms Group Ltd.
    !
    !

    !     .. Local Scalars ..
    !--------------------------------------------------------------------------
    logical            NOTA, NOTB
    integer ::            I, INFO, J, L, NCOLA, NROWA, NROWB
    real*4 ::             TEMP
    !     .. parameters ..
    real*4 ::             ONE         , ZERO
    parameter        ( ONE = 1.0E+0, ZERO = 0.0E+0 )
    !     ..
    !     .. Executable Statements ..
    !
    !     Set  NOTA  and  NOTB  as  true if  A  and  B  respectively are not
    !     transposed and set  NROWA, NCOLA and  NROWB  as the number of rows
    !     and  columns of  A  and the  number of  rows  of  B  respectively.
    !
    NOTA  = LSAME( TRANSA, 'N' )
    NOTB  = LSAME( TRANSB, 'N' )
    if( NOTA )then
       NROWA = M
       NCOLA = K
    else
       NROWA = K
       NCOLA = M
    end if
    if( NOTB )then
       NROWB = K
    else
       NROWB = N
    end if
    !
    !     Test the input parameters.
    !
    INFO = 0
    if(      ( .not.NOTA                 ).and. &
         ( .not.LSAME( TRANSA, 'C' ) ).and. &
         ( .not.LSAME( TRANSA, 'T' ) )      )then
       INFO = 1
    else if( ( .not.NOTB                 ).and. &
         ( .not.LSAME( TRANSB, 'C' ) ).and. &
         ( .not.LSAME( TRANSB, 'T' ) )      )then
       INFO = 2
    else if( M < 0               )then
       INFO = 3
    else if( N < 0               )then
       INFO = 4
    else if( K < 0               )then
       INFO = 5
    else if( LDA < MAX( 1, NROWA ) )then
       INFO = 8
    else if( LDB < MAX( 1, NROWB ) )then
       INFO = 10
    else if( LDC < MAX( 1, M     ) )then
       INFO = 13
    end if
    if( INFO /= 0 )then
       CALL xerbla( 'SGEMM ', INFO )
       RETURN
    end if
    !
    !     Quick return if possible.
    !
    if( ( M == 0 ).OR.( N == 0 ).OR. &
         ( ( ( ALPHA == ZERO ).OR.( K == 0 ) ).and.( BETA == ONE ) ) ) &
         RETURN
    !
    !     And if  alpha == zero.
    !
    if( ALPHA == ZERO )then
       if( BETA == ZERO )then
          do J =  1, N
             do I =  1, M
                C( I, J ) = ZERO
             end do
          end do
       else
          do J =  1, N
             do I =  1, M
                C( I, J ) = BETA*C( I, J )
             end do
          end do
       end if
       RETURN
    end if
    !
    !     Start the operations.
    !
    if( NOTB )then
       if( NOTA )then
          !
          !           Form  C := alpha*A*B + beta*C.
          !
          do J =  1, N
             if( BETA == ZERO )then
                do I =  1, M
                   C( I, J ) = ZERO
                end do
             else if( BETA /= ONE )then
                do I =  1, M
                   C( I, J ) = BETA*C( I, J )
                end do
             end if
             do L = 1, K
                if( B( L, J ) /= ZERO )then
                   TEMP = ALPHA*B( L, J )
                   do I =  1, M
                      C( I, J ) = C( I, J ) + TEMP*A( I, L )
                   end do
                end if
             end do
          end do
       else
          !
          !           Form  C := alpha*A'*B + beta*C
          !
          do J =  1, N
             do I =  1, M
                TEMP = ZERO
                do L = 1, K
                   TEMP = TEMP + A( L, I )*B( L, J )
                end do
                if( BETA == ZERO )then
                   C( I, J ) = ALPHA*TEMP
                else
                   C( I, J ) = ALPHA*TEMP + BETA*C( I, J )
                end if
             end do
          end do
       end if
    else
       if( NOTA )then
          !
          !           Form  C := alpha*A*B' + beta*C
          !
          do J =  1, N
             if( BETA == ZERO )then
                do I =  1, M
                   C( I, J ) = ZERO
                end do
             else if( BETA /= ONE )then
                do I =  1, M
                   C( I, J ) = BETA*C( I, J )
                end do
             end if
             do L = 1, K
                if( B( J, L ) /= ZERO )then
                   TEMP = ALPHA*B( J, L )
                   do I =  1, M
                      C( I, J ) = C( I, J ) + TEMP*A( I, L )
                   end do
                end if
             end do
          end do
       else
          !
          !           Form  C := alpha*A'*B' + beta*C
          !
          do J =  1, N
             do I =  1, M
                TEMP = ZERO
                do L = 1, K
                   TEMP = TEMP + A( L, I )*B( J, L )
                end do
                if( BETA == ZERO )then
                   C( I, J ) = ALPHA*TEMP
                else
                   C( I, J ) = ALPHA*TEMP + BETA*C( I, J )
                end if
             end do
          end do
       end if
    end if
    !
    RETURN
    !
    !     End of SGEMM .
    !
  end subroutine SGEMM
  !============================================================================
  subroutine SGEMV ( TRANS, M, N, ALPHA, A, LDA, X, INCX, &
       BETA, Y, INCY )
    !$acc routine seq
    !     .. Scalar Arguments ..
    real*4 ::             ALPHA, BETA
    integer ::            INCX, INCY, LDA, M, N
    character ::        TRANS
    !     .. Array Arguments ..
    real*4 ::             A( LDA, * ), X( * ), Y( * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  SGEMV  performs one of the matrix-vector operations
    !
    !     y := alpha*A*x + beta*y,   or   y := alpha*A'*x + beta*y,
    !
    !  where alpha and beta are scalars, x and y are vectors and A is an
    !  m by n matrix.
    !
    !  parameters
    !  ==========
    !
    !  TRANS  - character.
    !           On entry, TRANS specifies the operation to be performed as
    !           follows:
    !
    !              TRANS = 'N' or 'n'   y := alpha*A*x + beta*y.
    !
    !              TRANS = 'T' or 't'   y := alpha*A'*x + beta*y.
    !
    !              TRANS = 'C' or 'c'   y := alpha*A'*x + beta*y.
    !
    !           Unchanged on exit.
    !
    !  M      - integer.
    !           On entry, M specifies the number of rows of the matrix A.
    !           M must be at least zero.
    !           Unchanged on exit.
    !
    !  N      - integer.
    !           On entry, N specifies the number of columns of the matrix A.
    !           N must be at least zero.
    !           Unchanged on exit.
    !
    !  ALPHA  - real*4          .
    !           On entry, ALPHA specifies the scalar alpha.
    !           Unchanged on exit.
    !
    !  A      - real*4           array of DIMENSION ( LDA, n ).
    !           Before entry, the leading m by n part of the array A must
    !           contain the matrix of coefficients.
    !           Unchanged on exit.
    !
    !  LDA    - integer.
    !           On entry, LDA specifies the first dimension of A as declared
    !           in the calling (sub) program. LDA must be at least
    !           max( 1, m ).
    !           Unchanged on exit.
    !
    !  X      - real*4           array of DIMENSION at least
    !           ( 1 + ( n - 1 )*abs( INCX ) ) when TRANS = 'N' or 'n'
    !           and at least
    !           ( 1 + ( m - 1 )*abs( INCX ) ) otherwise.
    !           Before entry, the incremented array X must contain the
    !           vector x.
    !           Unchanged on exit.
    !
    !  INCX   - integer.
    !           On entry, INCX specifies the increment for the elements of
    !           X. INCX must not be zero.
    !           Unchanged on exit.
    !
    !  BETA   - real*4          .
    !           On entry, BETA specifies the scalar beta. When BETA is
    !           supplied as zero then Y need not be set on input.
    !           Unchanged on exit.
    !
    !  Y      - real*4           array of DIMENSION at least
    !           ( 1 + ( m - 1 )*abs( INCY ) ) when TRANS = 'N' or 'n'
    !           and at least
    !           ( 1 + ( n - 1 )*abs( INCY ) ) otherwise.
    !           Before entry with BETA non-zero, the incremented array Y
    !           must contain the vector y. On exit, Y is overwritten by the
    !           updated vector y.
    !
    !  INCY   - integer.
    !           On entry, INCY specifies the increment for the elements of
    !           Y. INCY must not be zero.
    !           Unchanged on exit.
    !
    !
    !  Level 2 Blas routine.
    !
    !  -- Written on 22-October-1986.
    !     Jack Dongarra, Argonne National Lab.
    !     Jeremy Du Croz, Nag Central Office.
    !     Sven Hammarling, Nag Central Office.
    !     Richard Hanson, Sandia National Labs.
    !
    !
    !     .. parameters ..
    real*4 ::             ONE         , ZERO
    !--------------------------------------------------------------------------
    parameter        ( ONE = 1.0E+0, ZERO = 0.0E+0 )
    !     .. Local Scalars ..
    real*4 ::             TEMP
    integer ::            I, INFO, IX, IY, J, JX, JY, KX, KY, LENX, LENY

    INFO = 0
    if     ( .not.LSAME( TRANS, 'N' ).and. &
         .not.LSAME( TRANS, 'T' ).and. &
         .not.LSAME( TRANS, 'C' )      )then
       INFO = 1
    else if( M < 0 )then
       INFO = 2
    else if( N < 0 )then
       INFO = 3
    else if( LDA < MAX( 1, M ) )then
       INFO = 6
    else if( INCX == 0 )then
       INFO = 8
    else if( INCY == 0 )then
       INFO = 11
    end if
    if( INFO /= 0 )then
       CALL xerbla( 'SGEMV ', INFO )
       RETURN
    end if
    !
    !     Quick return if possible.
    !
    if( ( M == 0 ).OR.( N == 0 ).OR. &
         ( ( ALPHA == ZERO ).and.( BETA == ONE ) ) ) &
         RETURN
    !
    !     Set  LENX  and  LENY, the lengths of the vectors x and y, and set
    !     up the start points in  X  and  Y.
    !
    if( LSAME( TRANS, 'N' ) )then
       LENX = N
       LENY = M
    else
       LENX = M
       LENY = N
    end if
    if( INCX > 0 )then
       KX = 1
    else
       KX = 1 - ( LENX - 1 )*INCX
    end if
    if( INCY > 0 )then
       KY = 1
    else
       KY = 1 - ( LENY - 1 )*INCY
    end if
    !
    !     Start the operations. In this version the elements of A are
    !     accessed sequentially with one pass through A.
    !
    !     First form  y := beta*y.
    !
    if( BETA /= ONE )then
       if( INCY == 1 )then
          if( BETA == ZERO )then
             do I =  1, LENY
                Y( I ) = ZERO
             end do
          else
             do I =  1, LENY
                Y( I ) = BETA*Y( I )
             end do
          end if
       else
          IY = KY
          if( BETA == ZERO )then
             do I =  1, LENY
                Y( IY ) = ZERO
                IY      = IY   + INCY
             end do
          else
             do I =  1, LENY
                Y( IY ) = BETA*Y( IY )
                IY      = IY           + INCY
             end do
          end if
       end if
    end if
    if( ALPHA == ZERO ) &
         RETURN
    if( LSAME( TRANS, 'N' ) )then
       !
       !        Form  y := alpha*A*x + y.
       !
       JX = KX
       if( INCY == 1 )then
          do J =  1, N
             if( X( JX ) /= ZERO )then
                TEMP = ALPHA*X( JX )
                do I =  1, M
                   Y( I ) = Y( I ) + TEMP*A( I, J )
                end do
             end if
             JX = JX + INCX
          end do
       else
          do J =  1, N
             if( X( JX ) /= ZERO )then
                TEMP = ALPHA*X( JX )
                IY   = KY
                do I =  1, M
                   Y( IY ) = Y( IY ) + TEMP*A( I, J )
                   IY      = IY      + INCY
                end do
             end if
             JX = JX + INCX
          end do
       end if
    else
       !
       !        Form  y := alpha*A'*x + y.
       !
       JY = KY
       if( INCX == 1 )then
          do J =  1, N
             TEMP = ZERO
             do I =  1, M
                TEMP = TEMP + A( I, J )*X( I )
             end do
             Y( JY ) = Y( JY ) + ALPHA*TEMP
             JY      = JY      + INCY
          end do
       else
          do J =  1, N
             TEMP = ZERO
             IX   = KX
             do I =  1, M
                TEMP = TEMP + A( I, J )*X( IX )
                IX   = IX   + INCX
             end do
             Y( JY ) = Y( JY ) + ALPHA*TEMP
             JY      = JY      + INCY
          end do
       end if
    end if
    !
    RETURN
    !
    !     End of SGEMV .
    !
  end subroutine SGEMV
  !============================================================================
  subroutine SGER  ( M, N, ALPHA, X, INCX, Y, INCY, A, LDA )
    !$acc routine seq
    !     .. Scalar Arguments ..
    real*4 ::             ALPHA
    integer ::            INCX, INCY, LDA, M, N
    !     .. Array Arguments ..
    real*4 ::             A( LDA, * ), X( * ), Y( * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  SGER   performs the rank 1 operation
    !
    !     A := alpha*x*y' + A,
    !
    !  where alpha is a scalar, x is an m element vector, y is an n element
    !  vector and A is an m by n matrix.
    !
    !  parameters
    !  ==========
    !
    !  M      - integer.
    !           On entry, M specifies the number of rows of the matrix A.
    !           M must be at least zero.
    !           Unchanged on exit.
    !
    !  N      - integer.
    !           On entry, N specifies the number of columns of the matrix A.
    !           N must be at least zero.
    !           Unchanged on exit.
    !
    !  ALPHA  - real*4          .
    !           On entry, ALPHA specifies the scalar alpha.
    !           Unchanged on exit.
    !
    !  X      - real*4           array of dimension at least
    !           ( 1 + ( m - 1 )*abs( INCX ) ).
    !           Before entry, the incremented array X must contain the m
    !           element vector x.
    !           Unchanged on exit.
    !
    !  INCX   - integer.
    !           On entry, INCX specifies the increment for the elements of
    !           X. INCX must not be zero.
    !           Unchanged on exit.
    !
    !  Y      - real*4           array of dimension at least
    !           ( 1 + ( n - 1 )*abs( INCY ) ).
    !           Before entry, the incremented array Y must contain the n
    !           element vector y.
    !           Unchanged on exit.
    !
    !  INCY   - integer.
    !           On entry, INCY specifies the increment for the elements of
    !           Y. INCY must not be zero.
    !           Unchanged on exit.
    !
    !  A      - real*4           array of DIMENSION ( LDA, n ).
    !           Before entry, the leading m by n part of the array A must
    !           contain the matrix of coefficients. On exit, A is
    !           overwritten by the updated matrix.
    !
    !  LDA    - integer.
    !           On entry, LDA specifies the first dimension of A as declared
    !           in the calling (sub) program. LDA must be at least
    !           max( 1, m ).
    !           Unchanged on exit.
    !
    !
    !  Level 2 Blas routine.
    !
    !  -- Written on 22-October-1986.
    !     Jack Dongarra, Argonne National Lab.
    !     Jeremy Du Croz, Nag Central Office.
    !     Sven Hammarling, Nag Central Office.
    !     Richard Hanson, Sandia National Labs.
    !
    !
    !     .. parameters ..
    real*4 ::             ZERO
    !--------------------------------------------------------------------------
    parameter        ( ZERO = 0.0E+0 )
    !     .. Local Scalars ..
    real*4 ::             TEMP
    integer ::            I, INFO, IX, J, JY, KX

    INFO = 0
    if     ( M < 0 )then
       INFO = 1
    else if( N < 0 )then
       INFO = 2
    else if( INCX == 0 )then
       INFO = 5
    else if( INCY == 0 )then
       INFO = 7
    else if( LDA < MAX( 1, M ) )then
       INFO = 9
    end if
    if( INFO /= 0 )then
       CALL xerbla( 'SGER  ', INFO )
       RETURN
    end if
    !
    !     Quick return if possible.
    !
    if( ( M == 0 ).OR.( N == 0 ).OR.( ALPHA == ZERO ) ) &
         RETURN
    !
    !     Start the operations. In this version the elements of A are
    !     accessed sequentially with one pass through A.
    !
    if( INCY > 0 )then
       JY = 1
    else
       JY = 1 - ( N - 1 )*INCY
    end if
    if( INCX == 1 )then
       do J =  1, N
          if( Y( JY ) /= ZERO )then
             TEMP = ALPHA*Y( JY )
             do I =  1, M
                A( I, J ) = A( I, J ) + X( I )*TEMP
             end do
          end if
          JY = JY + INCY
       end do
    else
       if( INCX > 0 )then
          KX = 1
       else
          KX = 1 - ( M - 1 )*INCX
       end if
       do J =  1, N
          if( Y( JY ) /= ZERO )then
             TEMP = ALPHA*Y( JY )
             IX   = KX
             do I =  1, M
                A( I, J ) = A( I, J ) + X( IX )*TEMP
                IX        = IX        + INCX
             end do
          end if
          JY = JY + INCY
       end do
    end if
    !
    RETURN
    !
    !     End of SGER  .
    !
  end subroutine SGER
  !============================================================================
  subroutine sscal(n,sa,sx,incx)
    !$acc routine seq
    !
    !     scales a vector by a constant.
    !     uses unrolled loops for increment equal to 1.
    !     jack dongarra, linpack, 3/11/78.
    !     modified 3/93 to return if incx .le. 0.
    !     modified 12/3/93, array(1) declarations changed to array(*)
    !
    real*4 ::  sa,sx(*)
    integer :: i,incx,m,mp1,n,nincx
    !
    !--------------------------------------------------------------------------
    if( n <= 0 .or. incx <= 0 )RETURN

    nincx = n*incx
    do i =  1,nincx,incx
       sx(i) = sa*sx(i)
    end do
  end subroutine sscal
  !============================================================================
  subroutine sswap (n,sx,incx,sy,incy)
    !$acc routine seq
    !
    !     interchanges two vectors.
    !     uses unrolled loops for increments equal to 1.
    !     jack dongarra, linpack, 3/11/78.
    !     modified 12/3/93, array(1) declarations changed to array(*)
    !
    real*4 ::  sx(*),sy(*),stemp
    integer :: i,incx,incy,ix,iy,m,mp1,n
    !
    !--------------------------------------------------------------------------
    if(n <= 0)RETURN

    ix = 1
    iy = 1
    if(incx < 0)ix = (-n+1)*incx + 1
    if(incy < 0)iy = (-n+1)*incy + 1
    do i =  1,n
       stemp = sx(ix)
       sx(ix) = sy(iy)
       sy(iy) = stemp
       ix = ix + incx
       iy = iy + incy
    end do
  end subroutine sswap
  !============================================================================
  subroutine STRSM ( SIDE, UPLO, TRANSA, DIAG, M, N, ALPHA, A, LDA, &
       B, LDB )
    !$acc routine seq
    !     .. Scalar Arguments ..
    character ::        SIDE, UPLO, TRANSA, DIAG
    integer ::            M, N, LDA, LDB
    real*4 ::             ALPHA
    !     .. Array Arguments ..
    real*4 ::             A( LDA, * ), B( LDB, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  STRSM  solves one of the matrix equations
    !
    !     op( A )*X = alpha*B,   or   X*op( A ) = alpha*B,
    !
    !  where alpha is a scalar, X and B are m by n matrices, A is a unit, or
    !  non-unit,  upper or lower triangular matrix  and  op( A )  is one  of
    !
    !     op( A ) = A   or   op( A ) = A'.
    !
    !  The matrix X is overwritten on B.
    !
    !  parameters
    !  ==========
    !
    !  SIDE   - character.
    !           On entry, SIDE specifies whether op( A ) appears on the left
    !           or right of X as follows:
    !
    !              SIDE = 'L' or 'l'   op( A )*X = alpha*B.
    !
    !              SIDE = 'R' or 'r'   X*op( A ) = alpha*B.
    !
    !           Unchanged on exit.
    !
    !  UPLO   - character.
    !           On entry, UPLO specifies whether the matrix A is an upper or
    !           lower triangular matrix as follows:
    !
    !              UPLO = 'U' or 'u'   A is an upper triangular matrix.
    !
    !              UPLO = 'L' or 'l'   A is a lower triangular matrix.
    !
    !           Unchanged on exit.
    !
    !  TRANSA - character.
    !           On entry, TRANSA specifies the form of op( A ) to be used in
    !           the matrix multiplication as follows:
    !
    !              TRANSA = 'N' or 'n'   op( A ) = A.
    !
    !              TRANSA = 'T' or 't'   op( A ) = A'.
    !
    !              TRANSA = 'C' or 'c'   op( A ) = A'.
    !
    !           Unchanged on exit.
    !
    !  DIAG   - character.
    !           On entry, DIAG specifies whether or not A is unit triangular
    !           as follows:
    !
    !              DIAG = 'U' or 'u'   A is assumed to be unit triangular.
    !
    !              DIAG = 'N' or 'n'   A is not assumed to be unit
    !                                  triangular.
    !
    !           Unchanged on exit.
    !
    !  M      - integer.
    !           On entry, M specifies the number of rows of B. M must be at
    !           least zero.
    !           Unchanged on exit.
    !
    !  N      - integer.
    !           On entry, N specifies the number of columns of B.  N must be
    !           at least zero.
    !           Unchanged on exit.
    !
    !  ALPHA  - real*4          .
    !           On entry,  ALPHA specifies the scalar  alpha. When  alpha is
    !           zero then  A is not referenced and  B need not be set before
    !           entry.
    !           Unchanged on exit.
    !
    !  A      - real*4           array of DIMENSION ( LDA, k ), where k is m
    !           when  SIDE = 'L' or 'l'  and is  n  when  SIDE = 'R' or 'r'.
    !           Before entry  with  UPLO = 'U' or 'u',  the  leading  k by k
    !           upper triangular part of the array  A must contain the upper
    !           triangular matrix  and the strictly lower triangular part of
    !           A is not referenced.
    !           Before entry  with  UPLO = 'L' or 'l',  the  leading  k by k
    !           lower triangular part of the array  A must contain the lower
    !           triangular matrix  and the strictly upper triangular part of
    !           A is not referenced.
    !           Note that when  DIAG = 'U' or 'u',  the diagonal elements of
    !           A  are not referenced either,  but are assumed to be  unity.
    !           Unchanged on exit.
    !
    !  LDA    - integer.
    !           On entry, LDA specifies the first dimension of A as declared
    !           in the calling (sub) program.  When  SIDE = 'L' or 'l'  then
    !           LDA  must be at least  max( 1, m ),  when  SIDE = 'R' or 'r'
    !           then LDA must be at least max( 1, n ).
    !           Unchanged on exit.
    !
    !  B      - real*4           array of DIMENSION ( LDB, n ).
    !           Before entry,  the leading  m by n part of the array  B must
    !           contain  the  right-hand  side  matrix  B,  and  on exit  is
    !           overwritten by the solution matrix  X.
    !
    !  LDB    - integer.
    !           On entry, LDB specifies the first dimension of B as declared
    !           in  the  calling  (sub)  program.   LDB  must  be  at  least
    !           max( 1, m ).
    !           Unchanged on exit.
    !
    !
    !  Level 3 Blas routine.
    !
    !
    !  -- Written on 8-February-1989.
    !     Jack Dongarra, Argonne National Laboratory.
    !     Iain Duff, AERE Harwell.
    !     Jeremy Du Croz, Numerical Algorithms Group Ltd.
    !     Sven Hammarling, Numerical Algorithms Group Ltd.
    !
    !

    !--------------------------------------------------------------------------
    logical            LSIDE, NOUNIT, UPPER
    integer ::            I, INFO, J, K, NROWA
    real*4 ::             TEMP
    !     .. parameters ..
    real*4 ::             ONE         , ZERO
    parameter        ( ONE = 1.0E+0, ZERO = 0.0E+0 )
    !     ..
    !     .. Executable Statements ..
    !
    !     Test the input parameters.
    !
    LSIDE  = LSAME( SIDE  , 'L' )
    if( LSIDE )then
       NROWA = M
    else
       NROWA = N
    end if
    NOUNIT = LSAME( DIAG  , 'N' )
    UPPER  = LSAME( UPLO  , 'U' )
    !
    INFO   = 0
    if(      ( .not.LSIDE                ).and. &
         ( .not.LSAME( SIDE  , 'R' ) )      )then
       INFO = 1
    else if( ( .not.UPPER                ).and. &
         ( .not.LSAME( UPLO  , 'L' ) )      )then
       INFO = 2
    else if( ( .not.LSAME( TRANSA, 'N' ) ).and. &
         ( .not.LSAME( TRANSA, 'T' ) ).and. &
         ( .not.LSAME( TRANSA, 'C' ) )      )then
       INFO = 3
    else if( ( .not.LSAME( DIAG  , 'U' ) ).and. &
         ( .not.LSAME( DIAG  , 'N' ) )      )then
       INFO = 4
    else if( M < 0               )then
       INFO = 5
    else if( N < 0               )then
       INFO = 6
    else if( LDA < MAX( 1, NROWA ) )then
       INFO = 9
    else if( LDB < MAX( 1, M     ) )then
       INFO = 11
    end if
    if( INFO /= 0 )then
       CALL xerbla( 'STRSM ', INFO )
       RETURN
    end if
    !
    !     Quick return if possible.
    !
    if( N == 0 ) &
         RETURN
    !
    !     And when  alpha == zero.
    !
    if( ALPHA == ZERO )then
       do J =  1, N
          do I =  1, M
             B( I, J ) = ZERO
          end do
       end do
       RETURN
    end if
    !
    !     Start the operations.
    !
    if( LSIDE )then
       if( LSAME( TRANSA, 'N' ) )then
          !
          !           Form  B := alpha*inv( A )*B.
          !
          if( UPPER )then
             do J =  1, N
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, J ) = ALPHA*B( I, J )
                   end do
                end if
                do K =  M, 1, -1
                   if( B( K, J ) /= ZERO )then
                      if( NOUNIT ) &
                           B( K, J ) = B( K, J )/A( K, K )
                      do I =  1, K - 1
                         B( I, J ) = B( I, J ) - B( K, J )*A( I, K )
                      end do
                   end if
                end do
             end do
          else
             do J =  1, N
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, J ) = ALPHA*B( I, J )
                   end do
                end if
                do K =  1, M
                   if( B( K, J ) /= ZERO )then
                      if( NOUNIT ) &
                           B( K, J ) = B( K, J )/A( K, K )
                      do I =  K + 1, M
                         B( I, J ) = B( I, J ) - B( K, J )*A( I, K )
                      end do
                   end if
                end do
             end do
          end if
       else
          !
          !           Form  B := alpha*inv( A' )*B.
          !
          if( UPPER )then
             do J =  1, N
                do I =  1, M
                   TEMP = ALPHA*B( I, J )
                   do K =  1, I - 1
                      TEMP = TEMP - A( K, I )*B( K, J )
                   end do
                   if( NOUNIT ) &
                        TEMP = TEMP/A( I, I )
                   B( I, J ) = TEMP
                end do
             end do
          else
             do J =  1, N
                do I =  M, 1, -1
                   TEMP = ALPHA*B( I, J )
                   do K =  I + 1, M
                      TEMP = TEMP - A( K, I )*B( K, J )
                   end do
                   if( NOUNIT ) &
                        TEMP = TEMP/A( I, I )
                   B( I, J ) = TEMP
                end do
             end do
          end if
       end if
    else
       if( LSAME( TRANSA, 'N' ) )then
          !
          !           Form  B := alpha*B*inv( A ).
          !
          if( UPPER )then
             do J =  1, N
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, J ) = ALPHA*B( I, J )
                   end do
                end if
                do K =  1, J - 1
                   if( A( K, J ) /= ZERO )then
                      do I =  1, M
                         B( I, J ) = B( I, J ) - A( K, J )*B( I, K )
                      end do
                   end if
                end do
                if( NOUNIT )then
                   TEMP = ONE/A( J, J )
                   do I =  1, M
                      B( I, J ) = TEMP*B( I, J )
                   end do
                end if
             end do
          else
             do J =  N, 1, -1
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, J ) = ALPHA*B( I, J )
                   end do
                end if
                do K =  J + 1, N
                   if( A( K, J ) /= ZERO )then
                      do I =  1, M
                         B( I, J ) = B( I, J ) - A( K, J )*B( I, K )
                      end do
                   end if
                end do
                if( NOUNIT )then
                   TEMP = ONE/A( J, J )
                   do I =  1, M
                      B( I, J ) = TEMP*B( I, J )
                   end do
                end if
             end do
          end if
       else
          !
          !           Form  B := alpha*B*inv( A' ).
          !
          if( UPPER )then
             do K =  N, 1, -1
                if( NOUNIT )then
                   TEMP = ONE/A( K, K )
                   do I =  1, M
                      B( I, K ) = TEMP*B( I, K )
                   end do
                end if
                do J =  1, K - 1
                   if( A( J, K ) /= ZERO )then
                      TEMP = A( J, K )
                      do I =  1, M
                         B( I, J ) = B( I, J ) - TEMP*B( I, K )
                      end do
                   end if
                end do
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, K ) = ALPHA*B( I, K )
                   end do
                end if
             end do
          else
             do K =  1, N
                if( NOUNIT )then
                   TEMP = ONE/A( K, K )
                   do I =  1, M
                      B( I, K ) = TEMP*B( I, K )
                   end do
                end if
                do J =  K + 1, N
                   if( A( J, K ) /= ZERO )then
                      TEMP = A( J, K )
                      do I =  1, M
                         B( I, J ) = B( I, J ) - TEMP*B( I, K )
                      end do
                   end if
                end do
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, K ) = ALPHA*B( I, K )
                   end do
                end if
             end do
          end if
       end if
    end if
    !
    RETURN
    !
    !     End of STRSM .
    !
  end subroutine STRSM
  !============================================================================
  integer function isamax(n,sx,incx)
    !$acc routine seq
    !
    !     finds the index of element having max. absolute value.
    !     jack dongarra, linpack, 3/11/78.
    !     modified 3/93 to return if incx .le. 0.
    !     modified 12/3/93, array(1) declarations changed to array(*)
    !
    real*4 ::  sx(*),smax
    integer :: i,incx,ix,n
    !
    !--------------------------------------------------------------------------
    isamax = 0
    if( n < 1 .or. incx <= 0 ) RETURN
    isamax = 1
    if(n == 1)RETURN

    ix = 1
    smax = abs(sx(1))
    ix = ix + incx
    do i =  2,n
       if(.not. (abs(sx(ix)) <= smax)) then
          isamax = i
          smax = abs(sx(ix))
       end if
       ix = ix + incx
    end do
    RETURN
  end function isamax
  !============================================================================
  ! This is a collection of real*8 BLAS routines that BATSRUS uses.
  ! You are encouraged to use the local BLAS library if available.
  !
  ! subroutines: dcopy, dgemm, dgemv, dger,  dscal, dswap, dtrsm
  !
  subroutine  dcopy(n,dx,incx,dy,incy)
    !$acc routine seq
    !
    !     copies a vector, x, to a vector, y.
    !     uses unrolled loops for increments equal to one.
    !     jack dongarra, linpack, 3/11/78.
    !
    real*8 :: dx(*),dy(*)
    integer :: i,incx,incy,ix,iy,m,mp1,n
    !
    !--------------------------------------------------------------------------
    if(n <= 0)RETURN

    ix = 1
    iy = 1
    if(incx < 0)ix = (-n+1)*incx + 1
    if(incy < 0)iy = (-n+1)*incy + 1
    do i =  1,n
       dy(iy) = dx(ix)
       ix = ix + incx
       iy = iy + incy
    end do
    RETURN
  end subroutine dcopy
  !============================================================================
  subroutine DGEMM ( TRANSA, TRANSB, M, N, K, ALPHA, A, LDA, B, LDB, &
       BETA, C, LDC )
    !$acc routine seq
    !     .. Scalar Arguments ..
    character ::        TRANSA, TRANSB
    integer ::            M, N, K, LDA, LDB, LDC
    real*8 ::   ALPHA, BETA
    !     .. Array Arguments ..
    real*8 ::   A( LDA, * ), B( LDB, * ), C( LDC, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  DGEMM  performs one of the matrix-matrix operations
    !
    !     C := alpha*op( A )*op( B ) + beta*C,
    !
    !  where  op( X ) is one of
    !
    !     op( X ) = X   or   op( X ) = X',
    !
    !  alpha and beta are scalars, and A, B and C are matrices, with op( A )
    !  an m by k matrix,  op( B )  a  k by n matrix and  C an m by n matrix.
    !
    !  parameters
    !  ==========
    !
    !  TRANSA - character.
    !           On entry, TRANSA specifies the form of op( A ) to be used in
    !           the matrix multiplication as follows:
    !
    !              TRANSA = 'N' or 'n',  op( A ) = A.
    !
    !              TRANSA = 'T' or 't',  op( A ) = A'.
    !
    !              TRANSA = 'C' or 'c',  op( A ) = A'.
    !
    !           Unchanged on exit.
    !
    !  TRANSB - character.
    !           On entry, TRANSB specifies the form of op( B ) to be used in
    !           the matrix multiplication as follows:
    !
    !              TRANSB = 'N' or 'n',  op( B ) = B.
    !
    !              TRANSB = 'T' or 't',  op( B ) = B'.
    !
    !              TRANSB = 'C' or 'c',  op( B ) = B'.
    !
    !           Unchanged on exit.
    !
    !  M      - integer.
    !           On entry,  M  specifies  the number  of rows  of the  matrix
    !           op( A )  and of the  matrix  C.  M  must  be at least  zero.
    !           Unchanged on exit.
    !
    !  N      - integer.
    !           On entry,  N  specifies the number  of columns of the matrix
    !           op( B ) and the number of columns of the matrix C. N must be
    !           at least zero.
    !           Unchanged on exit.
    !
    !  K      - integer.
    !           On entry,  K  specifies  the number of columns of the matrix
    !           op( A ) and the number of rows of the matrix op( B ). K must
    !           be at least  zero.
    !           Unchanged on exit.
    !
    !  ALPHA  - real*8.
    !           On entry, ALPHA specifies the scalar alpha.
    !           Unchanged on exit.
    !
    !  A      - real*8 array of DIMENSION ( LDA, ka ), where ka is
    !           k  when  TRANSA = 'N' or 'n',  and is  m  otherwise.
    !           Before entry with  TRANSA = 'N' or 'n',  the leading  m by k
    !           part of the array  A  must contain the matrix  A,  otherwise
    !           the leading  k by m  part of the array  A  must contain  the
    !           matrix A.
    !           Unchanged on exit.
    !
    !  LDA    - integer.
    !           On entry, LDA specifies the first dimension of A as declared
    !           in the calling (sub) program. When  TRANSA = 'N' or 'n' then
    !           LDA must be at least  max( 1, m ), otherwise  LDA must be at
    !           least  max( 1, k ).
    !           Unchanged on exit.
    !
    !  B      - real*8 array of DIMENSION ( LDB, kb ), where kb is
    !           n  when  TRANSB = 'N' or 'n',  and is  k  otherwise.
    !           Before entry with  TRANSB = 'N' or 'n',  the leading  k by n
    !           part of the array  B  must contain the matrix  B,  otherwise
    !           the leading  n by k  part of the array  B  must contain  the
    !           matrix B.
    !           Unchanged on exit.
    !
    !  LDB    - integer.
    !           On entry, LDB specifies the first dimension of B as declared
    !           in the calling (sub) program. When  TRANSB = 'N' or 'n' then
    !           LDB must be at least  max( 1, k ), otherwise  LDB must be at
    !           least  max( 1, n ).
    !           Unchanged on exit.
    !
    !  BETA   - real*8.
    !           On entry,  BETA  specifies the scalar  beta.  When  BETA  is
    !           supplied as zero then C need not be set on input.
    !           Unchanged on exit.
    !
    !  C      - real*8 array of DIMENSION ( LDC, n ).
    !           Before entry, the leading  m by n  part of the array  C must
    !           contain the matrix  C,  except when  beta  is zero, in which
    !           case C need not be set on entry.
    !           On exit, the array  C  is overwritten by the  m by n  matrix
    !           ( alpha*op( A )*op( B ) + beta*C ).
    !
    !  LDC    - integer.
    !           On entry, LDC specifies the first dimension of C as declared
    !           in  the  calling  (sub)  program.   LDC  must  be  at  least
    !           max( 1, m ).
    !           Unchanged on exit.
    !
    !
    !  Level 3 Blas routine.
    !
    !  -- Written on 8-February-1989.
    !     Jack Dongarra, Argonne National Laboratory.
    !     Iain Duff, AERE Harwell.
    !     Jeremy Du Croz, Numerical Algorithms Group Ltd.
    !     Sven Hammarling, Numerical Algorithms Group Ltd.
    !
    !

    !--------------------------------------------------------------------------
    logical            NOTA, NOTB
    integer ::            I, INFO, J, L, NCOLA, NROWA, NROWB
    real*8 ::   TEMP
    !     .. parameters ..
    real*8 ::   ONE    , ZERO
    parameter        ( ONE = 1, ZERO = 0 )
    !     ..
    !     .. Executable Statements ..
    !
    !     Set  NOTA  and  NOTB  as  true if  A  and  B  respectively are not
    !     transposed and set  NROWA, NCOLA and  NROWB  as the number of rows
    !     and  columns of  A  and the  number of  rows  of  B  respectively.
    !
    NOTA  = LSAME( TRANSA, 'N' )
    NOTB  = LSAME( TRANSB, 'N' )
    if( NOTA )then
       NROWA = M
       NCOLA = K
    else
       NROWA = K
       NCOLA = M
    end if
    if( NOTB )then
       NROWB = K
    else
       NROWB = N
    end if
    !
    !     Test the input parameters.
    !
    INFO = 0
    if(      ( .not.NOTA                 ).and. &
         ( .not.LSAME( TRANSA, 'C' ) ).and. &
         ( .not.LSAME( TRANSA, 'T' ) )      )then
       INFO = 1
    else if( ( .not.NOTB                 ).and. &
         ( .not.LSAME( TRANSB, 'C' ) ).and. &
         ( .not.LSAME( TRANSB, 'T' ) )      )then
       INFO = 2
    else if( M < 0               )then
       INFO = 3
    else if( N < 0               )then
       INFO = 4
    else if( K < 0               )then
       INFO = 5
    else if( LDA < MAX( 1, NROWA ) )then
       INFO = 8
    else if( LDB < MAX( 1, NROWB ) )then
       INFO = 10
    else if( LDC < MAX( 1, M     ) )then
       INFO = 13
    end if
    if( INFO /= 0 )then
       CALL xerbla( 'DGEMM ', INFO )
       RETURN
    end if
    !
    !     Quick return if possible.
    !
    if( ( M == 0 ).OR.( N == 0 ).OR. &
         ( ( ( ALPHA == ZERO ).OR.( K == 0 ) ).and.( BETA == ONE ) ) ) &
         RETURN
    !
    !     And if  alpha == zero.
    !
    if( ALPHA == ZERO )then
       if( BETA == ZERO )then
          do J =  1, N
             do I =  1, M
                C( I, J ) = ZERO
             end do
          end do
       else
          do J =  1, N
             do I =  1, M
                C( I, J ) = BETA*C( I, J )
             end do
          end do
       end if
       RETURN
    end if
    !
    !     Start the operations.
    !
    if( NOTB )then
       if( NOTA )then
          !
          !           Form  C := alpha*A*B + beta*C.
          !
          do J =  1, N
             if( BETA == ZERO )then
                do I =  1, M
                   C( I, J ) = ZERO
                end do
             else if( BETA /= ONE )then
                do I =  1, M
                   C( I, J ) = BETA*C( I, J )
                end do
             end if
             do  L = 1, K
                if( B( L, J ) /= ZERO )then
                   TEMP = ALPHA*B( L, J )
                   do I =  1, M
                      C( I, J ) = C( I, J ) + TEMP*A( I, L )
                   end do
                end if
             end do
          end do
       else
          !
          !           Form  C := alpha*A'*B + beta*C
          !
          do J =  1, N
             do I =  1, M
                TEMP = ZERO
                do L = 1, K
                   TEMP = TEMP + A( L, I )*B( L, J )
                end do
                if( BETA == ZERO )then
                   C( I, J ) = ALPHA*TEMP
                else
                   C( I, J ) = ALPHA*TEMP + BETA*C( I, J )
                end if
             end do
          end do
       end if
    else
       if( NOTA )then
          !
          !           Form  C := alpha*A*B' + beta*C
          !
          do J =  1, N
             if( BETA == ZERO )then
                do I =  1, M
                   C( I, J ) = ZERO
                end do
             else if( BETA /= ONE )then
                do I =  1, M
                   C( I, J ) = BETA*C( I, J )
                end do
             end if
             do L = 1, K
                if( B( J, L ) /= ZERO )then
                   TEMP = ALPHA*B( J, L )
                   do I =  1, M
                      C( I, J ) = C( I, J ) + TEMP*A( I, L )
                   end do
                end if
             end do
          end do
       else
          !
          !           Form  C := alpha*A'*B' + beta*C
          !
          do J =  1, N
             do I =  1, M
                TEMP = ZERO
                do L = 1, K
                   TEMP = TEMP + A( L, I )*B( J, L )
                end do
                if( BETA == ZERO )then
                   C( I, J ) = ALPHA*TEMP
                else
                   C( I, J ) = ALPHA*TEMP + BETA*C( I, J )
                end if
             end do
          end do
       end if
    end if
    !
    RETURN
    !
    !     End of DGEMM .
    !
  end subroutine DGEMM
  !============================================================================
  subroutine DGEMV ( TRANS, M, N, ALPHA, A, LDA, X, INCX, &
       BETA, Y, INCY )
    !$acc routine seq
    !     .. Scalar Arguments ..
    real*8 ::   ALPHA, BETA
    integer ::            INCX, INCY, LDA, M, N
    character ::        TRANS
    !     .. Array Arguments ..
    real*8 ::   A( LDA, * ), X( * ), Y( * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  DGEMV  performs one of the matrix-vector operations
    !
    !     y := alpha*A*x + beta*y,   or   y := alpha*A'*x + beta*y,
    !
    !  where alpha and beta are scalars, x and y are vectors and A is an
    !  m by n matrix.
    !
    !  parameters
    !  ==========
    !
    !  TRANS  - character.
    !           On entry, TRANS specifies the operation to be performed as
    !           follows:
    !
    !              TRANS = 'N' or 'n'   y := alpha*A*x + beta*y.
    !
    !              TRANS = 'T' or 't'   y := alpha*A'*x + beta*y.
    !
    !              TRANS = 'C' or 'c'   y := alpha*A'*x + beta*y.
    !
    !           Unchanged on exit.
    !
    !  M      - integer.
    !           On entry, M specifies the number of rows of the matrix A.
    !           M must be at least zero.
    !           Unchanged on exit.
    !
    !  N      - integer.
    !           On entry, N specifies the number of columns of the matrix A.
    !           N must be at least zero.
    !           Unchanged on exit.
    !
    !  ALPHA  - real*8.
    !           On entry, ALPHA specifies the scalar alpha.
    !           Unchanged on exit.
    !
    !  A      - real*8 array of DIMENSION ( LDA, n ).
    !           Before entry, the leading m by n part of the array A must
    !           contain the matrix of coefficients.
    !           Unchanged on exit.
    !
    !  LDA    - integer.
    !           On entry, LDA specifies the first dimension of A as declared
    !           in the calling (sub) program. LDA must be at least
    !           max( 1, m ).
    !           Unchanged on exit.
    !
    !  X      - real*8 array of DIMENSION at least
    !           ( 1 + ( n - 1 )*abs( INCX ) ) when TRANS = 'N' or 'n'
    !           and at least
    !           ( 1 + ( m - 1 )*abs( INCX ) ) otherwise.
    !           Before entry, the incremented array X must contain the
    !           vector x.
    !           Unchanged on exit.
    !
    !  INCX   - integer.
    !           On entry, INCX specifies the increment for the elements of
    !           X. INCX must not be zero.
    !           Unchanged on exit.
    !
    !  BETA   - real*8.
    !           On entry, BETA specifies the scalar beta. When BETA is
    !           supplied as zero then Y need not be set on input.
    !           Unchanged on exit.
    !
    !  Y      - real*8 array of DIMENSION at least
    !           ( 1 + ( m - 1 )*abs( INCY ) ) when TRANS = 'N' or 'n'
    !           and at least
    !           ( 1 + ( n - 1 )*abs( INCY ) ) otherwise.
    !           Before entry with BETA non-zero, the incremented array Y
    !           must contain the vector y. On exit, Y is overwritten by the
    !           updated vector y.
    !
    !  INCY   - integer.
    !           On entry, INCY specifies the increment for the elements of
    !           Y. INCY must not be zero.
    !           Unchanged on exit.
    !
    !
    !  Level 2 Blas routine.
    !
    !  -- Written on 22-October-1986.
    !     Jack Dongarra, Argonne National Lab.
    !     Jeremy Du Croz, Nag Central Office.
    !     Sven Hammarling, Nag Central Office.
    !     Richard Hanson, Sandia National Labs.
    !
    !
    !     .. parameters ..
    real*8 ::   ONE         , ZERO
    !--------------------------------------------------------------------------
    parameter        ( ONE = 1, ZERO = 0 )
    !     .. Local Scalars ..
    real*8 ::   TEMP
    integer ::            I, INFO, IX, IY, J, JX, JY, KX, KY, LENX, LENY

    INFO = 0
    if     ( .not.LSAME( TRANS, 'N' ).and. &
         .not.LSAME( TRANS, 'T' ).and. &
         .not.LSAME( TRANS, 'C' )      )then
       INFO = 1
    else if( M < 0 )then
       INFO = 2
    else if( N < 0 )then
       INFO = 3
    else if( LDA < MAX( 1, M ) )then
       INFO = 6
    else if( INCX == 0 )then
       INFO = 8
    else if( INCY == 0 )then
       INFO = 11
    end if
    if( INFO /= 0 )then
       CALL xerbla( 'DGEMV ', INFO )
       RETURN
    end if
    !
    !     Quick return if possible.
    !
    if( ( M == 0 ).OR.( N == 0 ).OR. &
         ( ( ALPHA == ZERO ).and.( BETA == ONE ) ) ) &
         RETURN
    !
    !     Set  LENX  and  LENY, the lengths of the vectors x and y, and set
    !     up the start points in  X  and  Y.
    !
    if( LSAME( TRANS, 'N' ) )then
       LENX = N
       LENY = M
    else
       LENX = M
       LENY = N
    end if
    if( INCX > 0 )then
       KX = 1
    else
       KX = 1 - ( LENX - 1 )*INCX
    end if
    if( INCY > 0 )then
       KY = 1
    else
       KY = 1 - ( LENY - 1 )*INCY
    end if
    !
    !     Start the operations. In this version the elements of A are
    !     accessed sequentially with one pass through A.
    !
    !     First form  y := beta*y.
    !
    if( BETA /= ONE )then
       if( INCY == 1 )then
          if( BETA == ZERO )then
             do I =  1, LENY
                Y( I ) = ZERO
             end do
          else
             do I =  1, LENY
                Y( I ) = BETA*Y( I )
             end do
          end if
       else
          IY = KY
          if( BETA == ZERO )then
             do I =  1, LENY
                Y( IY ) = ZERO
                IY      = IY   + INCY
             end do
          else
             do I =  1, LENY
                Y( IY ) = BETA*Y( IY )
                IY      = IY           + INCY
             end do
          end if
       end if
    end if
    if( ALPHA == ZERO ) &
         RETURN
    if( LSAME( TRANS, 'N' ) )then
       !
       !        Form  y := alpha*A*x + y.
       !
       JX = KX
       if( INCY == 1 )then
          do J =  1, N
             if( X( JX ) /= ZERO )then
                TEMP = ALPHA*X( JX )
                do I =  1, M
                   Y( I ) = Y( I ) + TEMP*A( I, J )
                end do
             end if
             JX = JX + INCX
          end do
       else
          do J =  1, N
             if( X( JX ) /= ZERO )then
                TEMP = ALPHA*X( JX )
                IY   = KY
                do I =  1, M
                   Y( IY ) = Y( IY ) + TEMP*A( I, J )
                   IY      = IY      + INCY
                end do
             end if
             JX = JX + INCX
          end do
       end if
    else
       !
       !        Form  y := alpha*A'*x + y.
       !
       JY = KY
       if( INCX == 1 )then
          do J =  1, N
             TEMP = ZERO
             do I =  1, M
                TEMP = TEMP + A( I, J )*X( I )
             end do
             Y( JY ) = Y( JY ) + ALPHA*TEMP
             JY      = JY      + INCY
          end do
       else
          do J =  1, N
             TEMP = ZERO
             IX   = KX
             do I =  1, M
                TEMP = TEMP + A( I, J )*X( IX )
                IX   = IX   + INCX
             end do
             Y( JY ) = Y( JY ) + ALPHA*TEMP
             JY      = JY      + INCY
          end do
       end if
    end if
    !
    RETURN
    !
    !     End of DGEMV .
    !
  end subroutine DGEMV
  !============================================================================
  subroutine DGER  ( M, N, ALPHA, X, INCX, Y, INCY, A, LDA )
    !$acc routine seq
    !     .. Scalar Arguments ..
    real*8 ::   ALPHA
    integer ::            INCX, INCY, LDA, M, N
    !     .. Array Arguments ..
    real*8 ::   A( LDA, * ), X( * ), Y( * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  DGER   performs the rank 1 operation
    !
    !     A := alpha*x*y' + A,
    !
    !  where alpha is a scalar, x is an m element vector, y is an n element
    !  vector and A is an m by n matrix.
    !
    !  parameters
    !  ==========
    !
    !  M      - integer.
    !           On entry, M specifies the number of rows of the matrix A.
    !           M must be at least zero.
    !           Unchanged on exit.
    !
    !  N      - integer.
    !           On entry, N specifies the number of columns of the matrix A.
    !           N must be at least zero.
    !           Unchanged on exit.
    !
    !  ALPHA  - real*8.
    !           On entry, ALPHA specifies the scalar alpha.
    !           Unchanged on exit.
    !
    !  X      - real*8 array of dimension at least
    !           ( 1 + ( m - 1 )*abs( INCX ) ).
    !           Before entry, the incremented array X must contain the m
    !           element vector x.
    !           Unchanged on exit.
    !
    !  INCX   - integer.
    !           On entry, INCX specifies the increment for the elements of
    !           X. INCX must not be zero.
    !           Unchanged on exit.
    !
    !  Y      - real*8 array of dimension at least
    !           ( 1 + ( n - 1 )*abs( INCY ) ).
    !           Before entry, the incremented array Y must contain the n
    !           element vector y.
    !           Unchanged on exit.
    !
    !  INCY   - integer.
    !           On entry, INCY specifies the increment for the elements of
    !           Y. INCY must not be zero.
    !           Unchanged on exit.
    !
    !  A      - real*8 array of DIMENSION ( LDA, n ).
    !           Before entry, the leading m by n part of the array A must
    !           contain the matrix of coefficients. On exit, A is
    !           overwritten by the updated matrix.
    !
    !  LDA    - integer.
    !           On entry, LDA specifies the first dimension of A as declared
    !           in the calling (sub) program. LDA must be at least
    !           max( 1, m ).
    !           Unchanged on exit.
    !
    !
    !  Level 2 Blas routine.
    !
    !  -- Written on 22-October-1986.
    !     Jack Dongarra, Argonne National Lab.
    !     Jeremy Du Croz, Nag Central Office.
    !     Sven Hammarling, Nag Central Office.
    !     Richard Hanson, Sandia National Labs.
    !
    !
    !     .. parameters ..
    real*8 ::   ZERO
    !--------------------------------------------------------------------------
    parameter        ( ZERO = 0.0D+0 )
    !     .. Local Scalars ..
    real*8 ::   TEMP
    integer ::            I, INFO, IX, J, JY, KX

    INFO = 0
    if     ( M < 0 )then
       INFO = 1
    else if( N < 0 )then
       INFO = 2
    else if( INCX == 0 )then
       INFO = 5
    else if( INCY == 0 )then
       INFO = 7
    else if( LDA < MAX( 1, M ) )then
       INFO = 9
    end if
    if( INFO /= 0 )then
       CALL xerbla( 'DGER  ', INFO )
       RETURN
    end if
    !
    !     Quick return if possible.
    !
    if( ( M == 0 ).OR.( N == 0 ).OR.( ALPHA == ZERO ) ) &
         RETURN
    !
    !     Start the operations. In this version the elements of A are
    !     accessed sequentially with one pass through A.
    !
    if( INCY > 0 )then
       JY = 1
    else
       JY = 1 - ( N - 1 )*INCY
    end if
    if( INCX == 1 )then
       do J =  1, N
          if( Y( JY ) /= ZERO )then
             TEMP = ALPHA*Y( JY )
             do I =  1, M
                A( I, J ) = A( I, J ) + X( I )*TEMP
             end do
          end if
          JY = JY + INCY
       end do
    else
       if( INCX > 0 )then
          KX = 1
       else
          KX = 1 - ( M - 1 )*INCX
       end if
       do J =  1, N
          if( Y( JY ) /= ZERO )then
             TEMP = ALPHA*Y( JY )
             IX   = KX
             do I =  1, M
                A( I, J ) = A( I, J ) + X( IX )*TEMP
                IX        = IX        + INCX
             end do
          end if
          JY = JY + INCY
       end do
    end if
    !
    RETURN
    !
    !     End of DGER  .
    !
  end subroutine DGER
  !============================================================================
  subroutine  dscal(n,da,dx,incx)
    !$acc routine seq
    !
    !     scales a vector by a constant.
    !     uses unrolled loops for increment equal to one.
    !     jack dongarra, linpack, 3/11/78.
    !     modified 3/93 to return if incx .le. 0.
    !
    real*8 :: da,dx(*)
    integer :: i,incx,m,mp1,n,nincx
    !
    !--------------------------------------------------------------------------
    if( n <= 0 .or. incx <= 0 )RETURN
    nincx = n*incx
    do i =  1,nincx,incx
       dx(i) = da*dx(i)
    end do
    RETURN
  end subroutine dscal
  !============================================================================
  subroutine  dswap (n,dx,incx,dy,incy)
    !$acc routine seq
    !
    !     interchanges two vectors.
    !     uses unrolled loops for increments equal one.
    !     jack dongarra, linpack, 3/11/78.
    !
    real*8 :: dx(*),dy(*),dtemp
    integer :: i,incx,incy,ix,iy,m,mp1,n
    !
    !--------------------------------------------------------------------------
    if(n <= 0)RETURN

    ix = 1
    iy = 1
    if(incx < 0)ix = (-n+1)*incx + 1
    if(incy < 0)iy = (-n+1)*incy + 1
    do i =  1,n
       dtemp = dx(ix)
       dx(ix) = dy(iy)
       dy(iy) = dtemp
       ix = ix + incx
       iy = iy + incy
    end do
    RETURN
  end subroutine dswap
  !============================================================================
  subroutine DTRSM ( SIDE, UPLO, TRANSA, DIAG, M, N, ALPHA, A, LDA, &
       B, LDB )
    !$acc routine seq
    !     .. Scalar Arguments ..
    character ::        SIDE, UPLO, TRANSA, DIAG
    integer ::            M, N, LDA, LDB
    real*8 ::   ALPHA
    !     .. Array Arguments ..
    real*8 ::   A( LDA, * ), B( LDB, * )
    !     ..
    !
    !  Purpose
    !  =======
    !
    !  DTRSM  solves one of the matrix equations
    !
    !     op( A )*X = alpha*B,   or   X*op( A ) = alpha*B,
    !
    !  where alpha is a scalar, X and B are m by n matrices, A is a unit, or
    !  non-unit,  upper or lower triangular matrix  and  op( A )  is one  of
    !
    !     op( A ) = A   or   op( A ) = A'.
    !
    !  The matrix X is overwritten on B.
    !
    !  parameters
    !  ==========
    !
    !  SIDE   - character.
    !           On entry, SIDE specifies whether op( A ) appears on the left
    !           or right of X as follows:
    !
    !              SIDE = 'L' or 'l'   op( A )*X = alpha*B.
    !
    !              SIDE = 'R' or 'r'   X*op( A ) = alpha*B.
    !
    !           Unchanged on exit.
    !
    !  UPLO   - character.
    !           On entry, UPLO specifies whether the matrix A is an upper or
    !           lower triangular matrix as follows:
    !
    !              UPLO = 'U' or 'u'   A is an upper triangular matrix.
    !
    !              UPLO = 'L' or 'l'   A is a lower triangular matrix.
    !
    !           Unchanged on exit.
    !
    !  TRANSA - character.
    !           On entry, TRANSA specifies the form of op( A ) to be used in
    !           the matrix multiplication as follows:
    !
    !              TRANSA = 'N' or 'n'   op( A ) = A.
    !
    !              TRANSA = 'T' or 't'   op( A ) = A'.
    !
    !              TRANSA = 'C' or 'c'   op( A ) = A'.
    !
    !           Unchanged on exit.
    !
    !  DIAG   - character.
    !           On entry, DIAG specifies whether or not A is unit triangular
    !           as follows:
    !
    !              DIAG = 'U' or 'u'   A is assumed to be unit triangular.
    !
    !              DIAG = 'N' or 'n'   A is not assumed to be unit
    !                                  triangular.
    !
    !           Unchanged on exit.
    !
    !  M      - integer.
    !           On entry, M specifies the number of rows of B. M must be at
    !           least zero.
    !           Unchanged on exit.
    !
    !  N      - integer.
    !           On entry, N specifies the number of columns of B.  N must be
    !           at least zero.
    !           Unchanged on exit.
    !
    !  ALPHA  - real*8.
    !           On entry,  ALPHA specifies the scalar  alpha. When  alpha is
    !           zero then  A is not referenced and  B need not be set before
    !           entry.
    !           Unchanged on exit.
    !
    !  A      - real*8 array of DIMENSION ( LDA, k ), where k is m
    !           when  SIDE = 'L' or 'l'  and is  n  when  SIDE = 'R' or 'r'.
    !           Before entry  with  UPLO = 'U' or 'u',  the  leading  k by k
    !           upper triangular part of the array  A must contain the upper
    !           triangular matrix  and the strictly lower triangular part of
    !           A is not referenced.
    !           Before entry  with  UPLO = 'L' or 'l',  the  leading  k by k
    !           lower triangular part of the array  A must contain the lower
    !           triangular matrix  and the strictly upper triangular part of
    !           A is not referenced.
    !           Note that when  DIAG = 'U' or 'u',  the diagonal elements of
    !           A  are not referenced either,  but are assumed to be  unity.
    !           Unchanged on exit.
    !
    !  LDA    - integer.
    !           On entry, LDA specifies the first dimension of A as declared
    !           in the calling (sub) program.  When  SIDE = 'L' or 'l'  then
    !           LDA  must be at least  max( 1, m ),  when  SIDE = 'R' or 'r'
    !           then LDA must be at least max( 1, n ).
    !           Unchanged on exit.
    !
    !  B      - real*8 array of DIMENSION ( LDB, n ).
    !           Before entry,  the leading  m by n part of the array  B must
    !           contain  the  right-hand  side  matrix  B,  and  on exit  is
    !           overwritten by the solution matrix  X.
    !
    !  LDB    - integer.
    !           On entry, LDB specifies the first dimension of B as declared
    !           in  the  calling  (sub)  program.   LDB  must  be  at  least
    !           max( 1, m ).
    !           Unchanged on exit.
    !
    !
    !  Level 3 Blas routine.
    !
    !
    !  -- Written on 8-February-1989.
    !     Jack Dongarra, Argonne National Laboratory.
    !     Iain Duff, AERE Harwell.
    !     Jeremy Du Croz, Numerical Algorithms Group Ltd.
    !     Sven Hammarling, Numerical Algorithms Group Ltd.
    !
    !
    !     .. Local Scalars ..
    !--------------------------------------------------------------------------
    logical            LSIDE, NOUNIT, UPPER
    integer ::            I, INFO, J, K, NROWA
    real*8 ::             TEMP
    !     .. parameters ..
    real*8 ::             ONE    , ZERO
    parameter        ( ONE = 1, ZERO = 0 )
    !     ..
    !     .. Executable Statements ..
    !
    !     Test the input parameters.
    !
    LSIDE  = LSAME( SIDE  , 'L' )
    if( LSIDE )then
       NROWA = M
    else
       NROWA = N
    end if
    NOUNIT = LSAME( DIAG  , 'N' )
    UPPER  = LSAME( UPLO  , 'U' )
    !
    INFO   = 0
    if(      ( .not.LSIDE                ).and. &
         ( .not.LSAME( SIDE  , 'R' ) )      )then
       INFO = 1
    else if( ( .not.UPPER                ).and. &
         ( .not.LSAME( UPLO  , 'L' ) )      )then
       INFO = 2
    else if( ( .not.LSAME( TRANSA, 'N' ) ).and. &
         ( .not.LSAME( TRANSA, 'T' ) ).and. &
         ( .not.LSAME( TRANSA, 'C' ) )      )then
       INFO = 3
    else if( ( .not.LSAME( DIAG  , 'U' ) ).and. &
         ( .not.LSAME( DIAG  , 'N' ) )      )then
       INFO = 4
    else if( M < 0               )then
       INFO = 5
    else if( N < 0               )then
       INFO = 6
    else if( LDA < MAX( 1, NROWA ) )then
       INFO = 9
    else if( LDB < MAX( 1, M     ) )then
       INFO = 11
    end if
    if( INFO /= 0 )then
       CALL xerbla( 'DTRSM ', INFO )
       RETURN
    end if
    !
    !     Quick return if possible.
    !
    if( N == 0 ) &
         RETURN
    !
    !     And when  alpha == zero.
    !
    if( ALPHA == ZERO )then
       do J =  1, N
          do I =  1, M
             B( I, J ) = ZERO
          end do
       end do
       RETURN
    end if
    !
    !     Start the operations.
    !
    if( LSIDE )then
       if( LSAME( TRANSA, 'N' ) )then
          !
          !           Form  B := alpha*inv( A )*B.
          !
          if( UPPER )then
             do J =  1, N
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, J ) = ALPHA*B( I, J )
                   end do
                end if
                do K =  M, 1, -1
                   if( B( K, J ) /= ZERO )then
                      if( NOUNIT ) &
                           B( K, J ) = B( K, J )/A( K, K )
                      do I =  1, K - 1
                         B( I, J ) = B( I, J ) - B( K, J )*A( I, K )
                      end do
                   end if
                end do
             end do
          else
             do J =  1, N
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, J ) = ALPHA*B( I, J )
                   end do
                end if
                do K =  1, M
                   if( B( K, J ) /= ZERO )then
                      if( NOUNIT ) &
                           B( K, J ) = B( K, J )/A( K, K )
                      do I =  K + 1, M
                         B( I, J ) = B( I, J ) - B( K, J )*A( I, K )
                      end do
                   end if
                end do
             end do
          end if
       else
          !
          !           Form  B := alpha*inv( A' )*B.
          !
          if( UPPER )then
             do J =  1, N
                do I =  1, M
                   TEMP = ALPHA*B( I, J )
                   do K =  1, I - 1
                      TEMP = TEMP - A( K, I )*B( K, J )
                   end do
                   if( NOUNIT ) &
                        TEMP = TEMP/A( I, I )
                   B( I, J ) = TEMP
                end do
             end do
          else
             do J =  1, N
                do I =  M, 1, -1
                   TEMP = ALPHA*B( I, J )
                   do K =  I + 1, M
                      TEMP = TEMP - A( K, I )*B( K, J )
                   end do
                   if( NOUNIT ) &
                        TEMP = TEMP/A( I, I )
                   B( I, J ) = TEMP
                end do
             end do
          end if
       end if
    else
       if( LSAME( TRANSA, 'N' ) )then
          !
          !           Form  B := alpha*B*inv( A ).
          !
          if( UPPER )then
             do J =  1, N
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, J ) = ALPHA*B( I, J )
                   end do
                end if
                do K =  1, J - 1
                   if( A( K, J ) /= ZERO )then
                      do I =  1, M
                         B( I, J ) = B( I, J ) - A( K, J )*B( I, K )
                      end do
                   end if
                end do
                if( NOUNIT )then
                   TEMP = ONE/A( J, J )
                   do I =  1, M
                      B( I, J ) = TEMP*B( I, J )
                   end do
                end if
             end do
          else
             do J =  N, 1, -1
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, J ) = ALPHA*B( I, J )
                   end do
                end if
                do K =  J + 1, N
                   if( A( K, J ) /= ZERO )then
                      do I =  1, M
                         B( I, J ) = B( I, J ) - A( K, J )*B( I, K )
                      end do
                   end if
                end do
                if( NOUNIT )then
                   TEMP = ONE/A( J, J )
                   do I =  1, M
                      B( I, J ) = TEMP*B( I, J )
                   end do
                end if
             end do
          end if
       else
          !
          !           Form  B := alpha*B*inv( A' ).
          !
          if( UPPER )then
             do K =  N, 1, -1
                if( NOUNIT )then
                   TEMP = ONE/A( K, K )
                   do I =  1, M
                      B( I, K ) = TEMP*B( I, K )
                   end do
                end if
                do J =  1, K - 1
                   if( A( J, K ) /= ZERO )then
                      TEMP = A( J, K )
                      do I =  1, M
                         B( I, J ) = B( I, J ) - TEMP*B( I, K )
                      end do
                   end if
                end do
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, K ) = ALPHA*B( I, K )
                   end do
                end if
             end do
          else
             do K =  1, N
                if( NOUNIT )then
                   TEMP = ONE/A( K, K )
                   do I =  1, M
                      B( I, K ) = TEMP*B( I, K )
                   end do
                end if
                do J =  K + 1, N
                   if( A( J, K ) /= ZERO )then
                      TEMP = A( J, K )
                      do I =  1, M
                         B( I, J ) = B( I, J ) - TEMP*B( I, K )
                      end do
                   end if
                end do
                if( ALPHA /= ONE )then
                   do I =  1, M
                      B( I, K ) = ALPHA*B( I, K )
                   end do
                end if
             end do
          end if
       end if
    end if
    !
    RETURN
    !
    !     End of DTRSM .
    !
  end subroutine DTRSM
  !============================================================================
  integer function idamax(n,dx,incx)
    !$acc routine seq
    !
    !     finds the index of element having max. absolute value.
    !     jack dongarra, linpack, 3/11/78.
    !     modified 3/93 to return if incx .le. 0.
    !
    real*8 ::  dx(*),dmax
    integer :: i,incx,ix,n
    !
    !--------------------------------------------------------------------------
    idamax = 0
    if( n < 1 .or. incx <= 0 ) RETURN
    idamax = 1
    if(n == 1)RETURN

    ix = 1
    dmax = dabs(dx(1))
    ix = ix + incx
    do i =  2,n
       if(.not. (dabs(dx(ix)) <= dmax) ) then
          idamax = i
          dmax = dabs(dx(ix))
       end if
       ix = ix + incx
    end do
    RETURN
  end function idamax
  !============================================================================

end module ModBlasLapack
!==============================================================================
