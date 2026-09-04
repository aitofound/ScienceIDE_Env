#include "chromosphere.hpp"

using namespace chromosphere;

Vec chromosphere::ip1(const Vec& xn) {

  // instantiate
  Vec xn_ip1(arma::size(xn));

  // permutate
  #pragma omp parallel for collapse(3)
  for (uword j=0; j<nz; j++){
    for (uword i=0; i<nx-1; i++) {
      for (uword k=0; k<num_of_eq; k++) {

        xn_ip1(sub2ind(arma::size(nx, nz, num_of_eq), i, j, k))
          = xn(sub2ind(arma::size(nx, nz, num_of_eq), i+1, j, k));
      }
    }
  }

  // default B.C.
  #pragma omp parallel for collapse(2)
  for (uword j=0; j<nz; j++){
    for (uword k=0; k<num_of_eq; k++){
      xn_ip1(sub2ind(arma::size(nx, nz, num_of_eq), nx-1, j, k))
        = xn(sub2ind(arma::size(nx, nz, num_of_eq), nx-1, j, k));
    }
  }

  return xn_ip1;
}

Vec chromosphere::im1(const Vec& xn) {

  // instantiate
  Vec xn_im1(arma::size(xn));

  // permutate
  #pragma omp parallel for collapse(3)
  for (uword j=0; j<nz; j++){
    for (uword i=1; i<nx; i++) {
      for (uword k=0; k<num_of_eq; k++) {

        xn_im1(sub2ind(arma::size(nx, nz, num_of_eq), i, j, k))
          = xn(sub2ind(arma::size(nx, nz, num_of_eq), i-1, j, k));
      }
    }
  }

  // default B.C.
  #pragma omp parallel for collapse(2)
  for (uword j=0; j<nz; j++){
    for (uword k=0; k<num_of_eq; k++){
      xn_im1(sub2ind(arma::size(nx, nz, num_of_eq), 0, j, k))
        = xn(sub2ind(arma::size(nx, nz, num_of_eq), 0, j, k));
    }
  }

  return xn_im1;
}

Vec chromosphere::jp1(const Vec& xn) {

  // instantiate
  Vec xn_jp1(arma::size(xn));

  // permutate
  #pragma omp parallel for collapse(3)
  for (uword j=0; j<nz-1; j++){
    for (uword i=0; i<nx; i++) {
      for (uword k=0; k<num_of_eq; k++) {

        xn_jp1(sub2ind(arma::size(nx, nz, num_of_eq), i, j, k))
          = xn(sub2ind(arma::size(nx, nz, num_of_eq), i, j+1, k));
      }
    }
  }

  // default B.C.
  #pragma omp parallel for collapse(2)
  for (uword i=0; i<nx; i++){
    for (uword k=0; k<num_of_eq; k++){
      xn_jp1(sub2ind(arma::size(nx, nz, num_of_eq), i, nz-1, k))
        = xn(sub2ind(arma::size(nx, nz, num_of_eq), i, nz-1, k));
    }
  }

  return xn_jp1;
}

Vec chromosphere::jm1(const Vec& xn) {

  // instantiate
  Vec xn_jm1(arma::size(xn));

  // permutate
  #pragma omp parallel for collapse(3)
  for (uword j=1; j<nz; j++){
    for (uword i=0; i<nx; i++) {
      for (uword k=0; k<num_of_eq; k++) {
        xn_jm1(sub2ind(arma::size(nx, nz, num_of_eq), i, j, k))
          = xn(sub2ind(arma::size(nx, nz, num_of_eq), i, j-1, k));
      }
    }
  }

  // default B.C.
  #pragma omp parallel for collapse(2)
  for (uword i=0; i<nx; i++){
    for (uword k=0; k<num_of_eq; k++){
      xn_jm1(sub2ind(arma::size(nx, nz, num_of_eq), i, 0, k))
        = xn(sub2ind(arma::size(nx, nz, num_of_eq), i, 0, k));
    }
  }

  return xn_jm1;
}

Vec chromosphere::flux_lim(const Vec &r) {
  Vec one(arma::size(r), fill::ones);
  // ospre
  /* return (1.5*(r%r)+r)/((r%r)+r+one); */
  // minmod
  Vec zero(arma::size(r), fill::zeros);
  return arma::max(zero, arma::min(one, r));
}

