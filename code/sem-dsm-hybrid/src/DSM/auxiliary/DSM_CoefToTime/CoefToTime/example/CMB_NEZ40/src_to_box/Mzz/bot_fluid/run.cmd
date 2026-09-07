#copy the executable file
cp ../../../../../src/spectotime .


mkdir -p DATA OUTPUT_FILES
mkdir -p OUTPUT_FILES/solid_velo OUTPUT_FILES/stress
mkdir -p OUTPUT_FILES/chi_dot  OUTPUT_FILES/fluid_disp  OUTPUT_FILES/pressure 

#copy the resaved DSM coefficients
cp ../../../../../../Resave_Coef/example/CMB_NEZ40/src_to_box/Mzz/OUTPUT/dep_bot/order_* DATA/

#copy the elastic modulus coefficient, used to calculate stress.
#'matebot_forSpecCoefToTime' was written by lines 476-490 in DSM/src/savespec.f and now these lines are not executed. Just turn on the option (change the parameter output_more in savespec.f to .true.) to write out this file.
#cp ../../../../../../../../DSM//example/CMB_NEZ40/src_to_box/Mzz/OUTPUT_FILES/matetop_forSpecCoefToTime DATA/material_properties
#Now I just know the numbers in this file.
echo "   3432000.00000000
   10029.4000000000" > DATA/material_properties

#copy the distance table corresponding to the lower most depth from SPECFEM3D.
cp  ../../../../../../../../SPECFEM3D/EXAMPLES_COUPLING/CMB_NEZ40/OUTPUT_FILES/DATABASES_MPI/dist_table_elastic  DATA/dist_from_SPECFEM3D

#!!!!!!!!!!!
#!!!!!!!!!!!
#!!!!!!!!!!!
#I just know that the lines 2654-2686 of DATA/dist_from_SPECFEM3D are the distances of the last depth.
#If the model is changed, you have to accordingly modifiy the below line to get the correct distance table!!!!!!!!.
awk '{if(NR>=3482) print $0;}'  DATA/dist_from_SPECFEM3D > DATA/ndist_file




#copy Parbot_forSpecCoefToTime to Par_file and reedit it.
#The parameters 'tbegin_save', 'tend_save', 'npt_eachpack', 'lower_freq' and 'higher_freq' can not be determined by DSM running, so you need to specify them properly and rename this file as 'DATA/Par_file'.

#'tbegin_save' - The starting time to cut the DSM synthetics (e.g. 450 in this example is bouat 80 s before the P-wave arrival).
#'tend_save'   - The starting time to cut the DSM synthetics (e.g. 800 in this example is bouat 270 s before the P-wave arrival).
#'npt_eachpack' - The number of data points saved in each package (e.g. 1000).
#'lower_freq' and 'higher_freq' are used in the filtering, but not functional in the current version.

#file 'Parbot_forSpecCoefToTime' was written by lines 492-514 in DSM/src/savespec.f and now these lines are not executed.
#cp ../../../../../../../../DSM//example/CMB_NEZ40/src_to_box/Mzz/OUTPUT_FILES/Parbot_forSpecCoefToTime DATA/
#Now I just know the below numbers,
echo " .true.
           1
 214 1
        4000 300           2
        4096
   4096.00000000000
  6.00000000000000E-004
 200.0
 400.0
 600
 0.01 1.0" >DATA/Par_file


sbatch submit_job.cmd
