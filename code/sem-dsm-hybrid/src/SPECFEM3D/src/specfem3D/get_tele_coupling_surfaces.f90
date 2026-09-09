subroutine get_tele_coupling_surfaces()
  use tele_coupling_par
  use specfem_par
  use specfem_par_elastic
  use specfem_par_acoustic
  use specfem_par_poroelastic
  use constants, only: MAX_STRING_LEN

  implicit none

!other variables
integer ::ispec,iface,ipoint_elastic,ipoint_elas_acous,ipoint_iface
integer ::iface_coupling,iface_coupling_elastic
integer,target ::i,j,k
integer ::ipackage
integer        ::face_type
integer ::imax,imin,di,jmax,jmin,dj,kmax,kmin,dk
integer, pointer::index1,index2


double precision, dimension(NDIM2D,NGNOD2D,NGLLY,NGLLZ):: dershape2D_x
double precision, dimension(NDIM2D,NGNOD2D,NGLLX,NGLLZ):: dershape2D_y
double precision, dimension(NDIM2D,NGNOD2D,NGLLX,NGLLY):: dershape2D_bottom
double precision, dimension(NDIM2D,NGNOD2D,NGLLX,NGLLY):: dershape2D_top
double precision, dimension(NGNOD2D,NGLLY,NGLLZ)       :: shape2D_x,shape2D_y,&
                                                          shape2D_bottom,shape2D_top
!double precision, dimension(NGNOD2D)                   ::xcoord,ycoord,zcoord
real(kind=CUSTOM_REAL),dimension(NGLLX,NGLLY)          :: jacobian2Dw_face
!real(kind=CUSTOM_REAL),dimension(NGLLX,NGLLY,NGLLZ)    :: jacobian2D
real(kind=CUSTOM_REAL)                                 :: jacobian_whole_face
real(kind=CUSTOM_REAL),dimension(NDIM,NGLLX,NGLLY)     ::normal_face

real(kind=CUSTOM_REAL) ::lambdal,mu
real(kind=CUSTOM_REAL) ::c11,c12,c13,c14,c15,c16,&
            c22,c23,c24,c25,c26,c33,&
            c34,c35,c36,c44,c45,c46,&
            c55,c56,c66

character(len=MAX_STRING_LEN) :: final_LOCAL_PATH,clean_LOCAL_PATH
character(len=MAX_STRING_LEN) :: dir_this_proc
! integer ::ier


clean_LOCAL_PATH = adjustl(LOCAL_PATH)
final_LOCAL_PATH = clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH)) 
dir_this_proc=trim(final_LOCAL_PATH)
write(dir_this_proc,"(a,'iproc',i4.4,'/')")trim(dir_this_proc),myrank

! get the 2-D shape functions
call get_shape2D(myrank,shape2D_x,dershape2D_x,yigll,zigll,NGLLY,NGLLZ,NGNOD,NGNOD2D)
call get_shape2D(myrank,shape2D_y,dershape2D_y,xigll,zigll,NGLLX,NGLLZ,NGNOD,NGNOD2D)
call get_shape2D(myrank,shape2D_bottom,dershape2D_bottom,xigll,yigll,NGLLX,NGLLY,NGNOD,NGNOD2D)
call get_shape2D(myrank,shape2D_top,dershape2D_top,xigll,yigll,NGLLX,NGLLY,NGNOD,NGNOD2D)


ipackage=0
iface_coupling_elastic=0

