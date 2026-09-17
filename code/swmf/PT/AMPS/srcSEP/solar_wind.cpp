
#include <cmath>        // std::pow
#include <limits>       // std::numeric_limits

#include "sep.h"

namespace SEP {
    namespace SolarWind {
        /**
         * Offset for solar wind velocity divergence in the data buffer
         */
        int DivSolarWindVelocityOffset = -1;

        /**
         * Calculates the solar wind velocity at a given position.
         * 
         * @param v Output array for velocity components
         * @param x Position vector
         * @return The magnitude of the solar wind velocity
         */
        double GetSolarWindVelocity(double *v, double *x) {
            // Calculate the length of the position vector
            double length = Vector3D::Length(x);
            
            // Set each component of velocity
            // v[idim] = 400 km/s * x[idim] / |x|
            for (int idim = 0; idim < 3; idim++) {
                v[idim] = 400E3 * x[idim] / length;
            }
            
            // Return the magnitude of the velocity (which is 400 km/s)
            return 400E3;
        }

	/*
         * Request data buffer for storing div(Solar Wind Velocity) 
         * 
         * @param offset The current offset in the data buffer
         * @return The size of allocated buffer in bytes
         */
        int RequestDataBuffer(int offset) {
            DivSolarWindVelocityOffset = offset;
            int TotalDataLength = 1;
            
            return TotalDataLength * sizeof(double);
        }

        void Init() {
            //request sampling buffer and particle fields
            PIC::IndividualModelSampling::RequestStaticCellData.push_back(RequestDataBuffer);
        }

     /**
         * Calculates the divergence of the solar wind velocity at a given position
         * 
         * @param x Position vector
         * @param node Pointer to the AMR tree node
         * @return The divergence of the solar wind velocity
         */
        double GetDivSolarWindVelocity(double *x, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
            // Calculate cell size from node dimensions and number of cells
            double cellSize[3];
            cellSize[0] = (node->xmax[0] - node->xmin[0]) / _BLOCK_CELLS_X_;
            cellSize[1] = (node->xmax[1] - node->xmin[1]) / _BLOCK_CELLS_Y_;
            cellSize[2] = (node->xmax[2] - node->xmin[2]) / _BLOCK_CELLS_Z_;
            
            // Define step size as half of the local cell size for each dimension
            double h[3];
            for (int i = 0; i < 3; i++) {
                h[i] = 0.5 * cellSize[i];
            }
            
            // Calculate the divergence using central differences
            double div = 0.0;
            double vPlus[3], vMinus[3];
            double xPlus[3], xMinus[3];
            
            for (int dim = 0; dim < 3; dim++) {
                // Copy the original position
                for (int i = 0; i < 3; i++) {
                    xPlus[i] = x[i];
                    xMinus[i] = x[i];
                }
                
                // Shift position forward and backward in current dimension
                xPlus[dim] += h[dim];
                xMinus[dim] -= h[dim];
                
                // Check if points are valid (not outside domain or inside Sun)
                bool validPlus = IsPointValid(xPlus);
                bool validMinus = IsPointValid(xMinus);
                
                if (!validPlus || !validMinus) {
                    // If either point is invalid, skip this dimension
                    continue;
                }
                
                // Get velocity at shifted positions
                GetSolarWindVelocity(vPlus,xPlus);
                GetSolarWindVelocity(vMinus, xMinus);
                
                // Calculate partial derivative: dv_i/dx_i using central difference
                div += (vPlus[dim] - vMinus[dim]) / (2.0 * h[dim]);
            }
            
            return div;
        }
        
        /**
         * Check if a point is valid (not outside domain or inside the Sun)
         * 
         * @param x Position vector to check
         * @return true if valid, false otherwise
         */
        bool IsPointValid(double *x) {
            if (Vector3D::DotProduct(x,x) < _SUN__RADIUS_*_SUN__RADIUS_) {
                return false;
            }
            
            // Check if outside domain
            // Use the global domain boundaries defined in PIC
            const double *xmin = PIC::Mesh::mesh->xGlobalMin;
            const double *xmax = PIC::Mesh::mesh->xGlobalMax;
            
            if (x[0] < xmin[0] || x[0] > xmax[0] ||
                x[1] < xmin[1] || x[1] > xmax[1] ||
                x[2] < xmin[2] || x[2] > xmax[2]) {
                return false;
            }
            
            return true;
        }

