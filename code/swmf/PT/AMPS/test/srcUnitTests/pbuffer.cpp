

#include <gtest/gtest.h>
#include "pic.h"

void pbuffer_test_for_linker() {}
 
TEST(InlineTest, IntegerEquality) {
    EXPECT_EQ(5, 5);  // Passes
    EXPECT_NE(5, 3);  // Passes
}

class ParticleBufferTest : public ::testing::Test {
protected:
   PIC::ParticleBuffer::byte* ParticleDataBuffer; 
   long int MaxNPart,NAllPart,FirstPBufferParticle;
   int *ParticleNumberTable,*ParticleOffsetTable;
   PIC::ParticleBuffer::cParticleTable *ParticlePopulationTable;

    void SetUp() override {
      //set the initial state of the particle buffer
      ParticleDataBuffer=PIC::ParticleBuffer::ParticleDataBuffer;
      MaxNPart=PIC::ParticleBuffer::MaxNPart;
      NAllPart=PIC::ParticleBuffer::NAllPart;
      FirstPBufferParticle=PIC::ParticleBuffer::FirstPBufferParticle;
      ParticleNumberTable=PIC::ParticleBuffer::ParticleNumberTable;
      ParticleOffsetTable=PIC::ParticleBuffer::ParticleOffsetTable;
      ParticlePopulationTable=PIC::ParticleBuffer::ParticlePopulationTable;


      //set the default values for the partice buffer
      PIC::ParticleBuffer::ParticleDataBuffer=NULL;
      PIC::ParticleBuffer::MaxNPart=0;
      PIC::ParticleBuffer::NAllPart=0;
      PIC::ParticleBuffer::FirstPBufferParticle=-1;
      PIC::ParticleBuffer::ParticleNumberTable=NULL;
      PIC::ParticleBuffer::ParticleOffsetTable=NULL;
      PIC::ParticleBuffer::ParticlePopulationTable=NULL; 
        	

      // Initialize buffer with 100 particles
      PIC::ParticleBuffer::Init(100);

      ASSERT_NE(nullptr, PIC::ParticleBuffer::ParticleDataBuffer)
            << "Failed to initialize ParticleDataBuffer";
      ASSERT_EQ(100, PIC::ParticleBuffer::GetMaxNPart())
            << "Failed to set MaxNPart";
    }

    void TearDown() override {
      // Cleanup
      if (PIC::ParticleBuffer::ParticleNumberTable!=NULL) amps_free_managed(PIC::ParticleBuffer::ParticleNumberTable);
      PIC::ParticleBuffer::ParticleNumberTable=ParticleNumberTable;

      if (PIC::ParticleBuffer::ParticlePopulationTable!=NULL) amps_free_managed(PIC::ParticleBuffer::ParticlePopulationTable);
      PIC::ParticleBuffer::ParticlePopulationTable=ParticlePopulationTable;

      if (PIC::ParticleBuffer::ParticleOffsetTable!=NULL) amps_free_managed(PIC::ParticleBuffer::ParticleOffsetTable);
      PIC::ParticleBuffer::ParticleOffsetTable=ParticleOffsetTable;

      if (PIC::ParticleBuffer::ParticleDataBuffer!=NULL) amps_free_managed(PIC::ParticleBuffer::ParticleDataBuffer);
      PIC::ParticleBuffer::ParticleDataBuffer=ParticleDataBuffer;

      PIC::ParticleBuffer::MaxNPart=MaxNPart;
      PIC::ParticleBuffer::NAllPart=NAllPart;
    }

};


TEST_F(ParticleBufferTest, InitializationTest) {
    EXPECT_NE(nullptr, PIC::ParticleBuffer::ParticleDataBuffer);
    EXPECT_EQ(100, PIC::ParticleBuffer::GetMaxNPart());
    EXPECT_EQ(0, PIC::ParticleBuffer::GetAllPartNum());
}


