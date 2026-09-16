


#include <list>
#include <algorithm> 

#include "pic.h"


int PIC::ParticleSplitting::particle_num_limit_min=100;
int PIC::ParticleSplitting::particle_num_limit_max=150;
double PIC::ParticleSplitting::v_shift_max=0.01;
double PIC::ParticleSplitting::x_shift_max=0.2;
bool PIC::ParticleSplitting::apply_non_uniform_x_shift=false;
int PIC::ParticleSplitting::Mode=PIC::ParticleSplitting::_disactivated;

//functions for splitting/merging particles 
void PIC::ParticleSplitting::Split::SplitWithVelocityShift(int particle_num_limit_min,int particle_num_limit_max) {
  int inode,i,j,k,i0,j0,k0,di,dj,dk,nParticles;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  long int p;

  auto GetParticleNumber = [&] (int *ParticleNumberTable,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
    long int p;
    int spec;

    for (spec=0;spec<PIC::nTotalSpecies;spec++) ParticleNumberTable[spec]=0;

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      ParticleNumberTable[PIC::ParticleBuffer::GetI(p)]++;
      p=PIC::ParticleBuffer::GetNext(p);
    }
  };


  class cParticleListElement {
  public:
    long int ptr;
    double w;
  };


  class cParticleId {
  public:
    long int ptr;
    int nd;
  };

  class cParticleDescriptor {
  public:
    long int p;
    double w;
  };

  vector<cParticleListElement>  ParticleList;

  //reduce the number of model particles 

  auto GetVelocityRange = [&] (int spec,double *vmin,double *vmax,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node)  {
    long int p;
    int res=0,idim;
    bool initflag=false;
    double v[3];

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {

        res++;
        PIC::ParticleBuffer::GetV(v,p);

        if (initflag==false) {
          for (idim=0;idim<3;idim++) vmin[idim]=v[idim],vmax[idim]=v[idim];

          initflag=true;
        }
        else {
          for (idim=0;idim<3;idim++) {
            if (vmin[idim]>v[idim]) vmin[idim]=v[idim];
            if (vmax[idim]<v[idim]) vmax[idim]=v[idim]; 
          }
        }
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    return res;
  };


  class cVelocitySpaceElement {
  public:
    int iv,jv,kv;
    vector<long int> p;
  }; 

  cVelocitySpaceElement **VelocitySpaceTable=NULL;
  const int nSerachLevels=6;

  int nVelCells1d=1<<nSerachLevels;
  int nVelCells=nVelCells1d*nVelCells1d*nVelCells1d;


  //create the initial velovity space table
  auto InitVelocitySpaceTable = [&] (int spec,double *vmin,double *dv,int i, int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node)  {
    long int p;
    int idim,iv[3],iVelCell;
    double v[3];
    bool foundflag;

    VelocitySpaceTable=new cVelocitySpaceElement* [nVelCells];
    for (int i=0;i<nVelCells;i++) VelocitySpaceTable[i]=NULL;

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {
        PIC::ParticleBuffer::GetV(v,p);

        for (idim=0;idim<3;idim++) iv[idim]=(int)((v[idim]-vmin[idim])/dv[idim]); 

        iVelCell=iv[0]+nVelCells1d*(iv[1]+nVelCells1d*iv[2]);  

        if (VelocitySpaceTable[iVelCell]==NULL) {
          VelocitySpaceTable[iVelCell]=new cVelocitySpaceElement [1];

          VelocitySpaceTable[iVelCell]->iv=iv[0];
          VelocitySpaceTable[iVelCell]->jv=iv[1];
          VelocitySpaceTable[iVelCell]->kv=iv[2];
        } 

        VelocitySpaceTable[iVelCell]->p.push_back(p);
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }
  }; 

  auto RemoveOneParticle= [&] (int nCurrentResolutionLevel,int nMaxParticlesReduce,int i, int j, int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,bool& more_then_one_particle_left) { 
    int cnt=0,npart;
    long int p;
    double w_remove;

    more_then_one_particle_left=false;

    if (nMaxParticlesReduce==0) return cnt;


    int nCells1d=1<<nCurrentResolutionLevel;
    int nCells=nCells1d*nCells1d*nCells1d;


    for (int ii=0;ii<nCells;ii++) if (VelocitySpaceTable[ii]!=NULL) if ((npart=VelocitySpaceTable[ii]->p.size())>1) { 
      double w_tot=0.0,w,wmin;
      auto itmin=VelocitySpaceTable[ii]->p.begin(); 


      //find particle with smallest weight
      p=-1,wmin=-1.0;
    
  
      for (auto it=VelocitySpaceTable[ii]->p.begin();it!=VelocitySpaceTable[ii]->p.end();it++) {
        w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(*it);

        if ((wmin<0.0)||(w<wmin)) p=*it,wmin=w,itmin=it;
      }


/*
      for (auto& t : VelocitySpaceTable[ii]->p) {
        w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(t); 

        if ((wmin<0.0)||(w<wmin)) p=t,wmin=w;
      }
*/

      //find particles that closest to 'p' in the velocity space 
      long int p1;
      double d2,d2min=-1.0;
      double v1[3],v2[3];

      PIC::ParticleBuffer::GetV(v1,p);

      for (auto& t : VelocitySpaceTable[ii]->p) if (t!=p) {
        PIC::ParticleBuffer::GetV(v2,t);
        d2=0.0;
        
        for (int idim=0;idim<3;idim++) {
          double tt=v1[idim]-v2[idim];

          d2+=tt*tt;
        }

        if ((d2min<0.0)||(d2<d2min)) d2min=d2,p1=t;
      } 
 
      //merge particle p1 and p: weight of 'p' is added to 'p1'
      w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p1); 

      PIC::ParticleBuffer::GetV(v1,p1);
      PIC::ParticleBuffer::GetV(v2,p);

      for (int idim=0;idim<3;idim++) v1[idim]=(w*v1[idim]+wmin*v2[idim])/(w+wmin);

      PIC::ParticleBuffer::SetV(v1,p1);  
      PIC::ParticleBuffer::SetIndividualStatWeightCorrection(w+wmin,p1);
      
      //delete particles
      PIC::ParticleBuffer::DeleteParticle(p,node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);
      VelocitySpaceTable[ii]->p.erase(itmin);

      if (npart-1>1) more_then_one_particle_left=true;


/*
      p=VelocitySpaceTable[ii]->p[0];
      w_remove=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);

      //delete particles
      PIC::ParticleBuffer::DeleteParticle(p,node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);
      VelocitySpaceTable[ii]->p.erase(VelocitySpaceTable[ii]->p.begin());

      if (npart-1>1) more_then_one_particle_left=true;

      //distribute the weights
      for (auto& t : VelocitySpaceTable[ii]->p) w_tot+=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(t); 

      for (auto& t : VelocitySpaceTable[ii]->p) {
        w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(t);

        w=w*(1.0+w_remove/w_tot);
        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(w,t);
      } 
*/

      if (++cnt==nMaxParticlesReduce) break;
    }

    return cnt;
  }; 

  auto RebuildVelocitySpaceTable = [&] (int CurrentResolutionLevel) {
    cVelocitySpaceElement **NewVelocitySpaceTable;
    int iVelCell,iNewVelCell,iv,jv,kv;


    int NewResolutionLevel=CurrentResolutionLevel-1;

    int nVelCells1d=1<<CurrentResolutionLevel;
    int nNewVelCells1d=nVelCells1d/2;

    int nVelCells=nVelCells1d*nVelCells1d*nVelCells1d;
    int nNewVelCells=nNewVelCells1d*nNewVelCells1d*nNewVelCells1d;


    NewVelocitySpaceTable=new cVelocitySpaceElement* [nNewVelCells];
    for (int i=0;i<nNewVelCells;i++) NewVelocitySpaceTable[i]=NULL;

    for (int i=0;i<nVelCells;i++) {
      if (VelocitySpaceTable[i]!=NULL) {
        iv=VelocitySpaceTable[i]->iv;
        jv=VelocitySpaceTable[i]->jv;
        kv=VelocitySpaceTable[i]->kv;

        iVelCell=iv+nVelCells1d*(jv+nVelCells1d*kv);  

        iv/=2,jv/=2,kv/=2;
        iNewVelCell=iv+nNewVelCells1d*(jv+nNewVelCells1d*kv);

        if (NewVelocitySpaceTable[iNewVelCell]==NULL) {
          NewVelocitySpaceTable[iNewVelCell]=new cVelocitySpaceElement [1];

          *NewVelocitySpaceTable[iNewVelCell]=*VelocitySpaceTable[i];

          NewVelocitySpaceTable[iNewVelCell]->iv/=2;
          NewVelocitySpaceTable[iNewVelCell]->jv/=2;
          NewVelocitySpaceTable[iNewVelCell]->kv/=2;

        }
        else {
          NewVelocitySpaceTable[iNewVelCell]->p.insert(NewVelocitySpaceTable[iNewVelCell]->p.begin(),
              VelocitySpaceTable[i]->p.begin(),VelocitySpaceTable[i]->p.end());
        }
      }
    }

    //delete the previous table 
    for (int i=0;i<nVelCells;i++) if (VelocitySpaceTable[i]!=NULL) delete [] VelocitySpaceTable[i];

    delete [] VelocitySpaceTable;
    VelocitySpaceTable=NewVelocitySpaceTable;
  }; 


  auto DeleteVelocitySpaceTable = [&] (int CurrentResolutionLevel) {
    int nVelCells1d=1<<CurrentResolutionLevel;
    int nVelCells=nVelCells1d*nVelCells1d*nVelCells1d;

    for (int i=0;i<nVelCells;i++) if (VelocitySpaceTable[i]!=NULL) delete [] VelocitySpaceTable[i];

    delete [] VelocitySpaceTable;
  };

  auto ReduceParticleNumber1 = [&] (int spec,int nModelParticles,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,int nRequestedParticles)  {
    double vmin[3],vmax[3],dv[3]; 
    bool more_then_one_particle_left;


if (false) {
    class cParticleListElement {
    public:
      long int p;
      double w;
    }; 

    list<cParticleListElement> p_list;
    long int p;

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {
        cParticleListElement t;

        t.p=p;
        t.w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);

        p_list.push_back(t);
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    while ((nModelParticles>nRequestedParticles)&&(p_list.size()!=0)) {
      //1. find a particle with a smallest weight
      list<cParticleListElement>::iterator p_wmin_it=p_list.begin();
      list<cParticleListElement>::iterator it;
      double wmin=p_list.begin()->w;

      for (it=p_list.begin();it!=p_list.end();it++) { 
        if (wmin>it->w) {
          wmin=it->w;
          p_wmin_it=it;
        } 
      }

      //2. find a particle that is closest to p_wmin_it in the velocity space 
      double r2,r2min=-1.0;
      list<cParticleListElement>::iterator p_r2min_it;
      double d,v0[3],v1[3];
      int idim;

      PIC::ParticleBuffer::GetV(v0,p_wmin_it->p);   

      for (it=p_list.begin();it!=p_list.end();it++) if (it!=p_wmin_it) {
        PIC::ParticleBuffer::GetV(v1,it->p);
        r2=0.0;

        for (idim=0;idim<3;idim++) {
          d=v0[idim]-v1[idim]; 
          r2+=d*d;
        }

        if ((r2min<0.0)||(r2min>r2)) {
          r2min=r2,p_r2min_it=it;
        }
      }

      //merge particles if the velocity difference to not too large 
      if ((r2min>0.0)&&(r2min<0.0000001*Vector3D::DotProduct(v0,v0))) {
        double w0,w1;

        PIC::ParticleBuffer::GetV(v1,p_r2min_it->p); 

        w0=p_wmin_it->w;
        w1=p_r2min_it->w;

        for (idim=0;idim<3;idim++) v1[idim]=(w0*v0[idim]+w1*v1[idim])/(w0+w1); 

        PIC::ParticleBuffer::SetV(v1,p_r2min_it->p);
        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(w0+w1,p_r2min_it->p); 

        p_r2min_it->w=w0+w1;

        PIC::ParticleBuffer::DeleteParticle(p_wmin_it->p,node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);
        nModelParticles--;
      }

      p_list.erase(p_wmin_it); 
    }

   return;
}

    if (nModelParticles>nRequestedParticles) {
      int nDeleteParticles=nModelParticles-nRequestedParticles;  
      int nRemovedParticles=0;


static int ncall=0;
ncall++;



if (ncall==10792) {
  cout << "sfpsodfposiapfoipo"<< endl;
}




      GetVelocityRange(spec,vmin,vmax,i,j,k,node);

      for (int idim=0;idim<3;idim++) {
        double d=vmax[idim]-vmin[idim]; 

        vmin[idim]-=0.05*d; 
        vmax[idim]+=0.05*d;

        dv[idim]=(vmax[idim]-vmin[idim])/nVelCells1d;
      }


      InitVelocitySpaceTable(spec,vmin,dv,i,j,k,node);

      int CurrentResolutionLevel=nSerachLevels;

      while (nRemovedParticles<nDeleteParticles) {
        nRemovedParticles+=RemoveOneParticle(CurrentResolutionLevel,nDeleteParticles-nRemovedParticles,i,j,k,node,more_then_one_particle_left); 

        if ((nRemovedParticles<nDeleteParticles)&&(more_then_one_particle_left==false)) {
          RebuildVelocitySpaceTable(CurrentResolutionLevel);
          --CurrentResolutionLevel;
        } 
      }

      DeleteVelocitySpaceTable(CurrentResolutionLevel);
    }
  };

  auto ReduceParticleNumber = [&] (int spec,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,int nRequestedParticles)  {
    vector<cParticleDescriptor> ParticleList;  
    cParticleDescriptor t;
    double *v,w_max=0.0,w_min=-1.0,SummedWeight=0.0,w;
    int nModelParticles=0;
    long int p;

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) { 
        t.p=p;
        t.w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);
        ParticleList.push_back(t);

        SummedWeight+=t.w;
        nModelParticles++;

        if (w_max<t.w) w_max=t.w;
        if ((w_min<0.0)||(w_min>t.w)) w_min=t.w; 
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    //delete particles 
    int nDeleteParticles=nModelParticles-nRequestedParticles,ip; 
    double RemovedParticleWeight=0.0;
    int ii;

    for (ii=0;ii<nDeleteParticles;ii++) {
      const bool _search_continue=true;
      const bool _search_completed=false; 

      bool flag=_search_continue;
      int ip, cnt=0;

      while ((++cnt<1000000)&&(flag==_search_continue)) {
        ip=(int)(rnd()*nModelParticles);

        if (w_min/ParticleList[ip].w>rnd()) { // delete particles with smaller weight  
          RemovedParticleWeight+=ParticleList[ip].w;
          PIC::ParticleBuffer::DeleteParticle(ParticleList[ip].p,node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);

          ParticleList[ip]=ParticleList[nModelParticles-1];
          nModelParticles--;
          flag=_search_completed; 
        }
      } 
    }


    //distribute the removed weight among particles that left in the system 
    for (ii=0;ii<nModelParticles;ii++) {
      w=ParticleList[ii].w*(1.0+RemovedParticleWeight/(SummedWeight-RemovedParticleWeight));    
      p=ParticleList[ii].p;

      PIC::ParticleBuffer::SetIndividualStatWeightCorrection(w,p);
    }
  }; 


  //collect particles in the cell block 2x2x2
  auto CollectParticleBlock = [&] (int spec,int i0,int j0,int k0,list<cParticleId>& ParticleList) {
    cParticleId t;
    long int p;
    double ParticleWeight;

    ParticleList.clear(); 

    for (di=0;di<2;di++) for (dj=0;dj<2;dj++) for (dk=0;dk<2;dk++) { 
      i=i0+di;   
      j=j0+dj;
      k=k0+dk; 

      p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
      t.nd=_getCenterNodeLocalNumber(i,j,k);

      while (p!=-1) {
        if (PIC::ParticleBuffer::GetI(p)==spec) {
          t.ptr=p; 
          ParticleList.push_back(t);

          ParticleWeight=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);
          ParticleWeight/=8.0;
          PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ParticleWeight,p);
        }

        p=PIC::ParticleBuffer::GetNext(p);
      }
    }
  };

  auto IncreseParticleNumber = [&] (int spec,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
    double MeanV[3]={0.0,0.0,0.0},v[3],w;
    double MeanV2[3]={0.0,0.0,0.0};

    vector<cParticleDescriptor> ParticleList;
    cParticleDescriptor t;
    double w_max=0.0,SummedWeight=0.0,ThermalSpeed=0.0;
    int idim,nModelParticles=0;
    long int p;

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {
        t.p=p;
        t.w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);
        ParticleList.push_back(t); 

        SummedWeight+=t.w;
        if (w_max<t.w) w_max=t.w;

        PIC::ParticleBuffer::GetV(v,p); 

        for (int idim=0;idim<3;idim++) {
          MeanV[idim]+=t.w*v[idim];
          MeanV2[idim]+=t.w*v[idim]*v[idim];
        }

        nModelParticles++;
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    for (idim=0;idim<3;idim++) ThermalSpeed+=MeanV2[idim]/SummedWeight-MeanV[idim]*MeanV[idim]/(SummedWeight*SummedWeight); 

    ThermalSpeed=sqrt(fabs(ThermalSpeed));

    //add new particles in the system
    int nNewParticles=particle_num_limit_max-nModelParticles;
    int ip;
    long int pnew;
    double l[3],vnew[3],vold[3];
    bool flag;

    for (int ii=0;ii<nNewParticles;ii++) {
      flag=true;

      while (flag==true) {
        ip=(int)(rnd()*nModelParticles);

        int ip_wmax=-1;
        w_max=-1.0;
 
        for (int ii=0;ii<nModelParticles;ii++) {
          if (w_max<ParticleList[ii].w) w_max=ParticleList[ii].w,ip_wmax=ii;
        } 

        ip=ip_wmax;

        if (true) { // (ParticleList[ip].w/w_max>rnd()) { //split particles with larger weight  
          pnew=PIC::ParticleBuffer::GetNewParticle(node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]); 
          PIC::ParticleBuffer::CloneParticle(pnew,ParticleList[ip].p);

          //set new weight for both particles 
          ParticleList[ip].w/=2.0;
          t=ParticleList[ip];
          t.p=pnew;
          ParticleList.push_back(t);
          nModelParticles++;
          flag=false;

          PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ParticleList[ip].w,ParticleList[ip].p);
          PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ParticleList[ip].w,pnew);

          //perturb a new particle velocity;
          Vector3D::Distribution::Uniform(l,0.5*v_shift_max*ThermalSpeed); 

          //get velocity for the new particle 
          PIC::ParticleBuffer::GetV(vnew,pnew); 

          for (idim=0;idim<3;idim++) {
            vold[idim]=vnew[idim]-l[idim];
            vnew[idim]+=l[idim];
          }

          PIC::ParticleBuffer::SetV(vnew,pnew);
          PIC::ParticleBuffer::SetV(vold,ParticleList[ip].p);

          double dx[3],*xmin,*xmax,x[3],x0[3];

          xmin=node->xmin;
          

          dx[0]=(node->xmax[0]-xmin[0])/_BLOCK_CELLS_X_;
          dx[1]=(node->xmax[1]-xmin[1])/_BLOCK_CELLS_Y_;
          dx[2]=(node->xmax[2]-xmin[2])/_BLOCK_CELLS_Z_;




 



double r;
bool flag=true;
do {
  flag=true;


    r=pow(rnd(),1.0/3.0);

  
  if (apply_non_uniform_x_shift==true) if (r>rnd()) {
    flag=false;
    continue;
  }

  PIC::ParticleBuffer::GetX(x0,pnew);

  double ll[3];

  Vector3D::Distribution::Uniform(ll,x_shift_max*r*sqrt(dx[0]*dx[0]+dx[1]*dx[1]+dx[2]*dx[2]));

  x0[0]+=ll[0];
  if (x0[0]<xmin[0]+i*dx[0]) {flag=false;continue;}
  if (x0[0]>xmin[0]+(i+1)*dx[0]) {flag=false;continue;} 

  x0[1]+=ll[1];
  if (x0[1]<xmin[1]+j*dx[1]) {flag=false;continue;}
  if (x0[1]>xmin[1]+(j+1)*dx[1]) {flag=false;continue;}

  x0[2]+=ll[2];
  if (x0[2]<xmin[2]+k*dx[2]) {flag=false;continue;}
  if (x0[2]>xmin[2]+(k+1)*dx[2]) {flag=false;continue;}
}
while (flag==false);



PIC::ParticleBuffer::SetX(x0,pnew);



        }
      }
    }
  };


  //loop through the nodes
  int ParticleNumberTable[PIC::nTotalSpecies],spec; 

  for (inode=0;inode<PIC::DomainBlockDecomposition::nLocalBlocks;inode++) {
    node=PIC::DomainBlockDecomposition::BlockTable[inode];

    if (node->block!=NULL) {
      for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        GetParticleNumber(ParticleNumberTable,i,j,k,node);

        for (spec=0;spec<PIC::nTotalSpecies;spec++) {
          if (ParticleNumberTable[spec]>particle_num_limit_max) {
            ReduceParticleNumber1(spec,ParticleNumberTable[spec],i,j,k,node,(int)(particle_num_limit_min+0.9*(particle_num_limit_max-particle_num_limit_min)));
          }
          else if ((1<ParticleNumberTable[spec])&&(ParticleNumberTable[spec]<particle_num_limit_min)) {
            IncreseParticleNumber(spec,i,j,k,node);
          }
        }
      }
    }      
  }

