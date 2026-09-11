/*
PURPOSE:
--------
MPI communication functions for DatumStoredAtVertex data across field line vertices.
Uses direct field line traversal with no state management for maximum simplicity.
Mirrors the functionality of the Edge-based functions but operates on vertex data.
UPDATED: Now accounts for species-dependent data storage (PIC::nTotalSpecies).

KEY FEATURES:
-------------
1. NO STATE: No initialization, cleanup, or caching - just pure functions
2. DIRECT TRAVERSAL: Uses standard loops over field lines and vertices
3. BUFFER-BASED: Uses std::vector<double> for all MPI operations
4. ZERO SETUP: No setup/teardown required - just call the functions
5. CONSISTENT PATTERN: All functions use same pack/MPI/unpack strategy
6. SPECIES-AWARE: Properly handles data size scaling with PIC::nTotalSpecies

DATA SIZE CALCULATION:
---------------------
Total elements per vertex = S->length * PIC::nTotalSpecies
Total buffer size = total_vertices * S->length * PIC::nTotalSpecies

This accounts for the fact that each vertex stores data for all species in the simulation.

CORE FUNCTIONS:
---------------
- MPIAllReduceDatumStoredAtVertex(S)           // Sum across all processes
- MPIReduceDatumStoredAtVertex(S, root_rank)   // Reduce to root process
- MPIBcastDatumStoredAtVertex(S, root_rank)    // Broadcast from root
- MPIAllReduceDatumStoredAtVertexFieldLine(field_line_idx, S)    // All-reduce single field line
- MPIReduceDatumStoredAtVertexFieldLine(field_line_idx, S, root_rank)   // Reduce single field line
- MPIBcastDatumStoredAtVertexFieldLine(field_line_idx, S, root_rank)    // Broadcast single field line

USAGE EXAMPLES:
===============

EXAMPLE 1: Sum particle density data across all processes (multi-species)
-------------------------------------------------------------------------
```cpp
// Each process has computed local particle density data at vertices
// Data includes contributions from all PIC::nTotalSpecies species
PIC::Datum::cDatum* particleDensityData;
// ... local computation fills particleDensityData for all species ...

// Sum all species data across all processes (result on all processes)
PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtVertex(particleDensityData);

// Now each process has the global particle density for all species at all vertices
std::cout << "Global particle density data computed for " << PIC::nTotalSpecies 
          << " species" << std::endl;
```

EXAMPLE 2: Distribute initial conditions from master process (multi-species)
---------------------------------------------------------------------------
```cpp
int rank;
MPI_Comm_rank(MPI_COMM_WORLD, &rank);

PIC::Datum::cDatum* initialConditions;

if (rank == 0) {
    // Master process sets up initial conditions for all species at vertices
    std::cout << "Setting up initial conditions for " << PIC::nTotalSpecies 
              << " species..." << std::endl;
    // ... fill initialConditions with setup data for all species ...
}

// Broadcast from rank 0 to all other processes
PIC::FieldLine::Parallel::MPIBcastDatumStoredAtVertex(initialConditions, 0);

// Now all processes have the initial conditions for all species
std::cout << "Process " << rank << " received initial conditions for all species" << std::endl;
```

EXAMPLE 3: Work with specific field line multi-species data
----------------------------------------------------------
```cpp
// Sum velocity data for field line 2 (all species) across all processes
PIC::Datum::cDatum* velocityData;
PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtVertexFieldLine(2, velocityData);

// Reduce field line 1 pressure data (all species) to rank 0
PIC::Datum::cDatum* pressureData;
PIC::FieldLine::Parallel::MPIReduceDatumStoredAtVertexFieldLine(1, pressureData, 0);

// Broadcast field line 0 temperature data (all species) from rank 0 to all processes
PIC::Datum::cDatum* temperatureData;
PIC::FieldLine::Parallel::MPIBcastDatumStoredAtVertexFieldLine(0, temperatureData, 0);
```
*/

#include "pic.h"
#include <iostream>
#include <vector>
#include <cmath>


