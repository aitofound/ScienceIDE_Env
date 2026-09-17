/*
PURPOSE:
--------
Ultra-simplified MPI communication for DatumStoredAtEdge data across field line segments.
Uses direct field line traversal with no state management for maximum simplicity.

KEY FEATURES:
-------------
1. NO STATE: No initialization, cleanup, or caching - just pure functions
2. DIRECT TRAVERSAL: Uses standard loops over field lines and segments
3. BUFFER-BASED: Uses std::vector<double> for all MPI operations
4. ZERO SETUP: No setup/teardown required - just call the functions
5. CONSISTENT PATTERN: All functions use same pack/MPI/unpack strategy

CORE FUNCTIONS:
---------------
- MPIAllReduceDatumStoredAtEdge(S)           // Sum across all processes
- MPIReduceDatumStoredAtEdge(S, root_rank)   // Reduce to root process
- MPIBcastDatumStoredAtEdge(S, root_rank)    // Broadcast from root
- MPIGatherDatumStoredAtEdge(S, root_rank)   // Gather to root process
- MPIAllGatherDatumStoredAtEdge(S)           // Gather to all processes
- MPIAllReduceDatumStoredAtEdgeFieldLine(field_line_idx, S)    // All-reduce single field line
- MPIReduceDatumStoredAtEdgeFieldLine(field_line_idx, S, root_rank)   // Reduce single field line
- MPIBcastDatumStoredAtEdgeFieldLine(field_line_idx, S, root_rank)    // Broadcast single field line
- MPIGatherDatumStoredAtEdgeFieldLine(field_line_idx, S, root_rank)   // Gather single field line
- MPIAllGatherDatumStoredAtEdgeFieldLine(field_line_idx, S)    // All-gather single field line

USAGE EXAMPLES:
===============

EXAMPLE 1: Sum particle counts across all processes
---------------------------------------------------
```cpp
// Each process has computed local particle counts
PIC::Datum::cDatumStored particleCounts;
// ... local computation fills particleCounts ...

// Sum all values across all processes (result on all processes)
PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtEdge(particleCounts);

// Now each process has the global sum in all segments
std::cout << "Global particle counts computed" << std::endl;
```

EXAMPLE 2: Distribute initial conditions from master process
------------------------------------------------------------
```cpp
int rank;
MPI_Comm_rank(MPI_COMM_WORLD, &rank);

PIC::Datum::cDatumStored initialConditions;

if (rank == 0) {
    // Master process sets up initial conditions
    std::cout << "Setting up initial conditions..." << std::endl;
    // ... fill initialConditions with setup data ...
}

// Broadcast from rank 0 to all other processes
PIC::FieldLine::Parallel::MPIBcastDatumStoredAtEdge(initialConditions, 0);

// Now all processes have the initial conditions
std::cout << "Process " << rank << " received initial conditions" << std::endl;
```

EXAMPLE 3: Collect simulation results for output
------------------------------------------------
```cpp
int rank;
MPI_Comm_rank(MPI_COMM_WORLD, &rank);

PIC::Datum::cDatumStored simulationResults;
// ... each process computes local results ...

// Collect all results to rank 0 for file output
PIC::FieldLine::Parallel::MPIGatherDatumStoredAtEdge(simulationResults, 0);

if (rank == 0) {
    std::cout << "Writing complete results to file..." << std::endl;
    // ... write results to file ...
    // Root now has data from all processes
}
```

EXAMPLE 4: Share data among all processes for analysis
------------------------------------------------------
```cpp
PIC::Datum::cDatumStored analysisData;
// ... each process computes local analysis data ...

// Every process gets data from every other process
PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(analysisData);

// Now every process can perform global analysis
std::cout << "Performing global analysis with complete dataset" << std::endl;
```

EXAMPLE 5: Work with specific field lines
-----------------------------------------
```cpp
// Sum data for field line 2 only across all processes
PIC::Datum::cDatumStored fieldLine2Data;
PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtEdgeFieldLine(2, fieldLine2Data);

// Reduce field line 1 data to rank 0
PIC::FieldLine::Parallel::MPIReduceDatumStoredAtEdgeFieldLine(1, fieldLine2Data, 0);

// Broadcast field line 0 data from rank 0 to all processes
PIC::FieldLine::Parallel::MPIBcastDatumStoredAtEdgeFieldLine(0, fieldLine2Data, 0);

// Gather field line 3 data from all processes to rank 0
PIC::FieldLine::Parallel::MPIGatherDatumStoredAtEdgeFieldLine(3, fieldLine2Data, 0);

// Collect field line 0 data from all processes to all processes
PIC::Datum::cDatumStored fieldLine0Data;
PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdgeFieldLine(0, fieldLine0Data);
```

EXAMPLE 6: Complete simulation workflow
---------------------------------------
```cpp
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // NO INITIALIZATION NEEDED - just use the functions directly!
    
    // Step 1: Broadcast boundary conditions from rank 0
    PIC::Datum::cDatumStored boundaryConditions;
    if (rank == 0) {
        // Set up boundary conditions
    }
    PIC::FieldLine::Parallel::MPIBcastDatumStoredAtEdge(boundaryConditions, 0);
    
    // Step 2: Each process does local computation
    PIC::Datum::cDatumStored localResults;
    // ... compute local physics ...
    
    // Step 3: Sum shared quantities across all processes
    PIC::Datum::cDatumStored sharedQuantities;
    PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtEdge(sharedQuantities);
    
    // Step 4: Continue computation with updated shared data
    // ... more local computation ...
    
    // Step 5: Gather final results to rank 0 for output
    PIC::Datum::cDatumStored finalResults;
    PIC::FieldLine::Parallel::MPIGatherDatumStoredAtEdge(finalResults, 0);
    
    if (rank == 0) {
        std::cout << "Simulation complete - writing output" << std::endl;
    }
    
    // NO CLEANUP NEEDED - just finalize MPI
    MPI_Finalize();
    return 0;
}
```

EXAMPLE 7: Multi-physics coupling
---------------------------------
```cpp
// Electric field computation
PIC::Datum::cDatumStored electricField;
// ... compute local electric field ...
PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtEdge(electricField);

// Magnetic field computation  
PIC::Datum::cDatumStored magneticField;
// ... compute local magnetic field ...
PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtEdge(magneticField);

// Particle dynamics (needs data from all processes)
PIC::Datum::cDatumStored particleData;
PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(particleData);

std::cout << "Multi-physics coupling complete" << std::endl;
```

EXAMPLE 8: Load balancing check
-------------------------------
```cpp
// Count computational load per process
PIC::Datum::cDatumStored workLoad;
// ... compute local work metrics ...

// Everyone gets everyone's load data
PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(workLoad);

// Now each process can assess if load balancing is needed
// ... analyze load distribution ...
std::cout << "Load balancing analysis complete" << std::endl;
```

TECHNICAL DETAILS:
==================

MEMORY STRATEGY:
---------------
- Uses temporary std::vector<double> buffers for each operation
- No persistent state or caches
- Predictable memory usage
- Automatic cleanup when functions return

FIELD LINE TRAVERSAL:
--------------------
All functions use this standard pattern:
```cpp
for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
    for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
         Segment != NULL; 
         Segment = Segment->GetNext()) {
        // Process segment data
    }
}
```

ERROR HANDLING:
--------------
- Simple error messages to stderr
- Graceful handling of empty field lines
- MPI error code checking
- Clear error context information

PERFORMANCE:
-----------
- Direct traversal cost: O(segments) per operation
- Memory usage: O(segments * S.length) per operation
- No cache overhead or management
- Simple, predictable performance characteristics

THREAD SAFETY:
--------------
- No static variables or persistent state
- Each function call is independent
- Safe for single-threaded MPI programs
- No synchronization needed
*/

