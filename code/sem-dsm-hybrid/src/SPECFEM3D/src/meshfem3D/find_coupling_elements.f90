!find the elements on the coupling boundary, where stress and displacement will
!be saved

!WENBO
subroutine find_coupling_elements(COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW, &
             COUPLING_IETA_HIGH,COUPLING_IR_TOP,COUPLING_IR_BOTTOM,coupling_nspec_xlow,coupling_ixlow,&
             coupling_ispec_xlow,coupling_isubregion_xlow, &
             coupling_nspec_xhigh,coupling_ixhigh,coupling_ispec_xhigh, &
             coupling_isubregion_xhigh,coupling_nspec_ylow,coupling_iylow, &
             coupling_ispec_ylow,coupling_isubregion_ylow,coupling_nspec_yhigh, &
             coupling_iyhigh,coupling_ispec_yhigh,coupling_isubregion_yhigh, &
             coupling_nspec_rtop,coupling_irtop,coupling_ispec_rtop, &
             coupling_isubregion_rtop,coupling_nspec_rbottom,coupling_irbottom, &
             coupling_ispec_rbottom,coupling_isubregion_rbottom,ixele, &
             iyele,irele,isubregion,ispec_superbrick,ispec,iproc_xi,iproc_eta, &
             NEX_PER_PROC_XI,NEX_PER_PROC_ETA,NER,USE_REGULAR_MESH,NDOUBLINGS,myrank)

 implicit none

 logical USE_REGULAR_MESH
 integer NDOUBLINGS
 integer myrank
 integer ::COUPLING_IXI_LOW,COUPLING_IXI_HIGH,COUPLING_IETA_LOW,COUPLING_IETA_HIGH,COUPLING_IR_TOP,COUPLING_IR_BOTTOM
 integer ::COUPLING_IXI_LOW_local,COUPLING_IXI_HIGH_local,COUPLING_IETA_LOW_local, &
           COUPLING_IETA_HIGH_local,COUPLING_IR_TOP_local,COUPLING_IR_BOTTOM_local

 integer ::coupling_nspec_xlow,coupling_nspec_xhigh,coupling_nspec_ylow,coupling_nspec_yhigh,&
           coupling_nspec_rtop,coupling_nspec_rbottom
 integer,dimension(coupling_nspec_xlow)  ::coupling_ispec_xlow,coupling_isubregion_xlow
 integer,dimension(coupling_nspec_xhigh) ::coupling_ispec_xhigh,coupling_isubregion_xhigh
 integer,dimension(coupling_nspec_ylow)  ::coupling_ispec_ylow,coupling_isubregion_ylow
 integer,dimension(coupling_nspec_yhigh) ::coupling_ispec_yhigh,coupling_isubregion_yhigh
 integer,dimension(coupling_nspec_rtop)  ::coupling_ispec_rtop,coupling_isubregion_rtop
 integer,dimension(coupling_nspec_rbottom)  ::coupling_ispec_rbottom,coupling_isubregion_rbottom

  
 integer ::coupling_ixlow,coupling_ixhigh,coupling_iylow,coupling_iyhigh,coupling_irtop,coupling_irbottom
 logical ::ix_in_coupling,ix_on_low_coupling,ix_on_high_coupling,iy_in_coupling,iy_on_low_coupling,iy_on_high_coupling
 logical ::ir_in_coupling,ir_on_top_coupling,ir_on_bottom_coupling

 integer ::ixele,iyele,irele,irele_true
 integer ::isubregion,ispec_superbrick,ispec,iproc_xi,iproc_eta,NEX_PER_PROC_XI,NEX_PER_PROC_ETA,NER

 integer ::int_tmp