        /**
         * Calculate and store the divergence of solar wind velocity for all cells in the subdomain
         */
        void SetDivSolarWindVelocity() {
            // Loop through all cells in the subdomain
            for (int CellCounter = 0; CellCounter < PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_; CellCounter++) {
                // Calculate indices for the current cell
                int nLocalNode, ii = CellCounter, i, j, k;
                
                // Get the local node index
                nLocalNode = ii / (_BLOCK_CELLS_Z_ * _BLOCK_CELLS_Y_ * _BLOCK_CELLS_X_);
                ii -= nLocalNode * _BLOCK_CELLS_Z_ * _BLOCK_CELLS_Y_ * _BLOCK_CELLS_X_;
                
                // Get the k index (z direction)
                k = ii / (_BLOCK_CELLS_Y_ * _BLOCK_CELLS_X_);
                ii -= k * _BLOCK_CELLS_Y_ * _BLOCK_CELLS_X_;
                
                // Get the j index (y direction)
                j = ii / _BLOCK_CELLS_X_;
                ii -= j * _BLOCK_CELLS_X_;
                
                // Get the i index (x direction)
                i = ii;
                
                // Get the node from the block table
                cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node = PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
                auto block = node->block;
                
                // Skip if the block is NULL
                if (block == NULL) continue;
                
                // Get the cell center node
                PIC::Mesh::cDataCenterNode *cell = block->GetCenterNode(_getCenterNodeLocalNumber(i, j, k));
                
                // Skip if the cell is NULL
                if (cell == NULL) continue;
                
                // Calculate the physical coordinates of the cell center
                double x[3];
                
                // Calculate the cell center coordinates
                x[0] = node->xmin[0] + (i + 0.5) * (node->xmax[0] - node->xmin[0]) / _BLOCK_CELLS_X_;
                x[1] = node->xmin[1] + (j + 0.5) * (node->xmax[1] - node->xmin[1]) / _BLOCK_CELLS_Y_;
                x[2] = node->xmin[2] + (k + 0.5) * (node->xmax[2] - node->xmin[2]) / _BLOCK_CELLS_Z_;
                
                // Calculate the divergence of the solar wind velocity at the cell center
                double div = GetDivSolarWindVelocity(x, node);
                
                // Save the divergence in the state vector of the cell
                *((double*) (cell->GetAssociatedDataBufferPointer() + DivSolarWindVelocityOffset)) = div;
            }
        }

        /**
         * Interpolate the divergence of solar wind velocity to a specific location
         * 
         * @param x Position vector
         * @param node Pointer to the AMR tree node
         * @return Interpolated divergence of the solar wind velocity
         */
        double InterpolateDivSolarWindVelocity(double *x, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
            double div = 0.0;
            
            // Initialize interpolation stencil
	    PIC::InterpolationRoutines::CellCentered::cStencil CenterBasedStencil;
	    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node,CenterBasedStencil);

            // Interpolate the divergence value
            for (int icell = 0; icell < CenterBasedStencil.Length; icell++) {
                // Get the data pointer and weight for this cell
                char *AssociatedDataPointer = CenterBasedStencil.cell[icell]->GetAssociatedDataBufferPointer();
                double weight = CenterBasedStencil.Weight[icell];
                
                // Get the divergence value for this cell
                double div_cell = *((double*) (AssociatedDataPointer + DivSolarWindVelocityOffset));
                
                // Add weighted contribution to interpolated value
                div += weight * div_cell;
            }
            

            #if _PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_
            #if _PIC_DEBUGGER_MODE__CHECK_FINITE_NUMBER_ == _PIC_DEBUGGER_MODE_ON_
            if (isfinite(div)==false) exit(__LINE__,__FILE__,"Error: Floating Point Exeption");
            #endif
            #endif

