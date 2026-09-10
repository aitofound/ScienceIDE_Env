//$Id$

//implementation of methods in namespace FieldLine

#include "pic.h"

//static variables of class cFieldLineVertex
int PIC::FieldLine::cFieldLineVertex::totalAssociatedDataLength=-1;
int PIC::FieldLine::cFieldLineVertex::sampleDataLength=-1;
int PIC::FieldLine::cFieldLineVertex::CollectingSamplingOffset=-1;
int PIC::FieldLine::cFieldLineVertex::CompletedSamplingOffset=-1;

int PIC::FieldLine::cFieldLineSegment::totalAssociatedDataLength=-1;
int PIC::FieldLine::cFieldLineSegment::sampleDataLength=-1;
int PIC::FieldLine::cFieldLineSegment::CollectingSamplingOffset=-1;
int PIC::FieldLine::cFieldLineSegment::CompletedSamplingOffset=-1;
vector<PIC::Datum::cDatumStored*> PIC::FieldLine::cFieldLineSegment::DataStoredAtSegment;
vector<PIC::Datum::cDatumSampled*> PIC::FieldLine::cFieldLineSegment::DataSampledAtSegment;

//hooks for calculating the magnertic tube radius and the volume of the field line segment
PIC::FieldLine::fMagneticTubeRadius PIC::FieldLine::MagneticTubeRadius=NULL;
PIC::FieldLine::fSegmentVolume PIC::FieldLine::SegmentVolume=NULL;

//process data associated with a filed line before it is saved in a file
PIC::FieldLine::fUserDefinedfDataProcessingManager PIC::FieldLine::UserDefinedfDataProcessingManager=NULL;

//the following is used to output the distance from the beginning of the 
//field line in units other than SI
//first -> the conversion factor
//second -> the string contains the unit symbol
std::pair<double,string> PIC::FieldLine::cFieldLine::OutputLengthConversionFactor(1.0,"");

//user-defined function that defiens an title that is printed in the Tecplot output file (e.g., simulation time of the file)
PIC::FieldLine::fUserDefinedTecplotFileTitle PIC::FieldLine::UserDefinedTecplotFileTitle=NULL; 

//sample cycle counter 
int PIC::FieldLine::SampleCycleCounter=0;

//offset of the field line related data in a particle state vector
int PIC::FieldLine::ParticleDataOffset=-1; 

      void PIC::FieldLine::cFieldLineSegment::DeleteAttachedParticles() {
        if (_PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_) {
          long int ptr_next,ptr=FirstParticleIndex;

          while (ptr!=-1) {
            ptr_next=PIC::ParticleBuffer::GetNext(ptr);
            PIC::ParticleBuffer::DeleteParticle(ptr);
            ptr=ptr_next;
          }

          FirstParticleIndex=-1;
        }
      }


namespace PIC {
  namespace FieldLine {

    cDatumStored DatumAtVertexElectricField(3,"\"Ex [V/m]\",\"Ey [V/m]\",\"Ez [V/m]\",",false);
    cDatumStored DatumAtVertexMagneticField(3,"\"Bx [nT]\",\"By [nT]\",\"Bz [nT]\",",true);
    cDatumStored DatumAtVertexPlasmaVelocity(3,"\"Plasma Vx [m/s]\",\"Plasma Vy [m/s]\",\"Plasma Vz [m/s]\"",true);
    cDatumStored DatumAtVertexPlasmaDensity(1,"\"Plasma number density [1/m^3]\"", true);
    cDatumStored DatumAtVertexPlasmaTemperature(1,"\"Plasma Temperature [K]\"", true);
    cDatumStored DatumAtVertexPlasmaPressure(1,"\"Plasma pressure [Pa]\"", true);
    cDatumStored DatumAtVertexMagneticFluxFunction(1,"\"MagneticFluxFunction [nT*m]\"", true);
    cDatumStored DatumAtVertexPlasmaWaves(2,"\"Wave1\",\"Wave2\"",true);
    cDatumStored DatumAtVertexShockLocation(1,"\"Shock Location\"",true);

    cDatumStored DatumAtVertexPrevious::DatumAtVertexElectricField(3,"\"Ex [V/m]\",\"Ey [V/m]\",\"Ez [V/m]\",",false);
    cDatumStored DatumAtVertexPrevious::DatumAtVertexMagneticField(3,"\"Bx [nT]\",\"By [nT]\",\"Bz [nT]\",",false);
    cDatumStored DatumAtVertexPrevious::DatumAtVertexPlasmaVelocity(3,"\"Plasma Vx [m/s]\",\"Plasma Vy [m/s]\",\"Plasma Vz [m/s]\"",false);
    cDatumStored DatumAtVertexPrevious::DatumAtVertexPlasmaDensity(1,"\"Plasma number density [1/m^3]\"", false);
    cDatumStored DatumAtVertexPrevious::DatumAtVertexPlasmaTemperature(1,"\"Plasma Temperature [K]\"", false);
    cDatumStored DatumAtVertexPrevious::DatumAtVertexPlasmaPressure(1,"\"Plasma pressure [Pa]\"", false);
    cDatumStored DatumAtVertexPrevious::DatumAtVertexPlasmaWaves(2,"\"Wave1\",\"Wave2\"",false);

    cDatumStored DatumAtVertexFluence(1,"\"Fluence\"", true); 
    cDatumStored DatumAtVertexShockLocationDistance(1,"\"Distance To Shock [AU]\"",true);

    cDatumTimed DatumAtVertexParticleWeight(1,"\"Particle Weight\"",false);
    cDatumTimed DatumAtVertexParticleNumber(1,"\"Particle Number\"",true);
    cDatumTimed DatumAtVertexNumberDensity(1,"\"Number Density[1/m^3]\"",true);
    cDatumWeighted DatumAtVertexParticleEnergy(1,"\"Kinetic energy [MeV]\"",true);
    cDatumWeighted DatumAtVertexParticleSpeed(1,"\"Speed [m/s]\"",true);

    cDatumTimed DatumAtVertexNumberDensityEnergyBinned("Number Density[1/m^3]",10,0.1,600.0,PIC::Datum::cDatum::DistTypeLog); // units for the energy limits are MeV 

    cDatumWeighted DatumAtVertexParticleCosPitchAngle(1,"\"Cos Pitch Angle\"",true);
    cDatumWeighted DatumAtVertexParticleAbsCosPitchAngle(1,"\"Abs Cos Pitch Angle\"",true);

    cDatumTimed DatumAtVertexNumberDensity_mu_positive(1,"\"Number Density (mu positive) [1/m^3]\"",true); 
    cDatumTimed DatumAtVertexNumberDensity_mu_negative(1,"\"Number Density (mu negative) [1/m^3]\"",true);

    cDatumTimed DatumAtVertexParticleFlux_mu_positive(1,"\"Particle Flux (mu positive) [1/m^3]\"",true);
    cDatumTimed DatumAtVertexParticleFlux_mu_negative(1,"\"Particle Flux (mu negative) [1/m^3]\"",true);

    cDatumWeighted DatumAtGridParticleEnergy(1,"\"Kinetic energy [J]\"",true);

    cVertexAllocationManager VertexAllocationManager;

    vector<cDatumStored*> DataStoredAtVertex;
    vector<cDatumSampled*> DataSampledAtVertex;

    cAssociatedDataAMRstack<cFieldLineVertex>  VerticesAll;
    cAssociatedDataAMRstack<cFieldLineSegment> SegmentsAll;
    cFieldLine *FieldLinesAll = NULL;
    
    long int nFieldLine=0;

    double TimeLastUpdate = -1;

    //=========================================================================
    bool cFieldLine::is_broken() {
      int count;
      cFieldLineSegment* Segment;
      //  cFieldLineVertex*  Vertex; 
      
      //check connectivity forward
      count = 1, Segment = FirstSegment;//, Vertex  = FirstVertex;

      for (int iSegment=0; iSegment<nSegment; iSegment++) {
        if (Segment->GetNext()==NULL) break; //|| Vertex->GetNext()==NULL) break;

        Segment = Segment->GetNext();
        //    Vertex  = Vertex->GetNext();

        count++;
      }
      
      if ((count<nSegment) || (Segment != LastSegment)) return true; // || Vertex != LastVertex)

      //check connectivity backward
      count = 1, Segment = LastSegment;//, Vertex  = LastVertex;

      for (int iSegment=0; iSegment<nSegment; iSegment++) {
	      if (Segment->GetPrev()==NULL) break; // || Vertex->GetPrev()==NULL) break;

	      Segment = Segment->GetPrev();
	      //    Vertex  = Vertex->GetPrev();

	      count++;
      }
      
      if ((count<nSegment) || (Segment != FirstSegment)) return true; // || Vertex != FirstVertex)

      //the line is fully connected
      return false;
    }
    
    //delete the filed line
    void  cFieldLine::Delete() {
      //delete vetexes
      cFieldLineVertex *v,*v_next; 
      
      for (v=FirstVertex;v!=NULL;v=v_next) {
        v_next=v->GetNext();
        VerticesAll.deleteElement(v);
      } 

      FirstVertex=NULL,LastVertex=NULL;

      //delete segments 
      cFieldLineSegment *s,*s_next;

      for (s=FirstSegment;s!=NULL;s=s_next) {
        s_next=s->GetNext();

        //delete particles that attached to the segment 
        s->DeleteAttachedParticles();

        //delete the segment
        SegmentsAll.deleteElement(s);
      }  

      nSegment=0; 
      FirstSegment=NULL,LastSegment=NULL;
      TotalLength=0.0,IsSet=0;
      SegmentPointerTable.clear();
    }

    //=========================================================================
    cFieldLineVertex* cFieldLine::AddFront(double *xIn) {
      if (nSegment==0) {
        return Add(xIn); 
      }
      else {
        cFieldLineVertex* newVertex;
        cFieldLineSegment* newSegment;

        //allocate the new vertex
        newVertex = VerticesAll.newElement();
        newVertex->SetX(xIn);

        //link the list of vertexes
        newVertex->SetNext(FirstVertex);
        newVertex->SetPrev(NULL);
        FirstVertex->SetPrev(newVertex);

        //allocate a new segement  
        newSegment=SegmentsAll.newElement();
        newSegment->SetVertices(newVertex,FirstVertex);

        newSegment->SetPrev(NULL);
        newSegment->SetNext(FirstSegment);

        FirstSegment->SetPrev(newSegment);

        FirstVertex=newVertex;
        FirstSegment=newSegment; 

        SegmentPointerTable.insert(SegmentPointerTable.begin(),FirstSegment);        

        nSegment++; 
      }

      return FirstVertex;
    } 

    cFieldLineVertex* cFieldLine::AddBack(double *xIn) {
       return Add(xIn);
    }

    cFieldLineVertex* cFieldLine::Add(double *xIn) {

      if (PIC::ParticleBuffer::OptionalParticleFieldAllocationManager.MomentumParallelNormal==false) {
        exit(__LINE__,__FILE__,"Error: when field line module is used, PIC::ParticleBuffer::OptionalParticleFieldAllocationManager.MomentumParallelNormal has to be set to TRUE");
      }

      // check if field lineis unset
      if (IsSet == 0) {
        if (FirstVertex==NULL) {
          //allocate the first vertex
          LastVertex = (FirstVertex = VerticesAll.newElement());
          FirstVertex->SetX(xIn);

          //the first segment can't be allocated yet
          nSegment = 0; TotalLength = 0.0; IsSet = 0;
        }
        else{
         //allocate the second vertex
         LastVertex = VerticesAll.newElement();
         LastVertex->SetX(xIn);
         LastVertex->SetPrev(FirstVertex);
         FirstVertex->SetNext(LastVertex);

         //allocate the first segment
         LastSegment = (FirstSegment = SegmentsAll.newElement());
         LastSegment->SetVertices(LastVertex->GetPrev(), LastVertex);
         nSegment++; TotalLength+= LastSegment->GetLength(); IsSet = 1;

         SegmentPointerTable.push_back(FirstSegment);
       }

       return LastVertex;
     }
      
     //allocate vertex
     cFieldLineVertex* newVertex;
     newVertex = VerticesAll.newElement();
     newVertex->SetX(xIn);
      
     //connect it to the last vertex in the field line
     LastVertex->SetNext(newVertex);
     newVertex->SetPrev(LastVertex);
      
     //now the new vertex is the last one
     LastVertex = newVertex;
      
     //allocate segment
     cFieldLineSegment* newSegment;
     newSegment = SegmentsAll.newElement();
     newSegment->SetVertices(LastVertex->GetPrev(), LastVertex);
      
     //connect it to the last segment in the line
     LastSegment->SetNext(newSegment);
     newSegment->SetPrev(LastSegment);
      
     //now the new segment is the last one
     LastSegment = newSegment;
      
     //update house-keeping data
     nSegment++;
     TotalLength += LastSegment->GetLength();

     SegmentPointerTable.push_back(LastSegment);

     return LastVertex;
   }