#include "pic.h"
#include <iostream>
#include <vector>

namespace PIC {
namespace FieldLine {
namespace Parallel {

// ============================================================================
// Helper functions for buffer operations - no caching, direct traversal
// ============================================================================

// Count total segments across all field lines
int CountTotalSegments() {
    int total_segments = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            total_segments++;
        }
    }
    
    return total_segments;
}

// Count segments for a specific field line
int CountFieldLineSegments(int field_line_idx) {
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        return 0;
    }
    
    int segments = 0;
    for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
         Segment != NULL; 
         Segment = Segment->GetNext()) {
        segments++;
    }
    
    return segments;
}

// Count segments for a specific process
int CountProcessSegments(int target_process) {
    int process_segments = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            if (Segment->Thread == target_process) {
                process_segments++;
            }
        }
    }
    
    return process_segments;
}

// Pack all field line data into a contiguous buffer
int PackAllFieldLinesData(const cDatumStored& S, std::vector<double>& buffer) {
    int total_segments = CountTotalSegments();
    int total_elements = total_segments * S.length;
    
    buffer.resize(total_elements, 0.0);
    
    if (total_segments == 0) {
        return 0;
    }
    
    // Pack data using direct traversal
    cDatumStored S_copy = S;
    int element_index = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            double* seg_data = Segment->GetDatum_ptr(S_copy);
            if (seg_data) {
                for (int i = 0; i < S.length; i++) {
                  buffer[element_index++] = seg_data[i];

                  if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                    validate_numeric(seg_data[i],__LINE__,__FILE__);
		  }
                }
            } else {
                // Leave as zeros if no data
                element_index += S.length;
            }
        }
    }
    
    return total_segments;
}

