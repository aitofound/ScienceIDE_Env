// This program implements Weighted Particle Merging and Splitting algorithms tailored for particles characterized by
// one spatial coordinate (s) and two velocity coordinates (vParallel and vNormal).
// The algorithms conserve momentum, report energy discrepancies,
// and prioritize merging/splitting based on particle weights and bin population.

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <array>
#include <algorithm>
#include <iomanip>

#include "pic.h"


// Structure to hold particle data for binning
struct cParticleData {
    double s;          // Spatial coordinate
    double vParallel;  // Parallel velocity component
    double vNormal;    // Normal velocity component
    double w;          // Weight (mass)
    long int id;       // Particle identifier
};

// Function to compute a unique key for a bin based on its spatial and velocity indices
long long PIC::FieldLine::cFieldLineSegment::computeBinKey(int is, int ivParallel, int ivNormal, 
                        int numBinsSpatial, int numBinsVParallel, int numBinsVNormal) {
    // Using prime number multipliers to generate a unique key
    const long long prime1 = 73856093;
    const long long prime2 = 19349663;
    const long long prime3 = 83492791;

    return static_cast<long long>(is) * prime1 ^ 
           static_cast<long long>(ivParallel) * prime2 ^ 
           static_cast<long long>(ivNormal) * prime3;
}

