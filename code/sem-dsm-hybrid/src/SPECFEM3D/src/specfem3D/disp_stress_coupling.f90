subroutine compute_traction_disp_elastic()

  use specfem_par
  use specfem_par_elastic
  use specfem_par_acoustic
  use specfem_par_poroelastic
  use tele_coupling_par
  use constants,only: CUSTOM_REAL,NGLLX,NGLLY,NGLLZ,NDIM, &
                      N_SLS, &
                      ONE_THIRD

  implicit none

! local parameters

  real(kind=CUSTOM_REAL) xixl,xiyl,xizl,etaxl,etayl,etazl,gammaxl,gammayl,gammazl
  real(kind=CUSTOM_REAL) duxdxl,duxdyl,duxdzl,duydxl,duydyl,duydzl,duzdxl,duzdyl,duzdzl

  real(kind=CUSTOM_REAL) duxdxl_plus_duydyl,duxdxl_plus_duzdzl,duydyl_plus_duzdzl
  real(kind=CUSTOM_REAL) duxdyl_plus_duydxl,duzdxl_plus_duxdzl,duzdyl_plus_duydzl

  real(kind=CUSTOM_REAL) sigma_xxtpl,sigma_yytpl,sigma_zztpl,sigma_xytpl,sigma_xztpl,sigma_yztpl

  real(kind=CUSTOM_REAL) hp1,hp2,hp3

  real(kind=CUSTOM_REAL) tempx1l,tempx2l,tempx3l
  real(kind=CUSTOM_REAL) tempy1l,tempy2l,tempy3l
  real(kind=CUSTOM_REAL) tempz1l,tempz2l,tempz3l

  real(kind=CUSTOM_REAL) lambdal,mul,lambdalplus2mul
  real(kind=CUSTOM_REAL) kappal

  ! local anisotropy parameters
  real(kind=CUSTOM_REAL) c11,c12,c13,c14,c15,c16,c22,c23,c24,c25,c26,&
                        c33,c34,c35,c36,c44,c45,c46,c55,c56,c66

  ! local attenuation parameters
  real(kind=CUSTOM_REAL) R_xx_val,R_yy_val
  integer i_SLS

  real(kind=CUSTOM_REAL),dimension(NDIM) :: normal_vector
  integer iface_coupling,ispec,iglob,ipoint_iface,ipoint
  integer it_coupling,it_coupling_thispack
  integer i,j,k,l
  integer imax,imin,di,jmax,jmin,dj,kmax,kmin,dk


  it_coupling=(it-1)/DECIMATE_COUPLING+1
  it_coupling_thispack=mod(it_coupling,NSTEP_BETWEEN_OUTPUTBOUND)
  if(it_coupling_thispack.eq.0) it_coupling_thispack=NSTEP_BETWEEN_OUTPUTBOUND
   if(it_coupling_thispack.eq.1) traction_coupling(:,:,:)=0.d0

  do iface_coupling = 1,nele_tele_coupling

   ispec = coupling_ele_property(iface_coupling)%ispec_coupling

   if(ispec_is_elastic(ispec)) then
    imin=coupling_ele_property(iface_coupling)%istart;imax=coupling_ele_property(iface_coupling)%iend
    jmin=coupling_ele_property(iface_coupling)%jstart;jmax=coupling_ele_property(iface_coupling)%jend
    kmin=coupling_ele_property(iface_coupling)%kstart;kmax=coupling_ele_property(iface_coupling)%kend
    di=1;dj=1;dk=1

    ipoint_iface=0
    do k=kmin,kmax,di
      do j=jmin,jmax,dj
        do i=imin,imax,dk

          tempx1l = 0.
          tempx2l = 0.
          tempx3l = 0.

          tempy1l = 0.
          tempy2l = 0.
          tempy3l = 0.

          tempz1l = 0.
          tempz2l = 0.
          tempz3l = 0.

          do l=1,NGLLX
            hp1 = hprime_xx(i,l)
            iglob = ibool(l,j,k,ispec)
            tempx1l = tempx1l + displ(1,iglob)*hp1
            tempy1l = tempy1l + displ(2,iglob)*hp1
            tempz1l = tempz1l + displ(3,iglob)*hp1

            !!! can merge these loops because NGLLX = NGLLY = NGLLZ
            hp2 = hprime_yy(j,l)
            iglob = ibool(i,l,k,ispec)
            tempx2l = tempx2l + displ(1,iglob)*hp2
            tempy2l = tempy2l + displ(2,iglob)*hp2
            tempz2l = tempz2l + displ(3,iglob)*hp2

            !!! can merge these loops because NGLLX = NGLLY = NGLLZ
            hp3 = hprime_zz(k,l)
            iglob = ibool(i,j,l,ispec)
            tempx3l = tempx3l + displ(1,iglob)*hp3
            tempy3l = tempy3l + displ(2,iglob)*hp3
            tempz3l = tempz3l + displ(3,iglob)*hp3
          enddo

          ! get derivatives of ux, uy and uz with respect to x, y and z
          xixl = xix(i,j,k,ispec)
          xiyl = xiy(i,j,k,ispec)
          xizl = xiz(i,j,k,ispec)
          etaxl = etax(i,j,k,ispec)
          etayl = etay(i,j,k,ispec)
          etazl = etaz(i,j,k,ispec)
          gammaxl = gammax(i,j,k,ispec)
          gammayl = gammay(i,j,k,ispec)
          gammazl = gammaz(i,j,k,ispec)

          duxdxl = xixl*tempx1l + etaxl*tempx2l + gammaxl*tempx3l
          duxdyl = xiyl*tempx1l + etayl*tempx2l + gammayl*tempx3l
          duxdzl = xizl*tempx1l + etazl*tempx2l + gammazl*tempx3l

          duydxl = xixl*tempy1l + etaxl*tempy2l + gammaxl*tempy3l
          duydyl = xiyl*tempy1l + etayl*tempy2l + gammayl*tempy3l
          duydzl = xizl*tempy1l + etazl*tempy2l + gammazl*tempy3l

          duzdxl = xixl*tempz1l + etaxl*tempz2l + gammaxl*tempz3l
          duzdyl = xiyl*tempz1l + etayl*tempz2l + gammayl*tempz3l
          duzdzl = xizl*tempz1l + etazl*tempz2l + gammazl*tempz3l

          ! precompute some sums to save CPU time
          duxdxl_plus_duydyl = duxdxl + duydyl
          duxdxl_plus_duzdzl = duxdxl + duzdzl
          duydyl_plus_duzdzl = duydyl + duzdzl
          duxdyl_plus_duydxl = duxdyl + duydxl
          duzdxl_plus_duxdzl = duzdxl + duxdzl
          duzdyl_plus_duydzl = duzdyl + duydzl

          kappal = kappastore(i,j,k,ispec)
          mul = mustore(i,j,k,ispec)

          if(ATTENUATION) then
            ! use unrelaxed parameters if attenuation
            mul  = mul * one_minus_sum_beta(i,j,k,ispec)
          endif

          ! full anisotropic case, stress calculations
          if(ANISOTROPY) then
            c11 = c11store(i,j,k,ispec)
            c12 = c12store(i,j,k,ispec)
            c13 = c13store(i,j,k,ispec)
            c14 = c14store(i,j,k,ispec)
            c15 = c15store(i,j,k,ispec)
            c16 = c16store(i,j,k,ispec)
            c22 = c22store(i,j,k,ispec)
            c23 = c23store(i,j,k,ispec)
            c24 = c24store(i,j,k,ispec)
            c25 = c25store(i,j,k,ispec)
            c26 = c26store(i,j,k,ispec)
            c33 = c33store(i,j,k,ispec)
            c34 = c34store(i,j,k,ispec)
            c35 = c35store(i,j,k,ispec)
            c36 = c36store(i,j,k,ispec)
            c44 = c44store(i,j,k,ispec)
            c45 = c45store(i,j,k,ispec)
            c46 = c46store(i,j,k,ispec)
            c55 = c55store(i,j,k,ispec)
            c56 = c56store(i,j,k,ispec)
            c66 = c66store(i,j,k,ispec)

            sigma_xxtpl = c11*duxdxl + c16*duxdyl_plus_duydxl + c12*duydyl + &
                      c15*duzdxl_plus_duxdzl + c14*duzdyl_plus_duydzl + c13*duzdzl
            sigma_yytpl = c12*duxdxl + c26*duxdyl_plus_duydxl + c22*duydyl + &
                      c25*duzdxl_plus_duxdzl + c24*duzdyl_plus_duydzl + c23*duzdzl
            sigma_zztpl = c13*duxdxl + c36*duxdyl_plus_duydxl + c23*duydyl + &
                      c35*duzdxl_plus_duxdzl + c34*duzdyl_plus_duydzl + c33*duzdzl
            sigma_xytpl = c16*duxdxl + c66*duxdyl_plus_duydxl + c26*duydyl + &
                      c56*duzdxl_plus_duxdzl + c46*duzdyl_plus_duydzl + c36*duzdzl
            sigma_xztpl = c15*duxdxl + c56*duxdyl_plus_duydxl + c25*duydyl + &
                      c55*duzdxl_plus_duxdzl + c45*duzdyl_plus_duydzl + c35*duzdzl
            sigma_yztpl = c14*duxdxl + c46*duxdyl_plus_duydxl + c24*duydyl + &
                      c45*duzdxl_plus_duxdzl + c44*duzdyl_plus_duydzl + c34*duzdzl

          else

            ! isotropic case
            lambdalplus2mul = kappal + 4.d0/3.d0 * mul
            lambdal = lambdalplus2mul - 2.*mul

            ! compute stress sigma
            sigma_xxtpl = lambdalplus2mul*duxdxl + lambdal*duydyl_plus_duzdzl
            sigma_yytpl = lambdalplus2mul*duydyl + lambdal*duxdxl_plus_duzdzl
            sigma_zztpl = lambdalplus2mul*duzdzl + lambdal*duxdxl_plus_duydyl

            sigma_xytpl = mul*duxdyl_plus_duydxl
            sigma_xztpl = mul*duzdxl_plus_duxdzl
            sigma_yztpl = mul*duzdyl_plus_duydzl

          endif ! ANISOTROPY

          ! subtract memory variables if attenuation
          if(ATTENUATION) then
             do i_sls = 1,N_SLS
                R_xx_val = R_xx(i,j,k,ispec,i_sls)
                R_yy_val = R_yy(i,j,k,ispec,i_sls)
                sigma_xxtpl = sigma_xxtpl - R_xx_val
                sigma_yytpl = sigma_yytpl - R_yy_val
                sigma_zztpl = sigma_zztpl + R_xx_val + R_yy_val
                sigma_xytpl = sigma_xytpl - R_xy(i,j,k,ispec,i_sls)
                sigma_xztpl = sigma_xztpl - R_xz(i,j,k,ispec,i_sls)
                sigma_yztpl = sigma_yztpl - R_yz(i,j,k,ispec,i_sls)
             enddo
          endif
          if(LOW_RESOLUTION) then
               ipoint=iface_coupling
          else
               ipoint_iface=ipoint_iface+1
               ipoint=(iface_coupling-1)*NGLLX*NGLLY+ipoint_iface
          end if

          normal_vector(:)=normal_vect_coupling(:,ipoint)
          traction_coupling(it_coupling_thispack,1,ipoint)=normal_vector(1)*sigma_xxtpl+ &
               normal_vector(2)*sigma_xytpl+normal_vector(3)*sigma_xztpl
          traction_coupling(it_coupling_thispack,2,ipoint)=normal_vector(1)*sigma_xytpl+ &
               normal_vector(2)*sigma_yytpl+normal_vector(3)*sigma_yztpl
          traction_coupling(it_coupling_thispack,3,ipoint)=normal_vector(1)*sigma_xztpl+ &
               normal_vector(2)*sigma_yztpl+normal_vector(3)*sigma_zztpl

          disp_coupling(it_coupling_thispack,:,ipoint)=displ(:,ibool(i,j,k,ispec))
        enddo
      enddo
    enddo
   end if
  enddo  ! the integral boundary spectral element loop


