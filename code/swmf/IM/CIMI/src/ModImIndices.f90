module ModImIndices
  implicit none
  save
  private
  
  logical,public :: UseKpApF107IndicesFile = .false.
  logical,public :: UseDstKyoto=.false.,UseAeKyoto=.false.
  
  !vars to hold file data
  real,    allocatable :: Time_I(:)
  real,    allocatable :: Ap_I(:),ApDaily_I(:),Kp_I(:),F107_I(:)

  real :: StartTime
  
  !size of data arrays (8*(filelength-header))
  integer :: nData
  
  integer, parameter :: Year_=1,Month_=2,Day_=3,Hour_=4,Minute_=5,Second_=6


  ! For ae index
  real, allocatable :: TimeAeIndex_I(:), AeIndex_I(:)
  character(len=100),public :: NameAeFile
  integer :: NumAeElements

  ! For dst index
  real, allocatable :: TimeDstIndex_I(:), DstIndex_I(:)
  character(len=100),public :: NameDstFile
  integer :: NumDstElements

  
  !public routines
  public :: read_kpapf107_indices_file
  public :: get_im_indices_F107
  public :: get_im_indices_ap
  public :: get_im_indices_Kp
  public :: get_im_indices_F107A
  public :: read_ae_wdc_kyoto
  public :: interpolate_ae
  public :: read_dst_wdc_kyoto
  public :: interpolate_dst
  
