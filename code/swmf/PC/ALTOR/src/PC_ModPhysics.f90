!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PC_ModPhysics

  use ModConst, ONLY: cBoltzmann,  cElectronMass, cLightSpeed, &
       cEps, cElectronCharge
  use ModUtilities, ONLY: CON_stop
  
  implicit none
  save

  !\
  ! Rest nass energy
  !/
  real, parameter :: eRestMassEnergy = cElectronMass*cLightSpeed**2

  real, parameter :: Omega2ToNRatio = cElectronCharge**2/(cElectronMass*cEps)

  ! Named indexes for I/O variable units
  integer, parameter :: nIoUnit = 17

  integer, parameter :: UnitX_           = 1
  integer, parameter :: UnitU_           = 2
  integer, parameter :: UnitRho_         = 3
  integer, parameter :: UnitT_           = 4
  integer, parameter :: UnitOmega_       = 5
  integer, parameter :: UnitN_           = 6
  integer, parameter :: UnitP_           = 7
  integer, parameter :: UnitB_           = 8
  integer, parameter :: UnitRhoU_        = 9
  integer, parameter :: UnitEnergyDens_  = 10
  integer, parameter :: UnitPoynting_    = 11
  integer, parameter :: UnitJ_           = 12
  integer, parameter :: UnitElectric_    = 13
  integer, parameter :: UnitTemperature_ = 14
  integer, parameter :: UnitMass_        = 15
  integer, parameter :: UnitCharge_      = 16
  integer, parameter :: UnitEnergy_      = 17


  ! Conversion between units: e.g. VarSi = VarNo*No2Si_V(UnitVar_)
  ! The following should always be true: No2Si_V*Si2Io_V = No2Io_V
  real, dimension(nIoUnit) ::  Si2No_V = -1.0, No2Si_V= -1.0, &
       Io2No_V = 1.0, No2Io_V = 1.0

  character (len=8), dimension(nIoUnit) :: NameSiUnit_V=(/&
       'm       ',&
       'm/s     ',&
       'kg/m3   ',&
       's       ',&
       '1/s     ',&
       '1/m3    ',&
       'N/m2    ',&
       'T       ',&
       'kg/s m2 ',&
       'J/m3    ',&
       'W/m2    ',&
       'A/m2    ',&
       'V/m     ',&
       'K       ',&
       'kg      ',&
       'C       ',&
       'J       '/)