end subroutine compute_traction_disp_elastic




subroutine compute_pres_disp_acoustic()
use specfem_par, only:it,ibool,hprime_xx,hprime_yy,hprime_zz,xix,xiy,xiz,etax,&
       etay,etaz,gammax,gammay,gammaz,rhostore
use specfem_par_acoustic, only:ispec_is_acoustic,potential_acoustic,&
       potential_dot_dot_acoustic
use tele_coupling_par,only:DECIMATE_COUPLING,NSTEP_BETWEEN_OUTPUTBOUND,&
       traction_coupling,disp_coupling,nele_tele_coupling,coupling_ele_property,LOW_RESOLUTION,&
       normal_vect_coupling
use constants,only: CUSTOM_REAL,NGLLX,NGLLY,NGLLZ,NDIM,ZERO 
implicit none

! local parameters
real(kind=CUSTOM_REAL) xixl,xiyl,xizl,etaxl,etayl,etazl,gammaxl,gammayl,gammazl
real(kind=CUSTOM_REAL) temp1l,temp2l,temp3l
real(kind=CUSTOM_REAL) rho_invl
integer iface_coupling,ispec,ipoint_iface,ipoint
!integer iglob
integer it_coupling,it_coupling_thispack

real(kind=CUSTOM_REAL),dimension(NDIM) :: normal_vector
integer i,j,k,l
integer imax,imin,di,jmax,jmin,dj,kmax,kmin,dk


