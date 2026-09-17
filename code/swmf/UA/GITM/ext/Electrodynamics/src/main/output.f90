
subroutine output_ie(iError)

  use ModMain
  use ModErrors
  use ModTimeAmie

  implicit none

  integer, intent(out) :: iError
  integer, dimension(7) :: itime

  real :: dst, dstCalc
  integer :: i, j

  iError = 0

  call time_real_to_int(currentTime, itime)

  if (.not. isInitialized) then

    write(*, *) ' => opening file : ', trim(filename)
    open(LunMainOutput, &
         file=trim(filename), &
         status='unknown', &
         form='UNFORMATTED', &
         iostat=iError)

    if (iError /= 0) then
      call set_error("Error opening output file: ")
      call set_error(filename)
      return
    endif

    write(LunMainOutput) nLats, nLons, nTimes
    if (isGeographic) then
      write(LunMainOutput) 90.0 - abs(geoLats(1, :))
      write(LunMainOutput) geoLons(:, 1)/15.0
      write(LunMainOutput) 6
      write(LunMainOutput) 'Electric Potential (kV)       '
      write(LunMainOutput) 'Auroral Energy Flux (mW/m2)   '
      write(LunMainOutput) 'Auroral Mean Energy (keV)     '
      write(LunMainOutput) 'isPolarCap                    '
      write(LunMainOutput) 'Magnetic Latitude (deg)       '
      write(LunMainOutput) 'Magnetic Local Time (hours)   '
    else
      write(LunMainOutput) 90.0 - abs(magLats(1, :))
      write(LunMainOutput) magMlts(:, 1)
      write(LunMainOutput) 4
      write(LunMainOutput) 'Electric Potential (kV)       '
      write(LunMainOutput) 'Auroral Energy Flux (mW/m2)   '
      write(LunMainOutput) 'Auroral Mean Energy (keV)     '
      write(LunMainOutput) 'isPolarCap                    '

    endif

    isInitialized = .true.

  endif

  write(LunMainOutput) iT, iTime(1), iTime(2), iTime(3), iTime(4), iTime(5)

  dst = 0.0
  dstCalc = 0.0
  write(LunMainOutput) swV, imfBx, imfBy, imfBz, 0.0, &
    ae, au, al, dst, dstCalc, hp, 0.0, &
    maxval(potential) - minval(potential)

  write(LunMainOutput) &
    ((potential(j, i), j=1, nLons), i=1, nLats)
  write(LunMainOutput) &
    ((eDiffuseEFlux(j, i), j=1, nLons), i=1, nLats)
  write(LunMainOutput) &
    ((eDiffuseAveE(j, i), j=1, nLons), i=1, nLats)
  write(LunMainOutput) &
    ((polarCap(j, i), j=1, nLons), i=1, nLats)

  if (isGeographic) then
    write(LunMainOutput) &
      ((magLats(j, i), j=1, nLons), i=1, nLats)
    write(LunMainOutput) &
      ((magMlts(j, i), j=1, nLons), i=1, nLats)
  endif

  return

end subroutine output_ie
