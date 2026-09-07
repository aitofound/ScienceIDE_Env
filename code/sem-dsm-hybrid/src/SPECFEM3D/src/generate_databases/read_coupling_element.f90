subroutine read_coupling_element(myrank,prname,LOCAL_PATH)
   use tele_coupling_par

   implicit none
!   include "mpif.h"
   integer myrank
   character(len=256) prname,LOCAL_PATH
!   character(len=256) file_name
   integer ::i,ier
   integer ::int_tmp
   character(len=256) char_tmp
! The below two lines are used to avoid possible error reports during compling
! the code. LOCAL_PATH and myrank are not used now, that occationally causes error 
! reports. But they might be usful in the future, so we keep them here.
   int_tmp=myrank
   char_tmp=LOCAL_PATH

!   call create_name_database(prname,myrank,LOCAL_PATH)
   open(unit=175,file=prname(1:len_trim(prname))//'coupling_xlow.txt',status='old',&
   action='read',form='formatted',iostat=ier)
   read(175,*) coupling_nspec_xlow
   if(coupling_nspec_xlow.gt.0) then
        allocate(coupling_ispec_xlow(coupling_nspec_xlow))
        allocate(coupling_isubregion_xlow(coupling_nspec_xlow))
   end if
   do i=1,coupling_nspec_xlow
        read(175,*)coupling_ispec_xlow(i),coupling_isubregion_xlow(i)
   end do
   close(175)

   open(unit=175,file=prname(1:len_trim(prname))//'coupling_xhigh.txt',status='old',&
      action='read',form='formatted',iostat=ier)
   read(175,*) coupling_nspec_xhigh
   if(coupling_nspec_xhigh.gt.0) then
        allocate(coupling_ispec_xhigh(coupling_nspec_xhigh))
        allocate(coupling_isubregion_xhigh(coupling_nspec_xhigh))
   end if
   do i=1,coupling_nspec_xhigh
        read(175,*)coupling_ispec_xhigh(i),coupling_isubregion_xhigh(i)
   end do
   close(175)


   open(unit=175,file=prname(1:len_trim(prname))//'coupling_ylow.txt',status='old',&
      action='read',form='formatted',iostat=ier)
  read(175,*) coupling_nspec_ylow
  if(coupling_nspec_ylow.gt.0) then
     allocate(element_yLow(coupling_nspec_ylow))
     allocate(element_yLowReg(coupling_nspec_ylow))
  end if
  do i=1,coupling_nspec_ylow
        read(175,*)element_yLow(i),element_yLowReg(i)
  end do
  close(175)


  open(unit=175,file=prname(1:len_trim(prname))//'coupling_yhigh.txt',status='old',&
     action='read',form='formatted',iostat=ier)
   read(175,*) coupling_nspec_yhigh
   if(coupling_nspec_yhigh.gt.0) then
         allocate(coupling_ispec_yhigh(coupling_nspec_yhigh))
         allocate(coupling_isubregion_yhigh(coupling_nspec_yhigh))
   end if
   do i=1,coupling_nspec_yhigh
         read(175,*)coupling_ispec_yhigh(i),coupling_isubregion_yhigh(i)
   end do
   close(175)
   
  open(unit=175,file=prname(1:len_trim(prname))//'coupling_rtop.txt',status='old',&
     action='read',form='formatted',iostat=ier)
  read(175,*) coupling_nspec_rtop
  if(coupling_nspec_rtop.gt.0) then
        allocate(coupling_ispec_rtop(coupling_nspec_rtop))
        allocate(coupling_isubregion_rtop(coupling_nspec_rtop))
  end if
  do i=1,coupling_nspec_rtop
        read(175,*)coupling_ispec_rtop(i),coupling_isubregion_rtop(i)
  end do
  close(175)

  open(unit=175,file=prname(1:len_trim(prname))//'coupling_rbottom.txt',status='old',&
     action='read',form='formatted',iostat=ier)
  read(175,*) coupling_nspec_rbottom
  if(coupling_nspec_rbottom.gt.0) then
        allocate(coupling_ispec_rbottom(coupling_nspec_rbottom))
        allocate(coupling_isubregion_rbottom(coupling_nspec_rbottom))
  end if
  do i=1,coupling_nspec_rbottom
        read(175,*)coupling_ispec_rbottom(i),coupling_isubregion_rbottom(i)
  end do
  close(175)
end subroutine read_coupling_element
