subroutine save_coupling_element(coupling_nspec_xlow,coupling_ispec_xlow,coupling_isubregion_xlow,&
                coupling_nspec_xhigh,coupling_ispec_xhigh, &
                coupling_isubregion_xhigh,coupling_nspec_ylow,coupling_ispec_ylow,&
                coupling_isubregion_ylow,coupling_nspec_yhigh,coupling_ispec_yhigh,&
                coupling_isubregion_yhigh,coupling_nspec_rtop,coupling_ispec_rtop,coupling_isubregion_rtop,&
                coupling_nspec_rbottom,coupling_ispec_rbottom,coupling_isubregion_rbottom,LOCAL_PATH,iproc)

  use constants
  use coupling_mesh_par, only :R_TOP_BOUND
  implicit none
  !include "constants.h"
  !name of the database files
  integer ::iproc
  character(len=256) proname,file_name,LOCAL_PATH,clean_LOCAL_PATH
  integer ::coupling_nspec_xlow,coupling_nspec_xhigh,coupling_nspec_ylow,coupling_nspec_yhigh,&
            coupling_nspec_rtop,coupling_nspec_rbottom
  integer,dimension(coupling_nspec_xlow)   ::coupling_ispec_xlow,coupling_isubregion_xlow
  integer,dimension(coupling_nspec_xhigh)  ::coupling_ispec_xhigh,coupling_isubregion_xhigh
  integer,dimension(coupling_nspec_ylow)   ::coupling_ispec_ylow,coupling_isubregion_ylow
  integer,dimension(coupling_nspec_yhigh)  ::coupling_ispec_yhigh,coupling_isubregion_yhigh
  integer,dimension(coupling_nspec_rtop)      ::coupling_ispec_rtop,coupling_isubregion_rtop
  integer,dimension(coupling_nspec_rbottom)      ::coupling_ispec_rbottom,coupling_isubregion_rbottom

  integer i


  write(proname,"('/proc',i6.6,'_')") iproc
  clean_LOCAL_PATH = adjustl(LOCAL_PATH)
  file_name = clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH))//proname(1:len_trim(proname))//'coupling_xlow.txt'

  open(unit=16,file=file_name(1:len_trim(file_name)),status='unknown',action='write',form='formatted')
  write(16,*) coupling_nspec_xlow
  do i=1,coupling_nspec_xlow
     write(16,*)coupling_ispec_xlow(i),coupling_isubregion_xlow(i)
  end do
  close(16)
  
  file_name =  clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH))//proname(1:len_trim(proname))//'coupling_xhigh.txt'

  open(unit=16,file=file_name(1:len_trim(file_name)),status='unknown',action='write',form='formatted')
  write(16,*) coupling_nspec_xhigh
  do i=1,coupling_nspec_xhigh
     write(16,*)coupling_ispec_xhigh(i),coupling_isubregion_xhigh(i)
  end do
  close(16)

  file_name = clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH))//proname(1:len_trim(proname))//'coupling_ylow.txt'
  open(unit=16,file=file_name(1:len_trim(file_name)),status='unknown',action='write',form='formatted')
  write(16,*) coupling_nspec_ylow
  do i=1,coupling_nspec_ylow
     write(16,*)coupling_ispec_ylow(i),coupling_isubregion_ylow(i)
  end do
  close(16)

   file_name = clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH))//proname(1:len_trim(proname))//'coupling_yhigh.txt'
  open(unit=16,file=file_name(1:len_trim(file_name)),status='unknown',action='write',form='formatted')
  write(16,*) coupling_nspec_yhigh
  do i=1,coupling_nspec_yhigh
     write(16,*)coupling_ispec_yhigh(i),coupling_isubregion_yhigh(i)
  end do
  close(16)

  file_name =  clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH))//proname(1:len_trim(proname))//'coupling_rtop.txt'
  open(unit=16,file=file_name(1:len_trim(file_name)),status='unknown',action='write',form='formatted')
  if(dabs(R_TOP_BOUND-R_EARTH_SURF).lt.TINYVAL) then
    ! The top coupling boundary is free surface. It is traction-free, and theorefore disappears in the coupling.
    write(16,*) 0
  else
    write(16,*) coupling_nspec_rtop
    do i=1,coupling_nspec_rtop
       write(16,*)coupling_ispec_rtop(i),coupling_isubregion_rtop(i)
    end do
    close(16)
  end if

  file_name =  clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH))//proname(1:len_trim(proname))//'coupling_rbottom.txt'
  open(unit=16,file=file_name(1:len_trim(file_name)),status='unknown',action='write',form='formatted')
  write(16,*) coupling_nspec_rbottom
  do i=1,coupling_nspec_rbottom
     write(16,*)coupling_ispec_rbottom(i),coupling_isubregion_rbottom(i)
  end do
  close(16)
end subroutine