namespace PIC {
namespace FieldLine {
namespace Parallel {

// ============================================================================
// Helper functions for vertex buffer operations - species-aware implementation
// ============================================================================

// Calculate effective data length accounting for species
inline int GetEffectiveDataLength(PIC::Datum::cDatum* S) {
    if (S == nullptr) {
        return 0;
    }
    return S->length * PIC::nTotalSpecies;
}

// Count total vertices across all field lines
int CountTotalVertices() {
    int total_vertices = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            // Count begin vertex (end vertex will be counted as begin of next segment)
            total_vertices++;
        }
        
        // Add the final vertex of the last segment
        PIC::FieldLine::cFieldLineSegment* LastSegment = FieldLinesAll[iFieldLine].GetLastSegment();
        if (LastSegment != NULL) {
            total_vertices++;
        }
    }
    
    return total_vertices;
}

// Count vertices for a specific field line
int CountFieldLineVertices(int field_line_idx) {
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        return 0;
    }
    
    int vertices = 0;
    for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
         Segment != NULL; 
         Segment = Segment->GetNext()) {
        
        // Count begin vertex
        vertices++;
    }
    
    // Add the final vertex of the last segment
    PIC::FieldLine::cFieldLineSegment* LastSegment = FieldLinesAll[field_line_idx].GetLastSegment();
    if (LastSegment != NULL) {
        vertices++;
    }
    
    return vertices;
}

// Count vertices for a specific process (operates on all segments regardless of Thread)
int CountProcessVertices(int target_process) {
    int process_vertices = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            // Count begin vertex (regardless of Segment->Thread)
            process_vertices++;
            
            // If this is the last segment, also count end vertex
            if (Segment->GetNext() == NULL) {
                process_vertices++;
            }
        }
    }
    
    return process_vertices;
}

// Return true for sampled vertex datums.  Sampled datums are accumulators
// created from particles during a sampling window.  They are diagnostics, not
// primary plasma or geometry state variables.  If a sampled datum contains NaN
// or Inf, it usually means an invalid particle contribution contaminated the
// diagnostic buffer; allowing that value into MPI reduction spreads the NaN to
// every rank.  Stored datums are not sanitized here because non-finite stored
// plasma/field state should remain a hard debugger error.
inline bool IsSampledVertexDatum(PIC::Datum::cDatum* S) {
    return (S!=nullptr) &&
           ((S->type == PIC::Datum::cDatum::Sampled_) ||
            (S->type == PIC::Datum::cDatum::Timed_) ||
            (S->type == PIC::Datum::cDatum::Weighted_));
}

// Conservative numerical range for sampled diagnostics.  The upper bound is far
// below DBL_MAX so summing several ranks cannot immediately overflow, but far
// above any meaningful sampled particle energy/speed/weight expected in AMPS.
inline bool IsSafeSampledVertexDatumValue(double value) {
    const double MaxAbsSampledVertexDatumValue = 1.0e300;
    return std::isfinite(value) && (std::fabs(value) < MaxAbsSampledVertexDatumValue);
}

inline bool ShouldPrintVertexDatumSanitizerWarning() {
    static int nWarningsPrinted = 0;
    if (nWarningsPrinted < 100) {
        nWarningsPrinted++;
        return true;
    }
    return false;
}

inline double SanitizeSampledVertexDatumForMPI(PIC::Datum::cDatum* S,
                                               double value,
                                               double* storage_location,
                                               const char* stage,
                                               const char* vertex_role,
                                               int iFieldLine,
                                               int species,
                                               int component,
                                               int buffer_index,
                                               PIC::FieldLine::cFieldLineSegment* Segment,
                                               PIC::FieldLine::cFieldLineVertex* Vertex) {
    if (IsSampledVertexDatum(S)==false) return value;
    if (IsSafeSampledVertexDatumValue(value)==true) return value;

    // A sampled diagnostic value is invalid.  Reset the completed sampling
    // buffer value itself, not only the MPI buffer entry.  Otherwise the same
    // stale NaN would be encountered again at the next output call.  This is a
    // downstream protection layer: the sampling code also validates new
    // contributions before they are accumulated, but this sanitizer catches
    // values that were already present in the completed buffer before the patch
    // or values that reached the buffer through another sampling path.
    if (storage_location!=nullptr) *storage_location = 0.0;

    if ((_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) &&
        ShouldPrintVertexDatumSanitizerWarning()) {
        std::cerr << "Warning: sanitized non-finite sampled field-line vertex datum before/after MPI reduction"
                  << "; stage=" << ((stage!=nullptr) ? stage : "unknown")
                  << "; datum=" << ((S!=nullptr) ? S->name : "NULL")
                  << "; rank=" << PIC::ThisThread
                  << "; field_line=" << iFieldLine
                  << "; vertex=" << ((vertex_role!=nullptr) ? vertex_role : "unknown")
                  << "; species=" << species
                  << "; component=" << component
                  << "; datum_length=" << ((S!=nullptr) ? S->length : -1)
                  << "; buffer_index=" << buffer_index
                  << "; bad_value=" << value
                  << "; segment_ptr=" << Segment
                  << "; vertex_ptr=" << Vertex
                  << std::endl;
    }

    return 0.0;
}