/// \brief Performs Weighted Particle Merging on a linked list of particles.
///
/// This function groups particles into spatial and velocity bins and merges them within each combined bin
/// based on their weights. The merging process prioritizes combining particles with lower weights first, 
/// two at a time, until the total number of particles is within the specified range [nParticleRangeMin, nParticleRangeMax].
/// Momentum is conserved exactly, and energy discrepancies are reported.
///
/// \param head The identifier of the first particle in the linked list.
/// \param numBinsSpatial The number of spatial bins along the 's' axis.
/// \param numBinsVParallel The number of velocity bins along the 'vParallel' axis.
/// \param numBinsVNormal The number of velocity bins along the 'vNormal' axis.
/// \param sRange The range of spatial coordinate 's' (typically [0,1)).
/// \param vParallelRange Reference to a variable that will store the dynamically calculated maximum absolute vParallel.
/// \param vNormalRange Reference to a variable that will store the dynamically calculated maximum absolute vNormal.
/// \param nParticleRangeMin The minimum desired number of particles after merging.
/// \param nParticleRangeMax The maximum desired number of particles after merging.
/// \param mergeThreshold Minimum number of particles in a bin to perform merging (default is 2).
void PIC::FieldLine::cFieldLineSegment::WeightedParticleMerging(int spec,long int& head, int numBinsSpatial, int numBinsVParallel, int numBinsVNormal,
                             double sRange, double &vParallelRange, double &vNormalRange,
                             int nParticleRangeMin, int nParticleRangeMax,
                             int mergeThreshold) {
    namespace PB = PIC::ParticleBuffer;

    // Initial bin sizes (will be updated after dynamic V_MAX calculation)
    double binSizeSpatial = sRange / numBinsSpatial;
    double binSizeVParallel = 0.0;
    double binSizeVNormal = 0.0;

    // First pass: Count particles and determine velocity ranges
    long int p_temp = head;
    int totalParticles = 0;
    double dynamicVParallelMax = 0.0;
    double dynamicVNormalMax = 0.0;

    while (p_temp != -1) {
        if (PB::GetI(p_temp) == spec) {
            double vParallel = PB::GetVParallel(p_temp);
            double vNormal = PB::GetVNormal(p_temp);

            dynamicVParallelMax = std::max(dynamicVParallelMax, std::abs(vParallel));
            dynamicVNormalMax = std::max(dynamicVNormalMax, std::abs(vNormal));
            totalParticles++;
        }
        p_temp = PIC::ParticleBuffer::GetNext(p_temp);
    }

    // Check if merging needed
    if (totalParticles < nParticleRangeMax) {
        return;
    }

    // Update velocity ranges and calculate bin sizes
    vParallelRange = dynamicVParallelMax;
    vNormalRange = dynamicVNormalMax;
    binSizeVParallel = (2.0 * vParallelRange) / numBinsVParallel;
    binSizeVNormal = (2.0 * vNormalRange) / numBinsVNormal;

    // Calculate target number of particles
    int targetParticles = static_cast<int>(0.5 * (nParticleRangeMin + nParticleRangeMax));
    int particlesToRemove = totalParticles - targetParticles;

    if (particlesToRemove <= 0) return;

    // Map to store particles in each bin
    std::unordered_map<long long, std::vector<cParticleData>> bins;

    // Second pass: Bin particles
    p_temp = head;
    while (p_temp != -1) {
        if (PB::GetI(p_temp) != spec) {
            p_temp = PIC::ParticleBuffer::GetNext(p_temp);
            continue;
        }

        PIC::ParticleBuffer::byte *p_data_temp = PB::GetParticleDataPointer(p_temp);
        double s = PB::GetFieldLineCoord(p_data_temp);
        double vParallel = PB::GetVParallel(p_data_temp);
        double vNormal = PB::GetVNormal(p_data_temp);
        double w = PB::GetIndividualStatWeightCorrection(p_data_temp); 

        // Calculate bin indices
        int is = std::min(std::max(static_cast<int>(std::floor(s / binSizeSpatial)), 0),
                         numBinsSpatial - 1);
        int ivParallel = std::min(std::max(static_cast<int>(std::floor((vParallel + vParallelRange) /
                                 binSizeVParallel)), 0), numBinsVParallel - 1);
        int ivNormal = std::min(std::max(static_cast<int>(std::floor((vNormal + vNormalRange) /
                               binSizeVNormal)), 0), numBinsVNormal - 1);

        long long binKey = computeBinKey(is, ivParallel, ivNormal, numBinsSpatial,
                                       numBinsVParallel, numBinsVNormal);

        cParticleData pdata{s, vParallel, vNormal, w, p_temp};
        bins[binKey].push_back(pdata);

        p_temp = PIC::ParticleBuffer::GetNext(p_temp);
    }

    // Calculate average particles per bin for populated bins
    int populatedBins = 0;
    for (const auto& bin : bins) {
        if (!bin.second.empty()) {
            populatedBins++;
        }
    }

    double avgParticlesPerBin = static_cast<double>(targetParticles) / populatedBins;

    // Track energy conservation
    double totalEnergyBefore = 0.0;
    double totalEnergyAfter = 0.0;

    // Process each bin
    for (auto& bin : bins) {
        if (particlesToRemove <= 0) break;

        auto& particles = bin.second;
        if (particles.size() <= 1) continue;

        // Sort particles in this bin by weight (ascending)
        std::sort(particles.begin(), particles.end(),
                 [](const cParticleData& a, const cParticleData& b) {
                     return a.w < b.w;
                 });

        // Calculate how many particles to remove from this bin
        int binExcess = static_cast<int>(particles.size() - avgParticlesPerBin);
        int binParticlesToRemove = std::min(binExcess, particlesToRemove);

        if (binParticlesToRemove <= 0) continue;

        // Merge particles pairwise, starting with smallest weights
        for (size_t i = 0; i < particles.size() - 1 && binParticlesToRemove > 0; i += 2) {
            cParticleData& p1 = particles[i];
            cParticleData& p2 = particles[i + 1];

            // Calculate merged properties
            double mergedWeight = p1.w + p2.w;
            double totalMomentumParallel = (p1.w * p1.vParallel) + (p2.w * p2.vParallel);
            double totalMomentumNormal = (p1.w * p1.vNormal) + (p2.w * p2.vNormal);
            double mergedSWeighted = (p1.s * p1.w) + (p2.s * p2.w);

            double energyBefore = 0.5 * p1.w * (p1.vParallel * p1.vParallel + p1.vNormal * p1.vNormal) +
                                 0.5 * p2.w * (p2.vParallel * p2.vParallel + p2.vNormal * p2.vNormal);

            // Calculate merged velocities (conserve momentum)
            double mergedVParallel = totalMomentumParallel / mergedWeight;
            double mergedVNormal = totalMomentumNormal / mergedWeight;
            double mergedS = mergedSWeighted / mergedWeight;

	    if ((isfinite(mergedVParallel)==false)||(isfinite(mergedVNormal)==false)||(isfinite(mergedS)==false)) exit(__LINE__,__FILE__,"Error: found nan"); 

            // Update first particle with merged properties
            PIC::ParticleBuffer::byte *ptr_data = PB::GetParticleDataPointer(p1.id);
            PB::SetVParallel(mergedVParallel, ptr_data);
            PB::SetVNormal(mergedVNormal, ptr_data);
            PB::SetFieldLineCoord(mergedS, ptr_data);
            PB::SetIndividualStatWeightCorrection(mergedWeight, ptr_data);

            // Remove second particle
            PB::DeleteParticle(p2.id, head);

            double energyAfter = 0.5 * mergedWeight * (mergedVParallel * mergedVParallel +
                                                      mergedVNormal * mergedVNormal);

            totalEnergyBefore += energyBefore;
            totalEnergyAfter += energyAfter;

            binParticlesToRemove--;
            particlesToRemove--;
            totalParticles--;
        }
    }

    // Report results
    /*
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Merging completed. Final particle count: " << totalParticles << "\n";
    std::cout << "Target was: " << targetParticles << "\n";
    std::cout << "Total Kinetic Energy Before: " << totalEnergyBefore << " J\n";
    std::cout << "Total Kinetic Energy After:  " << totalEnergyAfter << " J\n";
    std::cout << "Energy Discrepancy: " <<
                 2 * fabs(totalEnergyAfter - totalEnergyBefore)/(totalEnergyAfter + totalEnergyBefore) <<
                 " J\n";
		 */
}