it_coupling=(it-1)/DECIMATE_COUPLING+1
it_coupling_thispack=mod(it_coupling,NSTEP_BETWEEN_OUTPUTBOUND)
if(it_coupling_thispack.eq.0) it_coupling_thispack=NSTEP_BETWEEN_OUTPUTBOUND

!Comment the below line, because they have been initialized in the 
!subroutine compute_traction_disp_elastic. If compute_traction_disp_elastic
!is commented, make the below line uncommented
!if(it_coupling_thispack.eq.1) traction_coupling(:,:,:)=0.d0

do iface_coupling = 1,nele_tele_coupling

   ispec = coupling_ele_property(iface_coupling)%ispec_coupling
   if(ispec_is_acoustic(ispec)) then
    imin=coupling_ele_property(iface_coupling)%istart;imax=coupling_ele_property(iface_coupling)%iend
    jmin=coupling_ele_property(iface_coupling)%jstart;jmax=coupling_ele_property(iface_coupling)%jend
    kmin=coupling_ele_property(iface_coupling)%kstart;kmax=coupling_ele_property(iface_coupling)%kend
    di=1;dj=1;dk=1

    ipoint_iface=0
    do k=kmin,kmax,di
      do j=jmin,jmax,dj
        do i=imin,imax,dk

        ! derivative along x
        temp1l = ZERO
        do l = 1,NGLLX
          temp1l = temp1l + potential_acoustic(ibool(l,j,k,ispec))*hprime_xx(i,l)
        enddo

        ! derivative along y
        temp2l = ZERO
        do l = 1,NGLLZ
          temp2l = temp2l + potential_acoustic(ibool(i,l,k,ispec))*hprime_yy(j,l)
        enddo

        ! derivative along z
        temp3l = ZERO
        do l = 1,NGLLZ
          temp3l = temp3l + potential_acoustic(ibool(i,j,l,ispec))*hprime_zz(k,l)
        enddo

        xixl = xix(i,j,k,ispec)
        xiyl = xiy(i,j,k,ispec)
        xizl = xiz(i,j,k,ispec)
        etaxl = etax(i,j,k,ispec)
        etayl = etay(i,j,k,ispec)
        etazl = etaz(i,j,k,ispec)
        gammaxl = gammax(i,j,k,ispec)
        gammayl = gammay(i,j,k,ispec)
        gammazl = gammaz(i,j,k,ispec)

        rho_invl = 1.0_CUSTOM_REAL / rhostore(i,j,k,ispec)
                ! derivatives of acoustic scalar potential field on GLL points
        if(LOW_RESOLUTION) then
             ipoint=iface_coupling
        else
             ipoint_iface=ipoint_iface+1
             ipoint=(iface_coupling-1)*NGLLX*NGLLY+ipoint_iface
        end if
        disp_coupling(it_coupling_thispack,1,ipoint) = (temp1l*xixl + temp2l*etaxl + temp3l*gammaxl) * rho_invl
        disp_coupling(it_coupling_thispack,2,ipoint) = (temp1l*xiyl + temp2l*etayl + temp3l*gammayl) * rho_invl
        disp_coupling(it_coupling_thispack,3,ipoint) = (temp1l*xizl + temp2l*etazl + temp3l*gammazl) * rho_invl
        normal_vector(:)=normal_vect_coupling(:,ipoint)
        traction_coupling(it_coupling_thispack,:,ipoint)=potential_dot_dot_acoustic(ibool(i,j,k,ispec))*&
                normal_vector(:)
        end do
      end do
    end do

   end if
end do
end subroutine compute_pres_disp_acoustic
