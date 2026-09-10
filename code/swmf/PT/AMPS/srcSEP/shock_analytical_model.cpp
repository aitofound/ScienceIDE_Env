#include "sep.h"
//analytic model of a shock wave (Tenishev-2005-AIAA-4928

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
#include "../srcInterface/amps2swmf.h"
#endif

double SEP::ParticleSource::ShockWave::Tenishev2005::rShock =1.0E-5*_SUN__RADIUS_;
bool SEP::ParticleSource::ShockWave::Tenishev2005::InitFlag=false;
double SEP::ParticleSource::ShockWave::Tenishev2005::MinFieldLineHeliocentricDistance=-1.0;  


void SEP::ParticleSource::ShockWave::Tenishev2005::Init() {
  InitFlag=true;

  //determine the  initial location of the shock that is the minimum helpocentric distance of the beginning of the simulated field lines  
  //loop through all simulated field lines
  for (int i = 0; i < PIC::FieldLine::nFieldLine; i++) {
    double r, *x;

    x = PIC::FieldLine::FieldLinesAll[i].GetFirstSegment()->GetBegin()->GetX();
    r = Vector3D::Length(x);

    if ((MinFieldLineHeliocentricDistance < 0.0) || (MinFieldLineHeliocentricDistance > r))
      MinFieldLineHeliocentricDistance = r;
  }

  rShock=MinFieldLineHeliocentricDistance;

  #if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  if (_PIC_FIELD_LINE_MODE_==_PIC_MODE_OFF_) { 
    rShock=AMPS2SWMF::Heliosphere::rMin;
  }
  #endif 
}
  
double SEP::ParticleSource::ShockWave::Tenishev2005::GetCompressionRatio() {
  double r = rShock / _AU_;
  double res;

  if (r<0.04) {
    res=1.7;
  }
  else {
    res=2.0+(1.4-2.0)/(0.14-0.04)*(r-0.04);
    if (res<1.0) res=1.0;
  } 

  return res;
}

double SEP::ParticleSource::ShockWave::Tenishev2005::GetShockSpeed() {
  double r = rShock / _AU_;
  double res;

  if (SEP::ShockModelType==SEP::cShockModelType::SwCme1d) {
    return SEP::SW1DAdapter::gState.V_sh_ms;
  }


  if (r < 0.1)
    res = 1800.0;
  else if (r < 0.15)
    res = 1800.0 + (1500.0 - 1800.0) / (0.15 - 0.1) * (r - 0.1);
  else if (r < 0.3)
    res = 1500.0;
  else if (r < 0.5)
    res = 1500.0 + (1100.0 - 1500.0) / (0.5 - 0.1) * (r - 0.3);
  else if (r < 1.3)
    res = 1100 + (900.0 - 1100.0) / (1.3 - 0.5) * (r - 0.5);
  else
    res = 900.0;

  return res*1.0E3;
}

void SEP::ParticleSource::ShockWave::Tenishev2005::UpdateShockLocation() {
  static double simulation_time_last = 0.0;

  if (InitFlag==false) Init();

  if (simulation_time_last != PIC::SimulationTime::TimeCounter) {
    double speed = GetShockSpeed();
    rShock += speed * (PIC::SimulationTime::TimeCounter - simulation_time_last);
    simulation_time_last=PIC::SimulationTime::TimeCounter;
  }
}

double SEP::ParticleSource::ShockWave::Tenishev2005::GetSolarWindDensity() {
  double t=_AU_/rShock;
  return 5.0E6*t*t;
}

double SEP::ParticleSource::ShockWave::Tenishev2005::GetInjectionRate() {
  double r_sh,s,density,efficientcy,res;
  double n_m3, V_ms, divV;

  if (InitFlag==false) Init(); 

  switch (SEP::ShockModelType) {
  case SEP::cShockModelType::Analytic1D: 
    s=SEP::ParticleSource::ShockWave::Tenishev2005::GetCompressionRatio();
    density=GetSolarWindDensity();
    break;
  case SEP::cShockModelType::SwCme1d:
    s=SEP::SW1DAdapter::gState.rc;
    r_sh=SEP::SW1DAdapter::gState.r_sh_m;  

    if (SEP::SW1DAdapter::QueryAtRadius(r_sh, n_m3, V_ms, divV, /*applyClamp=*/true)) {
      density=n_m3;
    }
    else {
      density=0.0;
    }

    break;
  default:
    exit(__LINE__,__FILE__,"Error: the case is not known");
  }



  efficientcy=(s-1.0)/s;
  res=density*efficientcy;

  return res;
}

