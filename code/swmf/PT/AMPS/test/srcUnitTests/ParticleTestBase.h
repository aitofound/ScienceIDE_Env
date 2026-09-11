// Base class for particle-related tests

#ifndef _PARTICLE_TEST_BASE_CLASS_
#define _PARTICLE_TEST_BASE_CLASS_

template<size_t BufferSize = 200>
class ParticleTestBase {
protected:
    PIC::ParticleBuffer::byte* ParticleDataBuffer; 
    long int MaxNPart, NAllPart, FirstPBufferParticle;
    int *ParticleNumberTable, *ParticleOffsetTable;
    PIC::ParticleBuffer::cParticleTable *ParticlePopulationTable;

    virtual void SetUp() {
        // Store initial state
        ParticleDataBuffer = PIC::ParticleBuffer::ParticleDataBuffer;
        MaxNPart = PIC::ParticleBuffer::MaxNPart;
        NAllPart = PIC::ParticleBuffer::NAllPart;
        FirstPBufferParticle = PIC::ParticleBuffer::FirstPBufferParticle;
        ParticleNumberTable = PIC::ParticleBuffer::ParticleNumberTable;
        ParticleOffsetTable = PIC::ParticleBuffer::ParticleOffsetTable;
        ParticlePopulationTable = PIC::ParticleBuffer::ParticlePopulationTable;

        // Reset buffer state
        PIC::ParticleBuffer::ParticleDataBuffer = NULL;
        PIC::ParticleBuffer::MaxNPart = 0;
        PIC::ParticleBuffer::NAllPart = 0;
        PIC::ParticleBuffer::FirstPBufferParticle = -1;
        PIC::ParticleBuffer::ParticleNumberTable = NULL;
        PIC::ParticleBuffer::ParticleOffsetTable = NULL;
        PIC::ParticleBuffer::ParticlePopulationTable = NULL;

        // Initialize buffer
        PIC::ParticleBuffer::Init(BufferSize);

        // Verify initialization
        ASSERT_NE(nullptr, PIC::ParticleBuffer::ParticleDataBuffer)
            << "Failed to initialize ParticleDataBuffer";
        ASSERT_EQ(BufferSize, PIC::ParticleBuffer::GetMaxNPart())
            << "Failed to set MaxNPart";
    }

    virtual void TearDown() {
        // Clean up allocated resources
        if (PIC::ParticleBuffer::ParticleNumberTable != NULL) 
            amps_free_managed(PIC::ParticleBuffer::ParticleNumberTable);
        PIC::ParticleBuffer::ParticleNumberTable = ParticleNumberTable;

        if (PIC::ParticleBuffer::ParticlePopulationTable != NULL) 
            amps_free_managed(PIC::ParticleBuffer::ParticlePopulationTable);
        PIC::ParticleBuffer::ParticlePopulationTable = ParticlePopulationTable;

        if (PIC::ParticleBuffer::ParticleOffsetTable != NULL) 
            amps_free_managed(PIC::ParticleBuffer::ParticleOffsetTable);
        PIC::ParticleBuffer::ParticleOffsetTable = ParticleOffsetTable;

        if (PIC::ParticleBuffer::ParticleDataBuffer != NULL) 
            amps_free_managed(PIC::ParticleBuffer::ParticleDataBuffer);
        PIC::ParticleBuffer::ParticleDataBuffer = ParticleDataBuffer;

        // Restore initial state
        PIC::ParticleBuffer::MaxNPart = MaxNPart;
        PIC::ParticleBuffer::NAllPart = NAllPart;
    }
};


#endif