!The below two lines are used to avoid possible error reports during compling
!the code. NER and myrank are not used now, that occationally causes error 
!reports. But they might be usful in the future, so we keep them here.
 int_tmp=NER
 int_tmp=myrank

 if(USE_REGULAR_MESH) then
   COUPLING_IXI_LOW_local=(COUPLING_IXI_LOW-(iproc_xi)*NEX_PER_PROC_XI)
   COUPLING_IXI_HIGH_local=(COUPLING_IXI_HIGH-(iproc_xi)*NEX_PER_PROC_XI)
   COUPLING_IETA_LOW_local=(COUPLING_IETA_LOW-(iproc_eta)*NEX_PER_PROC_ETA)
   COUPLING_IETA_HIGH_local=(COUPLING_IETA_HIGH-(iproc_eta)*NEX_PER_PROC_ETA)
   COUPLING_IR_TOP_local=COUPLING_IR_TOP
   COUPLING_IR_BOTTOM_local=COUPLING_IR_BOTTOM
 else 
   if(NDOUBLINGS == 1) then
     select case (isubregion)
     case(1)
       COUPLING_IXI_LOW_local=(COUPLING_IXI_LOW-1-(iproc_xi)*NEX_PER_PROC_XI)/2+1
       COUPLING_IXI_HIGH_local=(COUPLING_IXI_HIGH-(iproc_xi)*NEX_PER_PROC_XI)/2
       COUPLING_IETA_LOW_local=(COUPLING_IETA_LOW-1-(iproc_eta)*NEX_PER_PROC_ETA)/2+1
       COUPLING_IETA_HIGH_local=(COUPLING_IETA_HIGH-(iproc_eta)*NEX_PER_PROC_ETA)/2
       COUPLING_IR_TOP_local=COUPLING_IR_TOP
       COUPLING_IR_BOTTOM_local=COUPLING_IR_BOTTOM
     case(2)
       COUPLING_IXI_LOW_local=(COUPLING_IXI_LOW-1-(iproc_xi)*NEX_PER_PROC_XI)/4+1
       COUPLING_IXI_HIGH_local=(COUPLING_IXI_HIGH-(iproc_xi)*NEX_PER_PROC_XI)/4
       COUPLING_IETA_LOW_local=(COUPLING_IETA_LOW-1-(iproc_eta)*NEX_PER_PROC_ETA)/4+1
       COUPLING_IETA_HIGH_local=(COUPLING_IETA_HIGH-(iproc_eta)*NEX_PER_PROC_ETA)/4
       COUPLING_IR_TOP_local=COUPLING_IR_TOP
       COUPLING_IR_BOTTOM_local=COUPLING_IR_BOTTOM
     case(3)
       COUPLING_IXI_LOW_local=(COUPLING_IXI_LOW-(iproc_xi)*NEX_PER_PROC_XI)
       COUPLING_IXI_HIGH_local=(COUPLING_IXI_HIGH-(iproc_xi)*NEX_PER_PROC_XI)
       COUPLING_IETA_LOW_local=(COUPLING_IETA_LOW-(iproc_eta)*NEX_PER_PROC_ETA)
       COUPLING_IETA_HIGH_local=(COUPLING_IETA_HIGH-(iproc_eta)*NEX_PER_PROC_ETA)
       COUPLING_IR_TOP_local=COUPLING_IR_TOP
       COUPLING_IR_BOTTOM_local=COUPLING_IR_BOTTOM
     case default
       stop 'Wrong number of subregions'
     end select
   else if(NDOUBLINGS == 2) then
     select case (isubregion)
     case(1)
        COUPLING_IXI_LOW_local=(COUPLING_IXI_LOW-1-(iproc_xi)*NEX_PER_PROC_XI)/4+1
        COUPLING_IXI_HIGH_local=(COUPLING_IXI_HIGH-(iproc_xi)*NEX_PER_PROC_XI)/4
        COUPLING_IETA_LOW_local=(COUPLING_IETA_LOW-1-(iproc_eta)*NEX_PER_PROC_ETA)/4+1
        COUPLING_IETA_HIGH_local=(COUPLING_IETA_HIGH-(iproc_eta)*NEX_PER_PROC_ETA)/4
        COUPLING_IR_TOP_local=COUPLING_IR_TOP
        COUPLING_IR_BOTTOM_local=COUPLING_IR_BOTTOM
     case(2)
        COUPLING_IXI_LOW_local=(COUPLING_IXI_LOW-1-(iproc_xi)*NEX_PER_PROC_XI)/8+1
        COUPLING_IXI_HIGH_local=(COUPLING_IXI_HIGH-(iproc_xi)*NEX_PER_PROC_XI)/8
        COUPLING_IETA_LOW_local=(COUPLING_IETA_LOW-1-(iproc_eta)*NEX_PER_PROC_ETA)/8+1
        COUPLING_IETA_HIGH_local=(COUPLING_IETA_HIGH-(iproc_eta)*NEX_PER_PROC_ETA)/8
        COUPLING_IR_TOP_local=COUPLING_IR_TOP
        COUPLING_IR_BOTTOM_local=COUPLING_IR_BOTTOM
     case(3)
        COUPLING_IXI_LOW_local=(COUPLING_IXI_LOW-1-(iproc_xi)*NEX_PER_PROC_XI)/2+1
        COUPLING_IXI_HIGH_local=(COUPLING_IXI_HIGH-(iproc_xi)*NEX_PER_PROC_XI)/2
        COUPLING_IETA_LOW_local=(COUPLING_IETA_LOW-1-(iproc_eta)*NEX_PER_PROC_ETA)/2+1
        COUPLING_IETA_HIGH_local=(COUPLING_IETA_HIGH-(iproc_eta)*NEX_PER_PROC_ETA)/2
        COUPLING_IR_TOP_local=COUPLING_IR_TOP
        COUPLING_IR_BOTTOM_local=COUPLING_IR_BOTTOM
      case(4)
        COUPLING_IXI_LOW_local=(COUPLING_IXI_LOW-1-(iproc_xi)*NEX_PER_PROC_XI)/4+1
        COUPLING_IXI_HIGH_local=(COUPLING_IXI_HIGH-(iproc_xi)*NEX_PER_PROC_XI)/4
        COUPLING_IETA_LOW_local=(COUPLING_IETA_LOW-1-(iproc_eta)*NEX_PER_PROC_ETA)/4+1
        COUPLING_IETA_HIGH_local=(COUPLING_IETA_HIGH-(iproc_eta)*NEX_PER_PROC_ETA)/4
        COUPLING_IR_TOP_local=COUPLING_IR_TOP
        COUPLING_IR_BOTTOM_local=COUPLING_IR_BOTTOM
      case(5)
        COUPLING_IXI_LOW_local=COUPLING_IXI_LOW-(iproc_xi)*NEX_PER_PROC_XI
        COUPLING_IXI_HIGH_local=COUPLING_IXI_HIGH-(iproc_xi)*NEX_PER_PROC_XI
        COUPLING_IETA_LOW_local=COUPLING_IETA_LOW-(iproc_eta)*NEX_PER_PROC_ETA
        COUPLING_IETA_HIGH_local=COUPLING_IETA_HIGH-(iproc_eta)*NEX_PER_PROC_ETA
        COUPLING_IR_TOP_local=COUPLING_IR_TOP
        COUPLING_IR_BOTTOM_local=COUPLING_IR_BOTTOM
      case default
        stop 'Wrong number of subregions'
      end select
   else
      stop 'Wrong number of doublings'
   end if
 end if
  
 ix_in_coupling=.false.;ix_on_low_coupling=.false.;ix_on_high_coupling=.false.
 iy_in_coupling=.false.;iy_on_low_coupling=.false.;iy_on_high_coupling=.false.
 ir_in_coupling=.false.;ir_on_top_coupling=.false.;ir_on_bottom_coupling=.false.

 if(ixele.le.COUPLING_IXI_HIGH_local.and.ixele.ge.COUPLING_IXI_LOW_local) then
    ix_in_coupling=.true.
 end if
 if(ixele.eq.COUPLING_IXI_LOW_local) then
    ix_on_low_coupling=.true.
 end if
 if(ixele.eq.COUPLING_IXI_HIGH_local) then
    ix_on_high_coupling=.true.
 end if
 if(iyele.le.COUPLING_IETA_HIGH_local.and.iyele.ge.COUPLING_IETA_LOW_local) then
     iy_in_coupling=.true.
 end if
 if(iyele.eq.COUPLING_IETA_LOW_local) then
    iy_on_low_coupling=.true.
 end if
 if(iyele.eq.COUPLING_IETA_HIGH_local) then
    iy_on_high_coupling=.true.
 end if

