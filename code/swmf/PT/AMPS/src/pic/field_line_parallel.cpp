
#include "pic.h"

#include <unordered_set>
#include <stdexcept>
#include <vector>
#include <map>


PIC::ParallelFieldLines::cThreadSegmentTable* PIC::ParallelFieldLines::ThreadSegmentTable;

/**
 * Performs static domain decomposition of field lines by assigning consecutive segments
 * of specified length to processes.
 *
 * @param SegmentWindowLength Number of consecutive segments to assign to each process
 * @return bool Returns true if decomposition was successful, false otherwise
 */
bool PIC::ParallelFieldLines::StaticDecompositionSegmentNumberWindow(double SegmentWindowLengthFraction) {
    namespace FL = PIC::FieldLine;

    // Input validation
    if (FL::FieldLinesAll == nullptr) {
        printf("Warning: FieldLinesAll is null in StaticDecompositionByWindow\n");
        return false;
    }

    if (FL::nFieldLine <= 0) {
        printf("Warning: Invalid number of field lines: %d\n", FL::nFieldLine);
        return false;
    }

      // Validate WindowLength parameter
    if (SegmentWindowLengthFraction <= 0.0 || SegmentWindowLengthFraction > 1.0) {
        throw std::invalid_argument("WindowLength must be in the range (0,1]");
    }

    // Process each field line
    bool success = true;
    int SegmentWindowLength;

    for (int iFieldLine = 0; iFieldLine < FL::nFieldLine; iFieldLine++) {
        FL::cFieldLineSegment* firstSegment = FL::FieldLinesAll[iFieldLine].GetFirstSegment();
        if (firstSegment == nullptr) {
            printf("Warning: Field line %d has no segments\n", iFieldLine);
            success = false;
            continue;
        }

        // Count total segments in this field line
        int nSegments = 0;
        auto currentSegment = firstSegment;
        while (currentSegment != nullptr) {
            nSegments++;
            currentSegment = currentSegment->GetNext();
        }

	SegmentWindowLength=nSegments/PIC::nTotalThreads*SegmentWindowLengthFraction;
        if (SegmentWindowLength==0) SegmentWindowLength=1;	

        // Start assignment from the first segment
        currentSegment = firstSegment;
        int currentThread = 0;
        int segmentsInCurrentWindow = 0;

        // Assign segments to threads
        while (currentSegment != nullptr) {
            // Assign current segment to current thread
            currentSegment->Thread = currentThread;
            segmentsInCurrentWindow++;

            // Move to next segment
            currentSegment = currentSegment->GetNext();

            // If we've filled the current window or reached the end, prepare for next thread
            if (segmentsInCurrentWindow >= SegmentWindowLength) {
               currentThread++;
	       if (currentThread==PIC::nTotalThreads) currentThread=0;
               segmentsInCurrentWindow = 0;
            }
        }

        // Log distribution info if needed
        if (PIC::ThisThread == 0) {
            printf("Field line %d: %d segments distributed across %d processes with window length %d\n",
                   iFieldLine, nSegments, PIC::nTotalThreads, SegmentWindowLength);
        }
    }

    // Generate thread segment table if all went well
    if (success) {
        try {
            GenerateThreadSegmentTable();
        }
        catch (const std::exception& e) {
            printf("Error generating thread segment table: %s\n", e.what());
            success = false;
        }
    }

    // Optional: Verify the distribution
    if (success && PIC::ThisThread == 0) {
        for (int iFieldLine = 0; iFieldLine < FL::nFieldLine; iFieldLine++) {
            std::vector<int> segmentsPerThread(PIC::nTotalThreads, 0);
            int consecutiveCount = 0;
            int lastThread = -1;

            auto segment = FL::FieldLinesAll[iFieldLine].GetFirstSegment();
            while (segment != nullptr) {
                segmentsPerThread[segment->Thread]++;

                // Check consecutiveness
                if (lastThread == segment->Thread) {
                    consecutiveCount++;
                } else {
                    if (consecutiveCount > 0 && consecutiveCount < SegmentWindowLength) {
                        printf("Warning: Field line %d has non-window-sized consecutive segment group (%d)\n",
                               iFieldLine, consecutiveCount);
                    }
                    consecutiveCount = 1;
                }

                lastThread = segment->Thread;
                segment = segment->GetNext();
            }

            // Print distribution summary
            printf("Field line %d distribution:\n", iFieldLine);
            for (int i = 0; i < PIC::nTotalThreads; i++) {
                printf("  Process %d: %d segments\n", i, segmentsPerThread[i]);
            }
            printf("\n");
        }
    }

    return success;
}

