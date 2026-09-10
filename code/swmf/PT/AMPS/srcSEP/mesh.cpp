/*
 * mesh.cpp
 *
 *  Created on: May 16, 2020
 *      Author: vtenishe
 */





#include "sep.h"


double** SEP::Mesh::FieldLineTable=NULL;
int SEP::Mesh::FieldLineTableLength=0;

int SEP::ParticleTrajectoryCalculation=SEP::ParticleTrajectoryCalculation_RelativisticBoris;
int SEP::DomainType=DomainType_ParkerSpiral;
int SEP::Domain_nTotalParkerSpirals=1;


double SEP::Mesh::localSphericalSurfaceResolution(double *x) {
  double res,r,l[3];
  int idim;
  double SubsolarAngle;

  if (_MODEL_CASE_==_MODEL_CASE_SEP_TRANSPORT_) {
    res=(_PIC_FIELD_LINE_MODE_!=_PIC_MODE_ON_) ? 0.1 : 10.0; 
  }
  else {
    res=2.0;
  }

  return res*_RADIUS_(_SUN_);
}

double SEP::Mesh::localResolution(double *x) {
  double res=20.0*_RADIUS_(_SUN_); 

  if (_PIC_FIELD_LINE_MODE_!=_PIC_MODE_OFF_) {
    if (Vector3D::Length(x)<4.0*_RADIUS_(_SUN_)) return 0.2*_RADIUS_(_SUN_);
  }

  if (_DOMAIN_GEOMETRY_==_DOMAIN_GEOMETRY_BOX_) {
    res=max(20.0*_RADIUS_(_SUN_),0.1*Vector3D::Length(x));
  } 

  if (Vector3D::DotProduct(x,x)<2.0*_RADIUS_(_SUN_)*_RADIUS_(_SUN_)) {
    res=0.1*_RADIUS_(_SUN_);
  }
  
  return res;
}


void SEP::Mesh::ImportFieldLine(list<SEP::cFieldLine> *field_line) {
  int i,ip,idim;
  list<SEP::cFieldLine>::iterator it;
  FILE *fLine;

  double **NewFieldLineTable; 
  int NewFieldLineTableLength=FieldLineTableLength+field_line->size();


  NewFieldLineTable=new double *[NewFieldLineTableLength];
  NewFieldLineTable[0]=new double [3*NewFieldLineTableLength];

  for (i=0;i<NewFieldLineTableLength;i++) NewFieldLineTable[i]=NewFieldLineTable[0]+3*i;

  for (ip=0;ip<FieldLineTableLength;ip++) for (idim=0;idim<3;idim++) NewFieldLineTable[ip][idim]=FieldLineTable[ip][idim]; 

  for (it=field_line->begin();it!=field_line->end();it++,ip++) {
    for (idim=0;idim<3;idim++) NewFieldLineTable[ip][idim]=it->x[idim];
  }

  if (FieldLineTable!=NULL) {
    delete [] FieldLineTable[0];
    delete [] FieldLineTable;
  }

  FieldLineTable=NewFieldLineTable;
  FieldLineTableLength=NewFieldLineTableLength;
}

void SEP::Mesh::PrintFieldLine(list<SEP::cFieldLine> *field_line,const char *fname) {
  list<SEP::cFieldLine>::iterator it;
  FILE *fLine;

  if (PIC::ThisThread==0) {
    fLine=fopen(fname,"w");
    fprintf(fLine,"VARIABLES=\"x\",\"y\",\"z\"\nZONE T=\"Magnetic Field Line\", I=%i\n",field_line->size());

    for (it=field_line->begin();it!=field_line->end();it++) {
      fprintf(fLine,"%e %e %e\n",it->x[0],it->x[1],it->x[2]);
    }

    fclose(fLine);
  }
}



void SEP::Mesh::LoadFieldLine_flampa(list<SEP::cFieldLine> *field_line,const char *fname) {
   SEP::cFieldLine p;
   CiFileOperations ifile;
   char str[10000],dat[10000],*endptr;

   ifile.openfile(fname);

   while (ifile.eof()==false) {
     if (ifile.GetInputStr(str,sizeof(str))==true) {
       ifile.CutInputStr(dat,str);

       for (int idim=0;idim<3;idim++) {
         ifile.CutInputStr(dat,str);
         p.x[idim]=strtod(dat,&endptr)*_RADIUS_(_SUN_);
       }

       ifile.CutInputStr(dat,str);
       ifile.CutInputStr(dat,str);

       for (int idim=0;idim<3;idim++) {
         ifile.CutInputStr(dat,str);
         p.U[idim]=strtod(dat,&endptr);
       }

       for (int idim=0;idim<3;idim++) {
         ifile.CutInputStr(dat,str);
         p.B[idim]=strtod(dat,&endptr);
       }

       for (int i=0;i<2;i++) {
         ifile.CutInputStr(dat,str);
         p.Wave[i]=strtod(dat,&endptr);
       }

       field_line->push_back(p);
     }
   }

   ifile.closefile();
}

