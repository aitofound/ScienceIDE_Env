subroutine compute_parameter_coupling(COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,&
                                 COUPLING_IR_TOP,COUPLING_IR_BOTTOM,coupling_nspec_xlow, &
                                 coupling_nspec_xhigh,coupling_nspec_ylow,coupling_nspec_yhigh,coupling_nspec_rtop,&
                                 coupling_nspec_rbottom,iproc_xi,iproc_eta, &
                                 NEX_XI,NEX_ETA,NEX_PER_PROC_XI,NEX_PER_PROC_ETA,NER, &
                                 USE_REGULAR_MESH,NDOUBLINGS,ner_doublings,DEBUG_COUPLING,myrank)

  implicit none
  logical USE_REGULAR_MESH
  integer myrank
  logical ::DEBUG_COUPLING
  integer NDOUBLINGS
  integer:: COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,COUPLING_IR_TOP,COUPLING_IR_BOTTOM
  integer::coupling_nspec_xlow,coupling_nspec_xhigh,coupling_nspec_ylow,coupling_nspec_yhigh,coupling_nspec_rtop,&
           coupling_nspec_rbottom
  integer::iproc_xi,iproc_eta
  integer::NEX_XI,NEX_ETA,NEX_PER_PROC_XI,NEX_PER_PROC_ETA,NER
  integer, dimension(2) :: ner_doublings
!  integer nblayers
!  integer ner_layer(nblayers)
  
  integer::ixmin,ixmax,iymin,iymax
  logical::ixlow_inside,ixhigh_inside,ixlowhigh_inside,iylow_inside,iyhigh_inside,iylowhigh_inside
  integer::dix,diy,dix1,diy1,dir1,dix2,diy2,dir2,dix3,diy3,dir3,dix4,diy4,dir4,dix5,diy5,dir5

 integer ::int_tmp

!The below three lines are used to avoid possible error reports during compling
!the code. NER and myrank are not used now, that occationally causes error 
!reports. But they might be usful in the future, so we keep them here.
 int_tmp=NEX_XI
 int_tmp=NEX_ETA
 int_tmp=NER


  coupling_nspec_xlow=0;coupling_nspec_xhigh=0
  coupling_nspec_ylow=0;coupling_nspec_yhigh=0
  coupling_nspec_rtop=0;coupling_nspec_rbottom =0
  ixmin=iproc_xi*NEX_PER_PROC_XI+1
  ixmax=(iproc_xi+1)*NEX_PER_PROC_XI
  iymin=iproc_eta*NEX_PER_PROC_ETA+1
  iymax=(iproc_eta+1)*NEX_PER_PROC_ETA
  ixlow_inside=.false.;ixhigh_inside=.false.;ixlowhigh_inside=.false.
  iylow_inside=.false.;iyhigh_inside=.false.;iylowhigh_inside=.false.

  if(ixmin.le.COUPLING_IXI_LOW.and.ixmax.ge.COUPLING_IXI_LOW) then
      ixlow_inside=.true.
  endif

  if(ixmin.le.COUPLING_IXI_HIGH.and.ixmax.ge.COUPLING_IXI_HIGH) then
      ixhigh_inside=.true.
  end if
  if(ixmin.ge.COUPLING_IXI_LOW.and.ixmax.le.COUPLING_IXI_HIGH) then
      ixlowhigh_inside=.true.
  end if
  if(iymin.le.COUPLING_IETA_LOW.and.iymax.ge.COUPLING_IETA_LOW) then
       iylow_inside=.true.
  end if
  if(iymin.le.COUPLING_IETA_HIGH.and.iymax.ge.COUPLING_IETA_HIGH) then
       iyhigh_inside=.true.
  end if
  if(iymin.ge.COUPLING_IETA_LOW.and.iymax.le.COUPLING_IETA_HIGH) then
       iylowhigh_inside=.true.
  end if


  dix=0
  if(ixlow_inside.and.ixhigh_inside) then
      dix=COUPLING_IXI_HIGH-COUPLING_IXI_LOW+1
  else if(ixlow_inside.and.(.not.ixhigh_inside)) then
      dix=ixmax-COUPLING_IXI_LOW+1
  else if(.not.ixlow_inside.and.ixhigh_inside) then
      dix=COUPLING_IXI_HIGH-ixmin+1
  else if(ixlowhigh_inside) then
      dix=NEX_PER_PROC_XI
  end if

  diy=0
  if(iylow_inside.and.iyhigh_inside) then
      diy=COUPLING_IETA_HIGH-COUPLING_IETA_LOW+1
  else if(iylow_inside.and.(.not.iyhigh_inside)) then
      diy=iymax-COUPLING_IETA_LOW+1
  else if(.not.iylow_inside.and.iyhigh_inside) then
      diy=COUPLING_IETA_HIGH-iymin+1
  else if(iylowhigh_inside) then
      diy=NEX_PER_PROC_ETA
  end if


  if(USE_REGULAR_MESH) then
   dix1=dix;diy1=diy;dir1=COUPLING_IR_TOP-COUPLING_IR_BOTTOM+1
   dix2=0;diy2=0;dir2=0
   dix3=0;diy3=0;dir3=0
   dix4=0;diy4=0;dir4=0
   dix5=0;diy5=0;dir5=0
   coupling_nspec_rtop=dix1*diy1
   coupling_nspec_rbottom=dix1*diy1
  else if(NDOUBLINGS.eq.1) then
   dix1=dix/2;diy1=diy/2
   dix2=dix/4;diy2=diy/4
   dix3=dix;diy3=diy
   dix4=0;diy4=0
   dix5=0;diy5=0
   if(ner_doublings(1)-2.gt.COUPLING_IR_BOTTOM.and.ner_doublings(1)-2.lt.COUPLING_IR_TOP) then
       dir1=ner_doublings(1)-2-COUPLING_IR_BOTTOM+1;dir2=2;dir3=COUPLING_IR_TOP-ner_doublings(1);dir4=0;dir5=0
       coupling_nspec_rbottom=dix1*diy1
       coupling_nspec_rtop=dix3*diy3
   else 
       stop 'COUPLING_IR_TOP is below doubling layer or COUPLING_IR_BOTTOM is above it'
   end if
 !  else if(ner_doublings(1).eq.Tele_ir.or.ner_doublings(1)-1.eq.Tele_ir) then
 !      dir1=0;dir2=2;dir3=NER-ner_doublings(1);dir4=0;dir5=0
 !      coupling_nspec_rtop=4*dix2*diy2
 !  else 
 !    dir1=0;dir2=0;dir3=NER-Tele_ir+1;dir4=0;dir5=0
 !      TeleEle_nr=dix3*diy3
 !  end if