// Pack all field line vertex data into a contiguous buffer (species-aware)
int PackAllFieldLinesVertexData(PIC::Datum::cDatum* S, std::vector<double>& buffer) {
    int total_vertices = CountTotalVertices();
    int effective_data_length = GetEffectiveDataLength(S);
    int total_elements = total_vertices * effective_data_length;
    
    buffer.resize(total_elements, 0.0);
    
    if (total_vertices == 0 || effective_data_length == 0) {
        return 0;
    }
    
    // Pack data using direct traversal
    int element_index = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            // Pack begin vertex data for all species
            PIC::FieldLine::cFieldLineVertex* BeginVertex = Segment->GetBegin();
            if (BeginVertex) {
                double* vertex_data = BeginVertex->GetDatum_ptr(S,BeginVertex->GetCompletedSamplingOffset());
                if (vertex_data) {
                    // Pack data for all species
                    for (int species = 0; species < PIC::nTotalSpecies; species++) {
                        for (int i = 0; i < S->length; i++) {
                            int data_idx = species * S->length + i;
                            double value_to_pack = SanitizeSampledVertexDatumForMPI(
                                S,vertex_data[data_idx],vertex_data+data_idx,
                                "pack-before-all-rank-reduction","begin",
                                iFieldLine,species,i,element_index,Segment,BeginVertex);
                            buffer[element_index++] = value_to_pack;
                            
                            if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                                validate_numeric(value_to_pack, __LINE__, __FILE__);
                            }
                        }
                    }
                } else {
                    // Leave as zeros if no data
                    element_index += effective_data_length;
                }
            } else {
                element_index += effective_data_length;
            }
            
            // If this is the last segment, also pack end vertex
            if (Segment->GetNext() == NULL) {
                PIC::FieldLine::cFieldLineVertex* EndVertex = Segment->GetEnd();
                if (EndVertex) {
                    double* vertex_data = EndVertex->GetDatum_ptr(S,EndVertex->GetCompletedSamplingOffset());
                    if (vertex_data) {
                        // Pack data for all species
                        for (int species = 0; species < PIC::nTotalSpecies; species++) {
                            for (int i = 0; i < S->length; i++) {
                                int data_idx = species * S->length + i;
                                double value_to_pack = SanitizeSampledVertexDatumForMPI(
                                    S,vertex_data[data_idx],vertex_data+data_idx,
                                    "pack-before-all-rank-reduction","end",
                                    iFieldLine,species,i,element_index,Segment,EndVertex);
                                buffer[element_index++] = value_to_pack;
                                
                                if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                                    validate_numeric(value_to_pack, __LINE__, __FILE__);
                                }
                            }
                        }
                    } else {
                        element_index += effective_data_length;
                    }
                } else {
                    element_index += effective_data_length;
                }
            }
        }
    }
    
    return total_vertices;
}