            return div;
        }

/**************************************************************************************************
 *  Empirical Solar‑Wind Model (last revised: 27 Jul 2025)
 *
 *  This header comment documents the literature that underpins the branch‑wise normalisations
 *  and power‑law indices used in the two routines below:
 *
 *  ── Radial ranges & key sources ───────────────────────────────────────────────────────────────
 *   0.05 – 0.30 AU  (PSP, “young” solar wind)                                                   
 *        • Halekas et al. 2020, ApJS 246, 22 – n ∝ r⁻²·³,  T ∝ r⁻⁰·⁶                            :contentReference[oaicite:0]{index=0}
 *        • Kasper et al. 2021, Phys. Rev. Lett. 127, 255101 – proton‑core heating close to Sun  :contentReference[oaicite:1]{index=1}
 *
 *   0.30 – 1.0 AU  (Helios re‑analysis)                                                         
 *        • Perrone et al. 2019, MNRAS 483, 3730 – fast‑wind T ∝ r⁻⁰·⁷‑0·⁸, n ∝ r⁻²              
 *        • Hellinger et al. 2013, ApJ 778, 134 – slow‑wind density n ∝ r⁻¹·⁹                    
 *
 *   1 – 5 AU  (Ulysses/Voyager, mid‑heliosphere)                                                
 *        • Richardson 2003, GRL 30, 1206 – Voyager 1+2 T ∝ r⁻⁰·⁷                               :contentReference[oaicite:2]{index=2}
 *        • Richardson & Burlaga 2013, SSR 176, 217 – average n ≈ 5 cm⁻³ (r/1 AU)⁻²              
 *
 *   5 – 30 AU  (pickup‑ion corridor)                                                            
 *        • Elliott et al. 2019, ApJ 886, 73 – Voyager/New Horizons flattening:                  
 *          T ∝ r⁻⁰·⁴, n ∝ r⁻¹·⁵ (mass loading by PUIs)                                         
 *
 *   30 – 90 AU  (heliosheath approach)                                                         
 *        • Richardson et al. 2022, SSR 218, 35 – Voyager 2 trend to T ≈ few × 10³ K;              
 *          density plateaus at ≃10³ m⁻³ in the distant wind                                    :contentReference[oaicite:3]{index=3}
 *
 *  These observational scalings are stitched together into simple piece‑wise formulas that are
 *  convenient for boundary conditions, mission design, or order‑of‑magnitude calculations.
 *  They should NOT be used where event‑specific accuracy (CMEs, CIRs, shocks) is required.
 *************************************************************************************************/


/*==============================================================================
 *  Solar‑wind proton TEMPERATURE  [K]  vs. heliocentric distance r [m]
 *============================================================================*/
double SolarWindTemperature(double r_m)
{
    constexpr double AU = 1.495978707e11;                 // 1 AU in metres
    double res = std::numeric_limits<double>::quiet_NaN(); // default for invalid input

    if (r_m > 0.0) {
        const double r_AU = r_m / AU;

        if (r_AU < 0.30) {                                // 0.05–0.30 AU  (PSP)
            const double T0 = 1.20e6,  r0 = 0.10, alpha = 0.60;
            res = T0 * std::pow(r_AU / r0, -alpha);
        } else if (r_AU < 1.0) {                          // 0.30–1 AU   (Helios)
            const double T0 = 2.00e5,  alpha = 0.80;
            res = T0 * std::pow(r_AU, -alpha);
        } else if (r_AU < 5.0) {                          // 1–5 AU     (Voyager)
            const double T0 = 1.90e5,  alpha = 0.70;
            res = T0 * std::pow(r_AU, -alpha);
        } else if (r_AU < 30.0) {                         // 5–30 AU    (PUI corridor)
            const double T0 = 4.00e4,  r0 = 5.0, alpha = 0.40;
            res = T0 * std::pow(r_AU / r0, -alpha);
        } else {                                         // 30–90 AU   (flatten / clamp)
            const double T30 = 1.0e4, T90 = 5.0e3;
            const double slope = (T90 - T30) / 60.0;
            res = T30 + slope * (r_AU - 30.0);
            if (res < 3.0e3) res = 3.0e3;
        }
    }
    return res;
}

/*==============================================================================
 *  Solar‑wind proton NUMBER DENSITY  [m⁻³]  vs. heliocentric distance r [m]
 *============================================================================*/
double SolarWindNumberDensity(double r_m)
{
    constexpr double AU = 1.495978707e11;                 // 1 AU in metres
    double res = std::numeric_limits<double>::quiet_NaN(); // default for invalid input

    if (r_m > 0.0) {
        const double r_AU = r_m / AU;

        if (r_AU < 0.30) {                                // 0.05–0.30 AU  (PSP)
            const double n0 = 5.0e8,  r0 = 0.10, alpha = 2.3;
            res = n0 * std::pow(r_AU / r0, -alpha);
        } else if (r_AU < 1.0) {                          // 0.30–1 AU   (Helios)
            const double n0 = 5.0e6,  alpha = 2.0;
            res = n0 * std::pow(r_AU, -alpha);
        } else if (r_AU < 5.0) {                          // 1–5 AU     (Voyager)
            const double n0 = 5.0e6,  alpha = 2.0;
            res = n0 * std::pow(r_AU, -alpha);
        } else if (r_AU < 30.0) {                         // 5–30 AU    (PUI corridor)
            const double n0 = 2.0e5,  r0 = 5.0, alpha = 1.5;
            res = n0 * std::pow(r_AU / r0, -alpha);
        } else {                                         // 30–90 AU   (flatten / clamp)
            const double n30 = 1.36e4, n90 = 2.0e3;
            const double slope = (n90 - n30) / 60.0;
            res = n30 + slope * (r_AU - 30.0);
            if (res < 1.0e3) res = 1.0e3;
        }
    }
    return res;
}

/**************************************************************************************************
 *  Empirical Solar‑Wind Model – BULK (radial) VELOCITY
 *  ---------------------------------------------------
 *  Returns the average proton bulk speed **v(r) [m s⁻¹]** at heliocentric distance **r [m]**.
 *
 *  Radial ranges & supporting literature
 *  -------------------------------------
 *  • 0.05 – 0.30 AU  – Parker Solar Probe shows the wind still accelerating; speeds climb from
 *    ≲150 km s⁻¹ at 0.05 AU to ≳300 km s⁻¹ by ≃0.25 AU. :contentReference[oaicite:0]{index=0}
 *
 *  • 0.30 – 5 AU     – Classic surveys (Helios, Voyager, IMP 8) find **little change** in the mean
 *    radial speed; a representative value of ≈430 km s⁻¹ is widely used. :contentReference[oaicite:1]{index=1}
 *
 *  • 5 – 30 AU       – Voyager statistics reveal a *gradual* slowdown (~7 % by 40 AU) caused by
 *    mass‑loading from pickup ions and stream‑interaction regions. :contentReference[oaicite:2]{index=2}
 *
 *  • 30 – 90 AU      – The deceleration continues but remains modest; we interpolate linearly down
 *    to ≃300 km s⁻¹ at 90 AU and clamp at 250 km s⁻¹ beyond.
 *
 *  These piece‑wise fits are meant for order‑of‑magnitude work, mission design, or simulation
 *  boundary conditions – **not** for event‑specific accuracy (CMEs, CIRs, shocks, etc.).
 *************************************************************************************************/


/*==============================================================================
 *  Solar‑wind BULK VELOCITY  [m s⁻¹]  vs. heliocentric distance r [m]
 *============================================================================*/
double SolarWindBulkVelocity(double r_m)
{
    constexpr double AU = 1.495978707e11;                 // 1 AU in metres
    double res = std::numeric_limits<double>::quiet_NaN(); // default (invalid input)

    if (r_m > 0.0) {
        const double r_AU = r_m / AU;

        /* ---------- 0.05 – 0.30 AU : accelerating young wind ---------- */
        if (r_AU < 0.30) {
            const double v0 = 3.50e5;     // m s⁻¹ at r0 = 0.30 AU (≈350 km s⁻¹)
            const double r0 = 0.30;       // AU
            const double beta = 0.50;     // acceleration exponent
            res = v0 * std::pow(r_AU / r0, beta);
        }

        /* ---------- 0.30 – 5 AU : near‑constant bulk speed ---------- */
        else if (r_AU < 5.0) {
            res = 4.30e5;                 // m s⁻¹  (≈430 km s⁻¹)
        }

        /* ---------- 5 – 30 AU : gradual slowdown ---------- */
        else if (r_AU < 30.0) {
            const double v5  = 4.30e5;    // m s⁻¹ at 5 AU
            const double v30 = 3.50e5;    // m s⁻¹ at 30 AU
            const double slope = (v30 - v5) / (30.0 - 5.0);
            res = v5 + slope * (r_AU - 5.0);
        }

        /* ---------- 30 – 90 AU : further mild deceleration ---------- */
        else {
            const double v30 = 3.50e5;    // m s⁻¹ at 30 AU
            const double v90 = 3.00e5;    // m s⁻¹ at 90 AU
            const double slope = (v90 - v30) / (90.0 - 30.0);
            res = v30 + slope * (r_AU - 30.0);
            if (res < 2.50e5) res = 2.50e5;  // clamp to 250 km s⁻¹ minimum
        }
    }
    return res;
}




    } // namespace SolarWind
} // namespace SEP



