subroutine read_parameter_coupling(Imain_coupling,R_TOP_BOUND,NXI_TOPOGRAPHY_TAPER,NETA_TOPOGRAPHY_TAPER,NXI_NO_TOPOGRAPHY,&
                         NETA_NO_TOPOGRAPHY,COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH, &
                         COUPLING_IR_TOP,COUPLING_IR_BOTTOM,NEX_XI,NEX_ETA,NER,NDOUBLINGS,USE_REGULAR_MESH,&
                         ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES,&
                         CENTER_LATITUDE_IN_DEGREES,CENTER_LONGITUDE_IN_DEGREES,GAMMA_ROTATION_AZIMUTH,&      
                         rotation_matrix,ner_doublings,myrank)

implicit none
include "constants.h"

integer::NXI_TOPOGRAPHY_TAPER,NETA_TOPOGRAPHY_TAPER,NXI_NO_TOPOGRAPHY,NETA_NO_TOPOGRAPHY
integer::COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,COUPLING_IR_TOP,COUPLING_IR_BOTTOM
integer::NEX_XI,NEX_ETA,NER,NDOUBLINGS
double precision ::R_TOP_BOUND,ANGULAR_WIDTH_XI_IN_DEGREES,ANGULAR_WIDTH_ETA_IN_DEGREES, &
          CENTER_LATITUDE_IN_DEGREES,CENTER_LONGITUDE_IN_DEGREES,GAMMA_ROTATION_AZIMUTH

! rotation matrix from Euler angles
double precision, dimension(NDIM,NDIM) :: rotation_matrix

logical USE_REGULAR_MESH
integer::ner_doublings(2)
integer::myrank
integer::Imain_coupling

integer, external :: err_occurred

! open parameter file
 open(unit=IIN,file=MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES)) &
       //'Coupling_Par_file',status='old',action='read')

 call read_value_double_precision(IIN,IGNORE_JUNK,R_TOP_BOUND,'coupling.R_TOP_BOUND')
 if(err_occurred() /= 0) stop 'Error reading coupling parameter R_TOP_BOUND'

 call read_value_double_precision(IIN,IGNORE_JUNK,ANGULAR_WIDTH_XI_IN_DEGREES,'coupling.ANGULAR_WIDTH_XI_IN_DEGREES')
 if(err_occurred() /= 0) stop 'Error reading coupling parameter ANGULAR_WIDTH_XI_IN_DEGREES'

 call read_value_double_precision(IIN,IGNORE_JUNK,ANGULAR_WIDTH_ETA_IN_DEGREES,'coupling.ANGULAR_WIDTH_ETA_IN_DEGREES')
 if(err_occurred() /= 0) stop 'Error reading coupling parameter ANGULAR_WIDTH_ETA_IN_DEGREES'

 call read_value_double_precision(IIN,IGNORE_JUNK,CENTER_LATITUDE_IN_DEGREES,'coupling.CENTER_LATITUDE_IN_DEGREES')
 if(err_occurred() /= 0) stop 'Error reading coupling parameter CENTER_LATITUDE_IN_DEGREES'

 call read_value_double_precision(IIN,IGNORE_JUNK,CENTER_LONGITUDE_IN_DEGREES,'coupling.CENTER_LONGITUDE_IN_DEGREES')
 if(err_occurred() /= 0) stop 'Error reading coupling parameter CENTER_LONGITUDE_IN_DEGREES'

 call read_value_double_precision(IIN,IGNORE_JUNK,GAMMA_ROTATION_AZIMUTH,'coupling.GAMMA_ROTATION_AZIMUTH')
 if(err_occurred() /= 0) stop 'Error reading coupling parameter GAMMA_ROTATION_AZIMUTH'

 call euler_angles(rotation_matrix,CENTER_LONGITUDE_IN_DEGREES,CENTER_LATITUDE_IN_DEGREES,GAMMA_ROTATION_AZIMUTH)

 call read_value_integer(IIN,IGNORE_JUNK,NXI_TOPOGRAPHY_TAPER, 'coupling.NXI_TOPOGRAPHY_TAPER')
 if(err_occurred() /= 0) return
 call read_value_integer(IIN,IGNORE_JUNK,NETA_TOPOGRAPHY_TAPER, 'coupling.NETA_TOPOGRAPHY_TAPER')
 if(err_occurred() /= 0) return
 call read_value_integer(IIN,IGNORE_JUNK,NXI_NO_TOPOGRAPHY, 'coupling.NXI_NO_TOPOGRAPHY')
 if(err_occurred() /= 0) return
 call read_value_integer(IIN,IGNORE_JUNK,NETA_NO_TOPOGRAPHY, 'coupling.NETA_NO_TOPOGRAPHY')
 if(err_occurred() /= 0) return
 call read_value_integer(IIN,IGNORE_JUNK,COUPLING_IXI_LOW, 'coupling.COUPLING_IXI_LOW')
 if(err_occurred() /= 0) return
 call read_value_integer(IIN,IGNORE_JUNK,COUPLING_IXI_HIGH, 'coupling.COUPLING_IXI_HIGH')
 if(err_occurred() /= 0) return
 call read_value_integer(IIN,IGNORE_JUNK,COUPLING_IETA_LOW, 'coupling.COUPLING_IETA_LOW')
 if(err_occurred() /= 0) return
 call read_value_integer(IIN,IGNORE_JUNK,COUPLING_IETA_HIGH, 'coupling.COUPLING_IETA_HIGH')
 if(err_occurred() /= 0) return
 call read_value_integer(IIN,IGNORE_JUNK,COUPLING_IR_TOP, 'coupling.COUPLING_IR_TOP')
 if(err_occurred() /= 0) return
 call read_value_integer(IIN,IGNORE_JUNK,COUPLING_IR_BOTTOM, 'coupling.COUPLING_IR_BOTTOM')
 if(err_occurred() /= 0) return