/// \brief Performs Weighted Particle Splitting on a linked list of particles.
///
/// This function groups particles into spatial and velocity bins and splits them within each combined bin
/// based on their weights. The splitting process prioritizes splitting particles with larger weights first 
/// in the most populated bins, until the total number of particles is within the specified range [nParticleRangeMin, nParticleRangeMax].
/// Momentum is conserved exactly, and energy discrepancies are reported.
///
/// \param head The identifier of the first particle in the linked list.
/// \param numBinsSpatial The number of spatial bins along the 's' axis.
/// \param numBinsVParallel The number of velocity bins along the 'vParallel' axis.
/// \param numBinsVNormal The number of velocity bins along the 'vNormal' axis.
/// \param sRange The range of spatial coordinate 's' (typically [0,1)).
/// \param vParallelRange Reference to a variable that will store the dynamically calculated maximum absolute vParallel.
/// \param vNormalRange Reference to a variable that will store the dynamically calculated maximum absolute vNormal.
/// \param nParticleRangeMin The minimum desired number of particles after splitting.
/// \param nParticleRangeMax The maximum desired number of particles after splitting.
/// \param splitThreshold Minimum number of particles in a bin to perform splitting (default is 2).
void PIC::FieldLine::cFieldLineSegment::WeightedParticleSplitting(int spec,long int& head, int numBinsSpatial, int numBinsVParallel, int numBinsVNormal,
                               double sRange, double &vParallelRange, double &vNormalRange,
                               int nParticleRangeMin, int nParticleRangeMax,
                               int splitThreshold) {
    namespace PB = PIC::ParticleBuffer;

    // First pass: Count particles and collect particle data
    std::vector<cParticleData> particles;
    long int p_temp = head;
    int totalParticles = 0;

    while (p_temp != -1) {
        if (PB::GetI(p_temp) == spec) {
            PIC::ParticleBuffer::byte *p_data_temp = PB::GetParticleDataPointer(p_temp);

            // Collect particle data
            cParticleData pdata;
            pdata.s = PB::GetFieldLineCoord(p_data_temp);
            pdata.vParallel = PB::GetVParallel(p_data_temp);
            pdata.vNormal = PB::GetVNormal(p_data_temp);
            pdata.w = PB::GetIndividualStatWeightCorrection(p_data_temp);
            pdata.id = p_temp;

	    if (isfinite(pdata.w)==false) exit(__LINE__,__FILE__,"Error: nan is found");
	    if (pdata.w==0.0) exit(__LINE__,__FILE__,"Error: stat weight is zero");
	    if (std::isnormal(pdata.w)==false) exit(__LINE__,__FILE__,"Error: nan is found");

            particles.push_back(pdata);
            totalParticles++;
        }
        p_temp = PIC::ParticleBuffer::GetNext(p_temp);
    }

    // Check if splitting is needed
    if (totalParticles >  nParticleRangeMin) {
        return;
    }

    // Calculate target number of particles
    int targetParticles = static_cast<int>(0.5 * (nParticleRangeMax + nParticleRangeMin));

    //Limit the number of new particle to be created by increased the total number of particle 
    if (targetParticles>1.2*totalParticles) targetParticles=static_cast<int>(1.2*totalParticles); 

    // Sort particles by weight in descending order
    std::sort(particles.begin(), particles.end(),
              [](const cParticleData& a, const cParticleData& b) {
                  return a.w > b.w;
              });

    // Track energy conservation
    double totalEnergyBefore = 0.0;
    double totalEnergyAfter = 0.0;

    // Split particles starting with highest weights until target is reached
    for (auto& p_to_split : particles) {
        if (totalParticles >= targetParticles) break;

        double newWeight = p_to_split.w / 2.0;

        // Calculate energy before splitting
        double energyBefore = 0.5 * p_to_split.w * (p_to_split.vParallel * p_to_split.vParallel +
                                                   p_to_split.vNormal * p_to_split.vNormal);


        if (std::isnormal(newWeight)==false) continue;

        // Update weight of original particle
        PB::SetIndividualStatWeightCorrection(newWeight,p_to_split.id);

        // Create new particle with half the weight
        long int newParticle = PB::GetNewParticle(head);
        PB::CloneParticle(newParticle, p_to_split.id);

        double energyAfter = 0.5 * newWeight * (p_to_split.vParallel * p_to_split.vParallel +
                                              p_to_split.vNormal * p_to_split.vNormal) * 2;

        // Update energy trackers
        totalEnergyBefore += energyBefore;
        totalEnergyAfter += energyAfter;

        totalParticles++;
    }

    /*
    // Report results
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Splitting completed. Final particle count: " << totalParticles << "\n";
    std::cout << "Target was: " << targetParticles << "\n";
    std::cout << "Total Kinetic Energy Before: " << totalEnergyBefore << " J\n";
    std::cout << "Total Kinetic Energy After:  " << totalEnergyAfter << " J\n";
    std::cout << "Energy Discrepancy: " <<
                 2 * fabs(totalEnergyAfter - totalEnergyBefore)/(totalEnergyAfter + totalEnergyBefore) <<
                 " J\n";
		 */
}



    /*EXAMPLE:
/// \brief Example usage of the WeightedParticleMerging and WeightedParticleSplitting functions.
int main() {
    // Define binning parameters
    int numBinsSpatial = 10;       // Number of spatial bins along 's'
    int numBinsVParallel = 10;     // Number of velocity bins along 'vParallel'
    int numBinsVNormal = 10;       // Number of velocity bins along 'vNormal'
    double sRange = 1.0;           // Spatial coordinate range [0,1)

    // Initial velocity ranges (will be updated dynamically)
    double vParallelRange = 100.0; // Initial guess; will be updated based on particle velocities
    double vNormalRange = 100.0;   // Initial guess; will be updated based on particle velocities

    // Define desired particle range
    int nParticleRangeMin = 50;    // Minimum desired number of particles
    int nParticleRangeMax = 100;   // Maximum desired number of particles

    // Identifier of the first particle in the linked list
    long int head = 1; // Starting with particle ID 1

    // Perform Weighted Particle Merging with Prioritized Pairwise Low-Weight Merging
    WeightedParticleMerging(head, numBinsSpatial, numBinsVParallel, numBinsVNormal, 
                            sRange, vParallelRange, vNormalRange, 
                            nParticleRangeMin, nParticleRangeMax, 2); // Merge bins with 2 or more particles

    // Perform Weighted Particle Splitting with Prioritized Pairwise High-Weight Splitting
    WeightedParticleSplitting(head, numBinsSpatial, numBinsVParallel, numBinsVNormal, 
                              sRange, vParallelRange, vNormalRange, 
                              nParticleRangeMin, nParticleRangeMax, 2); // Split bins with 2 or more particles

    return 0;
}
*/ 