/**
 * Performs static decomposition of all field lines by segment number.
 * Distributes segments across available threads and generates a lookup table.
 *
 * @return bool Returns true if decomposition was successful, false otherwise.
 */
bool PIC::ParallelFieldLines::StaticDecompositionSegmentNumber() {
  return StaticDecompositionSegmentNumberWindow(1.0);
}


void PIC::ParallelFieldLines::StaticDecompositionSegmentNumber(PIC::FieldLine::cFieldLineSegment* SegmentIn) { 
    // Count segments
    int nSegments = 0;
    auto Segment = SegmentIn;
    while (Segment != NULL) {
        nSegments++;
        Segment = Segment->GetNext();
    }
    
    // Handle edge case
    if (nSegments == 0) return;
    
    // Calculate segments per thread
    int nSegmentsPerThread = std::max(1, nSegments / PIC::nTotalThreads);
    
    // Assign threads
    int cnt = 0;
    Segment = SegmentIn;
    while (Segment != NULL) {
        Segment->Thread = std::min(cnt / nSegmentsPerThread, 
                                 PIC::nTotalThreads - 1);
        cnt++;
        Segment = Segment->GetNext();
    }
}

void PIC::ParallelFieldLines::StaticDecompositionFieldLineLength(double WindowLength) {
namespace FL = PIC::FieldLine;
  int iFieldLine;

  if (FL::FieldLinesAll==NULL) return;
  
  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    StaticDecompositionFieldLineLength(FL::FieldLinesAll[iFieldLine].GetFirstSegment(),WindowLength);
  }

  GenerateThreadSegmentTable();
} 


void PIC::ParallelFieldLines::StaticDecompositionFieldLineLength(PIC::FieldLine::cFieldLineSegment* SegmentIn, double WindowLength) {
    // Validate WindowLength parameter
    if (WindowLength <= 0.0 || WindowLength > 1.0) {
        throw std::invalid_argument("WindowLength must be in the range (0,1]");
    }

    // First pass: calculate total length
    double totalLength = 0.0;
    auto Segment = SegmentIn;
    while (Segment != NULL) {
        totalLength += Segment->GetLength();
        Segment = Segment->GetNext();
    }

    // Handle edge cases
    if (totalLength == 0.0 || SegmentIn == NULL) return;

    // Calculate LengthWindow based on the new formula
    double LengthWindow = (totalLength / PIC::nTotalThreads) * WindowLength;

    // Second pass: assign threads based on cumulative length and LengthWindow
    double currentLength = 0.0;
    Segment = SegmentIn;
    int currentThread = 0;
    double WindowStartLocation=0.0;

    while (Segment != NULL) {
        // Calculate the position within the current window
        double positionInWindow = currentLength - WindowStartLocation;

        // Check if we need to move to the next thread
        if (positionInWindow >= LengthWindow) { 
            currentThread++;
	    if (currentThread==PIC::nTotalThreads) currentThread=0;
	    WindowStartLocation=currentLength;
        }

        // Assign thread number, ensuring it's within bounds
        Segment->Thread = std::min(currentThread, PIC::nTotalThreads - 1);

        // Update accumulated length
        currentLength += Segment->GetLength();
        Segment = Segment->GetNext();
    }
}

