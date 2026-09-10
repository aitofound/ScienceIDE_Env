/*
 * parker.cpp
 *
 *  Created on: May 16, 2020
 *      Author: vtenishe
 */


#include "sep.h"


#include <iostream>
#include <cmath>

// Function to calculate the Parker spiral magnetic field in 3D, using arrays
void calculateIMFParkerSpiral3D(double* x, double u_sw, double* B) {
    // x is the 3D position array (x[0], x[1], x[2]) in meters
    // u_sw is the solar wind speed in m/s
    // B is the output 3D magnetic field array (B[0], B[1], B[2]) in Tesla


    // Constants
    const double B0 = 5e-9;           // Magnetic field at 1 AU in Tesla (5 nT)
    const double r0 = 1.496e11;       // 1 AU in meters
    const double omega_sun = 2.865e-6; // Angular rotation speed of the Sun in rad/s


    // Calculate the heliocentric distance r
    double r = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);

    // Calculate r_sw (the critical radius where the solar wind affects the magnetic field)
    double r_sw = u_sw / omega_sun;

    // Calculate the radial component of the magnetic field
    double B_r = B0 * pow(r0 / r, 2);

    // Calculate the azimuthal component of the magnetic field
    double B_phi = B_r * (r / r_sw);

    // Convert the magnetic field to Cartesian coordinates and store in B array
    B[0] = B_r * (x[0] / r) - B_phi * (x[1] / r); // Bx
    B[1] = B_r * (x[1] / r) + B_phi * (x[0] / r); // By
    B[2] = B_r * (x[2] / r);                      // Bz
}


void SEP::ParkerSpiral::GetB(double* B,double *x,double u_sw) {
  double B0=5.0E-9,R0=_AU_;
  double omega=2*Pi/(25.4*24*3600.0);


  calculateIMFParkerSpiral3D(x,u_sw,B);
  return;


  double e_r[3],e_phi[3],e_z[3]={0.0,0.0,1.0};
  int idim;

  for (idim=0;idim<3;idim++) e_r[idim]=x[idim];

  Vector3D::CrossProduct(e_phi,e_r,e_z);

  Vector3D::Normalize(e_r);
  Vector3D::Normalize(e_phi);


  double aa,bb,r,theta;


  r=Vector3D::Length(x);
  aa=B0*pow(R0/r,2);

  theta=acos(Vector3D::DotProduct(e_z,e_r));
  bb=aa*(r-R0)*omega*sin(theta)/u_sw;

  for (idim=0;idim<3;idim++) B[idim]=aa*e_r[idim]-bb*e_phi[idim];
}


void SEP::ParkerSpiral::CreateFileLine(list<SEP::cFieldLine> *field_line,double *xstart,double length_rsun) {
  double l[3],dl;
  int idim;
  SEP::cFieldLine p;

  const int npoints=4000;

  double u_sw=400.0E3;

  dl=length_rsun*_RADIUS_(_SUN_)/npoints;

  for (idim=0;idim<3;idim++) {
    p.x[idim]=xstart[idim]*_RADIUS_(_SUN_);
  }

  GetB(p.B,p.x,u_sw);
  field_line->push_back(p);


  for (int ipoint=0;ipoint<npoints;ipoint++) {
    GetB(l,p.x,u_sw);
    Vector3D::Normalize(l,dl);

    for (idim=0;idim<3;idim++) p.x[idim]+=l[idim];

    GetB(p.B,p.x,u_sw);
    field_line->push_back(p);
  }
}

void SEP::ParkerSpiral::CreateStraitFileLine(list<SEP::cFieldLine> *field_line,double *xstart,double length_rsun) {
  double l[3]={1.0,0.0,0.0},dl;
  int idim;
  SEP::cFieldLine p;

  const int npoints=4000;

  double u_sw=400.0E3;

  dl=length_rsun*_RADIUS_(_SUN_)/npoints;

  for (idim=0;idim<3;idim++) {
    p.x[idim]=xstart[idim]*_RADIUS_(_SUN_);
    l[idim]=xstart[idim];
  }

  GetB(p.B,p.x,u_sw);
  field_line->push_back(p);

  Vector3D::Normalize(l,dl);

  for (int ipoint=0;ipoint<npoints;ipoint++) {
    for (idim=0;idim<3;idim++) p.x[idim]+=l[idim];

    field_line->push_back(p);
  }
}

//init the magnetic field in teh entire domain with Parker spiral 
void SEP::ParkerSpiral::InitDomain(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  if (startNode==NULL) startNode=PIC::Mesh::mesh->rootTree;

  if (startNode->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    PIC::Mesh::cDataBlockAMR *block;

    if ((block=startNode->block)!=NULL) {
      double B[3],x[3];
      int idim,i,j,k,LocalCellNumber;
      PIC::Mesh::cDataCenterNode *cell;
      double *data;

      for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);

        if ((cell=block->GetCenterNode(LocalCellNumber))!=NULL) {
          cell->GetX(x);
          SEP::ParkerSpiral::GetB(B,x);

          if (cell->Measure==0.0) {
            PIC::Mesh::mesh->InitCellMeasureBlock(startNode);

            if (cell->Measure==0.0) {
              PIC::Mesh::mesh->CenterNodes.deleteElement(cell);
              startNode->block->SetCenterNode(NULL,LocalCellNumber);
              continue;
            }
          }

          if (_PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_) {
            data=(double*)(cell->GetAssociatedDataBufferPointer()+PIC::CPLR::SWMF::MagneticFieldOffset);
	  } else {
            data=(double*)(cell->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset);
	  }

          for (idim=0;idim<3;idim++) data[idim]=B[idim];
        }
      }
    }
  }
  else {
    int iDownNode;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *downNode;

    for (iDownNode=0;iDownNode<(1<<DIM);iDownNode++) if ((downNode=startNode->downNode[iDownNode])!=NULL) {
      InitDomain(downNode);
    }
  }
}

