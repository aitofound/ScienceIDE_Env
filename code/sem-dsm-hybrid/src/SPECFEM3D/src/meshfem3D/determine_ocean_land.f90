subroutine determine_ocean_land(xelm,yelm,zelm,imaterial_number,&
                            isubregion,nmeshregions,ir,ir1,ir2,iy,iy1,iy2,ix,ix1,ix2,&
                            iproc_xi,NPROC_XI,iproc_eta,NPROC_ETA,&
                            NXI_NO_TOPOGRAPHY,NETA_NO_TOPOGRAPHY)
  use constants
  implicit none
!  include "constants.h"

 double precision xelm(NGNOD_EIGHT_CORNERS),yelm(NGNOD_EIGHT_CORNERS),zelm(NGNOD_EIGHT_CORNERS)
 integer imaterial_number
 integer isubregion,nmeshregions,ir,ir1,ir2,iy,iy1,iy2,ix,ix1,ix2
 integer iproc_xi,NPROC_XI,iproc_eta,NPROC_ETA
 integer NXI_NO_TOPOGRAPHY,NETA_NO_TOPOGRAPHY
 
 !local parameters
 double precision z_thismesh,y_thismesh,x_thismesh
 integer,save:: npx_interface_topo,npy_interface_topo
 double precision,save:: orig_x_interface_topo,orig_y_interface_topo
 double precision,save:: spacing_x_interface_topo,spacing_y_interface_topo
 double precision,dimension(:,:), allocatable,save:: interface_topo
 integer ix_tmp,iy_tmp,ix_find,iy_find
 integer ignod


 if(isubregion.eq.1.and.ir.eq.ir1.and.ix.eq.ix1.and.iy.eq.iy1) then

   open(unit=45,file=MF_IN_DATA_FILES(1:len_trim(MF_IN_DATA_FILES))//'real_bathymetry_topography',status='old')
   read(45,*) npx_interface_topo,npy_interface_topo
   read(45,*) orig_x_interface_topo,orig_y_interface_topo
   read(45,*) spacing_x_interface_topo,spacing_y_interface_topo
   allocate(interface_topo(npx_interface_topo,npy_interface_topo))
 
   do iy_tmp=1,npy_interface_topo
     do ix_tmp=1,npx_interface_topo
         read(45,*) interface_topo(ix_tmp,iy_tmp)
     enddo
   enddo
   close(45)
 end if

 x_thismesh=0.d0
 y_thismesh=0.d0
 z_thismesh=0.d0
 do ignod=1,NGNOD_EIGHT_CORNERS
    x_thismesh=x_thismesh+xelm(ignod)
    y_thismesh=y_thismesh+yelm(ignod)
    z_thismesh=z_thismesh+zelm(ignod)
 end do
 x_thismesh=x_thismesh/NGNOD_EIGHT_CORNERS
 y_thismesh=y_thismesh/NGNOD_EIGHT_CORNERS
 z_thismesh=z_thismesh/NGNOD_EIGHT_CORNERS


 ix_find=int((x_thismesh - orig_x_interface_topo) / spacing_x_interface_topo) + 1
 iy_find = int((y_thismesh - orig_y_interface_topo) / spacing_y_interface_topo) + 1

!SEM box boundaries with no topograpny
 if((iproc_xi.eq.0.and.ix.le.NXI_NO_TOPOGRAPHY).or.&
    (iproc_xi.eq.NPROC_XI-1.and.ix.ge.ix2-2*NXI_NO_TOPOGRAPHY).or.&
    (iproc_eta.eq.0.and.iy.le.NETA_NO_TOPOGRAPHY).or.&
    (iproc_eta.eq.NPROC_ETA-1.and.iy.ge.iy2-2*NETA_NO_TOPOGRAPHY)) then 

! The media type (acoustic or elastic material) is determined based on the local topography.
! In some extreme cases, where the topography is steep, the linearly interpolated 
! z_thismesh value on land might exceed interfac_topo. This could lead to incorrect 
! classification of the element as ocean type. 
! To prevent this, an additional condition z_thismesh.lt.100 is applied to exclude such cases.
   if(z_thismesh.gt.interface_topo(ix_find,iy_find).and.z_thismesh.lt.100) then
    !acoustic
    imaterial_number=1
   else
    !elastic
    imaterial_number=2
   end if

!with topography
 else
   if(z_thismesh.gt.interface_topo(ix_find,iy_find).and.z_thismesh.lt.100) then
    !acoustic
    imaterial_number=1
   else
    !elastic
    imaterial_number=2
   end if
 end if

 if(isubregion.eq.nmeshregions.and.ir.eq.ir2.and.ix.eq.ix2.and.iy.eq.iy2) then
   deallocate(interface_topo)
 end if
end subroutine determine_ocean_land
