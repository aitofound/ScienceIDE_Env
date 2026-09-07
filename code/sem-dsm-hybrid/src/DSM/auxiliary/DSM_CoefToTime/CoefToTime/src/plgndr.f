        real*8 function plgndr(l,m,x)
cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
c computing the associated Legendre polynominal
c   (from Numerical Recipies).
c   required subroutines: error_handling
c   required functions: plgndr
cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
        integer l,m
        real*8 x
        integer i,ll
        real*8 fact,pll,pmm,pmmp1,somx2
c
c        if(m.lt.0.or.m.gt.l.or.abs(x).gt.1.) stop 'Error in m and l'
cWENBO
c              print *,'l,m',l,m,x
        if(m.lt.0.or.m.gt.l.or.abs(x).gt.1.) then
              print *,'l,m',l,m,x
              stop 'Error in m and l'
        end if
        pmm=1.d0
        if(m.gt.0) then
          somx2=dsqrt((1.d0-x)*(1.d0+x))
          fact=1.d0
          do 11 i=1,m
            pmm=-pmm*fact*somx2
            fact=fact+2.d0
11        continue
        endif
        if(l.eq.m) then
          plgndr=pmm
        else
          pmmp1=x*dble(2*m+1)*pmm
          if(l.eq.m+1) then
            plgndr=pmmp1
          else
            do 12 ll=m+2,l
              pll=(x*dble(2*ll-1)*pmmp1-dble(ll+m-1)*pmm)/dble(ll-m)
              pmm=pmmp1
              pmmp1=pll
12          continue
            plgndr=pll
          endif
        endif
c
        return
        end