// Test particle allocation and deletion
TEST_F(ParticleBufferTest, ParticleAllocationAndDeletionTest) {
    // Allocate new particle
    long int particlePtr = PIC::ParticleBuffer::GetNewParticle(true);
    EXPECT_GE(particlePtr, 0);
    EXPECT_EQ(PIC::ParticleBuffer::GetAllPartNum(), 1);

    // Verify particle is marked as allocated
    EXPECT_TRUE(PIC::ParticleBuffer::IsParticleAllocated(particlePtr));

    // Delete particle
    PIC::ParticleBuffer::DeleteParticle(particlePtr);
    EXPECT_EQ(PIC::ParticleBuffer::GetAllPartNum(), 0);
    EXPECT_FALSE(PIC::ParticleBuffer::IsParticleAllocated(particlePtr));
}


TEST_F(ParticleBufferTest, ParticleDataAccessTest) {
    long int ptr = PIC::ParticleBuffer::GetNewParticle(true);
    double pos[3] = {1.0, 2.0, 3.0};
    double vel[3] = {4.0, 5.0, 6.0};

    // Verify data
    if (_PIC_FIELD_LINE_MODE_!=_PIC_MODE_ON_) {
      double* readPos = PIC::ParticleBuffer::GetX(ptr);
      double* readVel = PIC::ParticleBuffer::GetV(ptr);

      // Set position and velocity
      PIC::ParticleBuffer::SetX(pos, ptr);
      PIC::ParticleBuffer::SetV(vel, ptr);


      for(int i = 0; i < 3; i++) {
          EXPECT_DOUBLE_EQ(pos[i], readPos[i]);
          EXPECT_DOUBLE_EQ(vel[i], readVel[i]);
      }
      
    }
    else {
      double readPos[3],readVel[3];
      double S=4.5,readS,v_parallel,v_normal;

      // Set position and velocity
      PIC::ParticleBuffer::SetFieldLineCoord(S,ptr); 
      PIC::ParticleBuffer::SetV(vel, ptr);


      readS=PIC::ParticleBuffer::GetFieldLineCoord(ptr);
      PIC::ParticleBuffer::GetV(readVel,ptr);

      for(int i = 0; i < 2; i++) {
        EXPECT_DOUBLE_EQ(vel[i], readVel[i]);
      }

      EXPECT_DOUBLE_EQ(S,readS);

      
      v_parallel=PIC::ParticleBuffer::GetVParallel(ptr);
      EXPECT_DOUBLE_EQ(v_parallel, readVel[0]);

      
      v_normal=PIC::ParticleBuffer::GetVNormal(ptr);
      EXPECT_DOUBLE_EQ(v_normal, readVel[1]); 

      EXPECT_DOUBLE_EQ(readVel[2],0.0);
    }
    
}


TEST_F(ParticleBufferTest, ParticleCloneTest) {
    long int srcPtr = PIC::ParticleBuffer::GetNewParticle(true);
    double pos[3] = {1.0, 2.0, 3.0};
    double S=4.5,vel[3] = {4.0, 5.0, 6.0};
 
    if (_PIC_FIELD_LINE_MODE_!=_PIC_MODE_ON_) {
      PIC::ParticleBuffer::SetX(pos, srcPtr);
    }
    else {
      PIC::ParticleBuffer::SetFieldLineCoord(S,srcPtr);
    }

    PIC::ParticleBuffer::SetV(vel, srcPtr);

    long int destPtr = PIC::ParticleBuffer::GetNewParticle(true);
    PIC::ParticleBuffer::CloneParticle(destPtr, srcPtr);

    if (_PIC_FIELD_LINE_MODE_!=_PIC_MODE_ON_) {
      double* readPos = PIC::ParticleBuffer::GetX(destPtr);
      double* readVel = PIC::ParticleBuffer::GetV(destPtr);

      for(int i = 0; i < 3; i++) {
        EXPECT_DOUBLE_EQ(pos[i], readPos[i]);
	EXPECT_DOUBLE_EQ(vel[i], readVel[i]);
      }
    }
    else {
      double readVel[3],readS,v_parallel,v_normal;

      readS=PIC::ParticleBuffer::GetFieldLineCoord(destPtr);
      PIC::ParticleBuffer::GetV(readVel,destPtr);

      for(int i = 0; i < 2; i++) {
        EXPECT_DOUBLE_EQ(vel[i], readVel[i]);
      }

      EXPECT_DOUBLE_EQ(S,readS);


      v_parallel=PIC::ParticleBuffer::GetVParallel(destPtr);
      EXPECT_DOUBLE_EQ(v_parallel, readVel[0]);


      v_normal=PIC::ParticleBuffer::GetVNormal(destPtr);
      EXPECT_DOUBLE_EQ(v_normal, readVel[1]);

      EXPECT_DOUBLE_EQ(readVel[2],0.0);
    }

}