   void cFieldLine::CutFront(int nDeletedSegments) {

     auto RemoveSingleSegment = [&] (cFieldLineSegment* s) {
       if ((s==FirstSegment)&&(s==LastSegment)) {
         //delete particles that attached to the segment 
         s->DeleteAttachedParticles();

         //remove the segment 
         VerticesAll.deleteElement(FirstVertex);
         VerticesAll.deleteElement(LastVertex);
         SegmentsAll.deleteElement(s);

         FirstVertex=NULL,LastVertex=NULL;
         FirstSegment=NULL,LastSegment=NULL;
 
         nSegment=0,TotalLength=0.0,IsSet=0;
         SegmentPointerTable.clear();
       }
       else {
         //remove segment at the beginning of the line
         TotalLength-=s->GetLength();
         nSegment--;

         //delete particles that attached to the segment
         s->DeleteAttachedParticles();

         auto next_vertex=FirstVertex->GetNext();
         VerticesAll.deleteElement(FirstVertex);
         FirstVertex=next_vertex;
         FirstVertex->SetPrev(NULL);

         FirstSegment=FirstSegment->GetNext();
         SegmentsAll.deleteElement(s);

         FirstSegment->SetPrev(NULL);
         SegmentPointerTable.erase(SegmentPointerTable.begin());
       }
     };

     for (int i=0;i<nDeletedSegments;i++) if (nSegment!=0) RemoveSingleSegment(FirstSegment);
   }


   void cFieldLine::CutBack(int nDeletedSegments) {
     auto RemoveSingleSegment = [&] (cFieldLineSegment* s) {
       if ((s==FirstSegment)&&(s==LastSegment)) {
         //delete particles that attached to the segment
         s->DeleteAttachedParticles();
         
         //the line consists only of one segment -> remove all segments of the line
         VerticesAll.deleteElement(FirstVertex);
         VerticesAll.deleteElement(LastVertex);
         SegmentsAll.deleteElement(s);

         FirstVertex=NULL,LastVertex=NULL;
         FirstSegment=NULL,LastSegment=NULL;

         nSegment=0,TotalLength=0.0,IsSet=0;
         SegmentPointerTable.clear();
       }
       else {
         //delete particles that attached to the segment
         s->DeleteAttachedParticles();
         
         //remove segment at the end of the line
         TotalLength-=s->GetLength();
         nSegment--;

         auto prev_vertex=LastVertex->GetPrev();
         VerticesAll.deleteElement(LastVertex);
         LastVertex=prev_vertex;
         LastVertex->SetNext(NULL);

         LastSegment=LastSegment->GetPrev();
         SegmentsAll.deleteElement(s);

         LastSegment->SetNext(NULL);
         SegmentPointerTable.pop_back();
       }
     };

     for (int i=0;i<nDeletedSegments;i++) if (nSegment!=0) RemoveSingleSegment(LastSegment);
   }




    //=========================================================================
    void cFieldLine::ResetSegmentWeights() {
      cFieldLineSegment *Segment = FirstSegment;
      double w[PIC::nTotalSpecies];

      //compute weights and normalize them
      for(int spec=0; spec < PIC::nTotalSpecies; spec++) TotalWeight[spec] = 0;

      for(int iSegment=0; iSegment<nSegment; iSegment++) {
        //compute weights
        _FIELDLINE_SEGMENT_WEIGHT_(w, Segment);

        //set segment's weight
        Segment->SetWeight(w);
        for(int spec=0; spec < PIC::nTotalSpecies; spec++) TotalWeight[spec] += w[spec];

        //get the next one
        Segment = Segment->GetNext();
      }
    }

    //=========================================================================
    // choose a random segment on field line and return pointer SegmentOut;
    // since statistical weights may vary, compute a correction factor
    // for a particle to be injected on this segment
    //-----------------------------------------------------------------------
    // choose uniformly a segment, i.e. weight_uniform = 1.0 / nSegment
    void cFieldLine::GetSegmentRandom(int& iSegment,double& WeightCorrectionFactor,int spec) {
      int iSegmentChoice = (int)(nSegment * rnd());

      WeightCorrectionFactor = 1.0;
      iSegment = iSegmentChoice;
      // cycle through segment until get the chosen one
      //      SegmentOut = FirstSegment;
      //      for(int iSegment=0; iSegment < iSegmentChoice; iSegment++)
      //	SegmentOut = SegmentOut->GetNext();

      // SegmentOut now is a pointer to a uniformly chosen segment;
      // find the correction factor: weight_segment / weight_uniform
      //-----------------------------------------------------------------------
      //      WeightCorrectionFactor = 
      //      	SegmentOut->GetWeight(spec) / TotalWeight[spec] * nSegment;
    }
    
    //=========================================================================
    long int InjectParticle(int spec) {
      // this is a wrapper that can call either the default injection procedure
      // or a user-defined procedure
      //      return InjectParticle_default(spec);
      return _PIC_INJECT_PARTICLE_ONTO_FIELD_LINE_(spec);
    }
    
    //=========================================================================
 long int InjectParticle_default(int spec) {
   double WeightCorrection,p[3],v[3];
   int iFieldLine,iSegment;

   iFieldLine = (int)(nFieldLine * rnd());
   FieldLinesAll[iFieldLine].GetSegmentRandom(iSegment,WeightCorrection, spec);

   int q=3;
   double vmin=1e6, vmax=1e6;
   double pvmin = pow(vmin, 1-q), pvmax = pow(vmax, 1-q);
   double r= rnd();
   double absv = pow( (1-r)*pvmin + r*pvmax, 1.0/(1-q));

   double m0=PIC::MolecularData::GetMass(spec);

   switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
   case _PIC_MODE_OFF_:
     Vector3D::Distribution::Uniform(p,absv*m0);
     break;

   case _PIC_MODE_ON_:
     Vector3D::Distribution::Uniform(v,absv);
     Relativistic::Vel2Momentum(p,v,PIC::MolecularData::GetMass(spec));
     break;
   }

   return InjectParticle_default(spec,p,1.0,iFieldLine,iSegment);
}



    long int InjectParticle_default(int spec,double *p,double ParticleWeightCorrectionFactor,int iFieldLine,int iSegment,double sIn) {
      //namespace alias
      namespace PB = PIC::ParticleBuffer;

      // pointer particle to the particle to be injected
      PB::byte ptrData[PB::ParticleDataLength];
      
      //default settings
      PB::SetIndividualStatWeightCorrection(ParticleWeightCorrectionFactor,ptrData);
      PB::SetI(spec, ptrData);
      
      // pick a random field line
      PB::SetFieldLineId(iFieldLine, ptrData);

      // inject particle onto this field line
      double WeightCorrection;
      //    FieldLinesAll[iFieldLine].GetSegmentRandom(iSegment,
      //					       WeightCorrection, spec);
      
      //Inject at the beginning of the field line FOR PARKER SPIRAL
      cFieldLineSegment* Segment=FieldLinesAll[iFieldLine].GetSegment(iSegment);
      double S = (sIn>=0) ? sIn : iSegment + rnd();


      PB::SetFieldLineCoord(S, ptrData);
      double x[3], v[3];

      FieldLinesAll[iFieldLine].GetCartesian(x, S);
      PB::SetX(x, ptrData);

      //verify that the 'x' belongs to the subdomain of the current MPI process
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->findTreeNode(x,NULL);       

      if (_PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_NODE_) {
        if (node==NULL) return -1;
        if (node->block==NULL) return -1;
        if (node->Thread!=PIC::ThisThread) return -1;  
      }	
      
      // fix kintic energy and pitch angle far now
      //    double cosPhi=pow(2,-0.5);
      //    double vpar = 1e6 * cosPhi;
      //    double KinEnergyPerp=5E-17*(1-cosPhi*cosPhi);
      //direction of velocity: INJECT ALONG HE FIELD LINE FOR PARKER SPIRAL
      //    vpar*=(rnd()<0.5)?1:-1;
      
      // inject power law, 
      // see Numerical Studies of the Solar Energetic Particle 
      // Transport and Acceleration, Tenishev et al.
      //int q=3;
      //double vmin=1e6, vmax=1e6;
      //double pvmin = pow(vmin, 1-q), pvmax = pow(vmax, 1-q);
      //double r= rnd();
      //double absv = pow( (1-r)*pvmin + r*pvmax, 1.0/(1-q));

      //direction of velocity: INJECT ALONG HE FIELD LINE FOR PARKER SPIRAL
      //double cosPhi =  1 - 2*rnd();

      //double vpar = absv*cosPhi;
      //double KinEnergyPerp = 9.1E-31 * 0.5 * (absv*absv - vpar*vpar);


      //velocity is paral to the field line
      //Segment->GetDir(v);
      //for (int i=0; i<3; i++) v[i]*=vpar;
      //PB::SetV(v, ptrData);
     

      int idim;
      double vpar[3],vperp[3],l[3],c0=0.0,m0=PIC::MolecularData::GetMass(spec);
      double pPerpAbs,pParAbs,pPerpAbs2=0.0,pParAbs2=0.0;

      Segment->GetDir(l);

      switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
      case _PIC_MODE_OFF_:
        for (c0=0.0,idim=0;idim<3;idim++) {
          v[idim]=p[idim]/m0;
          c0+=p[idim]*l[idim];
        }

        for (pPerpAbs2=0.0,pParAbs2=0.0,idim=0;idim<3;idim++) {
          double t;

          t=p[idim]-c0*l[idim];
          pPerpAbs2+=t*t;

          t=c0*l[idim];
          pParAbs2+=t*t; 
        }

        pPerpAbs=sqrt(pPerpAbs2);
        pParAbs=sqrt(pParAbs2);
        break;

      case _PIC_MODE_ON_:
        ::Relativistic::Momentum2Vel(v,p,m0);
        c0=Vector3D::DotProduct(p,l);

        for (pPerpAbs2=0.0,pParAbs2=0.0,idim=0;idim<3;idim++) {
          double t;

          t=p[idim]-c0*l[idim];
          pPerpAbs2+=t*t;

          t=c0*l[idim];
          pParAbs2+=t*t;
        } 
        
        pPerpAbs=sqrt(pPerpAbs2);
        pParAbs=sqrt(pParAbs2);
        break;
      }


      //PB::SetV(v,ptrData);

      double vParallel,vNormal;

      Vector3D::GetComponents(vParallel,vNormal,v,l);
      PB::SetVParallel(vParallel,ptrData);
      PB::SetVNormal(vNormal,ptrData);

      PB::SetMomentumParallel(pParAbs,ptrData);
      PB::SetMomentumNormal(pPerpAbs,ptrData);
 
      //magnetic field
      double B[3], AbsB;
      Segment->GetMagneticField(S-(int)S, B);
      AbsB = pow(B[0]*B[0]+B[1]*B[1]+B[2]*B[2], 0.5)+1E-15;
      
      //magnetic moment
      double mu=pPerpAbs2/(2.0*m0*AbsB);
      PB::SetMagneticMoment(mu, ptrData);

//      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
//      node=PIC::Mesh::mesh->findTreeNode(x);
      
      //generate a new particles
      long int NewParticle;
      PB::byte* NewParticleData;

      switch (_PIC_PARTICLE_LIST_ATTACHING_) {
      case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
        NewParticle=PB::GetNewParticle(Segment->FirstParticleIndex);
        NewParticleData=PB::GetParticleDataPointer(NewParticle); 

        PB::CloneParticle(NewParticleData,ptrData);
        PB::SetParticleAllocated(NewParticleData);
        break;
      case _PIC_PARTICLE_LIST_ATTACHING_NODE_:
        NewParticle=PB::InitiateParticle(x,NULL,NULL,NULL,ptrData,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
        break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is unknown");
      }  

      return NewParticle;
    }

    //=========================================================================
    void cFieldLine::SetMagneticField(double *BIn, int iVertex){
      cFieldLineVertex *Vertex;
      int nVertex = (is_loop()) ? nSegment : nSegment+1;

      if (iVertex == -1) Vertex = LastVertex;
      else if ((iVertex > 0.5*nSegment) && (iVertex <= nVertex)) {
        Vertex = LastVertex;

        for (int i=nSegment; i>iVertex; i--) Vertex = Vertex->GetPrev();
      }
      else if (iVertex >= 0) {
        Vertex = FirstVertex;

        for (int i=0; i<iVertex; i++) Vertex = Vertex->GetNext();
      }
      else exit(__LINE__, __FILE__, "ERROR: invalid index of vertex");

      Vertex->SetMagneticField(BIn);
    }
    //=========================================================================
    void cFieldLine::GetMagneticField(double* BOut, double S) {
      // check correctness
      if ((S < 0.0) || (S > nSegment))exit(__LINE__,__FILE__,"ERROR: trying to get magnetic field at an invalid location");

      // interpolate the magnetic field at the location S:
      //  floor(S) is the number of the segment,
      //  S - floor(S) is the location along segment (between 0 & 1)
      //-----------------------------------------------------------------------

      // number of the begin vertex
      int iSegment = (int) S;

      cFieldLineSegment* Segment = GetSegment(iSegment);
      Segment->GetMagneticField(S - iSegment, BOut);
    }    

    //=========================================================================
    void cFieldLine::GetPlasmaVelocity(double* VelOut, double S) {
      // check correctness
      if ((S < 0.0) || (S > nSegment))exit(__LINE__,__FILE__,"ERROR: trying to get magnetic field at an invalid location");
    
      // interpolate the magnetic field at the location S:
      //  floor(S) is the number of the segment,
      //  S - floor(S) is the location along segment (between 0 & 1)
      //-----------------------------------------------------------------------
   
      // number of the begin vertex
      int iSegment = (int) S;

      cFieldLineSegment* Segment = GetSegment(iSegment);
      Segment->GetPlasmaVelocity(S - iSegment, VelOut);
    }

    double cFieldLine::GetPlasmaDensity(double S) {
      // check correctness
      if ((S < 0.0) || (S > nSegment))exit(__LINE__,__FILE__,"ERROR: trying to get magnetic field at an invalid location");

      // interpolate the magnetic field at the location S:
      //  floor(S) is the number of the segment,
      //  S - floor(S) is the location along segment (between 0 & 1)
      //-----------------------------------------------------------------------

      // number of the begin vertex
      int iSegment = (int) S;
      double VelOut; 

      cFieldLineSegment* Segment = GetSegment(iSegment);
      Segment->GetPlasmaDensity(S - iSegment, VelOut);

      return VelOut;
    }


