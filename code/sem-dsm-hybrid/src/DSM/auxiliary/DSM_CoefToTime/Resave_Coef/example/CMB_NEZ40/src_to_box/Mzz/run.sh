mkdir -p DATA OUTPUT
mkdir -p OUTPUT/dep_bot OUTPUT/dep_top
#copy the executable file
cp ../../../../src/resave .

#copy the DSM results to DATA.
cp ../../../../../../../DSM/example/CMB_NEZ40/src_to_box/Mzz/OUTPUT_FILES/coef_cAnddcdr/freq* DATA/

#copy the file 'topbot_forResave' for preparing the input parameter file 'DATA/Par_file'.
#The parameter 'nl_eachpack' in 'topbot_forResave' can not be determined by DSM running, so you need to specify it(i.e. 300) and rename this file as 'DATA/Par_file'
#Note that the file 'topbot_forResave' was saved by lines 432-450 in DSM/src/savespec.f, but now that part was not executed (the parameter output_more=.false.). Just turn it on if you want to write it out.
#cp ../../../../../../../DSM/example/CMB_NEZ40/src_to_box/Mzz/OUTPUT_FILES/topbot_forResave DATA/
#cp DATA/topbot_forResave DATA/Par_file


# .false.   #top_fluid
# .true.    #bottom_fluid
#           1   #source_type - 1 for moment tensor and 2 for single force
#        4096  #nfreq
#        4001   #maximum_sphe_degree_l. Need to make output_more=.true. in DSM/src/savespec.f to print out this parameter.
# 300           #number of degrees saved in one pacakge

echo .false. >DATA/Par_file
echo  .true. >>DATA/Par_file
echo 1 >>DATA/Par_file
echo 4096 >>DATA/Par_file
echo 4001 >>DATA/Par_file
echo 300 >>DATA/Par_file

sbatch submit_job.cmd
