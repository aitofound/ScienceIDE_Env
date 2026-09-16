#include "ComputedQuantities.hh"

#include <G4Element.hh>
#include <G4Exception.hh>
#include <G4IonisParamMat.hh>
#include <G4Material.hh>
#include <G4PhysicalConstants.hh>
#include <G4SystemOfUnits.hh>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

namespace {

std::string NormalizedToken(const std::string& in){
  // Case- and punctuation-tolerant token used by --quantities.  For example,
  // "n_eq", "N-EQ", and "neq" all map to "neq".
  std::string out;
  for(unsigned char ch: in)
    if(std::isalnum(ch)) out.push_back(static_cast<char>(std::tolower(ch)));
  return out;
}

bool ContainsToken(const G4String& name,const std::string& token){
  std::string n=name;
  std::transform(n.begin(),n.end(),n.begin(),[](unsigned char c){return std::tolower(c);});
  return n.find(token)!=std::string::npos;
}

G4double EffectiveZoverA_mol_per_g(const G4Material* material){
  // Bethe-Bloch uses the effective Z/A of the absorber.  For a compound or
  // mixture, the mass-fraction average is
  //
  //   (Z/A)_eff = sum_i w_i Z_i / A_i ,
  //
  // where w_i is the mass fraction and A_i is in g/mol.  Geant4 stores element
  // fractions for materials; for materials built with atom counts, Geant4 has
  // already converted those into mass fractions internally.
  if(!material) return 0.5;

  const auto* elems = material->GetElementVector();
  const auto* frac  = material->GetFractionVector();
  const std::size_t n = material->GetNumberOfElements();

  G4double zOverA = 0.0;
  for(std::size_t i=0;i<n;++i){
    const G4Element* el = (*elems)[i];
    const G4double A_g_mol = el->GetA()/(g/mole);
    if(A_g_mol>0.0) zOverA += frac[i]*el->GetZ()/A_g_mol;
  }

  // A very defensive fallback for unusual user-defined materials.
  if(zOverA<=0.0 || !std::isfinite(zOverA)) zOverA=0.5;
  return zOverA;
}

G4double BetheMassStoppingPower_MeV_cm2_g(G4double kineticEnergyMeV,
                                          G4double projectileMassMeV,
                                          G4double projectileCharge,
                                          const G4Material* material)
{
  // Self-contained Bethe-Bloch electronic stopping approximation.
  //
  //   S/rho = K z^2 (Z/A) (1/beta^2)
  //           [ 1/2 ln(2 m_e c^2 beta^2 gamma^2 T_max / I^2) - beta^2 ]
  //
  // with K = 0.307075 MeV cm^2/mol.  When multiplied by Z/A in mol/g, the
  // result is MeV cm^2/g.  Density-effect, shell, Barkas, charge-state, and
  // low-energy nuclear stopping corrections are intentionally omitted here;
  // therefore this helper should be treated as an LET-bin assignment estimate,
  // not as a replacement for NIST PSTAR/ASTAR or Geant4 EM stopping tables.
  if(!material || kineticEnergyMeV<=0.0) return 0.0;

  const G4double me = 0.510998950; // electron rest energy [MeV]
  const G4double K  = 0.307075;    // Bethe constant [MeV cm^2/mol]

  const G4double gamma = 1.0 + kineticEnergyMeV/projectileMassMeV;
  const G4double beta2 = std::max(1.0e-8,1.0 - 1.0/(gamma*gamma));
  const G4double massRatio = me/projectileMassMeV;
  const G4double tmax = (2.0*me*beta2*gamma*gamma) /
                        (1.0 + 2.0*gamma*massRatio + massRatio*massRatio);

  // Use the material mean excitation energy stored by Geant4 when available.
  // Fallback is 75 eV, a common water/tissue scale, converted to MeV.
  G4double I_MeV = 75.0e-6;
  if(material->GetIonisation()){
    const G4double I = material->GetIonisation()->GetMeanExcitationEnergy()/MeV;
    if(I>0.0 && std::isfinite(I)) I_MeV=I;
  }

  G4double argument = (2.0*me*beta2*gamma*gamma*tmax)/(I_MeV*I_MeV);
  if(argument<=1.0) argument=1.0 + 1.0e-12;

  G4double bracket = 0.5*std::log(argument) - beta2;
  if(bracket<0.0) bracket=0.0;

  return K * projectileCharge*projectileCharge
           * EffectiveZoverA_mol_per_g(material)
           * bracket / beta2;
}

G4double ReferenceByMaterialName(const G4Material* material){
  // Reference 1-MeV neutron NIEL values in MeV cm^2/g.
  //
  // These values are deliberately kept in one compact dispatch function so they
  // can be replaced by a vetted material-response table later.  The Si value is
  // chosen on the common ASTM-E722 / semiconductor-displacement-damage scale.
  // Values for non-Si materials are order-of-magnitude engineering proxies.  A
  // final electronics analysis should use material-specific NIEL/displacement
  // damage functions, threshold displacement energies, and device-response data.
  if(!material) return 2.0e-3;
  const G4String n = material->GetName();

  if(ContainsToken(n,"silicondioxide") || ContainsToken(n,"sio2")) return 1.8e-3;
  if(ContainsToken(n,"sic"))                                      return 1.6e-3;
  if(ContainsToken(n,"gaas"))                                     return 2.6e-3;
  if(ContainsToken(n,"ingaas"))                                   return 2.2e-3;
  if(ContainsToken(n,"g4_ge") || ContainsToken(n,"germanium"))    return 3.0e-3;
  if(ContainsToken(n,"g4_si") || ContainsToken(n,"silicon"))      return 2.0e-3;

  // Tissue/water values are not normally reported as semiconductor n_eq; they
  // are included only to keep the output defined for all target materials.
  if(ContainsToken(n,"water") || ContainsToken(n,"tissue") ||
     ContainsToken(n,"skin")  || ContainsToken(n,"lens")  ||
     ContainsToken(n,"cns")   || ContainsToken(n,"bfo"))          return 1.0e-3;

  return 2.0e-3;
}

G4double HardnessShape(const G4String& particleName,G4double E){
  // Dimensionless surrogate for the energy dependence of the damage function.
  // It is normalized so that 1-MeV neutrons have hardness 1.  Protons and alpha
  // particles are assigned stronger low-energy damage effectiveness and a mild
  // high-energy decrease; alpha particles additionally scale roughly with z^2.
  // This is a placeholder for real NIEL(E) tables.
  const G4double x = std::max(1.0e-3,E);
  if(particleName=="neutron"){
    return std::max(0.05,std::min(10.0,std::pow(x,0.20)));
  }
  if(particleName=="alpha"){
    const G4double ePerNuc = std::max(1.0e-3,x/4.0);
    return 4.0*(0.35 + 1.60/std::sqrt(ePerNuc+1.0));
  }

  // Default charged-hadron branch, used for protons.
  return 0.35 + 1.60/std::sqrt(x+1.0);
}

} // namespace