do iface_coupling=1,nele_tele_coupling
    face_type=coupling_ele_property(iface_coupling)%face_type
    select case(face_type)
    case(1)
      index1=>j;index2=>k
      iface=1
    case(2)
      index1=>j;index2=>k
      iface=2
    case(3)
      index1=>i;index2=>k
      iface=3
    case(4)
      index1=>i;index2=>k
      iface=4
   case(5)
     index1=>i;index2=>j
     iface=5
   case(6)
     index1=>i;index2=>j
     iface=6
   case default
     stop 'Error boundary type'
   end select


   ispec=coupling_ele_property(iface_coupling)%ispec_coupling
   if(ispec_is_elastic(ispec)) iface_coupling_elastic=iface_coupling_elastic+1

   call get_jacobian_boundary_face(myrank,NSPEC_AB, &
               xstore,ystore,zstore,ibool,NGLOB_AB,&
               dershape2D_x,dershape2D_y,dershape2D_bottom,dershape2D_top,&
               dble(wgllwgll_xy),dble(wgllwgll_xz),dble(wgllwgll_yz),&
               ispec,iface,jacobian2Dw_face,normal_face,NGLLX,NGLLY,NGNOD2D)

   if(LOW_RESOLUTION) then
     jacobian_whole_face=0.d0
     do i=1,NGLLX
       do j=1,NGLLY
           jacobian_whole_face=jacobian_whole_face+jacobian2Dw_face(i,j)
       end do
     end do
   end if
   imin=coupling_ele_property(iface_coupling)%istart;imax=coupling_ele_property(iface_coupling)%iend
   jmin=coupling_ele_property(iface_coupling)%jstart;jmax=coupling_ele_property(iface_coupling)%jend
   kmin=coupling_ele_property(iface_coupling)%kstart;kmax=coupling_ele_property(iface_coupling)%kend
   di=1;dj=1;dk=1
   
   ipoint_iface=0
   do k=kmin,kmax,dk
     do j=jmin,jmax,dj
       do i=imin,imax,di
         if(ispec_is_elastic(ispec)) then
            if(.not.ANISOTROPY) then
              mu=mustore(i,j,k,ispec)
              lambdal=kappastore(i,j,k,ispec)-2.d0/3.d0*mustore(i,j,k,ispec)
              c11=lambdal+TWO*mu; c12=lambdal; c13=lambdal
              c14=0.d0;           c15=0.d0;   c16=0.d0
              c22=lambdal+TWO*mu; c23=lambdal; c24=0.d0
              c25=0.d0;           c26=0.d0;   
              c33=lambdal+TWO*mu; c34=0.d0;    c35=0.d0
              c36=0.d0
              c44=mu;             c45=0.d0;    c46=0.d0
              c55=mu;             c56=0.d0
              c66=mu
            else
              c11=c11store(i,j,k,ispec); c12=c12store(i,j,k,ispec)
              c13=c13store(i,j,k,ispec); c14=c14store(i,j,k,ispec)
              c15=c15store(i,j,k,ispec); c16=c16store(i,j,k,ispec)
              c22=c22store(i,j,k,ispec); c23=c23store(i,j,k,ispec)
              c24=c24store(i,j,k,ispec); c25=c25store(i,j,k,ispec)
              c26=c26store(i,j,k,ispec); c33=c33store(i,j,k,ispec)
              c34=c34store(i,j,k,ispec); c35=c35store(i,j,k,ispec)
              c36=c36store(i,j,k,ispec); c44=c44store(i,j,k,ispec)
              c45=c45store(i,j,k,ispec); c46=c46store(i,j,k,ispec)
              c55=c55store(i,j,k,ispec); c56=c56store(i,j,k,ispec)
              c66=c66store(i,j,k,ispec)
            end if
          end if

           if(LOW_RESOLUTION) then
               ipoint_elas_acous=iface_coupling
               ipoint_elastic=iface_coupling_elastic
           else
               ipoint_iface=ipoint_iface+1
               ipoint_elas_acous=(iface_coupling-1)*NGLLX*NGLLY+ipoint_iface
               ipoint_elastic=(iface_coupling_elastic-1)*NGLLX*NGLLY+ipoint_iface
           end if
!          if(DEBUG_COUPLING) print *,'normal',iface,normal_face(:,index1,index2),&
!                   xstore(ibool(i,j,k,ispec)),ystore(ibool(i,j,k,ispec)),&
!                   zstore(ibool(i,j,k,ispec))
           if(iface.eq.5.or.iface.eq.1.or.iface.eq.4) then
              normal_face(:,index1,index2)=-normal_face(:,index1,index2)
           end if
!          In the representation thoerem, the normal vector points out of the
!          coupling box.
           normal_vect_coupling(:,ipoint_elas_acous)=-normal_face(:,index1,index2)
       end do
     end do
   end do
end do

end subroutine get_tele_coupling_surfaces