    //=========================================================================
    void cFieldLine::Output(FILE* fout, bool OutputGeometryOnly=false) {
      namespace PB = PIC::ParticleBuffer;

      cFieldLineVertex *Vertex=FirstVertex;
      int nVertex = (is_loop()) ? nSegment : nSegment+1;
      int iVertex,iSegment;
      cFieldLineSegment* Segment;

      double *DistanceTable=new double [nVertex]; 

      DistanceTable[0]=0.0;
      
      double LengthConversionFactor=PIC::FieldLine::cFieldLine::OutputLengthConversionFactor.first;

      //calculate length of vertexes from the beginning of the field line 
      for (iSegment=0,Segment=FirstSegment; iSegment<nSegment; iSegment++,Segment=Segment->GetNext()) {
        DistanceTable[iSegment+1]=DistanceTable[iSegment]+Segment->GetLength();
      } 

      // Create mapping from vertices to adjacent segments for edge data averaging
      std::vector<std::vector<cFieldLineSegment*>> vertexToSegmentsMap(nVertex);

      if (!OutputGeometryOnly) {
      // Build the vertex-to-segments mapping
        for (iSegment=0, Segment=FirstSegment; iSegment<nSegment; iSegment++, Segment=Segment->GetNext()) {
          // Each segment connects vertex iSegment to vertex iSegment+1
          vertexToSegmentsMap[iSegment].push_back(Segment);

          if (iSegment+1 < nVertex) {
            vertexToSegmentsMap[iSegment+1].push_back(Segment);
          }

          // Special handling for closed loops
          if (is_loop() && iSegment == nSegment-1) {
            // Last segment connects back to first vertex
            vertexToSegmentsMap[0].push_back(Segment);
          }
        }
      }


      if (OutputGeometryOnly==true) { 
        for (int iVertex=0; iVertex<nVertex; iVertex++) {
          //print coordinates
          double x[DIM];

          Vertex->GetX(x);

          for (int idim=0; idim<DIM; idim++) fprintf(fout, "%e ", LengthConversionFactor*x[idim]);

          fprintf(fout,"%e \n",LengthConversionFactor*DistanceTable[iVertex]);
          Vertex = Vertex->GetNext();
        }

        delete [] DistanceTable;
        return;
      }
      
      int nSegmentParticles[nSegment],nSegmentParticles_mu_negative[nSegment],nSegmentParticles_mu_positive[nSegment];
      double ModelParticleSpeedTable[nSegment],v[3];

      for (iSegment=0;iSegment<nSegment; iSegment++) {
        nSegmentParticles[iSegment]=0,nSegmentParticles_mu_negative[iSegment]=0,nSegmentParticles_mu_positive[iSegment]=0;
        ModelParticleSpeedTable[iSegment]=0.0; 
      }
   

      if (_PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_) {
        for (iSegment=0,Segment=FirstSegment; iSegment<nSegment; iSegment++,Segment=Segment->GetNext()) {
          nSegmentParticles[iSegment]=0; 
          ModelParticleSpeedTable[iSegment]=0.0;
     
          long int ptr=Segment->FirstParticleIndex;

          while (ptr!=-1) {
            nSegmentParticles[iSegment]++;

            //v=PB::GetV(ptr);

            v[0]=PB::GetVParallel(ptr);
            v[1]=PB::GetVNormal(ptr); 
            v[2]=0.0;


            ModelParticleSpeedTable[iSegment]+=Vector3D::Length(v);

            if (v[0]>0.0) {
              nSegmentParticles_mu_positive[iSegment]++;
            }
            else {
              nSegmentParticles_mu_negative[iSegment]++;
            }

            ptr=PB::GetNext(ptr);
          }

          if (nSegmentParticles[iSegment]!=0) ModelParticleSpeedTable[iSegment]/=nSegmentParticles[iSegment];
        }
      }


      for (iVertex=0; iVertex<nVertex; iVertex++) {
        //print coordinates
        double x[DIM];
        double Value[3];
        vector<cDatumStored*>::iterator itrDatumStored;

        Vertex->GetX(x);

        for (int idim=0; idim<DIM; idim++) fprintf(fout, "%e ", LengthConversionFactor*x[idim]);

        fprintf(fout, "%e ", LengthConversionFactor*DistanceTable[iVertex]);

        if (_PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_) {
          if (iVertex==0) {
            fprintf(fout, "%e  %e  %e  %e  %d  ",(double)nSegmentParticles[0],
              (double)nSegmentParticles_mu_positive[0],(double)nSegmentParticles_mu_negative[0],ModelParticleSpeedTable[0],iVertex);
          }
          else if (iVertex==nVertex-1) {
            fprintf(fout, "%e  %e  %e  %e %d  ",(double)nSegmentParticles[iVertex-1],
              (double)nSegmentParticles_mu_positive[iVertex-1],(double)nSegmentParticles_mu_negative[iVertex-1],ModelParticleSpeedTable[iVertex-1],iVertex); 
          }
          else {
            fprintf(fout, "%e  %e  %e  %e  %d  ",0.5*(nSegmentParticles[iVertex-1]+nSegmentParticles[iVertex]), 
              0.5*(nSegmentParticles_mu_positive[iVertex-1]+nSegmentParticles_mu_positive[iVertex]),
              0.5*(nSegmentParticles_mu_negative[iVertex-1]+nSegmentParticles_mu_negative[iVertex]),
              0.5*(ModelParticleSpeedTable[iVertex-1]+ModelParticleSpeedTable[iVertex]),iVertex);
          }
        }

        for (itrDatumStored = DataStoredAtVertex.begin();itrDatumStored!= DataStoredAtVertex.end();  itrDatumStored++) {
          if ((*itrDatumStored)->doPrint) {
            Vertex->GetDatum(*(*itrDatumStored), Value);
            for (int i=0; i<(*itrDatumStored)->length; i++) fprintf(fout, "%e ", Value[i]);
          }
        }

        vector<cDatumSampled*>::iterator itrDatum;

        for(itrDatum = DataSampledAtVertex.begin();itrDatum!= DataSampledAtVertex.end();itrDatum++) if ((*itrDatum)->doPrint) {
          cDatumTimed*    ptrDatumTimed;
          cDatumWeighted* ptrDatumWeighted;

          if ((*itrDatum)->type == PIC::Datum::cDatumSampled::Timed_) {
            ptrDatumTimed = static_cast<cDatumTimed*> ((*itrDatum));
            Vertex->GetDatumAverage(*ptrDatumTimed, Value, 0,SampleCycleCounter);
          }
          else {
            ptrDatumWeighted = static_cast<cDatumWeighted*> ((*itrDatum));
            Vertex->GetDatumAverage(*ptrDatumWeighted, Value, 0);
          }

          for(int i=0; i<(*itrDatum)->length; i++) {
            fprintf(fout, "%e ", Value[i]);
          }
        }

    for (auto it : PIC::FieldLine::cFieldLineSegment::DataStoredAtSegment) {
      if (it->doPrint) {
        // Initialize averaging arrays
        std::vector<double> avgValue(it->length, 0.0);
        double totalWeight = 0.0;
        
        // Average over all segments connected to this vertex
        for (auto segment : vertexToSegmentsMap[iVertex]) {
          double segmentValue[it->length];
          segment->GetDatum(*it, segmentValue);
          
          // Weight by segment length for better averaging
          double weight = segment->GetLength();
          totalWeight += weight;
          
          for (int i = 0; i < it->length; i++) {
            avgValue[i] += weight * segmentValue[i];
          }
        }
        
        // Normalize by total weight
        if (totalWeight > 0.0) {
          for (int i = 0; i < it->length; i++) {
            avgValue[i] /= totalWeight;
            fprintf(fout, "%e ", avgValue[i]);
          }
        } else {
          // No adjacent segments - output zeros
          for (int i = 0; i < it->length; i++) {
            fprintf(fout, "%e ", 0.0);
          }
        }
      }
    }

    // Output segment-sampled data (averaged over adjacent segments)
    for (auto it : PIC::FieldLine::cFieldLineSegment::DataSampledAtSegment) {
      if (it->doPrint) {
        // Initialize averaging arrays
        std::vector<double> avgValue(it->length, 0.0);
        double totalWeight = 0.0;
        
        // Average over all segments connected to this vertex
        for (auto segment : vertexToSegmentsMap[iVertex]) {
          double segmentValue[it->length];
          
          // Handle different sampling types
          if (it->type == PIC::Datum::cDatumSampled::Timed_) {
            cDatumTimed* ptrDatumTimed = static_cast<cDatumTimed*>(it);
            segment->GetDatumAverage(*ptrDatumTimed, segmentValue, 0, SampleCycleCounter);
          } else if (it->type == PIC::Datum::cDatumSampled::Weighted_) {
            cDatumWeighted* ptrDatumWeighted = static_cast<cDatumWeighted*>(it);
            segment->GetDatumAverage(*ptrDatumWeighted, segmentValue, 0);
          } else {
            exit(__LINE__,__FILE__,"Error: not implemented");
          }
          
          // Weight by segment length
          double weight = segment->GetLength();
          totalWeight += weight;
          
          for (int i = 0; i < it->length; i++) {
            avgValue[i] += weight * segmentValue[i];
          }
        }
        
        // Normalize by total weight
        if (totalWeight > 0.0) {
          for (int i = 0; i < it->length; i++) {
            avgValue[i] /= totalWeight;
            fprintf(fout, "%e ", avgValue[i]);
          }
        } else {
          // No adjacent segments - output zeros
          for (int i = 0; i < it->length; i++) {
            fprintf(fout, "%e ", 0.0);
          }
        }
      }
    }


        fprintf(fout,"\n");

        //flush completed sampling buffer
        Vertex->flushCollectingSamplingBuffer();
        Vertex = Vertex->GetNext();
      }

      delete [] DistanceTable;
    }
    
