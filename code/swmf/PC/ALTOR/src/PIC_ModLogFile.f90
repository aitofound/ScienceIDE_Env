!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PIC_ModLogFile
  use PC_ModSize, ONLY: nPType, nDim
  use PC_ModPhysics, ONLY: No2Io_V, UnitT_
  implicit none
  SAVE
  PRIVATE !Except

  !\
  !Unit for the log file
  !/
  integer :: iLogUnit = -1

  !\
  !Writing frequency for the logfile
  !/
  integer, public :: nLogFile = -1

  !\
  ! Starting energy (to be used for normalization)
  !/
  real :: Energy0 = -1.0


  public :: open_logfile
  public :: write_logfile
  public :: close_logfile
  
  integer, parameter ::                &
       Time_ = 1,                      &
       PartFirst_ = 2,                 &
       PartLast_  = 1 + max(nPType,1), &
       Bx_        = 2 + max(nPType,1), &
       Ez_        = 7 + max(nPType,1), &
       ETotal_    = 8 + max(nPType,1)

  real  :: Value_V(Time_:ETotal_)
  character(LEN=30) ::NameFormat
  integer,public::nToWrite=0, nToWrite_II(3,100) 
contains
  !======================
  subroutine open_logfile
     use ModIoUnit,        ONLY: io_unit_new
     use PIC_ModProc,      ONLY: iProc
  
     character(LEN=100) :: Name
     character(LEN=1  ) :: Name1
     character(len=*), parameter :: FileDir='PC/plots/'
  
     integer :: iP
     !---------------------
     !\
     !Open log file
     !/
     if(iProc==0)then
        write(Name,'(a,i4.4,a)')'log_n',nLogFile,'.log'
        iLogUnit = io_unit_new()
        open(iLogUnit,file=FileDir//Name,status='replace')
        write(Name,'(a)')'iStep Time'
        do iP = 1, nPType
           write(Name1,'(i1.1)')iP
           Name = trim(Name)//' '//'Energy'//Name1
        end do
        Name = trim(Name)//' Bx2 By2 Bz2 Ex2 Ey2 Ez2 ETotal'
        write(iLogUnit,'(a)') Name
        do iP = 1, nToWrite
           nToWrite_II(1,iP) = io_unit_new()
           Name = ' '
           write(Name,'(a,i4.4,a)')'log_p',iP,'.out'
           open(nToWrite_II(1,iP),file=FileDir//Name,status='replace')
           if(nDim==2)write(nToWrite_II(1,iP),'(a)')&
             'iStep Time x y Wx Wy Wz'
           if(nDim==3)write(nToWrite_II(1,iP),'(a)')&
             'iStep Time x y z Wx Wy Wz'
        end do
     end if
     call write_logfile
  end subroutine open_logfile
  !===========================
  subroutine write_logfile
    use PIC_ModProc,      ONLY: iProc
    use PIC_ModField,     ONLY: get_field_energy
    use PIC_ModMain,      ONLY: iStep, tSimulation, Dx_D
    use PIC_ModParticles, ONLY: Energy_P, Wx_, Wz_
    use PC_BATL_particles, ONLY: Particle_I
    use ModUtilities, ONLY: flush_unit
    integer :: iP
    !-----------------------
    Value_V = 0.0
    call get_field_energy(Value_V(Bx_:Ez_))
    if(iProc/=0)return
    Value_V(Time_) = tSimulation*No2Io_V(UnitT_)
    Value_V(PartFirst_:PartLast_) = Energy_P
    Value_V(ETotal_) = sum(Value_V(PartFirst_:Ez_))
    if(Energy0 <=0)then
       !Initial stage, save ETotal and use it for normalization
       Energy0 =  Value_V(ETotal_)
       if(Energy0 <=0)Energy0 = 1.0
       !Make format statement
       if(nPType==1)then
          write(NameFormat,'(a)')'(i10,9es13.5)'
       else
          write(NameFormat,'(a,i2,a)')'(i10,',ETotal_,'es13.5)'
       end if
    end if
    Value_V(ETotal_) =  Value_V(ETotal_) / Energy0
    write(iLogUnit,trim(NameFormat))iStep, Value_V
    call flush_unit(iLogUnit)
    do iP=1, nToWrite
       write(nToWrite_II(1,iP),'(i10,7es13.5)')iStep,&
            tSimulation*No2Io_V(UnitT_), &
            Particle_I(nToWrite_II(2,iP))%State_VI(1:nDim,nToWrite_II(3,iP))&
                *Dx_D,&
            Particle_I(nToWrite_II(2,iP))%State_VI(Wx_:Wz_,nToWrite_II(3,iP))
       call flush_unit(nToWrite_II(1,iP))
    end do
  end subroutine write_logfile
  !===========================
  subroutine close_logfile
    use PIC_ModProc, ONLY: iProc
    integer :: iP
    !--------------------
    if(iProc==0)then
       close(iLogUnit)
       do iP=1, nToWrite
          close(nToWrite_II(1,iP))
       end do
    end if
  end subroutine close_logfile
end module PIC_ModLogFile