Vec chromosphere::rhs_explicit(const Vec &xn) {
  Vec xn_rhs(size(xn));

  Vec xn_ip1 = ip1(xn);
  Vec xn_im1 = im1(xn);
  /* Vec xn_jp1 = jp1(xn); */
  /* Vec xn_jm1 = jm1(xn); */

  // TODO: Boundary conditions.

  Vec xn_ip2 = ip1(xn_ip1);
  Vec xn_im2 = im1(xn_im1);
  /* Vec xn_jp2 = jp1(xn_jp1); */
  /* Vec xn_jm2 = jm1(xn_jm1); */

  const Vec dxn_iph = xn_ip1-xn;
  const Vec dxn_imh = xn-xn_im1;
  /* const Vec dxn_jph = xn_jp1-xn; */
  /* const Vec dxn_jmh = xn-xn_jm1; */

cout << "dxn_iph has nan: " << dxn_iph.has_nan() << endl
     << "dxn_imh has nan: " << dxn_imh.has_nan() << endl
;

  // limiter inputs
  Vec r_i = dxn_imh / dxn_iph;
  Vec r_ip1 = dxn_iph / (xn_ip2-xn_ip1);
  Vec r_im1 = (xn_im1-xn_im2) / dxn_imh;
  /* const Vec r_j = dxn_jmh / dxn_jph; */
  /* const Vec r_jp1 = dxn_jph / (xn_jp2-xn_jp1); */
  /* const Vec r_jm1 = (xn_jm1-xn_jm2) / dxn_jmh; */

  r_i.replace(datum::nan, 1);
  r_ip1.replace(datum::nan, 1);
  r_im1.replace(datum::nan, 1);

  // extrapolated cell edge variables
  const Vec Lxn_iph = xn     + 0.5*flux_lim(r_i)%dxn_imh;
  const Vec Lxn_imh = xn_im1 - 0.5*flux_lim(r_im1)%dxn_imh;
  const Vec Rxn_iph = xn_ip1 + 0.5*flux_lim(r_ip1)%dxn_iph;
  const Vec Rxn_imh = xn     - 0.5*flux_lim(r_i)%dxn_iph;
  /* const Vec Lxn_jph = xn     + 0.5*flux_lim(r_j)%dxn_jmh; */
  /* const Vec Lxn_jmh = xn_jm1 - 0.5*flux_lim(r_jm1)%dxn_jmh; */
  /* const Vec Rxn_jph = xn_jp1 + 0.5*flux_lim(r_jp1)%dxn_jph; */
  /* const Vec Rxn_jmh = xn     - 0.5*flux_lim(r_j)%dxn_jph; */


  cout << "Lxn_iph has nan: " << Lxn_iph.has_nan() << endl
       << "Rxn_iph has nan: " << Rxn_iph.has_nan() << endl
       << "Lxn_iph(nn) zeros: " << endl;
  find(get_scalar(Lxn_iph, TN) == 0.0).print();
  cout << "Fastspeed(Lxn_iph): " << fast_speed_x(Lxn_iph).has_nan() << endl
       << "Fastspeed(Rxn_iph): " << fast_speed_x(Rxn_iph).has_nan() << endl
  ;

  // TODO: Properly find max eigenvalues
  const Vec a_imh = arma::abs(0.5*(fast_speed_x(Lxn_imh)+fast_speed_x(Rxn_imh)));
  const Vec a_iph = arma::abs(0.5*(fast_speed_x(Lxn_iph)+fast_speed_x(Rxn_iph)));

  const Vec Fximh = 0.5*(Fx(Lxn_imh)+Fx(Rxn_imh)-a_imh%(Rxn_imh-Lxn_imh));
  const Vec Fxiph = 0.5*(Fx(Lxn_iph)+Fx(Rxn_iph)-a_iph%(Rxn_iph-Lxn_iph));

  cout << "a_imh has nan: " << a_imh.has_nan() << endl
       << "a_iph has nan: " << a_iph.has_nan() << endl
       << "Fximh has nan: " << Fximh.has_nan() << endl
       << "Fxiph has nan: " << Fxiph.has_nan() << endl
  ;

  xn_rhs = -1.0*(Fxiph - Fximh)/dx;

  return xn_rhs;
}