// Unpack buffer data back to field line vertices (species-aware)
void UnpackAllFieldLinesVertexData(PIC::Datum::cDatum* S, const std::vector<double>& buffer) {
    int effective_data_length = GetEffectiveDataLength(S);
    if (effective_data_length == 0) {
        return;
    }
    
    int element_index = 0;
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            // Unpack begin vertex data for all species
            PIC::FieldLine::cFieldLineVertex* BeginVertex = Segment->GetBegin();
            if (BeginVertex) {
                double* vertex_data = BeginVertex->GetDatum_ptr(S,BeginVertex->GetCompletedSamplingOffset());
                if (vertex_data) {
                    // Unpack data for all species
                    for (int species = 0; species < PIC::nTotalSpecies; species++) {
                        for (int i = 0; i < S->length; i++) {
                            int data_idx = species * S->length + i;
                            double value_from_reduce = buffer[element_index++];
                            value_from_reduce = SanitizeSampledVertexDatumForMPI(
                                S,value_from_reduce,vertex_data+data_idx,
                                "unpack-after-all-rank-reduction","begin",
                                iFieldLine,species,i,element_index-1,Segment,BeginVertex);
                            vertex_data[data_idx] = value_from_reduce;
                            
                            if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                                validate_numeric(value_from_reduce, __LINE__, __FILE__);
                            }
                        }
                    }
                } else {
                    // Skip if no data pointer
                    element_index += effective_data_length;
                }
            } else {
                element_index += effective_data_length;
            }
            
            // If this is the last segment, also unpack end vertex
            if (Segment->GetNext() == NULL) {
                PIC::FieldLine::cFieldLineVertex* EndVertex = Segment->GetEnd();
                if (EndVertex) {
                    double* vertex_data = EndVertex->GetDatum_ptr(S,EndVertex->GetCompletedSamplingOffset());
                    if (vertex_data) {
                        // Unpack data for all species
                        for (int species = 0; species < PIC::nTotalSpecies; species++) {
                            for (int i = 0; i < S->length; i++) {
                                int data_idx = species * S->length + i;
                                double value_from_reduce = buffer[element_index++];
                                value_from_reduce = SanitizeSampledVertexDatumForMPI(
                                    S,value_from_reduce,vertex_data+data_idx,
                                    "unpack-after-all-rank-reduction","end",
                                    iFieldLine,species,i,element_index-1,Segment,EndVertex);
                                vertex_data[data_idx] = value_from_reduce;
                                
                                if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                                    validate_numeric(value_from_reduce, __LINE__, __FILE__);
                                }
                            }
                        }
                    } else {
                        element_index += effective_data_length;
                    }
                } else {
                    element_index += effective_data_length;
                }
            }
        }
    }
}

// Pack data for specific field line vertices (species-aware)
int PackFieldLineVertexData(int field_line_idx, PIC::Datum::cDatum* S, std::vector<double>& buffer) {
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        buffer.clear();
        return 0;
    }
    
    int vertices = CountFieldLineVertices(field_line_idx);
    int effective_data_length = GetEffectiveDataLength(S);
    int total_elements = vertices * effective_data_length;
    
    buffer.resize(total_elements, 0.0);
    
    if (vertices == 0 || effective_data_length == 0) {
        return 0;
    }
    
    // Pack data for this field line
    int element_index = 0;
    
    for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
         Segment != NULL; 
         Segment = Segment->GetNext()) {
        
        // Pack begin vertex data for all species
        PIC::FieldLine::cFieldLineVertex* BeginVertex = Segment->GetBegin();
        if (BeginVertex) {
            double* vertex_data = BeginVertex->GetDatum_ptr(S);
            if (vertex_data) {
                // Pack data for all species
                for (int species = 0; species < PIC::nTotalSpecies; species++) {
                    for (int i = 0; i < S->length; i++) {
                        int data_idx = species * S->length + i;
                        double value_to_pack = SanitizeSampledVertexDatumForMPI(
                            S,vertex_data[data_idx],vertex_data+data_idx,
                            "pack-field-line-before-reduction","begin",
                            field_line_idx,species,i,element_index,Segment,BeginVertex);
                        buffer[element_index++] = value_to_pack;
                    }
                }
            } else {
                element_index += effective_data_length;  // Leave as zeros
            }
        } else {
            element_index += effective_data_length;
        }
        
        // If this is the last segment, also pack end vertex
        if (Segment->GetNext() == NULL) {
            PIC::FieldLine::cFieldLineVertex* EndVertex = Segment->GetEnd();
            if (EndVertex) {
                double* vertex_data = EndVertex->GetDatum_ptr(S);
                if (vertex_data) {
                    // Pack data for all species
                    for (int species = 0; species < PIC::nTotalSpecies; species++) {
                        for (int i = 0; i < S->length; i++) {
                            int data_idx = species * S->length + i;
                            double value_to_pack = SanitizeSampledVertexDatumForMPI(
                                S,vertex_data[data_idx],vertex_data+data_idx,
                                "pack-field-line-before-reduction","end",
                                field_line_idx,species,i,element_index,Segment,EndVertex);
                            buffer[element_index++] = value_to_pack;
                        }
                    }
                } else {
                    element_index += effective_data_length;
                }
            } else {
                element_index += effective_data_length;
            }
        }
    }
    
    return vertices;
}

