PRO read_refstate,file,nr,r_ref,d_ref,temp_ref,p_ref,gacc_ref,gamma_ref
;file: input, character string, filename
;nr: output, integer, number of grid points in r for the 1-D reference state
;r_ref: output,, double vector of nr length, grid positions in r for the 1-D reference state profile
;d_ref: output, double vector of nr length, density profile for the 1-D reference state
;temp_ref: output, double vector of nr length, temperature profile for the 1-D reference state
;p_ref: output, double vector of nr length, pressure profile for the 1-D reference state
;gacc_ref: output, double vector of nr length, gravitational acceleration profile for the 1-D reference state
;gamma_ref: output, double vector of nr length, profile for the ratio of spacific heats for the 1-D reference state.

nr=long(1)
print,'read ',file

openr,/get_lun,unit,file
readu,unit,nr
r_ref=dblarr(nr)
d_ref=dblarr(nr)
temp_ref=dblarr(nr)
p_ref=dblarr(nr)
gacc_ref=dblarr(nr)
gamma_ref=dblarr(nr)
readu,unit,r_ref
readu,unit,d_ref
readu,unit,temp_ref
readu,unit,p_ref
readu,unit,gacc_ref
readu,unit,gamma_ref
free_lun,unit

return
end
