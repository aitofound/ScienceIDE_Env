# *********************************************************
#                                                     
#                  PLUTO 4.1  Makefile  
#                                                     
# *********************************************************

pluto:                              # Default target

ARCH         = Casper.mpicc.defs
PLUTO_DIR    = /home/lorenzo/PLUTO
SRC          = $(PLUTO_DIR)/Src
INCLUDE_DIRS = -I. -I$(SRC)
VPATH        = ./:$(SRC)/New:$(SRC):$(SRC)/Time_Stepping:$(SRC)/States

include $(PLUTO_DIR)/Config/$(ARCH)

# ---------------------------------------------------------
#         Set headers and object files 
# ---------------------------------------------------------

HEADERS = pluto.h prototypes.h structs.h definitions.h macros.h mod_defs.h plm_coeffs.h
OBJ = adv_flux.o arrays.o boundary.o check_states.o  \
      cmd_line_opt.o entropy_switch.o  \
      findshock.o flag_shock.o flag.o flatten.o get_nghost.o   \
      init.o int_bound_reset.o input_data.o mappers3D.o  \
      parse_file.o plm_coeffs.o set_indexes.o set_geometry.o set_output.o \
      tools.o var_names.o visc_flux.o 

OBJ += bin_io.o colortable.o initialize.o jet_domain.o \
       main.o restart.o show_config.o  \
       set_image.o setup.o set_grid.o startup.o split_source.o \
       userdef_output.o write_data.o write_tab.o \
       write_img.o write_vtk.o 

OBJ += update_stage.o 

include $(SRC)/Math_Tools/makefile

# ---------------------------------------------------------
#  Define macros by adding -D<name> where <name> has been
#  set to TRUE in the system configuration file (.defs) 
# ---------------------------------------------------------

ifeq ($(strip $(PARALLEL)), TRUE)
 CFLAGS += -I$(SRC)/Parallel -DPARALLEL
 include $(SRC)/Parallel/makefile
 ifeq ($(strip $(USE_ASYNC_IO)), TRUE)
  CFLAGS += -DUSE_ASYNC_IO
 endif
endif

ifeq ($(strip $(USE_HDF5)), TRUE)
 CFLAGS += -DUSE_HDF5
 OBJ    += hdf5_io.o
endif
      
ifeq ($($strip $(USE_PNG)), TRUE)
 CFLAGS += -DUSE_PNG
endif

-include local_make

# ---------------------------------------------------------
#   Additional_header_files_here   ! dont change this line
# ---------------------------------------------------------

HEADERS += ppm_coeffs.h

# ---------------------------------------------------------
#   Additional_object_files_here   ! dont change this line
# ---------------------------------------------------------

OBJ += ppm_states.o
OBJ += ppm_coeffs.o
OBJ += rk_update.o
include $(SRC)/HD/makefile
include $(SRC)/EOS/Ideal/makefile

# ---------------------------------------------------------
#    PLUTO target rule
# ---------------------------------------------------------

pluto: $(OBJ) 
	$(CC) $(OBJ) $(LDFLAGS) -o $@

# ---------------------------------------------------------
#                    Suffix rule
# ---------------------------------------------------------

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $<

clean:
	@rm -f	*.o
	@echo make clean: done

# ---------------------------------------------------------
#          Dependencies for object files
# ---------------------------------------------------------

$(OBJ):  $(HEADERS)

