module ModCimiUtil

    implicit none

    private ! except

    public::lintp2aIM
    public::lintp2IM
    public::lintp3IM
    public::locate1IM
    public::tridagIM
    public::ln_gamma

contains
!==============================================================================

! OLD LINTP
!!-----------------------------------------------------------------------
! subroutine lintp(xx,yy,n,x,y,ier)
!  !-----------------------------------------------------------------------
!  !  1-D interpolation.  xx must be increasing or decreasin monotonically.
!  !  x is between xx(1) and xx(n)
!  !
!  !  input: xx,yy,n,x
!  !  output: y,ier
!
!  implicit none
!
!  integer:: n,ier,i,jl,ju,jm,j
!  real:: xx(n),yy(n),x,y,d
!
!  ier = 0
!
!  ! Make sure xx is increasing or decreasing monotonically
!  do i=2,n
!     if (xx(n) > xx(1).and.xx(i) < xx(i-1)) then
!        write(*,*) ' lintp: xx is not increasing monotonically '
!        write(*,*) n,xx
!        stop
!     endif
!     if (xx(n) < xx(1).and.xx(i) > xx(i-1)) then
!        write(*,*) ' lintp: xx is not decreasing monotonically '
!        write(*,*) n,xx
!        stop
!     endif
!  enddo
!
!  ! Set ier=1 if out of range
!  if (xx(n) > xx(1)) then
!     if (x < xx(1).or.x > xx(n)) ier=1
!  else
!     if (x > xx(1).or.x < xx(n)) ier=1
!  endif
!  if (ier == 1) then
!     write(*,*) ' Error: ier == 1'
!     print *,'n,x ',n,x
!     print *,'xx(1:n) ',xx(1:n)
!     stop
!  endif
!
!  ! initialize lower and upper values
!  jl=1
!  ju=n
!
!  ! if not done compute a midpoint
! 10 if (ju-jl > 1) then
!     jm=(ju+jl)/2
!     ! now replace lower or upper limit
!     if ((xx(n) > xx(1)).eqv.(x > xx(jm))) then
!        jl=jm
!     else
!        ju=jm
!     endif
!     ! try again
!     go to 10
!  endif
!
!  ! this is the j
!  j=jl      ! if x <= xx(1) then j=1
!  ! if x > x(j).and.x <= x(j+1) then j=j
!  ! if x > x(n) then j=n-1
!  d=xx(j+1)-xx(j)
!  y=(yy(j)*(xx(j+1)-x)+yy(j+1)*(x-xx(j)))/d
!
! end subroutine lintp

!==============================================================================
subroutine lintp2aIM(x,y,v,nx,ny,x1,y1,v1)

  ! Calculate 2-d interplation. x is 2-D and y is 1-D.
  ! The grid can be distorted.

  implicit none

  integer, intent(in):: nx, ny
  real, intent(in) :: x(nx,ny), y(ny), v(nx,ny), x1, y1
  real, intent(out):: v1

  integer:: j, j1, i, i1, i2, i3
  real:: a, a1, b, x1d(1000)   ! max(nx)=1000
  real:: q00, q01, q10, q11
  !----------------------------------------------------------------------------
  call locate1IM(y,ny,y1,j)
  j1 = j+1
  if (j == 0.or.j1 > ny) then
     b = 1
     if (j == 0)  j  = j1
     if (j1 > ny) j1 = j
  else
     b = (y1 - y(j))/(y(j+1) - y(j))
  endif

  ! Interpolate along y(j)
  x1d(1:nx) = x(1:nx,j)
  call locate1IM(x1d,nx,x1,i)
  i1 = i + 1
  if (i == 0.or.i1 > nx) then
     a = 1
     if (i == 0) i = i1
     if (i1 > nx) i1 = i
  else
     a = (x1-x1d(i))/(x1d(i+1)-x1d(i))
  endif

  ! Interpolate along y(j1)
  x1d(1:nx) = x(1:nx,j1)
  call locate1IM(x1d,nx,x1,i2)
  i3 = i2 + 1
  if (i2 == 0 .or. i3 > nx) then
     a1 = 1
     if (i2 == 0) i2 = i3
     if (i3 > nx) i3 = i2
  else
     a1 = (x1-x1d(i2))/(x1d(i2+1)-x1d(i2))
  endif

  ! Coefficients for v(i,j) and v(i1,j)
  q00 = (1-a)*(1-b)
  q10 = a*(1-b)

  ! Coefficients for v(i2,j1) and v(i3,j1)
  q01 = (1-a1)*b
  q11 = a1*b

  v1 = q00*v(i,j) + q01*v(i2,j1) + q10*v(i1,j) + q11*v(i3,j1)