//  PIC::Parallel::ExchangeParticleData();
}  


//=======================================================================================
void PIC::ParticleSplitting::Split::Scatter(int particle_num_limit_min,int particle_num_limit_max) {
  int inode,i,j,k,i0,j0,k0,di,dj,dk,nParticles;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  long int p;

  if ((_BLOCK_CELLS_X_%2!=0)||(_BLOCK_CELLS_Y_%2!=0)||(_BLOCK_CELLS_Z_%2!=0)) {
    exit(__LINE__,__FILE__,"Error: the number of cells in a block has to be even in each direction. Change the input file"); 
  }

  //get the number of model particle in the cell block 2x2x2 started from position i0,j0,k0
  auto GetParticleNumber = [] (int spec,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node)  {
    int ParticleNumber=0;
    long int p;

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (PIC::ParticleBuffer::GetI(p)==spec) ParticleNumber++;
      p=PIC::ParticleBuffer::GetNext(p);
    }

    return ParticleNumber;
  };

  auto GetBlockParticleNumber = [&] (int spec,int i0,int j0,int k0,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node)  {
    int di,dj,dk,i,j,k;
    int ParticleNumber=0;

    for (di=0;di<2;di++) for (dj=0;dj<2;dj++) for (dk=0;dk<2;dk++) { 
      i=i0+di;   
      j=j0+dj;
      k=k0+dk; 

      ParticleNumber+=GetParticleNumber(spec,i,j,k,node);
    }

    return ParticleNumber;
  };  

  class cParticleListElement {
  public:
    long int ptr;
    double w;
  };


  class cParticleId {
  public:
    long int ptr;
    int nd;
  };

  class cParticleDescriptor {
  public:
    long int p;
    double w;
  };

  vector<cParticleListElement>  ParticleList;

  //reduce the number of model particles 

  auto GetVelocityRange = [&] (int spec,double *vmin,double *vmax,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node)  {
    long int p;
    int res=0,idim;
    bool initflag=false;
    double v[3];

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {

        res++;
        PIC::ParticleBuffer::GetV(v,p);

        if (initflag==false) {
          for (idim=0;idim<3;idim++) vmin[idim]=v[idim],vmax[idim]=v[idim];

          initflag=true;
        }
        else {
          for (idim=0;idim<3;idim++) {
            if (vmin[idim]>v[idim]) vmin[idim]=v[idim];
            if (vmax[idim]<v[idim]) vmax[idim]=v[idim]; 
          }
        }
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    return res;
  };


  class cVelocitySpaceElement {
  public:
    int iv,jv,kv;
    vector<long int> p;
  }; 

  cVelocitySpaceElement **VelocitySpaceTable=NULL;
  const int nSerachLevels=6;

  int nVelCells1d=1<<nSerachLevels;
  int nVelCells=nVelCells1d*nVelCells1d*nVelCells1d;


  //create the initial velovity space table
  auto InitVelocitySpaceTable = [&] (int spec,double *vmin,double *dv,int i, int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node)  {
    long int p;
    int idim,iv[3],iVelCell;
    double v[3];
    bool foundflag;

    VelocitySpaceTable=new cVelocitySpaceElement* [nVelCells];
    for (int i=0;i<nVelCells;i++) VelocitySpaceTable[i]=NULL;

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {
        PIC::ParticleBuffer::GetV(v,p);

        for (idim=0;idim<3;idim++) iv[idim]=(int)((v[idim]-vmin[idim])/dv[idim]); 

        iVelCell=iv[0]+nVelCells1d*(iv[1]+nVelCells1d*iv[2]);  

        if (VelocitySpaceTable[iVelCell]==NULL) {
          VelocitySpaceTable[iVelCell]=new cVelocitySpaceElement [1];

          VelocitySpaceTable[iVelCell]->iv=iv[0];
          VelocitySpaceTable[iVelCell]->jv=iv[1];
          VelocitySpaceTable[iVelCell]->kv=iv[2];
        } 

        VelocitySpaceTable[iVelCell]->p.push_back(p);
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }
  }; 

  auto RemoveOneParticle= [&] (int nCurrentResolutionLevel,int nMaxParticlesReduce,int i, int j, int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,bool& more_then_one_particle_left) { 
    int cnt=0,npart;
    long int p;
    double w_remove;

    more_then_one_particle_left=false;

    if (nMaxParticlesReduce==0) return cnt;


    int nCells1d=1<<nCurrentResolutionLevel;
    int nCells=nCells1d*nCells1d*nCells1d;



    //create a list of the non-zero cells in the velocity space
    class cVelListEl {
    public:
      cVelocitySpaceElement* SpaceTableEl;
      int nTotalParticles;
    }; 

    vector <cVelListEl> SpaceTableElList;

    for (int ii=0;ii<nCells;ii++) if (VelocitySpaceTable[ii]!=NULL) if ((npart=VelocitySpaceTable[ii]->p.size())>1) {
      cVelListEl t;
  
      t.SpaceTableEl=VelocitySpaceTable[ii];
      t.nTotalParticles=npart;

      SpaceTableElList.push_back(t);
    } 


    auto cmp = [] (const cVelListEl& a, const cVelListEl &b) {
      return a.nTotalParticles>b.nTotalParticles;
    }; 
    
    sort(SpaceTableElList.begin(),SpaceTableElList.end(),cmp ); 
//      sort(SpaceTableElList.begin(),SpaceTableElList.end(), [] (const cVelListEl& a, const cVelListEl &b) {return (a.nTotalParticles)>(b.nTotalParticles);});  


    auto RemoveParticle = [&] (vector <cVelListEl>::iterator& it) {
      if (it->nTotalParticles==1) return;
      it->nTotalParticles--;   

      cVelocitySpaceElement* VelocitySpaceTableEl=it->SpaceTableEl;  

      double w_tot=0.0,w;

      p=VelocitySpaceTableEl->p[0];
      w_remove=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);

      //delete particles
      PIC::ParticleBuffer::DeleteParticle(p,node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);
      VelocitySpaceTableEl->p.erase(VelocitySpaceTableEl->p.begin());
      
      //distribute the weights
      for (auto& t : VelocitySpaceTableEl->p) w_tot+=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(t);
      
      for (auto& t : VelocitySpaceTableEl->p) {
        w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(t);
      
        w=w*(1.0+w_remove/w_tot);
        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(w,t);
      }
    }; 

    auto it_last=SpaceTableElList.begin();

    if (SpaceTableElList.size()==0) return 0;


    for (it_last=SpaceTableElList.begin();it_last!=SpaceTableElList.end();it_last++) {
      if (SpaceTableElList.begin()->nTotalParticles!=it_last->nTotalParticles) {
        break;
      }
    }

while (cnt<nMaxParticlesReduce) {
    for (auto it=SpaceTableElList.begin();it!=it_last;it++) {
      if (it->nTotalParticles!=1) {
        RemoveParticle(it);
        if (++cnt==nMaxParticlesReduce) return cnt;
      }
    }

    if (it_last!=SpaceTableElList.end()) if (it_last->nTotalParticles==SpaceTableElList.begin()->nTotalParticles) {
      for (;it_last!=SpaceTableElList.end();it_last++) {
        if (it_last->nTotalParticles!=SpaceTableElList.begin()->nTotalParticles) {
          break;
        }
      }

      auto t=it_last;

      if (++t==SpaceTableElList.end()) it_last=SpaceTableElList.end();
    }  

    if (SpaceTableElList.begin()->nTotalParticles==1) {
      break;
    }
}
     

    return cnt;
  }; 

  auto RebuildVelocitySpaceTable = [&] (int CurrentResolutionLevel) {
    cVelocitySpaceElement **NewVelocitySpaceTable;
    int iVelCell,iNewVelCell,iv,jv,kv;


    int NewResolutionLevel=CurrentResolutionLevel-1;

    int nVelCells1d=1<<CurrentResolutionLevel;
    int nNewVelCells1d=nVelCells1d/2;

    int nVelCells=nVelCells1d*nVelCells1d*nVelCells1d;
    int nNewVelCells=nNewVelCells1d*nNewVelCells1d*nNewVelCells1d;


    NewVelocitySpaceTable=new cVelocitySpaceElement* [nNewVelCells];
    for (int i=0;i<nNewVelCells;i++) NewVelocitySpaceTable[i]=NULL;

    for (int i=0;i<nVelCells;i++) {
      if (VelocitySpaceTable[i]!=NULL) {
        iv=VelocitySpaceTable[i]->iv;
        jv=VelocitySpaceTable[i]->jv;
        kv=VelocitySpaceTable[i]->kv;

        iVelCell=iv+nVelCells1d*(jv+nVelCells1d*kv);  

        iv/=2,jv/=2,kv/=2;
        iNewVelCell=iv+nNewVelCells1d*(jv+nNewVelCells1d*kv);

        if (NewVelocitySpaceTable[iNewVelCell]==NULL) {
          NewVelocitySpaceTable[iNewVelCell]=new cVelocitySpaceElement [1];

          *NewVelocitySpaceTable[iNewVelCell]=*VelocitySpaceTable[i];

          NewVelocitySpaceTable[iNewVelCell]->iv/=2;
          NewVelocitySpaceTable[iNewVelCell]->jv/=2;
          NewVelocitySpaceTable[iNewVelCell]->kv/=2;

        }
        else {
          NewVelocitySpaceTable[iNewVelCell]->p.insert(NewVelocitySpaceTable[iNewVelCell]->p.begin(),
              VelocitySpaceTable[i]->p.begin(),VelocitySpaceTable[i]->p.end());
        }
      }
    }

    //delete the previous table 
    for (int i=0;i<nVelCells;i++) if (VelocitySpaceTable[i]!=NULL) delete [] VelocitySpaceTable[i];

    delete [] VelocitySpaceTable;
    VelocitySpaceTable=NewVelocitySpaceTable;
  }; 

  auto DeleteVelocitySpaceTable = [&] (int CurrentResolutionLevel) {
    int nVelCells1d=1<<CurrentResolutionLevel;
    int nVelCells=nVelCells1d*nVelCells1d*nVelCells1d;

    for (int i=0;i<nVelCells;i++) if (VelocitySpaceTable[i]!=NULL) delete [] VelocitySpaceTable[i];

    delete [] VelocitySpaceTable;
  };

  auto ReduceParticleNumber1 = [&] (int spec,int nModelParticles,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,int nRequestedParticles)  {
    double vmin[3],vmax[3],dv[3]; 
    bool more_then_one_particle_left;

    if (nModelParticles>nRequestedParticles) {
      int nDeleteParticles=nModelParticles-nRequestedParticles;  
      int nRemovedParticles=0;

      GetVelocityRange(spec,vmin,vmax,i,j,k,node);

      for (int idim=0;idim<3;idim++) {
        double d=vmax[idim]-vmin[idim]; 

        vmin[idim]-=0.05*d; 
        vmax[idim]+=0.05*d;

        dv[idim]=(vmax[idim]-vmin[idim])/nVelCells1d;
      }


      InitVelocitySpaceTable(spec,vmin,dv,i,j,k,node);

      int CurrentResolutionLevel=nSerachLevels;

      while (nRemovedParticles<nDeleteParticles) {
        nRemovedParticles+=RemoveOneParticle(CurrentResolutionLevel,nDeleteParticles-nRemovedParticles,i,j,k,node,more_then_one_particle_left); 

        if ((nRemovedParticles<nDeleteParticles)&&(more_then_one_particle_left==false)) {
          RebuildVelocitySpaceTable(CurrentResolutionLevel);
          --CurrentResolutionLevel;
        } 
      }

      DeleteVelocitySpaceTable(CurrentResolutionLevel);    
    }
  };

  auto PrintStat = [&] (int spec,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
    double w_tot=0.0,v[3],v_tot[3]={0.0,0.0,0.0},w;
    int nModelParticles=0;
    long int p;

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {
        PIC::ParticleBuffer::GetV(v,p);
        w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);

        for (int i=0;i<3;i++) v_tot[i]+=w*v[i];


        w_tot+=w;
        nModelParticles++;
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    if (nModelParticles!=0) for (int i=0;i<3;i++) v_tot[i]/nModelParticles;

//    cout << "w_tot=" << w_tot << ", npart=" << nModelParticles << ", v=" << v_tot[0] << ", " << v_tot[1] << ", " << v_tot[2] << ", i,j,k=" << i << ", " << j << ", " << k << endl;
  };

  auto ReduceParticleNumber = [&] (int spec,int i,int j,int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,int nRequestedParticles)  {
    vector<cParticleDescriptor> ParticleList;  
    cParticleDescriptor t;
    double *v,w_max=0.0,w_min=-1.0,SummedWeight=0.0,w;
    int nModelParticles=0;
    long int p;

    p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) { 
        t.p=p;
        t.w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);
        ParticleList.push_back(t);

        SummedWeight+=t.w;
        nModelParticles++;

        if (w_max<t.w) w_max=t.w;
        if ((w_min<0.0)||(w_min>t.w)) w_min=t.w; 
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    //delete particles 
    int nDeleteParticles=nModelParticles-nRequestedParticles,ip; 
    double RemovedParticleWeight=0.0;
    int ii;

    for (ii=0;ii<nDeleteParticles;ii++) {
      const bool _search_continue=true;
      const bool _search_completed=false; 

      bool flag=_search_continue;
      int ip, cnt=0;

      while ((++cnt<1000000)&&(flag==_search_continue)) {
        ip=(int)(rnd()*nModelParticles);

        if (w_min/ParticleList[ip].w>rnd()) { // delete particles with smaller weight  
          RemovedParticleWeight+=ParticleList[ip].w;
          PIC::ParticleBuffer::DeleteParticle(ParticleList[ip].p,node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);

          ParticleList[ip]=ParticleList[nModelParticles-1];
          nModelParticles--;
          flag=_search_completed; 
        }
      } 
    }


    //distribute the removed weight among particles that left in the system 
    for (ii=0;ii<nModelParticles;ii++) {
      w=ParticleList[ii].w*(1.0+RemovedParticleWeight/(SummedWeight-RemovedParticleWeight));    
      p=ParticleList[ii].p;

      PIC::ParticleBuffer::SetIndividualStatWeightCorrection(w,p);
    }
  }; 


  //collect particles in the cell block 2x2x2
  auto CollectParticleBlock = [&] (int spec,int i0,int j0,int k0,list<cParticleId>& ParticleList) {
    cParticleId t;
    long int p;
    double ParticleWeight;

    ParticleList.clear(); 

    for (di=0;di<2;di++) for (dj=0;dj<2;dj++) for (dk=0;dk<2;dk++) { 
      i=i0+di;   
      j=j0+dj;
      k=k0+dk; 

      p=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
      t.nd=_getCenterNodeLocalNumber(i,j,k);

      while (p!=-1) {
        if (PIC::ParticleBuffer::GetI(p)==spec) {
          t.ptr=p; 
          ParticleList.push_back(t);

          ParticleWeight=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);
          ParticleWeight/=8.0;
          PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ParticleWeight,p);
        }

        p=PIC::ParticleBuffer::GetNext(p);
      }
    }
  };

  //scatter particles 
  auto ScatterParticles = [&] (int i,int j, int k,double *xmin,double *dx,list<cParticleId>& ParticleList) {
    int nd;
    double x[3];

    nd=_getCenterNodeLocalNumber(i,j,k); 

    for (auto& it : ParticleList) {
      if (it.nd!=nd) {
        p=PIC::ParticleBuffer::GetNewParticle(node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]); 
        PIC::ParticleBuffer::CloneParticle(p,it.ptr);

        x[0]=xmin[0]+(i+rnd())*dx[0];
        x[1]=xmin[1]+(j+rnd())*dx[1];
        x[2]=xmin[2]+(k+rnd())*dx[2]; 

        PIC::ParticleBuffer::SetX(x,p);
      }
    }
  };


  for (inode=0;inode<PIC::DomainBlockDecomposition::nLocalBlocks;inode++) {
    node=PIC::DomainBlockDecomposition::BlockTable[inode];

    if (node->block!=NULL) {
      for (int s=0;s<PIC::nTotalSpecies;s++) for (i0=0;i0+1<_BLOCK_CELLS_X_;i0+=2) for (j0=0;j0+1<_BLOCK_CELLS_Y_;j0+=2) for (k0=0;k0+1<_BLOCK_CELLS_Z_;k0+=2) {
        nParticles=GetBlockParticleNumber(s,i0,j0,k0,node);

        if ((nParticles!=0)&&(nParticles<particle_num_limit_min*8)) { // 8<- the number of cell in the cell block 2x2x2
          //the number of particles is below the limit: scatter the particles 
          list<cParticleId> ParticleList;
          double dx[3],*xmin,x[3];

          xmin=node->xmin;

          dx[0]=(node->xmax[0]-xmin[0])/_BLOCK_CELLS_X_;
          dx[1]=(node->xmax[1]-xmin[1])/_BLOCK_CELLS_Y_;
          dx[2]=(node->xmax[2]-xmin[2])/_BLOCK_CELLS_Z_;

          CollectParticleBlock(s,i0,j0,k0,ParticleList);


          for (di=0;di<2;di++) for (dj=0;dj<2;dj++) for (dk=0;dk<2;dk++) { 
            i=i0+di;   
            j=j0+dj;
            k=k0+dk; 

            ScatterParticles(i,j,k,xmin,dx,ParticleList);
          }
        }

        for (di=0;di<2;di++) for (dj=0;dj<2;dj++) for (dk=0;dk<2;dk++) { 
          i=i0+di;   
          j=j0+dj;
          k=k0+dk; 

          int npart;
          int CurrentResolutionLevel=nSerachLevels;

          if ((npart=GetParticleNumber(s,i,j,k,node))>particle_num_limit_max) {

if (PIC::ThisThread==0) PrintStat(s,i,j,k,node);

            ReduceParticleNumber1(s,npart,i,j,k,node,particle_num_limit_min+0.9*(particle_num_limit_max-particle_num_limit_min));

if (PIC::ThisThread==0) PrintStat(s,i,j,k,node);
          }
        }
      }
    }
  }
}  

