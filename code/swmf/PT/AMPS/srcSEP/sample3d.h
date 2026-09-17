/* 
 * sample3d.h
 * 
 * 3D particle sampling for SEP model
 * Samples particle population in different energy channels as a function of time and locations
 * Samples pitch angle distribution for different locations as a function of time in 3D
 */

 #ifndef _SAMPLE3D_H_
 #define _SAMPLE3D_H_
 
 #include "sep.h"
 #include "pic.h"
 #include "array_3d.h"
 #include "array_4d.h"
 #include "array_5d.h"
 #include <vector>
 #include <string>
 
 namespace SEP {
   namespace Sampling {
     namespace Sample3D {
       extern size_t MaxSampleLocations;  // Maximum number of sampling locations 

       // Structure to define a sampling point in 3D space
       struct SamplingPoint {
         double x[3];          // Position in 3D space
         std::string label;    // Label for the sampling point
         
         SamplingPoint() {
           x[0] = 0.0; x[1] = 0.0; x[2] = 0.0;
           label = "point";
         }
         
         SamplingPoint(double x1, double x2, double x3, const std::string& lbl) {
           x[0] = x1; x[1] = x2; x[2] = x3;
           label = lbl;
         }
       };
       
       // Energy channel information
       extern double MinEnergy;             // Minimum energy [J]
       extern double MaxEnergy;             // Maximum energy [J]
       extern int nEnergyBins;              // Number of energy bins
       extern double dLogEnergy;            // Logarithmic energy step
       
       // Pitch angle distribution settings
       extern int nPitchAngleBins;          // Number of pitch angle bins
       extern double dMu;                   // Pitch angle step (in cos(pitch angle))
       
       // Sampling points and regions
       extern std::vector<SamplingPoint> SamplingPoints;
       
       // Time information
       extern double SamplingTime;          // Total sampling time
       extern int SamplingCounter;          // Number of samples collected
       extern int OutputIterations;         // Number of iterations between outputs
       extern int internalCounter;          // Internal counter for output decision
       extern std::vector<double> TimeBuffer; // Buffer to store sampling times
       extern int MaxTimePoints;            // Maximum number of time points to store
       
       // Sampling data structures
       // Population by energy channel and location over time
       extern std::vector<array_3d<double>> EnergyChannelPopulation;  // [point][energy_bin][time]
       
       // Pitch angle distribution by energy channel and location over time
       extern std::vector<array_4d<double>> PitchAngleDistribution;   // [point][mu_bin][energy_bin][time]
       
       // Flux by energy channel and location over time
       extern std::vector<array_3d<double>> FluxByEnergy;             // [point][energy_bin][time]
       
       // Return flux (particles moving toward the Sun) by energy channel and location
       extern std::vector<array_3d<double>> ReturnFluxByEnergy;       // [point][energy_bin][time]
       
       // Background magnetic field data at each sampling point
       extern std::vector<double*> BackgroundB;                       // [point][3]
       
       // Output file handles
       extern std::vector<FILE*> OutputPopulationFiles;
       extern std::vector<FILE*> OutputFluxFiles;
       extern std::vector<FILE*> OutputReturnFluxFiles;
       extern std::vector<FILE*> OutputPitchAngleFiles;
       
       // Functions
       void Init();                                  // Initialize the sampling module
       void InitSampleLocations(size_t maxLocations); // Set the maximum number of sample locations
       bool AddSamplingPoint(const SamplingPoint& point); // Add a sampling point
       void SetEnergyRange(double emin, double emax, int nbins); // Set the energy range and bins
       void SetPitchAngleBins(int nbins);           // Set the number of pitch angle bins
       void SetOutputIterations(int iterations);    // Set number of iterations between outputs
       
       void Sampling();                             // Main sampling function called by AMPS
       void SampleParticlesAtPoint(int pointIndex); // Sample particles at a specific point
       
       void Output();                               // Output collected data to files
       void OutputPopulation();                     // Output population data
       void OutputFlux();                           // Output flux data
       void OutputPitchAngleDistribution();         // Output pitch angle distribution
       
       void Clear();                                // Clear sampling buffers
       
       void Manager();                              // Main manager function to be registered with PIC
       
       bool IsPointInSamplingRegion(double* x, int pointIndex); // Check if particle is in sampling region
     }
   }
 }
 
 #endif /* _SAMPLE3D_H_ */

