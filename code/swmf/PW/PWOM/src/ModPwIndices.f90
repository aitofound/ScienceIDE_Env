module ModPwIndices
  implicit none
  save
  private
  
  logical,public:: UsePwIndicesFile = .false.
  
  !vars to hold file data
  real,    allocatable :: Time_I(:)
  real,    allocatable :: Ap_I(:),ApDaily_I(:),Kp_I(:),F107_I(:)

  real :: StartTime
  
  !size of data arrays (8*(filelength-header))
  integer :: nData
  
  integer, parameter :: Year_=1,Month_=2,Day_=3,Hour_=4,Minute_=5,Second_=6
  
  !public routines
  public :: read_pw_indices_file
  public :: get_pw_indices_F107
  public :: get_pw_indices_ap
  public :: get_pw_indices_Kp
  public :: get_pw_indices_F107A
  public :: get_pw_indices_ap_array
contains
  subroutine read_pw_indices_file(StartTimeIn)
    use ModTimeConvert, ONLY: time_int_to_real,time_real_to_int
    use ModIoUnit, ONLY: UnitTmp_
    use ModUtilities,ONLY: CON_stop
    use ModConst, ONLY: iYearBase
    real, intent(in) :: StartTimeIn
    !file names
    Character(len=100) :: NameFile='PW/IndicesKpApF107.dat'
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
    !save the start time of the simulation
    StartTime=StartTimeIn
    
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
       write(*,*) 'PW Warning: trying to read PwIndices a second time'
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
    !Check min and max time in file against simulation start time

    if (StartTime < Time_I(1) .or. StartTime > Time_I(nData))then
       write(*,*) StartTime
       call time_real_to_int(StartTime,iTime_I)
       write(*,*) iTime_I
       write(*,*) Time_I(1)
       call time_real_to_int(Time_I(1),iTime_I)
       write(*,*) iTime_I
       write(*,*) Time_I(nData)
       call time_real_to_int(Time_I(nData),iTime_I)
       write(*,*) iTime_I
       
       
       call CON_stop('PW Error: start time out of range of '//&
            'PW/IndicesKpApF107.dat file. Extend file or use alternate method.')
    endif
  end subroutine read_pw_indices_file

  !=============================================================================

  subroutine get_pw_indices_F107(tSimulation,F107)
    use ModInterpolate, only: linear
    real, intent(in) :: tSimulation
    real, intent(out):: F107
    real :: CurrentTime
    !---------------------------------------------------------------------------
    CurrentTime=StartTime+tSimulation

    F107 = linear(F107_I(:),1,nData,CurrentTime,Time_I)    
  end subroutine get_pw_indices_F107
  
  !=============================================================================

  subroutine get_pw_indices_ap(tSimulation,ap)
    use ModInterpolate, only: linear
    real, intent(in) :: tSimulation
    real, intent(out):: ap
    real :: CurrentTime
    !---------------------------------------------------------------------------
    CurrentTime=StartTime+tSimulation

    ap = linear(Ap_I(:),1,nData,CurrentTime,Time_I)    
  end subroutine get_pw_indices_ap
  
  !=============================================================================

  subroutine get_pw_indices_Kp(tSimulation,Kp)
    use ModInterpolate, only: linear
    real, intent(in) :: tSimulation
    real, intent(out):: Kp
    real :: CurrentTime
    !---------------------------------------------------------------------------
    CurrentTime=StartTime+tSimulation

    Kp = linear(Kp_I(:),1,nData,CurrentTime,Time_I)    
  end subroutine get_pw_indices_Kp

  !=============================================================================
  ! F107A - 81 day AVERAGE OF F10.7 FLUX (centered on day)
  subroutine get_pw_indices_F107A(tSimulation,F107A)
    use ModInterpolate, only: linear
    use ModUtilities,ONLY: CON_stop
    
    real, intent(in) :: tSimulation
    real, intent(out):: F107A
    real :: F107,Time,CurrentTime
    integer :: iDay,iCount
    real, parameter:: SecondsPerDay=86400.0

    !---------------------------------------------------------------------------
    CurrentTime=StartTime+tSimulation

    !loop over days starting 40 back from current
    F107A=0.0
    iCount=0
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
       call CON_stop('PW Error: could not get F107A')
    endif
  end subroutine get_pw_indices_F107A

  !=============================================================================
  ! routine to calculate AP(7) array for MSIS input where the array
  ! is defined as
  !             (1) DAILY AP
  !             (2) 3 HR AP INDEX FOR CURRENT TIME
  !             (3) 3 HR AP INDEX FOR 3 HRS BEFORE CURRENT TIME
  !             (4) 3 HR AP INDEX FOR 6 HRS BEFORE CURRENT TIME
  !             (5) 3 HR AP INDEX FOR 9 HRS BEFORE CURRENT TIME
  !             (6) AVERAGE OF EIGHT 3 HR AP INDICIES FROM 12 TO 33 HRS PRIOR
  !                    TO CURRENT TIME
  !             (7) AVERAGE OF EIGHT 3 HR AP INDICIES FROM 36 TO 57 HRS PRIOR
  !                    TO CURRENT TIME
  subroutine get_pw_indices_ap_array(tSimulation,ApMsis_I)
    use ModInterpolate, only: linear
    real, intent(in) :: tSimulation
    real, intent(out):: ApMsis_I(7)
    real :: CurrentTime
    real, parameter:: SecondsPerHour=3600.0
    !---------------------------------------------------------------------------
    CurrentTime=StartTime+tSimulation

    !get daily AP
    ApMsis_I(1)  =linear(ApDaily_I(:),1,nData,CurrentTime,Time_I)

    !get AP at current time
    ApMsis_I(2)  = linear(Ap_I(:),1,nData,CurrentTime,Time_I)

    !get AP 3hr before current time
    ApMsis_I(3)  = &
         linear(Ap_I(:),1,nData,CurrentTime-3.0*SecondsPerHour,Time_I)

    !get AP 6hr before current time
    ApMsis_I(4)  = &
         linear(Ap_I(:),1,nData,CurrentTime-6.0*SecondsPerHour,Time_I)

    !get AP 3hr before current time
    ApMsis_I(5)  = &
         linear(Ap_I(:),1,nData,CurrentTime-9.0*SecondsPerHour,Time_I)

    !get AP average from 12-33hr before current time
    ApMsis_I(6)  = 0.125*(&
         linear(Ap_I(:),1,nData,CurrentTime-12.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-15.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-18.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-21.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-24.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-27.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-30.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-33.0*SecondsPerHour,Time_I))

    !get AP average from 36-57hr before current time
    ApMsis_I(7)  = 0.125*(&
         linear(Ap_I(:),1,nData,CurrentTime-36.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-39.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-42.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-45.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-48.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-51.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-54.0*SecondsPerHour,Time_I)&
         +linear(Ap_I(:),1,nData,CurrentTime-57.0*SecondsPerHour,Time_I))

  end subroutine get_pw_indices_ap_array


  
end module ModPwIndices