!   print *,'TeleEle_nr=',TeleEle_nr,dix,diy
   
  else if(NDOUBLINGS.eq.2) then
   dix1=dix/4;diy1=diy/4
   dix2=dix/8;diy2=diy/8
   dix3=dix/2;diy3=diy/2
   dix4=dix/4;diy4=diy/4
   dix5=dix;diy5=diy
   if(ner_doublings(2)-2.gt.COUPLING_IR_BOTTOM.and.ner_doublings(1).lt.COUPLING_IR_TOP) then
       dir1=ner_doublings(2)-2-COUPLING_IR_BOTTOM+1;dir2=2;dir3=(ner_doublings(1) - 2) - ner_doublings(2)
       dir4=2;dir5=COUPLING_IR_TOP-ner_doublings(1)
       coupling_nspec_rbottom=dix1*diy1
       coupling_nspec_rtop=dix5*diy5
   else 
       stop 'COUPLING_IR_TOP is below doubling layer or COUPLING_IR_BOTTOM is above it'
!   else if (ner_doublings(2).eq.Tele_ir.or.ner_doublings(2)-1.eq.Tele_ir) then
!       dir1=0;dir2=2;dir3=(ner_doublings(1) - 2) - ner_doublings(2)
!       dir4=2;dir5=NER-ner_doublings(1)
!       TeleEle_nr=4*dix2*diy2
!   else if(ner_doublings(2).lt.Tele_ir.and.ner_doublings(1)-1.gt.Tele_ir) then
!       dir1=0;dir2=0;dir3=ner_doublings(1) - 2 -Tele_ir+1
!       dir4=2;dir5=NER-ner_doublings(1)
!       TeleEle_nr=dix3*diy3
!   else if(ner_doublings(1).eq.Tele_ir.or.ner_doublings(1)-1.eq.Tele_ir) then
!       dir1=0;dir2=0;dir3=0;dir4=2;dir5=NER-ner_doublings(1)
!       TeleEle_nr=4*dix4*diy4
!   else
!       dir1=0;dir2=0;dir3=0;dir4=0;dir5=NER-Tele_ir+1
!       TeleEle_nr=dix5*diy5
   end if
!   print *,'TeleEle_nr1=',TeleEle_nr,dix,diy
  end if

  if(ixlow_inside) then
     coupling_nspec_xlow=diy1*dir1+4*diy2*dir2+diy3*dir3+4*diy4*dir4+diy5*dir5
     if(DEBUG_COUPLING) then
         print  *,'coupling_nspec_xlow1',coupling_nspec_xlow,diy1*dir1,4*diy2*dir2,&
                            diy3*dir3,diy3,dix3,4*diy4*dir4,diy5*dir5,ixlowhigh_inside,&
                        ixmin,COUPLING_IXI_LOW,ixmax,COUPLING_IXI_HIGH,myrank,dir1,diy1,dir2,diy2,dir3,diy3
     end if
  end if 
  if(ixhigh_inside) then
     coupling_nspec_xhigh=diy1*dir1+4*diy2*dir2+diy3*dir3+4*diy4*dir4+diy5*dir5
     if(DEBUG_COUPLING) then
         print  *,'coupling_nspec_xhigh1',coupling_nspec_xlow,diy1*dir1,4*diy2*dir2,diy3*dir3,4*diy4*dir4,diy5*dir5,myrank
     end if
  end if
  if(iylow_inside) then
     coupling_nspec_ylow= dix1*dir1+4*dix2*dir2+dix3*dir3+4*dix4*dir4+dix5*dir5
     if(DEBUG_COUPLING) then
         print     *,'coupling_nspec_ylow1',dix1*dir1,4*dix2*dir2,dix3*dir3,4*dix4*dir4,dix5*dir5,myrank
     end if
  end if
  if(iyhigh_inside) then
     coupling_nspec_yhigh=dix1*dir1+4*dix2*dir2+dix3*dir3+4*dix4*dir4+dix5*dir5
     if(DEBUG_COUPLING) then
        print     *,'coupling_nspec_yhigh1',dix1*dir1,4*dix2*dir2,dix3*dir3,4*dix4*dir4,dix5*dir5,myrank
     end if
  end if
  if(DEBUG_COUPLING) then
        print *,'Nele_x_y_z',myrank,coupling_nspec_xlow,coupling_nspec_xhigh,coupling_nspec_ylow,coupling_nspec_yhigh,&
                coupling_nspec_rbottom,coupling_nspec_rtop
  end if
end subroutine compute_parameter_coupling