void PIC::ParallelFieldLines::GenerateThreadSegmentTable() {
namespace FL = PIC::FieldLine;
  int i,iFieldLine,*cnt;

  if (ThreadSegmentTable!=NULL) exit(__LINE__,__FILE__,"Error: re-initialization of ThreadSegmentTable");

  if (FL::FieldLinesAll==NULL) return;
 
  ThreadSegmentTable=new cThreadSegmentTable[FL::nFieldLine];
  cnt=new int [PIC::nTotalThreads];


  //count the number of segments
  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    for (i=0;i<PIC::nTotalThreads;i++) cnt[i]=0; 

    for (auto Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
      cnt[Segment->Thread]++;
    }

    ThreadSegmentTable[iFieldLine].TableLength=new int [PIC::nTotalThreads];
    ThreadSegmentTable[iFieldLine].Table=new FL::cFieldLineSegment** [PIC::nTotalThreads];

    for (int thread=0;thread<PIC::nTotalThreads;thread++) {
      ThreadSegmentTable[iFieldLine].TableLength[thread]=cnt[thread];
      ThreadSegmentTable[iFieldLine].Table[thread]=(cnt[thread]!=0) ? new FL::cFieldLineSegment*[cnt[thread]] : NULL; 
    }

    //populate the table 
    for (i=0;i<PIC::nTotalThreads;i++) cnt[i]=0;

    for (auto Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
      ThreadSegmentTable[iFieldLine].Table[Segment->Thread][cnt[Segment->Thread]]=Segment;
      cnt[Segment->Thread]++;
    }
  }

  delete [] cnt;
}



//        PIC::ParallelFieldLines::GetFieldLinePopulationStat();