//==========================================================================================================
//function to loop through field line segments
void PIC::ParticleSplitting::FledLine::WeightedParticleMerging(int numBinsSpatial, int numBinsVParallel, int numBinsVNormal,
                             int nParticleRangeMin, int nParticleRangeMax,
                             int mergeThreshold) { 
  namespace FL = PIC::FieldLine;
  double sRange=1,vParallelRange,vNormalRange;

  if (FL::FieldLinesAll==NULL) return; 

  //loop through all field lines and segments 
  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    for (auto Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
      if (Segment->FirstParticleIndex!=-1) for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
        Segment->WeightedParticleMerging(spec,Segment->FirstParticleIndex,numBinsSpatial,numBinsVParallel,numBinsVNormal,
                             sRange,vParallelRange,vNormalRange,
                             nParticleRangeMin,nParticleRangeMax,
                             mergeThreshold);
      }
    }
  }    
}	

void PIC::ParticleSplitting::FledLine::WeightedParticleSplitting(int numBinsSpatial, int numBinsVParallel, int numBinsVNormal,
                               int nParticleRangeMin, int nParticleRangeMax,
                               int splitThreshold) { 
  namespace FL = PIC::FieldLine;
  double sRange=1,vParallelRange,vNormalRange;

  if (FL::FieldLinesAll==NULL) return;

  //loop through all field lines and segments
  for (int iFieldLine=0;iFieldLine<FL::nFieldLine;iFieldLine++) {
    for (auto Segment=FL::FieldLinesAll[iFieldLine].GetFirstSegment();Segment!=NULL;Segment=Segment->GetNext()) {
      if (Segment->FirstParticleIndex!=-1) for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
        Segment->WeightedParticleSplitting(spec,Segment->FirstParticleIndex,numBinsSpatial,numBinsVParallel,numBinsVNormal,
                               sRange,vParallelRange,vNormalRange,nParticleRangeMin,nParticleRangeMax,splitThreshold); 
      } 
    }
  }
}