bool SEP::Mesh::NodeSplitCriterion(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double *xmin=startNode->xmin;
  double *xmax=startNode->xmax;
  int i;
  double d2;


  if (startNode->RefinmentLevel>=7) return false;


  

  bool field_line_intersect_node=false;

  double d2min=pow(4.0*_RADIUS_(_SUN_),2);
  double cell_res2,cell_res; ////=0.1+(0.5-0.1)/(200-1.2)*(sqrt(pow(0.5*(xmin[0]+xmax[0]),2)+pow(0.5*(xmin[1]+xmax[1]),2)+pow(0.5*(xmin[2]+xmax[2]),2))/_RADIUS_(_SUN_)-1.2);

  double r=sqrt(pow(0.5*(xmin[0]+xmax[0]),2)+pow(0.5*(xmin[1]+xmax[1]),2)+pow(0.5*(xmin[2]+xmax[2]),2))/_RADIUS_(_SUN_);

  if (r<1.1) return false;

  cell_res=(r>1.1) ? 0.15*exp(log(0.5/0.1)/log(200.0/1.2)*log(r/1.2)) : 0.1;


  if (cell_res<0.1) cell_res=0.1;
  if (cell_res>0.8) cell_res=0.8;

  cell_res2=pow(cell_res*_RADIUS_(_SUN_),2);

  for (i=0;i<FieldLineTableLength;i++) {
    field_line_intersect_node=true;
    d2=0.0;

    for (int idim=0;idim<3;idim++) {
      if ((FieldLineTable[i][idim]<xmin[idim]) || (FieldLineTable[i][idim]>xmax[idim])) field_line_intersect_node=false;

      double t; /////....=0.5*(xmin[idim]+xmax[idim])-FieldLineTable[i][idim];

      t=min(fabs(xmin[idim]-FieldLineTable[i][idim]),fabs(xmax[idim]-FieldLineTable[i][idim]));

      d2+=t*t;
    }

    if ((d2<d2min)||(field_line_intersect_node==true)) {
      //the block is close to the field line
      double t0,t1,t2,l2;

      t0=(xmax[0]-xmin[0])/_BLOCK_CELLS_X_;
      t1=(xmax[1]-xmin[1])/_BLOCK_CELLS_Y_;
      t2=(xmax[2]-xmin[2])/_BLOCK_CELLS_Z_;

      l2=t0*t0+t1*t1+t2*t2;

      if (l2>cell_res2) {
        //the block needs to be refined
        return true;
      }
    }
  }

  return false;
}


double SEP::Mesh::localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  double res,CellSize,CharacteristicSpeed=40000E3;

  CellSize=startNode->GetCharacteristicCellSize();
  res=0.3*CellSize/CharacteristicSpeed;

  if (res>1.0) res=1.0;

//res=10.0;

  return res/1.0;
}

//init the magnetic filed line in AMPS
void SEP::Mesh::InitFieldLineAMPS(list<SEP::cFieldLine> *field_line) {
  int idim,ip;
  list<SEP::cFieldLine>::iterator it;

  namespace FL=PIC::FieldLine;

  if(FL::nFieldLine == FL::nFieldLineMax) exit(__LINE__,__FILE__,"ERROR: reached limit for field line number");

  for (ip=0,it=field_line->begin();it!=field_line->end();it++,ip++) {
    //add vertex to the spiral
    FL::FieldLinesAll[FL::nFieldLine].Add(it->x);

    FL::cFieldLineVertex *Vertex=FL::FieldLinesAll[FL::nFieldLine].GetVertex(ip);

    Vertex->SetMagneticField(it->B);
    Vertex->SetPlasmaVelocity(it->U);

    //set the background plasma velocity
    const double speed=400.0E3;
    double v[3]; 

    Vertex->GetX(v);
    Vector3D::Normalize(v,speed);
    Vertex->SetPlasmaVelocity(v);

    if (FL::DatumAtVertexPlasmaWaves.offset>=0) Vertex->SetDatum(FL::DatumAtVertexPlasmaWaves,it->Wave);
  }
  
  FL::nFieldLine++;
}