int SEP::ParticleSource::ShockWave::Tenishev2005::GetInjectionLocation(int iFieldLine,double &S,double *xInjection) {
  double r, *x, r_sh,r2 = rShock * rShock;
  int iSegment;
  PIC::FieldLine::cFieldLineSegment *Segment =
      PIC::FieldLine::FieldLinesAll[iFieldLine].GetFirstSegment();

  if (SEP::ShockModelType==SEP::cShockModelType::SwCme1d) {
    r_sh=SEP::SW1DAdapter::gState.r_sh_m;
    r2=r_sh*r_sh;
  }

  UpdateShockLocation();
  x = Segment->GetBegin()->GetX();

  S = -1.0;
  if (r2 <= Vector3D::DotProduct(x, x)) return -1;

  for (iSegment = 0; Segment != NULL; iSegment++, Segment = Segment->GetNext()) {
    double *x1 = Segment->GetBegin()->GetX();
    double r1_2 = Vector3D::DotProduct(x1, x1);

    if (r1_2 < r2) {
        double *x2 = Segment->GetEnd()->GetX();
        double r2_2 = Vector3D::DotProduct(x2, x2);

        if (r2_2 >= r2) {
            // Solve quadratic equation |x1 + t(x2-x1)|^2 = r2
            double d[3];
            double a = 0.0, b = 0.0, c = -r2;

            for (int i = 0; i < 3; i++) {
                d[i] = x2[i] - x1[i];
                a += d[i] * d[i];           // |d|^2
                b += 2.0 * x1[i] * d[i];    // 2(x1·d)
                c += x1[i] * x1[i];         // |x1|^2
            }

            // Solve at^2 + bt + c = 0
            double discriminant = b*b - 4*a*c;

            if (discriminant < 0) {
                // Use segment start point since it's inside shock
                for (int i = 0; i < 3; i++) {
                   xInjection[i] = x1[i];
                }

		S=iSegment; 
                return iSegment;
            }

            // Two real solutions
            double t1 = (-b - sqrt(discriminant))/(2*a);
            double t2 = (-b + sqrt(discriminant))/(2*a);

            // Find valid t
            if (t1 >= 0 && t1 <= 1) {  // t1 is in [0,1]
                for (int i = 0; i < 3; i++) {
                    xInjection[i] = x1[i] + t1 * d[i];
                }

		S=t1+iSegment; 
                return iSegment;
            }
            else if (t2 >= 0 && t2 <= 1) {  // t2 is in [0,1]
                for (int i = 0; i < 3; i++) {
                    xInjection[i] = x1[i] + t2 * d[i];
                }

		S=t2+iSegment; 
                return iSegment;
            }
            else if (t1 < 0 && t2 > 1) {  // Solutions bracket the segment
                for (int i = 0; i < 3; i++) {
                    xInjection[i] = x1[i];
                }

		S=iSegment;
                return iSegment;
            }
        }
    }
}


return -1;
}


