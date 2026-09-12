SHELL =/bin/sh

DEFAULT_TARGET : lib  

include ../../Makefile.conf

MAINLIBOBJ = exosphere_module.o  main_lib.o inner_boundary_injection.o outer_boundary_injection.o mesh.o mover.o parker.o scattering.o drift.o sep.o field_line.o sampling.o diffusion.o output.o shock_injection.o BoundaryInjection.o BoundaryInjection_GCR.o GCR_Badavi2011ASR.o parser.o diffusion_dxx.o tests.o shock_analytical_model.o diffusion_dmumu.o DLT.o QLT.o cooling.o QLT1.o sampling_output.o output_fl_background.o shock_analytical_model2D.o sample3d_init.o sample3d_sampling.o sample3d_output.o solar_wind.o   alfven_turbulence.o parker_streaming_calculator.o turbulence_advection_kolmogorov.o wave_energy_initialization.o test_wave_energy_initialization.o wave_particle_coupling_kolmogorov.o fte_mover.o growth_rate_validation_test.o kolmogorov.o kolmogorov_scatter.o fte_mover_dmumu.o  shock1d_vertex_distance.o sw1d.o turbulence_cascade_kolmogorov.o turbulence_reflection_kolmogorov.o turbulence_wave_number_resolved/turbulence_wave_number_resolved.o 
# sep_cli.o is kept under srcSEP/util to separate command-line parsing from
# the physics driver.  It is archived into main.a because only the standalone
# executable uses these run-time switches; the AMPS library objects remain
# independent of the CLI.
MAINOBJ = main.o util/sep_cli.o debug/sep_debug_fieldline_datum.o  

lib : $(MAINLIBOBJ)
	ar -rc mainlib.a $(MAINLIBOBJ)
	@echo "Target 'main' is done." 

amps : $(MAINOBJ) 
	ar -rc main.a $(MAINOBJ)