end subroutine lintp2aIM
!==============================================================================
subroutine lintp2IM(x, y, v, nx, ny, x1, y1, v1)

  ! Do 2-D interpolation. x and y must be increasing or decreasing
  ! monotonically

  implicit none
  
  integer, intent(in):: nx, ny
  real, intent(in):: x(nx), y(ny), v(nx,ny), x1, y1
  real, intent(out):: v1

  integer:: i, j, i1, j1
  real:: a, b, q00, q01, q10, q11
  !----------------------------------------------------------------------------
  call locate1IM(x,nx,x1,i)
  if (i > (nx-1)) i=nx-1      ! extrapolation if out of range
  if (i < 1) i=1              ! extrapolation if out of range
  i1 = i + 1
  a = (x1 - x(i))/(x(i1) - x(i))

  call locate1IM(y,ny,y1,j)
  if (j > (ny-1)) j=ny-1      ! extrapolation if out of range
  if (j < 1) j=1              ! extrapolation if out of range
  j1 = j + 1
  b = (y1 - y(j))/(y(j1) - y(j))

  q00 = (1-a)*(1.-b)
  q01 = (1-a)*b
  q10 = a*(1-b)
  q11 = a*b
  v1 = q00*v(i,j) + q01*v(i,j1) + q10*v(i1,j) + q11*v(i1,j1)

end subroutine lintp2IM
!==============================================================================
subroutine locate1IM(xx, n, x, j)

  ! Return a value of j such that x is between xx(j) and xx(j+1).
  ! xx must be increasing or decreasing monotonically.
  ! If xx is increasing:
  !    If x=xx(m), j=m-1 so if x=xx(1), j=0  and if x=xx(n), j=n-1
  !    If x < xx(1), j=0  and if x > xx(n), j=n
  ! If xx is decreasing:
  !    If x=xx(m), j=m so if x=xx(1), j=1  and if x=xx(n), j=n
  !    If x > xx(1), j=0  and if x < xx(n), j=n
  !
  ! Make sure xx is increasing or decreasing monotonically
  ! Input: xx,n,x
  ! Output: j

  use ModUtilities, ONLY: CON_stop

  implicit none

  integer, intent(in):: n
  real,    intent(in):: xx(n), x
  integer, intent(out):: j

  integer:: i, jl, ju, jm
  !----------------------------------------------------------------------------
  do i=2,n
     if (xx(n) > xx(1).and.xx(i) < xx(i-1)) then
        write(*,*) ' locate1IM: xx is not increasing monotonically '
        write(*,*) n, (xx(j),j=1,n)
        call CON_stop('CIMI stopped in locate1IM')
     endif
     if (xx(n) < xx(1).and.xx(i) > xx(i-1)) then
        write(*,*) ' locate1IM: xx is not decreasing monotonically '
        write(*,*) ' n, xx  ',n,xx
        call CON_stop('CIMI stopped in locate1IM')
     endif
  enddo

  jl=0
  ju=n+1
  test: do
     if (ju-jl <= 1) EXIT test
     jm=(ju+jl)/2
     if ((xx(n) > xx(1)).eqv.(x > xx(jm))) then
        jl=jm
     else
        ju=jm
     endif
  end do test
  j=jl