TEST_F(ParticleBufferTest, ParticleListOperationsTest) {
    long int firstParticle = -1;
    
    // Add particles to list
    long int ptr1 = PIC::ParticleBuffer::GetNewParticle(firstParticle, true);
    long int ptr2 = PIC::ParticleBuffer::GetNewParticle(firstParticle, true);
    
    // Verify list structure
    EXPECT_EQ(ptr2, firstParticle);
    EXPECT_EQ(ptr1, PIC::ParticleBuffer::GetNext(ptr2));
    EXPECT_EQ(-1, PIC::ParticleBuffer::GetNext(ptr1));
    
    // Test particle exclusion
    PIC::ParticleBuffer::ExcludeParticleFromList(ptr2, firstParticle);
    EXPECT_EQ(ptr1, firstParticle);
}

// Standalone test for OptionalDataStorage
TEST(ParticleBuffer, RequestDataStorageBeforeInit) {
    // First clean up any existing buffer to ensure clean state
   PIC::ParticleBuffer::byte* ParticleDataBuffer;
   long int MaxNPart,NAllPart,FirstPBufferParticle;
   int *ParticleNumberTable,*ParticleOffsetTable;
   PIC::ParticleBuffer::cParticleTable *ParticlePopulationTable;

   ParticleDataBuffer=PIC::ParticleBuffer::ParticleDataBuffer;
   MaxNPart=PIC::ParticleBuffer::MaxNPart;
   NAllPart=PIC::ParticleBuffer::NAllPart;
   FirstPBufferParticle=PIC::ParticleBuffer::FirstPBufferParticle;
   ParticleNumberTable=PIC::ParticleBuffer::ParticleNumberTable;
   ParticleOffsetTable=PIC::ParticleBuffer::ParticleOffsetTable;
   ParticlePopulationTable=PIC::ParticleBuffer::ParticlePopulationTable;


    //set the default values for the partice buffer
    PIC::ParticleBuffer::ParticleDataBuffer=NULL;
    PIC::ParticleBuffer::MaxNPart=0;
    PIC::ParticleBuffer::NAllPart=0;
    PIC::ParticleBuffer::FirstPBufferParticle=-1;
    PIC::ParticleBuffer::ParticleNumberTable=NULL;
    PIC::ParticleBuffer::ParticleOffsetTable=NULL;
    PIC::ParticleBuffer::ParticlePopulationTable=NULL;


    // Store initial particle data length
    long int initialDataLength = PIC::ParticleBuffer::GetParticleDataLength();

    // Request additional storage
    long int offset;
    int additionalDataLength = sizeof(double);
    PIC::ParticleBuffer::RequestDataStorage(offset, additionalDataLength);

    // Verify offset and new data length
    EXPECT_GE(offset, initialDataLength);
    EXPECT_EQ(PIC::ParticleBuffer::GetParticleDataLength(), initialDataLength + additionalDataLength);

    // Verify we can still initialize buffer after requesting storage
    PIC::ParticleBuffer::Init(100);
    EXPECT_NE(nullptr, PIC::ParticleBuffer::ParticleDataBuffer);

    //verify access to the particle internal data  
    long int ptr = PIC::ParticleBuffer::GetNewParticle(true);
    auto ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
    double pos[3] = {1.0, 2.0, 3.0};
    double vel[3] = {4.0, 5.0, 6.0};
    double a=3.1415;

    // Set position and velocity
    if (_PIC_FIELD_LINE_MODE_!=_PIC_MODE_ON_) { 
      PIC::ParticleBuffer::SetX(pos, ptr);
      PIC::ParticleBuffer::SetV(vel, ptr);
      *((double*)(ParticleData+offset))=a;
    }
    else {
     PIC::ParticleBuffer::SetFieldLineCoord(pos[0],ptr);
     PIC::ParticleBuffer::SetV(vel, ptr);
     *((double*)(ParticleData+offset))=a;
    }



    // Verify data
    if (_PIC_FIELD_LINE_MODE_!=_PIC_MODE_ON_) { 
      double* readPos = PIC::ParticleBuffer::GetX(ptr);
      double* readVel = PIC::ParticleBuffer::GetV(ptr);

      for(int i = 0; i < 3; i++) {
        EXPECT_DOUBLE_EQ(pos[i], readPos[i]);
        EXPECT_DOUBLE_EQ(vel[i], readVel[i]);
      }
    }
    else {
       double readVel[3],readS,v_parallel,v_normal;

       readS=PIC::ParticleBuffer::GetFieldLineCoord(ptr);
       PIC::ParticleBuffer::GetV(readVel,ptr);

       for(int i = 0; i < 2; i++) {
         EXPECT_DOUBLE_EQ(vel[i], readVel[i]);
       }

       EXPECT_DOUBLE_EQ(pos[0],readS);

       v_parallel=PIC::ParticleBuffer::GetVParallel(ptr);
       EXPECT_DOUBLE_EQ(v_parallel, readVel[0]);


       v_normal=PIC::ParticleBuffer::GetVNormal(ptr);
       EXPECT_DOUBLE_EQ(v_normal, readVel[1]);

       EXPECT_DOUBLE_EQ(readVel[2],0.0);
    }

    EXPECT_DOUBLE_EQ(*((double*)(ParticleData+offset)),a);

    // Clean up
    if (PIC::ParticleBuffer::ParticleNumberTable!=NULL) amps_free_managed(PIC::ParticleBuffer::ParticleNumberTable);
    PIC::ParticleBuffer::ParticleNumberTable=ParticleNumberTable;

    if (PIC::ParticleBuffer::ParticlePopulationTable!=NULL) amps_free_managed(PIC::ParticleBuffer::ParticlePopulationTable);
    PIC::ParticleBuffer::ParticlePopulationTable=ParticlePopulationTable;

    if (PIC::ParticleBuffer::ParticleOffsetTable!=NULL) amps_free_managed(PIC::ParticleBuffer::ParticleOffsetTable);
    PIC::ParticleBuffer::ParticleOffsetTable=ParticleOffsetTable;

    if (PIC::ParticleBuffer::ParticleDataBuffer!=NULL) amps_free_managed(PIC::ParticleBuffer::ParticleDataBuffer);
    PIC::ParticleBuffer::ParticleDataBuffer=ParticleDataBuffer;

    PIC::ParticleBuffer::MaxNPart=MaxNPart;
    PIC::ParticleBuffer::NAllPart=NAllPart;
}

// Test individual particle weight corrections
TEST_F(ParticleBufferTest, ParticleWeights) {
  long int ptr = PIC::ParticleBuffer::GetNewParticle(true);

#if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
  double weight = 2.5;
  PIC::ParticleBuffer::SetIndividualStatWeightCorrection(weight, ptr);
  EXPECT_DOUBLE_EQ(weight, PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr));
#endif
}

/*
// Test buffer full handling
TEST_F(ParticleBufferTest, BufferFull) {
  // Allocate all particles
  int maxPart = PIC::ParticleBuffer::GetMaxNPart();
  for (int i = 0; i < maxPart; i++) {
    EXPECT_NO_THROW(PIC::ParticleBuffer::GetNewParticle(true));
  }

  EXPECT_EQ(maxPart, PIC::ParticleBuffer::GetAllPartNum());

  // Next allocation should fail
  EXPECT_DEATH(PIC::ParticleBuffer::GetNewParticle(true), "The particle buffer is full");
}
*/
