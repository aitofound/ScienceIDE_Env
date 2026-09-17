
#include "sep.h"


namespace SEP {
namespace AlfvenTurbulence_Kolmogorov {
    namespace FL = PIC::FieldLine;

    // Functions to calculate Kolmogorov turbulence wavenumber bounds
    // Based on empirical scaling laws for solar wind turbulence
    // Input: S - internal coordinate along magnetic field line
    //        iFieldLine - field line index
    // Output: wavenumber in m^-1

    double GetKmin(double S, int iFieldLine) {
        // Get the segment using the provided field line index
        auto Segment = FL::FieldLinesAll[iFieldLine].GetSegment(static_cast<int>(S));
        
        // Convert to Cartesian coordinates
        double x[3]; // Assuming 3D Cartesian coordinates
        Segment->GetCartesian(x, S);
        
        // Calculate heliocentric distance R in AU
        double R = sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]) / 1.496e11; // Convert from meters to AU
        
        // Kolmogorov outer-scale wavenumber scaling
        // kmin(R) ≈ kmin(R0) * (R/R0)^(-α), where α ≈ 1.1-1.3
        const double kmin_R0 = 6.28e-9; // m^-1 at 1 AU
        const double R0 = 1.0; // AU (reference distance)
        const double alpha = 1.2; // Using middle value of α range (1.1-1.3)
        
        double kmin = kmin_R0 * pow(R / R0, -alpha);
        
        return kmin;
    }

    double GetKmax(double S, int iFieldLine) {
        // Get the segment using the provided field line index
        auto Segment = FL::FieldLinesAll[iFieldLine].GetSegment(static_cast<int>(S));
        
        // Convert to Cartesian coordinates
        double x[3]; // Assuming 3D Cartesian coordinates
        Segment->GetCartesian(x, S);
        
        // Calculate heliocentric distance R in AU
        double R = sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]) / 1.496e11; // Convert from meters to AU
        
        // Kolmogorov ion-scale wavenumber scaling (empirical radial scaling)
        // kmax(R) ≈ kmax(R0) * (R/R0)^(-1.08)
        const double kmax_R0 = 3.14e-6; // m^-1 at 1 AU
        const double R0 = 1.0; // AU (reference distance)
        const double beta = 1.08; // Empirical scaling exponent
        
        double kmax = kmax_R0 * pow(R / R0, -beta);
        
        return kmax;
    }

} // namespace AlfvenTurbulence_Kolmogorov
} // namespace SEP