// Unpack data for specific field line vertices (species-aware)
void UnpackFieldLineVertexData(int field_line_idx, PIC::Datum::cDatum* S, const std::vector<double>& buffer) {
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        return;
    }
    
    int effective_data_length = GetEffectiveDataLength(S);
    if (effective_data_length == 0) {
        return;
    }
    
    int element_index = 0;
    
    for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[field_line_idx].GetFirstSegment();
         Segment != NULL; 
         Segment = Segment->GetNext()) {
        
        // Unpack begin vertex data for all species
        PIC::FieldLine::cFieldLineVertex* BeginVertex = Segment->GetBegin();
        if (BeginVertex) {
            double* vertex_data = BeginVertex->GetDatum_ptr(S);
            if (vertex_data) {
                // Unpack data for all species
                for (int species = 0; species < PIC::nTotalSpecies; species++) {
                    for (int i = 0; i < S->length; i++) {
                        int data_idx = species * S->length + i;
                        double value_from_reduce = buffer[element_index++];
                        value_from_reduce = SanitizeSampledVertexDatumForMPI(
                            S,value_from_reduce,vertex_data+data_idx,
                            "unpack-field-line-after-reduction","begin",
                            field_line_idx,species,i,element_index-1,Segment,BeginVertex);
                        vertex_data[data_idx] = value_from_reduce;
                    }
                }
            } else {
                element_index += effective_data_length;  // Skip
            }
        } else {
            element_index += effective_data_length;
        }
        
        // If this is the last segment, also unpack end vertex
        if (Segment->GetNext() == NULL) {
            PIC::FieldLine::cFieldLineVertex* EndVertex = Segment->GetEnd();
            if (EndVertex) {
                double* vertex_data = EndVertex->GetDatum_ptr(S);
                if (vertex_data) {
                    // Unpack data for all species
                    for (int species = 0; species < PIC::nTotalSpecies; species++) {
                        for (int i = 0; i < S->length; i++) {
                            int data_idx = species * S->length + i;
                            double value_from_reduce = buffer[element_index++];
                            value_from_reduce = SanitizeSampledVertexDatumForMPI(
                                S,value_from_reduce,vertex_data+data_idx,
                                "unpack-field-line-after-reduction","end",
                                field_line_idx,species,i,element_index-1,Segment,EndVertex);
                            vertex_data[data_idx] = value_from_reduce;
                        }
                    }
                } else {
                    element_index += effective_data_length;
                }
            } else {
                element_index += effective_data_length;
            }
        }
    }
}

// ============================================================================
// MPI Operations for Vertex Data - Species-aware implementation
// ============================================================================

void MPIAllReduceDatumStoredAtVertex(PIC::Datum::cDatum* S) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // Pack all vertex data into buffer
    std::vector<double> buffer;
    int total_vertices = PackAllFieldLinesVertexData(S, buffer);
    
    if (total_vertices == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No vertices found for all-reduce operation" << std::endl;
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
    
    // Unpack results back to vertices
    UnpackAllFieldLinesVertexData(S, buffer);
    
    if (rank == 0) {
        std::cout << "Completed MPI all-reduce for " << total_vertices 
                  << " vertices (" << buffer.size() << " elements) across " 
                  << PIC::nTotalSpecies << " species" << std::endl;
    }
}

