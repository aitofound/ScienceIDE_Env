subroutine read_para()
   use spectotime_par
   implicit none

! other variables
   character(len=256) ::para_file
   integer ::ios
   
   para_file="DATA/Par_file_freq2sac"
   open(unit=10,file=trim(para_file),action='read',form="formatted",status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open parameter file'
      call exit_mpi('error opening parameter file')
   end if
   read(10,*) nstation_total
   read(10,*) nfrequency
   read(10,*) time_series_length
   read(10,*) source_depth,source_lat,source_lon
   read(10,*) ncomp
   read(10,*) omega_imag
   freq_input_dir="DATA"
   sac_output_dir="OUTPUT_FILES"
   read(10,*,iostat=ios) freq_input_dir
   if(ios /= 0) freq_input_dir="DATA"
   read(10,*,iostat=ios) sac_output_dir
   if(ios /= 0) sac_output_dir="OUTPUT_FILES"
   close(10)
   if(nstation_total.lt.nproc) stop 'nstation_total is smaller than nproc'
end subroutine read_para



subroutine read_station_spec()
    use spectotime_par
    implicit none 
    ! other variables
   character(len=256) ::station_file,spec_file
   integer ::ifreq,ista_total,istation,icomp
   complex(kind=8),dimension(:),allocatable ::spec_read_tmp
   integer ::ios

   character(len=30) :: dummy_net, dummy_name
   double precision :: dummy_lon, dummy_lat


   allocate(spec_read_tmp(nstation_total))

   station_file="DATA/station_list"
   open(unit=20,file=trim(station_file),action='read',form="formatted",status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open station list file'
      call exit_mpi('error opening station_list file')
   end if

   istation = 0
   do ista_total = 1, nstation_total
       ! Keep only the stations partitioned to this processor 
       if (ista_total >= ista_start .and. ista_total <= ista_end) then
           istation = istation + 1
           ! Read station details: longitude, latitude, network, and name
           read(20, *) station_net(istation), station_name(istation),&
                                          station_lon(istation), station_lat(istation)
       else
           ! Skip unwanted lines by reading into dummy variables
           read(20, *) dummy_net, dummy_name, dummy_lon, dummy_lat
       end if
   end do
   close(20)
   if(istation.ne.nstation) stop 'Error in read station_list'



!**********************************************************************
!**************************Read the FFT file***************************
   do ifreq=1,nfrequency
      write(spec_file,"(A,'/freq_',i5.5)")trim(freq_input_dir),ifreq-1
!      open(unit=30,file=trim(spec_file),action='read',form="formatted",status="old",iostat=ios)
      open(unit=30,file=trim(spec_file),action='read',form="unformatted",status="old",iostat=ios)
      if(ios /= 0) then
         print *,'error:could not open spectrum file ',trim(spec_file)
         call exit_mpi('error opening spectrum file')
      end if
      do icomp=1,ncomp
         read(30) spec_read_tmp(:)
         spec(1:nstation,icomp,ifreq)=spec_read_tmp(ista_start:ista_end)
      end do
      close(30)
   end do

end subroutine read_station_spec
