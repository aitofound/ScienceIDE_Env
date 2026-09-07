# Compatibility entry point. The configured top-level Makefile owns the
# compiler settings, dependency tracking, and cleanup for this executable.

SPECFEM3D_ROOT := ../..

.PHONY: default all cubedsphere_topo xcubedsphere_topo clean

default all cubedsphere_topo xcubedsphere_topo:
	$(MAKE) -C $(SPECFEM3D_ROOT) xcubedsphere_topo

clean:
	$(MAKE) -C $(SPECFEM3D_ROOT) CLEAN=cubedsphere_topo clean