///////////////////////////////////////////////////////////////////////////////////////////////////////
void PIC::ParticleSplitting::Split::SplitWithVelocityShift_FL(int particle_num_limit_min,int particle_num_limit_max,double WeightSplittingLimit) {
  int inode,i,j,k,i0,j0,k0,di,dj,dk,nParticles;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  long int p;
  
  namespace FL = PIC::FieldLine;
  namespace PB = PIC::ParticleBuffer;

  auto GetParticleNumber = [&] (int *ParticleNumberTable,FL::cFieldLineSegment *Segment) {
    long int p;
    int spec;

    for (spec=0;spec<PIC::nTotalSpecies;spec++) ParticleNumberTable[spec]=0;

    p=Segment->FirstParticleIndex;

    while (p!=-1) {
      ParticleNumberTable[PIC::ParticleBuffer::GetI(p)]++;
      p=PIC::ParticleBuffer::GetNext(p);
    }
  };


  class cParticleListElement {
  public:
    long int ptr;
    double w;
  };


  class cParticleId {
  public:
    long int ptr;
    int nd;
  };

  class cParticleDescriptor {
  public:
    long int p;
    double w;
  };

  vector<cParticleListElement>  ParticleList;

  //reduce the number of model particles 
  auto GetVelocityRange = [&] (int spec,double *vmin,double *vmax,FL::cFieldLineSegment *Segment)  {
    long int p;
    int res=0,idim;
    bool initflag=false;
    double vNorm,vParallel,v[3];

    p=Segment->FirstParticleIndex;

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {

        res++;
        v[0]=PB::GetVParallel(p);
        v[1]=PB::GetVNormal(p);
        v[2]=0.0;

        if (initflag==false) {
          for (idim=0;idim<3;idim++) vmin[idim]=v[idim],vmax[idim]=v[idim];

          initflag=true;
        }
        else {
          for (idim=0;idim<3;idim++) {
            if (vmin[idim]>v[idim]) vmin[idim]=v[idim];
            if (vmax[idim]<v[idim]) vmax[idim]=v[idim]; 
          }
        }
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    return res;
  };


  class cVelocitySpaceElement {
  public:
    int iv,jv,kv;
    vector<long int> p;
  }; 

  cVelocitySpaceElement **VelocitySpaceTable=NULL;
  const int nSerachLevels=6;

  int nVelCells1d=1<<nSerachLevels;
  int nVelCells=nVelCells1d*nVelCells1d;


  //create the initial velovity space table
  auto InitVelocitySpaceTable = [&] (int spec,double *vmin,double *dv,FL::cFieldLineSegment *Segment)  {
    long int p;
    int idim,iv[3],iVelCell;
    double v[3];
    bool foundflag;

    VelocitySpaceTable=new cVelocitySpaceElement* [nVelCells];
    for (int i=0;i<nVelCells;i++) VelocitySpaceTable[i]=NULL;

    p=Segment->FirstParticleIndex;

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {
        v[0]=PB::GetVParallel(p);
        v[1]=PB::GetVNormal(p);
        v[2]=0.0;

        for (idim=0;idim<2;idim++) {
          iv[idim]=(int)((v[idim]-vmin[idim])/dv[idim]); 

          if (iv[idim]<0) iv[idim]=0;
          if (iv[idim]>=nVelCells1d) iv[idim]=nVelCells1d-1;   
        } 

        iVelCell=iv[0]+nVelCells1d*iv[1];  

        if (VelocitySpaceTable[iVelCell]==NULL) {
          VelocitySpaceTable[iVelCell]=new cVelocitySpaceElement [1];

          VelocitySpaceTable[iVelCell]->iv=iv[0];
          VelocitySpaceTable[iVelCell]->jv=iv[1];
          VelocitySpaceTable[iVelCell]->kv=iv[2];
        } 

        VelocitySpaceTable[iVelCell]->p.push_back(p);
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }
  }; 

  auto RemoveOneParticle= [&] (int nCurrentResolutionLevel,int nMaxParticlesReduce,FL::cFieldLineSegment *Segment,bool& more_then_one_particle_left) { 
    int cnt=0,npart;
    long int p;
    double w_remove;

    more_then_one_particle_left=false;

    if (nMaxParticlesReduce==0) return cnt;


    int nCells1d=1<<nCurrentResolutionLevel;
    int nCells=nCells1d*nCells1d;


    for (int ii=0;ii<nCells;ii++) if (VelocitySpaceTable[ii]!=NULL) if ((npart=VelocitySpaceTable[ii]->p.size())>1) { 
      double w_tot=0.0,w,wmin;
      auto itmin=VelocitySpaceTable[ii]->p.begin(); 


      //find particle with smallest weight
      p=-1,wmin=-1.0;
    
  
      for (auto it=VelocitySpaceTable[ii]->p.begin();it!=VelocitySpaceTable[ii]->p.end();it++) {
        w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(*it);

        if ((wmin<0.0)||(w<wmin)) p=*it,wmin=w,itmin=it;
      }


      //find particles that closest to 'p' in the velocity space 
      long int p1;
      double d2,d2min=-1.0;
      double v1[3],v2[3];

    //  PIC::ParticleBuffer::GetV(v1,p);
      
      v1[0]=PB::GetVParallel(p);
      v1[1]=PB::GetVNormal(p);
      v1[2]=0.0;

      for (auto& t : VelocitySpaceTable[ii]->p) if (t!=p) {
        //PIC::ParticleBuffer::GetV(v2,p);
        v2[0]=PB::GetVParallel(t);
        v2[1]=PB::GetVNormal(t);
        v2[2]=0.0;


        d2=0.0;
        
        for (int idim=0;idim<3;idim++) {
          double tt=v1[idim]-v2[idim];

          d2+=tt*tt;
        }

        if ((d2min<0.0)||(d2<d2min)) d2min=d2,p1=t;
      } 
 
      //merge particle p1 and p: weight of 'p' is added to 'p1'
      w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p1); 

      //PIC::ParticleBuffer::GetV(v1,p1);
      v1[0]=PB::GetVParallel(p1);
      v1[1]=PB::GetVNormal(p1);
      v1[2]=0.0;
      
      
      
      //PIC::ParticleBuffer::GetV(v2,p);
      v2[0]=PB::GetVParallel(p);
      v2[1]=PB::GetVNormal(p);
      v2[2]=0.0;

      for (int idim=0;idim<3;idim++) v1[idim]=(w*v1[idim]+wmin*v2[idim])/(w+wmin);

      //PIC::ParticleBuffer::SetV(v1,p1);  
      PB::SetVParallel(v1[0],p1);
      PB::SetVNormal(v1[1],p1);
      
      PIC::ParticleBuffer::SetIndividualStatWeightCorrection(w+wmin,p1);
      
      //delete particles
      PIC::ParticleBuffer::DeleteParticle(p,Segment->FirstParticleIndex);
      VelocitySpaceTable[ii]->p.erase(itmin);

      if (npart-1>1) more_then_one_particle_left=true;

      if (++cnt==nMaxParticlesReduce) break;
    }

    return cnt;
  }; 

  auto RebuildVelocitySpaceTable = [&] (int CurrentResolutionLevel) {
    cVelocitySpaceElement **NewVelocitySpaceTable;
    int iVelCell,iNewVelCell,iv,jv;


    int NewResolutionLevel=CurrentResolutionLevel-1;

    int nVelCells1d=1<<CurrentResolutionLevel;
    int nNewVelCells1d=nVelCells1d/2;

    int nVelCells=nVelCells1d*nVelCells1d;
    int nNewVelCells=nNewVelCells1d*nNewVelCells1d;


    NewVelocitySpaceTable=new cVelocitySpaceElement* [nNewVelCells];
    for (int i=0;i<nNewVelCells;i++) NewVelocitySpaceTable[i]=NULL;

    for (int i=0;i<nVelCells;i++) {
      if (VelocitySpaceTable[i]!=NULL) {
        iv=VelocitySpaceTable[i]->iv;
        jv=VelocitySpaceTable[i]->jv;

        iVelCell=iv+nVelCells1d*jv;  

        iv/=2,jv/=2;
        iNewVelCell=iv+nNewVelCells1d*jv;

        if (NewVelocitySpaceTable[iNewVelCell]==NULL) {
          NewVelocitySpaceTable[iNewVelCell]=new cVelocitySpaceElement [1];

          *NewVelocitySpaceTable[iNewVelCell]=*VelocitySpaceTable[i];

          NewVelocitySpaceTable[iNewVelCell]->iv/=2;
          NewVelocitySpaceTable[iNewVelCell]->jv/=2;
        }
        else {
          NewVelocitySpaceTable[iNewVelCell]->p.insert(NewVelocitySpaceTable[iNewVelCell]->p.begin(),
              VelocitySpaceTable[i]->p.begin(),VelocitySpaceTable[i]->p.end());
        }
      }
    }

    //delete the previous table 
    for (int i=0;i<nVelCells;i++) if (VelocitySpaceTable[i]!=NULL) delete [] VelocitySpaceTable[i];

    delete [] VelocitySpaceTable;
    VelocitySpaceTable=NewVelocitySpaceTable;
  }; 


  auto DeleteVelocitySpaceTable = [&] (int CurrentResolutionLevel) {
    int nVelCells1d=1<<CurrentResolutionLevel;
    int nVelCells=nVelCells1d*nVelCells1d;

    for (int i=0;i<nVelCells;i++) if (VelocitySpaceTable[i]!=NULL) delete [] VelocitySpaceTable[i];

    delete [] VelocitySpaceTable;
  };

  auto ReduceParticleNumber1 = [&] (int spec,int nModelParticles,FL::cFieldLineSegment *Segment,int nRequestedParticles)  {
    double vmin[3],vmax[3],dv[3]; 
    bool more_then_one_particle_left;


    if (nModelParticles>nRequestedParticles) {
      int nDeleteParticles=nModelParticles-nRequestedParticles;  
      int nRemovedParticles=0;


      static int ncall=0;
      ncall++;

      GetVelocityRange(spec,vmin,vmax,Segment);

      for (int idim=0;idim<3;idim++) {
        double d=vmax[idim]-vmin[idim]; 

        if (d==0.0) d=1.0;

        vmin[idim]-=0.05*d; 
        vmax[idim]+=0.05*d;

        dv[idim]=(vmax[idim]-vmin[idim])/nVelCells1d;
      }


      InitVelocitySpaceTable(spec,vmin,dv,Segment);

      int CurrentResolutionLevel=nSerachLevels;

      while (nRemovedParticles<nDeleteParticles) {
        nRemovedParticles+=RemoveOneParticle(CurrentResolutionLevel,nDeleteParticles-nRemovedParticles,Segment,more_then_one_particle_left); 

        if ((nRemovedParticles<nDeleteParticles)&&(more_then_one_particle_left==false)) {
          RebuildVelocitySpaceTable(CurrentResolutionLevel);
          --CurrentResolutionLevel;
        } 
      }

      DeleteVelocitySpaceTable(CurrentResolutionLevel);
    }
  };

  auto ReduceParticleNumber = [&] (int spec,FL::cFieldLineSegment *Segment,int nRequestedParticles)  {
    vector<cParticleDescriptor> ParticleList;  
    cParticleDescriptor t;
    double *v,w_max=0.0,w_min=-1.0,SummedWeight=0.0,w;
    int nModelParticles=0;
    long int p;

    p=Segment->FirstParticleIndex;

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) { 
        t.p=p;
        t.w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);
        ParticleList.push_back(t);

        SummedWeight+=t.w;
        nModelParticles++;

        if (w_max<t.w) w_max=t.w;
        if ((w_min<0.0)||(w_min>t.w)) w_min=t.w; 
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    //delete particles 
    int nDeleteParticles=nModelParticles-nRequestedParticles,ip; 
    double RemovedParticleWeight=0.0;
    int ii;

    for (ii=0;ii<nDeleteParticles;ii++) {
      const bool _search_continue=true;
      const bool _search_completed=false; 

      bool flag=_search_continue;
      int ip, cnt=0;

      while ((++cnt<1000000)&&(flag==_search_continue)) {
        ip=(int)(rnd()*nModelParticles);

        if (w_min/ParticleList[ip].w>rnd()) { // delete particles with smaller weight  
          RemovedParticleWeight+=ParticleList[ip].w;
          PIC::ParticleBuffer::DeleteParticle(ParticleList[ip].p,Segment->FirstParticleIndex);

          ParticleList[ip]=ParticleList[nModelParticles-1];
          nModelParticles--;
          flag=_search_completed; 
        }
      } 
    }


    //distribute the removed weight among particles that left in the system 
    for (ii=0;ii<nModelParticles;ii++) {
      w=ParticleList[ii].w*(1.0+RemovedParticleWeight/(SummedWeight-RemovedParticleWeight));    
      p=ParticleList[ii].p;

      PIC::ParticleBuffer::SetIndividualStatWeightCorrection(w,p);
    }
  }; 




  auto IncreseParticleNumber = [&] (int spec,FL::cFieldLineSegment *Segment) {
    double MeanV[3]={0.0,0.0,0.0},v[3],w;
    double MeanV2[3]={0.0,0.0,0.0};

    vector<cParticleDescriptor> ParticleList;
    cParticleDescriptor t;
    double w_max=0.0,SummedWeight=0.0,ThermalSpeed=0.0;
    int idim,nModelParticles=0;
    long int p;

    p=Segment->FirstParticleIndex;

    while (p!=-1) {
      if (spec==PIC::ParticleBuffer::GetI(p)) {
        t.p=p;
        t.w=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(p);

        if (t.w>WeightSplittingLimit) {
          ParticleList.push_back(t); 

          SummedWeight+=t.w;
          if (w_max<t.w) w_max=t.w;

          //PIC::ParticleBuffer::GetV(v,p); 
          v[0]=PB::GetVParallel(p);
          v[1]=PB::GetVNormal(p);
          v[2]=0.0;

          for (int idim=0;idim<3;idim++) {
            MeanV[idim]+=t.w*v[idim];
            MeanV2[idim]+=t.w*v[idim]*v[idim];
          }

          nModelParticles++;
        }
      }

      p=PIC::ParticleBuffer::GetNext(p);
    }

    if (nModelParticles<2) return;

    for (idim=0;idim<3;idim++) ThermalSpeed+=MeanV2[idim]/SummedWeight-MeanV[idim]*MeanV[idim]/(SummedWeight*SummedWeight); 

    ThermalSpeed=sqrt(fabs(ThermalSpeed));

    //add new particles in the system
    int nNewParticles=particle_num_limit_max-nModelParticles;
    int ip;
    long int pnew;
    double l[3],vnew[3],vold[3];
    bool flag;

    for (int ii=0;ii<nNewParticles;ii++) {
      flag=true;

      while (flag==true) {
        ip=(int)(rnd()*nModelParticles);

        int ip_wmax=-1;
        w_max=-1.0;
 
        for (int i=0;i<nModelParticles;i++) {
          if (w_max<ParticleList[i].w) w_max=ParticleList[i].w,ip_wmax=i;
        } 

        ip=ip_wmax;

        if (true) { // (ParticleList[ip].w/w_max>rnd()) { //split particles with larger weight  
          pnew=PIC::ParticleBuffer::GetNewParticle(Segment->FirstParticleIndex); 
          PIC::ParticleBuffer::CloneParticle(pnew,ParticleList[ip].p);

          //set new weight for both particles 
          ParticleList[ip].w/=2.0;
          t=ParticleList[ip];
          t.p=pnew;
          ParticleList.push_back(t);
          nModelParticles++;
          flag=false;

          PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ParticleList[ip].w,ParticleList[ip].p);
          PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ParticleList[ip].w,pnew);

          //perturb a new particle velocity;
          Vector3D::Distribution::Uniform(l,0.5*v_shift_max*ThermalSpeed); 

          //get velocity for the new particle 
       //   PIC::ParticleBuffer::GetV(vnew,pnew); 
        vnew[0]=PB::GetVParallel(pnew);
        vnew[1]=PB::GetVNormal(pnew);
        vnew[2]=0.0;

          for (idim=0;idim<3;idim++) {
            vold[idim]=vnew[idim]-l[idim];
            vnew[idim]+=l[idim];
          }

          //PIC::ParticleBuffer::SetV(vnew,pnew);
          PB::SetVParallel(vnew[0],pnew);
          PB::SetVNormal(vnew[1],pnew);

       //   PIC::ParticleBuffer::SetV(vold,ParticleList[ip].p);
          PB::SetVParallel(vold[0],ParticleList[ip].p);
          PB::SetVNormal(vold[1],ParticleList[ip].p);


/*
          double dx[3],*xmin,*xmax,x[3],x0[3];

          xmin=node->xmin;
          

          dx[0]=(node->xmax[0]-xmin[0])/_BLOCK_CELLS_X_;
          dx[1]=(node->xmax[1]-xmin[1])/_BLOCK_CELLS_Y_;
          dx[2]=(node->xmax[2]-xmin[2])/_BLOCK_CELLS_Z_;
*/

/*

 



double r;
bool flag=true;
do {
  flag=true;


    r=pow(rnd(),1.0/3.0);

  
  if (apply_non_uniform_x_shift==true) if (r>rnd()) {
    flag=false;
    continue;
  }

  PIC::ParticleBuffer::GetX(x0,pnew);

  double ll[3];

  Vector3D::Distribution::Uniform(ll,x_shift_max*r*sqrt(dx[0]*dx[0]+dx[1]*dx[1]+dx[2]*dx[2]));

  x0[0]+=ll[0];
  if (x0[0]<xmin[0]+i*dx[0]) {flag=false;continue;}
  if (x0[0]>xmin[0]+(i+1)*dx[0]) {flag=false;continue;} 

  x0[1]+=ll[1];
  if (x0[1]<xmin[1]+j*dx[1]) {flag=false;continue;}
  if (x0[1]>xmin[1]+(j+1)*dx[1]) {flag=false;continue;}

  x0[2]+=ll[2];
  if (x0[2]<xmin[2]+k*dx[2]) {flag=false;continue;}
  if (x0[2]>xmin[2]+(k+1)*dx[2]) {flag=false;continue;}
}
while (flag==false);



PIC::ParticleBuffer::SetX(x0,pnew);*/



        }
      }
    }
  };


  //loop through the nodes
  int ParticleNumberTable[PIC::nTotalSpecies],spec; 
  
  
  
  for (int iFieldLine=0; iFieldLine<FL::nFieldLine; iFieldLine++) {  
    FL::cFieldLineSegment* Segment;
    int iSegment;
    
    for (iSegment=0,Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment(); iSegment<FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber(); iSegment++,Segment=Segment->GetNext()) {
      GetParticleNumber(ParticleNumberTable,Segment);

      for (spec=0;spec<PIC::nTotalSpecies;spec++) {
        if (ParticleNumberTable[spec]>particle_num_limit_max) {
          ReduceParticleNumber1(spec,ParticleNumberTable[spec],Segment,particle_num_limit_min+0.9*(particle_num_limit_max-particle_num_limit_min));
        }
        else if ((1<ParticleNumberTable[spec])&&(ParticleNumberTable[spec]<particle_num_limit_min)) {
          IncreseParticleNumber(spec,Segment);
        }
      }
    }
    
  }
  

