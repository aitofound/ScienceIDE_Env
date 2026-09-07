subroutine read_parameter_coupling(myrank)

!include "constants.h"

use constants
use coupling_mesh_par
implicit none


integer myrank
integer :: ier


 if(myrank == 0 .and. Imain_coupling /= ISTANDARD_OUTPUT) &
       open(unit=Imain_coupling,file=trim(OUTPUT_FILES)//'/output_coupling.txt',status='unknown')
 if(myrank.eq.0) then
      write(Imain_coupling,*)
      write(Imain_coupling,*) '******************************************'
      write(Imain_coupling,*) '*** COUPLING WITH SEM (WENBO WU)***'
      write(Imain_coupling,*) '******************************************'
      write(Imain_coupling,*)
      write(Imain_coupling,*) 'Reading the coupling boundary information from file ',&
                MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES))//'Coupling_Par_file'
 end if

! open parameter file
 open(unit=IIN,file=MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES)) &
       //'Coupling_Par_file',status='old',action='read')

 call read_value_dble_precision_mesh(IIN,IGNORE_JUNK,R_TOP_BOUND,'coupling.R_TOP_BOUND',ier)
 if(ier /= 0) stop 'Error reading coupling parameter R_TOP_BOUND'

 call read_value_dble_precision_mesh(IIN,IGNORE_JUNK,ANGULAR_WIDTH_XI_IN_DEGREES,'coupling.ANGULAR_WIDTH_XI_IN_DEGREES',ier)
 if(ier /= 0) stop 'Error reading coupling parameter ANGULAR_WIDTH_XI_IN_DEGREES'

 call read_value_dble_precision_mesh(IIN,IGNORE_JUNK,ANGULAR_WIDTH_ETA_IN_DEGREES,'coupling.ANGULAR_WIDTH_ETA_IN_DEGREES',ier)
 if(ier /= 0) stop 'Error reading coupling parameter ANGULAR_WIDTH_ETA_IN_DEGREES'

 call read_value_dble_precision_mesh(IIN,IGNORE_JUNK,CENTER_LATITUDE_IN_DEGREES,'coupling.CENTER_LATITUDE_IN_DEGREES',ier)
 if(ier /= 0) stop 'Error reading coupling parameter CENTER_LATITUDE_IN_DEGREES'

 call read_value_dble_precision_mesh(IIN,IGNORE_JUNK,CENTER_LONGITUDE_IN_DEGREES,'coupling.CENTER_LONGITUDE_IN_DEGREES',ier)
 if(ier /= 0) stop 'Error reading coupling parameter CENTER_LONGITUDE_IN_DEGREES'

 call read_value_dble_precision_mesh(IIN,IGNORE_JUNK,GAMMA_ROTATION_AZIMUTH,'coupling.GAMMA_ROTATION_AZIMUTH',ier)
 if(ier /= 0) stop 'Error reading coupling parameter GAMMA_ROTATION_AZIMUTH'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,NXI_TOPOGRAPHY_TAPER, 'coupling.NXI_TOPOGRAPHY_TAPER',ier)
 if(ier /= 0) stop 'Error reading coupling parameter NXI_TOPOGRAPHY_TAPER'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,NETA_TOPOGRAPHY_TAPER, 'coupling.NETA_TOPOGRAPHY_TAPER',ier)
 if(ier /= 0) stop 'Error reading coupling parameter NETA_TOPOGRAPHY_TAPER'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,NXI_NO_TOPOGRAPHY, 'coupling.NXI_NO_TOPOGRAPHY',ier)
 if(ier /= 0) stop 'Error reading coupling parameter NXI_NO_TOPOGRAPHY'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,NETA_NO_TOPOGRAPHY, 'coupling.NETA_NO_TOPOGRAPHY',ier)
 if(ier /= 0) stop 'Error reading coupling parameter NETA_NO_TOPOGRAPHY'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,COUPLING_IXI_LOW, 'coupling.COUPLING_IXI_LOW',ier)
 if(ier /= 0) stop 'Error reading coupling parameter COUPLING_IXI_LOW'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,COUPLING_IXI_HIGH, 'coupling.COUPLING_IXI_HIGH',ier)
 if(ier /= 0) stop 'Error reading coupling parameter COUPLING_IXI_HIGH'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,COUPLING_IETA_LOW, 'coupling.COUPLING_IETA_LOW',ier)
 if(ier /= 0) stop 'Error reading coupling parameter COUPLING_IETA_LOW'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,COUPLING_IETA_HIGH, 'coupling.COUPLING_IETA_HIGH',ier)
 if(ier /= 0) stop 'Error reading coupling parameter COUPLING_IETA_HIGH'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,COUPLING_IR_TOP, 'coupling.COUPLING_IR_TOP',ier)
 if(ier /= 0) stop 'Error reading coupling parameter COUPLING_IR_TOP'

 call read_value_integer_mesh(IIN,IGNORE_JUNK,COUPLING_IR_BOTTOM, 'coupling.COUPLING_IR_BOTTOM',ier)
 if(ier /= 0) stop 'Error reading coupling parameter COUPLING_IR_BOTTOM'

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
end subroutine read_parameter_coupling


