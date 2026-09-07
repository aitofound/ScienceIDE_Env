subroutine read_parameter_SEMtoTele(myrank)

!include "constants.h"

use constants
use tele_coupling_par
implicit none


integer myrank
!integer::NXI_TOPOGRAPHY_TAPER,NETA_TOPOGRAPHY_TAPER,NXI_NO_TOPOGRAPHY,NETA_NO_TOPOGRAPHY
!integer::COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,COUPLING_IR_TOP,Tele_irBot
!integer::NEX_XI,NEX_ETA,NER,NDOUBLINGS
!logical USE_REGULAR_MESH
!integer::ner_doublings(2)
!integer::myrank
!integer::Imain_SEMtoTele

integer :: ier
integer ::int_tmp
!The below line is used to avoid possible error reports during compling
!the code. myrank is not used now, that occationally causes error 
!reports. But it might be usful in the future, so we keep them here.
int_tmp=myrank


! open parameter file
 open(unit=IIN,file=MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES)) &
       //'Coupling_Par_file',status='old',action='read')

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,R_TOP_BOUND,'SEMtoTele.R_TOP_BOUND',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter R_TOP_BOUND'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,ANGULAR_WIDTH_XI_IN_DEGREES,'SEMtoTele.ANGULAR_WIDTH_XI_IN_DEGREES',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter ANGULAR_WIDTH_XI_IN_DEGREES'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,ANGULAR_WIDTH_ETA_IN_DEGREES,'SEMtoTele.ANGULAR_WIDTH_ETA_IN_DEGREES',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter ANGULAR_WIDTH_ETA_IN_DEGREES'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,CENTER_LATITUDE_IN_DEGREES,'SEMtoTele.CENTER_LATITUDE_IN_DEGREES',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter CENTER_LATITUDE_IN_DEGREES'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,CENTER_LONGITUDE_IN_DEGREES,'SEMtoTele.CENTER_LONGITUDE_IN_DEGREES',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter CENTER_LONGITUDE_IN_DEGREES'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,GAMMA_ROTATION_AZIMUTH,'SEMtoTele.GAMMA_ROTATION_AZIMUTH',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter GAMMA_ROTATION_AZIMUTH'

! call read_value_double_precision_tele(IIN,IGNORE_JUNK,DEPTH_BLOCK_KM_tmp,'SEMtoTele.DEPTH_BLOCK_KM',ier)
! if(ier /= 0) stop 'Error reading SEMtoTele paramete DEPTH_BLOCK_KM'