namespace SEP {
namespace ParticleSource {
namespace ShockWave {

// Function to increment integrated wave energy due to shock passing
// r0, r1: initial and final heliocentric distances of the shock
// dt: time needed for shock to move from r0 to r1
void ShockTurbulenceEnergyInjection(double r0, double r1, double dt) {
    using namespace PIC::FieldLine;
    
    // Validate input parameters
    if (r1 <= r0 || r0 < 0.0 || dt <= 0.0) {
        return; // Invalid shock propagation or time
    }
    
    // Calculate shock velocity from distance and time
    double v = (r1 - r0) / dt; // Shock speed [m/s]
    
    // Physical parameters for turbulence generation
    const double eta = 0.02;   // Fraction of upstream kinetic-energy flux converted to broad-band Alfvén waves at the shock ramp 
    const double f=0.5;        // Fraction of the injected power that populates the outward-propagating (+) wave sense; (1−f) goes into the inward (−) sense. 
    double shock_velocity_squared = v * v; // Shock velocity²
    
    // Lambda function to calculate magnetic tube volume for a segment portion
    auto calculateTubeVolume = [](cFieldLineSegment* segment, double sStart, double sEnd, int iFieldLine) -> double {
        // Get segment length
        double segmentLength = segment->GetLength();
        double affectedLength = (sEnd - sStart) * segmentLength;
        
        // Get 3D positions at start and end
        double xStart[3], xEnd[3];
        segment->GetCartesian(xStart, sStart);
        segment->GetCartesian(xEnd, sEnd);
        
        // Get magnetic tube radii at start and end positions
        double radiusStart = SEP::FieldLine::MagneticTubeRadius(xStart, iFieldLine);
        double radiusEnd = SEP::FieldLine::MagneticTubeRadius(xEnd, iFieldLine);
        
        // Calculate volume using truncated cone formula: V = (π/3) * h * (r1² + r1*r2 + r2²)
        return (M_PI / 3.0) * affectedLength * 
               (radiusStart*radiusStart + radiusStart*radiusEnd + radiusEnd*radiusEnd);
    };
    
    // Lambda function to get average mass density for a segment portion
    auto getAverageMassDensity = [](cFieldLineSegment* segment, double sStart, double sEnd) -> double {
        // Get plasma number density at segment endpoints
        double densityStart, densityEnd;
        segment->GetPlasmaDensity(sStart, densityStart);  // Number density
        segment->GetPlasmaDensity(sEnd, densityEnd);      // Number density
        
        // Calculate average number density and convert to mass density
        double avgNumberDensity = 0.5 * (densityStart + densityEnd);
        return avgNumberDensity * ProtonMass; // kg/m³
    };
    
    // Loop through all field lines
    for (int iFieldLine = 0; iFieldLine < nFieldLine; iFieldLine++) {
        PIC::FieldLine::cFieldLine* fieldLine = &FieldLinesAll[iFieldLine];
        
        // Get the position of the first vertex to calculate datum vector
        PIC::FieldLine::cFieldLineVertex* firstVertex = fieldLine->GetFirstVertex();
        if (firstVertex == NULL) {
            continue;
        }
        
        double firstVertexPos[3];
        firstVertex->GetX(firstVertexPos);
        
        // Calculate heliocentric distance of the first vertex (datum)
        double datumDistance = sqrt(firstVertexPos[0]*firstVertexPos[0] + 
                                   firstVertexPos[1]*firstVertexPos[1] + 
                                   firstVertexPos[2]*firstVertexPos[2]);
        
        // Convert heliocentric distances to distances along field line
        // by subtracting the datum distance
        double distance0 = r0 - datumDistance;
        double distance1 = r1 - datumDistance;
        
        // Convert distances to field line coordinates using GetS method
        double s0 = fieldLine->GetS(distance0);
        double s1 = fieldLine->GetS(distance1);
        
        // Skip if conversion failed or invalid coordinates
        if (s0 < 0.0 || s1 < 0.0 || s0 >= s1) {
            continue;
        }
        
        // Process segments between s0 and s1
        int startSegment = static_cast<int>(floor(s0));
        int endSegment = static_cast<int>(floor(s1));
        
        for (int iSegment = startSegment; iSegment <= endSegment && 
             iSegment < fieldLine->GetTotalSegmentNumber(); iSegment++) {
            
            cFieldLineSegment* segment = fieldLine->GetSegment(iSegment);
            if (segment == NULL) continue;
            
            // Calculate segment boundaries affected by shock
            double segmentStart = max(s0, static_cast<double>(iSegment));
            double segmentEnd = min(s1, static_cast<double>(iSegment + 1));
            
            if (segmentEnd <= segmentStart) continue;
            
            // Convert to local segment coordinates [0,1]
            double sStart = segmentStart - static_cast<double>(iSegment);
            double sEnd = segmentEnd - static_cast<double>(iSegment);
            
            // Calculate tube volume and mass density using lambda functions
            double tubeVolume = calculateTubeVolume(segment, sStart, sEnd, iFieldLine);
            double massDensity = getAverageMassDensity(segment, sStart, sEnd);
            
            // Calculate deposited turbulence energy
            // ΔE± = 0.25η_f± ρu² V_tube (using mass density and provided shock speed v)
            double deltaE_forward = 0.25 * eta * f * massDensity * 
                                   shock_velocity_squared * tubeVolume;
            double deltaE_backward = 0.25 * eta * (1.0-f) * massDensity * 
                                    shock_velocity_squared * tubeVolume;
            
            // Save energy directly to the segment using datum pointer
            double* wave_data = segment->GetDatum_ptr(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
            if (wave_data != NULL) {
                wave_data[0] += deltaE_forward;   // E_plus (forward waves)
                wave_data[1] += deltaE_backward;  // E_minus (backward waves)
            }
        }
    }
}

} // namespace ShockWave
} // namespace ParticleSource
} // namespace SEP