//===================================   EXCHANGE PARTICLES BETWEEN PROCESSES ============================================
void PIC::ParallelFieldLines::ExchangeFieldLineParticles(cThreadSegmentTable &ThreadSegmentTable) {
    int From, To;
    long int Particle, newParticle;
    int nTotalSendParticles[PIC::nTotalThreads]; 
    int nRecvSegments[PIC::nTotalThreads];
    int nSendSegments[PIC::nTotalThreads]; 

    for (int t=0;t<PIC::nTotalThreads;t++) nTotalSendParticles[t]=0,nRecvSegments[t]=0,nSendSegments[t]=0;

    MPI_Request RecvParticleDataRequestTable[PIC::nTotalThreads];
    MPI_Request SendParticleDataRequestTable[PIC::nTotalThreads];
    MPI_Request SendMessageSizeRequestTable[PIC::nTotalThreads];
    MPI_Request RecvMessageSizeRequestTable[PIC::nTotalThreads];

    MPI_Request SendDescriptorRequestTable[PIC::nTotalThreads];
    int SendDescriptorRequestTableLength=0;

    MPI_Request RecvDescriptorRequestTable[PIC::nTotalThreads];
    int RecvDescriptorRequestTableLength=0; 

    std::vector<int> SendMessageLength(PIC::nTotalThreads, 0);
    std::vector<int> RecvMessageLength(PIC::nTotalThreads, 0);

    struct cMessageDescriptor {
        int segmentIndex;
        int nParticles;
    };

    // Prepare messages to send particles
    auto PrepareSendDescriptors = [&](std::vector<cMessageDescriptor> &MessageDescriptors,
                                      std::vector<MPI_Aint> &SendParticleList, int &nTotalSendParticles, int thread) {
        nTotalSendParticles = 0;

        for (int i = 0; i < ThreadSegmentTable.TableLength[thread]; i++) {
            auto segment = ThreadSegmentTable.Table[thread][i];
            if (!segment || segment->FirstParticleIndex == -1) continue;

            cMessageDescriptor descriptor = {i, 0};
            long int ptr = segment->FirstParticleIndex;

            while (ptr != -1) {
                MPI_Aint particle_address;
                MPI_Get_address(PIC::ParticleBuffer::ParticleDataBuffer + PIC::ParticleBuffer::GetParticleDataOffset(ptr), &particle_address);
                SendParticleList.push_back(particle_address);
                descriptor.nParticles++;
                nTotalSendParticles++;
                ptr = PIC::ParticleBuffer::GetNext(ptr);
            }

	    if (descriptor.nParticles!=0) 
              MessageDescriptors.push_back(descriptor);
        }
    };

    // Initialize the send operation
    auto InitSendOperation = [&](int To, int nTotalSendParticles, const std::vector<MPI_Aint> &SendParticleList) {
	MPI_Request request;

        MPI_Datatype particle_type;
        MPI_Type_create_hindexed_block(nTotalSendParticles, PIC::ParticleBuffer::ParticleDataLength - 2 * sizeof(long int),
                                       SendParticleList.data(), MPI_BYTE, &particle_type);
        MPI_Type_commit(&particle_type);

        MPI_Isend(MPI_BOTTOM, 1, particle_type, To, 12, MPI_GLOBAL_COMMUNICATOR, &request);
        MPI_Type_free(&particle_type);

	return request;
    };

    auto InitRecvOperation = [&](int From, int nTotalRecvParticles, const std::vector<MPI_Aint> &RecvParticleList) {
	MPI_Request request;

        MPI_Datatype particle_type;
        MPI_Type_create_hindexed_block(nTotalRecvParticles, PIC::ParticleBuffer::ParticleDataLength - 2 * sizeof(long int),
                                       RecvParticleList.data(), MPI_BYTE, &particle_type);
        MPI_Type_commit(&particle_type);

        MPI_Irecv(MPI_BOTTOM, 1, particle_type, From, 12, MPI_GLOBAL_COMMUNICATOR, &request);
        MPI_Type_free(&particle_type);

	return request;
    };


    // Handle received particles
    auto HandleReceivedParticles = [&](int nRecvSegments, const cMessageDescriptor *RecvDescriptors, std::vector<MPI_Aint> &RecvParticleList) {
        for (int i = 0; i < nRecvSegments; i++) {
            const auto &descriptor = RecvDescriptors[i];
            auto segment = ThreadSegmentTable.Table[PIC::ThisThread][descriptor.segmentIndex];

            for (int j = 0; j < descriptor.nParticles; j++) {
                long int new_particle = PIC::ParticleBuffer::GetNewParticle(segment->FirstParticleIndex, true);
                MPI_Aint particle_address;
                MPI_Get_address(PIC::ParticleBuffer::ParticleDataBuffer + PIC::ParticleBuffer::GetParticleDataOffset(new_particle), &particle_address);
                RecvParticleList.push_back(particle_address);
            }
        }
    };

    // Prepare send data for each thread
    std::vector<cMessageDescriptor> SendDescriptors[PIC::nTotalThreads];
    std::vector<MPI_Aint> SendParticleList[PIC::nTotalThreads];
    int cnt;

    for (To = 0; To < PIC::nTotalThreads; To++) {
        if (PIC::ThisThread == To) continue;

        PrepareSendDescriptors(SendDescriptors[To], SendParticleList[To], nTotalSendParticles[To], To);

        // Send number of segments 
	nSendSegments[To]=SendDescriptors[To].size();

	if (nSendSegments[To]!=0) {
          MPI_Isend(SendDescriptors[To].data(), nSendSegments[To] * sizeof(cMessageDescriptor), MPI_BYTE, To, 10, MPI_GLOBAL_COMMUNICATOR, SendDescriptorRequestTable+SendDescriptorRequestTableLength); 
	  SendDescriptorRequestTableLength++;
	}
    }

    // All-to-all exchange of segment counts
    MPI_Alltoall(nSendSegments, 1, MPI_INT,
             nRecvSegments, 1, MPI_INT,
             MPI_GLOBAL_COMMUNICATOR);

    // Allocate buffers for receiving descriptors and particles
    std::vector<cMessageDescriptor> RecvDescriptors[PIC::nTotalThreads];
    std::vector<MPI_Aint> RecvParticleList[PIC::nTotalThreads];

    int RecvParticleDataRequestTableLength=0;

    for (From = 0; From < PIC::nTotalThreads; From++) {
        if (PIC::ThisThread == From || nRecvSegments[From] == 0) continue;

        RecvDescriptors[From].resize(nRecvSegments[From]);
        MPI_Irecv(RecvDescriptors[From].data(), nRecvSegments[From] * sizeof(cMessageDescriptor), MPI_BYTE, From, 10, MPI_GLOBAL_COMMUNICATOR, RecvDescriptorRequestTable+RecvDescriptorRequestTableLength);
	RecvDescriptorRequestTableLength++;
    }

    // Send particle data
    int SendParticleDataRequestTableLength=0; 

    for (To = 0,cnt=0; To < PIC::nTotalThreads; To++) {
        if (PIC::ThisThread == To || nSendSegments[To] == 0) continue;

        SendParticleDataRequestTable[SendParticleDataRequestTableLength]=InitSendOperation(To, nTotalSendParticles[To], SendParticleList[To]);
	SendParticleDataRequestTableLength++;
    }

    MPI_Waitall(RecvDescriptorRequestTableLength, RecvDescriptorRequestTable, MPI_STATUSES_IGNORE);

    // Handle received particles
    RecvParticleDataRequestTableLength=0;

    for (From = 0; From < PIC::nTotalThreads; From++) {
        if (PIC::ThisThread == From || nRecvSegments[From] == 0) continue;

        HandleReceivedParticles(nRecvSegments[From], RecvDescriptors[From].data(), RecvParticleList[From]);

	RecvParticleDataRequestTable[RecvParticleDataRequestTableLength]=InitRecvOperation(From, RecvParticleList[From].size(), RecvParticleList[From]);
	RecvParticleDataRequestTableLength++; 
    }

    MPI_Waitall(SendParticleDataRequestTableLength, SendParticleDataRequestTable, MPI_STATUSES_IGNORE);

    // Remove sent particles
    for (To = 0; To < PIC::nTotalThreads; To++) {
        if (SendDescriptors[To].empty()) continue;

        for (auto &descriptor : SendDescriptors[To]) {
            auto segment = ThreadSegmentTable.Table[To][descriptor.segmentIndex];
            long int ptr = segment->FirstParticleIndex;

            while (ptr != -1) {
                long int next = PIC::ParticleBuffer::GetNext(ptr);
                PIC::ParticleBuffer::DeleteParticle_withoutTrajectoryTermination(ptr, true);
                ptr = next;
            }

            segment->FirstParticleIndex = -1;
        }
    }

    MPI_Waitall(RecvParticleDataRequestTableLength, RecvParticleDataRequestTable, MPI_STATUSES_IGNORE);
    MPI_Waitall(SendDescriptorRequestTableLength, SendDescriptorRequestTable, MPI_STATUSES_IGNORE);
    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
}