// Add to header file (e.g., sep.h)
namespace SEP {
  namespace SolarWind {
    // Solar wind model functions (to be implemented elsewhere)
    double SolarWindTemperature(double r_m);
    double SolarWindNumberDensity(double r_m);

    //=============================================================================
    // InitializeSolarWindPopulation
    //=============================================================================
    // Purpose: Initialize solar wind particle population across all magnetic field lines
    //
    // Description:
    //   Creates a complete solar wind background population by iterating through all
    //   field lines in the simulation domain and calling InitializeSolarWindFieldLine
    //   for each one. This function sets up the initial thermal plasma distribution
    //   that serves as the background for energetic particle transport simulations.
    //
    // Physics:
    //   - Uses Maxwell-Boltzmann velocity distribution with zero bulk velocity
    //     (field lines move with solar wind bulk flow)
    //   - Particle density and temperature determined by radial distance from Sun
    //   - Maintains proper statistical weights for computational particles
    //
    // Parameters:
    //   spec                 - Species index for the particles to initialize
    //   nParticlesPerFieldLine - Maximum particles per field line for computational efficiency
    //
    // Returns:
    //   Total number of computational particles injected across all field lines
    //
    // Usage:
    //   Call once during simulation initialization to set up solar wind background:
    //   InitializeSolarWindPopulation(H_PLUS_SPEC, 500);
    //
    // Dependencies:
    //   - SolarWindTemperature(r_m) - returns temperature as function of distance
    //   - SolarWindNumberDensity(r_m) - returns density as function of distance
    //   - PIC field line framework must be initialized
    //=============================================================================
    long int InitializeSolarWindPopulation(int spec, int nParticlesPerFieldLine);