    //=========================================================================
    void Init() {

      static bool init_flag=false;

      if (init_flag==true) return;
      init_flag=true;
      
    //  if(PIC::nTotalThreads > 1) exit(__LINE__, __FILE__,"Not implemented for multiple processors");
      
      // allocate container for field lines
      FieldLinesAll = new cFieldLine [nFieldLineMax];

      //activate segment data storage 
      cFieldLineSegment::InitDatum();

      // activate vertex data storage
      long int Offset = 0;

      // activate data that are stored but NOT sampled
      if (VertexAllocationManager.MagneticField==true)        DatumAtVertexMagneticField.       activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.ElectricField==true)        DatumAtVertexElectricField.       activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PlasmaVelocity==true)       DatumAtVertexPlasmaVelocity.      activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PlasmaDensity==true)        DatumAtVertexPlasmaDensity.       activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PlasmaTemperature==true)    DatumAtVertexPlasmaTemperature.   activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PlasmaPressure==true)       DatumAtVertexPlasmaPressure.      activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.MagneticFluxFunction==true) DatumAtVertexMagneticFluxFunction.activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PlasmaWaves==true)          DatumAtVertexPlasmaWaves.         activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.ShockLocation==true)        DatumAtVertexShockLocation.       activate(Offset, &DataStoredAtVertex);

      if (VertexAllocationManager.PreviousVertexData.MagneticField==true)        DatumAtVertexPrevious::DatumAtVertexMagneticField.       activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PreviousVertexData.ElectricField==true)        DatumAtVertexPrevious::DatumAtVertexElectricField.       activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PreviousVertexData.PlasmaVelocity==true)       DatumAtVertexPrevious::DatumAtVertexPlasmaVelocity.      activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PreviousVertexData.PlasmaDensity==true)        DatumAtVertexPrevious::DatumAtVertexPlasmaDensity.       activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PreviousVertexData.PlasmaTemperature==true)    DatumAtVertexPrevious::DatumAtVertexPlasmaTemperature.   activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PreviousVertexData.PlasmaPressure==true)       DatumAtVertexPrevious::DatumAtVertexPlasmaPressure.      activate(Offset, &DataStoredAtVertex);
      if (VertexAllocationManager.PreviousVertexData.PlasmaWaves==true)          DatumAtVertexPrevious::DatumAtVertexPlasmaWaves.         activate(Offset, &DataStoredAtVertex);

      if (VertexAllocationManager.Fluence==true) DatumAtVertexFluence.activate(Offset, &DataStoredAtVertex);

      if (VertexAllocationManager.DistanceToShockLocation==true) DatumAtVertexShockLocationDistance.activate(Offset, &DataStoredAtVertex);

      // activate data that is sampled
      long int SamplingOffset = Offset;
      Offset = 0;

      DatumAtVertexParticleWeight.  activate(Offset, &DataSampledAtVertex);
      DatumAtVertexParticleNumber.  activate(Offset, &DataSampledAtVertex);
      DatumAtVertexNumberDensity.   activate(Offset, &DataSampledAtVertex);
      DatumAtVertexParticleEnergy.  activate(Offset, &DataSampledAtVertex);
      DatumAtVertexParticleSpeed.   activate(Offset, &DataSampledAtVertex); 

      DatumAtVertexNumberDensityEnergyBinned.   activate(Offset, &DataSampledAtVertex);

      DatumAtVertexParticleCosPitchAngle.   activate(Offset, &DataSampledAtVertex);
      DatumAtVertexParticleAbsCosPitchAngle.activate(Offset, &DataSampledAtVertex);

      DatumAtVertexNumberDensity_mu_positive.activate(Offset, &DataSampledAtVertex);
      DatumAtVertexNumberDensity_mu_negative.activate(Offset, &DataSampledAtVertex);

      DatumAtVertexParticleFlux_mu_positive.activate(Offset, &DataSampledAtVertex);
      DatumAtVertexParticleFlux_mu_negative.activate(Offset, &DataSampledAtVertex);


      // assign offsets and data length
      cFieldLineVertex::SetDataOffsets(SamplingOffset, Offset);
      
      PIC::IndividualModelSampling::DataSampledList.push_back(&DatumAtGridParticleEnergy);

      //request data in the particle state vector to keep the field line-related data
      long int offset;
      int DataLength;

      DataLength=sizeof(cParticleFieldLineData);
      PIC::ParticleBuffer::RequestDataStorage(offset,DataLength);

      ParticleDataOffset=offset;
    }
    
    //=========================================================================
    void Output(const char* fname, bool OutputGeometryOnly) {

      static int lastDataOutputFileNumber=-1;

      if ((OutputGeometryOnly==false)&&(lastDataOutputFileNumber!=DataOutputFileNumber)) {
        //swap sampling offsets
        cFieldLineVertex::swapSamplingBuffers();
	lastDataOutputFileNumber=DataOutputFileNumber;

        PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtVertex(&PIC::FieldLine::DatumAtVertexParticleWeight);
      
        for(auto itrDatum = DataSampledAtVertex.begin();itrDatum!= DataSampledAtVertex.end();itrDatum++) if ((*itrDatum)->doPrint) {
          cDatumTimed*    ptrDatumTimed;
          cDatumWeighted* ptrDatumWeighted;
 
          if ((*itrDatum)->type == PIC::Datum::cDatumSampled::Timed_) {
            ptrDatumTimed = static_cast<cDatumTimed*> ((*itrDatum));
	    PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtVertex(ptrDatumTimed);
          }
          else {
            ptrDatumWeighted = static_cast<cDatumWeighted*> ((*itrDatum));
	    PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtVertex(ptrDatumWeighted);
          }
	}
  
        //execute a user-defined function to processes sampled data before output
        if (UserDefinedfDataProcessingManager!=NULL) UserDefinedfDataProcessingManager();
      }


      FILE* fout;
      fout = fopen(fname,"w");
      
      fprintf(fout, "TITLE=\"Field line geometry\"\n");

      //if no units are defined in OutputLengthConversionFactor, then set the default to [m] 
      if (PIC::FieldLine::cFieldLine::OutputLengthConversionFactor.second=="") {
        PIC::FieldLine::cFieldLine::OutputLengthConversionFactor.second="m";
      }

      auto *units=PIC::FieldLine::cFieldLine::OutputLengthConversionFactor.second.c_str();
      
switch (DIM) {
  case 3:
    {
      fprintf(fout,"VARIABLES=\"x[%s]\",\"y[%s]\",\"z[%s]\",\"Distance from the beginning[%s]\"",units,units,units,units);
      
      vector<cDatumStored*>::iterator itrDatumStored;
      vector<cDatumSampled*>::iterator itrDatum;

      if (OutputGeometryOnly==false) {
        if (_PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_) {
          fprintf(fout,",\"nSegmentParticles\",\"nSegmentParticles_mu_positive\", \"nSegmentParticles_mu_negative\",\"Model Particle Speed\",\"iSegment\"");
        }

        for (itrDatumStored = DataStoredAtVertex.begin();itrDatumStored!= DataStoredAtVertex.end(); itrDatumStored++) {
          if ((*itrDatumStored)->doPrint) (*itrDatumStored)->PrintName(fout);
        }

        for (itrDatum = DataSampledAtVertex.begin(); itrDatum!= DataSampledAtVertex.end(); itrDatum++) {
          if ((*itrDatum)->doPrint) (*itrDatum)->PrintName(fout);
        }

	for (auto it : PIC::FieldLine::cFieldLineSegment::DataStoredAtSegment) {
          if (it->doPrint==true) it->PrintName(fout);  
	}

	for (auto it : PIC::FieldLine::cFieldLineSegment::DataSampledAtSegment) {
          if (it->doPrint==true) it->PrintName(fout);
        }
      }

      fprintf(fout,"\n");

      //output title of the output file if such is defined 
      if (UserDefinedTecplotFileTitle!=NULL) {
        char title[_MAX_STRING_LENGTH_PIC_];

        UserDefinedTecplotFileTitle(title);
        fprintf(fout, "TITLE=\"%s\"\n",title);
      } 
    }
      break;
  default:
      exit(__LINE__,__FILE__,"not implemented");
      break;
}
      
      for (int iFieldLine=0; iFieldLine<nFieldLine; iFieldLine++) {
        fprintf(fout,"ZONE T=\"Field-line %i\" F=POINT\n",iFieldLine);
        FieldLinesAll[iFieldLine].Output(fout, OutputGeometryOnly);
      }
      
      //reset the sanple counpter 
      SampleCycleCounter=0;

      fclose(fout);
    }

    //=========================================================================
    void Sampling() {
      SampleCycleCounter++;

      int iFieldLine,spec;
      PIC::FieldLine::cFieldLineSegment *Segment;
      long int ptr;
      double LocalParticleWeight;

      if (_SIMULATION_PARTICLE_WEIGHT_MODE_ != _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_) {
        exit(__LINE__,__FILE__,"Error: the function is implemented only for the case _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_. Changed input file.");
      }

      /*
      if (DatumAtVertexFluence.is_active()==true) {
        if (_SIMULATION_TIME_STEP_MODE_ != _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_) {
           exit(__LINE__,__FILE__,"Error: the function is implemented only for the case _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_. Change input file."); 
        }
      }
      */


      for (iFieldLine=0;iFieldLine<nFieldLine;iFieldLine++) {
        for (Segment=FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
          ptr=Segment->FirstParticleIndex;

          while (ptr!=-1) {
            spec=PIC::ParticleBuffer::GetI(ptr);

            LocalParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
            LocalParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);

            PIC::FieldLine::Sampling(ptr,LocalParticleWeight,NULL);
            ptr=PIC::ParticleBuffer::GetNext(ptr);
          }
        }
      }
    }

    namespace {
      // ---------------------------------------------------------------------
      // Defensive helpers for field-line vertex sampling.
      //
      // The field-line output samples particle quantities to vertices and later
      // reduces those sampled quantities over MPI.  A single NaN in a sampled
      // diagnostic such as DatumAtVertexParticleEnergy is sticky: subsequent
      // additions keep the value NaN and the failure may only be observed much
      // later in PIC::FieldLine::Parallel::PackAllFieldLinesVertexData().
      //
      // Sampling is diagnostic and must not repair or alter the particle state.
      // Therefore the policy here is: validate every quantity used to form a
      // sampled value; skip the particle for this diagnostic sample if the state
      // is outside the safe numerical range; and cap only the diagnostic speed
      // used in the relativistic energy conversion so that Speed2E() is never
      // evaluated at v >= c.  The mover remains responsible for preventing bad
      // particles from being produced in the first place.
      // ---------------------------------------------------------------------
      inline bool IsSafeFieldLineSamplingValue(double v) {
        const double MaxAbsSamplingValue = 1.0e300;
        return std::isfinite(v) && (std::fabs(v) < MaxAbsSamplingValue);
      }

      inline double ClampFieldLineSamplingPitchAngleCosine(double mu) {
        if (mu >  1.0) return  1.0;
        if (mu < -1.0) return -1.0;
        return mu;
      }

      inline bool ShouldPrintFieldLineSamplingWarning() {
        // Keep diagnostics useful without flooding a parallel run.  The skipped
        // samples are diagnostic-only events; a small number of warnings is
        // enough to identify the failure mode while keeping stdout/stderr usable.
        static int nWarningsPrinted = 0;
        if (nWarningsPrinted < 25) {
          nWarningsPrinted++;
          return true;
        }
        return false;
      }

      inline void PrintFieldLineSamplingWarning(const char* reason,long int ptr,int spec,
          int iFieldLine,double S,double Weight,double vParallel,double vNormal,
          double Speed,double extra0=0.0,double extra1=0.0) {
        if ((_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) && ShouldPrintFieldLineSamplingWarning()) {
          std::cerr << "Warning: skipping field-line vertex particle sample: "
                    << reason
                    << "; rank=" << PIC::ThisThread
                    << "; ptr=" << ptr
                    << "; spec=" << spec
                    << "; field_line=" << iFieldLine
                    << "; s=" << S
                    << "; weight=" << Weight
                    << "; vParallel=" << vParallel
                    << "; vNormal=" << vNormal
                    << "; speed=" << Speed
                    << "; extra0=" << extra0
                    << "; extra1=" << extra1
                    << std::endl;
        }
      }
    }

    void Sampling(long int ptr, double Weight, char* CellSamplingBuffer){
      // namespace alias
      namespace PB = PIC::ParticleBuffer;

      // find the field line and location along it
      int iFieldLine = PB::GetFieldLineId(ptr);
      double TimeStep,S = PB::GetFieldLineCoord(ptr);

      // Validate the field-line coordinate before it is used to access the
      // field-line container.  Invalid field-line coordinates can occur for
      // particles that have just left a field line or for particles whose mover
      // state was corrupted earlier.  Sampling is diagnostic, so the safe action
      // is to skip this particle instead of letting it contaminate the sampled
      // vertex arrays.
      if ((iFieldLine<0)||(iFieldLine>=nFieldLine)||
          (IsSafeFieldLineSamplingValue(S)==false)) {
        PrintFieldLineSamplingWarning("invalid field-line coordinate",ptr,-1,
            iFieldLine,S,Weight,0.0,0.0,0.0);
        return;
      }

      cFieldLineSegment* CurrentSegment=FieldLinesAll[iFieldLine].GetSegment(S);
      if (CurrentSegment==NULL) {
        PrintFieldLineSamplingWarning("null segment for field-line coordinate",ptr,-1,
            iFieldLine,S,Weight,0.0,0.0,0.0);
        return;
      }

      // weights of vertices.  The interpolation weight is a diagnostic weight
      // between the begin and end vertices of the containing segment.  Roundoff
      // near a segment boundary can put the fractional part marginally outside
      // [0,1]; clamp that small numerical overshoot so a valid particle does not
      // generate a negative sampled contribution.
      double w = S - floor(S);
      if (w<0.0) w=0.0;
      if (w>1.0) w=1.0;

      //magnetic field
      double B[3],l[3];

      FieldLinesAll[iFieldLine].GetMagneticField(B,S);
      CurrentSegment->GetDir(l);

      double AbsB = pow(B[0]*B[0]+B[1]*B[1]+B[2]*B[2], 0.5);
      if ((IsSafeFieldLineSamplingValue(B[0])==false)||
          (IsSafeFieldLineSamplingValue(B[1])==false)||
          (IsSafeFieldLineSamplingValue(B[2])==false)||
          (IsSafeFieldLineSamplingValue(l[0])==false)||
          (IsSafeFieldLineSamplingValue(l[1])==false)||
          (IsSafeFieldLineSamplingValue(l[2])==false)||
          (IsSafeFieldLineSamplingValue(AbsB)==false)) {
        PrintFieldLineSamplingWarning("invalid magnetic field or field-line direction",ptr,-1,
            iFieldLine,S,Weight,0.0,0.0,0.0,AbsB);
        return;
      }

      //magnetic moment, mass, velocity and energy
      double mu = PB::GetMagneticMoment(ptr);
      int spec = PB::GetI(ptr);
      double v[3],Speed;
      double x[3];

      if ((spec<0)||(spec>=PIC::nTotalSpecies)) {
        PrintFieldLineSamplingWarning("invalid species index",ptr,spec,
            iFieldLine,S,Weight,0.0,0.0,0.0);
        return;
      }

      if ((IsSafeFieldLineSamplingValue(Weight)==false)||(Weight<=0.0)) {
        PrintFieldLineSamplingWarning("invalid particle statistical weight",ptr,spec,
            iFieldLine,S,Weight,0.0,0.0,0.0);
        return;
      }

      //PB::GetV(v, ptr);

      //time step
      switch (_SIMULATION_TIME_STEP_MODE_) {
      case _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_:
        TimeStep=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
	break;
      case _SINGLE_GLOBAL_TIME_STEP_:
        TimeStep=PIC::ParticleWeightTimeStep::GlobalParticleWeight[0];
        break;
      default:
	exit(__LINE__,__FILE__,"Error: not implemented");
      }

      if (IsSafeFieldLineSamplingValue(TimeStep)==false) {
        PrintFieldLineSamplingWarning("invalid sampling time step",ptr,spec,
            iFieldLine,S,Weight,0.0,0.0,0.0,TimeStep);
        return;
      }

      v[0]=PB::GetVParallel(ptr);
      v[1]=PB::GetVNormal(ptr);
      v[2]=0.0;

      if ((IsSafeFieldLineSamplingValue(v[0])==false)||
          (IsSafeFieldLineSamplingValue(v[1])==false)) {
        PrintFieldLineSamplingWarning("invalid particle velocity components",ptr,spec,
            iFieldLine,S,Weight,v[0],v[1],0.0);
        return;
      }

      Speed=Vector3D::Length(v);

      // Keep the sampler out of the singular point of relativistic energy
      // conversion.  This cap is used only for diagnostic conversion to energy;
      // it does not change the stored particle velocity.  The mover has its own
      // responsibility to keep particles subluminal.
      const double MinFieldLineSamplingSpeed=1.0;
      const double MaxFieldLineSamplingSpeed=(1.0-1.0e-12)*SpeedOfLight;
      if ((IsSafeFieldLineSamplingValue(Speed)==false)||(Speed<MinFieldLineSamplingSpeed)) {
        PrintFieldLineSamplingWarning("invalid or too small particle speed",ptr,spec,
            iFieldLine,S,Weight,v[0],v[1],Speed);
        return;
      }

      double DiagnosticSpeed=Speed;
      if (DiagnosticSpeed>MaxFieldLineSamplingSpeed) DiagnosticSpeed=MaxFieldLineSamplingSpeed;

      double CosPitchAngleSign=(v[0]*Vector3D::DotProduct(B,l)>0.0) ? 1.0 : -1.0;

      PB::GetX(x, ptr);

      double m0= PIC::MolecularData::GetMass(spec);
      if ((IsSafeFieldLineSamplingValue(m0)==false)||(m0<=0.0)) {
        PrintFieldLineSamplingWarning("invalid species mass",ptr,spec,
            iFieldLine,S,Weight,v[0],v[1],Speed,m0);
        return;
      }

      double E=Relativistic::Speed2E(DiagnosticSpeed,m0);
      if ((IsSafeFieldLineSamplingValue(E)==false)||(E<0.0)) {
        PrintFieldLineSamplingWarning("invalid diagnostic kinetic energy",ptr,spec,
            iFieldLine,S,Weight,v[0],v[1],Speed,E);
        return;
      }

      double CosPitchAngle=ClampFieldLineSamplingPitchAngleCosine(v[0]/Speed);

      // volume
      double volume = 0.0;

      if (SegmentVolume!=NULL) {
        volume=SegmentVolume(CurrentSegment,iFieldLine);
      }
      else {
        exit(__LINE__,__FILE__,"Error: hook SegmentVolume is not initialized; provide a function for calculating the volumne of a segment of a field line (e.g., SEP::FieldLine::GetSegmentVolume in srcSEP/shock_injection.cpp");
      }

      if ((IsSafeFieldLineSamplingValue(volume)==false)||(volume<=0.0)) {
        PrintFieldLineSamplingWarning("invalid field-line segment volume",ptr,spec,
            iFieldLine,S,Weight,v[0],v[1],Speed,volume);
        return;
      }


      //sample to vertices
      cFieldLineVertex* V=CurrentSegment->GetBegin();
      cFieldLineVertex* Vnext=(V!=NULL) ? V->GetNext() : NULL;
      if ((V==NULL)||(Vnext==NULL)) {
        PrintFieldLineSamplingWarning("invalid vertex connectivity for sampling",ptr,spec,
            iFieldLine,S,Weight,v[0],v[1],Speed);
        return;
      }

      const double SafeFluxSpeed=Speed;

      // The actual calls below use the guarded cFieldLineVertex::SampleDatum()
      // implementation.  The per-contribution guard in cFieldLineVertex protects
      // every sampled datum from NaN/Inf, while the checks above make sure the
      // quantities used here are physically and numerically well-defined.
      V->SampleDatum(DatumAtVertexNumberDensity,Weight/volume, spec, (1-w));
      V->SampleDatum(DatumAtVertexParticleWeight,Weight,spec, (1-w));
      V->SampleDatum(DatumAtVertexParticleNumber,1.0,spec, (1-w));
      V->SampleDatum(DatumAtVertexParticleEnergy,Weight*E*J2MeV,spec, (1-w));
      V->SampleDatum(DatumAtVertexParticleSpeed,Weight*Speed,spec, (1-w));

      //sample binned number density
      int ibin=DatumAtVertexNumberDensityEnergyBinned.GetBinIndex(E*J2MeV);
      if (ibin>=0) V->SampleDatum(DatumAtVertexNumberDensityEnergyBinned,Weight/volume, spec, ibin,(1-w));

      V->SampleDatum(DatumAtVertexParticleCosPitchAngle,Weight*CosPitchAngle,spec, (1-w));
      V->SampleDatum(DatumAtVertexParticleAbsCosPitchAngle,Weight*fabs(CosPitchAngle),spec, (1-w));

      if (CosPitchAngleSign>0.0) {
        V->SampleDatum(DatumAtVertexNumberDensity_mu_positive,Weight/volume, spec, (1-w));
        V->SampleDatum(DatumAtVertexParticleFlux_mu_positive,Weight*SafeFluxSpeed/volume, spec, (1-w));
      }
      else {
        V->SampleDatum(DatumAtVertexNumberDensity_mu_negative,Weight/volume, spec, (1-w));
        V->SampleDatum(DatumAtVertexParticleFlux_mu_negative,Weight*SafeFluxSpeed/volume, spec, (1-w));
      }

      if (DatumAtVertexFluence.is_active()==true) {
        double t;

        V->GetDatum(DatumAtVertexFluence,t);
        if (IsSafeFieldLineSamplingValue(t)==false) t=0.0;
        t+=Weight/volume*SafeFluxSpeed*(1-w)/volume*TimeStep;
        if (IsSafeFieldLineSamplingValue(t)==true) V->SetDatum(DatumAtVertexFluence,t);
      }


      V = Vnext;
      V->SampleDatum(DatumAtVertexNumberDensity,Weight/volume,spec,  (w));
      V->SampleDatum(DatumAtVertexParticleWeight,Weight,spec, (w));
      V->SampleDatum(DatumAtVertexParticleNumber,1.0,spec, (w));
      V->SampleDatum(DatumAtVertexParticleEnergy,Weight*E*J2MeV,spec, w);
      V->SampleDatum(DatumAtVertexParticleSpeed,Weight*Speed,spec, w);

      if (ibin>=0) V->SampleDatum(DatumAtVertexNumberDensityEnergyBinned,Weight/volume, spec, ibin,w);

      V->SampleDatum(DatumAtVertexParticleCosPitchAngle,Weight*CosPitchAngle,spec, w);
      V->SampleDatum(DatumAtVertexParticleAbsCosPitchAngle,Weight*fabs(CosPitchAngle),spec, w);

      if (CosPitchAngleSign>0.0) {
        V->SampleDatum(DatumAtVertexNumberDensity_mu_positive,Weight/volume, spec, (w));
        V->SampleDatum(DatumAtVertexParticleFlux_mu_positive,Weight*SafeFluxSpeed/volume, spec, (w));
      }
      else {
        V->SampleDatum(DatumAtVertexNumberDensity_mu_negative,Weight/volume, spec, (w));
        V->SampleDatum(DatumAtVertexParticleFlux_mu_negative,Weight*SafeFluxSpeed/volume, spec, (w));
      }

      if (DatumAtVertexFluence.is_active()==true) {
        double t;

        V->GetDatum(DatumAtVertexFluence,t);
        if (IsSafeFieldLineSamplingValue(t)==false) t=0.0;
        t+=Weight/volume*SafeFluxSpeed*(w)/volume*TimeStep;
        if (IsSafeFieldLineSamplingValue(t)==true) V->SetDatum(DatumAtVertexFluence,t);
      }


      //.........................
      if ((CellSamplingBuffer!=NULL)&&(DatumAtGridParticleEnergy.offset>=0)) {
        double* grid_energy=(double*)(CellSamplingBuffer+DatumAtGridParticleEnergy.offset);
        if (IsSafeFieldLineSamplingValue(*grid_energy)==false) *grid_energy=0.0;
        const double dEGrid=Weight*E;
        if (IsSafeFieldLineSamplingValue(dEGrid)==true) {
          const double updated=*grid_energy+dEGrid;
          if (IsSafeFieldLineSamplingValue(updated)==true) *grid_energy=updated;
        }
      }
    }

    //=========================================================================
    void FieldLineWeight_Uniform(double* Weight, cFieldLineSegment* Segment) {
      for(int spec=0; spec<PIC::nTotalSpecies; spec++) Weight[spec] = 1.0;
    }

    //=========================================================================
    void InitLoop2D(double *xStart,  //start loop here
		    double DArc,     //increment in the angle of arc 
		    double DMin,     //min allowed length of the arc
		    double DMax      //max allowed length of the arc
		    ) {
      // in 2D case (e.g. cylindrical symmetry) generate a magnetic field line;
      // magnetic flux function, psi, is utilized:
      // loop is a line of constant value
      //-----------------------------------------------------------------------
      // check correctness
      if (_PIC_SYMMETRY_MODE_ != _PIC_SYMMETRY_MODE__AXIAL_) {
      exit(__LINE__, __FILE__,"ERROR: implemented only for axial symmetry!");
      }

      // check for limit of number of field lines allowed
      if (nFieldLine == nFieldLineMax) exit(__LINE__,__FILE__,"ERROR: reached limit for field line number");

      // mark time of extraction
      TimeLastUpdate = PIC::SimulationTime::Get();

      // list of variables utilized
      //.......................................................................
      // current location
      double x[3] = {xStart[0], xStart[1], xStart[2]};

      // candidate location
      //      double xNew[3] = {0.0, 0.0, 0.0};
      // the first location; used to close the loop
      double xFirst[3] = {xStart[0], xStart[1], xStart[2]};

      // min value resolved for magnetic field (squared)
      const double epsB2 = 1E-30;
      const double epsB  = 1E-15;

      // plasma velocity, stored at vertex
      double V[3] = {0.0, 0.0, 0.0};

      //plasma velocity
      double T = 0;

      // magnetic field vector, unit vector and magnitude
      // NOTE: y-component is ignored!!!
      double B[3] = {0.0, 0.0, 0.0};
      double b[3] = {0.0, 0.0, 0.0};
      double absB =  0.0;

      // flux function
      double Psi0 = 0.0, Psi = 0.0;

      //housekeeping for controlling loop's generation
      // Arc = curvature * Length
      double Arc = 0.0;

      // direction of new segment
      double Dir[3] = {0.0, 0.0, 0.0};

      // angle swiped by the loop so far
      double Angle = 0.0;

      // estimate for the next segment's length
      double Length = 0.5*(DMin + DMax);

      // position and magnetic field and next candidate vertex
      double xNew[3] = {0.0,0.0,0.0};
      double BNew[3] = {0.0,0.0,0.0};
      double bNew[3] = {0.0,0.0,0.0};
      double absBNew;

      // new vertex
      cFieldLineVertex *Vertex;
      //.......................................................................

      //increase counter of field lines
      nFieldLine++;

      //rotate to the y=0 plane
      x[0] = pow(x[0]*x[0] + x[1]*x[1], 0.5) * ( (x[0]>0) ? 1 : -1);
      x[1] = 0.0;

      // get magnetic field at the current location
      CPLR::InitInterpolationStencil(x);
      CPLR::GetBackgroundMagneticField(B);
      absB = pow(B[0]*B[0]+B[2]*B[2],0.5);
      b[0] = B[0]/absB; b[1] = 0; b[2] = B[2]/absB;

      // target value of the flux function
      Psi0 = CPLR::GetBackgroundMagneticFluxFunction();

      // check correctness: need to have non-zero components in x-z plane
      if (B[0]*B[0] + B[2]*B[2] < epsB2) exit(__LINE__,__FILE__, "ERROR: magnetic field magnitude is below min resolved value");
      
      //add the initial vertex
      Vertex = FieldLinesAll[nFieldLine-1].Add(x);

      //      FieldLinesAll[nFieldLine-1].SetMagneticField(B,0);
      Vertex -> SetMagneticField(B);
      Vertex -> SetDatum(DatumAtVertexMagneticFluxFunction, Psi0);
      CPLR::GetBackgroundPlasmaVelocity(V);
      Vertex -> SetDatum(DatumAtVertexPlasmaVelocity, V);
      T = CPLR::GetBackgroundPlasmaTemperature();
      Vertex -> SetDatum(DatumAtVertexPlasmaTemperature, T);	

      //generate the loop
      //......................................................................
      // new vertices are added until:
      // - 2*Pi angle is swiped AND 
      //   candidate vertex is spatially close to the first one => SUCCESS
      // - 4*Pi angle is swiped => FAILURE
      // - exited the domain => FAILURE
      //......................................................................
      // new point is generated in 2 steps
      // 1) make the first guess, x_0, in the direction of B
      // 2) iterate towards point with same value of FLUX via gradient descent
      //......................................................................
      
      while (fabs(Angle) < 4*Pi) {
        // get the first guess
        //------------------------------------------------------------------
        // control inner while loop
        int count;
        const  int countMax = 100;
        count = 0;

        //predictor
        bool DoBreak = false;

        while(true) {
          count++;

          if(count > countMax) exit(__LINE__, __FILE__,"ERROR: can't generate field-line loop ");

          // get new location
          xNew[0] = x[0] + Length * b[0];
          xNew[2] = x[2] + Length * b[2];

          // get magnetic field at this location
          CPLR::InitInterpolationStencil(xNew);
          CPLR::GetBackgroundMagneticField(BNew);
          absBNew = pow(BNew[0]*BNew[0]+BNew[2]*BNew[2],0.5);

          // need to have non-zero field
          if (absBNew < epsB) exit(__LINE__,__FILE__, "ERROR: magnetic field magnitude is below min resolved");

          bNew[0] = BNew[0]/absBNew; bNew[1] = 0; bNew[2] = BNew[2]/absBNew;

          // find angle of arc to the new location
          Arc = fabs(b[0]*bNew[2] - b[2]*bNew[0]);
          if (DoBreak) break;

          //check condition to exit loop
          if ((Arc < 0.5 * DArc) || (Arc > DArc)) {
            // too small or too large step; factor -> 1 as count grows
            // to avoid reaching stationary points and infinite loop
            Length *= 1. + (0.75 * DArc / max(Arc, 0.075*DArc) - 1.) / count;
          }
          else break;

          if (Length < DMin) {Length = DMin; DoBreak = true;}
          if (Length > DMax) {Length = DMax; DoBreak = true;}
        }

        // corrector
        {
          Dir[0] = 0.5*(BNew[0]+B[0]);
          Dir[1] = 0;
          Dir[2] = 0.5*(BNew[2]+B[2]);

          double misc = pow(Dir[0]*Dir[0] + Dir[2]*Dir[2], 0.5);
          Dir[0]/= misc; Dir[2]/= misc;

          //new vertex location
          xNew[0] = x[0] +  Length * Dir[0];
          xNew[2] = x[2] +  Length * Dir[2];
        }
        //------------------------------------------------------------------

        // iterate towards location with needed value of flux function
        //------------------------------------------------------------------
        //get magnetic field at the candidate location
        CPLR::InitInterpolationStencil(xNew);
        CPLR::GetBackgroundMagneticField(BNew);

        // need to have non-zero field
        if(BNew[0]*BNew[0]+BNew[2]*BNew[2] < epsB2) exit(__LINE__,__FILE__,"ERROR: magnetic field magnitude is below min resolved");

        // flux function
        Psi = CPLR::GetBackgroundMagneticFluxFunction();

        // correct the initial guess until flux function is close enough
        // to the original value
        //------------------------------------------------------------------
        while(fabs(2*(Psi-Psi0)/(Psi+Psi0)) > 1E-5) {
          double misc = BNew[0]*BNew[0] + BNew[2]*BNew[2];

          xNew[0]+=-0.001 * (Psi-Psi0) * BNew[2] / misc;
          xNew[2]+= 0.001 * (Psi-Psi0) * BNew[0] / misc;
          CPLR::InitInterpolationStencil(xNew);
          CPLR::GetBackgroundMagneticField(BNew);
          Psi = CPLR::GetBackgroundMagneticFluxFunction();
        }
        //------------------------------------------------------------------

        // next vertex is found;
        // add it to the field line and update information
        absBNew = pow(BNew[0]*BNew[0]+BNew[2]*BNew[2],0.5);
        bNew[0] = BNew[0]/absBNew; bNew[1] = 0; bNew[2] = BNew[2]/absBNew;

        //housekeeping
        Angle += asin(b[0]*bNew[2] - b[2]*bNew[0]);

        //add this vertex
        Vertex = FieldLinesAll[nFieldLine-1].Add(xNew);

        //	FieldLinesAll[nFieldLine-1].SetMagneticField(BNew);
        Vertex -> SetMagneticField(BNew);
        Vertex -> SetDatum(DatumAtVertexMagneticFluxFunction, Psi0);
        CPLR::GetBackgroundPlasmaVelocity(V);
        Vertex -> SetDatum(DatumAtVertexPlasmaVelocity, V);
        T = CPLR::GetBackgroundPlasmaTemperature();
        Vertex -> SetDatum(DatumAtVertexPlasmaTemperature, T);

        //check conditions for completing the loop
        cFieldLineSegment* First=FieldLinesAll[nFieldLine-1].GetFirstSegment();
        cFieldLineSegment* Last =FieldLinesAll[nFieldLine-1].GetLastSegment();

        double Dist = 0.0; //distance between last and first verticies
        double DirFirst[3], DirLast[3];

        First->GetDir(DirFirst); Last->GetDir(DirLast);

        for (int idim=0; idim<DIM; idim++) {
          Dist  += pow(xNew[idim]-xFirst[idim], 2);
        }

        Dist   = pow(Dist, 0.5);

        if ((fabs( 0.5*fabs(Angle)/Pi - 1) < 0.1) && (Dist < Length)) {
          FieldLinesAll[nFieldLine-1].close_loop();
          return;
        }

        //save current location and magnetic field
        x[0] = xNew[0]; x[1] = xNew[1]; x[2] = xNew[2];
        b[0] = bNew[0]; b[1] = bNew[1]; b[2] = bNew[2];
        B[0] = BNew[0]; B[1] = BNew[1]; B[2] = BNew[2];
        absB = absBNew;
      }

      exit(__LINE__, __FILE__,"ERROR: can't generate field-line loop ");
    }

    //=========================================================================
    // update field lines 
    void Update(){
      cFieldLineVertex*  Vertex;
      cFieldLineSegment* Segment;
      double dt = PIC::SimulationTime::Get() - TimeLastUpdate;

      TimeLastUpdate += dt;

      for (int iFieldLine=0; iFieldLine<nFieldLine; iFieldLine++) {
        int cnt = 0;
        bool done = false;

        for (Vertex = FieldLinesAll[iFieldLine].GetFirstVertex();true;Vertex = Vertex->GetNext(), cnt++) {
          double X[3],B[3],V[3];

          done = ((Vertex==FieldLinesAll[iFieldLine].GetLastVertex()) && (cnt!=0)) ? true : false;
          if (done) break;

          Vertex->GetX(X);
          Vertex->GetPlasmaVelocity(V);

          for (int i=0; i<DIM; i++) X[i] += V[i] * dt;

          if (_PIC_SYMMETRY_MODE_ == _PIC_SYMMETRY_MODE__AXIAL_) {
          // rotate to the y=0 plane
          X[0] = pow(X[0]*X[0]+X[1]*X[1], 0.5);
          X[1] = 0.0;
          } //if (_PIC_SYMMETRY_MODE_ == _PIC_SYMMETRY_MODE__AXIAL_)

          if (_PIC_DATAFILE__TIME_INTERPOLATION_MODE_ == _PIC_MODE_ON_) {
           // flux function
          CPLR::InitInterpolationStencil(X);
          CPLR::GetBackgroundMagneticField(B);

          double Psi = CPLR::GetBackgroundMagneticFluxFunction();
          double Psi0;
          Vertex->GetDatum(DatumAtVertexMagneticFluxFunction, Psi0);

          // correct the initial guess until flux function is close enough
          // to the original value
          //------------------------------------------------------------------
          while(fabs(2*(Psi-Psi0)/(Psi+Psi0)) > 1E-5) {
            double misc = B[0]*B[0] + B[2]*B[2];

            X[0]+=-0.001 * (Psi-Psi0) * B[2] / misc;
            X[2]+= 0.001 * (Psi-Psi0) * B[0] / misc;

            CPLR::InitInterpolationStencil(X);
            CPLR::GetBackgroundMagneticField(B);
            Psi = CPLR::GetBackgroundMagneticFluxFunction();
          }
	        //------------------------------------------------------------------
         }//_PIC_DATAFILE__TIME_INTERPOLATION_MODE_ == _PIC_MODE_ON_

          Vertex->SetX(X);

          // update background data
          PIC::CPLR::InitInterpolationStencil(X);
          PIC::CPLR::GetBackgroundMagneticField(B);
          PIC::CPLR::GetBackgroundPlasmaVelocity(V);
          Vertex->SetDatum(DatumAtVertexMagneticField, B);
          Vertex->SetDatum(DatumAtVertexPlasmaVelocity,V);

          double T = CPLR::GetBackgroundPlasmaTemperature();
          Vertex -> SetDatum(DatumAtVertexPlasmaTemperature, T);
	     }

	      cnt = 0; done = false;

	      for (Segment = FieldLinesAll[iFieldLine].GetFirstSegment();!done; Segment = Segment->GetNext(), cnt++) {
	        done = Segment==FieldLinesAll[iFieldLine].GetLastSegment()&&cnt!=0;
	        Segment->SetVertices(Segment->GetBegin(),Segment->GetEnd());
        }
      }
    }

    //=========================================================================
    void InitSimpleParkerSpiral(double *xStart) {
      
if (DIM != 3) {
      exit(__LINE__,__FILE__,"Implemetned only for 3D case");
}

      if(nFieldLine == nFieldLineMax) exit(__LINE__,__FILE__,"ERROR: reached limit for field line number");

      //increase counter of field lines
      nFieldLine++;
      
      //spatial step and # of segments
      const double Ds=1E9;
      int nSegment = 1000;
           
      //Parker spiral starts @ 10 Sun radii
      const double R0 = 10 *_SUN__RADIUS_;

      //magnetic field at this location
      const double B0 = 1.83E-6;

      //velocity of solar wind
      const double SpeedSolarWind = 4.0E5;

      //rate of solar rotation at equator (rad/sec)
      const double Omega = 2.97211E-6;      

      //position of the Sun
      double xSun[DIM]={0.0,0.0,0.0};

      //relative position to Sun
      double x2Sun[DIM]={0.0,0.0,0.0};

      //distance to Sun
      double R2Sun=0;

      //distance to sun in equatorial plane
      double r2Sun=0;

      //find original latitude
      for(int idim=0; idim<DIM; idim++) x2Sun[idim] = xStart[idim] - xSun[idim];
      R2Sun = sqrt(x2Sun[0]*x2Sun[0]+x2Sun[1]*x2Sun[1]+x2Sun[2]*x2Sun[2]);

      //original latitude 
      double costheta = x2Sun[2] / R2Sun;
      double sintheta = sqrt(1 - costheta*costheta); // >=0 always     

      //start position
      double x[DIM]={xStart[0],xStart[1],xStart[2]};


      //if too close to the Sun => move to distance R0
      if (R2Sun < R0) for (int idim=0; idim<DIM; idim++) x[idim] = xSun[idim] + x2Sun[idim] * R0/R2Sun;

      for (int iSegment=0;iSegment<nSegment; iSegment++) {
        //distances to sun
        for(int idim=0; idim<DIM; idim++) x2Sun[idim]=x[idim]-xSun[idim];

        R2Sun = sqrt(x2Sun[0]*x2Sun[0]+x2Sun[1]*x2Sun[1]+x2Sun[2]*x2Sun[2]);
        r2Sun = sqrt(x2Sun[0]*x2Sun[0]+x2Sun[1]*x2Sun[1]);

        //angle in the equatorial plane
        double cosphi = x2Sun[0]/r2Sun, sinphi=x2Sun[1]/r2Sun;

        double misc1 = B0 * pow(R0/R2Sun, 2);
        double misc2 = (R2Sun - R0) * Omega/SpeedSolarWind * sintheta;
      
        //magnetic field at location x
        double B[DIM];

        B[0] = misc1 * (sintheta*cosphi + misc2 * sinphi);
        B[1] = misc1 * (sintheta*sinphi - misc2 * cosphi);
        B[2] = misc1 *  costheta;
      
        //add vertex to the spiral
        FieldLinesAll[nFieldLine-1].Add(x);
        FieldLinesAll[nFieldLine-1].SetMagneticField(B,iSegment);
      
        //magnitude of the magnetic field
        double AbsB = sqrt(B[0]*B[0]+B[1]*B[1]+B[2]*B[2]);

        //direction of the magnetic field
        double b[DIM] = {B[0]/AbsB, B[1]/AbsB, B[2]/AbsB};

        // next vertex of the spiral
        x[0]+=b[0]*Ds; x[1]+=b[1]*Ds; x[2]+=b[2]*Ds;
      }
    }
  }

  //---------------------------------------------------------------------------