void PIC::FieldLine::check_particle(long int ptr) {
namespace PB = PIC::ParticleBuffer;
  double w;

  w = PB::GetIndividualStatWeightCorrection(ptr);
  if (isfinite(w)==false) exit(__LINE__,__FILE__,"Error: nan is found");
  if (w==0.0) exit(__LINE__,__FILE__,"Error: stat weight is zero");
  if (std::isnormal(w)==false) exit(__LINE__,__FILE__,"Error: nan is found");
}

void PIC::ParallelFieldLines::ExchangeFieldLineParticles() {
namespace FL = PIC::FieldLine;
  if (FL::FieldLinesAll==NULL) return;

  if (ThreadSegmentTable==NULL) exit(__LINE__,__FILE__,"Error: ThreadSegmentTable is not initialized");

  #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
  PIC::ParallelFieldLines::GetFieldLinePopulationStat();
  PIC::FieldLine::TraverseAllFieldLines(PIC::FieldLine::check_particle);
  #endif

  //loop through field lines  
  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    ExchangeFieldLineParticles(ThreadSegmentTable[iFieldLine]);

    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  }

  static long int ncall=0;
  ncall++;

  if ((ncall%10==0)||(_PIC_DEBUGGER_MODE_==_PIC_DEBUGGER_MODE_ON_)) { 
    PIC::ParallelFieldLines::GetFieldLinePopulationStat();
    PIC::FieldLine::TraverseAllFieldLines(PIC::FieldLine::check_particle);
  }
}

