module particles_initiate !p102particles_initiate_foil_move.f90
  use constants
  use variables_particles
  use particles, only : R
  use variables_box
  use variables_data
  use variables_out, only : logPrint
  use plasma_profiles, only : coordinate
  use MPImodule, only : mpiProc,mpiHost,mpiProcN,MPIerror_report
  implicit none
  !
  integer,parameter :: nPlas=1 !input! only 1 
  integer,save :: nSavePlas !current status of jPlas for moving frame -- 
  integer :: iPlas(1:nPlas)=(/ 0 /) !plasma pfofiles: 0-uni, 1-lin, 2-exp
  real :: pl_n(0:nPlas)=(/ 1.0, 1.0 /)!density /equal for iPlas=0
  real :: pl_len(0:nPlas)=(/ 50.0, 50.0 /)!plasma start and lengths
  !
  real,save :: pl_lenTotal(1:nPlas)
  !real,save :: pl_nDiff(1:nPlas) !defect btw profiles in moving frame/not done
  !real,save :: pl_square(0:nPlas) !(n0+n1)*l/2
  !
contains
  !
  subroutine particles_foil__!---------------------------------------------
    ! info: making a choice of profiles (0-uni, 1-lin, 2-exp) 
    !       with 
    ! info: || move not done ? 
    ! info: || mask not done (supposed mask=0) ?
    ! ions done 
    ! density: pl_n(0:nPlas) <= 1.0, >= 1/nPart(1)
    ! plasma_length: pl_len(0:nPlas) > 0
    !
    integer :: i1,n,nn,j,iIndex(nDim)
    integer :: nPartProc !number of particles per proc
    real :: tmp,tmp2,tmp3,x(1:nDim) 
    integer :: jPlas
    logical :: logCond(3)
    !
    forall (jPlas=1:nPlas) &
         pl_lenTotal(jPlas)=sum(pl_len(1:jPlas))
    !forall (jPlas=1:nPlas) & 
     !    pl_nDiff(jPlas)=ZERO  
    !pl_square(0)=ZERO
    nSavePlas=1
    !forall (jPlas=1:nPlas) & 
         !pl_square(jPlas)=(pl_n(jPlas-1)+pl_n(jPlas))*pl_len(jPlas)*HALF
    if (mpiProc == mpiHost) then
       write(*,*) 'PLASMA PROFILE= ',iPlas(1:nPlas)
       write(*,*) 'PLASMA LENGTH= ',pl_len(1:nPlas)
       !write(*,*) 'PLASMA LIN> ',pl_nDiff
       !write(*,*) 'PLASMA SQ > ',pl_square
       write(*,*)'pl_start=',xMin(1)+pl_len(0),'left_bound=',dx(1)*(iBound1(1))
    end if
    !
    if ( xMin(1)+pl_len(0) < dx(1)*iBound1(1) ) &
         write(*,*) 'WARNING: pl_start < left_bound'
    !
    select case(nPartSorts)
    case(0)
       np1=1; np2=0
    case(1:)
       nn=0
       do j=0,1 !emulation for j=0, real distribution for j=1
          n=0
          !
          include 'p102_foil.fh' !>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
          !
          np1(1)=1
          np2(1)=nn
          np1(2)=nPartMax
          np2(2)=nPartMax-1
          !
          if (j == 0) then ! calculation of the 1st and last particle numbers 
             nPartProc=ceiling(ONE*n*nPartSorts/mpiProcN) !particles p/proc
             if(logPrint) write(*,*) 'We need ',n*nPartSorts,' particles, ', &
                  nPartProc,' per proc' 
             if(nPartProc > nPartMax) then 
                write(*,*) '** We have only ',nPartMax,' particles of ', &
                     nPartProc,' needed'
                iError=1
                call MPIerror_report(11,'particles_foil__', &
                     ' Not enough particles!')
                stop
             end if
             nPartProc=ceiling(ONE*n/mpiProcN) !only electrons p/proc
          end if
       end do
       !
    end select
    !
    if (mpiProc == mpiHost) then
       write(*,*) 'PLASMA BOUNDARY====================' 
       do j=1,nDim 
          write(*,*) ' iDim, minmax= ' ,j, &
               minval(R(j,1:nn)),maxval(R(j,1:nn))
       end do
    end if
    !
    if(nPartSorts == 2) then
       np1(2)=nn+1
       np2(2)=nn*2
       R(1:nDim,np1(2):np2(2))=R(1:nDim,np1(1):np2(1))
       R(nDim+1:nDim+3,np1(2):np2(2))=-R(nDim+1:nDim+3,np1(1):np2(1))
    end if
  end subroutine particles_foil__
  !
  !
  !
  subroutine add_particles__ ! works with up to 2 sorts
    !
    integer :: n1,n2 
    integer :: i1,n,nn,j,iIndex(nDim)
    integer :: nPartProc !number of particles per proc
    !
    real :: tmp,tmp2,tmp3,x(1:nDim) 
    integer :: jPlas
    logical :: logCond(3)
    !
    j=1; n1=np1(j); n2=np2(j) !running through electrons
    nn=n2 !start point for adding particles
    !
    do j=0,1 !emulation for j=0, real distribution for j=1
       n=0
       !
       include 'p102_foil.fh' !>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
       !
       ! >>>>>>>>>>>>>>>>>>>>>> old piece >>>>>>>>>>>>>>>>>>>>>>>
       !
       ! ---------------! calculation of the 1st and last particle numbers
       !                ! with up to 2 sorts
       if (j == 0) then 
          ! 
          nPartProc=ceiling(ONE*n*nPartSorts/mpiProcN)+&
               sum(np2-np1)+nPartSorts
          if(logPrint) write(*,*) 'We need ',n*nPartSorts, &
               ' new particles, total ', &
               nPartProc,' per proc' 
          if(nPartProc > nPartMax) then 
             write(*,*) '** We have only ',nPartMax,' particles of ', &
                  nPartProc,' needed'
             open(unit=20,file='stop.run',form='formatted')
             close(20)
             !
             iError=1
             call MPIerror_report(11,'particles_foil__', &
                  ' Not enough particles!')
             stop
          end if
          nPartProc=ceiling(ONE*n/mpiProcN) !only electrons p/proc
       end if
       !
       if (j == 1) then
          np2(1)=nn 
          !
          if(nPartSorts == 2) then
             n1=n2+1 ! for ions
             R(:,np1(2)-np2(1)+n1-1:np1(2)-1)=R(:,n1:np2(1))
             np1(2)=np1(2)-np2(1)+n1-1
          end if
          !
       end if
       !
    end do
    !
  end subroutine add_particles__
  !
end module particles_initiate
