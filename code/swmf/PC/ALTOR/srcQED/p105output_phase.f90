module output_phase
  use particles, only : R, nPartCell
  use fields , only : iCommArray,iCommArrayGlob
  use variables_data
  use variables_out, only : logPrint,format_output
  use MPImodule, only : mpiProc,mpiHost,nameProcOut,&
       MPIallreduce_min,MPIallreduce_max,MPIreduce_sum_i
  implicit none
  character (len=1) :: namePref(4)=(/'x','u','v','w'/)
contains
  !
  !----------------------------------------
  subroutine output_phase_all_out(n1,n2,nameSort)
    !--------------------------------------
    integer, intent(in) :: n1,n2
    character (len=1), intent(in) :: nameSort
    character (len=32) :: name
    !
    name='r'//nameTimeStep//nameSort//'.'//nameProcOut
    !
    if (format_output == 'f') then
       open(unit=1,file='f_'//name,form='formatted')
       write(1,*) iTime,nDim+3,n2-n1+1,nPartCell 
       write(1,*) R(:, n1:n2)
       close(1)
    else if (format_output == 'u') then
       open(unit=1,file='u_'//name,form='unformatted') 
       write(1) iTime,nDim+3,n2-n1+1,nPartCell & 
            ,R(:, n1:n2)
       close(1)
    end if
    !
    write(*,*) 'PHASE ALL: ',name
    !
  end subroutine output_phase_all_out
  !
  !----------------------------------------
  subroutine output_phase_out(n1,n2,nameSort)
    !--------------------------------------
    integer, intent(in) :: n1,n2
    character (len=1), intent(in) :: nameSort
    character (len=32) :: nameSuff
    integer :: k
    integer :: kX=1,kPx=nDim+1,kPy=nDim+2,kPz=nDim+3 !location in R 
    !
    nameLocal='PHASE OUT:'
    !
    ! local particle limits: minmax for momenta; box limits for coordinates
    !
    !do k=nDim*0+1,nDim+3 ! fix limits by the box size
    do k=nDim*0+1,nDim+3
       xMinPart(k)=minval(R(k,n1:n2)); xMaxPart(k)=maxval(R(k,n1:n2))
    end do
    ! fix limits by the box size
    xMinPart(1:nDim)=xMin(1:nDim); xMaxPart(1:nDim)=xMax(1:nDim)
    !
    ! global particle limits
    !
    call MPIallreduce_min(xMinPart,xMinPartGlob,nDim+3)
    call MPIallreduce_max(xMaxPart,xMaxPartGlob,nDim+3)
    !
    if(logPrint .and. mpiProc == mpiHost) then
       write(*,*) 'PE',nameProcOut,' PHASE: X MIN LOC=',xMinPart(1:nDim)
       write(*,*) 'PE',nameProcOut,' PHASE: X MIN GLO=',xMinPartGlob(1:nDim)
       write(*,*) 'PE',nameProcOut,' PHASE: X MAX LOC=',xMaxPart(1:nDim)
       write(*,*) 'PE',nameProcOut,' PHASE: X MAX GLO=',xMaxPartGlob(1:nDim)
    end if
    !
    !xMinPart=xMinPartGlob; xMaxPart=xMaxPartGlob
    !
    nameSuff=nameTimeStep//nameSort//'.'//nameProcOut
    !
    !if(logPrint) write(*,*)'phase output>>>>>>>>>> '
    
    call output_phase_plane(kX,kPx,nPhase(kX),nPhase(kPx),n1,n2,nameSuff)
    call output_phase_plane(kX,kPy,nPhase(kX),nPhase(kPy),n1,n2,nameSuff)
    !call output_phase_plane(kX,kPz,nPhase(kX),nPhase(kPz),n1,n2,nameSuff)
    call output_phase_plane(kPx,kPy,nPhase(kPx),nPhase(kPy),n1,n2,nameSuff)
    !call output_phase_volume(kX,kPx,kPy,nPhase(kX),nPhase(kPx),nPhase(kPy), &
    !    n1,n2,nameSuff)
    
    !
  end subroutine output_phase_out
  !
  !--------------------------------------------------------------------
  subroutine output_phase_plane(iIndex1,iIndex2,iMax1,iMax2,n1,n2,nameIn)
    !------------------------------------------------------------------
    !
    integer, intent(in) :: iIndex1,iIndex2,iMax1,iMax2,n1,n2
    character (len=*), intent(in) :: nameIn
    character (len=12) :: name
    ! ii - for particle location in iii array, iIndex - joining index in R
    integer :: ii(2),n,k,iIndex(2),i1,i2   
    ! min,max values in 2D space, and their difference, global min,max 
    real :: x1(2),x2(2),xDel(2)
    ! the integer array iii collecting particles with dimensions iMax 
    integer :: iMax(2),iMaxProd
    integer :: iii(iMax1,iMax2)  
    !
    nameLocal='PHASE 2D:'
    !
    iIndex=(/iIndex1,iIndex2/)
    iMax=(/iMax1,iMax2/)
    iMaxProd=product(iMax)
    name=namePref(iIndex(1))//namePref(iIndex(2))//nameIn
    !write(*,*) 'PHASE NAME  ',name,'  PE',mpiProc
    !
    ! local boundaries for 2D phase space
    !
    x1=xMinPartGlob(iIndex); x2=xMaxPartGlob(iIndex)
    !
    ! no output if no important information
    if(any( (x2-x1) <1.e-5 )) then 
       !write(*,*) 'phase is not written: ',name,x1,x2,x2-x1
       return 
    end if
    !
    xDel=x2-x1
    if(logPrint) write(*,*) 'PHASE NAME  ',name,'  PE',mpiProc,x1,x2,xDel
    !if(logPrint) write(*,*)' phase: min,max,del',x1,x2,xDel
    !
    ! calculation of phase array
    iii=0
    LOOP:    do n=n1,n2
       ii=(R(iIndex,n)-x1)/xDel*iMax
       do k=1,2
          ! why lost particles???
          if(ii(k)<1) ii(k)=1 !cycle LOOP !ii(k)=1
          if(ii(k)>iMax(k)) ii(k)=iMax(k) !cycle LOOP  !ii(k)=iMax(k)
       end do
       iii(ii(1),ii(2))=iii(ii(1),ii(2))+1
       !       if(logPrint) write(*,*)' phase:',ii,iii(ii(1),ii(2))
    end do LOOP
    !
    ! writing to 1D array
    iCommArray(1:iMaxProd)=0
    forall(i1=1:iMax(1),i2=1:iMax(2)) &
         iCommArray((i2-1)*iMax(1)+i1)=iii(i1,i2)
    iCommArrayGlob(1:iMaxProd)=0
    !
    ! summation to PE00
    !call MPIreduce_sum_i(iCommArray,iCommArrayGlob,iMaxProd)
    call MPIreduce_sum_i(iii,iCommArrayGlob,iMaxProd)
    if(logPrint) then 
       write(*,*)' Phase: iii= ',maxval(iii),sum(iii)
       write(*,*)' iCommArray=', &
            maxval(iCommArray(1:iMaxProd)), &
            sum(iCommArray(1:iMaxProd))
       write(*,*)' iCommGlob= ', &
            maxval(iCommArrayGlob(1:iMaxProd)), &
            sum(iCommArrayGlob(1:iMaxProd))
    end if
    !
    ! output from mpiHost
    if(mpiProc == mpiHost) then
       !
       if (format_output == 'f') then
          open(unit=1,file='f_'//name,form='formatted')
          write(1,*) iTime,2,iMax,x1,x2,nPartCell !,dx 
          write(1,*) iCommArrayGlob(1:iMaxProd)
          close(1)
       else if (format_output == 'u') then
          open(unit=1,file='u_'//name,form='unformatted') 
          write(1) iTime,2,iMax,x1,x2,nPartCell & !,dx &
               ,iCommArrayGlob(1:iMaxProd)
          close(1)
       end if
       !
       write(*,*) 'PHASE: ',name, x1(1),x2(1),'  ', x1(2),x2(2)
       !
    end if
    !
  end subroutine output_phase_plane
  !
  !-----------------------------------------------------------------------
  subroutine output_phase_volume(iIndex1,iIndex2,iIndex3, &
       iMax1,iMax2,iMax3,n1,n2,nameIn)
  !-----------------------------------------------------------------------
    integer, intent(in) :: iIndex1,iIndex2,iIndex3,iMax1,iMax2,iMax3,n1,n2
    character (len=*), intent(in) :: nameIn
    character (len=12) :: name
    ! ii - for particle location in iii array, iIndex - joining index in R
    integer :: ii(3),n,k,iIndex(3),i1,i2,i3 
    ! min,max values in 2D space, and their difference
    real :: x1(3),x2(3),xDel(3) 
    ! the integer array iii collecting particles with dimensions iMax 
    integer :: iMax(3),iMaxProd
    integer :: iii(iMax1,iMax2,iMax3) 
    !
    nameLocal='PHASE 3D:'
    !
    iIndex=(/iIndex1,iIndex2,iIndex3/)
    iMax=(/iMax1,iMax2,iMax3/)
    iMaxProd=product(iMax)
    name=namePref(iIndex(1))//namePref(iIndex(2))//namePref(iIndex(3))//nameIn
    !write(*,*) 'PHASE NAME  ',name,'  PE',mpiProc
    !
    ! local boundaries for 2D phase space
    !
    x1=xMinPartGlob(iIndex); x2=xMaxPartGlob(iIndex)
    !
    ! no output if no important information
    if(any( (x2-x1) <1.e-5 )) then 
       !write(*,*) 'phase is not written: ',name,x1,x2,x2-x1
       return 
    end if
    !
    xDel=x2-x1
    if(logPrint) write(*,*) 'PHASE NAME  ',name,'  PE',mpiProc,x1,x2,xDel
    !
    ! calculation of phase array
    iii=0
    LOOP:    do n=n1,n2
       ii=(R(iIndex,n)-x1)/xDel*iMax
       do k=1,3
          if(ii(k)<1) ii(k)=1 ! cycle LOOP !ii(k)=1 !??? + count out
          if(ii(k)>iMax(k)) ii(k)=iMax(k) !cycle LOOP  !ii(k)=iMax(k)
       end do
       iii(ii(1),ii(2),ii(3))=iii(ii(1),ii(2),ii(3))+1
       !       if(logPrint) write(*,*)' phase:',ii,iii(ii(1),ii(2),ii(3))
    end do LOOP
    !
    ! writing to 1D array
    iCommArray(1:iMaxProd)=0
    forall(i1=1:iMax(1),i2=1:iMax(2),i3=1:iMax(3)) &
         iCommArray((i3-1)*iMax(1)*iMax(2)+(i2-1)*iMax(1)+i1)=iii(i1,i2,i3)
    iCommArrayGlob(1:iMaxProd)=0
    !
    ! summation to PE00
    call MPIreduce_sum_i(iCommArray,iCommArrayGlob,iMaxProd)
    !call MPIreduce_sum_i(iii,iCommArrayGlob,iMaxProd)
    if(logPrint) then 
       write(*,*)' Phase: iii= ',maxval(iii),sum(iii)
       write(*,*)' iCommArray=', &
            maxval(iCommArray(1:iMaxProd)), &
            sum(iCommArray(1:iMaxProd))
       write(*,*)' iCommGlob= ', &
            maxval(iCommArrayGlob(1:iMaxProd)), &
            sum(iCommArrayGlob(1:iMaxProd))
    end if
    !
    ! output from mpiHost
    if(mpiProc == mpiHost) then
       !
       if (format_output == 'f') then
          open(unit=20,file='f_'//name,form='formatted')
          write(20,*) iTime,3,iMax,x1,x2,nPartCell !,dx 
          write(20,*) iCommArrayGlob(1:iMaxProd)
          close(20)
       else if (format_output == 'u') then
          open(unit=20,file='u_'//name,form='unformatted') 
          write(20) iTime,3,iMax,x1,x2,nPartCell & !,dx & 
               ,iCommArrayGlob(1:iMaxProd)
          close(20)
       end if
       !
       write(*,*) 'PHASE: ',name, x1(1),x2(1),'  ', x1(2),x2(2) &
            ,'  ', x1(3),x2(3)
       !
    end if
    !
  end subroutine output_phase_volume
  !
end module output_phase