contains
  subroutine assign_other_units
    use PC_ModSize, ONLY: nDim
    !\
    !May be found after UnitX_, UnitT_ and UnitN_ are set
    !/
    No2Si_V(UnitTemperature_) = eRestMassEnergy/cBoltzmann
    No2Si_V(UnitMass_       ) = cElectronMass
    No2Si_V(UnitCharge_     ) = abs(cElectronCharge)
    No2Si_V(UnitRho_        ) = No2Si_V(UnitN_)*No2Si_V(UnitMass_)
    No2Si_V(UnitRhoU_       ) = No2Si_V(UnitU_)*No2Si_V(UnitRho_)
    No2Si_V(UnitB_          ) = No2Si_V(UnitOmega_)*No2Si_V(UnitMass_)/&
         No2Si_V(UnitCharge_)
    No2Si_V(UnitElectric_   ) = No2Si_V(UnitB_)*No2Si_V(UnitU_)
    No2Si_V(UnitEnergyDens_ ) = eRestMassEnergy*No2Si_V(UnitN_)
    No2Si_V(UnitP_          ) = No2Si_V(UnitEnergyDens_)
    No2Si_V(UnitPoynting_   ) = No2Si_V(UnitEnergyDens_)*No2Si_V(UnitU_)
    No2Si_V(UnitJ_          ) = No2Si_V(UnitN_)*No2Si_V(UnitU_)*&
         No2Si_V(UnitCharge_)
    No2Si_V(UnitEnergy_     ) = eRestMassEnergy*No2Si_V(UnitN_)*&
         No2Si_V(UnitX_)**nDim
    Si2No_V = 1/No2Si_V
    call show_normalization
  contains
    !============
    subroutine show_normalization
      use PIC_ModProc,  ONLY: iProc
      character(LEN=16), dimension(nIoUnit) :: NameUnit_V=(/&
           'UnitX_          ',&
           'UnitU_          ',&
           'UnitRho_        ',&
           'UnitT_          ',&
           'UnitOmega_      ',&
           'UnitN_          ',&
           'UnitP_          ',&
           'UnitB_          ',&
           'UnitRhoU_       ',&
           'UnitEnergyDens_ ',&
           'UnitPoynting_   ',&
           'UnitJ_          ',&
           'UnitElectric_   ',&
           'UnitTemperature_',&
           'UnitMass_       ',&
           'UnitCharge_     ',&
           'UnitEnergy_     '/)
      integer:: iVar
      !--------------
      if(iProc/=0)RETURN
      do iVar = 1, nIoUnit
         write(*,'(a,es10.3,a)')'Io2Si_V('//NameUnit_V(iVar)//')=',&
              No2Si_V(iVar),' '//NameSiUnit_V(iVar)
      end do
    end subroutine show_normalization
  end subroutine assign_other_units
  !==========================
  subroutine read_units(NameCommand)
    use ModReadParam, ONLY: read_var
    character(LEN=*), intent(in) :: NameCommand
    !------------------
    No2Si_V(UnitU_) = cLightSpeed
    select case(NameCommand)
    case('#UNITX')
       call read_var('No2Si_V(UnitX_)',No2Si_V(UnitX_))
       No2Si_V(UnitT_)     = No2Si_V(UnitX_)/No2Si_V(UnitU_) 
       No2Si_V(UnitOmega_) = 1/No2Si_V(UnitT_)
       No2Si_V(UnitN_)     =  No2Si_V(UnitOmega_)**2/Omega2ToNRatio
    case('#UNITT')
       call read_var('No2Si_V(UnitT_)',No2Si_V(UnitT_))
       No2Si_V(UnitX_)     = No2Si_V(UnitT_)*No2Si_V(UnitU_) 
       No2Si_V(UnitOmega_) = 1/No2Si_V(UnitT_)
       No2Si_V(UnitN_)     =  No2Si_V(UnitOmega_)**2/Omega2ToNRatio
    case('#UNITOMEGA')     
       call read_var('No2Si_V(UnitOmega_)',No2Si_V(UnitOmega_))
       No2Si_V(UnitT_)     = 1/No2Si_V(UnitOmega_) 
       No2Si_V(UnitX_)     = No2Si_V(UnitT_)*No2Si_V(UnitU_)
       No2Si_V(UnitN_)     =  No2Si_V(UnitOmega_)**2/Omega2ToNRatio
    case('#UNITN')  
       call read_var('No2Si_V(UnitN_)',No2Si_V(UnitN_))
       No2Si_V(UnitOmega_) = sqrt(Omega2ToNRatio*No2Si_V(UnitN_))  
       No2Si_V(UnitT_)     = 1/No2Si_V(UnitOmega_) 
       No2Si_V(UnitX_)     = No2Si_V(UnitT_)*No2Si_V(UnitU_)
    case default
       call CON_stop('Unknown command '//NameCommand)
    end select
    call assign_other_units
  end subroutine read_units
  !============
  subroutine set_default_units
    No2Si_V(UnitU_)     = cLightSpeed
    No2Si_V(UnitT_)     = 1
    No2Si_V(UnitX_)     = No2Si_V(UnitT_)*No2Si_V(UnitU_) 
    No2Si_V(UnitOmega_) = 1
    No2Si_V(UnitN_)     = 1/Omega2ToNRatio
    call assign_other_units
  end subroutine set_default_units
  !============
  subroutine set_io_cycle_wavelength
    use ModNumConst, ONLY: cTwoPi
    !\
    ! In most standard normalizations for PIC and hybrid,
    ! the dimensionless coordinate is normalized by c/\omega_{pe,pi}
    ! time by 1/\omega_{pe,pi,Bi} etc. However, the input/output  of 
    ! coordinates normalized per wavelength (which corresponds to the 
    ! dimensionless distance of 2\pi) and a time normalized by the 
    ! wave period (which corresponds to the dimensionless time of  
    ! 2\pi) may be preferable.
    !/
    Io2No_V(UnitX_) = cTwoPi; No2Io_V(UnitX_) = 1/Io2No_V(UnitX_)
    Io2No_V(UnitT_) = cTwoPi; No2Io_V(UnitT_) = 1/Io2No_V(UnitT_)
  end subroutine set_io_cycle_wavelength
end module PC_ModPhysics