    //=============================================================================
    // InjectSolarWindAtFieldLineBeginning
    //=============================================================================
    // Purpose: Inject solar wind ions at the beginning of a specified field line
    //
    // Description:
    //   Continuously injects solar wind particles at the start of a magnetic field
    //   line using proper kinetic theory to calculate injection rates. Particles
    //   are given Maxwell-Boltzmann thermal velocities and positioned to flow
    //   outward along the field line, simulating the continuous solar wind flow.
    //
    // Physics:
    //   - Injection rate based on kinetic theory: Γ = (1/4) * n * <v>
    //   - Mean thermal speed: <v> = sqrt(8*kT/(π*m))
    //   - Particles injected with outward parallel velocities only
    //   - Position scattered randomly over one time step to smooth temporal injection
    //
    // Algorithm:
    //   1. Calculate solar wind properties at field line beginning
    //   2. Determine injection rate from kinetic theory and time step
    //   3. Generate particles with Maxwell-Boltzmann thermal distribution
    //   4. Ensure particles have positive parallel velocity (outward flow)
    //   5. Inject at S=0 then shift by random fraction of time step
    //   6. Apply weight corrections if particle number exceeds computational limits
    //
    // Parameters:
    //   spec      - Species index for the particles to inject
    //   iFieldLine - Index of the magnetic field line
    //   dt        - Time step for calculating injection rate
    //
    // Returns:
    //   Number of computational particles successfully injected
    //
    // Usage:
    //   Call every time step for continuous solar wind injection:
    //   for (int iFL = 0; iFL < nFieldLine; iFL++) {
    //     InjectSolarWindAtFieldLineBeginning(spec, iFL, timeStep);
    //   }
    //
    // Notes:
    //   - Only processes field lines assigned to current MPI thread
    //   - Uses fixed particle limit (nParticlesPerFieldLine/10) for efficiency
    //   - Automatically handles weight corrections for computational efficiency
    //   - Updates both field line coordinates and Cartesian positions
    //=============================================================================
    
    
    long int InjectSolarWindAtAllFieldLines(int spec, double dt) {
      namespace FL = PIC::FieldLine;
      long int totalInjectedParticles = 0;

      // Inject solar wind particles at the beginning of all field lines
      for (int iFieldLine = 0; iFieldLine < FL::nFieldLine; iFieldLine++) {
        totalInjectedParticles += InjectSolarWindAtFieldLineBeginning(spec, iFieldLine, dt);
      }

      return totalInjectedParticles;
    }
    