/*  for (inode=0;inode<PIC::DomainBlockDecomposition::nLocalBlocks;inode++) {
    node=PIC::DomainBlockDecomposition::BlockTable[inode];

    if (node->block!=NULL) {
      for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        GetParticleNumber(ParticleNumberTable,i,j,k,node);

        for (spec=0;spec<PIC::nTotalSpecies;spec++) {
          if (ParticleNumberTable[spec]>particle_num_limit_max) {
            ReduceParticleNumber1(spec,ParticleNumberTable[spec],i,j,k,node,particle_num_limit_min+0.9*(particle_num_limit_max-particle_num_limit_min));
          }
          else if ((1<ParticleNumberTable[spec])&&(ParticleNumberTable[spec]<particle_num_limit_min)) {
            IncreseParticleNumber(spec,i,j,k,node);
          }
        }
      }
    }      
  }*/

//  PIC::Parallel::ExchangeParticleData();
} 

//===============================================================================================
/**
 * PIC::ParticleSplitting::MergeParticleList(): 
 * Merges three low-weight particles into two within the same species.
 * 1. **Conservation Laws:** Ensures that weight, momentum, and energy are conserved during the merging process.
 * 
 * 2. **Merging Strategy:** 
 *    - Prioritizes merging smaller (lower-weight) particles first to efficiently reduce particle count.
 *    - Merges three particles into two, conserving statistical weight, momentum, and energy.
 * 
 * 3. **Velocity Space Binning:**
 *    - Utilizes a binned velocity space to organize particles, enhancing the efficiency of locating suitable candidates for merging.
 *    - Prioritizes merging within overpopulated bins to maintain an even distribution of particles across velocity space.
 * 
 * 4. **Operational Workflow:**
 *    - Identifies overpopulated bins based on the defined thresholds.
 *    - Selects the smallest particles within these bins for merging.
 *    - Ensures that the closest particles in velocity space are chosen to maintain physical realism.
 */
