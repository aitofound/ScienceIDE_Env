module resave_par
     implicit none
     integer,parameter ::ncomp_fluid=1
     integer,parameter ::ncomp_solid=3
     integer,parameter ::ndepth=2
     logical ::top_fluid,bot_fluid
     integer ::source_type
     integer ::nfrequency
     integer ::m0,nl_total
     integer ::nl_eachpack
     integer,dimension(:),allocatable ::ifreq_start

     integer ::nproc,myrank

    
     integer ::npack_total,npack_thisproc,l_start,l_end,nl_remained
     integer,dimension(2) ::ncomp_foridep

     complex(kind=8),dimension(:,:,:,:), allocatable ::coef_c_top,coef_dcdr_top     
     complex(kind=8),dimension(:,:,:,:), allocatable ::coef_c_bot,coef_dcdr_bot
     complex(kind=8),dimension(:,:,:,:), allocatable ::coef_c_read,coef_dcdr_read

end module resave_par
