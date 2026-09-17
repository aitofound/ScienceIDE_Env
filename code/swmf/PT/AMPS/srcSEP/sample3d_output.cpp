/* 
 * sample3d_output.cpp
 * 
 * Implementation of output functions and manager for 3D particle sampling
 */

 #include "sample3d.h"
 #include "pic.h"
 #include <cmath>
 #include <cstdio>
 
 namespace SEP {
   namespace Sampling {
     namespace Sample3D {
       
       //----------------------------------------------------------------------------
       // Function to output population data
       //----------------------------------------------------------------------------
       void OutputPopulation() {
         // Reduce data across all MPI processes
         for (size_t i = 0; i < SamplingPoints.size(); i++) {
             EnergyChannelPopulation[i].reduce(0, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);
         }
         
         // Output data from main thread
         if (PIC::ThisThread == 0) {
             for (size_t i = 0; i < SamplingPoints.size(); i++) {
                 if (OutputPopulationFiles[i] != NULL) {
                     // Write population data for all time points
                     for (size_t t = 0; t < TimeBuffer.size(); t++) {
                         fprintf(OutputPopulationFiles[i], "%.6e ", TimeBuffer[t]);
                         
                         for (int e = 0; e < nEnergyBins; e++) {
                             fprintf(OutputPopulationFiles[i], "%.6e ", 
                                 EnergyChannelPopulation[i](e, t, 0) / ((SamplingCounter > 0) ? SamplingCounter : 1));
                         }
                         
                         fprintf(OutputPopulationFiles[i], "\n");
                     }
                     
                     fflush(OutputPopulationFiles[i]);
                 }
             }
         }
       }
       
       //----------------------------------------------------------------------------
       // Function to output flux data
       //----------------------------------------------------------------------------
       void OutputFlux() {
         // Reduce data across all MPI processes
         for (size_t i = 0; i < SamplingPoints.size(); i++) {
             FluxByEnergy[i].reduce(0, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);
             ReturnFluxByEnergy[i].reduce(0, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);
         }
         
         // Output data from main thread
         if (PIC::ThisThread == 0) {
             // Output regular flux
             for (size_t i = 0; i < SamplingPoints.size(); i++) {
                 if (OutputFluxFiles[i] != NULL) {
                     // Write flux data for all time points
                     for (size_t t = 0; t < TimeBuffer.size(); t++) {
                         fprintf(OutputFluxFiles[i], "%.6e ", TimeBuffer[t]);
                         
                         for (int e = 0; e < nEnergyBins; e++) {
                             fprintf(OutputFluxFiles[i], "%.6e ", 
                                 FluxByEnergy[i](e, t, 0) / ((SamplingCounter > 0) ? SamplingCounter : 1));
                         }
                         
                         fprintf(OutputFluxFiles[i], "\n");
                     }
                     
                     fflush(OutputFluxFiles[i]);
                 }
             }
             
             // Output return flux
             for (size_t i = 0; i < SamplingPoints.size(); i++) {
                 if (OutputReturnFluxFiles[i] != NULL) {
                     // Write return flux data for all time points
                     for (size_t t = 0; t < TimeBuffer.size(); t++) {
                         fprintf(OutputReturnFluxFiles[i], "%.6e ", TimeBuffer[t]);
                         
                         for (int e = 0; e < nEnergyBins; e++) {
                             fprintf(OutputReturnFluxFiles[i], "%.6e ", 
                                 ReturnFluxByEnergy[i](e, t, 0) / ((SamplingCounter > 0) ? SamplingCounter : 1));
                         }
                         
                         fprintf(OutputReturnFluxFiles[i], "\n");
                     }
                     
                     fflush(OutputReturnFluxFiles[i]);
                 }
             }
         }
       }
       
       //----------------------------------------------------------------------------
       // Function to output pitch angle distribution
       //----------------------------------------------------------------------------
       void OutputPitchAngleDistribution() {
         // Reduce data across all MPI processes
         for (size_t i = 0; i < SamplingPoints.size(); i++) {
             PitchAngleDistribution[i].reduce(0, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);
         }
         
         // Output data from main thread
         if (PIC::ThisThread == 0) {
             for (size_t i = 0; i < SamplingPoints.size(); i++) {
                 if (OutputPitchAngleFiles[i] != NULL) {
                     // Normalize pitch angle distributions for each energy bin
                     for (size_t t = 0; t < TimeBuffer.size(); t++) {
                         for (int e = 0; e < nEnergyBins; e++) {
                             double sum = 0.0;
                             
                             // Calculate sum for normalization
                             for (int m = 0; m < nPitchAngleBins; m++) {
                                 sum += PitchAngleDistribution[i](m, e, t, 0);
                             }
                             
                             sum *= dMu;  // Account for bin width
                             if (sum <= 0.0) sum = 1.0;  // Avoid division by zero
                             
                             // Normalize
                             for (int m = 0; m < nPitchAngleBins; m++) {
                                 PitchAngleDistribution[i](m, e, t, 0) /= sum;
                             }
                         }
                     }
                     
                     // Write pitch angle distribution data
                     for (size_t t = 0; t < TimeBuffer.size(); t++) {
                         for (int m = 0; m < nPitchAngleBins; m++) {
                             double mu = -1.0 + (m + 0.5) * dMu;
                             
                             fprintf(OutputPitchAngleFiles[i], "%.6e %.6e ", TimeBuffer[t], mu);
                             
                             for (int e = 0; e < nEnergyBins; e++) {
                                 fprintf(OutputPitchAngleFiles[i], "%.6e ", 
                                     PitchAngleDistribution[i](m, e, t, 0));
                             }
                             
                             fprintf(OutputPitchAngleFiles[i], "\n");
                         }
                     }
                     
                     fflush(OutputPitchAngleFiles[i]);
                 }
             }
         }
       }
       
       //----------------------------------------------------------------------------
       // Main function to output all data
       //----------------------------------------------------------------------------
       void Output() {
         OutputPopulation();
         OutputFlux();
         OutputPitchAngleDistribution();
       }
       
       //----------------------------------------------------------------------------
       // Main manager function to be registered with PIC
       //----------------------------------------------------------------------------
       void Manager() {
         // Check if sampling points have been defined
         if (SamplingPoints.size() == 0) return;
         
         // Perform sampling
         Sampling();
         
         // Increment internal counter
         internalCounter++;
         
         // Output data periodically based on OutputIterations setting
         // This can be configured using SetOutputIterations()
         if (internalCounter % OutputIterations == 0) {
             Output();
             Clear();
         }
       }
       
     } // namespace Sample3D
   } // namespace Sampling
 } // namespace SEP