contains
  subroutine read_kpapf107_indices_file
    use ModTimeConvert, ONLY: time_int_to_real,time_real_to_int
    use ModIoUnit, ONLY: UnitTmp_
    use ModUtilities,ONLY: CON_stop
    use ModConst, ONLY: iYearBase
    
    !file names
    Character(len=100) :: NameFile='IM/IndicesKpApF107.dat'
    Character(len=200) :: header

    integer :: iTime_I(7)
    
    integer, parameter :: nHeader = 40
    integer :: nLines,iStatus,iLine, iData, iRecord
    integer :: iMlt, iLat

    !file data record var entries
    integer :: YYY, MM, DD,  days
    real :: days_m
    integer :: Bsr, dB
    real :: Kp1,    Kp2,    Kp3,    Kp4,    Kp5,    Kp6,    Kp7,    Kp8 
    integer :: ap1,  ap2,  ap3,  ap4,  ap5,  ap6,  ap7,  ap8,    Ap, SN
    real :: F107obs, F107adj
    integer :: Definitive !1 is preliminary, 2 is definitive
    !---------------------------------------------------------------------------
        
    !open file and get number of lines
    open(UnitTmp_,file=NameFile)
    nLines = 0
    do    
       read(UnitTmp_,*,iostat=iStatus) 
       if (iStatus/=0) exit
       nLines = nLines + 1
    enddo
    close (UnitTmp_)

    !find how many records have year greater than iYearBase
    iRecord=0
    open(UnitTmp_,file=NameFile)
    do iLine = 1, nLines
       if (iLine <= nHeader) then
          !read and discard header
          read(UnitTmp_,"(a)") header
       else
          read(UnitTmp_,*) YYY, MM, DD,  days,  days_m,  Bsr, dB,&
               Kp1,    Kp2,    Kp3,    Kp4,    Kp5,    Kp6,    Kp7,    Kp8,  &
               ap1,  ap2,  ap3,  ap4,  ap5,  ap6,  ap7,  ap8,    Ap,&
               SN, F107obs, F107adj, Definitive
          if(YYY>iYearBase) iRecord=iRecord+1
       endif
    enddo
    close (UnitTmp_)
    !allocate arrays to hold data. Note that each entry in the file is one day
    !with 8 values of ap and Kp. So data entries must be 8 times the number of
    !records with year above iYearBase
    nData = 8*iRecord
    if (.not.allocated(Time_I)) then
       allocate(Time_I(nData),Ap_I(nData),ApDaily_I(nData),&
            Kp_I(nData),F107_I(nData))
    else
       write(*,*) 'IM Warning: trying to read ImIndices a second time'
    endif
    
    !open file for reading 
    open(UnitTmp_,file=NameFile)

    iData=0
    do iLine = 1, nLines
       if (iLine <= nHeader) then
          !read and discard header
          read(UnitTmp_,"(a)") header
       else
          read(UnitTmp_,*) YYY, MM, DD,  days,  days_m,  Bsr, dB,&
               Kp1,    Kp2,    Kp3,    Kp4,    Kp5,    Kp6,    Kp7,    Kp8,  &
               ap1,  ap2,  ap3,  ap4,  ap5,  ap6,  ap7,  ap8,    Ap,&
               SN, F107obs, F107adj, Definitive
          !write(*,*) YYY, MM, DD,  days,  days_m,  Bsr, dB,&
          !     Kp1,    Kp2,    Kp3,    Kp4,    Kp5,    Kp6,    Kp7,    Kp8,  &
          !     ap1,  ap2,  ap3,  ap4,  ap5,  ap6,  ap7,  ap8,    Ap,&
          !     SN, F107obs, F107adj, Definitive
          !stop
          !set time for record and data
          if (YYY>iYearBase) then
             iTime_I(Year_) = YYY
             iTime_I(Month_) = MM
             iTime_I(Day_) = DD
             iTime_I(Hour_) = 1
             iTime_I(Minute_) = 30
             iTime_I(Second_) = 0
             iTime_I(7) = 0
             iData=iData+1
             call time_int_to_real(iTime_I,Time_I(iData))
             Ap_I(iData) = real(ap1)
             ApDaily_I(iData) = real(Ap)
             Kp_I(iData)  = Kp1
             F107_I(iData) = F107obs !could also use adj
             
             iData=iData+1
             iTime_I(Hour_) = 4
             iTime_I(Minute_) = 30
             call time_int_to_real(iTime_I,Time_I(iData))
             Ap_I(iData) = real(ap2)
             ApDaily_I(iData) = real(Ap)
             Kp_I(iData)  = Kp2
             F107_I(iData) = F107obs !could also use adj
             
             iData=iData+1
             iTime_I(Hour_) = 7
             iTime_I(Minute_) = 30
             call time_int_to_real(iTime_I,Time_I(iData))
             Ap_I(iData) = real(ap3)
             ApDaily_I(iData) = real(Ap)
             Kp_I(iData)  = Kp3
             F107_I(iData) = F107obs !could also use adj
             
             iData=iData+1
             iTime_I(Hour_) = 10
             iTime_I(Minute_) = 30
             call time_int_to_real(iTime_I,Time_I(iData))
             Ap_I(iData) = real(ap4)
             ApDaily_I(iData) = real(Ap)
             Kp_I(iData)  = Kp4
             F107_I(iData) = F107obs !could also use adj
             
             iData=iData+1
             iTime_I(Hour_) = 13
             iTime_I(Minute_) = 30
             call time_int_to_real(iTime_I,Time_I(iData))
             Ap_I(iData) = real(ap5)
             ApDaily_I(iData) = real(Ap)
             Kp_I(iData)  = Kp5
             F107_I(iData) = F107obs !could also use adj
             
             iData=iData+1
             iTime_I(Hour_) = 16
             iTime_I(Minute_) = 30
             call time_int_to_real(iTime_I,Time_I(iData))
             Ap_I(iData) = real(ap6)
             ApDaily_I(iData) = real(Ap)
             Kp_I(iData)  = Kp6
             F107_I(iData) = F107obs !could also use adj
             
             iData=iData+1
             iTime_I(Hour_) = 19
             iTime_I(Minute_) = 30
             call time_int_to_real(iTime_I,Time_I(iData))
             Ap_I(iData) = real(ap7)
             ApDaily_I(iData) = real(Ap)
             Kp_I(iData)  = Kp7
             F107_I(iData) = F107obs !could also use adj
             
             iData=iData+1
             iTime_I(Hour_) = 22
             iTime_I(Minute_) = 30
             call time_int_to_real(iTime_I,Time_I(iData))
             Ap_I(iData) = real(ap8)
             ApDaily_I(iData) = real(Ap)
             Kp_I(iData)  = Kp8
             F107_I(iData) = F107obs !could also use adj
          endif
       endif
    end do
    close (UnitTmp_)

  end subroutine read_kpapf107_indices_file

  !=============================================================================

  subroutine get_im_indices_F107(CurrentTime,F107)
    use ModInterpolate, only: linear
    real, intent(in) :: CurrentTime
    real, intent(out):: F107
    !---------------------------------------------------------------------------
    
    F107 = linear(F107_I(:),1,nData,CurrentTime,Time_I)    
  end subroutine get_im_indices_F107
  
  !=============================================================================

  subroutine get_im_indices_ap(CurrentTime,ap)
    use ModInterpolate, only: linear
    real, intent(in) :: CurrentTime
    real, intent(out):: ap
    !---------------------------------------------------------------------------
    
    ap = linear(Ap_I(:),1,nData,CurrentTime,Time_I)    
  end subroutine get_im_indices_ap
  
  !=============================================================================

  subroutine get_im_indices_Kp(CurrentTime,Kp)
    use ModInterpolate, only: linear
    real, intent(in) :: CurrentTime
    real, intent(out):: Kp
    !---------------------------------------------------------------------------
    
    Kp = linear(Kp_I(:),1,nData,CurrentTime,Time_I)    
  end subroutine get_im_indices_Kp

  !=============================================================================
  ! F107A - 81 day AVERAGE OF F10.7 FLUX (centered on day)
  subroutine get_im_indices_F107A(CurrentTime,F107A)
    use ModInterpolate, only: linear
    use ModUtilities,ONLY: CON_stop
    real, intent(in) :: CurrentTime
    real, intent(out):: F107A
    real :: F107,Time
    integer :: iDay,iCount
    real, parameter:: SecondsPerDay=86400.0
    !---------------------------------------------------------------------------
    
    !loop over days starting 40 back from current
    F107A=0.0
    iCount=iCount+1
    do iDay=-40,40
       Time= CurrentTime-real(iDay)*SecondsPerDay
       F107 = linear(F107_I(:),1,nData,Time,Time_I)
       if (F107>0) then
          F107A = F107A+F107
          iCount=iCount+1
       endif
    enddo

    if (iCount>0) then
       F107A = F107A/real(iCount)
    else
       call CON_stop('IM Error: could not get F107A')
    endif
  end subroutine get_im_indices_F107A
  
  !****************************************************************************
  !                         read_ae_wdc_kyoto
  !  Routine reads in the AE index file at simulation setup.
  !
  ! *** NOTE ***
  ! AE index file is to be in the WDC Kyoto IAGA2000 format.
  ! 
  !****************************************************************************
  subroutine read_ae_wdc_kyoto(iOutputError)
    
    use ModImTime
    use ModTimeConvert, ONLY: time_int_to_real
    use ModIoUnit, ONLY: UnitTmp_
    
    integer, intent(out) :: iOutputError
    
    integer :: ierror
    logical :: done
    
    integer 	      :: AeLun_ = UnitTmp_
    integer, parameter :: nHeader = 18
    character(LEN=100) :: AE_fmt = &
         "(I4,1X,I2,1X,I2,1X,I2,1X,I2,1X,I2,1X,I3,"//&
         "1X,I3,F13.2,F10.2,F10.2,F10.2)"
    character(len=100) :: line
    integer :: iLine, nLines
    integer  :: iYear, iMonth, iDay, &
         iHour, iMinute, iSecond, imSecond, iDoy
    real :: AE, AU, AL, AO, AE_RecordTime
    !-----------------------------------------------------------------------
    iOutputError = 0
    
    ! Open AE index file
    open(AeLun_, file=NameAeFile, status="old", iostat=ierror)
    
    ! Check to see file exists, else returns
    if (ierror.ne.0) then
       iOutputError = 1
       return
    endif
    
    done = .false.
    nLines = 1
    
    ! Cycles through the file once to determine its length.
    do while ( .not. done )
       
       read(AeLun_, '(a)', iostat = ierror ) line
       if (ierror.ne.0) done = .true.
       
       nLines = nLines + 1
    end do
    close(AeLun_)
    
    ! Sets the number of elements in the global variable.
    NumAeElements = nLines-nHeader-2
    
    ! Allocates AE Index and Time variables
    ALLOCATE( TimeAeIndex_I( NumAeElements ) )
    ALLOCATE( AeIndex_I( NumAeElements ) )
    
    ! Opens file again to record entries
    
    open(AeLun_, file=NameAeFile, status="old", iostat=ierror)
    
    do iLine = 1, nLines-2
       
       ! Cycles through the header
       if (iLine .le. nHeader) then
          
          read(AeLun_, '(a)', iostat = ierror ) line
          
       else
          
          ! Reads the AE values and stores them in temporaray
          ! variables.
          read(AeLun_, TRIM(AE_fmt), iostat = ierror ) &
               iYear, iMonth, iDay, &
               iHour, iMinute, iSecond, imSecond, &
               iDoy, AE, AU, AL, AO
          
          ! Stores the AE values in the variable arrays.
          if (ierror .ne. 0) then
             
             iOutputError = 1
             return
             
          else
             
             ! Convert the read in time to reference time.
             call time_int_to_real( (/iYear, iMonth, iDay, &
                  iHour, iMinute, iSecond, imSecond /), &
                  AE_RecordTime )
             
             ! Stores the reference time and AE values in the global
             ! variables.
             
             TimeAeIndex_I(iLine-nHeader) = AE_RecordTime
             AeIndex_I(iLine-nHeader) = AE
             
          end if
          
       end if
       
    end do
    
    ! Closes AE Index file.
    close(AeLun_)
    
  end subroutine read_ae_wdc_kyoto
  
  !****************************************************************************
  !
  !                            interpolate_ae
  !  Routine calculates AE index at the current simulation time.
  !
  !****************************************************************************
  subroutine interpolate_ae(CurrentTime, AeOut)
    
    use ModTimeConvert, ONLY: time_real_to_int
    use ModInterpolate, only: linear
    
    real, intent(in) :: CurrentTime
    real, intent(out) :: AeOut
    !character(len=6)  :: tchar
    
    !tchar='difint'
    
    !call lintpIM_diff( TimeAeIndex_I, AeIndex_I, NumAeElements, &
    !     CurrentTime, AeOut,tchar)
    AeOut = linear(AeIndex_I(:),1,NumAeElements,CurrentTime,TimeAeIndex_I)
    
    return
    
  end subroutine interpolate_ae

  !****************************************************************************
  !                         read_dst_wdc_kyoto
  !  Routine reads in the DST index file at simulation setup.
  !
  ! *** NOTE ***
  ! DST index file is to be in the WDC Kyoto IAGA2000 format.
  ! 
  !****************************************************************************
  subroutine read_dst_wdc_kyoto(iOutputError)
    
    use ModImTime
    use ModTimeConvert, ONLY: time_int_to_real
    use ModIoUnit, ONLY: UnitTmp_
    
    integer, intent(out) :: iOutputError
    
    integer :: ierror
    logical :: done
    
    integer 	      :: DstLun_ = UnitTmp_
    integer, parameter :: nHeader = 18
    character(LEN=100) :: DST_fmt = &
         "(I4,1X,I2,1X,I2,1X,I2,1X,I2,1X,I2,1X,I3,"//&
         "1X,I3,F13.2)"
    character(len=100) :: line
    integer :: iLine, nLines
    integer  :: iYear, iMonth, iDay, &
         iHour, iMinute, iSecond, imSecond, iDoy
    real :: DST, DST_RecordTime
    !-----------------------------------------------------------------------
    iOutputError = 0
    
    ! Open DST index file
    open(DstLun_, file=NameDstFile, status="old", iostat=ierror)
    
    ! Check to see file exists, else returns
    if (ierror.ne.0) then
       iOutputError = 1
       return
    endif
    
    done = .false.
    nLines = 1
    
    ! Cycles through the file once to determine its length.
    do while ( .not. done )
       
       read(DstLun_, '(a)', iostat = ierror ) line
       if (ierror.ne.0) done = .true.
       
       nLines = nLines + 1
    end do
    close(DstLun_)
    
    ! Sets the number of elements in the global variable.
    NumDstElements = nLines-nHeader-2
    
    ! Allocates DST Index and Time variables
    ALLOCATE( TimeDstIndex_I( NumDstElements ) )
    ALLOCATE( DstIndex_I( NumDstElements ) )
    
    ! Opens file again to record entries
    
    open(DstLun_, file=NameDstFile, status="old", iostat=ierror)
    
    do iLine = 1, nLines-2
       
       ! Cycles through the header
       if (iLine .le. nHeader) then
          
          read(DstLun_, '(a)', iostat = ierror ) line
          
       else
          
          ! Reads the DST values and stores them in temporaray
          ! variables.
          read(DstLun_, TRIM(DST_fmt), iostat = ierror ) &
               iYear, iMonth, iDay, &
               iHour, iMinute, iSecond, imSecond, &
               iDoy, DST
          
          ! Stores the DST values in the variable arrays.
          if (ierror .ne. 0) then
             
             iOutputError = 1
             return
             
          else
             
             ! Convert the read in time to reference time.
             call time_int_to_real( (/iYear, iMonth, iDay, &
                  iHour, iMinute, iSecond, imSecond /), &
                  DST_RecordTime )
             
             ! Stores the reference time and DST values in the global
             ! variables.
             
             TimeDstIndex_I(iLine-nHeader) = DST_RecordTime
             DstIndex_I(iLine-nHeader) = DST
             
          end if
          
       end if
       
    end do
    
    ! Closes DST Index file.
    close(DstLun_)
    
  end subroutine read_dst_wdc_kyoto
  
  !****************************************************************************
  !
  !                            interpolate_dst
  !  Routine calculates DST index at the current simulation time.
  !
  !****************************************************************************
  subroutine interpolate_dst(CurrentTime, DstOut)
    
    use ModTimeConvert, ONLY: time_real_to_int
    use ModInterpolate, only: linear
    
    real, intent(in) :: CurrentTime
    real, intent(out) :: DstOut
    !character(len=6)  :: tchar
    
    !tchar='difint'
    
    !call lintpIM_diff( TimeDstIndex_I, DstIndex_I, NumDstElements, &
    !     CurrentTime, DstOut,tchar)
    DstOut = linear(DstIndex_I(:),1,NumDstElements,CurrentTime,TimeDstIndex_I)
    return
    
  end subroutine interpolate_dst

  
end module ModImIndices