void PIC::ParticleSplitting::MergeParticleList(int spec,long int& FirstParticle,int nRequestedParticleNumber) { 
namespace PB = PIC::ParticleBuffer;

  // Structure to hold particle information
  struct ParticleInfo {
    long int index;
    double v[3];
    double weight;
    bool active;
  };

  // Structure to represent a bin in the linked list
  struct BinNode {
    int binIndex;
    int binSize;
    std::list<ParticleInfo*>* particlePtrs;
  };

  // Step 1: Collect all particles of the specified species in the cell
  std::vector<ParticleInfo> particles;
  long int particleIndex = FirstParticle;

  // Reserve memory for the particles vector now that we know the total number of particles
  particles.reserve(2000);

  while (particleIndex != -1) {
    if (PB::GetI(particleIndex) == spec) {
      ParticleInfo p;
      p.index = particleIndex;
      PB::GetV(p.v, particleIndex);
      p.weight = PB::GetIndividualStatWeightCorrection(particleIndex);
	    p.active = true;
      particles.emplace_back(std::move(p));
    }
    particleIndex = PB::GetNext(particleIndex);
  }

  // Early exit if no merging is needed
  if (particles.size() <= nRequestedParticleNumber) {
    return;
  }

  int nTotalActiveParticles=particles.size();

  // Step 2: Determine velocity range (calculate vMin and vMax once)
  double vMin[3] = {particles[0].v[0], particles[0].v[1], particles[0].v[2]};
  double vMax[3] = {particles[0].v[0], particles[0].v[1], particles[0].v[2]};
  
  for (const auto &p : particles) {
    for (int d = 0; d < 3; ++d) {
      if (p.v[d] < vMin[d]) vMin[d] = p.v[d];
      if (p.v[d] > vMax[d]) vMax[d] = p.v[d];
    }
  }

  // Add safety margin to vMin and vMax
  for (int d = 0; d < 3; ++d) {
    double velocityRange = vMax[d] - vMin[d];
    double safetyMargin = 0.01 * velocityRange; // 1% of the velocity range
  
    vMin[d] -= safetyMargin;
    vMax[d] += safetyMargin;
  }

  // Define initial number of bins per dimension
  int nBinsPerDimension = 10; // Starting value; adjust as needed
  const int minBinsPerDimension = 1; // Minimum allowed value

  while (nBinsPerDimension >= minBinsPerDimension && nTotalActiveParticles > nRequestedParticleNumber ) {
    // Step 3: Define velocity bins using vMin and vMax with safety margin
    double dv[3];
   
    for (int d = 0; d < 3; ++d) {
      dv[d] = (vMax[d] - vMin[d]) / nBinsPerDimension;
      if (dv[d] == 0.0) dv[d] = 1e-6; // Prevent division by zero
    }

    // Compute total number of bins
    int totalBins = nBinsPerDimension * nBinsPerDimension * nBinsPerDimension;

    // Create bins: a vector of lists 
    std::vector<std::list<ParticleInfo*>> bins(totalBins);

    // Step 4: Assign particles to bins using flat indices
    for (auto &p : particles) if (p.active==true) {
      int binIndex[3];

      for (int d = 0; d < 3; ++d) {
        binIndex[d] = static_cast<int>((p.v[d] - vMin[d]) / dv[d]);
        if (binIndex[d] >= nBinsPerDimension) binIndex[d] = nBinsPerDimension - 1;
        if (binIndex[d] < 0) binIndex[d] = 0; // Clamp to 0
      }
	    
      int flatIndex = binIndex[0]
                    + nBinsPerDimension * (binIndex[1]
                    + nBinsPerDimension * binIndex[2]);

      bins[flatIndex].push_back(&p);
    }

    // Step 5: Sort particles within each bin by ascending weight
    for (auto &bin : bins) {
      if (bin.size() >= 3) {
        bin.sort([](const ParticleInfo* a, const ParticleInfo* b) {
          return a->weight < b->weight;
        });
      }
    }

    // Step 6: Create a sorted linked list of bins sorted by bin size descending
    std::list<BinNode> binList;

    for (int i = 0; i < totalBins; ++i) {
      int binSize = bins[i].size();

      if (binSize >= 3) {
        BinNode node = {i, binSize, &bins[i]};
        binList.emplace_back(std::move(node));
      }
    }

    // Sort the binList in descending order of binSize
    binList.sort([](const BinNode& a, const BinNode& b) {
      return a.binSize > b.binSize;
    });

    // Step 7: Merge particles in bins while conserving weight, momentum, and energy
    auto currentBinIt = binList.begin();
    int nextBinSize=-1; 

    if (std::next(currentBinIt)!=binList.end()) nextBinSize=std::next(currentBinIt)->binSize;

    while (!binList.empty() && nTotalActiveParticles > nRequestedParticleNumber && currentBinIt != binList.end()) {
      BinNode& binNode = *currentBinIt;
      std::list<ParticleInfo*>& particlePtrs = *(binNode.particlePtrs);

      if (particlePtrs.size()<3) {
      //There is not more bins with the number of particles > 3 => break the current loop and repeat with smaller number if bins
        break;
      }

      // While there are at least three particles to merge in the bin
      while (particlePtrs.size() >= 3 && nTotalActiveParticles > nRequestedParticleNumber) {
        // Select the first three particles (smallest weights)
        auto it1 = particlePtrs.begin();
        auto it2 = std::next(it1);
        auto it3 = std::next(it2);
	
        ParticleInfo* p1 = *it1;
        ParticleInfo* p2 = *it2;
        ParticleInfo* p3 = *it3;

        particlePtrs.erase(it1,std::next(it3));

        // Compute total weight and set new weights
        double W_total = p1->weight + p2->weight + p3->weight;
        double W_new = W_total * 0.5;

        // Compute total momentum and energy
        double momentum_total[3] = {0.0, 0.0, 0.0};
        double energy_total = p1->weight*Vector3D::DotProduct(p1->v,p1->v)+p2->weight*Vector3D::DotProduct(p2->v,p2->v)+p3->weight*Vector3D::DotProduct(p3->v,p3->v);

        for (int d = 0; d < 3; ++d) {
          momentum_total[d] = p1->weight * p1->v[d]
                            + p2->weight * p2->v[d]
                            + p3->weight * p3->v[d];
        }

        // Compute mean velocity
        double v_mean[3];

        for (int d = 0; d < 3; ++d) {
          v_mean[d] = momentum_total[d] / W_total;
        }

        // Compute magnitude of delta_v to satisfy energy conservation
        double delta_v_magnitude2=energy_total/W_total-Vector3D::DotProduct(v_mean,v_mean);
        double delta_v_magnitude=(delta_v_magnitude2>0.0) ? sqrt(delta_v_magnitude2) : 0.0;
	
        // Generate random direction for delta_v
        double theta = acos(1.0 - 2.0 * rnd()); // theta in [0, pi]
        double phi = 2.0 * M_PI * rnd();        // phi in [0, 2*pi]

        double sin_theta = sin(theta);
        double delta_v[3];

        if (_PIC_FIELD_LINE_MODE_==_PIC_MODE_OFF_) { 
          delta_v[0] = delta_v_magnitude * sin_theta * cos(phi);
          delta_v[1] = delta_v_magnitude * sin_theta * sin(phi);
          delta_v[2] = delta_v_magnitude * cos(theta);
    	}
        else {
          delta_v[0] = delta_v_magnitude * cos(phi);
          delta_v[1] = delta_v_magnitude * sin(phi);
          delta_v[2] = 0.0;
        } 

        // Compute new velocities
        double vA[3], vB[3];
       
        for (int d = 0; d < 3; ++d) {
          vA[d] = v_mean[d] + delta_v[d];
          vB[d] = v_mean[d] - delta_v[d];
        }

        // Remove the original third particle from PB
        PB::DeleteParticle(p3->index,FirstParticle);
	      p3->active=false;

        // Update weight ang velocity in the first two particles 
        // Particle A
        auto* dataA = PB::GetParticleDataPointer(p1->index);
        PB::SetV(vA,dataA);
        PB::SetIndividualStatWeightCorrection(W_new,dataA);

        p1->weight = W_new;
        p1->v[0] = vA[0]; p1->v[1] = vA[1]; p1->v[2] = vA[2];

        // Particle B
        auto* dataB = PB::GetParticleDataPointer(p2->index);
        PB::SetV(vB,dataB);
        PB::SetIndividualStatWeightCorrection(W_new,dataB);

        p2->weight = W_new;
        p2->v[0] = vB[0]; p2->v[1] = vB[1]; p2->v[2] = vB[2];

        //Place p1 anps p2 in the appropriate locations in particlePtrs 
        auto pos = std::find_if(particlePtrs.begin(), particlePtrs.end(),
                  [&p1](ParticleInfo* elem) { return elem->weight > p1->weight; });

        if (pos !=particlePtrs.end()) {
          particlePtrs.emplace(pos,std::move(p1));
          particlePtrs.emplace(pos,std::move(p2));
        }
        else {
          particlePtrs.emplace_back(std::move(p1));
          particlePtrs.emplace_back(std::move(p2)); 
        }

        // Update bin size
        binNode.binSize--;
        nTotalActiveParticles--;

        // Check if the bin needs to be repositioned in the list
        if (binNode.binSize < nextBinSize) {
          // Remove current bin from its position
          auto currentBinCopy = binNode;
		      binList.pop_front();

          // Find the appropriate position to insert the current bin
          auto insertIt = binList.begin();
          
          while (insertIt != binList.end() && insertIt->binSize > currentBinCopy.binSize) {
            ++insertIt;
          }

          // Insert the current bin at the correct position
          if (insertIt != binList.end()) { 
            currentBinIt = binList.emplace(insertIt, std::move(currentBinCopy));
          }
          else {
            binList.emplace_back(std::move(currentBinCopy)); 
          }

          // Continue with the new most populated bin
          currentBinIt = binList.begin();
		      nextBinSize = (std::next(currentBinIt)!=binList.end()) ? std::next(currentBinIt)->binSize : -1; 
          break;
        }

        // If the bin still has enough particles, continue merging within it
      }
    }

    // If the merging was successful, exit the while loop
    // Decrease nBinsPerDimension and retry
    nBinsPerDimension = nBinsPerDimension / 2;
    
    if (nBinsPerDimension < minBinsPerDimension) {
      break;
    }     
  }
}