// Unpack buffer data back to field line segments
void UnpackAllFieldLinesData(const cDatumStored& S, const std::vector<double>& buffer) {
    cDatumStored S_copy = S;
    int element_index = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            double* seg_data = Segment->GetDatum_ptr(S_copy);
            if (seg_data) {
                for (int i = 0; i < S.length; i++) {
                  seg_data[i] = buffer[element_index++];

                  if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                    validate_numeric(seg_data[i],__LINE__,__FILE__);
                  }
                }
            } else {
                // Skip if no data pointer
                element_index += S.length;
            }
        }
    }
}

// Pack data for specific field line
int PackFieldLineData(int field_line_idx, const cDatumStored& S, std::vector<double>& buffer) {
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        buffer.clear();
        return 0;
    }
    
    int segments = CountFieldLineSegments(field_line_idx);
    int total_elements = segments * S.length;
    
    buffer.resize(total_elements, 0.0);
    
    if (segments == 0) {
        return 0;
    }
    
    // Pack data for this field line
    cDatumStored S_copy = S;
    int element_index = 0;
    
    for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
         Segment != NULL; 
         Segment = Segment->GetNext()) {
        
        double* seg_data = Segment->GetDatum_ptr(S_copy);
        if (seg_data) {
            for (int i = 0; i < S.length; i++) {
                buffer[element_index++] = seg_data[i];
            }
        } else {
            element_index += S.length;  // Leave as zeros
        }
    }
    
    return segments;
}

// Unpack data for specific field line
void UnpackFieldLineData(int field_line_idx, const cDatumStored& S, const std::vector<double>& buffer) {
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        return;
    }
    
    cDatumStored S_copy = S;
    int element_index = 0;
    
    for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
         Segment != NULL; 
         Segment = Segment->GetNext()) {
        
        double* seg_data = Segment->GetDatum_ptr(S_copy);
        if (seg_data) {
            for (int i = 0; i < S.length; i++) {
                seg_data[i] = buffer[element_index++];
            }
        } else {
            element_index += S.length;  // Skip
        }
    }
}

// Pack data for specific process (for scatter/gather operations)
int PackProcessData(int target_process, const cDatumStored& S, std::vector<double>& buffer) {
    int process_segments = CountProcessSegments(target_process);
    int process_elements = process_segments * S.length;
    
    buffer.resize(process_elements, 0.0);
    
    if (process_segments == 0) {
        return 0;
    }
    
    // Pack data for this process
    cDatumStored S_copy = S;
    int element_index = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            if (Segment->Thread == target_process) {
                double* seg_data = Segment->GetDatum_ptr(S_copy);
                if (seg_data) {
                    for (int i = 0; i < S.length; i++) {
                        buffer[element_index++] = seg_data[i];
                    }
                } else {
                    element_index += S.length;  // Leave as zeros
                }
            }
        }
    }
    
    return process_segments;
}

// Unpack data for specific process
void UnpackProcessData(int target_process, const cDatumStored& S, const std::vector<double>& buffer) {
    cDatumStored S_copy = S;
    int element_index = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            if (Segment->Thread == target_process) {
                double* seg_data = Segment->GetDatum_ptr(S_copy);
                if (seg_data) {
                    for (int i = 0; i < S.length; i++) {
                        seg_data[i] = buffer[element_index++];
                    }
                } else {
                    element_index += S.length;  // Skip
                }
            }
        }
    }
}

// ============================================================================
// MPI Operations - Ultra-simplified implementation
// ============================================================================