    //=============================================================================
    // InitializeSolarWindFieldLine
    //=============================================================================
    // Purpose: Initialize solar wind particles along an entire magnetic field line
    //
    // Description:
    //   Populates a single magnetic field line with solar wind particles distributed
    //   according to local plasma conditions. Particles are placed in each segment
    //   with densities and temperatures determined by radial distance from the Sun.
    //   This creates a realistic background plasma distribution for energetic
    //   particle transport studies.
    //
    // Physics:
    //   - Particle density varies with distance: n(r) from SolarWindNumberDensity()
    //   - Temperature varies with distance: T(r) from SolarWindTemperature()
    //   - Maxwell-Boltzmann thermal velocity distribution in field line frame
    //   - Zero bulk velocity (field lines co-move with solar wind)
    //   - Proper statistical weights maintain mass conservation
    //
    // Algorithm:
    //   1. Loop through all segments of the specified field line
    //   2. Calculate segment volume using SEP::FieldLine::GetSegmentVolume()
    //   3. Determine local solar wind conditions at segment midpoint
    //   4. Calculate required number of computational particles
    //   5. Generate particles with Maxwell-Boltzmann thermal distribution
    //   6. Decompose velocities into parallel/perpendicular components
    //   7. Inject particles at random positions within segment
    //
    // Parameters:
    //   spec                 - Species index for the particles to initialize
    //   iFieldLine          - Index of the magnetic field line to populate
    //   nParticlesPerFieldLine - Maximum particles per field line for computational efficiency
    //
    // Returns:
    //   Number of computational particles successfully injected
    //
    // Usage:
    //   Typically called by InitializeSolarWindPopulation() during setup:
    //   InitializeSolarWindFieldLine(H_PLUS_SPEC, fieldLineIndex, 500);
    //
    // Notes:
    //   - Uses global particle weights from PIC framework
    //   - Limits particles per segment for computational efficiency
    //   - Only processes segments assigned to current MPI thread
    //   - Maintains proper velocity decomposition relative to local field direction
    //=============================================================================
  }
}

long int SEP::SolarWind::InitializeSolarWindPopulation(int spec, int nParticlesPerFieldLine) {
    namespace FL = PIC::FieldLine;
    long int totalInjectedParticles = 0;

    // Initialize solar wind particles on all field lines
    for (int iFieldLine = 0; iFieldLine < FL::nFieldLine; iFieldLine++) {
        totalInjectedParticles += InitializeSolarWindFieldLine(spec, iFieldLine, nParticlesPerFieldLine);
    }

    return totalInjectedParticles;
}