namespace Mover{
namespace FieldLine{
  //manager executing the particle moving procedure when the partice lists are attached to the field line segments
  void MoveParticlesMultiThread(int this_thread_id,int thread_id_table_size) {
    namespace FL = PIC::FieldLine;
    namespace PB = PIC::ParticleBuffer;

    long int SegmentCounter=0;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=NULL;
    long int ptr,ptr_next;
    double *x,LocalTimeStep;
    int spec;
    PB::byte* ParticleData;
    FL::cFieldLineSegment* Segment; 

    for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
      for (int i=0;i<FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber();i++) if (++SegmentCounter%thread_id_table_size==this_thread_id) {
        Segment=FL::FieldLinesAll[iFieldLine].SegmentPointerTable[i];

        ptr=Segment->FirstParticleIndex;

        while (ptr!=-1) {
          ptr_next=PB::GetNext(ptr);
 
          ParticleData=PB::GetParticleDataPointer(ptr);

          x=PB::GetX(ParticleData);
          spec=PB::GetI(ParticleData);

          node=PIC::Mesh::mesh->findTreeNode(x,node);
          if (node==NULL) exit(__LINE__,__FILE__,"Error: the point is not found");
          if (node->block==NULL) exit(__LINE__,__FILE__,"Error: the block is not allocated");

          LocalTimeStep=node->block->GetLocalTimeStep(spec);
          _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(ptr,LocalTimeStep,node);

          ptr=ptr_next;
        }
      }
    }
  }