Vec chromosphere::Fx(const Vec& xn){
  Vec F(arma::size(xn));

  const Vec bx = get_scalar(xn, BX);
  const Vec bz = get_scalar(xn, BZ);
  const Vec bp = get_scalar(xn, BP);
  const Vec ni = get_scalar(xn, NI);
  const Vec nn = get_scalar(xn, NN);
  const Vec Ti = get_scalar(xn, TI);
  const Vec Tn = get_scalar(xn, TN);
  const Vec b2 = bx%bx + bz%bz + bp%bp;
  const Vec pi = Ti/ni;
  const Vec pn = Tn/nn;

  const Vec nvx = get_scalar(xn, VX);
  const Vec nvz = get_scalar(xn, VZ);
  const Vec nvp = get_scalar(xn, VP);
  const Vec nux = get_scalar(xn, UX);
  const Vec vx = nvx/ni;
  const Vec vz = nvz/ni;
  const Vec vp = nvp/ni;
  const Vec ux = nux/nn;

  // local speeds
  F = scalar_to(vx%bx, BX);
  F += scalar_to(vx%bz, BZ);
  F += scalar_to(vx%bp, BP);
  F += scalar_to(nvx, NI);
  F += scalar_to(nux, NN);
  F += scalar_to(nvx, VX);
  F += scalar_to(nvx, VZ);
  F += scalar_to(nvx, VP);
  F += scalar_to(vx%Ti, TI);
  F += scalar_to(nux, UX);
  F += scalar_to(nux, UZ);
  F += scalar_to(nux, UP);
  F += scalar_to(ux%Tn, TN);
  // Pressure
  F += scalar_to(pi, VX);
  F += scalar_to(pn, UX);
  F += scalar_to(0.5*b2, UX);
  // magnetic tension
  F -= scalar_to(bx%bx, VX);
  F -= scalar_to(bz%bx, VZ);
  F -= scalar_to(bp%bx, VP);
  // induction
  F -= scalar_to(vx%bx, VX);
  F -= scalar_to(vz%bx, VZ);
  F -= scalar_to(vp%bx, VP);

  F += scalar_to(bx, GLM); //temporary

  return F;
}

Vec chromosphere::get_scalar(const Vec& xn, const uword& index){
  Vec scalar(nx*nz);

  #pragma omp parallel for collapse(2)
  for (uword j=0; j<nz; j++){
    for (uword i=0; i<nx; i++) {
      scalar(sub2ind(arma::size(nx, nz), i, j))
        = xn(sub2ind(arma::size(nx, nz, num_of_eq), i, j, index));
    }
  }

  return scalar;
}


Vec chromosphere::scalar_to(const Vec& scalar, const uword& index){
  Vec xn = zeros<Vec>(nx*nz*num_of_eq);

  #pragma omp parallel for collapse(2)
  for (uword j=0; j<nz; j++){
    for (uword i=0; i<nx; i++) {
        xn(sub2ind(arma::size(nx, nz, num_of_eq), i, j, index))
          = scalar(sub2ind(arma::size(nx, nz), i, j));
    }
  }

  return xn;
}

Vec chromosphere::fast_speed_x(const Vec& xn){

  // Get scalars
  const Vec bx = get_scalar(xn, BX);
  const Vec bz = get_scalar(xn, BZ);
  const Vec bp = get_scalar(xn, BP);
  const Vec ni = get_scalar(xn, NI);
  const Vec nn = get_scalar(xn, NN);
  const Vec Ti = get_scalar(xn, TI);
  const Vec Tn = get_scalar(xn, TN);
  const Vec b2 = bx%bx + bz%bz + bp%bp;
  const Vec pi = Ti/ni;
  const Vec pn = Tn/nn;

  // characteristic speeds
  const Vec cps2 = gammamono*(pi/ni);
  const Vec cps = arma::sqrt(cps2);
  const Vec cns2 = gammamono*(pn/nn);
  const Vec cns = arma::sqrt(cns2);
  const Vec ca2 = b2/ni;
  const Vec cax = bx/arma::sqrt(ni);
  const Vec cpsca = cps%cax;
  const Vec cpsca2 = cpsca%cpsca;

  // proton (ion) fast speed
  /* const Vec cp = 0.5*(arma::sqrt(2.0*(cps2 + ca2 + arma::sqrt((cps2+ca2)%(cps2+ca2)-4.0*cpsca2)))); */
  const Vec cp = cps;

  return scalar_to(cp, NI)
       + scalar_to(cns, NN)
       + scalar_to(cp, VX)
       + scalar_to(cp, VZ)
       + scalar_to(cp, VP)
       + scalar_to(cns, UX)
       + scalar_to(cns, UZ)
       + scalar_to(cns, UP)
       + scalar_to(cp, TI)
       + scalar_to(cns, TN)
       + scalar_to(cp, BX)
       + scalar_to(cp, BZ)
       + scalar_to(cp, BP)
       + scalar_to(cp, GLM)
       ;
}

extern "C" void chromo_state_() {
  cout << "Giving chromosphere state." << endl;
}