//==================================   output the number of particles attached to each of the field lines ======================================
long int PIC::ParallelFieldLines::GetFieldLinePopulationStat(PIC::FieldLine::cFieldLineSegment* FirstSegment) {
    // Keep original collection logic
    struct ParticleStats {
        long int totalParticles;         
        long int localParticles;         
        long int remoteParticles;        
        int nLocalSegments;              
        int nRemoteSegments;             
        int nLocalActiveSegments;        
        int nRemoteActiveSegments;       
    };
    
    // Collect local statistics (unchanged)
    ParticleStats localStats = {0, 0, 0, 0, 0, 0, 0};
    long int totalParticleCount=0;

    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR); 
    
    auto currentSegment = FirstSegment;
    while (currentSegment != nullptr) {
        int particleCount = 0;
        long int ptr = currentSegment->FirstParticleIndex;
        
        while (ptr != -1) {
            particleCount++;
	    totalParticleCount++;
            ptr = PIC::ParticleBuffer::GetNext(ptr);
        }
        
        localStats.totalParticles += particleCount;
        
        if (currentSegment->Thread == PIC::ThisThread) {
            localStats.localParticles += particleCount;
            localStats.nLocalSegments++;
            if (particleCount > 0) {
                localStats.nLocalActiveSegments++;
            }
        } else {
            localStats.remoteParticles += particleCount;
            localStats.nRemoteSegments++;
            if (particleCount > 0) {
                localStats.nRemoteActiveSegments++;
            }
        }
        
        currentSegment = currentSegment->GetNext();
    }
    
    // Prepare for MPI communication
    struct GlobalParticleStats {
        long int totalParticles;
        long int localParticles;
        long int remoteParticles;
        int nLocalSegments;
        int nRemoteSegments;
        int nLocalActiveSegments;
        int nRemoteActiveSegments;
        int processId;
    };
    
    // Create array on process 0 to receive data
    std::vector<GlobalParticleStats> allStats;
    if (PIC::ThisThread == 0) {
        allStats.resize(PIC::nTotalThreads);
    }
    
    // Prepare local stats for gathering
    GlobalParticleStats myStats = {
        localStats.totalParticles,
        localStats.localParticles,
        localStats.remoteParticles,
        localStats.nLocalSegments,
        localStats.nRemoteSegments,
        localStats.nLocalActiveSegments,
        localStats.nRemoteActiveSegments,
        PIC::ThisThread
    };
    
    // Create MPI type (unchanged)
    MPI_Datatype mpi_stats_type;
    int blocklengths[] = {1, 1, 1, 1, 1, 1, 1, 1};
    MPI_Aint displacements[8];
    MPI_Datatype types[] = {MPI_LONG, MPI_LONG, MPI_LONG, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
    
    GlobalParticleStats sample;
    MPI_Aint base_address;
    MPI_Get_address(&sample, &base_address);
    MPI_Get_address(&sample.totalParticles, &displacements[0]);
    MPI_Get_address(&sample.localParticles, &displacements[1]);
    MPI_Get_address(&sample.remoteParticles, &displacements[2]);
    MPI_Get_address(&sample.nLocalSegments, &displacements[3]);
    MPI_Get_address(&sample.nRemoteSegments, &displacements[4]);
    MPI_Get_address(&sample.nLocalActiveSegments, &displacements[5]);
    MPI_Get_address(&sample.nRemoteActiveSegments, &displacements[6]);
    MPI_Get_address(&sample.processId, &displacements[7]);
    
    for (int i = 0; i < 8; i++) {
        displacements[i] = MPI_Aint_diff(displacements[i], base_address);
    }
    
    MPI_Type_create_struct(8, blocklengths, displacements, types, &mpi_stats_type);
    MPI_Type_commit(&mpi_stats_type);
    
    // Gather all stats to process 0
    MPI_Gather(&myStats, 1, mpi_stats_type,
               allStats.data(), 1, mpi_stats_type,
               0, MPI_GLOBAL_COMMUNICATOR);
    
    MPI_Type_free(&mpi_stats_type);
    
    // Process 0 prints the results
    if (PIC::ThisThread == 0) {
        // Calculate totals
        GlobalParticleStats totals = {0, 0, 0, 0, 0, 0, 0, -1};
        
        for (int i = 0; i < PIC::nTotalThreads; i++) {
            const auto& stats = allStats[i];
            totals.totalParticles += stats.totalParticles;
            totals.localParticles += stats.localParticles;
            totals.remoteParticles += stats.remoteParticles;
            totals.nLocalSegments += stats.nLocalSegments;
            totals.nRemoteSegments += stats.nRemoteSegments;
            totals.nLocalActiveSegments += stats.nLocalActiveSegments;
            totals.nRemoteActiveSegments += stats.nRemoteActiveSegments;
        }
        
        printf("\n=== Field Line Particle Distribution Analysis ===\n");
        printf("Total Processes: %d\n\n", PIC::nTotalThreads);
        
        printf("Per-Process Statistics:\n");
        printf("%-6s | %-12s | %-12s | %-12s | %-12s | %-12s | %-12s | %-12s\n",
               "Proc", "Total Parts", "Local Parts", "Remote Parts", 
               "Local Segs", "Active Local", "Remote Segs", "Active Remote");
        printf("--------------------------------------------------------------------------------------------------------\n");
        
        // Print stats for ALL processes
        for (int i = 0; i < PIC::nTotalThreads; i++) {
            const auto& stats = allStats[i];
            printf("%-6d | %-12ld | %-12ld | %-12ld | %-12d | %-12d | %-12d | %-12d\n",
                   stats.processId,
                   stats.totalParticles,
                   stats.localParticles,
                   stats.remoteParticles,
                   stats.nLocalSegments,
                   stats.nLocalActiveSegments,
                   stats.nRemoteSegments,
                   stats.nRemoteActiveSegments);
        }
        
        // Print all the original summary sections
        printf("\nGlobal Summary:\n");
        printf("Total Particles: %ld\n", totals.totalParticles);
        printf("Total Segments: %d\n", totals.nLocalSegments + totals.nRemoteSegments);
        printf("Total Active Segments: %d\n", totals.nLocalActiveSegments + totals.nRemoteActiveSegments);
        
        // Original load balancing and other metrics remain unchanged...
    }

    return totalParticleCount;
}