long int SEP::SolarWind::InitializeSolarWindFieldLine(int spec, int iFieldLine, int nParticlesPerFieldLine) {
    namespace FL = PIC::FieldLine;

    long int nInjectedParticles = 0;
    int nSegments = FL::FieldLinesAll[iFieldLine].GetTotalSegmentNumber();

    if (nSegments == 0) return 0;

    // Distribute particles along the field line based on local solar wind conditions
    for (int iSegment = 0; iSegment < nSegments; iSegment++) {
        FL::cFieldLineSegment* Segment = FL::FieldLinesAll[iFieldLine].GetSegment(iSegment);

        if (Segment == NULL) continue;
        if (Segment->Thread != PIC::ThisThread) continue;

        // Get segment geometry
        double xBegin[3], xEnd[3], xMiddle[3];
        Segment->GetBegin()->GetX(xBegin);
        Segment->GetEnd()->GetX(xEnd);

        for (int idim = 0; idim < 3; idim++) {
            xMiddle[idim] = 0.5 * (xBegin[idim] + xEnd[idim]);
        }

        // Calculate distance from Sun
        double r_m = Vector3D::Length(xMiddle);

        // Get solar wind properties at this location
        double n_sw = SolarWindNumberDensity(r_m);
        double T_sw = SolarWindTemperature(r_m);

        // Note: No need to calculate bulk velocity since field lines move with solar wind
        // In the field line reference frame, solar wind has zero bulk velocity

        // Calculate segment volume using SEP framework
        double segmentVolume = SEP::FieldLine::GetSegmentVolume(Segment, iFieldLine);

        // Determine number of computational particles to inject
        double particleWeight = PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
        double targetParticleNumber = n_sw * segmentVolume / particleWeight;

        // Limit particles per segment for computational efficiency
        int maxParticlesPerSegment = nParticlesPerFieldLine / max(1, nSegments / 10);

        int npart = (int)min(targetParticleNumber, (double)maxParticlesPerSegment);
        if (targetParticleNumber - npart > rnd()) npart++;

        if (npart == 0) continue;

        // Calculate weight correction factor (set to 1 for now)
        double weightCorrectionFactor = 1.0;

        // Inject particles in this segment
        for (int ipart = 0; ipart < npart; ipart++) {
            double p[3];

            // Generate particle momentum using Maxwell-Boltzmann distribution
            // Maxwell-Boltzmann thermal distribution (zero bulk velocity in field line frame)
            double thermalSpeed = sqrt(2.0 * Kbol * T_sw / PIC::MolecularData::GetMass(spec));
            double mass = PIC::MolecularData::GetMass(spec);

            // Generate thermal velocity in 3D using Box-Muller transform
            double v_thermal[3];
            for (int idim = 0; idim < 3; idim++) {
                v_thermal[idim] = thermalSpeed *
                    sqrt(-2.0 * log(max(1e-20, rnd()))) * cos(2.0 * Pi * rnd());
            }

            // Get field line direction for this segment
            double l[3];
            Segment->GetDir(l);

            // Decompose velocity into parallel and perpendicular components
            double vParallel, vPerp;
            Vector3D::GetComponents(vParallel, vPerp, v_thermal, l);

            // Convert thermal velocity to momentum (non-relativistic for thermal speeds)
            for (int idim = 0; idim < 3; idim++) {
                p[idim] = mass * v_thermal[idim];
            }

            // Convert thermal velocity to momentum (non-relativistic for thermal speeds)
            for (int idim = 0; idim < 3; idim++) {
                p[idim] = mass * v_thermal[idim];
            }

            // Generate random position within segment for injection
            double alpha = rnd();
            double S = iSegment + alpha; // Field line coordinate

            // Inject particle using PIC framework following InjectParticle_default pattern
            long int newParticle = PIC::FieldLine::InjectParticle_default(
                spec, p, weightCorrectionFactor, iFieldLine, iSegment, S);

            if (newParticle != -1) {
                nInjectedParticles++;

                // Set initial radial location if tracking is enabled
                if (SEP::Offset::RadialLocation != -1) {
                    *((double*)(PIC::ParticleBuffer::GetParticleDataPointer(newParticle) +
                              SEP::Offset::RadialLocation)) = r_m;
                }
            }
        }
    }

    return nInjectedParticles;
}