namespace ComputedQuantities {

Selection ParseSelection(const std::string& text){
  Selection s;
  if(text.empty() || NormalizedToken(text)=="all") return s;

  s.tid=s.ddd=s.neq=s.let=s.hardness=false;

  std::stringstream ss(text);
  std::string item;
  while(std::getline(ss,item,',')){
    const std::string t = NormalizedToken(item);
    if(t.empty()) continue;
    if(t=="all"){
      s.tid=s.ddd=s.neq=s.let=s.hardness=true;
    } else if(t=="none"){
      s.tid=s.ddd=s.neq=s.let=s.hardness=false;
    } else if(t=="tid" || t=="totalionizingdose"){
      s.tid=true;
    } else if(t=="ddd" || t=="displacementdamagedose"){
      s.ddd=true;
    } else if(t=="neq" || t=="nequivalent" || t=="neutronequivalent" ||
              t=="1mevneutronequivalent" || t=="n1mev"){
      s.neq=true;
    } else if(t=="let" || t=="letspectrum"){
      s.let=true;
    } else if(t=="hardness" || t=="h10010" || t=="h100over10"){
      s.hardness=true;
    } else {
      const std::string msg = "Unknown computed quantity token: '" + item +
                              "'. Use --list-quantities for allowed values.";
      G4Exception("ComputedQuantities::ParseSelection","BadQuantity",FatalException,msg.c_str());
    }
  }
  return s;
}

std::string SelectionSummary(const Selection& s){
  std::vector<std::string> names;
  if(s.tid)      names.push_back("TID");
  if(s.ddd)      names.push_back("DDD");
  if(s.neq)      names.push_back("n_eq");
  if(s.let)      names.push_back("LET");
  if(s.hardness) names.push_back("H100/10");
  if(names.empty()) return "none";
  std::ostringstream out;
  for(std::size_t i=0;i<names.size();++i){
    if(i) out<<",";
    out<<names[i];
  }
  return out.str();
}

std::string QuantityCatalogText(){
  std::ostringstream out;
  out<<"\n  Computed quantities\n";
  out<<"  -------------------\n";
  out<<"    TID       Total ionizing dose from Geant4 Edep/mass. Output: Gy/primary, rad/primary, Gy/s, rad/s.\n";
  out<<"    DDD       Displacement damage dose proxy: D_d = integral Phi(E) NIEL(E) dE. Output: MeV/g and rad-equivalent.\n";
  out<<"    n_eq      1-MeV neutron-equivalent fluence proxy: n_eq = DDD / NIEL_1MeV_neutron. Output: cm^-2.\n";
  out<<"    LET       LET spectrum of transmitted protons/alphas binned in MeV cm^2/mg using a Bethe-Bloch estimate.\n";
  out<<"    H100/10   Spectral hardness index J(>100 MeV)/J(>10 MeV) for transmitted p+alpha+n.\n";
  out<<"\n  Selection syntax\n";
  out<<"  ----------------\n";
  out<<"    --quantities=all                         compute all quantities (default)\n";
  out<<"    --quantities=TID,DDD,n_eq                compute selected scalar quantities\n";
  out<<"    --quantities=TID,LET,H100/10             include LET spectrum and hardness index\n";
  out<<"    --quantities=none                        disable post-processed quantity output\n";
  out<<"\n  Physics assumptions\n";
  out<<"  -------------------\n";
  out<<"    TID is scored directly by Geant4 in each target slab. DDD and n_eq use a documented analytic NIEL surrogate; replace\n";
  out<<"    ComputedQuantities::NIEL_MeV_cm2_g() with tabulated SR-NIEL/NIEL data for quantitative device work. LET uses a\n";
  out<<"    self-contained Bethe-Bloch mass-stopping-power approximation for binning; replace with NIST/Geant4 stopping tables\n";
  out<<"    if high-accuracy LET spectra are required.\n";
  return out.str();
}

G4double MeVPerGramToRad(G4double mevPerGram){
  // 1 rad = 100 erg/g.  1 MeV = 1.602176634e-6 erg, therefore
  // 1 rad = 100 / 1.602176634e-6 = 6.241509074e7 MeV/g.
  static const G4double MeV_per_g_per_rad = 6.241509074e7;
  return mevPerGram / MeV_per_g_per_rad;
}

G4double ChargedLET_MeV_cm2_mg(const G4String& particleName,
                               G4double kineticEnergyMeV,
                               const G4Material* material)
{
  if(particleName=="neutron") return 0.0;
  if(particleName=="alpha"){
    const G4double S = BetheMassStoppingPower_MeV_cm2_g(kineticEnergyMeV,3727.379,2.0,material);
    return S/1000.0;
  }
  // Default charged-hadron branch.  The only primary charged hadron currently
  // generated is the proton; this also handles secondary protons.
  const G4double S = BetheMassStoppingPower_MeV_cm2_g(kineticEnergyMeV,938.272,1.0,material);
  return S/1000.0;
}

G4double NIEL_MeV_cm2_g(const G4String& particleName,
                        G4double kineticEnergyMeV,
                        const G4Material* material)
{
  const G4double ref = ReferenceNIEL1MeVNeutron_MeV_cm2_g(material);
  return ref * HardnessShape(particleName,kineticEnergyMeV);
}

G4double ReferenceNIEL1MeVNeutron_MeV_cm2_g(const G4Material* material){
  return ReferenceByMaterialName(material);
}

namespace LETBins {
  G4double Center(G4int i){ return Lmin*std::exp((i+0.5)/G4double(N)*std::log(Lmax/Lmin)); }
  G4double Edge  (G4int i){ return Lmin*std::exp(i/G4double(N)*std::log(Lmax/Lmin)); }
  G4double Width (G4int i){ return Edge(i+1)-Edge(i); }
  G4int Bin(G4double L){
    if(L<=Lmin) return 0;
    if(L>=Lmax) return N-1;
    return static_cast<G4int>(std::log(L/Lmin)/std::log(Lmax/Lmin)*N);
  }
}

} // namespace ComputedQuantities
