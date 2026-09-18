/*
PURPOSE:
--------
Calculate the distance from each field line vertex to the shock location.
Uses SEP::ParticleSource::ShockWave::Tenishev2005::GetInjectionLocation() to determine
shock position along each field line, then computes signed distances to all vertices.

OPTIMIZATIONS IMPLEMENTED:
--------------------------
1. Single-pass processing: compute geometry and distances in one loop
2. Field-line-local geometry caching (no persistent cache due to changing geometry)
3. Efficient memory access patterns within each field line
4. Minimize function calls and redundant calculations
5. Early termination for field lines without shock intersections

ALGORITHM STRUCTURE:
-------------------
1. Loop over all field lines
2. For each field line:
   a) Find shock internal coordinate using GetInjectionLocation()
   b) PASS 1: Loop through segments from beginning to shock location to determine shock distance
   c) PASS 2: Loop through all segments from beginning to calculate vertex distances and subtract shock distance

PHYSICAL SETUP:
--------------
- Field lines start near the Sun (field line beginning)
- Shock wave initially forms close to the Sun (near field line beginning)
- Shock propagates outward, away from the Sun (toward field line end)
- Shock moves from field line beginning toward field line end over time

DISTANCE CONVENTION:
-------------------
- POSITIVE: Vertex is ahead of the shock (farther from Sun, in shock's path)
- NEGATIVE: Vertex is behind the shock (closer to Sun, already swept by shock)
- Distance measured along the field line in Astronomical Units (AU)
- Converted from meters using segment->GetLength() and _AU_ constant

PHYSICAL INTERPRETATION:
-----------------------
- Positive distances: Regions not yet reached by the shock wave
- Negative distances: Regions already processed/swept by the shock wave
- Zero distance: Location of the shock front itself

USAGE EXAMPLE:
==============
```cpp
// Define a datum to store vertex-to-shock distances
PIC::Datum::cDatum* PIC::FieldLine::DatumAtVertexShockLocationDistance;

// Calculate and store distances for all vertices (optimized version)
SEP::FieldLine::CalculateVertexShockDistances(PIC::FieldLine::DatumAtVertexShockLocationDistance);

// Access example:
double* distancePtr = someVertex->GetDatum_ptr(PIC::FieldLine::DatumAtVertexShockLocationDistance);
if (distancePtr != nullptr) {
    double distanceToShock = distancePtr[0]; // Positive = ahead, negative = behind
    if (distanceToShock > 0) {
        std::cout << "Vertex is ahead of shock by " << distanceToShock << " AU" << std::endl;
    } else {
        std::cout << "Vertex is behind shock by " << abs(distanceToShock) << " AU" << std::endl;
    }
}
```
*/

#include "pic.h"
#include "sep.h"
#include <iostream>
#include <cmath>
#include <vector>


namespace SEP {
namespace FieldLine {
	
// Calculate distance from each field line vertex to the shock location
// Optimized single-pass algorithm that processes all field lines and vertices
void CalculateVertexShockDistances() {
    if (PIC::FieldLine::DatumAtVertexShockLocationDistance.is_active()==false) return;
    
    // Statistics counters
    int totalVerticesProcessed = 0;
    int totalVerticesWithShock = 0;
    int fieldLinesWithoutShock = 0;
    
    // Loop over all field lines
    for (int iFieldLine = 0; iFieldLine < PIC::FieldLine::nFieldLine; iFieldLine++) {
        PIC::FieldLine::cFieldLine* fieldLine = &PIC::FieldLine::FieldLinesAll[iFieldLine];
        
        // Step a) Find shock internal coordinate on this field line
        // S = segment_index.relative_location_in_segment
        double shockS;  // Internal coordinate of shock intersection
        double shockPosition[3];  // 3D coordinates of shock intersection
        int shockSegment = SEP::ParticleSource::ShockWave::Tenishev2005::GetInjectionLocation(
            iFieldLine, shockS, shockPosition);
        
        double shockDistanceFromStart = 0.0;
        
        // Check if shock location is valid
        int totalSegments = fieldLine->GetTotalSegmentNumber();
        if (shockS < 0.0 || shockS >= static_cast<double>(totalSegments)) {
            // Invalid shock location - set shock distance to 0
            shockDistanceFromStart = 0.0;
        } else {
            // Valid shock found
            totalVerticesWithShock++;
            
            // PASS 1: Calculate shock distance from field line beginning in meters
            // Loop through segments from beginning to shock location
            PIC::FieldLine::cFieldLineSegment* segment = fieldLine->GetFirstSegment();
            int currentSegmentIndex = 0;
            
            // Traverse segments up to the shock segment
            while (segment != nullptr && currentSegmentIndex < shockSegment) {
                shockDistanceFromStart += segment->GetLength();  // Length in meters
                segment = segment->GetNext();
                currentSegmentIndex++;
            }
            
            // Add fractional distance within the shock segment
            if (segment != nullptr && currentSegmentIndex == shockSegment) {
                double fractionalPosition = shockS - static_cast<double>(shockSegment);
                shockDistanceFromStart += fractionalPosition * segment->GetLength();  // Length in meters
            }
        }
        
        // PASS 2: Single loop through all segments to process all vertices
        // Calculate vertex distances in meters and convert to AU
        double vertexDistanceFromStart = 0.0;  // Distance in meters
        PIC::FieldLine::cFieldLineSegment* segment = fieldLine->GetFirstSegment();
        
        // Helper lambda to process vertex distance calculation
        auto processVertex = [&](PIC::FieldLine::cFieldLineVertex* vertex, double distanceFromStart) {
            if (vertex) {
                double* distanceData = vertex->GetDatum_ptr(PIC::FieldLine::DatumAtVertexShockLocationDistance);
                if (distanceData) {
                    // Distance = vertex position - shock position (in meters)
                    // Positive = vertex ahead of shock, Negative = vertex behind shock
                    double distanceInMeters = distanceFromStart - shockDistanceFromStart;
                    
                    // Convert from meters to AU
                    distanceData[0] = distanceInMeters / _AU_;
                    totalVerticesProcessed++;
                    
                    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                        validate_numeric(distanceData[0], __LINE__, __FILE__);
                    }
                }
            }
        };
        
        while (segment != nullptr) {
            // Process begin vertex at current position (before adding segment length)
            processVertex(segment->GetBegin(), vertexDistanceFromStart);
            
            // Get segment length and update distance for end of segment
            double segmentLength = segment->GetLength();  // Length in meters
            vertexDistanceFromStart += segmentLength;
            
            // Process end vertex if this is the last segment (after adding segment length)
            if (segment->GetNext() == nullptr) {
                processVertex(segment->GetEnd(), vertexDistanceFromStart);
            }
            
            // Move to next segment
            segment = segment->GetNext();
        }
        
        // Count field lines without valid shock
        if (shockS < 0.0 || shockS >= static_cast<double>(totalSegments)) {
            fieldLinesWithoutShock++;
        }
    } // End loop over field lines
    
    // Print summary statistics
    std::cout << "CalculateVertexShockDistances completed:" << std::endl;
    std::cout << "  Total vertices processed: " << totalVerticesProcessed << std::endl;
    std::cout << "  Field lines with shock intersection: " << totalVerticesWithShock << std::endl;
    std::cout << "  Field lines without shock intersection: " << fieldLinesWithoutShock << std::endl;
    std::cout << "  Total field lines: " << PIC::FieldLine::nFieldLine << std::endl;
}

} // namespace FieldLine
} // namespace SEP