void MPIAllReduceDatumStoredAtEdge(const cDatumStored& S) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // Pack all data into buffer
    std::vector<double> buffer;
    int total_segments = PackAllFieldLinesData(S, buffer);
    
    if (total_segments == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No segments found for all-reduce operation" << std::endl;
        }
        return;
    }
    
    // Single MPI_Allreduce call
    int result = MPI_Allreduce(MPI_IN_PLACE, buffer.data(), buffer.size(), 
                               MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Allreduce failed with code " << result << std::endl;
        return;
    }
    
    // Unpack results back to segments
    UnpackAllFieldLinesData(S, buffer);
    
    if (rank == 0) {
        std::cout << "Completed MPI all-reduce for " << total_segments 
                  << " segments (" << buffer.size() << " elements)" << std::endl;
    }
}

void MPIReduceDatumStoredAtEdge(cDatumStored& S, int root_rank) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (root_rank < 0 || root_rank >= size) {
        std::cerr << "Error: Invalid root_rank " << root_rank << std::endl;
        return;
    }

    // Pack send data
    std::vector<double> send_buffer;
    int total_segments = PackAllFieldLinesData(S, send_buffer);
    
    if (total_segments == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No segments found for reduce operation" << std::endl;
        }
        return;
    }
    
    // Prepare receive buffer (only on root)
    std::vector<double> recv_buffer;
    if (rank == root_rank) {
        recv_buffer.resize(send_buffer.size(), 0.0);
    }
    
    // MPI_Reduce operation
    int result = MPI_Reduce(send_buffer.data(), 
                           (rank == root_rank) ? recv_buffer.data() : nullptr,
                           send_buffer.size(), MPI_DOUBLE, MPI_SUM, 
                           root_rank, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Reduce failed with code " << result << std::endl;
        return;
    }
    
    // Unpack results (only on root)
    if (rank == root_rank) {
        UnpackAllFieldLinesData(S, recv_buffer);
        std::cout << "Completed MPI reduce for " << total_segments 
                  << " segments to root rank " << root_rank << std::endl;
    }
}

void MPIBcastDatumStoredAtEdge(cDatumStored& S, int root_rank) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (root_rank < 0 || root_rank >= size) {
        std::cerr << "Error: Invalid root_rank " << root_rank << std::endl;
        return;
    }

    // Pack data into buffer
    std::vector<double> buffer;
    int total_segments = PackAllFieldLinesData(S, buffer);
    
    if (total_segments == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No segments found for broadcast operation" << std::endl;
        }
        return;
    }
    
    // Broadcast buffer
    int result = MPI_Bcast(buffer.data(), buffer.size(), MPI_DOUBLE, 
                          root_rank, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Bcast failed with code " << result << std::endl;
        return;
    }
    
    // Unpack data back to segments
    UnpackAllFieldLinesData(S, buffer);
    
    if (rank == root_rank) {
        std::cout << "Completed MPI broadcast for " << total_segments 
                  << " segments from root rank " << root_rank << std::endl;
    }
}