  void MoveParticles () {
    namespace FL = PIC::FieldLine;
    namespace PB = PIC::ParticleBuffer; 
  
    if ((_PIC_PARTICLE_LIST_ATTACHING_!=_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_)||(_PIC_FIELD_LINE_MODE_!=_PIC_MODE_ON_)) {
      exit(__LINE__,__FILE__,"Error: wrong options");
    } 

    if (_PIC_FL_MOVER_MULTITHREAD_== _PIC_MODE_ON_) {
      std::thread tTable[_PIC_NUMBER_STD_THREADS_];

      for (int i=1;i<_PIC_NUMBER_STD_THREADS_;i++) {
        tTable[i]=std::thread(MoveParticlesMultiThread,i,_PIC_NUMBER_STD_THREADS_);
      }

      MoveParticlesMultiThread(0,_PIC_NUMBER_STD_THREADS_);

      for (int i=1;i<_PIC_NUMBER_STD_THREADS_;i++)  tTable[i].join();

      //update particle list indexes 
      for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
        for (auto Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
          Segment->FirstParticleIndex=Segment->tempFirstParticleIndex;
          Segment->tempFirstParticleIndex=-1;
        }
      }

      return;
    }


const int _process_by_particles=0;
const int _process_by_segments=1;

int _process_mode=_process_by_segments;