!The below lines are modified to match the new version SPECFEM3D. 
  if(isubregion.eq.1.or.isubregion.eq.2) then
     irele_true=irele     
  else if(isubregion.eq.3.or.isubregion.eq.4) then
!     irele_true=irele+1 
     irele_true=irele     
  else if(isubregion.eq.5) then
!     irele_true=irele+2
     irele_true=irele     
  end if
 


 if(irele_true.ge.COUPLING_IR_BOTTOM_local.and.irele_true.le.COUPLING_IR_TOP_local) then
    ir_in_coupling=.true.
 end if
 if(irele_true.eq.COUPLING_IR_TOP_local) then
    ir_on_top_coupling=.true.
 end if
 if(irele_true.eq.COUPLING_IR_BOTTOM_local) then
    ir_on_bottom_coupling=.true.
 end if

 if(ix_on_low_coupling.and.ir_in_coupling.and.iy_in_coupling) then
    if(isubregion.eq.2.or.isubregion.eq.4) then
      if(ispec_superbrick.eq.17.or.ispec_superbrick.eq.18.or.ispec_superbrick.eq.21.or.&
         ispec_superbrick.eq.24.or.ispec_superbrick.eq.25.or.ispec_superbrick.eq.26.or. &
         ispec_superbrick.eq.29.or.ispec_superbrick.eq.32) then
           coupling_ixlow=coupling_ixlow+1
           if(coupling_ixlow>coupling_nspec_xlow)  stop 'Error-superbrick, the number of x-low coupling boundary  &
                &elements larger than the pridicted number'
           coupling_ispec_xlow(coupling_ixlow)=ispec
           coupling_isubregion_xlow(coupling_ixlow)=isubregion
       end if
    else
       coupling_ixlow=coupling_ixlow+1
    if(coupling_ixlow>coupling_nspec_xlow)  stop 'Error, the number of x-low coupling boundary  &
         &elements larger than the pridicted number'
       coupling_ispec_xlow(coupling_ixlow)=ispec
       coupling_isubregion_xlow(coupling_ixlow)=isubregion
    end if
 end if



 if(ix_on_high_coupling.and.ir_in_coupling.and.iy_in_coupling) then
    if(isubregion.eq.2.or.isubregion.eq.4) then
      if(ispec_superbrick.eq.1.or.ispec_superbrick.eq.2.or.ispec_superbrick.eq.5.or.&
         ispec_superbrick.eq.8.or.ispec_superbrick.eq.9.or.ispec_superbrick.eq.10.or. &
         ispec_superbrick.eq.13.or.ispec_superbrick.eq.16) then
           coupling_ixhigh=coupling_ixhigh +1
           if(coupling_ixhigh>coupling_nspec_xhigh) stop 'Error-supperbrick, the number of x-high coupling boundary  &
                &elements larger than the pridicted number'
           coupling_ispec_xhigh(coupling_ixhigh)=ispec
           coupling_isubregion_xhigh(coupling_ixhigh)=isubregion
       end if
    else
       coupling_ixhigh=coupling_ixhigh+1
       if(coupling_ixhigh>coupling_nspec_xhigh) stop 'Error, the number of x-high coupling boundary  &
            &elements larger than the pridicted number'
       coupling_ispec_xhigh(coupling_ixhigh)=ispec
       coupling_isubregion_xhigh(coupling_ixhigh)=isubregion
    end if
 end if


  if(iy_on_low_coupling.and.ir_in_coupling.and.ix_in_coupling) then
     if(isubregion.eq.2.or.isubregion.eq.4) then
        if(ispec_superbrick.eq.10.or.ispec_superbrick.eq.11.or.ispec_superbrick.eq.14.or.&
           ispec_superbrick.eq.16.or.ispec_superbrick.eq.26.or.ispec_superbrick.eq.27.or. &
           ispec_superbrick.eq.30.or.ispec_superbrick.eq.32) then
             coupling_iylow=coupling_iylow+1
             if(coupling_iylow>coupling_nspec_ylow) stop 'Error-superbrick, the number of y-low &
                  & coupling boundary elements larger than the pridicted number'
             coupling_ispec_ylow(coupling_iylow)=ispec
             coupling_isubregion_ylow(coupling_iylow)=isubregion
         end if
     else
        coupling_iylow=coupling_iylow+1
        if(coupling_iylow>coupling_nspec_ylow) stop 'Error, the number of y-low &
             &coupling boundary elements larger than the pridicted number'
        coupling_ispec_ylow(coupling_iylow)=ispec
        coupling_isubregion_ylow(coupling_iylow)=isubregion
      end if
  end if


  if(iy_on_high_coupling.and.ir_in_coupling.and.ix_in_coupling) then
     if(isubregion.eq.2.or.isubregion.eq.4) then
        if(ispec_superbrick.eq.2.or.ispec_superbrick.eq.3.or.ispec_superbrick.eq.6.or. &
           ispec_superbrick.eq.8.or.ispec_superbrick.eq.18.or.ispec_superbrick.eq.19.or. &
           ispec_superbrick.eq.22.or.ispec_superbrick.eq.23) then
            coupling_iyhigh=coupling_iyhigh+1
            if(coupling_iyhigh>coupling_nspec_yhigh) stop 'Error-supperbrick, the number of y-high &
                 &coupling boundary elements larger than the pridicted number'
            coupling_ispec_yhigh(coupling_iyhigh)=ispec
            coupling_isubregion_yhigh(coupling_iyhigh)=isubregion
         end if
     else
        coupling_iyhigh=coupling_iyhigh+1
        if(coupling_iyhigh>coupling_nspec_yhigh) stop 'Error, the number of y-high &
             &coupling elements larger than the pridicted number'
        coupling_ispec_yhigh(coupling_iyhigh)=ispec
        coupling_isubregion_yhigh(coupling_iyhigh)=isubregion
     end if
  end if


  if(ir_on_top_coupling.and.ix_in_coupling.and.iy_in_coupling) then
      if(isubregion.eq.2.or.isubregion.eq.4) then
         if(ispec_superbrick.eq.8.or.ispec_superbrick.eq.16.or. &
            ispec_superbrick.eq.24.or.ispec_superbrick.eq.32) then
              coupling_irtop=coupling_irtop+1
              if(coupling_irtop>coupling_nspec_rtop) stop 'Error-supperbrick, the number of r-top  &
                   &coupling boundary elements are larger than the pridicted number'
              coupling_ispec_rtop(coupling_irtop)=ispec
              coupling_isubregion_rtop(coupling_irtop)=isubregion
         end if
      else
         coupling_irtop=coupling_irtop+1
              if(coupling_irtop>coupling_nspec_rtop) stop 'Error, the number of r-top  &
                   &coupling boundary elements are larger than the pridicted number'
         coupling_ispec_rtop(coupling_irtop)=ispec
         coupling_isubregion_rtop(coupling_irtop)=isubregion
      end if
  end if

  if(ir_on_bottom_coupling.and.ix_in_coupling.and.iy_in_coupling) then
      if(isubregion.eq.2.or.isubregion.eq.4) then
         if(ispec_superbrick.eq.8.or.ispec_superbrick.eq.16.or. &
            ispec_superbrick.eq.24.or.ispec_superbrick.eq.32) then
              coupling_irbottom=coupling_irbottom+1
              if(coupling_irbottom>coupling_nspec_rbottom) stop 'Error-supperbrick, the number of r-bottom  &
                   &coupling boundary elements are larger than the pridicted number'
              coupling_ispec_rbottom(coupling_irbottom)=ispec
              coupling_isubregion_rbottom(coupling_irbottom)=isubregion
         end if
      else
         coupling_irbottom=coupling_irbottom+1
              if(coupling_irbottom>coupling_nspec_rbottom) stop 'Error, the number of r-bottom  &
                   &coupling boundary elements are larger than the pridicted number'
         coupling_ispec_rbottom(coupling_irbottom)=ispec
         coupling_isubregion_rbottom(coupling_irbottom)=isubregion
      end if
  end if
end subroutine find_coupling_elements
