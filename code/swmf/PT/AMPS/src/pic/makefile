SHELL =/bin/sh

DEFAULT_TARGET : amps  

BATL=nobatl

include ../../Makefile.conf
include ../../Makefile.local

# -----------------------------------------------------------------------------
# NOTE:
# parallel/halo_sync.o and ecsim/halo_sync.o have the same archive member name
# (halo_sync.o). When both are added to amps.a, one may replace the other.
#
# To keep BOTH implementations in the same static library, we archive them
# under unique member names by creating renamed copies of the object files.
# This approach is source-extension agnostic and relies on the existing rules
# from Makefile.conf/Makefile.local to build the original .o files.
# -----------------------------------------------------------------------------
PAR_HALO_SYNC_OBJ   = parallel/halo_sync_parallel.o
ECSIM_HALO_SYNC_OBJ = ecsim/halo_sync_ecsim.o

PICOBJ = pic.o \
	field_line_parallel.o \
	field_line_particle_split.o \
	pic_time_step.o \
	pic_parallel.o \
	pic_bc.o \
	pic_init_const.o \
	pic_mesh.o \
	pic_mesh_search.o \
	pic_mol.o \
	pic_mol_parser.o \
	pic_mover.o \
	pic_mover_boris.o \
	pic_mover_guiding_center.o \
	pic_mover_error.o \
	pic_mover_trajectory_tracking.o \
	pic_mover_relativistic_guiding_center.o \
	pic_parser.o \
	pic_pbuffer.o \
	pic_weight_time.o \
	pic_internal_boundary_sphere.o \
	pic_sample_distribution_function.o \
	pic_sample_flux_distribution.o \
	pic_vel_distribution.o \
	pic_ices.o \
	pic_photolytic_reactions.o \
	pic_volume_particle_injection.o \
	pic_background_atmosphere.o \
	pic_column_integration.o \
	pic_particle_collision_models.o \
	pic_run_time_system_state.o \
	pic_lb.o \
	pic_qlb.o \
	pic_debugger.o \
	pic_debugger_concurrent.o \
	pic_internal_boundary_rotation_body.o \
	pic_internal_boundary_nastran_surface.o \
	pic_fluid.o \
	pic_swmf.o \
	pic__ray_tracing.o \
	pic_cut_cells.o \
	pic_pt.o \
	pic_sample_pitch_angle_distribution.o \
	pic_kameleon.o \
	pic_restart.o \
	pic_arms.o \
	pic_tecplot.o \
	pic_initial_conditions.o \
	pic_datafile.o \
	pic_interpolation_routines.o \
	pic_field_line.o \
	pic_ccmc.o \
	pic_user_particle_processing.o \
	pic_mover_relativistic_boris.o \
	pic_sample_energy_distribution_relativistic.o \
	pic_stopping_power.o \
	pic_bc_periodic.o \
	pic_field_solver_ecsim.o \
	pic_block_send_mask.o \
	pic_move_blocks.o \
	pic_rnd.o \
	pic_gyrokinetic.o \
	pic_electron_impact_ionization.o \
	pic_particle_spliting.o \
      	field_line_edge_parallel.o \
	field_line_vertex_parallel.o \
	mesh_boundary_blocks.o\
	mesh_boundary_cells.o \
	mesh_boundary_corners.o \
	ecsim/curl_b_second_order.o \
       	ecsim/curl_b_fourth_order.o \
	ecsim/grad_div_e_second_order.o \
	ecsim/grad_div_e_fourth_order.o \
	ecsim/grad_div_e_sixth_order.o \
	ecsim/grad_div_e_eighth_order.o \
	ecsim/curl_b_sixth_order.o \
	ecsim/curl_b_eighth_order.o \
	ecsim/laplacian_all_orders.o \
	ecsim/curl_curl_e_all_orders.o \
	ecsim/update_rhs.o \
	ecsim/get_stencil.o \
	ecsim/get_stencil_legacy_wrapper.o \
	ecsim/domain_bc.o \
	parallel/parallel_affinity.o \
	mesh/domain_boundary_bc_collectors.o \
	$(PAR_HALO_SYNC_OBJ) \
	parallel/init_send_halo.o \
	$(ECSIM_HALO_SYNC_OBJ) \
	units/units.o \
	gyro/drift_velocity.o \
	gyro/gyro_mover.o

	
ifneq ($(BATL),nobatl)	
PICOBJ+=pic_batsrus.o \
	pic_batsrus_wrapper.o
endif


# Build halo_sync objects with unique basenames (avoid archive member collisions)
$(PAR_HALO_SYNC_OBJ): parallel/halo_sync.cpp
	@mkdir -p $(dir $@)
	$(COMPILE.mpicxx) ${FLAGCC} -c $< -o $@

$(ECSIM_HALO_SYNC_OBJ): ecsim/halo_sync.cpp
	@mkdir -p $(dir $@)
	$(COMPILE.mpicxx) ${FLAGCC} -c $< -o $@

amps : $(PICOBJ)
	ar -src amps.a $(PICOBJ)
	@echo "Target 'amps' is done." 