  auto CountParticle = [&] (FL::cFieldLineSegment* Segment) {
    long int ptr;
    int cnt=0;

    ptr=Segment->FirstParticleIndex;

    while (ptr!=-1) {
      cnt++;
      ptr=PB::GetNext(ptr);
    }

    return cnt;
  };

  auto PopulateParticleTable = [&] (long int *ParticleTable,FL::cFieldLineSegment* Segment) {
    long int ptr;
    int cnt=0;

    ptr=Segment->FirstParticleIndex;

    while (ptr!=-1) {
      ParticleTable[cnt]=ptr;

      cnt++;
      ptr=PB::GetNext(ptr);
    }

    return cnt;
  };
      

  auto ProcessSegment = [&] (FL::cFieldLineSegment* Segment) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=NULL;
    long int ptr,ptr_next;
    double *x,LocalTimeStep;
    int spec;
    PB::byte* ParticleData;

    ptr=Segment->FirstParticleIndex;


    long int *ParticleTable=NULL;
    int ParticleTableLength=0;

    int SegmentParticleNumber=CountParticle(Segment);

    if (ParticleTableLength<SegmentParticleNumber) {
      if (ParticleTable!=NULL) delete [] ParticleTable;

      ParticleTable=new long int [SegmentParticleNumber]; 
      ParticleTableLength=SegmentParticleNumber;
    }

    PopulateParticleTable(ParticleTable,Segment);

    #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    #pragma omp parallel for default (none) private (ptr,x,spec, ParticleData,LocalTimeStep) firstprivate(node,SegmentParticleNumber,PIC::Mesh::mesh,ParticleTable) 
    #endif
    for (int ii=0;ii<SegmentParticleNumber;ii++) {
      ptr=ParticleTable[ii];
      ParticleData=PB::GetParticleDataPointer(ptr);

      x=PB::GetX(ParticleData);
      spec=PB::GetI(ParticleData);

      switch (_PIC_PARTICLE_LIST_ATTACHING_) {
      case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
        node=NULL;
        break;
      case _PIC_PARTICLE_LIST_ATTACHING_NODE_:
        node=PIC::Mesh::mesh->findTreeNode(x,node);
        if (node==NULL) exit(__LINE__,__FILE__,"Error: the point is not found");
        if (node->block==NULL) exit(__LINE__,__FILE__,"Error: the block is not allocated");
	break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is unknown");
      }

      LocalTimeStep=node->block->GetLocalTimeStep(spec); 
      _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(ptr,LocalTimeStep,node);
    }


    if (ParticleTable!=NULL) delete [] ParticleTable;
  };

    auto ProcessEntireSegment = [&] (FL::cFieldLineSegment* Segment) {
      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=NULL;
      long int ptr,ptr_next;
      double *x,LocalTimeStep;
      int spec;
      PB::byte* ParticleData;

      ptr=Segment->FirstParticleIndex;

      while (ptr!=-1) {
        ptr_next=PB::GetNext(ptr);

       ParticleData=PB::GetParticleDataPointer(ptr);

       x=PB::GetX(ParticleData);
       spec=PB::GetI(ParticleData);

      switch (_PIC_PARTICLE_LIST_ATTACHING_) {
      case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
        node=NULL;

        #if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
        LocalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
        #elif _SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_
        LocalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
        #else
        exit(__LINE__,__FILE__,"not implemented");
        #endif

        break;
      case _PIC_PARTICLE_LIST_ATTACHING_NODE_:
        node=PIC::Mesh::mesh->findTreeNode(x,node);
        if (node==NULL) exit(__LINE__,__FILE__,"Error: the point is not found");
        if (node->block==NULL) exit(__LINE__,__FILE__,"Error: the block is not allocated");
        LocalTimeStep=node->block->GetLocalTimeStep(spec);

        break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is unknown");
      }

       _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(ptr,LocalTimeStep,node);

       ptr=ptr_next;
    }
   };


    //loop through the field lines 
    int iFieldLine;
    FL::cFieldLineSegment *Segment; 
    int thread=0,SegmentCounter=0;

    
    #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    thread=omp_get_thread_num();
    #endif

    for (iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
      switch (_process_mode) {
      case _process_by_segments:
        #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
        #pragma omp parallel for schedule(dynamic) private (Segment) firstprivate(FL::FieldLinesAll,iFieldLine) 
        #endif
        for (int i=0;i<FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber();i++) {
          Segment=FL::FieldLinesAll[iFieldLine].SegmentPointerTable[i]; 
          ProcessEntireSegment(Segment);
        }

        break;
      case _process_by_particles:
        for (Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
          ProcessSegment(Segment);
          SegmentCounter++;
        }

        break;
      default:
        exit(__LINE__,__FILE__,"Error: the option is unknown");
      }
    } 
 

    //update particle list indexes 
    for (iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
      for (Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
        Segment->FirstParticleIndex=Segment->tempFirstParticleIndex;
        Segment->tempFirstParticleIndex=-1;                
      }
    }
  }

  // procedure that returns parameters of the guiding center motion
  void GuidingCenterMotion(double& ForceParal, double& AbsB,int spec,long int ptr,int   iFieldLine,double FieldLineCoord) {
    /* function returns guiding center velocity in direction perpendicular
     * to the magnetic field and the force parallel to it
     * for Lorentz force (considered here)
     *
     * v_{guide_perp} = 
     *  E\times B / B^2 + 
     *  \mu/(q\gamma) B\times\nabla B / B^2 +
     *  (p_{\parallel}^2)/(q\gamma m_0) B\times(B\cdot\nabla)B/B^4
     *
     * dp_{\parallel}/dt=
     *  q E_{\parallel} - \mu/\gamma \nabla_{\parallel}B
     *
     * \mu = p_{\perp}^2/(2m_0B)
     *
     * \gamma = 1/\sqrt{1-v^2/c^2}
     *********************************************************************/

    // namespace alias
    namespace FL = PIC::FieldLine;

if (_PIC__IDEAL_MHD_MODE_ == _PIC_MODE_OFF_) {
    exit(__LINE__,__FILE__,"Error: calculation of the electric field component of the Lorentz force is not implemented");
}

    double B[3],B0[3],B1[3], AbsBDeriv;
    double mu     = PIC::ParticleBuffer::GetMagneticMoment(ptr);
    double q      = PIC::MolecularData::GetElectricCharge(spec);

    FL::FieldLinesAll[iFieldLine].GetMagneticField(B0, (int)FieldLineCoord);
    FL::FieldLinesAll[iFieldLine].GetMagneticField(B,       FieldLineCoord);
    FL::FieldLinesAll[iFieldLine].GetMagneticField(B1, (int)FieldLineCoord+1-1E-7);
    AbsB   = pow(B[0]*B[0] + B[1]*B[1] + B[2]*B[2], 0.5);
    
    AbsBDeriv = (pow(B1[0]*B1[0] + B1[1]*B1[1] + B1[2]*B1[2], 0.5) -
        pow(B0[0]*B0[0] + B0[1]*B0[1] + B0[2]*B0[2], 0.5)) /  FL::FieldLinesAll[iFieldLine].GetSegmentLength(FieldLineCoord);

    //parallel force
switch(_PIC__IDEAL_MHD_MODE_) {
    case _PIC_MODE_ON_:
        // in this case E = - V \cross B => E_{\paral} = E*b = 0
        ForceParal = - mu * AbsBDeriv;
        break;
    default:
        exit(__LINE__, __FILE__, "not implemented");
}
  }
  
  // mover itself
  int Mover_SecondOrder(long int ptr, double dtTotal, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
    //aliases
    namespace FL = PIC::FieldLine;
    namespace PB = PIC::ParticleBuffer;

    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *newNode=NULL;
    double dtTemp;
    PIC::ParticleBuffer::byte *ParticleData;
    double AbsBInit=0.0;
//    double vInit[3]={0.0,0.0,0.0},xInit[3]={0.0,0.0,0.0};
    double AbsBMiddle=0.0;
    double vFinal[3]={0.0,0.0,0.0},xFinal[3]={0.0,0.0,0.0};

    int   iFieldLine = -1;
    double FieldLineCoordInit   = -1.0;
    double FieldLineCoordMiddle = -1.0;
    double FieldLineCoordFinal  = -1.0;
    
    double ForceParalInit=0.0, ForceParalMiddle=0.0;
    double dirInit[3]={0.0,0.0,0.0};
    double vParInit=0.0, vParMiddle=0.0, vParFinal=0.0;
    double pParInit,pPerpInit,pParMiddle,pParFinal;
    int i,j,k,spec;
	
    ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
    pPerpInit=PIC::ParticleBuffer::GetMomentumNormal(ParticleData);
    pParInit=PIC::ParticleBuffer::GetMomentumParallel(ParticleData);


    spec=PIC::ParticleBuffer::GetI(ParticleData);

    double m0 = PIC::MolecularData::GetMass(spec);
    double mu = PIC::ParticleBuffer::GetMagneticMoment(ptr);
    double v[3];
    double p[3];

    iFieldLine = PIC::ParticleBuffer::GetFieldLineId(ptr);
    FieldLineCoordInit = PIC::ParticleBuffer::GetFieldLineCoord(ptr);

    static long int nCall=0;
    nCall++;
    
    // predictor step
if (_PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE_ == _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE__ON_) {
    GuidingCenterMotion(ForceParalInit,AbsBInit,spec,ptr,iFieldLine,FieldLineCoordInit);
}
    
    FL::FieldLinesAll[iFieldLine].GetSegmentDirection(dirInit, FieldLineCoordInit);

    switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
    case _PIC_MODE_OFF_:
      vParInit=pParInit/m0;
      break;
    case _PIC_MODE_ON_:
      p[0]=pParInit,p[1]=pPerpInit,p[2]=0.0; 

      ::Relativistic::Momentum2Vel(v,p,m0);
      vParInit=v[0];
    }
    
    dtTemp=dtTotal/2.0;

    // advance coordinates half-step
    FieldLineCoordMiddle = FL::FieldLinesAll[iFieldLine].move(FieldLineCoordInit, dtTemp*vParInit);

    // advance momentum half-step
    pParMiddle=pParInit+dtTemp*ForceParalInit;
    
    if (FL::FieldLinesAll[iFieldLine].is_loop()) FL::FieldLinesAll[iFieldLine].fix_coord(FieldLineCoordMiddle);
    
    // check if a particle has left the domain
    if (FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoordMiddle)==NULL) { 
      //the particle left the computational domain
      int code=_PARTICLE_DELETED_ON_THE_FACE_;
      
      //call the function to processes particles that left the domain
      switch(code) {
      case _PARTICLE_DELETED_ON_THE_FACE_:
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;

      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }
    
    
    // corrector step
if (_PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE_ == _PIC_PARTICLE_MOVER__FORCE_INTEGRTAION_MODE__ON_) {
    GuidingCenterMotion(ForceParalMiddle,AbsBMiddle,spec,ptr,iFieldLine,FieldLineCoordMiddle);
}
    

    // advance coordinates full-step

    switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
    case _PIC_MODE_OFF_:
      vParMiddle=pParMiddle/m0;
      break;
    case _PIC_MODE_ON_:
      p[0]=pParMiddle,p[1]=pPerpInit,p[2]=0.0;
      ::Relativistic::Momentum2Vel(v,p,m0);
      vParMiddle=v[0];
    }

    FieldLineCoordFinal = FL::FieldLinesAll[iFieldLine].move(FieldLineCoordInit, dtTotal*vParMiddle);

    // advance momentum full-step
    pParFinal=pParInit+dtTotal*ForceParalMiddle;

    if (FL::FieldLinesAll[iFieldLine].is_loop()) FL::FieldLinesAll[iFieldLine].fix_coord(FieldLineCoordFinal);

    FL::cFieldLineSegment *Segment;
   
    //advance the particle's position and velocity
    //interaction with the faces of the block and internal surfaces
    if ((Segment=FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoordFinal))==NULL) {
      //the particle left the computational domain
      int code=_PARTICLE_DELETED_ON_THE_FACE_;
      
      //call the function that process particles that leaved the coputational domain
      switch (code) {
      case _PARTICLE_DELETED_ON_THE_FACE_:
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;

      default:
        exit(__LINE__,__FILE__,"Error: not implemented");
      }
    }

    FL::FieldLinesAll[iFieldLine].GetCartesian(xFinal, FieldLineCoordFinal);
    newNode=PIC::Mesh::Search::FindBlock(xFinal);


    ////PIC::ParticleBuffer::GetMomentumNormal(pPerpFinal,ParticleData) --> pPerpFinal was not updated 
    PIC::ParticleBuffer::SetMomentumParallel(pParFinal,ParticleData);


    double l[3],e0[3],e1[3],SpeedPar,SpeedPerp;

    Segment->GetDir(l);
    Vector3D::GetNormFrame(e0,e1,l);

    double vParallel,vNormal;

    switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
    case _PIC_MODE_OFF_:
