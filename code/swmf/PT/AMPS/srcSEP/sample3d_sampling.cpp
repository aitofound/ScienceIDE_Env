/* 
 * sample3d_sampling.cpp
 * 
 * Implementation of particle sampling functions for the SEP simulation framework.
 * 
 * This module provides 3D particle sampling capabilities for the SEP simulation framework.
 * It allows for sampling particle populations in different energy channels as a
 * function of time and location, as well as pitch angle distributions for different
 * locations as a function of time.
 * 
 * FEATURES:
 * ---------
 * - Sample particle energy distributions at specific 3D locations
 * - Sample pitch angle distributions at specific 3D locations
 * - Track time evolution of particle populations
 * - Calculate particle fluxes and return fluxes (particles moving toward the Sun)
 * 
 * USAGE:
 * ------
 * 1. Initialization:
 *    To use the module, first include the header file and initialize:
 *    
 *    #include "sample3d.h"
 *    SEP::Sampling::Sample3D::Init();
 * 
 * 2. Configuring Energy Channels:
 *    Set the energy range and bins:
 *    
 *    // Set energy range from 0.01 MeV to 300 MeV with 50 bins
 *    SEP::Sampling::Sample3D::SetEnergyRange(0.01 * MeV2J, 300.0 * MeV2J, 50);
 * 
 * 3. Configuring Pitch Angle Bins:
 *    Set the number of pitch angle bins:
 *    
 *    // Set 20 pitch angle bins
 *    SEP::Sampling::Sample3D::SetPitchAngleBins(20);
 * 
 * 4. Setting Output Frequency:
 *    Configure how often data is written to output files:
 *    
 *    // Output data every 50 iterations
 *    SEP::Sampling::Sample3D::SetOutputIterations(50);
 * 
 * 5. Adding Sampling Points:
 *    Add specific 3D locations where you want to sample particles:
 *    
 *    // Create a sampling point at (x, y, z) with radius r and label "label"
 *    SEP::Sampling::Sample3D::SamplingPoint point(x, y, z, r, "label");
 *    SEP::Sampling::Sample3D::AddSamplingPoint(point);
 *    
 *    // Example: sampling point at 1 AU on x-axis with 0.1 AU radius
 *    SEP::Sampling::Sample3D::SamplingPoint earthPoint(1.0 * _AU_, 0.0, 0.0, 0.1 * _AU_, "Earth");
 *    SEP::Sampling::Sample3D::AddSamplingPoint(earthPoint);
 * 
 * OUTPUT FILES:
 * ------------
 * The module creates output files in the following directories:
 * - PT/plots/Sample3D/Population/: Particle population by energy channel
 * - PT/plots/Sample3D/Flux/: Particle flux by energy channel
 * - PT/plots/Sample3D/ReturnFlux/: Return flux (sunward) by energy channel
 * - PT/plots/Sample3D/PitchAngle/: Pitch angle distributions
 * 
 * IMPLEMENTATION DETAILS:
 * ----------------------
 * The sampling process is automatically invoked during the simulation via the
 * PIC::IndividualModelSampling::SamplingProcedure system. Data is collected at
 * each time step and periodically written to output files.
 * 
 * The system leverages parallel processing via MPI for efficient sampling in large
 * simulations. Each process collects data from its local domain, and results are
 * reduced across all processes before output.
 */

 #include "sample3d.h"
 #include "pic.h"
 #include <cmath>
 #include <vector>
 
 namespace SEP {
   namespace Sampling {
     namespace Sample3D {
     
       //----------------------------------------------------------------------------
       // Function to sample particles at a specific point
       //----------------------------------------------------------------------------
       void SampleParticlesAtPoint(int pointIndex) {
         namespace PB = PIC::ParticleBuffer;
         
         // Get current time index (newest time point)
         int timeIndex = TimeBuffer.size() - 1;
         if (timeIndex < 0) return;  // No time points yet
         
         // Find the node and cell containing the sampling point
         cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::Mesh::mesh->findTreeNode(SamplingPoints[pointIndex].x);
         if (node == NULL) return;
	 if (node->block == NULL) return;
         
         int i, j, k;
         if (PIC::Mesh::mesh->FindCellIndex(SamplingPoints[pointIndex].x, i, j, k, node) == -1) {
           return;  // Point is not in a valid cell
         }
         
         // Calculate the cell dimensions and bounds
         double dx = (node->xmax[0] - node->xmin[0]) / _BLOCK_CELLS_X_;
         double dy = (node->xmax[1] - node->xmin[1]) / _BLOCK_CELLS_Y_;
         double dz = (node->xmax[2] - node->xmin[2]) / _BLOCK_CELLS_Z_;
         
         double cellMin[3], cellMax[3];
         cellMin[0] = node->xmin[0] + i * dx;
         cellMin[1] = node->xmin[1] + j * dy;
         cellMin[2] = node->xmin[2] + k * dz;
         
         cellMax[0] = cellMin[0] + dx;
         cellMax[1] = cellMin[1] + dy;
         cellMax[2] = cellMin[2] + dz;
         
         // Calculate cell volume
         double volume = dx * dy * dz;
         
         // Get the magnetic field direction at the cell center point
	 double xCenter[3],b[3]={0.0,0.0,0.0};

	 for (int i=0;i<3;i++) xCenter[i]=0.5*(cellMin[i]+cellMax[i]); 
         
	 PIC::CPLR::InitInterpolationStencil(xCenter,node);
	 PIC::CPLR::GetBackgroundMagneticField(b);
	 Vector3D::Normalize(b);

         
         // Get first particle in the cell
         long int ptr = node->block->FirstCellParticleTable[i + _BLOCK_CELLS_X_*(j + _BLOCK_CELLS_Y_*k)];
         
         // Loop through all particles in the cell
         while (ptr != -1) {
             PB::byte* particleData = PB::GetParticleDataPointer(ptr);
             int spec = PB::GetI(particleData);
             
             // Get particle velocity
             double *v,*x;
             v=PB::GetV(particleData);
	     x=PB::GetX(particleData);
             
             // Calculate magnitude of velocity and energy
             double speed = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
             double mass = PIC::MolecularData::GetMass(spec);
             double energy;
             
             // Calculate energy based on relativistic setting
             switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
                 case _PIC_MODE_OFF_:
                     energy = 0.5 * mass * speed * speed;
                     break;
                     
                 case _PIC_MODE_ON_:
                     energy = Relativistic::Speed2E(speed, mass);
                     break;
                     
                 default:
                     energy = Relativistic::Speed2E(speed, mass);
             }
             
             // Determine energy bin
             int energyBin = (int)(log(energy / MinEnergy) / dLogEnergy);
             if (energyBin < 0 || energyBin >= nEnergyBins) {
                 ptr = PB::GetNext(particleData);
                 continue;
             }
             
             // Calculate pitch angle (relative to local magnetic field)
             double mu = Vector3D::DotProduct(v,b)/Vector3D::Length(v);
             if (mu < -1.0) mu = -1.0;
             if (mu > 1.0) mu = 1.0;
             
             // Determine pitch angle bin
             int muBin = (int)((mu + 1.0) / dMu);
             if (muBin < 0) muBin = 0;
             if (muBin >= nPitchAngleBins) muBin = nPitchAngleBins - 1;
             
             // Get particle weight
             double particleWeight = PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
             particleWeight *= PB::GetIndividualStatWeightCorrection(particleData);
             
             // Update population count
             EnergyChannelPopulation[pointIndex](energyBin, timeIndex, 0) += particleWeight / volume;
             
             // Update pitch angle distribution
             PitchAngleDistribution[pointIndex](muBin, energyBin, timeIndex, 0) += particleWeight;
             
             // Update flux
             FluxByEnergy[pointIndex](energyBin, timeIndex, 0) += particleWeight / volume * speed;
             
             // Check if particle moving sunward 
             if (Vector3D::DotProduct(x,v)<0.0) {
                 ReturnFluxByEnergy[pointIndex](energyBin, timeIndex, 0) += particleWeight / volume * speed;
             }
             
             // Move to next particle
             ptr = PB::GetNext(particleData);
         }
       }
       
       //----------------------------------------------------------------------------
       // Main sampling function
       //----------------------------------------------------------------------------
       void Sampling() {
         // Update sampling time
         if (_SIMULATION_TIME_STEP_MODE_ == _SINGLE_GLOBAL_TIME_STEP_) {
             SamplingTime += PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
         } else {
             // For local time step mode, use average node time step
             double nodeTimeStep = 0.0;
             cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];
             int nNodes = 0;
             
             while (node != NULL) {
                 if (node->block != NULL) {
                     nodeTimeStep += node->block->GetLocalTimeStep(0);
                     nNodes++;
                 }
                 node = node->nextNodeThisThread;
             }
             
             if (nNodes > 0) nodeTimeStep /= nNodes;
             SamplingTime += nodeTimeStep;
         }
         
         // Add the current time to the time buffer
         if (TimeBuffer.size() < MaxTimePoints) {
             TimeBuffer.push_back(SamplingTime);
         } else {
             // Shift buffer and add new time
             for (int i = 0; i < MaxTimePoints - 1; i++) {
                 TimeBuffer[i] = TimeBuffer[i + 1];
             }
             TimeBuffer[MaxTimePoints - 1] = SamplingTime;
         }
         
         // Loop through all sampling points
         for (size_t i = 0; i < SamplingPoints.size(); i++) {
             SampleParticlesAtPoint(i);
         }
         
         // Increment counter
         SamplingCounter++;
       }
       
       //----------------------------------------------------------------------------
       // Function to clear all sampling buffers
       //----------------------------------------------------------------------------
       void Clear() {
         // Reset all data structures to zero
         for (size_t i = 0; i < SamplingPoints.size(); i++) {
             EnergyChannelPopulation[i] = 0.0;
             PitchAngleDistribution[i] = 0.0;
             FluxByEnergy[i] = 0.0;
             ReturnFluxByEnergy[i] = 0.0;
         }
         
         // Clear time buffer and reset counters
         TimeBuffer.clear();
         SamplingCounter = 0;
         internalCounter = 0;
       }
       
     } // namespace Sample3D
   } // namespace Sampling
 } // namespace SEP

