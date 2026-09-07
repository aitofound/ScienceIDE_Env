module constants
   include "constants.h"
end module constants

module  spectotime_par
   use constants, only:max_len
   implicit none
 
   integer, parameter ::ismooth=16
   double precision ::omega_imag

   integer ::nfrequency,nstation_total,nstation
   integer ::ista_start,ista_end
   double precision ::time_series_length
   double precision ::source_depth,source_lat,source_lon
   integer ::ncomp
   character(len=256) ::freq_input_dir,sac_output_dir
   double precision,dimension(:),allocatable ::station_lat,station_lon
   character(len=max_len), allocatable :: station_net(:), station_name(:)

   complex(kind=8),dimension(:,:,:),allocatable ::spec  !,spec1

   integer ::nproc,myrank

   real(kind=4), dimension(:),allocatable ::work_time
   complex(kind=8),dimension(:),allocatable ::work_spc

end module spectotime_par