void MPIReduceDatumStoredAtVertex(PIC::Datum::cDatum* S, int root_rank) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (root_rank < 0 || root_rank >= size) {
        std::cerr << "Error: Invalid root_rank " << root_rank << std::endl;
        return;
    }

    // Pack send data
    std::vector<double> send_buffer;
    int total_vertices = PackAllFieldLinesVertexData(S, send_buffer);
    
    if (total_vertices == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No vertices found for reduce operation" << std::endl;
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
        UnpackAllFieldLinesVertexData(S, recv_buffer);
        std::cout << "Completed MPI reduce for " << total_vertices 
                  << " vertices (" << recv_buffer.size() << " elements) across " 
                  << PIC::nTotalSpecies << " species to root rank " << root_rank << std::endl;
    }
}

void MPIBcastDatumStoredAtVertex(PIC::Datum::cDatum* S, int root_rank) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (root_rank < 0 || root_rank >= size) {
        std::cerr << "Error: Invalid root_rank " << root_rank << std::endl;
        return;
    }

    // Pack data into buffer
    std::vector<double> buffer;
    int total_vertices = PackAllFieldLinesVertexData(S, buffer);
    
    if (total_vertices == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No vertices found for broadcast operation" << std::endl;
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
    
    // Unpack data back to vertices
    UnpackAllFieldLinesVertexData(S, buffer);
    
    if (rank == root_rank) {
        std::cout << "Completed MPI broadcast for " << total_vertices 
                  << " vertices (" << buffer.size() << " elements) across " 
                  << PIC::nTotalSpecies << " species from root rank " << root_rank << std::endl;
    }
}

void MPIAllReduceDatumStoredAtVertexFieldLine(int field_line_idx, PIC::Datum::cDatum* S) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (field_line_idx < 0 || field_line_idx >= nFieldLine) {
        std::cerr << "Error: Invalid field_line_idx " << field_line_idx << std::endl;
        return;
    }

    // Pack data for this field line's vertices
    std::vector<double> buffer;
    int vertex_count = PackFieldLineVertexData(field_line_idx, S, buffer);
    
    if (vertex_count == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No vertices found for field line " << field_line_idx << std::endl;
        }
        return;
    }
    
    // All-reduce for this field line's vertices
    int result = MPI_Allreduce(MPI_IN_PLACE, buffer.data(), buffer.size(), 
                               MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Allreduce failed for field line " << field_line_idx 
                  << " with code " << result << std::endl;
        return;
    }
    
    // Unpack results
    UnpackFieldLineVertexData(field_line_idx, S, buffer);
    
    if (rank == 0) {
        std::cout << "Completed MPI all-reduce for field line " << field_line_idx 
                  << " (" << vertex_count << " vertices, " << buffer.size() 
                  << " elements) across " << PIC::nTotalSpecies << " species" << std::endl;
    }
}

void MPIReduceDatumStoredAtVertexFieldLine(int field_line_idx, PIC::Datum::cDatum* S, int root_rank) {
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

    // Pack send data for this field line's vertices
    std::vector<double> send_buffer;
    int vertex_count = PackFieldLineVertexData(field_line_idx, S, send_buffer);
    
    if (vertex_count == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No vertices found for field line " << field_line_idx << std::endl;
        }
        return;
    }
    
    // Prepare receive buffer (only on root)
    std::vector<double> recv_buffer;
    if (rank == root_rank) {
        recv_buffer.resize(send_buffer.size(), 0.0);
    }
    
    // MPI_Reduce operation for this field line's vertices
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
        UnpackFieldLineVertexData(field_line_idx, S, recv_buffer);
        std::cout << "Completed MPI reduce for field line " << field_line_idx 
                  << " (" << vertex_count << " vertices, " << recv_buffer.size() 
                  << " elements) across " << PIC::nTotalSpecies << " species to root rank " 
                  << root_rank << std::endl;
    }
}