void MPIGatherDatumStoredAtEdge(cDatumStored& S, int root_rank) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (root_rank < 0 || root_rank >= size) {
        std::cerr << "Error: Invalid root_rank " << root_rank << std::endl;
        return;
    }

    if (rank != root_rank) {
        // Non-root: send process-specific data
        std::vector<double> send_buffer;
        int segments_count = PackProcessData(rank, S, send_buffer);
        
        if (segments_count > 0) {
            // Send buffer size first, then data
            int buffer_size = send_buffer.size();
            MPI_Send(&buffer_size, 1, MPI_INT, root_rank, rank * 2, MPI_COMM_WORLD);
            MPI_Send(send_buffer.data(), buffer_size, MPI_DOUBLE, root_rank, rank * 2 + 1, MPI_COMM_WORLD);
        } else {
            // Send zero size to indicate no data
            int buffer_size = 0;
            MPI_Send(&buffer_size, 1, MPI_INT, root_rank, rank * 2, MPI_COMM_WORLD);
        }
    } else {
        // Root: receive from all other processes
        for (int proc = 0; proc < size; ++proc) {
            if (proc == root_rank) {
                continue;  // Skip self
            }
            
            // Receive buffer size first
            int buffer_size;
            MPI_Recv(&buffer_size, 1, MPI_INT, proc, proc * 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            if (buffer_size > 0) {
                // Receive data and unpack
                std::vector<double> recv_buffer(buffer_size);
                MPI_Recv(recv_buffer.data(), buffer_size, MPI_DOUBLE, proc, proc * 2 + 1, 
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                UnpackProcessData(proc, S, recv_buffer);
            }
        }
        
        std::cout << "Completed MPI gather for DatumStoredAtEdge data to root rank " 
                  << root_rank << std::endl;
    }
}

void MPIAllReduceDatumStoredAtEdgeFieldLine(int field_line_idx, cDatumStored& S) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        std::cerr << "Error: Invalid field_line_idx " << field_line_idx << std::endl;
        return;
    }

    // Pack data for this field line
    std::vector<double> buffer;
    int segment_count = PackFieldLineData(field_line_idx, S, buffer);
    
    if (segment_count == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No segments found for field line " << field_line_idx << std::endl;
        }
        return;
    }
    
    // All-reduce for this field line
    int result = MPI_Allreduce(MPI_IN_PLACE, buffer.data(), buffer.size(), 
                               MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Allreduce failed for field line " << field_line_idx 
                  << " with code " << result << std::endl;
        return;
    }
    
    // Unpack results
    UnpackFieldLineData(field_line_idx, S, buffer);
    
    if (rank == 0) {
        std::cout << "Completed MPI all-reduce for field line " << field_line_idx 
                  << " (" << segment_count << " segments)" << std::endl;
    }
}

void MPIAllGatherDatumStoredAtEdge(cDatumStored& S) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Pack process-specific data
    std::vector<double> send_buffer;
    int my_segments = PackProcessData(rank, S, send_buffer);
    int my_buffer_size = send_buffer.size();
    
    // Gather buffer sizes from all processes
    std::vector<int> all_buffer_sizes(size);
    int result = MPI_Allgather(&my_buffer_size, 1, MPI_INT, 
                               all_buffer_sizes.data(), 1, MPI_INT, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Allgather (sizes) failed with code " << result << std::endl;
        return;
    }
    
    // Calculate total buffer size and displacements
    int total_buffer_size = 0;
    std::vector<int> displacements(size);
    for (int i = 0; i < size; i++) {
        displacements[i] = total_buffer_size;
        total_buffer_size += all_buffer_sizes[i];
    }
    
    // Prepare receive buffer for all data
    std::vector<double> all_data(total_buffer_size);
    
    // Gather all process data
    result = MPI_Allgatherv(send_buffer.data(), my_buffer_size, MPI_DOUBLE,
                           all_data.data(), all_buffer_sizes.data(), 
                           displacements.data(), MPI_DOUBLE, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Allgatherv failed with code " << result << std::endl;
        return;
    }
    
    // Unpack data from each process
    for (int proc = 0; proc < size; proc++) {
        if (all_buffer_sizes[proc] > 0) {
            std::vector<double> proc_buffer(all_buffer_sizes[proc]);
            std::copy(all_data.begin() + displacements[proc],
                     all_data.begin() + displacements[proc] + all_buffer_sizes[proc],
                     proc_buffer.begin());
            
            UnpackProcessData(proc, S, proc_buffer);
        }
    }
    
    if (rank == 0) {
        int total_segments = 0;
        for (int i = 0; i < size; i++) {
            if (all_buffer_sizes[i] > 0) {
                total_segments += all_buffer_sizes[i] / S.length;
            }
        }
        std::cout << "Completed MPI all-gather for " << total_segments 
                  << " total segments from " << size << " processes" << std::endl;
    }
}

