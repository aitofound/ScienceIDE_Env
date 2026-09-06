subroutine simple_plasmasphere(Kp)
  
  use ModCimiGrid,    ONLY: MinLonPar, MaxLonPar,np,nt
  use ModCimiTrace, ONLY: ro, iba
  use ModPlasmasphere, ONLY:PlasDensity_C
  implicit none
  
  real, INTENT(in) 	 ::	Kp
  real		 ::	Lpp, L, nps, npt, nps_Lpp, npt_Lpp, coef
  integer 		 ::	i, j
  
  !----------------------------------------------------------------------------
  !PlasDensity_C is normally allocated in ModPlasmasphere, but not when simple
  !plasmasphere is used
  if (.not. allocated(PlasDensity_C)) &
       allocate(PlasDensity_C(np,nt))

  !initialize density to low value
  PlasDensity_C( :, :) = 0.1
  
  Lpp = 5.6 - 0.46 * Kp
  
  ! remove den jump at Lpp:
  nps_Lpp =  10. ** ( -0.3145 * Lpp + 3.9043 )
  
  npt_Lpp = 124. * ( 3. / Lpp ) ** 4.
  
  coef = nps_Lpp / npt_Lpp
  
  do j = MinLonPar, MaxLonPar
     
     do i = 1, iba(j)
        
        !add checkl when ro=0. this happens when on open fieldline
        !so just set L to large value
        
        if ( ro( i, j ) < 1e-10 ) then
           
           L = 20
           
        else
           
           L = ro( i, j )
           
        endif
        
        ! inside the plasmapause, use
        ! Carpenter and Anderson [1992] model
        nps = 10. ** ( -0.3145 * L + 3.9043 ) 
        
        ! outside the plasmapause, use
        ! Sheely et al. [2001] model ; valid only at L > 3
        npt = 124. * ( 3. / L ) ** 4.
        
        if ( L .le. Lpp ) then
           
           PlasDensity_C( i, j ) = nps * 1E6
           
        else
           
           PlasDensity_C( i, j ) = npt * coef * 1E6
           
        endif
        
     enddo ! End Latitude loop
     
  enddo ! End Longitude loop
  
end subroutine simple_plasmasphere
