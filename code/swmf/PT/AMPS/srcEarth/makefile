SHELL =/bin/sh

DEFAULT_TARGET : lib  

include ../../Makefile.conf

MAINLIBOBJ = Earth_Sampling.o \
	main_lib.o main_lib_geo.o Earth.o \
	BoundaryInjection.o \
	BoundaryInjection_ELECTRONS.o \
	BoundaryInjection_SEP.o \
	BoundaryInjection_GCR.o \
	BoundaryInjection_SW.o \
	GCR_Badavi2011ASR.o \
	CutoffRigidity.o \
	gridless/CutoffRigidityGridless.o \
	gridless/DensityGridless.o \
	gridless/DipoleInterface.o \
	gridless/GridlessParticleMovers.o \
	gridless/AnisotropicSpectrum.o \
	3d/Mode3D.o \
	3d/ElectricField.o \
	3d/GlobalMagneticField.o \
	3d/Mode3DParallel.o \
	3d/CutoffRigidityMode3D.o \
	3d/DensityMode3D.o \
	3d_forward/Density3D.o \
	3d_forward/SphereFlux3D.o \
	3d_forward/ForwardParticleMovers.o \
	3d_forward/Mode3DForward.o \
	3d_forward_swmf/Mode3DForwardSWMF.o \
	util/amps_param_parser.o \
	util/cutoff_cli.o \
	boundary/spectrum.o \
	ImpulseSource.o \
	DomainBoundaryParticlePropertyTable.o \
	parser.o \
	SamplingIndividualPoints.o \
	OutsideParticleFlux.o \
	EnergeticParticlePhysics.o \
	Atmosphere.o \
	pt.o 
	
	
MAINOBJ = main.o

lib : $(MAINLIBOBJ)
	ar -rc mainlib.a $(MAINLIBOBJ)
	@echo "Target 'main' is done." 

amps : $(MAINOBJ) 
	ar -rc main.a $(MAINOBJ)