void MPIAllGatherDatumStoredAtEdgeFieldLine(int field_line_idx, cDatumStored& S) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        std::cerr << "Error: Invalid field_line_idx " << field_line_idx << std::endl;
        return;
    }

    // Pack data for this field line from current process
    std::vector<double> send_buffer;
    int my_segments = 0;
    
    // Count and pack segments from this field line that belong to current process
    cDatumStored S_copy = S;
    for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
         Segment != NULL; 
         Segment = Segment->GetNext()) {
        
        if (Segment->Thread == rank) {
            double* seg_data = Segment->GetDatum_ptr(S_copy);
            if (seg_data) {
                for (int i = 0; i < S.length; i++) {
                    send_buffer.push_back(seg_data[i]);
                }
                my_segments++;
            } else {
                // Add zeros if no data
                for (int i = 0; i < S.length; i++) {
                    send_buffer.push_back(0.0);
                }
                my_segments++;
            }
        }
    }
    
    int my_buffer_size = send_buffer.size();
    
    // Gather buffer sizes from all processes
    std::vector<int> all_buffer_sizes(size);
    int result = MPI_Allgather(&my_buffer_size, 1, MPI_INT, 
                               all_buffer_sizes.data(), 1, MPI_INT, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Allgather (sizes) failed for field line " << field_line_idx 
                  << " with code " << result << std::endl;
        return;
    }
    
    // Calculate total buffer size and displacements
    int total_buffer_size = 0;
    std::vector<int> displacements(size);
    for (int i = 0; i < size; i++) {
        displacements[i] = total_buffer_size;
        total_buffer_size += all_buffer_sizes[i];
    }
    
    // Prepare receive buffer for all data
    std::vector<double> all_data(total_buffer_size);
    
    // Gather all process data for this field line
    result = MPI_Allgatherv(send_buffer.data(), my_buffer_size, MPI_DOUBLE,
                           all_data.data(), all_buffer_sizes.data(), 
                           displacements.data(), MPI_DOUBLE, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Allgatherv failed for field line " << field_line_idx 
                  << " with code " << result << std::endl;
        return;
    }
    
    // Unpack data from each process for this field line
    for (int proc = 0; proc < size; proc++) {
        if (all_buffer_sizes[proc] > 0) {
            std::vector<double> proc_buffer(all_buffer_sizes[proc]);
            std::copy(all_data.begin() + displacements[proc],
                     all_data.begin() + displacements[proc] + all_buffer_sizes[proc],
                     proc_buffer.begin());
            
            // Unpack to segments belonging to this process in this field line
            int element_index = 0;
            for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
                 Segment != NULL; 
                 Segment = Segment->GetNext()) {
                
                if (Segment->Thread == proc) {
                    double* seg_data = Segment->GetDatum_ptr(S_copy);
                    if (seg_data) {
                        for (int i = 0; i < S.length; i++) {
                            seg_data[i] = proc_buffer[element_index++];
                        }
                    } else {
                        element_index += S.length;  // Skip
                    }
                }
            }
        }
    }
    
    if (rank == 0) {
        int total_segments = 0;
        for (int i = 0; i < size; i++) {
            if (all_buffer_sizes[i] > 0) {
                total_segments += all_buffer_sizes[i] / S.length;
            }
        }
        std::cout << "Completed MPI all-gather for field line " << field_line_idx 
                  << " (" << total_segments << " total segments from " << size << " processes)" << std::endl;
    }
}

void MPIReduceDatumStoredAtEdgeFieldLine(int field_line_idx, cDatumStored& S, int root_rank) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        std::cerr << "Error: Invalid field_line_idx " << field_line_idx << std::endl;
        return;
    }
    
    if (root_rank < 0 || root_rank >= size) {
        std::cerr << "Error: Invalid root_rank " << root_rank << std::endl;
        return;
    }

    // Pack send data for this field line
    std::vector<double> send_buffer;
    int segment_count = PackFieldLineData(field_line_idx, S, send_buffer);
    
    if (segment_count == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No segments found for field line " << field_line_idx << std::endl;
        }
        return;
    }
    
    // Prepare receive buffer (only on root)
    std::vector<double> recv_buffer;
    if (rank == root_rank) {
        recv_buffer.resize(send_buffer.size(), 0.0);
    }
    
    // MPI_Reduce operation for this field line
    int result = MPI_Reduce(send_buffer.data(), 
                           (rank == root_rank) ? recv_buffer.data() : nullptr,
                           send_buffer.size(), MPI_DOUBLE, MPI_SUM, 
                           root_rank, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Reduce failed for field line " << field_line_idx 
                  << " with code " << result << std::endl;
        return;
    }
    
    // Unpack results (only on root)
    if (rank == root_rank) {
        UnpackFieldLineData(field_line_idx, S, recv_buffer);
        std::cout << "Completed MPI reduce for field line " << field_line_idx 
                  << " (" << segment_count << " segments) to root rank " << root_rank << std::endl;
    }
}

