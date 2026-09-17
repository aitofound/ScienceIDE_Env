;  Copyright (C) 2002 Regents of the University of Michigan,
;  portions used with permission 
;  For more information, see http://csem.engin.umich.edu/tools/swmf
pro close_device, pdf=pdf, png=png, delete=delete, verbose=verbose

  if !d.name ne 'PS' then return
  device, /close
  set_plot, 'X'
  !p.font=-1

  if not keyword_set(pdf) and not keyword_set(png) then return

  ;; Convert PS/EPS file to PDF or PNG and remove original if required
  common SETDEVICE, NameFile
  OutFile = NameFile
  i = strpos(OutFile,'.',/reverse_search)
  OutFile = strmid(OutFile, 0, i)

  if keyword_set(pdf) then begin
     ;; Use value of pdf or the default conversion command
     if typename(pdf) eq "STRING" then Convert = pdf else Convert = 'epspdf'
     OutFile += '.pdf'
  endif else begin
     if typename(png) eq "STRING" then Convert = png else Convert = 'convert'
     OutFile += '.png'
  endelse
  Command = Convert + ' ' + NameFile + ' ' + OutFile

  if keyword_set(delete) then Command = Command + '; /bin/rm ' + NameFile

  if keyword_set(verbose) then print,'Command = ', Command

  spawn, Command

end
