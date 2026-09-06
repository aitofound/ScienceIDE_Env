module CIMI_aurora
  use ModCimiGrid

  real, allocatable :: Fang_Ci(:)  ! nEnergies, 8
  real, allocatable :: Fang_y(:)   ! nEnergies, nAlts
  real, allocatable :: Fang_f(:)   ! nEnergies, nAlts
  real, allocatable :: Fang_Pij(:, :)

  !\
  ! Needed for Fang et al, 2013:
  !/
  real, allocatable :: Fang_Ion_Ci(:)  ! nEnergies, 8
  real, allocatable :: Fang_Ion_y(:)   ! nEnergies, nAlts
  real, allocatable :: Fang_Ion_Pij(:, :)

  integer ::nAlts = 45
  real, allocatable :: ColumnIntegralRho_I(:), Rho_I(:), &
    altitude_I(:), deltaAltitude_I(:), scaleHeight_I(:)

contains
  !===========================================================================
  subroutine init_mod_aurora
    integer :: iAlt
    !-----------------------------------------------------------------------
    ! Allocate variables necessary for Fang Energy Deposition
    if (allocated(Fang_Ci)) RETURN
    allocate(Fang_Ci(8))
    allocate(Fang_y(nAlts))
    allocate(Fang_f(nAlts))
    allocate(Fang_Pij(8, 4))

    ! Ions
    allocate(Fang_Ion_Ci(12))
    allocate(Fang_Ion_y(nAlts))
    allocate(Fang_Ion_Pij(12, 4))

    !electrons
    Fang_Pij(1, :) = (/1.25E+00, 1.45903, -2.42E-01, 5.95E-02/)
    Fang_Pij(2, :) = (/2.24E+00, -4.23E-07, 1.36E-02, 2.53E-03/)
    Fang_Pij(3, :) = (/1.42E+00, 1.45E-01, 1.70E-02, 6.40E-04/)
    Fang_Pij(4, :) = (/0.248775, -1.51E-01, 6.31E-09, 1.24E-03/)
    Fang_Pij(5, :) = (/-0.465119, -1.05E-01, -8.96E-02, 1.22E-02/)
    Fang_Pij(6, :) = (/3.86E-01, 1.75E-03, -7.43E-04, 4.61E-04/)
    Fang_Pij(7, :) = (/-6.45E-01, 8.50E-04, -4.29E-02, -2.99E-03/)
    Fang_Pij(8, :) = (/9.49E-01, 1.97E-01, -2.51E-03, -2.07E-03/)

    ! This is from:
    ! Fang, X., D. Lummerzheim, and C. H. Jackman (2013),
    !           Proton impact ionization and a fast calculation method,
    !           J. Geophys. Res. Space Physics, 118, 5369–5378,
    !           doi:10.1002/jgra.50484:

    !ions
    
    Fang_Ion_Pij(1, :) = (/2.55050E+00, 2.69476e-01, -2.58425E-01, 4.43190E-02/)
    Fang_Ion_Pij(2, :) = (/6.39287E-01, -1.85817e-01, -3.15636E-02, 1.01370E-02/)
    Fang_Ion_Pij(3, :) = (/1.63996E+00, 2.43580e-01, 4.29873E-02, 3.77803E-02/)
    Fang_Ion_Pij(4, :) = (/-2.13479E-01, 1.42464e-01, 1.55840E-02, 1.97407E-03/)
    Fang_Ion_Pij(5, :) = (/-1.65764E-01, 3.39654e-01, -9.87971E-03, 4.02411E-03/)
    Fang_Ion_Pij(6, :) = (/-3.59358E-02, 2.50330e-02, -3.29365E-02, 5.08057E-03/)
    Fang_Ion_Pij(7, :) = (/-6.26528E-01, 1.46865e+00, 2.51853E-01, -4.57132E-02/)
    Fang_Ion_Pij(8, :) = (/1.01384E+00, 5.94301e-02, -3.27839E-02, 3.42688E-03/)
    Fang_Ion_Pij(9, :) = (/-1.29454E-06, -1.43623e-01, 2.82583E-01, 8.29809E-02/)
    Fang_Ion_Pij(10, :) = (/-1.18622E-01, 1.79191e-01, 6.49171E-02, -3.99715E-03/)
    Fang_Ion_Pij(11, :) = (/2.94890E+00, -5.75821e-01, 2.48563E-02, 8.31078E-02/)
    Fang_Ion_Pij(12, :) = (/-1.89515E-01, 3.53452e-02, 7.77964E-02, -4.06034E-03/)

    allocate(ColumnIntegralRho_I(nAlts), &
      altitude_I(nAlts), Rho_I(nAlts), deltaAltitude_I(nAlts), &
      scaleHeight_I(nAlts))
    ! Scale Heights and Densities from Aaron B, 8 hour average at equinox.
    ! Altitudes < 100 obtained from MSIS, set to values to avoid discontinuity
    ! at 100 km.
    altitude_I = [60.0, 64.0, 68.0, 72.0, 76.0, 80.0, 84.0, 88.0, 92.0, &
        96.0, 100.0, 101.7240235, 103.4846226, 105.3007575, &
        107.1999348, 109.2189645, 111.4045805, 113.8181562, &
        116.5402055, 119.6670887, 123.313472, 127.6169763, &
        132.7009225, 138.651505, 145.5410287, 153.4247201, &
        162.3333895, 172.2725819, 183.2255933, 195.1576977, &
        208.021627, 221.7629592, 236.324522, 251.6493773, &
        267.6823795, 284.3707108, 301.6638769, 319.5135387, &
        337.8733968, 356.6993154, 375.9496133, 395.5854969, &
        415.5714235, 435.8754037, 456.4691579] * 1000
    Rho_I = [2.71E-04, 1.57E-04, 8.89E-05, 4.89E-05, 2.66E-05, &
        1.43E-05, 7.41E-06, 3.96E-06, 2.13E-06, 1.07E-06, &
        5.04E-07, 3.83E-07, 2.80E-07, 1.94E-07, 1.27E-07, &
        7.86E-08, 4.76E-08, 2.91E-08, 1.82E-08, 1.17E-08, &
        7.64E-09, 5.05E-09, 3.36E-09, 2.26E-09, 1.53E-09, &
        1.04E-09, 7.18E-10, 4.98E-10, 3.49E-10, 2.47E-10, &
        1.76E-10, 1.27E-10, 9.20E-11, 6.74E-11, 4.98E-11, &
        3.70E-11, 2.77E-11, 2.09E-11, 1.58E-11, 1.21E-11, &
        9.22E-12, 7.07E-12, 5.45E-12, 4.21E-12, 3.26E-12]
    scaleHeight_I = [8.396791582, 8.088556216, 7.833311192, 7.667150955, &
        7.514936086, 7.340735656, 7.35343484, 7.131672744, 6.631237377, &
        6.37906911, 6.436402158746115, 5.939530187, 5.276249753982746, &
        4.705934645830059, 4.3393623546203335, 4.277855753466067, &
        4.600686598, 5.303962948952029, 6.333275322813335, &
        7.667453774667858, 9.302456259013368, 11.256509480111763, &
        13.53237943490718, 16.106572916351627, 18.943797010975917, &
        22.01876113431859, 25.30201333505844, 28.76667067799556, &
        32.37533834, 36.07743929432726, 39.82704508482861, &
        43.57483730021151, 47.29718275498535, 50.960993725803014, &
        54.54656595886673, 58.02375981947714, 61.36935137240605, &
        64.54668333, 67.54047827530732, 70.32957720878622, &
        72.92914082046934, 75.34093625843916, 77.58860436834145, &
        79.66814764542183, 81.59643789] * 1000

    ! calculate linear dAltitudes (probably not correct)
    deltaAltitude_I(1) = altitude_I(2) - altitude_I(1)
    deltaAltitude_I(nAlts) = altitude_I(nAlts) - altitude_I(nAlts - 1)
    deltaAltitude_I(2:nAlts-1) = &
        (altitude_I(3:nAlts) - altitude_I(1:nAlts-2)) / 2

    ! Integrate density
    ColumnIntegralRho_I(nAlts) = Rho_I(nAlts) * deltaAltitude_I(nAlts)
    do iAlt = nAlts - 1, 1, -1
      ColumnIntegralRho_I(iAlt) = ColumnIntegralRho_I(iAlt + 1) + &
                                  Rho_I(iAlt) * deltaAltitude_I(iAlt)
    end do

  end subroutine init_mod_aurora
  !===========================================================================
  subroutine calc_fang_loss(ionization, energy, rMirror, iSpec)
  use ModCimiPlanet, ONLY: nspec, rc
  use ModPlanetConst,	ONLY: Earth_, rPlanet_I
                                 

  integer, intent(in) :: iSpec
  real, intent(in) :: energy, rMirror
  real, intent(out) :: ionization
  real :: rMirrorMeters, partialAltitude
  real :: maxIonization, particleIonization
  integer :: i, j, iAlt

  real :: Fang_de = 0.035
  !---------------------------------------------------------------------------

  rMirrorMeters = (rMirror - 1) * rPlanet_I(Earth_)
  ! If < 100 km, just assume it precipitates for now, could theoretically
  ! include mesosphere altitudes
  if (rMirrorMeters < altitude_I(1) - deltaAltitude_I(1)/2) then
    ionization = 1.0
    return
  end if

  if (iSpec == nspec) then
    ! Electrons
    do i = 1, 8
      Fang_Ci(i) = 0.0
      do j = 0, 3
        Fang_Ci(i) = Fang_Ci(i) + &
          Fang_Pij(i, j + 1)*log(energy)**j
      enddo
    enddo
    Fang_Ci = exp(Fang_Ci)

    ! /10.0 in this statement is for kg/m2 to g/cm2
    ! Fang doesn't include the dip angle, be we do.
    ! Right now use a very simple approximation that is not location dependent
    Fang_y(:) = 2.0/(energy)* &
                        (ColumnIntegralRho_I(1:nAlts)/10.0/6e-6)**0.7
    !sinDipAngle(j,i,1:nAlts,iBlock) / 6e-6) ** 0.7

    Fang_f(:) = &
    Fang_Ci(1)*Fang_y(:)**Fang_Ci(2)* &
    exp(-Fang_Ci(3)*Fang_y(:)**Fang_Ci(4)) + &
    Fang_Ci(5)*Fang_y(:)**Fang_Ci(6)* &
    exp(-Fang_Ci(7)*Fang_y(:)**Fang_Ci(8))
  else
    ! Ions
    do i = 1, 12
      Fang_Ion_Ci(i) = 0.0
      do j = 0, 3
        Fang_Ion_Ci(i) = Fang_Ion_Ci(i) + &
          Fang_Ion_Pij(i, j + 1)*log(energy)**j
      enddo
    enddo
    Fang_Ion_Ci = exp(Fang_Ion_Ci)

    ! /10.0 in this statement is for kg/m2 to g/cm2
    Fang_Ion_y(:) = 7.5/(energy)* &
                            (ColumnIntegralRho_I(1:nAlts)/10.0/1e-4)**0.9

    Fang_f(:) = &
        Fang_Ion_Ci(1)*Fang_Ion_y(:)**Fang_Ion_Ci(2)* &
        exp(-Fang_Ion_Ci(3)*Fang_Ion_y(:)**Fang_Ion_Ci(4)) + &
        Fang_Ion_Ci(5)*Fang_Ion_y(:)**Fang_Ion_Ci(6)* &
        exp(-Fang_Ion_Ci(7)*Fang_Ion_y(:)**Fang_Ion_Ci(8)) + &
        Fang_Ion_Ci(9)*Fang_Ion_y(:)**Fang_Ion_Ci(10)* &
        exp(-Fang_Ion_Ci(11)*Fang_Ion_y(:)**Fang_Ion_Ci(12))
  end if 

  ! Total ionization if a particle were to have a bounce point inside Earth
  maxIonization = sum(Fang_f * deltaAltitude_I / scaleHeight_I)

  particleIonization = 0.0
  do iAlt = nAlts, 1, -1
    ! If bounce point is below bottom of cell, add all of it
    if (altitude_I(iAlt) - deltaAltitude_I(iAlt)/2 > rMirrorMeters) then
      particleIonization = particleIonization + &
                    Fang_f(iAlt) * deltaAltitude_I(iAlt) / scaleHeight_I(iAlt)
    else
    ! Next cell where this is not true does partial calculation
      partialAltitude = altitude_I(iAlt) + deltaAltitude_I(iAlt) / 2 &
          - rMirrorMeters
      particleIonization = particleIonization + &
                           Fang_f(iAlt) * partialAltitude / scaleHeight_I(iAlt)
      EXIT
    end if
  end do

  ! Chance of precipitating is ratio of ionization above mirror point to total
  ! ionization if mirror point were below the atmosphere. 
  ionization = min(1., particleIonization / maxIonization)

  end subroutine calc_fang_loss
  !===========================================================================
end module CIMI_aurora