void MPIBcastDatumStoredAtEdgeFieldLine(int field_line_idx, cDatumStored& S, int root_rank) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        std::cerr << "Error: Invalid field_line_idx " << field_line_idx << std::endl;
        return;
    }
    
    if (root_rank < 0 || root_rank >= size) {
        std::cerr << "Error: Invalid root_rank " << root_rank << std::endl;
        return;
    }

    // Pack data for this field line
    std::vector<double> buffer;
    int segment_count = PackFieldLineData(field_line_idx, S, buffer);
    
    if (segment_count == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No segments found for field line " << field_line_idx << std::endl;
        }
        return;
    }
    
    // Broadcast buffer for this field line
    int result = MPI_Bcast(buffer.data(), buffer.size(), MPI_DOUBLE, 
                          root_rank, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Bcast failed for field line " << field_line_idx 
                  << " with code " << result << std::endl;
        return;
    }
    
    // Unpack data back to this field line
    UnpackFieldLineData(field_line_idx, S, buffer);
    
    if (rank == root_rank) {
        std::cout << "Completed MPI broadcast for field line " << field_line_idx 
                  << " (" << segment_count << " segments) from root rank " << root_rank << std::endl;
    }
}

void MPIGatherDatumStoredAtEdgeFieldLine(int field_line_idx, cDatumStored& S, int root_rank) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        std::cerr << "Error: Invalid field_line_idx " << field_line_idx << std::endl;
        return;
    }
    
    if (root_rank < 0 || root_rank >= size) {
        std::cerr << "Error: Invalid root_rank " << root_rank << std::endl;
        return;
    }

    if (rank != root_rank) {
        // Non-root: send process-specific data for this field line
        std::vector<double> send_buffer;
        int my_segments = 0;
        
        // Pack segments from this field line that belong to current process
        cDatumStored S_copy = S;
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            if (Segment->Thread == rank) {
                double* seg_data = Segment->GetDatum_ptr(S_copy);
                if (seg_data) {
                    for (int i = 0; i < S.length; i++) {
                        send_buffer.push_back(seg_data[i]);
                    }
                    my_segments++;
                } else {
                    // Add zeros if no data
                    for (int i = 0; i < S.length; i++) {
                        send_buffer.push_back(0.0);
                    }
                    my_segments++;
                }
            }
        }
        
        if (my_segments > 0) {
            // Send buffer size first, then data
            int buffer_size = send_buffer.size();
            MPI_Send(&buffer_size, 1, MPI_INT, root_rank, rank * 2, MPI_COMM_WORLD);
            MPI_Send(send_buffer.data(), buffer_size, MPI_DOUBLE, root_rank, rank * 2 + 1, MPI_COMM_WORLD);
        } else {
            // Send zero size to indicate no data
            int buffer_size = 0;
            MPI_Send(&buffer_size, 1, MPI_INT, root_rank, rank * 2, MPI_COMM_WORLD);
        }
    } else {
        // Root: receive from all other processes for this field line
        int total_segments_gathered = 0;
        
        for (int proc = 0; proc < size; ++proc) {
            if (proc == root_rank) {
                continue;  // Skip self
            }
            
            // Receive buffer size first
            int buffer_size;
            MPI_Recv(&buffer_size, 1, MPI_INT, proc, proc * 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            if (buffer_size > 0) {
                // Receive data and unpack
                std::vector<double> recv_buffer(buffer_size);
                MPI_Recv(recv_buffer.data(), buffer_size, MPI_DOUBLE, proc, proc * 2 + 1, 
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                // Unpack to segments belonging to this process in this field line
                cDatumStored S_copy = S;
                int element_index = 0;
                for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
                     Segment != NULL; 
                     Segment = Segment->GetNext()) {
                    
                    if (Segment->Thread == proc) {
                        double* seg_data = Segment->GetDatum_ptr(S_copy);
                        if (seg_data) {
                            for (int i = 0; i < S.length; i++) {
                                seg_data[i] = recv_buffer[element_index++];
                            }
                        } else {
                            element_index += S.length;  // Skip
                        }
                    }
                }
                
                total_segments_gathered += buffer_size / S.length;
            }
        }
        
        std::cout << "Completed MPI gather for field line " << field_line_idx 
                  << " (" << total_segments_gathered << " segments gathered) to root rank " 
                  << root_rank << std::endl;
    }
}

} // namespace Parallel
} // namespace FieldLine
} // namespace PIC