! close parameter file
 close(IIN)
 if(myrank.eq.0) then
     write(Imain_coupling,*) NXI_TOPOGRAPHY_TAPER,' x-elements are within topography-taper range'
     write(Imain_coupling,*) NXI_TOPOGRAPHY_TAPER,' y-elements are within topography-taper range'
     write(Imain_coupling,*) 'The x-low coupling boundary elements is the ones with xi=',COUPLING_IXI_LOW
     write(Imain_coupling,*) 'The x-high coupling boundary elements is the ones with xi=',COUPLING_IXI_HIGH
     write(Imain_coupling,*) 'The y-low boundary elements is the ones with yi=',COUPLING_IETA_LOW
     write(Imain_coupling,*) 'The y-high coupling boundary elements is the ones with xi=',COUPLING_IETA_HIGH
     write(Imain_coupling,*) 'The r-top coupling boundary elements is the ones with ri=',COUPLING_IR_TOP
     write(Imain_coupling,*) 'The r-bottom coupling boundary elements is the ones with ri=',COUPLING_IR_BOTTOM
     write(Imain_coupling,*)
 end if

 if(COUPLING_IXI_LOW.lt.0.or.COUPLING_IXI_HIGH.gt.NEX_XI.or.COUPLING_IETA_LOW.lt.0 &
    .or.COUPLING_IETA_HIGH.gt.NEX_ETA.or.COUPLING_IR_TOP.gt.NER.or.COUPLING_IR_TOP.lt.0&
    .or.COUPLING_IR_BOTTOM.gt.NER.or.COUPLING_IR_BOTTOM.lt.0) then
       stop 'The coupling boundary elements are out of the simulated area'

 end if


 if(COUPLING_IXI_LOW.gt.COUPLING_IXI_HIGH.or.COUPLING_IETA_LOW.gt.COUPLING_IETA_HIGH) then
       stop 'The element id at low xi (or eta) must be greater than that at high xi (or eta)'
 end if

 if(COUPLING_IXI_LOW.ge.NXI_NO_TOPOGRAPHY+1.or.COUPLING_IETA_LOW.ge.NETA_NO_TOPOGRAPHY+1.or.&
    (NEX_XI-COUPLING_IXI_HIGH).ge.NXI_NO_TOPOGRAPHY+1.or.(NEX_ETA-COUPLING_IETA_HIGH).ge.NETA_NO_TOPOGRAPHY+1) then
     stop 'Error:  topography detected at coupling boundary!!!'
 end if

 if(NDOUBLINGS.eq.1.and.(.not.USE_REGULAR_MESH)) then