void MPIBcastDatumStoredAtVertexFieldLine(int field_line_idx, PIC::Datum::cDatum* S, int root_rank) {
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

    // Pack data for this field line's vertices
    std::vector<double> buffer;
    int vertex_count = PackFieldLineVertexData(field_line_idx, S, buffer);
    
    if (vertex_count == 0) {
        if (rank == 0) {
            std::cerr << "Warning: No vertices found for field line " << field_line_idx << std::endl;
        }
        return;
    }
    
    // Broadcast buffer for this field line's vertices
    int result = MPI_Bcast(buffer.data(), buffer.size(), MPI_DOUBLE, 
                          root_rank, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        std::cerr << "Error: MPI_Bcast failed for field line " << field_line_idx 
                  << " with code " << result << std::endl;
        return;
    }
    
    // Unpack data back to this field line's vertices
    UnpackFieldLineVertexData(field_line_idx, S, buffer);
    
    if (rank == root_rank) {
        std::cout << "Completed MPI broadcast for field line " << field_line_idx 
                  << " (" << vertex_count << " vertices, " << buffer.size() 
                  << " elements) across " << PIC::nTotalSpecies << " species from root rank " 
                  << root_rank << std::endl;
    }
}

// ============================================================================
// Utility functions for setting vertex data across all field lines (species-aware)
// ============================================================================

void SetDatumStoredAtVertex(double val, PIC::Datum::cDatum* datum) {
    // Set a single value to all vertices across all field lines for all species
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            // Set begin vertex data for all species
            PIC::FieldLine::cFieldLineVertex* BeginVertex = Segment->GetBegin();
            if (BeginVertex) {
                double* vertex_data = BeginVertex->GetDatum_ptr(datum);
                if (vertex_data) {
                    for (int species = 0; species < PIC::nTotalSpecies; species++) {
                        for (int i = 0; i < datum->length; i++) {
                            int data_idx = species * datum->length + i;
                            vertex_data[data_idx] = val;
                            
                            if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                                validate_numeric(vertex_data[data_idx], __LINE__, __FILE__);
                            }
                        }
                    }
                }
            }
            
            // If this is the last segment, also set end vertex
            if (Segment->GetNext() == NULL) {
                PIC::FieldLine::cFieldLineVertex* EndVertex = Segment->GetEnd();
                if (EndVertex) {
                    double* vertex_data = EndVertex->GetDatum_ptr(datum);
                    if (vertex_data) {
                        for (int species = 0; species < PIC::nTotalSpecies; species++) {
                            for (int i = 0; i < datum->length; i++) {
                                int data_idx = species * datum->length + i;
                                vertex_data[data_idx] = val;
                                
                                if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                                    validate_numeric(vertex_data[data_idx], __LINE__, __FILE__);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void SetDatumStoredAtVertex(double* val, PIC::Datum::cDatum* datum) {
    // Set an array of values to all vertices across all field lines for all species
    if (val == nullptr) {
        std::cerr << "Error: SetDatumStoredAtVertex received null pointer for val array" << std::endl;
        return;
    }
    
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        for (PIC::FieldLine::cFieldLineSegment* Segment = FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL; 
             Segment = Segment->GetNext()) {
            
            // Set begin vertex data for all species
            PIC::FieldLine::cFieldLineVertex* BeginVertex = Segment->GetBegin();
            if (BeginVertex) {
                double* vertex_data = BeginVertex->GetDatum_ptr(datum);
                if (vertex_data) {
                    for (int species = 0; species < PIC::nTotalSpecies; species++) {
                        for (int i = 0; i < datum->length; i++) {
                            int data_idx = species * datum->length + i;
                            // Input array should contain species*length + component
                            vertex_data[data_idx] = val[data_idx];
                            
                            if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                                validate_numeric(vertex_data[data_idx], __LINE__, __FILE__);
                            }
                        }
                    }
                }
            }
            
            // If this is the last segment, also set end vertex
            if (Segment->GetNext() == NULL) {
                PIC::FieldLine::cFieldLineVertex* EndVertex = Segment->GetEnd();
                if (EndVertex) {
                    double* vertex_data = EndVertex->GetDatum_ptr(datum);
                    if (vertex_data) {
                        for (int species = 0; species < PIC::nTotalSpecies; species++) {
                            for (int i = 0; i < datum->length; i++) {
                                int data_idx = species * datum->length + i;
                                vertex_data[data_idx] = val[data_idx];
                                
                                if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                                    validate_numeric(vertex_data[data_idx], __LINE__, __FILE__);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace Parallel
} // namespace FieldLine
} // namespace PIC