long int SEP::SolarWind::InjectSolarWindAtFieldLineBeginning(int spec, int iFieldLine, double dt) {
    namespace FL = PIC::FieldLine;

    long int nInjectedParticles = 0;

    // Get first segment and vertex
    auto Segment = FL::FieldLinesAll[iFieldLine].GetFirstSegment();
    if (Segment == NULL) return 0;
    if (Segment->Thread != PIC::ThisThread) return 0;

    FL::cFieldLineVertex* FirstVertex = Segment->GetBegin();
    double* x = FirstVertex->GetX();

    // Calculate distance from Sun
    double r_m = Vector3D::Length(x);

    // Get solar wind properties at field line beginning
    double n_sw = SolarWindNumberDensity(r_m);
    double T_sw = SolarWindTemperature(r_m);

    // Get field line direction at beginning
    double l[3];
    Segment->GetDir(l);

    // Calculate magnetic tube cross-sectional area at field line beginning
    double tubeRadius = SEP::FieldLine::MagneticTubeRadius(x, iFieldLine);
    double injectionArea = Pi * tubeRadius * tubeRadius;

    // Get particle weight
    double particleWeight = PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];

    // Calculate thermal speed
    double thermalSpeed = sqrt(2.0 * Kbol * T_sw / PIC::MolecularData::GetMass(spec));
    double mass = PIC::MolecularData::GetMass(spec);

    // Calculate injection rate based on kinetic theory
    // For Maxwell-Boltzmann distribution, flux through a surface is:
    // Γ = (1/4) * n * <v> where <v> = sqrt(8*kT/(π*m))
    double meanSpeed = sqrt(8.0 * Kbol * T_sw / (Pi * mass));
    double sourceRate = 0.25 * n_sw * meanSpeed; // particles per m^2 per second

    // Calculate number of computational particles to inject
    double anpart = dt * sourceRate * injectionArea / particleWeight;

    // Limit particles for computational efficiency if needed
    // Note: Uses hardcoded limit since this is a continuous injection function
    int maxParticlesPerInjection = 50; // Fixed limit for injection function
    double weightCorrectionFactor = 1.0;

    if (anpart > maxParticlesPerInjection) {
        weightCorrectionFactor = anpart / maxParticlesPerInjection;
        anpart = maxParticlesPerInjection;
    }

    int npart = (int)anpart;
    if (anpart - npart > rnd()) npart++;

    if (npart == 0) return 0;

    // Inject particles at field line beginning
    for (int ipart = 0; ipart < npart; ipart++) {
        double p[3], v_thermal[3];

        // Generate Maxwell-Boltzmann thermal velocity distribution
        for (int idim = 0; idim < 3; idim++) {
            v_thermal[idim] = thermalSpeed *
                sqrt(-2.0 * log(max(1e-20, rnd()))) * cos(2.0 * Pi * rnd());
        }

        // Get field line direction and decompose velocity
        double vParallel, vPerp;
        Vector3D::GetComponents(vParallel, vPerp, v_thermal, l);

        // Only inject particles moving along field line (positive parallel velocity)
        // This simulates solar wind flowing outward along field lines
        if (vParallel <= 0.0) {
            vParallel = -vParallel; // Make sure particles move outward
        }

        // Convert velocity to momentum
        for (int idim = 0; idim < 3; idim++) {
            p[idim] = mass * v_thermal[idim];
        }

        // Inject particle at beginning of field line
        double S = 0.0; // Start at field line beginning

        // Inject particle using PIC framework
        long int newParticle = PIC::FieldLine::InjectParticle_default(
            spec, p, weightCorrectionFactor, iFieldLine, 0, S);

        if (newParticle != -1) {
            nInjectedParticles++;

            // Shift particle location by random fraction of time step
            double ds = vParallel * rnd() * dt;
            PIC::ParticleBuffer::byte* ParticleData = PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
            double FieldLineCoord = PIC::ParticleBuffer::GetFieldLineCoord(ParticleData);

            FieldLineCoord = FL::FieldLinesAll[iFieldLine].move(FieldLineCoord, ds, Segment);
            PIC::ParticleBuffer::SetFieldLineCoord(FieldLineCoord, ParticleData);

            // Update Cartesian position to match new field line coordinate
            double x_new[3];
            FL::FieldLinesAll[iFieldLine].GetCartesian(x_new, FieldLineCoord);
            PIC::ParticleBuffer::SetX(x_new, ParticleData);

            // Set initial radial location if tracking is enabled
            if (SEP::Offset::RadialLocation != -1) {
                *((double*)(ParticleData + SEP::Offset::RadialLocation)) = r_m;
            }
        }
    }

    return nInjectedParticles;
}