end subroutine locate1IM
!==============================================================================
subroutine lintp3IM(x, y, z, v, nx, ny, nz, x1, y1, z1, v1)

  ! 3-d interplation to position x1, y1, z1

  implicit none

  integer, intent(in):: nx, ny, nz
  real, intent(in):: x(nx),y(ny),z(nz),v(nx,ny,nz)
  real, intent(in):: x1, y1, z1
  real, intent(out):: v1

  integer:: i, j, k, i1, j1, k1
  real:: a, b, c, q000, q001, q010, q011, q100, q101, q110, q111
  !----------------------------------------------------------------------------
  call locate1IM(x,nx,x1,i)
  if (i > (nx-1)) i=nx-1      ! extrapolation if out of range
  if (i < 1) i=1              ! extrapolation if out of range

  call locate1IM(y,ny,y1,j)
  if (j > (ny-1)) j=ny-1      ! extrapolation if out of range
  if (j < 1) j=1              ! extrapolation if out of range

  call locate1IM(z,nz,z1,k)
  if (k > (nz-1)) k=nz-1      ! extrapolation if out of range
  if (k < 1) k=1              ! extrapolation if out of range

  i1 = i + 1
  j1 = j + 1
  k1 = k + 1
  a = (x1 - x(i))/(x(i1) - x(i))
  b = (y1 - y(j))/(y(j1) - y(j))
  c = (z1 - z(k))/(z(k1) - z(k))

  q000 = (1-a)*(1.-b)*(1.-c)*v(i,j,k)
  q001 = (1-a)*(1.-b)*c*v(i,j,k1)
  q010 = (1-a)*b*(1.-c)*v(i,j1,k)
  q011 = (1-a)*b*c*v(i,j1,k1)
  q100 = a*(1-b)*(1.-c)*v(i1,j,k)
  q101 = a*(1-b)*c*v(i1,j,k1)
  q110 = a*b*(1-c)*v(i1,j1,k)
  q111 = a*b*c*v(i1,j1,k1)

  v1 = q000 + q001 + q010 + q011 + q100 + q101 + q110 + q111

end subroutine lintp3IM
!==============================================================================
subroutine tridagIM(a,b,c,r,u,n,ier)

  implicit none

  integer, parameter:: nmax = 100
  integer:: n, ier, j
  real:: gam(nmax),a(n),b(n),c(n),r(n),u(n),bet
  !----------------------------------------------------------------------------

  ! problem can be simplified to n-1
  if(b(1) == 0.)then
     ier = 1
     RETURN
  endif
  ier = 0
  bet=b(1)
  u(1)=r(1)/bet

  ! decomposition and forward substitution
  do j=2, n
     gam(j) = c(j-1)/bet
     bet = b(j)-a(j)*gam(j)

     !    algotithm fails
     if(bet == 0.)then
        ier = 2
        RETURN
     endif
     u(j)=(r(j)-a(j)*u(j-1))/bet
  end do
  ! back substitution
  do j=n-1,1,-1
     u(j) = u(j) - gam(j+1)*u(j+1)
  end do

end subroutine tridagIM
!==============================================================================
real function ln_gamma(xx)

  implicit none

  real, intent(in) :: xx
  
  ! Calculate ln(gamma(xx))
  ! Added from Mei-Ching's stanadalone CIMI to calculate the natural
  ! logarithm of the gamma function, which is needed for calculating
  ! kappa distributions for the electrons.  -Colin, 07/25/2015.

  real, parameter:: stp = 2.50662827465d0
  real, parameter:: cof(6) = [76.18009173d0, -86.50532033d0, 24.01409822d0,&
       -1.231739516d0, 0.120858003d-2, -0.536382d-5]
  real:: x, tmp, ser
  integer:: j
  !----------------------------------------------------------------------------
  x = xx - 1
  tmp = x + 5.5
  tmp = (x + 0.5)*log(tmp) - tmp
  ser = 1
  do j = 1, 6
     x = x + 1
     ser = ser + cof(j)/x
  enddo
  ln_gamma = tmp + log(stp*ser)

end function ln_gamma

end module ModCimiUtil
