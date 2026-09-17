  !  Copyright (C) 2002 Regents of the University of Michigan,
  !  portions used with permission
  !  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_ModPlot
  
  use PT_ModConst, only: ckeV, cPi, cMeV
  use PT_ModProc, ONLY : iProc, iComm, iError
  use PT_ModUnit, ONLY : kinetic_energy_to_momentum
  
  implicit none
  SAVE
  
  character(len=*), parameter :: OutputDir = 'PT/IO2/'
  character(len=*), parameter :: SplitFile = 'split_levels.dat'
  character(len=*), parameter :: TimeFile =  'time.dat'
  character(len=*), parameter :: EnergyBinFile = 'energy_bin.dat'
  character(len=*), parameter :: LagrBinFile = 'lagr_bin.dat'
  character(len=*), parameter :: RFile = 'R_bin.dat'
  character(len=*), parameter :: ShockLocFile = 'shock_loc.dat'
  character(len=*), parameter :: EarthFile = 'intflux_earth.dat'

contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    use ModUtilities, ONLY: CON_stop

    character(len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#SAVEOUTPUT')
      ! maybe something will go here one day
    case default
       call CON_stop(NameSub//' Unknown command '//NameCommand)
    end select
  end subroutine read_param
  !============================================================================
  subroutine init
    use ModUtilities, only: touch_file
    use PT_ModDistribution, only: EnergyBin_I, LagrBin_I

    ! Only rank 0 creates output files: all saving below is done by rank 0,
    ! and concurrent creation of the same file can fail on some filesystems
    if(iProc /= 0) RETURN

    ! write energy bin file - stays constant throughout simulation
    open(801,file=OutputDir//EnergyBinFile,status='unknown')
    write(801,'(3000e15.6)')EnergyBin_I/ckeV
    close(801)
    
    ! Create time file - will be appended with the output time each timestep
    call touch_file(OutputDir//TimeFile)
    
    ! Create file that will hold spatial bins - time dependent
    call touch_file(OutputDir//LagrBinFile)
    call touch_file(OutputDir//RFile)

    ! Create shock location file and record shock at surface at t = 0
    call touch_file(OutputDir//ShockLocFile)

    ! Create file that will hold integral fluxes at Earth
    call touch_file(OutputDir//EarthFile)

  end subroutine init
  !============================================================================
  subroutine save_plot_all(Iter, Time)
    use PT_ModDistribution, only: Counts_II, nEnergyBins, nLagrBins, TotalWeight, &
                                  calculate_flux
    use ModMpi, ONLY: MPI_reduce_real_array, MPI_SUM, MPI_reduce_real_scalar
    
    real, intent(in) :: Time
    integer, intent(in) :: Iter
    integer :: iTime, iLagr
    real :: Flux_II(nEnergyBins, nLagrBins)

    ! dummy variables during integration
    real :: TotalWeightProc

    ! placeholders - need to reset to original value after summing over all processors
    TotalWeightProc = TotalWeight

    ! Sum over all processors
    call MPI_reduce_real_array(Counts_II, nEnergyBins*nLagrBins, MPI_SUM, 0, &
          iComm, iError)
    call MPI_reduce_real_scalar(TotalWeight, MPI_SUM, 0, iComm, iError)

    ! Only one processor saves data. Reset counts for the next integration timestep
    if(iProc/=0) then
      ! Reset Counts for next timestep
      Counts_II = 0.0
    else

      ! Convert counts to flux
      call calculate_flux(Time, Flux_II)

      ! append time of output to time file
      open(901, file=OutputDir//TimeFile, position="append", action="write")
        write(901, *) Time
      close(901)

      ! save updated positions of lagr coord bins
      call save_position_bins(Time)

      ! Save flux along the entire fieldline
      call save_flux(Time, Flux_II)

      ! Save flux at location of earth
      call save_integral_flux_earth(Time, Flux_II)

      ! Reset Counts for next timestep
      Counts_II = 0.0
      ! Reset total weight of processor to original value
      TotalWeight = TotalWeightProc

    end if

  end subroutine save_plot_all
  !============================================================================
  subroutine save_flux(Time, Flux_II)
    use PT_ModDistribution, only: nLagrBins

    real, intent(in) :: Time, Flux_II(:,:)
    character(len = 50) :: outputFile
    integer :: iLagr
    ! create output distribution function file name
    write(outputFile, '(A15, I0)') 'flux_time_', int(Time)
    outputFile = adjustl(outputFile)

    ! output flux (nEnergy x nLagrCoord)
    open(902, file=OutputDir//trim(outputFile), status='unknown', action="READWRITE")
    do iLagr = 1, nLagrBins
      write(902,'(4000e15.6)') Flux_II(:, iLagr) 
    end do
    close(902)

  end subroutine save_flux
  !============================================================================
  subroutine save_integral_flux_earth(Time, Flux_II)
    use PT_ModDistribution, only: LagrBin_I, nLagrBins, calculate_integral_flux
    use PT_ModFieldline, only: get_particle_location
    
    real, intent(in) :: Time, Flux_II(:,:)
    
    ! TODO: Add channels to PARAM
    real :: RBins_I(nLagrBins+1)
    real :: EnergyChannels(6) = (/1.0, 10.0, 30.0, 50.0, 100.0, 500.0/) * cMeV
    integer :: iBin, iR, nChannel = 6, iChannel
    real :: IntFluxChannels(6) = 0.0
    real :: EarthLoc = 215.0
    
    do iBin = 1, nLagrBins+1
      call get_particle_location(Time, LagrBin_I(iBin), RBins_I(iBin))
    end do

    ! Index of spatial bin containing Earth's location
    iR = minloc(EarthLoc - RBins_I, mask = (EarthLoc - RBins_I >= 0), dim = 1)

    do iChannel = 1, nChannel
      call calculate_integral_flux(Flux_II(:, iR), EnergyChannels(iChannel), IntFluxChannels(iChannel))
    end do
    
    ! TODO: Fix format after adding channels to PARAM
    open(911, file=OutputDir//EarthFile, position="append", action="write")
      write(911, '(7e15.6)') Time, IntFluxChannels
    close(911)

  end subroutine save_integral_flux_earth
  !============================================================================
  subroutine save_shock_location

    use PT_ModProc, only: iProc
    use PT_ModTime, only: DataInputTime
    use PT_ModGrid, only: State_VIB, R_, iShock_IB, Shock_

    integer :: iLine = 1
    ! Currently only designed for one fieldline!
    if(iProc.ne.0) return
    open(104, file = OutputDir//ShockLocFile, position = 'append', action = 'write')
    write(104, '(2e15.6)') DataInputTime, State_VIB(R_, iShock_IB(Shock_, iLine), iLine)
    close(104)

  end subroutine save_shock_location
  !============================================================================
  subroutine save_position_bins(Time)

    use PT_ModDistribution, only: LagrBin_I, nLagrBins
    use PT_ModFieldline, only: get_particle_location

    real, intent(in) :: Time
    real :: RBins_I(nLagrBins+1)
  
    integer :: iBin

    open(911, file=OutputDir//LagrBinFile, position="append", action="write")
      write(911, '(4000e15.6)') LagrBin_I
    close(911)

    do iBin = 1, nLagrBins+1
      call get_particle_location(Time, LagrBin_I(iBin), RBins_I(iBin))
    end do

    open(911, file=OutputDir//RFile, position="append", action="write")
      write(911, '(4000e15.6)') RBins_I
    close(911)

  end subroutine save_position_bins
  !============================================================================
  subroutine save_distribution_function(Iter, Time)
    use PT_ModDistribution, only: Counts_II, nEnergyBins, nLagrBins, TotalWeight, &
                                  calculate_distribution_function
    use ModMpi, ONLY: MPI_reduce_real_array, MPI_SUM, MPI_reduce_real_scalar
    real, intent(in) :: Time
    integer, intent(in) :: Iter
    integer :: iTime, iLagr
    character(len = 50) :: outputFile
    real :: DistFunc_II(nEnergyBins, nLagrBins)
    ! dummy variables during integration since they do not reset
    real :: TotalWeightProc


    ! Save fluxes stored at different radial distances
    !--------------------------------------------------------------------------
    ! placeholder for original total weight - need to reset after summing over all processors
    
    TotalWeightProc = TotalWeight

    call MPI_reduce_real_array(Counts_II, nEnergyBins*nLagrBins, MPI_SUM, 0, &
         iComm, iError)
    call MPI_reduce_real_scalar(TotalWeight, MPI_SUM, 0, iComm, iError)

    if(iProc/=0) then
      ! Reset Counts for next timestep
      Counts_II = 0.0
    else
      ! Convert counts to distribution function - variable name does not change
      call calculate_distribution_function(Time, DistFunc_II)

      ! append time of output to time file
      open(901, file=OutputDir//TimeFile, position="append", action="write")
        write(901, *) Time
      close(901)

      call save_position_bins(Time)
      ! create output distribution function file name
      write(outputFile, '(A15, I0)') 'distfunc_time_', int(Time)
      outputFile = adjustl(outputFile)

      ! output distribution function (nEnergy x nLagrCoord)
      open(902, file=OutputDir//trim(outputFile), status='unknown', action="READWRITE")
      do iLagr = 1, nLagrBins
        write(902,'(4000e15.6)') DistFunc_II(:, iLagr) 
      end do
      close(902)

      ! Reset Counts for next timestep
      Counts_II = 0.0
      ! Reset total weight of processor to original value
      TotalWeight = TotalWeightProc
    end if

  end subroutine save_distribution_function
  !============================================================================
  subroutine save_analytic_solution()
    ! currently assumes: 
    !     constant Dxx along entire fieldline
    use PT_ModGrid, only: State_VIB, U_, nVertex_B
    use PT_ModFieldline, only: DxxConst
    use PT_ModDistribution, only: EnergyBin_I, nEnergyBins, InjEnergy
    use PT_ModUnit, only: kinetic_energy_to_momentum

    real :: UpstreamU, DownstreamU, PowerLaw, P0, Pnorm
    real, allocatable :: fSteadyState(:), Tacc(:)
    integer :: i

    if(iProc.ne.0) return

    allocate(fSteadyState(1:nEnergyBins+1))
    allocate(Tacc(1:nEnergyBins+1))

    ! Shock moves at constant lagrangian speed - shift to shock frame (-1.0)
    UpstreamU = State_VIB(U_, nVertex_B(1)-5, 1) - 1.0
    DownstreamU = State_VIB(U_, 5, 1) - 1.0
    PowerLaw = -3.0 * UpstreamU / (UpstreamU - DownstreamU)
    P0 = kinetic_energy_to_momentum(InjEnergy)

    do i = 1, nEnergyBins+1
      Pnorm = kinetic_energy_to_momentum(EnergyBin_I(i)) / P0
      fSteadyState(i) = Pnorm ** PowerLaw
      Tacc(i) = 3.0 * DxxConst * (UpstreamU**(-1.0) + DownstreamU**(-1.0)) &
                / (UpstreamU - DownstreamU) * log(Pnorm)
    end do

    open(801,file=OutputDir//'steadystate_f.dat',status='unknown')
    write(801,'(1000e15.6)') fSteadyState
    close(801)

    open(801,file=OutputDir//'acceleration_time.dat',status='unknown')
    write(801,'(1000e15.6)') Tacc
    close(801)


  end subroutine save_analytic_solution
  !============================================================================
end module PT_ModPlot
!==============================================================================
