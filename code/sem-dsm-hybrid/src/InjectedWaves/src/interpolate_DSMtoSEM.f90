program interpolate_DSMtoSEM

use extract_Injectedwaves_par, only:dir_DSM_input,dir_output,para_file,ncomp_source,&
      source_type,dir_SEM_input,myrank,nproc,&
      sources_selected
implicit none

#ifdef USE_MPI
   include "mpif.h"
#endif
integer ::ndepth_this_media,idepth,idepth_start,idepth_end,media_type
integer ::icomp_source,ier
character(len=80) ::source_comp_name

#ifdef USE_MPI
   call initialize()
#endif

! Read the parameter and DSM input files
call read_para(para_file)
call read_DSM_para(dir_DSM_input,source_type)

! Interpretation for `ncomp_source`:
!   - Single-force source type: `ncomp_source = 1, 2, or 3` for any one, two or
!   three ofFr, Ft, or Fz
!   - Moment tensor source type: `ncomp_source = 6` for Mzz, Mrr, Mtt, Mzr,
!   Mzt, Mrt
!   - For single-force sources, you may choose specific components (e.g.,
!   vertical only 
!     with `ncomp_source = 1`) to save computational cost. See the mannual
!     for details.

! Loop over source components ([Fr, Ft, Fz] or [Mzz, Mrr, Mtt, Mzr, Mzt,
! Mrt])
do icomp_source=1,ncomp_source
   if (myrank == 0) then
      write(*, '(A,I2,A,I2,A)') "processing source ",icomp_source ," of ", ncomp_source, "..."
   end if

   source_comp_name=sources_selected(icomp_source)
  ! Process both fluid (media_type = 1) and solid media (media_type = 2)
   do media_type=1,2
     ! Read SEM depth and distance tables for the current media type from the
     ! SEM directory
     call read_SEM_depth_distance(dir_SEM_input,media_type,ndepth_this_media,icomp_source)

     ! Partition jobs among processors
     call job_partition(myrank,ndepth_this_media,nproc,idepth_start,idepth_end)
     if(idepth_end.gt.0) then
       do idepth=idepth_start,idepth_end
          ! Print progress for the master rank (rank 0)
          if (myrank .ge. 0) then
             write(*, '(A,I2,A,I3,A,I3,A)') "   media_type = ", media_type, &
                  "(1/2 for fluid/solid), processing depth ", &
                  idepth - idepth_start + 1, " of ", idepth_end - idepth_start + 1, "..."
          end if

         ! Check if distances and depths fall within the range of the Green's
         ! Function database.
         ! If valid, read the Green's Function database
         call read_DSM_database(idepth,media_type,source_comp_name)
         !do interpolations
         call do_interpolation(idepth,media_type)

         ! Apply IFFT and cut the specified time window
         call ifft_Greens_SEM(idepth,media_type)

         ! Save the interpolated Green's Function at the given depths and
         ! distances
         call cut_save_GreensFunc_SEM(idepth,idepth_end,media_type,source_comp_name,icomp_source,&
               ncomp_source,dir_output)
       end do !idepth
     end if ! idepth_end
  end do
end do
!deallocate(spc_zero_padding,time_series,data_in,data_filtered)

#ifdef USE_MPI
   call MPI_FINALIZE(ier)
#endif
if(myrank.eq.0) write(*, '(A)') "Job completed successfully!"

end program 