!**************************************************
subroutine  prepare_variables_coupling(myrank)
use constants
use coupling_mesh_par
use meshfem3D_par, only:NEX_XI,NEX_ETA,NER,&
        NEX_PER_PROC_XI,NEX_PER_PROC_ETA,NDOUBLINGS,&
        USE_REGULAR_MESH,ner_doublings,iproc_xi_current,iproc_eta_current
implicit none

integer myrank
double precision::x_temp(NGNOD_EIGHT_CORNERS),y_temp(NGNOD_EIGHT_CORNERS),&
                   z_temp(NGNOD_EIGHT_CORNERS)
double precision ::radi,theta,phi,lat_corner,lon_corner
integer :: icorner



 if(COUPLING_IXI_LOW.lt.0.or.COUPLING_IXI_HIGH.gt.NEX_XI.or.COUPLING_IETA_LOW.lt.0 &
    .or.COUPLING_IETA_HIGH.gt.NEX_ETA.or.COUPLING_IR_TOP.gt.NER.or.COUPLING_IR_TOP.lt.0&
    .or.COUPLING_IR_BOTTOM.gt.NER.or.COUPLING_IR_BOTTOM.lt.0) then
      call exit_MPI(myrank,'The coupling boundary elements are out of the simulated area')      
 end if


 if(COUPLING_IXI_LOW.gt.COUPLING_IXI_HIGH.or.COUPLING_IETA_LOW.gt.COUPLING_IETA_HIGH) then
      call exit_MPI(myrank,'The element id at low xi (or eta) must be greater than that at high xi (or eta)')
 end if

 if(COUPLING_IXI_LOW.ge.NXI_NO_TOPOGRAPHY+1.or.COUPLING_IETA_LOW.ge.NETA_NO_TOPOGRAPHY+1.or.&
    (NEX_XI-COUPLING_IXI_HIGH).ge.NXI_NO_TOPOGRAPHY+1.or.(NEX_ETA-COUPLING_IETA_HIGH).ge.NETA_NO_TOPOGRAPHY+1) then
    !write(Imain_coupling,*)  'WARNING:coupling boundaries are with topography!!!!!'
    call exit_MPI(myrank,'Error: topography detected at coupling boundary!!!')
 end if
 if(dabs(R_TOP_BOUND-R_EARTH_SURF).lt.TINYVAL.and.COUPLING_IR_TOP.ne.NER) then
    call exit_MPI(myrank,'Error,the top boundary is free surface, COUPLING_IR_TOP must &
                  &be equal to NER!!!')
 end if 

 if(NDOUBLINGS.eq.1.and.(.not.USE_REGULAR_MESH)) then
       !The xi-low and yi-low coupling boundary elements are set as ID=4*multiple+1
       !The xi-high and yi-high coupling boundary elements are set as ID=4*multiple
       if(modulo(COUPLING_IXI_LOW-1,4).ne.0) COUPLING_IXI_LOW=((COUPLING_IXI_LOW-1)/4)*4+1
       if(modulo(COUPLING_IXI_HIGH,4).ne.0)  COUPLING_IXI_HIGH=((COUPLING_IXI_HIGH-1)/4)*4
       if(modulo(COUPLING_IETA_LOW-1,4).ne.0) COUPLING_IETA_LOW=((COUPLING_IETA_LOW-1)/4)*4+1
       if(modulo(COUPLING_IETA_HIGH,4).ne.0)  COUPLING_IETA_HIGH=((COUPLING_IETA_HIGH-1)/4)*4
       !If the rtop or rbottom coupling boundary elements are located on the
       !doubling layer, shift them with one element
       if(COUPLING_IR_TOP.eq.ner_doublings(1)) COUPLING_IR_TOP=ner_doublings(1)-1
       if(COUPLING_IR_BOTTOM.eq.ner_doublings(1)) COUPLING_IR_BOTTOM=ner_doublings(1)-1
 else if(NDOUBLINGS.eq.2.and.(.not.USE_REGULAR_MESH)) then
       !The xi-low and yi-low coupling boundary elements are set as ID=8*multiple+1
       !The xi-high and yi-high coupling boundary elements are set as ID=8*multiple
       if(modulo(COUPLING_IXI_LOW-1,8).ne.0) COUPLING_IXI_LOW=((COUPLING_IXI_LOW-1)/8)*8+1
       if(modulo(COUPLING_IXI_HIGH,8).ne.0)  COUPLING_IXI_HIGH=((COUPLING_IXI_HIGH-1)/8)*8
       if(modulo(COUPLING_IETA_LOW-1,8).ne.0) COUPLING_IETA_LOW=((COUPLING_IETA_LOW-1)/8)*8+1
       if(modulo(COUPLING_IETA_HIGH,8).ne.0)  COUPLING_IETA_HIGH=((COUPLING_IETA_HIGH-1)/8)*8
       !If the rtop or rbottom coupling boundary elements are located on the
       !doubling layer, shift them with one element
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
      call exit_MPI(myrank,'The coupling elements are out of the simulated area')
 end if


 if(myrank.eq.0) then
    write (Imain_coupling,*) 'There ',NDOUBLINGS,'doubling layers'
    write(Imain_coupling,*)  'The new coupling bounddary elements are xi-low=',COUPLING_IXI_LOW
    write(Imain_coupling,*)  '                                        xi-high=',COUPLING_IXI_HIGH
    write(Imain_coupling,*)  '                                        iy-Low=',COUPLING_IETA_LOW
    write(Imain_coupling,*)  '                                        iy-high=',COUPLING_IETA_HIGH
    write(Imain_coupling,*)  '                                        ri-top=',COUPLING_IR_TOP
    write(Imain_coupling,*)  '                                        ri-bottom=',COUPLING_IR_BOTTOM
    write(Imain_coupling,*)
    write(Imain_coupling,*)
    write(Imain_coupling,*)
    write(Imain_coupling,*)
 end if

 if(myrank.eq.0)  write(Imain_coupling,*) 'counting the number of the coupling boundary elements'

 call euler_angles(rotation_matrix,CENTER_LONGITUDE_IN_DEGREES,CENTER_LATITUDE_IN_DEGREES,GAMMA_ROTATION_AZIMUTH)

 call compute_parameter_coupling(COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,&
               COUPLING_IR_TOP,COUPLING_IR_BOTTOM,coupling_nspec_xlow,&
               coupling_nspec_xhigh,coupling_nspec_ylow,coupling_nspec_yhigh,coupling_nspec_rtop,&
               coupling_nspec_rbottom,iproc_xi_current,iproc_eta_current,&
               NEX_XI,NEX_ETA,NEX_PER_PROC_XI,NEX_PER_PROC_ETA,NER,&
               USE_REGULAR_MESH,NDOUBLINGS,ner_doublings,DEBUG_COUPLING,myrank)
 if(myrank.eq.0) then
  x_temp(1)=0.0
  y_temp(1)=0.0
  z_temp(1)=0.0

  x_temp(2)=ANGULAR_WIDTH_XI_IN_DEGREES
  y_temp(2)=0.0
  z_temp(2)=0.0

  x_temp(3)=ANGULAR_WIDTH_XI_IN_DEGREES
  y_temp(3)=ANGULAR_WIDTH_ETA_IN_DEGREES
  z_temp(3)=0.0

  x_temp(4)=0.0
  y_temp(4)=ANGULAR_WIDTH_ETA_IN_DEGREES
  z_temp(4)=0.0

  x_temp(5)=0.0
  y_temp(5)=0.0
  z_temp(5)=0.0

  x_temp(6)=ANGULAR_WIDTH_XI_IN_DEGREES
  y_temp(6)=0.0
  z_temp(6)=0.0

  x_temp(7)=ANGULAR_WIDTH_XI_IN_DEGREES
  y_temp(7)=ANGULAR_WIDTH_ETA_IN_DEGREES
  z_temp(7)=0.0

  x_temp(8)=0.0
  y_temp(8)=ANGULAR_WIDTH_ETA_IN_DEGREES
  z_temp(8)=0.0


  !compute the four corners of simulated region. 
  call cubed_geo(x_temp,y_temp,z_temp,R_TOP_BOUND,ANGULAR_WIDTH_XI_IN_DEGREES, &
           ANGULAR_WIDTH_ETA_IN_DEGREES,rotation_matrix)

  write(Imain_coupling,*) 'this simulated cubed sphere region is:'
  do icorner=1,4
    radi=dsqrt(x_temp(icorner)**2+y_temp(icorner)**2+z_temp(icorner)**2)
    theta=dacos(z_temp(icorner)/radi)
    if(y_temp(icorner)<0.0) then
       phi=2*PI-dacos(x_temp(icorner)*(1.d0-TINYVAL)/(radi*dsin(theta)))
    else
       phi=dacos(x_temp(icorner)*(1.d0-TINYVAL)/(radi*dsin(theta)))
    end if
    lat_corner= 180/PI*(PI/2-theta)
    lon_corner=phi/PI*180.0
    write(Imain_coupling,*) 'corner',icorner,'  lat=',lat_corner,' lon=',lon_corner
   end do

  write(Imain_coupling,*)
  write(Imain_coupling,*)
  write(Imain_coupling,*) 'the number of coupling boundary elements in proc0:'
  write(Imain_coupling,*) 'number of the coupling elements at x-low =',coupling_nspec_xlow
  write(Imain_coupling,*) 'number of the coupling elements at x-high =',coupling_nspec_xhigh
  write(Imain_coupling,*) 'number of the coupling elements at y-low =',coupling_nspec_ylow
  write(Imain_coupling,*) 'number of the coupling elements at y-high =',coupling_nspec_yhigh
  write(Imain_coupling,*) 'number of the coupling elements at r-top =',coupling_nspec_rtop
  write(Imain_coupling,*) 'number of the coupling elements at r-bottom =',coupling_nspec_rbottom
 end if

 allocate(coupling_ispec_xlow(coupling_nspec_xlow))
 allocate(coupling_isubregion_xlow(coupling_nspec_xlow))
 allocate(coupling_ispec_xhigh(coupling_nspec_xhigh))
 allocate(coupling_isubregion_xhigh(coupling_nspec_xhigh))
 allocate(coupling_ispec_ylow(coupling_nspec_ylow))
 allocate(coupling_isubregion_ylow(coupling_nspec_ylow))
 allocate(coupling_ispec_yhigh(coupling_nspec_yhigh))
 allocate(coupling_isubregion_yhigh(coupling_nspec_yhigh))
 allocate(coupling_ispec_rtop(coupling_nspec_rtop))
 allocate(coupling_isubregion_rtop(coupling_nspec_rtop))
 allocate(coupling_ispec_rbottom(coupling_nspec_rbottom))
 allocate(coupling_isubregion_rbottom(coupling_nspec_rbottom))
end subroutine prepare_variables_coupling