! Z_DEPTH_BLOCK = - dabs(DEPTH_BLOCK_KM_tmp) * 1000.d0


 call read_value_integer_tele(IIN,IGNORE_JUNK,NXI_TOPOGRAPHY_TAPER, 'SEMtoTele.NXI_TOPOGRAPHY_TAPER',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter NXI_TOPOGRAPHY_TAPER'

 call read_value_integer_tele(IIN,IGNORE_JUNK,NETA_TOPOGRAPHY_TAPER, 'SEMtoTele.NETA_TOPOGRAPHY_TAPER',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter NETA_TOPOGRAPHY_TAPER'

 call read_value_integer_tele(IIN,IGNORE_JUNK,NXI_NO_TOPOGRAPHY, 'SEMtoTele.NXI_NO_TOPOGRAPHY',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter NXI_NO_TOPOGRAPHY'

 call read_value_integer_tele(IIN,IGNORE_JUNK,NETA_NO_TOPOGRAPHY, 'SEMtoTele.NETA_NO_TOPOGRAPHY',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter NETA_NO_TOPOGRAPHY'

 call read_value_integer_tele(IIN,IGNORE_JUNK,COUPLING_IXI_LOW, 'SEMtoTele.COUPLING_IXI_LOW',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter COUPLING_IXI_LOW'

 call read_value_integer_tele(IIN,IGNORE_JUNK,COUPLING_IXI_HIGH, 'SEMtoTele.COUPLING_IXI_HIGH',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter COUPLING_IXI_HIGH'

 call read_value_integer_tele(IIN,IGNORE_JUNK,COUPLING_IETA_LOW, 'SEMtoTele.COUPLING_IETA_LOW',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter COUPLING_IETA_LOW'

 call read_value_integer_tele(IIN,IGNORE_JUNK,COUPLING_IETA_HIGH, 'SEMtoTele.COUPLING_IETA_HIGH',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter COUPLING_IETA_HIGH'

 call read_value_integer_tele(IIN,IGNORE_JUNK,COUPLING_IR_TOP, 'SEMtoTele.COUPLING_IR_TOP',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter COUPLING_IR_TOP'

 call read_value_integer_tele(IIN,IGNORE_JUNK,COUPLING_IR_BOTTOM, 'SEMtoTele.COUPLING_IR_BOTTOM',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter COUPLING_IR_BOTTOM'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,COUPLING_DEPTH_TOLERENCE,'SEMtoTele.COUPLING_DEPTH_TOLERENCE',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter COUPLING_DEPTH_TOLERENCE'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,COUPLING_DIST_TOLERENCE,'SEMtoTele.COUPLING_DIST_TOLERENCE',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter COUPLING_DIST_TOLERENCE'


 call read_value_double_precision_tele(IIN,IGNORE_JUNK,LAT_CENTER_STRAIN_SAVED,'SEMtoTele.LAT_CENTER_STRAIN_SAVED',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter LAT_CENTER_STRAIN_SAVED'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,LON_CENTER_STRAIN_SAVED,'SEMtoTele.LON_CENTER_STRAIN_SAVED',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter LON_CENTER_STRAIN_SAVED'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,RADIUS_STRAIN_SAVED,'SEMtoTele.RADIUS_STRAIN_SAVED',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter RADIUS_STRAIN_SAVED'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,MIN_DEP_STRAIN_SAVED,'SEMtoTele.MIN_DEP_STRAIN_SAVED',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter MIN_DEP_STRAIN_SAVED'

 call read_value_double_precision_tele(IIN,IGNORE_JUNK,MAX_DEP_STRAIN_SAVED,'SEMtoTele.MAX_DEP_STRAIN_SAVED',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter MAX_DEP_STRAIN_SAVED'


 call read_value_logical_tele(IIN,IGNORE_JUNK,LOW_RESOLUTION,'SEMtoTele.LOW_RESOLUTION',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter LOW_RESOLUTION'

 call read_value_integer_tele(IIN,IGNORE_JUNK,NSTEP_BETWEEN_OUTPUTBOUND,'SEMtoTele.NSTEP_BETWEEN_OUTPUTBOUND',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter NSTEP_BETWEEN_OUTPUTBOUND'

 call read_value_integer_tele(IIN,IGNORE_JUNK,DECIMATE_COUPLING,'SEMtoTele.DECIMATE_COUPLING',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter DECIMATE_COUPLING'

 call read_value_integer_tele(IIN,IGNORE_JUNK,NPOINTS_PER_PACK,'SEMtoTele.NPOINTS_PER_PACK',ier)
 if(ier /= 0) stop 'Error reading SEMtoTele parameter NPOINTS_PER_PACK'

! close parameter file
 close(IIN)
end subroutine read_parameter_SEMtoTele




! read values from parameter file, ignoring white lines and comments

  subroutine read_value_integer_tele(iunit,ignore_junk,value_to_read, name,ier)

  implicit none

  logical ignore_junk
  integer iunit
  integer value_to_read
  character(len=*) name
  character(len=100) string_read
  integer :: ier

  call unused_string_tele(name)

  call read_next_line(iunit,ignore_junk,string_read,ier)
  if (ier /= 0) return

  read(string_read,*) value_to_read

  end subroutine read_value_integer_tele

!--------------------

  subroutine read_value_double_precision_tele(iunit,ignore_junk,value_to_read, name,ier)

  implicit none

  logical ignore_junk
  integer iunit
  double precision value_to_read
  character(len=*) name
  character(len=100) string_read
  integer :: ier

  call unused_string_tele(name)

  call read_next_line(iunit,ignore_junk,string_read,ier)
  if (ier /= 0) return

  read(string_read,*) value_to_read

  end subroutine read_value_double_precision_tele

!--------------------

  subroutine read_value_logical_tele(iunit,ignore_junk,value_to_read, name,ier)

  implicit none

  logical ignore_junk
  logical value_to_read
  integer iunit
  character(len=*) name
  character(len=100) string_read
  integer :: ier

  call unused_string_tele(name)

  call read_next_line(iunit,ignore_junk,string_read,ier)
  if (ier /= 0) return

  read(string_read,*) value_to_read

  end subroutine read_value_logical_tele

!--------------------

  subroutine read_value_string_tele(iunit,ignore_junk,value_to_read, name,ier)

  implicit none

  logical ignore_junk
  integer iunit
  character(len=*) value_to_read
  character(len=*) name
  character(len=100) string_read
  integer :: ier

  call unused_string_tele(name)

  call read_next_line(iunit,ignore_junk,string_read,ier)
  if (ier /= 0) return

  value_to_read = string_read

  end subroutine read_value_string_tele

!--------------------

!--------------------

  subroutine read_next_line(iunit,suppress_junk,string_read,ier)

  implicit none

  include "constants.h"


  logical suppress_junk
  character(len=100) string_read
  integer index_equal_sign,ier,iunit

  ier = 0
  do
    read(unit=iunit,fmt="(a100)",iostat=ier) string_read
    if(ier /= 0) stop 'error while reading parameter file'

! suppress leading white spaces, if any
    string_read = adjustl(string_read)

! suppress trailing carriage return (ASCII code 13) if any (e.g. if input text
! file coming from Windows/DOS)
    if(index(string_read,achar(13)) > 0) string_read = string_read(1:index(string_read,achar(13))-1)

! exit loop when we find the first line that is not a comment or a white line
    if(len_trim(string_read) == 0) cycle
    if(string_read(1:1) /= '#') exit

  enddo

! suppress trailing white spaces, if any
  string_read = string_read(1:len_trim(string_read))

! suppress trailing comments, if any
  if(index(string_read,'#') > 0) string_read = string_read(1:index(string_read,'#')-1)

  if(suppress_junk) then
! suppress leading junk (up to the first equal sign, included)
     index_equal_sign = index(string_read,'=')
     if(index_equal_sign <= 1 .or. index_equal_sign == len_trim(string_read)) stop 'incorrect syntax detected in Mesh_Par_file'
     string_read = string_read(index_equal_sign + 1:len_trim(string_read))
  end if

! suppress leading and trailing white spaces again, if any, after having
! suppressed the leading junk
  string_read = adjustl(string_read)
  string_read = string_read(1:len_trim(string_read))

  end subroutine read_next_line

!--------------------

!--------------------

  integer function err_occurred_tele()

  err_occurred_tele = 0

  end function err_occurred_tele

!--------------------

! dummy subroutine to avoid warnings about variable not used in other
! subroutines
  subroutine unused_string_tele(s)

  character(len=*) s

  if (len(s) == 1) continue

  end subroutine unused_string_tele
