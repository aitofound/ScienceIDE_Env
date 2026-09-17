PRO read_physdata,file,time,nr,nth,nph,rgrid,thgrid,phgrid,data
;file: input, character string, filename
;time: output, double, time of the output data
;nr: output, integer, number of grid points in r
;nth: output, integer, number of grid points in theta
;nph: output, integer, number of grid points in phi
;rgrid: output, double vector of nr length, grid positions in r for the data
;thgrid: output, double vector of nth length, grid positions in theta for the data
;phgrid: output, double vector of nph length, grid positions in phi for the data
;data: output, double array of (nr, nth, nph) size, field data cube

time=0.D0
nr=long(1)
nth=long(1)
nph=long(1)

print,'read ',file

openr,/get_lun,unit,file
readu,unit,time
readu,unit,nr,nth,nph
rgrid=dblarr(nr)
thgrid=dblarr(nth)
phgrid=dblarr(nph)
data=dblarr(nr,nth,nph)
readu,unit,rgrid
readu,unit,thgrid
readu,unit,phgrid
readu,unit,data
free_lun,unit
print,'time= ',time
print,'nr, nth, nph= ', nr, nth, nph

return
end
