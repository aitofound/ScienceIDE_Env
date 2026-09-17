/* 
 * sample3d_init.cpp
 * 
 * Implementation of initialization functions for 3D particle sampling
 */

 #include "sample3d.h"
 #include "pic.h"
 #include <cmath>
 #include <cstdio>
 #include <cstring>
 #include <sys/stat.h>
 
 namespace SEP {
   namespace Sampling {
     namespace Sample3D {
       size_t MaxSampleLocations = 50;  // Default maximum number of sampling locations
       
       // Energy channel information
       double MinEnergy = 0.01 * MeV2J;     // Default minimum energy: 0.01 MeV 
       double MaxEnergy = 300.0 * MeV2J;    // Default maximum energy: 300 MeV
       int nEnergyBins = 50;                // Default number of energy bins
       double dLogEnergy;                   // Will be computed in Init()
       
       // Pitch angle distribution settings
       int nPitchAngleBins = 20;            // Default number of pitch angle bins
       double dMu;                          // Will be computed in Init()
       
       // Sampling points and regions
       std::vector<SamplingPoint> SamplingPoints;
       
       // Time information
       double SamplingTime = 0.0;
       int SamplingCounter = 0;
       int OutputIterations = 10;  // Default: output every 10 iterations
       int internalCounter = 0;    // Counter for tracking output timing
       
       // Time buffer for storing sampling time points
       std::vector<double> TimeBuffer;
       int MaxTimePoints = 1000;  // Maximum number of time points to store
       
       // Output directories
       std::string OutputDir = "Sample3D";
       std::string PopulationDir;
       std::string FluxDir;
       std::string ReturnFluxDir;
       std::string PitchAngleDir;
       
       // Sampling data structures
       std::vector<array_3d<double>> EnergyChannelPopulation;
       std::vector<array_4d<double>> PitchAngleDistribution;
       std::vector<array_3d<double>> FluxByEnergy;
       std::vector<array_3d<double>> ReturnFluxByEnergy;
       std::vector<double*> BackgroundB;
       
       // Output file handles
       std::vector<FILE*> OutputPopulationFiles;
       std::vector<FILE*> OutputFluxFiles;
       std::vector<FILE*> OutputReturnFluxFiles;
       std::vector<FILE*> OutputPitchAngleFiles;
       
       // Static max allowed output length
       const int MAX_FILENAME_LEN = 1024;
       
       //----------------------------------------------------------------------------
       // Function to initialize the Sample3D module
       //----------------------------------------------------------------------------
       void Init() {
         // Register the sampling procedure with PIC
         PIC::IndividualModelSampling::SamplingProcedure.push_back(Manager);
         
         // Calculate derived parameters
         dLogEnergy = log(MaxEnergy / MinEnergy) / nEnergyBins;
         dMu = 2.0 / nPitchAngleBins;
         
         // Create output directories
         PopulationDir = OutputDir + "/Population";
         FluxDir = OutputDir + "/Flux";
         ReturnFluxDir = OutputDir + "/ReturnFlux";
         PitchAngleDir = OutputDir + "/PitchAngle";
         
         if (PIC::ThisThread == 0) {
             // Create the main directory
             char cmd[MAX_FILENAME_LEN];

	     sprintf(cmd, "rm -rf %s", OutputDir.c_str());
             system(cmd); 
             
             sprintf(cmd, "mkdir -p %s", OutputDir.c_str());
             system(cmd);
             
             sprintf(cmd, "mkdir -p %s", PopulationDir.c_str());
             system(cmd);
             
             sprintf(cmd, "mkdir -p %s", FluxDir.c_str());
             system(cmd);
             
             sprintf(cmd, "mkdir -p %s", ReturnFluxDir.c_str());
             system(cmd);
             
             sprintf(cmd, "mkdir -p %s", PitchAngleDir.c_str());
             system(cmd);
         }
         
         // Initialize empty vectors (will be populated as sampling points are added)
         EnergyChannelPopulation.clear();
         PitchAngleDistribution.clear();
         FluxByEnergy.clear();
         ReturnFluxByEnergy.clear();
         BackgroundB.clear();
         
         OutputPopulationFiles.clear();
         OutputFluxFiles.clear();
         OutputReturnFluxFiles.clear();
         OutputPitchAngleFiles.clear();
         
         // Initialize the time buffer
         TimeBuffer.clear();
         TimeBuffer.reserve(MaxTimePoints);
         
         // Reset counters
         SamplingTime = 0.0;
         SamplingCounter = 0;
         internalCounter = 0;
       }


//----------------------------------------------------------------------------
// Initialize the vector sizes and max number of sample locations
//----------------------------------------------------------------------------
void InitSampleLocations(size_t maxLocations) {
    // Set the maximum number of sample locations
    MaxSampleLocations = maxLocations;
    
    // Reserve space for all vectors
    SamplingPoints.reserve(MaxSampleLocations);
    EnergyChannelPopulation.reserve(MaxSampleLocations);
    PitchAngleDistribution.reserve(MaxSampleLocations);
    FluxByEnergy.reserve(MaxSampleLocations);
    ReturnFluxByEnergy.reserve(MaxSampleLocations);
    BackgroundB.reserve(MaxSampleLocations);
    OutputPopulationFiles.reserve(MaxSampleLocations);
    OutputFluxFiles.reserve(MaxSampleLocations);
    OutputReturnFluxFiles.reserve(MaxSampleLocations);
    OutputPitchAngleFiles.reserve(MaxSampleLocations);
}

     
       //----------------------------------------------------------------------------
       // Function to add a sampling point
       //----------------------------------------------------------------------------
bool  AddSamplingPoint(const SamplingPoint& point) {
    // Check if we've reached the maximum number of sampling points
    if (SamplingPoints.size() >= MaxSampleLocations) {
       exit(__LINE__,__FILE__,"Error: the max number of sampling locations is reached; Increase the limit with  InitSampleLocations(size_t maxLocations)");
       return false;
    }

    // Add to the vector of sampling points
    SamplingPoints.push_back(point);
    
    // Use emplace_back to construct the objects directly inside the vector
    EnergyChannelPopulation.emplace_back();
    PitchAngleDistribution.emplace_back();
    FluxByEnergy.emplace_back();
    ReturnFluxByEnergy.emplace_back();
    
    // Get references to the new objects
    array_3d<double>& newEnergyPopulation = EnergyChannelPopulation.back();
    array_4d<double>& newPitchAngleDistribution = PitchAngleDistribution.back();
    array_3d<double>& newFlux = FluxByEnergy.back();
    array_3d<double>& newReturnFlux = ReturnFluxByEnergy.back();
    
    // Initialize with appropriate dimensions
    newEnergyPopulation.init(nEnergyBins, MaxTimePoints, 1);
    newPitchAngleDistribution.init(nPitchAngleBins, nEnergyBins, MaxTimePoints, 1);
    newFlux.init(nEnergyBins, MaxTimePoints, 1);
    newReturnFlux.init(nEnergyBins, MaxTimePoints, 1);
    
    // Initialize with zeros
    newEnergyPopulation = 0.0;
    newPitchAngleDistribution = 0.0;
    newFlux = 0.0;
    newReturnFlux = 0.0;
    
    // Allocate and initialize background B field
    double* newB = new double[3];
    newB[0] = 0.0; newB[1] = 0.0; newB[2] = 0.0;
    BackgroundB.push_back(newB);
    
    // If this process is the main thread, create output files
    if (PIC::ThisThread == 0) {
        char filename[MAX_FILENAME_LEN];
        FILE* file;
        
        // Population file
        sprintf(filename, "%s/%s.dat", PopulationDir.c_str(), point.label.c_str());
        file = fopen(filename, "w");
        if (file != NULL) {
            fprintf(file, "VARIABLES = \"Time [s]\"");
            for (int i = 0; i < nEnergyBins; i++) {
                fprintf(file, ", \"E(%e-%e) MeV\"", 
                    MinEnergy * exp(i * dLogEnergy) * J2MeV, 
                    MinEnergy * exp((i + 1) * dLogEnergy) * J2MeV);
            }
            fprintf(file, "\n");
	    fflush(file);
            OutputPopulationFiles.push_back(file);
        }
        
        // Flux file
        sprintf(filename, "%s/%s.dat", FluxDir.c_str(), point.label.c_str());
        file = fopen(filename, "w");
        if (file != NULL) {
            fprintf(file, "VARIABLES = \"Time [s]\"");
            for (int i = 0; i < nEnergyBins; i++) {
                fprintf(file, ", \"E(%e-%e) MeV\"", 
                    MinEnergy * exp(i * dLogEnergy) * J2MeV, 
                    MinEnergy * exp((i + 1) * dLogEnergy) * J2MeV);
            }
            fprintf(file, "\n");
	    fflush(file);
            OutputFluxFiles.push_back(file);
        }
        
        // Return flux file
        sprintf(filename, "%s/%s.dat", ReturnFluxDir.c_str(), point.label.c_str());
        file = fopen(filename, "w");
        if (file != NULL) {
            fprintf(file, "VARIABLES = \"Time [s]\"");
            for (int i = 0; i < nEnergyBins; i++) {
                fprintf(file, ", \"E(%e-%e) MeV\"", 
                    MinEnergy * exp(i * dLogEnergy) * J2MeV, 
                    MinEnergy * exp((i + 1) * dLogEnergy) * J2MeV);
            }
            fprintf(file, "\n");
	    fflush(file);
            OutputReturnFluxFiles.push_back(file);
        }
        
        // Pitch angle file
        sprintf(filename, "%s/%s.dat", PitchAngleDir.c_str(), point.label.c_str());
        file = fopen(filename, "w");
        if (file != NULL) {
            fprintf(file, "VARIABLES = \"Time [s]\", \"Pitch Angle [cos(PA)]\"");
            for (int i = 0; i < nEnergyBins; i++) {
                fprintf(file, ", \"E(%e-%e) MeV\"", 
                    MinEnergy * exp(i * dLogEnergy) * J2MeV, 
                    MinEnergy * exp((i + 1) * dLogEnergy) * J2MeV);
            }
            fprintf(file, "\n");
	    fflush(file);
            OutputPitchAngleFiles.push_back(file);
        }
    }

    return true;
}     


       //----------------------------------------------------------------------------
       // Function to set the energy range and bins
       //----------------------------------------------------------------------------
       void SetEnergyRange(double emin, double emax, int nbins) {
         MinEnergy = emin;
         MaxEnergy = emax;
         nEnergyBins = nbins;
         dLogEnergy = log(MaxEnergy / MinEnergy) / nEnergyBins;
       }
     
       //----------------------------------------------------------------------------
       // Function to set the number of pitch angle bins
       //----------------------------------------------------------------------------
       void SetPitchAngleBins(int nbins) {
         nPitchAngleBins = nbins;
         dMu = 2.0 / nPitchAngleBins;
       }
       
       //----------------------------------------------------------------------------
       // Function to set the number of iterations between outputs
       //----------------------------------------------------------------------------
       void SetOutputIterations(int iterations) {
         if (iterations > 0) {
           OutputIterations = iterations;
         } else {
           // If invalid value, keep default
           OutputIterations = 10;
         }
       }
     
       //----------------------------------------------------------------------------
       // Function to check if a point is in the cell containing the sampling point
       //----------------------------------------------------------------------------
       bool IsPointInCell(double* x, double* cellMin, double* cellMax) {
         return (x[0] >= cellMin[0] && x[0] <= cellMax[0] &&
                 x[1] >= cellMin[1] && x[1] <= cellMax[1] &&
                 x[2] >= cellMin[2] && x[2] <= cellMax[2]);
       }
     
     } // namespace Sample3D
   } // namespace Sampling
 } // namespace SEP
 