//===============================================================================================
/*
    // Particle Splitting Procedure (Detailed Explanation)
    //
    // This procedure splits two initial particles (with masses w1 and w2, and velocities v1 and v2)
    // into three particles. The resulting three-particle configuration conserves total mass, total 
    // momentum, and total kinetic energy when a specific scaling factor is used.
    //
    // Steps:
    //
    // 1. Compute Total Mass and Center-of-Mass (CM) Velocity:
    //    Given two particles of mass w1, w2 and velocities v1, v2:
    //      - Total mass: W_total = w1 + w2
    //      - Center-of-mass velocity: vCM = (w1*v1 + w2*v2) / (w1 + w2)
    //
    //    The center-of-mass frame simplifies the analysis. In the CM frame, the total momentum is zero.
    //
    // 2. Compute Relative Velocities in the CM Frame:
    //    Transform to the CM frame by defining:
    //      u1 = v1 - vCM
    //      u2 = v2 - vCM
    //
    //    In this frame, the initial kinetic energy can be broken down into a "relative" part 
    //    that depends on u1 and u2.
    //
    // 3. Mass Splitting:
    //    We want to produce three particles with masses:
    //      w1' = (2/3)*w1
    //      w2' = (2/3)*w2
    //      w3  = (1/3)*(w1 + w2)
    //
    //    This ensures total mass is conserved:
    //      w1' + w2' + w3 = w1 + w2
    //
    //    At this point we have assigned the masses, but not the final velocities. Our goal is to choose 
    //    final velocities that also conserve momentum and energy.
    //
    // 4. Ensuring Momentum Conservation:
    //    We choose the final configuration such that the third particle is stationary in the CM frame:
    //      u3' = 0
    //
    //    Thus, the final CM velocities become:
    //      u1' = α * u1
    //      u2' = α * u2
    //      u3' = 0
    //
    //    We will determine α to ensure energy conservation. The choice of u3' = 0 simplifies momentum 
    //    conservation since w1'u1' + w2'u2' + w3*u3' must be zero. Given w1'u1 + w2'u2 = constant 
    //    and originally equals zero in the CM frame, scaling both u1 and u2 by the same factor α 
    //    keeps that relationship intact (because u3' = 0).
    //
    // 5. Energy Conservation:
    //    The initial relative kinetic energy in the CM frame is:
    //      E_rel_initial = 0.5*(w1*u1² + w2*u2²)
    //
    //    After splitting:
    //      E_rel_final = 0.5*(w1'*u1'² + w2'*u2'² + w3*u3'²)
    //                   = 0.5*(w1'*(α² u1²) + w2'*(α² u2²))
    //    Since u3' = 0, there's no contribution from the third particle here.
    //
    //    Substitute w1' = (2/3)*w1 and w2' = (2/3)*w2:
    //      E_rel_final = 0.5 * α² * (2/3) * (w1*u1² + w2*u2²)
    //                    = α² * (2/3) * E_rel_initial
    //
    //    For energy conservation, E_rel_final = E_rel_initial, hence:
    //      α² * (2/3) = 1  =>  α² = 3/2  =>  α = sqrt(3/2)
    //
    // 6. Applying the Scaling:
    //    Once α is known, we scale:
    //      u1' = sqrt(3/2)*u1
    //      u2' = sqrt(3/2)*u2
    //
    //    and set:
    //      u3' = 0
    //
    //    Converting back to the lab frame by adding vCM:
    //      v1' = vCM + u1'
    //      v2' = vCM + u2'
    //      v3' = vCM
    //
    //    Now we have final masses (w1', w2', w3) and final velocities (v1', v2', v3') that conserve 
    //    total mass, total momentum, and total kinetic energy.
    //
    // Summary:
    //    - Split the mass into w1' = 2/3 w1, w2' = 2/3 w2, and w3 = (w1 + w2)/3.
    //    - Shift to CM frame, scale relative velocities by α = sqrt(3/2), set the third particle’s 
    //      relative velocity to zero.
    //    - Transform back to the lab frame.
    //    - This ensures momentum and energy are both conserved.
    //
    // This comment should help in understanding the reasoning behind each step and verifying 
    // that the implementation follows the theory.
*/
void PIC::ParticleSplitting::SplitParticleList(int spec,long int& FirstParticle,int nRequestedParticleNumber) { 
namespace PB = PIC::ParticleBuffer;

  // Structure to hold particle information
  struct ParticleInfo {
    long int index;
    double v[3];
    double weight;
	  bool active;
  };

  // Structure to represent a bin in the linked list
  struct BinNode {
    int binIndex;
    int binSize;
   std::list<ParticleInfo*>* particlePtrs;
  };

  // Step 1: Collect all particles of the specified species in the cell
  std::vector<ParticleInfo> particles;
  long int particleIndex = FirstParticle;
  double WeightMin=-1.0,WeightMax=-1.0;
  int nTotalActiveParticles=0;

  // Reserve memory for the particles vector now that we know the total number of particles
  particles.reserve(nRequestedParticleNumber);

  while (particleIndex != -1) {
    if (PB::GetI(particleIndex) == spec) {
      ParticleInfo p;
  
      p.index = particleIndex;
      PB::GetV(p.v, particleIndex);
      p.weight = PB::GetIndividualStatWeightCorrection(particleIndex);
      p.active = true;
      particles.emplace_back(std::move(p));

      if ((WeightMin<0.0)||(WeightMin>p.weight)) WeightMin=p.weight;
      if (WeightMax<p.weight) WeightMax=p.weight;

	    // Early exit if no splitting is needed
      if (++nTotalActiveParticles>=nRequestedParticleNumber) return;
    }
  
    particleIndex = PB::GetNext(particleIndex);
  }

  // Set the minimum value of the particle weigh below which no splitting is done
  double SplitWeightThrehold=1.0E-4*WeightMax;

  // Step 2: Determine velocity range (calculate vMin and vMax once)
  double vMin[3] = {particles[0].v[0], particles[0].v[1], particles[0].v[2]};
  double vMax[3] = {particles[0].v[0], particles[0].v[1], particles[0].v[2]};
 
  for (const auto &p : particles) {
    for (int d = 0; d < 3; ++d) {
      if (p.v[d] < vMin[d]) vMin[d] = p.v[d];
      if (p.v[d] > vMax[d]) vMax[d] = p.v[d];
    }
  }

  // Add safety margin to vMin and vMax
  for (int d = 0; d < 3; ++d) {
    double velocityRange = vMax[d] - vMin[d];
    double safetyMargin = 0.01 * velocityRange; // 1% of the velocity range
  
    vMin[d] -= safetyMargin;
    vMax[d] += safetyMargin;
  }

  // Define initial number of bins per dimension
  int nBinsPerDimension = 10; // Starting value; adjust as needed
  const int minBinsPerDimension = 6; // Minimum allowed value

  while (nBinsPerDimension >= minBinsPerDimension && nTotalActiveParticles < nRequestedParticleNumber ) {
    // Step 3: Define velocity bins using vMin and vMax with safety margin
    double dv[3];
  
    for (int d = 0; d < 3; ++d) {
      dv[d] = (vMax[d] - vMin[d]) / nBinsPerDimension;
      if (dv[d] == 0.0) dv[d] = 1e-6; // Prevent division by zero
    }

    // Compute total number of bins
    int totalBins = nBinsPerDimension * nBinsPerDimension * nBinsPerDimension;
  
    // Create bins: a vector of lists 
    std::vector<std::list<ParticleInfo*>> bins(totalBins);

    // Step 4: Assign particles to bins using flat indices
    for (auto &p : particles) if (p.active==true) {
      int binIndex[3];

      if (p.weight<SplitWeightThrehold) continue;

      for (int d = 0; d < 3; ++d) {
        binIndex[d] = static_cast<int>((p.v[d] - vMin[d]) / dv[d]);
        if (binIndex[d] >= nBinsPerDimension) binIndex[d] = nBinsPerDimension - 1;
        if (binIndex[d] < 0) binIndex[d] = 0; // Clamp to 0
      }
	    
      int flatIndex = binIndex[0]
                    + nBinsPerDimension * (binIndex[1]
                    + nBinsPerDimension * binIndex[2]);

      bins[flatIndex].push_back(&p);
    }

    // Step 5: Sort particles within each bin by descending weight
    for (auto &bin : bins) {
      if (bin.size() >= 3) {
        bin.sort([](const ParticleInfo* a, const ParticleInfo* b) {
          return a->weight > b->weight;
        });
      }
    }

    // Step 6: Create a sorted linked list of bins sorted by bin size ascending
    std::list<BinNode> binList;

    for (int i = 0; i < totalBins; ++i) {
      int binSize = bins[i].size();

      if (binSize >= 2) {
        BinNode node = {i, binSize, &bins[i]};
        binList.emplace_back(std::move(node));
      }
    }

    // Sort the binList in descending order of binSize
    binList.sort([](const BinNode& a, const BinNode& b) {
      return a.binSize < b.binSize;
    });

    // Step 7: Split particles in bins while conserving weight, momentum, and energy
    auto currentBinIt = binList.begin();
    int nextBinSize=-1; 

    if (std::next(currentBinIt)!=binList.end()) nextBinSize=std::next(currentBinIt)->binSize;

    while (!binList.empty() && nTotalActiveParticles < nRequestedParticleNumber && currentBinIt != binList.end()) {
      BinNode& binNode = *currentBinIt;
      std::list<ParticleInfo*>& particlePtrs = *(binNode.particlePtrs);

      if (particlePtrs.size()<2) {
        //There is not more bins with the number of particles > 3 => break the current loop and repeat with smaller number if bins
        break;
      }

      // While there are at least three particles to merge in the bin
      while (particlePtrs.size() >= 2 && nTotalActiveParticles < nRequestedParticleNumber) {
        // Select the first three particles (smallest weights)
        auto it1 = particlePtrs.begin();
        auto it2 = std::next(it1);
                	
        ParticleInfo* p1 = *it1;
        ParticleInfo* p2 = *it2;   
        ParticleInfo p3;
                
        // Compute the center of mass velocity
        double *v1=p1->v,*v2=p2->v,*v3=p3.v;
        double w1=p1->weight,w2=p2->weight; 
        double w1p=2.0/3.0*w1,w2p=2.0/3.0*w2,w3p=1.0/3.0*(w1+w2);
        double vCM[3],u1[3],u2[3],momentum_total[3];
  		  double W_total=w1+w2;

        Vector3D::AddWeighted(momentum_total,w1,v1,w2,v2);
        for (int d=0;d<3;d++) vCM[d]=momentum_total[d]/W_total;

        if (2.0*Vector3D::Length(vCM)/(Vector3D::Length(v1)+Vector3D::Length(v2))<0.2) {
          //cannot do splitting -> the resulting particle has a too small velocity 
          particlePtrs.pop_front();
          continue;
        }

        //particles can be split -> remove them from particlePtrs
        particlePtrs.erase(it1,std::next(it2));

        //energy_total=0.5*(w1*Vector3D::DotProduct(v1,v1)+w2*Vector3D::DotProduct(v2,v2));
        for (int d=0;d<3;d++) v3[d]=vCM[d]; 

        //scaling factor  
        double alpha=sqrt(3.0/2.0);

        Vector3D::Substract(u1,v1,vCM);
        Vector3D::Substract(u2,v2,vCM);

        Vector3D::MultiplyScalar(alpha,u1);
        Vector3D::MultiplyScalar(alpha,u2);

        Vector3D::Add(v1,u1,vCM);
        Vector3D::Add(v2,u2,vCM);

        // Update weight ang velocity in the first two particles 
        // Particle A
        p1->weight=w1p;

        auto* dataA = PB::GetParticleDataPointer(p1->index);
        PB::SetV(p1->v,dataA);
        PB::SetIndividualStatWeightCorrection(p1->weight,dataA);

        // Particle B
        p2->weight=w2p; 

        auto* dataB = PB::GetParticleDataPointer(p2->index);
        PB::SetV(p2->v,dataB);
        PB::SetIndividualStatWeightCorrection(p2->weight,dataB);

        // Create Particle C
        p3.weight=w3p;

        long int newPtr=PB::GetNewParticle(FirstParticle);
        auto* dataC = PB::GetParticleDataPointer(newPtr);
        PB::CloneParticle(dataC,dataA);

        p3.index=newPtr;
        PB::SetV(p3.v,dataC);
        PB::SetIndividualStatWeightCorrection(w3p,dataC);

	      //Place p1 anps p2 in the appropriate locations in particlePtrs 
        auto InsertParticlePtr = [&](ParticleInfo* p) {
          auto pos = std::find_if(particlePtrs.begin(), particlePtrs.end(),
                     [&p](const ParticleInfo* elem) { 
                        return elem->weight < p->weight; 
                     });
    
          if (pos != particlePtrs.end()) {
            particlePtrs.emplace(pos, std::move(p));
          } else {
            particlePtrs.emplace_back(std::move(p));
          }
        };

        InsertParticlePtr(p1);
        InsertParticlePtr(p2);

        //Place p3 in the appropriate locations in particlePtrs 
        particles.emplace_back(std::move(p3));
        ParticleInfo* p3_ptr=particles.data()+particles.size()-1;
        InsertParticlePtr(p3_ptr);              

        // Update bin size
        binNode.binSize++;
    		nTotalActiveParticles++;

        // Check if the bin needs to be repositioned in the list
		    if (binNode.binSize > nextBinSize) {
          // Remove current bin from its position
          auto currentBinCopy = binNode;
		      binList.pop_front();

          // Find the appropriate position to insert the current bin
          auto insertIt = binList.begin();
          
          while (insertIt != binList.end() && insertIt->binSize < currentBinCopy.binSize) {
            ++insertIt;
          }

          // Insert the current bin at the correct position
		      if (insertIt != binList.end()) { 
            currentBinIt = binList.emplace(insertIt, std::move(currentBinCopy));
		      }
		      else {
            binList.emplace_back(std::move(currentBinCopy)); 
		      }

          // Continue with the new most populated bin
          currentBinIt = binList.begin();
  		    nextBinSize = (std::next(currentBinIt)!=binList.end()) ? std::next(currentBinIt)->binSize : -1; 
          break;
        }
      // If the bin still has enough particles, continue merging within it
      }
	  }

    // If the merging was successful, exit the while loop
    // Decrease nBinsPerDimension and retry
    nBinsPerDimension -= 2;
    if (nBinsPerDimension < minBinsPerDimension) {
      break;
    }      
  }
}