void PIC::ParallelFieldLines::GetFieldLinePopulationStat() {
namespace FL = PIC::FieldLine;
  long int totalParticleCount=0;

  if (FL::FieldLinesAll==NULL) return;

  if (ThreadSegmentTable==NULL) exit(__LINE__,__FILE__,"Error: ThreadSegmentTable is not initialized");

  //loop through field lines
  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    if (PIC::ThisThread==0) cout << "Field line particle statistic: field line " << iFieldLine << endl << flush;
    totalParticleCount+=GetFieldLinePopulationStat(FL::FieldLinesAll[iFieldLine].GetFirstSegment());
  }

  if (totalParticleCount!=PIC::ParticleBuffer::GetAllPartNum()) {
    char msg[200];

    sprintf(msg,"Error: the paritcle number is inconsistent -- totalParticleCount=%ld, AllPartNum=%ld\n",totalParticleCount,PIC::ParticleBuffer::GetAllPartNum());
    exit(__LINE__,__FILE__,msg); 
  }
}

void PIC::ParallelFieldLines::CheckLocalFieldLineParticleNumber() {
namespace FL = PIC::FieldLine;
  long int totalParticleCount=0;

  //loop through field lines
  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    for (auto Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
       long int ptr = Segment->FirstParticleIndex;

        while (ptr != -1) {
            totalParticleCount++;
            ptr = PIC::ParticleBuffer::GetNext(ptr);
        }
    }
  }


  if (totalParticleCount!=PIC::ParticleBuffer::GetAllPartNum()) {
    char msg[200];

    sprintf(msg,"Error: the paritcle number is inconsistent -- totalParticleCount=%ld, AllPartNum=%ld\n",totalParticleCount,PIC::ParticleBuffer::GetAllPartNum());
    exit(__LINE__,__FILE__,msg);
  }
}