//      vFinal[0]=pParFinal/m0,vFinal[1]=pPerpInit/m0,vFinal[2]=0.0;

      vParallel=pParFinal/m0;
      vNormal=pPerpInit/m0;

      for (int idim=0;idim<3;idim++) vFinal[idim]=SpeedPar*l[idim]+SpeedPerp*e0[idim]; 
      break;
    case _PIC_MODE_ON_:
      p[0]=pParFinal,p[1]=pPerpInit,p[2]=0.0;

      //for (int idim=0;idim<3;idim++) p[idim]=pParFinal*l[idim]+pPerpInit*e0[idim]; 
      ::Relativistic::Momentum2Vel(vFinal,p,m0);

      vParallel=vFinal[0];
      vNormal=vFinal[1];
    }

    //PIC::ParticleBuffer::SetV(vFinal,ParticleData);
    PIC::ParticleBuffer::SetVParallel(vParallel,ParticleData);
    PIC::ParticleBuffer::SetVNormal(vNormal,ParticleData);


    PIC::ParticleBuffer::SetX(xFinal,ParticleData);
    PB::SetFieldLineCoord(FieldLineCoordFinal, ParticleData);
  
    //adjust the value of 'startNode'
    startNode=newNode;

  //save the trajectory point
if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
  PIC::ParticleTracker::RecordTrajectoryPoint(xFinal,vFinal,spec,ParticleData,(void*)startNode);
}


if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
  if (_PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_) {
    PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(xFinal,vFinal,spec,ParticleData,(void*)startNode);
  }
}


    if (PIC::Mesh::mesh->FindCellIndex(xFinal,i,j,k,startNode,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cell where the particle is located");
    
    switch (_PIC_PARTICLE_LIST_ATTACHING_) {
    case  _PIC_PARTICLE_LIST_ATTACHING_NODE_:   
      #if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_ 
        { 
        PIC::Mesh::cDataBlockAMR *block=startNode->block;
        long int tempFirstCellParticle=block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
    
        PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
        PIC::ParticleBuffer::SetPrev(-1,ParticleData);
    
        if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
        block->tempParticleMovingListTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]=ptr;
        }
      #else  
        exit(__LINE__,__FILE__,"Error: the block need to be generalized to the case when _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_");
      #endif

      break;
    case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
       PIC::ParticleBuffer::SetNext(Segment->tempFirstParticleIndex,ParticleData);
       PIC::ParticleBuffer::SetPrev(-1,ParticleData);

       if (Segment->tempFirstParticleIndex!=-1) PIC::ParticleBuffer::SetPrev(ptr,Segment->tempFirstParticleIndex);
       Segment->tempFirstParticleIndex=ptr;
       break;
     default:
       exit(__LINE__,__FILE__,"Error: the option is unknown");
     }
 
    return _PARTICLE_MOTION_FINISHED_;
  }

}
}
  //namespace Mover -----------------------------------------------------------

}


//check consistency of the particles lists in case the lists are attached to the filed line segments  
void PIC::FieldLine::CheckParticleList() {
  namespace FL = PIC::FieldLine;
  namespace PB = PIC::ParticleBuffer;

  if ((_PIC_PARTICLE_LIST_ATTACHING_!=_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_)||(_PIC_FIELD_LINE_MODE_!=_PIC_MODE_ON_)) {
    exit(__LINE__,__FILE__,"Error: wrong options");
  }

  auto ProcessSegment = [&] (FL::cFieldLineSegment* Segment) {
    long int ptr;
    int cnt=0;

    ptr=Segment->FirstParticleIndex;

    while (ptr!=-1) {
      cnt++;
      ptr=PB::GetNext(ptr);
    }

    return cnt;
  };
 
  //loop through the field lines 
  int iFieldLine;
  FL::cFieldLineSegment *Segment;
  long int nTotalParticles=0;

  for (iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    for (Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
      nTotalParticles+=ProcessSegment(Segment);
    }
  }

  if (nTotalParticles!=PB::GetAllPartNum()) exit(__LINE__,__FILE__,"Error: the total number of particles stored in the lists is different from that stored in the particle buffer");
} 

void PIC::FieldLine::PopulateSegment(int spec,double NumberDensity,double Temperature,double* BulkVelocity,double Volume,int iSegment,int iFieldLine,int nMaxInjectedParticles) {
  namespace FL = PIC::FieldLine;
  namespace PB = PIC::ParticleBuffer;

  double l[3],anpart,ParticleWeight=0,x[3][3],WeightCorrectionFactor=1.0;
  int npart,cnt=0;
  cFieldLineSegment* Segment=FieldLinesAll[iFieldLine].GetSegment(iSegment);

  //determine the injected particle number 
  Segment->GetBegin()->GetX(x[0]);  
  Segment->GetEnd()->GetX(x[1]); 

  for (int idim=0;idim<3;idim++) {
    x[2][idim]=0.5*(x[0][idim]+x[1][idim]); 
    l[idim]=x[1][idim]-x[0][idim];
  }    

  for (int ip=0;ip<3;ip++) {
    //determine the local particle weight at the point x[ip]
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->findTreeNode(x[ip],NULL);    

    if (node!=NULL) {
      cnt++;
      ParticleWeight+=node->block->GetLocalParticleWeight(spec);
    }
  }

  if (cnt==0) exit(__LINE__,__FILE__,"Error: no segment point is within the domain");

  ParticleWeight/=cnt;      
  anpart=NumberDensity*Volume/ParticleWeight; 

  if ((nMaxInjectedParticles!=-1)&&(anpart>nMaxInjectedParticles)) {
    //limit the number of injected particles 
    WeightCorrectionFactor=anpart/nMaxInjectedParticles;
    anpart=nMaxInjectedParticles;  

    if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ != _INDIVIDUAL_PARTICLE_WEIGHT_ON_) exit(__LINE__,__FILE__,"Error: _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ must be set _INDIVIDUAL_PARTICLE_WEIGHT_ON_"); 
  }

  npart=(int)anpart;
  if (anpart-npart>rnd()) npart++;


  //populate new particles 
  for (;npart>0;npart--) {
    //get particle location 
    double xp[3],vp[3],vParallel,vNormal,S=rnd();
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;
    
    for (int idim=0;idim<3;idim++) xp[idim]=x[0][idim]+S*l[idim]; 
 
    node=PIC::Mesh::mesh->findTreeNode(xp,NULL);     

    if (node==NULL) continue; 
    else if (node->Thread!=PIC::ThisThread) continue; 

    //get porticle's microscopic properties 
    PIC::Distribution::MaxwellianVelocityDistribution(vp,BulkVelocity,Temperature,spec);
    Vector3D::GetComponents(vParallel,vNormal,vp,l);
    
    //add the particle to the particle list   
    // pointer particle to the particle to be injected
    PB::byte ptrData[PB::ParticleDataLength];

    //default settings
    PB::SetIndividualStatWeightCorrection(WeightCorrectionFactor,ptrData);
    PB::SetI(spec, ptrData);

    //set field line
    PB::SetFieldLineId(iFieldLine, ptrData);

    //set the localtion and velocity 
    S+=iSegment;
    PB::SetFieldLineCoord(S, ptrData);
    PB::SetX(xp, ptrData);

    PB::SetVParallel(vParallel,ptrData);
    PB::SetVNormal(vNormal,ptrData);


    long int NewParticle;
    PB::byte* NewParticleData;

    switch (_PIC_PARTICLE_LIST_ATTACHING_) {
    case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
      NewParticle=PB::GetNewParticle(Segment->FirstParticleIndex);
      NewParticleData=PB::GetParticleDataPointer(NewParticle);

      PB::CloneParticle(NewParticleData,ptrData);
      PB::SetParticleAllocated(NewParticleData);
      break;
    case _PIC_PARTICLE_LIST_ATTACHING_NODE_:
      NewParticle=PB::InitiateParticle(xp,NULL,NULL,NULL,ptrData,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is unknown");
    }
  } 
}

//inject particles at the beginning of a segment
int PIC::FieldLine::InjectMaxwellianLineBeginning(int spec,double NumberDensity,double Temperature,double* BulkVelocity,double InjectionArea,int iFieldLine,int nMaxInjectedParticles) {
  namespace FL = PIC::FieldLine;
  namespace PB = PIC::ParticleBuffer; 
  
  int nInjectedParticles=0;
  int npart;
  double dtTotal,SourceRate,n_sw,t_sw,v_v[3],l[3],anpart,ParticleWeight,*x,GlobalWeightCorrectionFactor=1.0;
  auto Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();
  FL::cFieldLineVertex* FirstVertex=Segment->GetBegin();
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=NULL;
  
  //get the direction of the first segment of the magnetic field line 
  Segment->GetDir(l);

  if (Vector3D::DotProduct(l,BulkVelocity)>0.0) { 
    for (int idim=0;idim<3;idim++) l[idim]*=-1.0;
  }

  //determine the corresponding source rate 
  SourceRate=PIC::BC::CalculateInjectionRate_MaxwellianDistribution(NumberDensity,Temperature,BulkVelocity,l,spec);

  //determine the number of the injected model particles 
  x=FirstVertex->GetX();
  node=PIC::Mesh::mesh->findTreeNode(x,node);

  if (node==NULL) exit(__LINE__,__FILE__,"Error: the location of the vertex is not found");
  if (node->Thread!=PIC::ThisThread) return 0;
  
  if (node->block==NULL) exit(__LINE__,__FILE__,"Error: the block is not allocated - something is wrong");
  
  dtTotal=node->block->GetLocalTimeStep(spec);
  ParticleWeight=node->block->GetLocalParticleWeight(spec);
  anpart=dtTotal*SourceRate/ParticleWeight*InjectionArea;

  if ((nMaxInjectedParticles!=-1)&&(anpart>nMaxInjectedParticles)) {
    //limit the number of injected particles 
    GlobalWeightCorrectionFactor=anpart/nMaxInjectedParticles;
    anpart=nMaxInjectedParticles;

    if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ != _INDIVIDUAL_PARTICLE_WEIGHT_ON_) exit(__LINE__,__FILE__,"Error: _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ must be set _INDIVIDUAL_PARTICLE_WEIGHT_ON_");
  }
  
  npart=(int)anpart;
  if (anpart-npart>rnd()) npart++;
  
  nInjectedParticles=npart;

  //populate new particles 
  for (;npart>0;npart--) {
    //get particle location 
    double vp[3],vParallel,vNormal,WeightCorrectionFactor,S=0.0;

    //get porticle's microscopic properties         
    WeightCorrectionFactor=PIC::Distribution::InjectMaxwellianDistribution(vp,BulkVelocity,Temperature,l,spec,_PIC_DISTRIBUTION_WEIGHT_CORRECTION_MODE__NO_WEIGHT_CORRECTION_);
    Vector3D::GetComponents(vParallel,vNormal,vp,l);
    vParallel*=-1.0;
    
    //add the particle to the particle list   
    // pointer particle to the particle to be injected
    PB::byte ptrData[PB::ParticleDataLength];

    //default settings
    PB::SetIndividualStatWeightCorrection(WeightCorrectionFactor*GlobalWeightCorrectionFactor,ptrData);
    PB::SetI(spec, ptrData);

    //set field line
    PB::SetFieldLineId(iFieldLine, ptrData);
    
    //move particle along the field line for a fraction of a time step
    S=FL::FieldLinesAll[iFieldLine].move(S,dtTotal*rnd()*vParallel);
    FL::FieldLinesAll[iFieldLine].GetCartesian(x,S);

    //set the localtion and velocity 
    PB::SetFieldLineCoord(S,ptrData);
    PB::SetX(x, ptrData);

    PB::SetVParallel(vParallel,ptrData);
    PB::SetVNormal(vNormal,ptrData);

    long int NewParticle;
    PB::byte* NewParticleData;

    switch (_PIC_PARTICLE_LIST_ATTACHING_) {
    case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
      NewParticle=PB::GetNewParticle(Segment->FirstParticleIndex);
      NewParticleData=PB::GetParticleDataPointer(NewParticle);

      PB::CloneParticle(NewParticleData,ptrData);
      PB::SetParticleAllocated(NewParticleData);
      break;
    case _PIC_PARTICLE_LIST_ATTACHING_NODE_:
      NewParticle=PB::InitiateParticle(x,NULL,NULL,NULL,ptrData,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
      break;
    default:
      exit(__LINE__,__FILE__,"Error: the option is unknown");
    }
  } 
  
  return nInjectedParticles;
}
  
long int  PIC::FieldLine::TraverseAllFieldLines(void (*processParticle)(long int)) {
    namespace FL = PIC::FieldLine;
    long int cnt=0;

    if (FL::FieldLinesAll == nullptr) {
        if (PIC::ThisThread == 0) printf("TraverseAllFieldLines: ERROR -> FieldLinesAll is NULL\n");
        return 0;
    }

    // Loop through all field lines
    for (int iFieldLine = 0; iFieldLine < FL::nFieldLine; iFieldLine++) {
        FL::cFieldLine& currentFieldLine = FL::FieldLinesAll[iFieldLine];
        FL::cFieldLineSegment* currentSegment = currentFieldLine.GetFirstSegment();

        // Loop through all segments in current field line
        while (currentSegment != nullptr) {
            // Only process segments assigned to this thread
            if (currentSegment->Thread == PIC::ThisThread) {
                // Get particles in current segment
                long int particlePtr = currentSegment->FirstParticleIndex;

                // Loop through all particles in current segment
                while (particlePtr != -1) {
                    // Process the particle using the provided function
                    processParticle(particlePtr);
                    cnt++;

                    // Move to next particle
                    particlePtr = PIC::ParticleBuffer::GetNext(particlePtr);
                }
            }

            currentSegment = currentSegment->GetNext();
        }
    }

    return cnt;
}
  