!       if(modulo(COUPLING_IXI_LOW-1,4).ne.0.or.modulo(COUPLING_IXI_HIGH,4).ne.0.or. &
!          modulo(COUPLING_IETA_LOW-1,4).ne.0.or.modulo(COUPLING_IETA_HIGH,4).ne.0) then
!            call exit_MPI(myrank,'for 1 doubling layer,COUPLING_IXI_LOW,COUPLING_IXI_LOW,COUPLING_IETA_LOW, &
!                        COUPLING_IETA_HIGH should be divided evenly by 4')
!       end if
       if(modulo(COUPLING_IXI_LOW-1,4).ne.0) COUPLING_IXI_LOW=((COUPLING_IXI_LOW-1)/4)*4+1
       if(modulo(COUPLING_IXI_HIGH,4).ne.0)  COUPLING_IXI_HIGH=((COUPLING_IXI_HIGH-1)/4)*4
       if(modulo(COUPLING_IETA_LOW-1,4).ne.0) COUPLING_IETA_LOW=((COUPLING_IETA_LOW-1)/4)*4+1
       if(modulo(COUPLING_IETA_HIGH,4).ne.0)  COUPLING_IETA_HIGH=((COUPLING_IETA_HIGH-1)/4)*4
       if(COUPLING_IR_TOP.eq.ner_doublings(1)) COUPLING_IR_TOP=ner_doublings(1)-1
       if(COUPLING_IR_BOTTOM.eq.ner_doublings(1)) COUPLING_IR_BOTTOM=ner_doublings(1)-1
 else if(NDOUBLINGS.eq.2.and.(.not.USE_REGULAR_MESH)) then
!       if(modulo(COUPLING_IXI_LOW-1,8).ne.0.or.modulo(COUPLING_IXI_HIGH,8).ne.0.or. &
!          modulo(COUPLING_IETA_LOW-1,8).ne.0.or.modulo(COUPLING_IETA_HIGH,8).ne.0) then
!          call exit_MPI(myrank,'for 1 doubling layer,COUPLING_IXI_LOW,COUPLING_IXI_LOW,COUPLING_IETA_LOW, &
!                        COUPLING_IETA_HIGH should be divided evenly by 8')
!       end if

       if(modulo(COUPLING_IXI_LOW-1,8).ne.0) COUPLING_IXI_LOW=((COUPLING_IXI_LOW-1)/8)*8+1
       if(modulo(COUPLING_IXI_HIGH,8).ne.0)  COUPLING_IXI_HIGH=((COUPLING_IXI_HIGH-1)/8)*8
       if(modulo(COUPLING_IETA_LOW-1,8).ne.0) COUPLING_IETA_LOW=((COUPLING_IETA_LOW-1)/8)*8+1
       if(modulo(COUPLING_IETA_HIGH,8).ne.0)  COUPLING_IETA_HIGH=((COUPLING_IETA_HIGH-1)/8)*8
       if(COUPLING_IR_TOP.eq.ner_doublings(1)) then
          COUPLING_IR_TOP=ner_doublings(1)-1
       else if(COUPLING_IR_TOP.eq.ner_doublings(2)) then
          COUPLING_IR_TOP=ner_doublings(2)-1
       end if
       if(COUPLING_IR_BOTTOM.eq.ner_doublings(1)) then
          COUPLING_IR_BOTTOM=ner_doublings(1)-1
       else if(COUPLING_IR_BOTTOM.eq.ner_doublings(2)) then
          COUPLING_IR_BOTTOM=ner_doublings(2)-1
       end if

 end if
   
 if(COUPLING_IXI_LOW.lt.0.or.COUPLING_IXI_HIGH.gt.NEX_XI.or.COUPLING_IETA_LOW.lt.0 &
    .or.COUPLING_IETA_HIGH.gt.NEX_ETA.or.COUPLING_IR_TOP.gt.NER.or.COUPLING_IR_TOP.lt.0&
    .or.COUPLING_IR_BOTTOM.gt.NER.or.COUPLING_IR_BOTTOM.lt.0) then
       stop 'The coupling elements are out of the simulated area'
 end if

 if(myrank.eq.0) then
    write (Imain_coupling,*) 'There ',NDOUBLINGS,'doubling layers'
    write(Imain_coupling,*)  'The new bounddary elements is xi-low=',COUPLING_IXI_LOW
    write(Imain_coupling,*)  '                              xi-high=',COUPLING_IXI_HIGH
    write(Imain_coupling,*)  '                              iy-Low=',COUPLING_IETA_LOW
    write(Imain_coupling,*)  '                              iy-high=',COUPLING_IETA_HIGH
    write(Imain_coupling,*)  '                              ri-up=',COUPLING_IR_TOP

    write(Imain_coupling,*)  '                              ri-down=',COUPLING_IR_BOTTOM
    write(Imain_coupling,*)
    write(Imain_coupling,*)
    write(Imain_coupling,*)
    write(Imain_coupling,*)
 end if
end subroutine read_parameter_coupling
