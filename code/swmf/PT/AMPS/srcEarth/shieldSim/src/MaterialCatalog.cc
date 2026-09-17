#include "MaterialCatalog.hh"

/* ============================================================================
 * MaterialCatalog.cc
 *
 * Material implementation and reference notes
 * -------------------------------------------
 * This file is the single place where shieldSim maps user-facing material names
 * to Geant4 materials.  The logic is deliberately centralized because material
 * names are used by several otherwise independent parts of the code:
 *
 *   - CLI parsing and --help/--list-materials output,
 *   - detector construction,
 *   - dose-vs-thickness sweep mode,
 *   - configuration printouts and Tecplot metadata.
 *
 * Material categories
 * -------------------
 * 1. Geant4/NIST aliases
 *      Entries such as Al, Cu, W, Ta, Kapton, and Water map directly to G4_*
 *      material names.  Their density/composition is therefore the value in the
 *      Geant4 material database used by the installed Geant4 version.
 *
 * 2. ShieldSim custom engineering approximations
 *      Entries such as HDPE, BPE, Kevlar, CFRP, SiCComposite, LunarRegolith,
 *      and MarsRegolith are built explicitly below.  They are intended for
 *      preliminary shielding trade studies.  The comments above each builder
 *      state whether the composition is defined by atom count or by mass
 *      fraction and give the reference assumptions.
 *
 * 3. Detector/absorber/target scoring materials
 *      Entries such as Skin, EyeLens, BFO, CNS, SoftTissue, Si, SiO2, SiC,
 *      GaAs, InGaAs, Ge, and H2O are intended for downstream scoring slabs.
 *      They represent either tissue-equivalent media for crewed-mission dose
 *      proxies or electronics/semiconductor media for detector/instrument
 *      response proxies.  These materials are selected through --scoring or
 *      the equivalent --target option.
 *
 * References used for the default catalog
 * ---------------------------------------
 * The help text prints compact reference notes.  The longer URLs are listed
 * here so the material-property provenance stays with the source code.
 *
 * [G4-MAT]  Geant4 Book for Application Developers, Appendix: Geant4 Material
 *           Database, material names and predefined G4_* materials:
 *           https://geant4.web.cern.ch/documentation/dev/bfad_html/
 *           ForApplicationDevelopers/Appendix/materialNames.html
 *
 * [NIST-XCOM] NIST XCOM / X-Ray Mass Attenuation Coefficients, material
 *           constants and elemental densities used in photon-interaction
 *           evaluations:
 *           https://physics.nist.gov/PhysRefData/XrayMassCoef/tab1.html
 *
 * [NIST-KAPTON] NIST STAR material composition for Kapton polyimide film,
 *           density 1.420 g/cm3 and H/C/N/O mass fractions:
 *           https://physics.nist.gov/cgi-bin/Star/compos.pl?matno=179
 *
 * [G4-PE]   Geant4/NIST material database includes G4_POLYETHYLENE with a
 *           polyethylene formula and density near 0.94 g/cm3.  ShieldSim uses
 *           0.95 g/cm3 for the HDPE approximation unless changed below.
 *
 * [FSRI-HDPE] Fire Safety Research Institute material database, measured HDPE
 *           sheet density near 971 kg/m3.  This is used as a sanity check for
 *           the HDPE-density scale, not as a unique mission-material standard:
 *           https://materials.fsri.org/materialdetail/high-density-polyethylene-hdpe
 *
 * [DUPONT-KEVLAR] DuPont Kevlar Aramid Fiber Technical Guide, typical Kevlar
 *           fiber density 1.44 g/cm3.  The polymer repeat unit is represented
 *           here as C14H10N2O2 for transport-element composition.
 *
 * [SIC]     PubChem / engineering handbooks give SiC density about 3.21 g/cm3.
 *           ShieldSim uses stoichiometric SiC for the ceramic filler phase.
 *
 * [LUNAR-SOURCEBOOK] Lunar Sourcebook chapters on lunar regolith composition
 *           and physical properties; lunar soils vary substantially with site,
 *           maturity, compaction, and grain-size distribution:
 *           https://www.lpi.usra.edu/publications/books/lunar_sourcebook/
 *
 * [MARS-APXS] Mars Pathfinder / MER / Curiosity APXS literature gives in-situ
 *           bulk chemistry of Martian soils and rocks.  ShieldSim's default is
 *           only a rough basaltic/regolith elemental mixture.
 *
 * [ICRU-44] ICRU Report 44, Tissue Substitutes in Radiation Dosimetry and
 *           Measurement.  Used here for the 4-element soft-tissue proxy
 *           convention documented in the builder comments.
 *
 * [ICRP-110] ICRP Publication 110 adult reference computational phantoms and
 *           reference tissue context.  Used here only as a provenance note for
 *           organ/tissue proxy naming; the default shieldSim organ materials
 *           are simplified transport proxies, not full phantom models.
 *
 * [NASA-STD-3001] NASA Space Flight Human-System Standard, Volume 1, crew
 *           health dose-limit context for skin, lens, BFO, and CNS endpoints.
 *           ShieldSim stores material definitions only; biological weighting
 *           and dose-equivalent conversion must be applied separately.
 *
 * [NIST-PSTAR] NIST PSTAR stopping-power tables.  Used as the provenance note
 *           for silicon/electronics stopping-power use cases.
 *
 * [SEMICONDUCTOR] Standard semiconductor material-property references for
 *           Si, SiO2, SiC, GaAs, InGaAs, and Ge densities/stoichiometry.
 *           Compound-semiconductor entries are practical transport proxies and
 *           should be replaced with device-specific compositions if needed.
 *
 * How to add a new material
 * -------------------------
 * See the long comment in include/MaterialCatalog.hh.  In short:
 *   - add an entry in ShieldMaterialCatalog();
 *   - if it is a G4_* material, no builder is needed;
 *   - if it is a new custom material, implement BuildNewMaterial() and register
 *     it in BuildCustomMaterial();
 *   - keep comments, density units, fraction conventions, and references with
 *     the builder so future users can audit the model.
 * ========================================================================== */

#include <G4Element.hh>
#include <G4Exception.hh>
#include <G4Material.hh>
#include <G4NistManager.hh>
#include <G4SystemOfUnits.hh>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>

namespace {

std::string NormalizeToken(const std::string& s){
  // Convert user input to a forgiving lookup token.  The user may type any of
  // these and get the same material:
  //   "HDPE", "high-density polyethylene", "High Density Polyethylene".
  // Only letters and digits are kept; spaces, slashes, hyphens, and underscores
  // are ignored.  The catalog can therefore support user-friendly names without
  // duplicating many exact spellings in the parser.
  std::string out;
  out.reserve(s.size());
  for(unsigned char ch : s){
    if(std::isalnum(ch)) out.push_back(static_cast<char>(std::tolower(ch)));
  }
  return out;
}

bool StartsWithG4(const std::string& s){
  // Geant4/NIST material names have the conventional prefix G4_.  We pass these
  // through directly so users can select materials that are not in the shorter
  // shieldSim catalog, e.g. G4_Pb, G4_STAINLESS-STEEL, or G4_AIR.
  return s.size()>=3 && s[0]=='G' && s[1]=='4' && s[2]=='_';
}

G4Element* El(const char* symbol){
  // Convenience wrapper for the Geant4/NIST element builder.  Elements are
  // cached internally by Geant4, so repeated calls are cheap and safe.
  return G4NistManager::Instance()->FindOrBuildElement(symbol);
}

G4Material* Existing(const char* name){
  // Return a previously-created material without raising a Geant4 exception if
  // it is absent.  This is essential in sweep mode because the geometry can be
  // rebuilt many times within one executable run.
  return G4Material::GetMaterial(name,false);
}

G4Material* BuildHDPE(){
  const char* name="ShieldSim_HDPE";
  if(auto* m=Existing(name)) return m;

  // High-density polyethylene approximation.
  //   Composition convention: atom counts in the repeat unit, CH2.
  //   Density: 0.95 g/cm3, representative of polyethylene/HDPE shielding
  //            stock and close to the Geant4 G4_POLYETHYLENE and measured HDPE
  //            density scale.  Replace with supplier data when available.
  auto* m = new G4Material(name,0.95*g/cm3,2);
  m->AddElement(El("C"),1);
  m->AddElement(El("H"),2);
  return m;
}

G4Material* BuildBPE(){
  const char* name="ShieldSim_BPE_5pctB";
  if(auto* m=Existing(name)) return m;

  // Borated polyethylene approximation.
  //   Composition convention: mass fractions.
  //   Default: 95 wt% of the HDPE approximation above + 5 wt% natural boron.
  //   Density: engineering placeholder 0.98 g/cm3.  Commercial borated-PE
  //            density depends on boron loading, polymer grade, and processing.
  //            Use measured supplier density for mission-quality analysis.
  auto* hdpe = BuildHDPE();
  auto* m = new G4Material(name,0.98*g/cm3,2);
  m->AddMaterial(hdpe,0.95);
  m->AddElement(El("B"),0.05);
  return m;
}

G4Material* BuildKevlar(){
  const char* name="ShieldSim_Kevlar";
  if(auto* m=Existing(name)) return m;

  // Para-aramid / Kevlar approximation.
  //   Composition convention: atom counts in the polymer repeat unit
  //                           C14 H10 N2 O2.
  //   Density: 1.44 g/cm3, typical Kevlar fiber density from technical guides.
  //   Note: woven Kevlar cloth, resin-impregnated laminates, or aramid composites
  //         can have different effective densities and should be defined as
  //         custom project-specific mixtures if needed.
  auto* m = new G4Material(name,1.44*g/cm3,4);
  m->AddElement(El("C"),14);
  m->AddElement(El("H"),10);
  m->AddElement(El("N"), 2);
  m->AddElement(El("O"), 2);
  return m;
}

G4Material* BuildEpoxy(){
  const char* name="ShieldSim_Epoxy";
  if(auto* m=Existing(name)) return m;

  // Generic epoxy-resin approximation used only as a matrix phase for simple
  // composite examples below.
  //   Composition convention: atom counts in a representative epoxy-like repeat
  //                           unit C21 H25 O5.
  //   Density: 1.20 g/cm3 engineering placeholder.
  //   Replace this with the actual resin formulation if the composite material
  //   is important to a quantitative result.
  auto* m = new G4Material(name,1.20*g/cm3,3);
  m->AddElement(El("C"),21);
  m->AddElement(El("H"),25);
  m->AddElement(El("O"), 5);
  return m;
}

G4Material* BuildCFRP(){
  const char* name="ShieldSim_CFRP";
  if(auto* m=Existing(name)) return m;

  // Carbon fiber / epoxy composite approximation.
  //   Composition convention: mass fractions.
  //   Default: 60 wt% elemental carbon fiber + 40 wt% generic epoxy matrix.
  //   Density: 1.60 g/cm3 engineering placeholder.
  //   This is a first-order shielding trade-study material.  Real CFRP depends
  //   on fiber volume fraction, layup, void fraction, resin, cure, and additives.
  auto* epoxy = BuildEpoxy();
  auto* m = new G4Material(name,1.60*g/cm3,2);
  m->AddElement(El("C"),0.60);
  m->AddMaterial(epoxy,0.40);
  return m;
}

G4Material* BuildSiC(){
  const char* name="ShieldSim_SiC";
  if(auto* m=Existing(name)) return m;

  // Silicon carbide filler/ceramic phase.
  //   Composition convention: atom counts, SiC.
  //   Density: 3.21 g/cm3 for dense SiC.  This helper is not exposed directly in
  //            the CLI; it is used by BuildSiCCompositePlastic().
  auto* m = new G4Material(name,3.21*g/cm3,2);
  m->AddElement(El("Si"),1);
  m->AddElement(El("C"), 1);
  return m;
}

G4Material* BuildSiCCompositePlastic(){
  const char* name="ShieldSim_SiCCompositePlastic";
  if(auto* m=Existing(name)) return m;

  // SiC-filled plastic composite approximation.
  //   Composition convention: mass fractions.
  //   Default: 70 wt% dense SiC + 30 wt% generic epoxy-like polymer matrix.
  //   Density: 2.20 g/cm3 engineering placeholder.
  //   Change both the SiC loading and density for a specific plastic composite.
  auto* sic   = BuildSiC();
  auto* epoxy = BuildEpoxy();
  auto* m = new G4Material(name,2.20*g/cm3,2);
  m->AddMaterial(sic,  0.70);
  m->AddMaterial(epoxy,0.30);
  return m;
}

G4Material* BuildLunarRegolith(){
  const char* name="ShieldSim_LunarRegolith";
  if(auto* m=Existing(name)) return m;

  // Approximate bulk lunar-regolith elemental composition.
  //   Composition convention: mass fractions; values sum to 1.0.
  //   Density: 1.60 g/cm3 representative loose/bulk density for transport trade
  //            studies.  Actual lunar regolith density varies strongly with
  //            depth, compaction, porosity, mare/highland provenance, glass
  //            content, agglutinates, and grain size.
  //   Replace this definition with a sample-specific or simulant-specific
  //   composition whenever the shielding result depends on regolith chemistry.
  auto* m = new G4Material(name,1.60*g/cm3,9);
  m->AddElement(El("O" ),0.430);
  m->AddElement(El("Si"),0.210);
  m->AddElement(El("Al"),0.100);
  m->AddElement(El("Ca"),0.080);
  m->AddElement(El("Mg"),0.060);
  m->AddElement(El("Fe"),0.090);
  m->AddElement(El("Ti"),0.020);
  m->AddElement(El("Na"),0.005);
  m->AddElement(El("K" ),0.005);
  return m;
}

G4Material* BuildMarsRegolith(){
  const char* name="ShieldSim_MarsRegolith";
  if(auto* m=Existing(name)) return m;

  // Approximate bulk Martian-regolith/soil elemental composition.
  //   Composition convention: mass fractions; values sum to 1.0.
  //   Density: 1.50 g/cm3 loose-material placeholder.  Real Martian soils and
  //            rocks vary by site, grain-size distribution, cementation,
  //            hydration/sulfate/chloride content, and compaction state.
  //   Replace this definition with APXS-derived site-specific chemistry or a
  //   measured simulant formulation for quantitative studies.
  auto* m = new G4Material(name,1.50*g/cm3,9);
  m->AddElement(El("O" ),0.440);
  m->AddElement(El("Si"),0.210);
  m->AddElement(El("Fe"),0.140);
  m->AddElement(El("Mg"),0.060);
  m->AddElement(El("Al"),0.050);
  m->AddElement(El("Ca"),0.050);
  m->AddElement(El("S" ),0.030);
  m->AddElement(El("Na"),0.010);
  m->AddElement(El("K" ),0.010);
  return m;
}

G4Material* BuildSoftTissueICRU4(){
  const char* name="ShieldSim_SoftTissue_ICRU4";
  if(auto* m=Existing(name)) return m;

  // Four-element soft-tissue proxy.
  //   Intended use: generic tissue/muscle or water-like organ dose scoring when
  //                 a full anthropomorphic phantom is not being modeled.
  //   Composition convention: mass fractions for a simplified ICRU-style
  //                           4-element tissue substitute, H/C/N/O only.
  //   Density: 1.00 g/cm3.
  //   Important: this is a scoring material, not a biological dose-equivalent
  //              model.  Quality factors, RBE, LET weighting, or organ
  //              weighting must be applied in post-processing if needed.
  auto* m = new G4Material(name,1.00*g/cm3,4);
  m->AddElement(El("H"),0.101);
  m->AddElement(El("C"),0.111);
  m->AddElement(El("N"),0.026);
  m->AddElement(El("O"),0.762);
  return m;
}

G4Material* BuildSkinEpidermis(){
  const char* name="ShieldSim_Skin_Epidermis";
  if(auto* m=Existing(name)) return m;

  // Skin/epidermis dose proxy.
  //   Intended use: shallow skin scoring layer, for example a user-specified
  //                 0.07 mm slab when comparing with skin-depth limits.
  //   Composition convention: mass fractions for an approximate ICRP-like skin
  //                           tissue.  Trace elements are included only at the
  //                           level needed for charged-particle transport.
  //   Density: 1.09 g/cm3.
  //   The NASA skin dose-limit context belongs in output interpretation; this
  //   material definition only controls transport and energy deposition.
  auto* m = new G4Material(name,1.09*g/cm3,9);
  m->AddElement(El("H" ),0.100);
  m->AddElement(El("C" ),0.204);
  m->AddElement(El("N" ),0.042);
  m->AddElement(El("O" ),0.645);
  m->AddElement(El("Na"),0.002);
  m->AddElement(El("P" ),0.001);
  m->AddElement(El("S" ),0.002);
  m->AddElement(El("Cl"),0.003);
  m->AddElement(El("K" ),0.001);
  return m;
}

G4Material* BuildEyeLens(){
  const char* name="ShieldSim_EyeLens";
  if(auto* m=Existing(name)) return m;

  // Crystalline eye-lens proxy.
  //   Intended use: organ-specific scoring slab for cataract-endpoint dose
  //                 studies, not a geometrically correct eye model.
  //   Composition convention: simplified H/C/N/O mass fractions representing a
  //                           protein-rich water-like tissue.
  //   Density: 1.07 g/cm3.
  //   Radiation protection quantities such as dose equivalent must be computed
  //   outside this material definition.
  auto* m = new G4Material(name,1.07*g/cm3,4);
  m->AddElement(El("H"),0.096);
  m->AddElement(El("C"),0.195);
  m->AddElement(El("N"),0.057);
  m->AddElement(El("O"),0.652);
  return m;
}

G4Material* BuildBFO(){
  const char* name="ShieldSim_BFO_Tissue";
  if(auto* m=Existing(name)) return m;

  // Blood-forming-organ tissue proxy.
  //   Intended use: a material for the scoring slab that the user may place at
  //                 a BFO-relevant depth, for example 50 mm of tissue.
  //   Composition convention: same simplified soft-tissue mass fractions as the
  //                           ICRU 4-element proxy.
  //   Density: 1.00 g/cm3.
  //   The 5 cm BFO-depth convention is not hard-coded here because the slab
  //   thickness is controlled by the CLI.  Use --target=BFO:50 for a 50 mm
  //   scoring slab if that is the intended approximation.
  auto* m = new G4Material(name,1.00*g/cm3,4);
  m->AddElement(El("H"),0.101);
  m->AddElement(El("C"),0.111);
  m->AddElement(El("N"),0.026);
  m->AddElement(El("O"),0.762);
  return m;
}

G4Material* BuildCNS(){
  const char* name="ShieldSim_CNS_Tissue";
  if(auto* m=Existing(name)) return m;

  // Central-nervous-system / brain-tissue proxy.
  //   Intended use: a simple material for CNS energy-deposition scoring, such
  //                 as a hippocampus-like tissue proxy in a slab geometry.
  //   Composition convention: mass fractions for an approximate brain/soft
  //                           tissue.  This is not a voxel phantom.
  //   Density: 1.04 g/cm3.
  auto* m = new G4Material(name,1.04*g/cm3,9);
  m->AddElement(El("H" ),0.107);
  m->AddElement(El("C" ),0.145);
  m->AddElement(El("N" ),0.022);
  m->AddElement(El("O" ),0.712);
  m->AddElement(El("Na"),0.002);
  m->AddElement(El("P" ),0.004);
  m->AddElement(El("S" ),0.002);
  m->AddElement(El("Cl"),0.003);
  m->AddElement(El("K" ),0.003);
  return m;
}

G4Material* BuildSiliconDioxide(){
  const char* name="ShieldSim_SiliconDioxide";
  if(auto* m=Existing(name)) return m;

  // Silicon dioxide / oxide scoring material.
  //   Intended use: gate oxide or insulating oxide TID proxy.
  //   Composition convention: atom counts, SiO2.
  //   Density: 2.20 g/cm3, representative amorphous/fused silica scale.
  //   Device oxides can have different density, hydrogen content, or process
  //   impurities; replace this with a project-specific definition if needed.
  auto* m = new G4Material(name,2.20*g/cm3,2);
  m->AddElement(El("Si"),1);
  m->AddElement(El("O" ),2);
  return m;
}

G4Material* BuildGaAs(){
  const char* name="ShieldSim_GaAs";
  if(auto* m=Existing(name)) return m;

  // Gallium arsenide semiconductor.
  //   Intended use: RF/microwave ICs and multi-junction solar-cell detector
  //                 material proxy.
  //   Composition convention: atom counts, GaAs.
  //   Density: 5.3176 g/cm3.
  auto* m = new G4Material(name,5.3176*g/cm3,2);
  m->AddElement(El("Ga"),1);
  m->AddElement(El("As"),1);
  return m;
}

G4Material* BuildInGaAs(){
  const char* name="ShieldSim_InGaAs";
  if(auto* m=Existing(name)) return m;

  // Indium gallium arsenide semiconductor proxy.
  //   Intended use: SWIR/NIR focal-plane arrays and detector materials.
  //   Composition convention: mass fractions corresponding approximately to
  //                           In0.53Ga0.47As, a common lattice-matched alloy.
  //   Density: 5.68 g/cm3 engineering reference value.
  //   Device-specific In/Ga fraction should be changed here when known.
  auto* m = new G4Material(name,5.68*g/cm3,3);
  m->AddElement(El("In"),0.361);
  m->AddElement(El("Ga"),0.194);
  m->AddElement(El("As"),0.445);
  return m;
}

G4Material* BuildCustomMaterial(const std::string& canonical){
  // Dispatch table for ShieldSim_* materials.  Every custom material listed in
  // ShieldMaterialCatalog() or DetectorMaterialCatalog() must appear here.
  // If it does not, the material will be advertised in --help/list output but
  // will fail at geometry construction.
  if(canonical=="ShieldSim_HDPE")                 return BuildHDPE();
  if(canonical=="ShieldSim_BPE_5pctB")            return BuildBPE();
  if(canonical=="ShieldSim_Kevlar")               return BuildKevlar();
  if(canonical=="ShieldSim_CFRP")                 return BuildCFRP();
  if(canonical=="ShieldSim_SiCCompositePlastic")  return BuildSiCCompositePlastic();
  if(canonical=="ShieldSim_SiC")                  return BuildSiC();
  if(canonical=="ShieldSim_SoftTissue_ICRU4")     return BuildSoftTissueICRU4();
  if(canonical=="ShieldSim_Skin_Epidermis")       return BuildSkinEpidermis();
  if(canonical=="ShieldSim_EyeLens")              return BuildEyeLens();
  if(canonical=="ShieldSim_BFO_Tissue")           return BuildBFO();
  if(canonical=="ShieldSim_CNS_Tissue")           return BuildCNS();
  if(canonical=="ShieldSim_SiliconDioxide")       return BuildSiliconDioxide();
  if(canonical=="ShieldSim_GaAs")                 return BuildGaAs();
  if(canonical=="ShieldSim_InGaAs")               return BuildInGaAs();
  if(canonical=="ShieldSim_LunarRegolith")        return BuildLunarRegolith();
  if(canonical=="ShieldSim_MarsRegolith")         return BuildMarsRegolith();
  return nullptr;
}

std::map<std::string,std::string> MakeAliasMapFor(const std::vector<MaterialCatalogEntry>& catalog){
  // Build a normalized alias -> canonical-name map from a catalog.
  // This keeps the parser data-driven: adding an alias to a catalog entry is
  // enough to make the CLI accept it.  The same helper is used for both the
  // shielding catalog and the detector/target catalog.
  std::map<std::string,std::string> out;
  for(const auto& e : catalog){
    out[NormalizeToken(e.key)] = e.canonicalName;
    out[NormalizeToken(e.displayName)] = e.canonicalName;
    out[NormalizeToken(e.canonicalName)] = e.canonicalName;
    for(const auto& a : e.aliases) out[NormalizeToken(a)] = e.canonicalName;
  }
  return out;
}

const MaterialCatalogEntry* FindCatalogEntryByCanonical(
    const std::vector<MaterialCatalogEntry>& catalog,
    const std::string& canonical)
{
  // Return the catalog entry for a canonical name, or nullptr for raw G4_*
  // materials that are accepted but not listed in the shieldSim short catalog.
  for(const auto& e : catalog)
    if(e.canonicalName==canonical) return &e;
  return nullptr;
}

} // namespace

const std::vector<MaterialCatalogEntry>& ShieldMaterialCatalog(){
  // The order below controls the order printed in --help and --list-materials.
  // Keep the list close to the user-interface grouping shown in the material
  // selection mock-up: metals, hydrogenous polymers, composites, water/regolith.
  static const std::vector<MaterialCatalogEntry> entries = {
    {"Structural Metals", "Al", "Aluminum (Al)", "G4_Al",
     "Geant4/NIST elemental aluminum; density from installed G4 material database.",
     "Refs: [G4-MAT], [NIST-XCOM]", {"Aluminum"}, false},
    {"Structural Metals", "Cu", "Copper (Cu)", "G4_Cu",
     "Geant4/NIST elemental copper; density from installed G4 material database.",
     "Refs: [G4-MAT], [NIST-XCOM]", {"Copper"}, false},
    {"Structural Metals", "W", "Tungsten (W)", "G4_W",
     "Geant4/NIST elemental tungsten; density from installed G4 material database.",
     "Refs: [G4-MAT], [NIST-XCOM]", {"Tungsten"}, false},
    {"Structural Metals", "Ta", "Tantalum (Ta)", "G4_Ta",
     "Geant4/NIST elemental tantalum; density from installed G4 material database.",
     "Refs: [G4-MAT], [NIST-XCOM]", {"Tantalum"}, false},

    {"Hydrogenous Polymers", "HDPE", "HDPE - High-Density Polyethylene", "ShieldSim_HDPE",
     "Custom polyethylene approximation: CH2 repeat unit, density 0.95 g/cm3.",
     "Refs: [G4-PE], [FSRI-HDPE]", {"Polyethylene","HighDensityPolyethylene","High-Density Polyethylene","PE"}, true},
    {"Hydrogenous Polymers", "BPE", "BPE - Borated Polyethylene (5% B)", "ShieldSim_BPE_5pctB",
     "Custom mixture: 95 wt% ShieldSim_HDPE + 5 wt% natural boron, density 0.98 g/cm3.",
     "Refs: [G4-PE], [FSRI-HDPE]; 5 wt% B is the selected catalog definition", {"BoratedPolyethylene","BPE5","BPE5B","Borated Polyethylene 5 B"}, true},
    {"Hydrogenous Polymers", "Kevlar", "Kevlar / Aramid", "ShieldSim_Kevlar",
     "Custom para-aramid approximation: C14H10N2O2 repeat unit, density 1.44 g/cm3.",
     "Ref: [DUPONT-KEVLAR]", {"Aramid","KevlarAramid","ParaAramid"}, true},
    {"Hydrogenous Polymers", "Kapton", "Polyimide / Kapton", "G4_KAPTON",
     "Geant4/NIST Kapton/polyimide material.",
     "Refs: [G4-MAT], [NIST-KAPTON]", {"Polyimide","G4_KAPTON"}, false},

    {"Composites", "CFRP", "CFRP - Carbon Fiber / Epoxy", "ShieldSim_CFRP",
     "Custom trade-study composite: 60 wt% carbon fiber + 40 wt% epoxy, density 1.60 g/cm3.",
     "Engineering placeholder; replace with measured layup/resin data", {"CarbonFiberEpoxy","CarbonFiber","Carbon Fiber Epoxy"}, true},
    {"Composites", "SiCComposite", "SiC Composite Plastic", "ShieldSim_SiCCompositePlastic",
     "Custom trade-study composite: 70 wt% SiC + 30 wt% epoxy, density 2.20 g/cm3.",
     "Refs: [SIC] for SiC phase; matrix/loading are catalog assumptions", {"SiCPlastic","SiCCompositePlastic","SiC Composite"}, true},

    {"Water / Regolith", "Water", "Water (H2O)", "G4_WATER",
     "Geant4/NIST liquid water.",
     "Refs: [G4-MAT], [NIST-XCOM]", {"H2O","G4_WATER"}, false},
    {"Water / Regolith", "LunarRegolith", "Lunar Regolith", "ShieldSim_LunarRegolith",
     "Custom rough bulk lunar-regolith mixture, density 1.60 g/cm3; mass fractions in source.",
     "Ref: [LUNAR-SOURCEBOOK]; site/sample dependent", {"MoonRegolith","Regolith","Lunar/MarsRegolith","Lunar Mars Regolith"}, true},
    {"Water / Regolith", "MarsRegolith", "Mars Regolith", "ShieldSim_MarsRegolith",
     "Custom rough bulk Martian-regolith mixture, density 1.50 g/cm3; mass fractions in source.",
     "Ref: [MARS-APXS]; site/sample dependent", {"MartianRegolith","MarsSoil","MartianSoil"}, true}
  };
  return entries;
}

const std::vector<MaterialCatalogEntry>& DetectorMaterialCatalog(){
  // The order below controls the order printed in --help and
  // --list-target-materials.  The grouping follows the absorber/detector UI:
  // crewed-mission tissue targets, silicon-family electronics, and compound
  // semiconductors / reference media.
  static const std::vector<MaterialCatalogEntry> entries = {
    {"Crewed Mission - Tissue Targets", "Skin", "Skin (Epidermis)", "ShieldSim_Skin_Epidermis",
     "Custom shallow-skin tissue proxy, density 1.09 g/cm3; use user-selected thickness, e.g. 0.07 mm.",
     "Refs: [ICRP-110], [NASA-STD-3001]; endpoint/dose-equivalent conversion is post-processing", {"Epidermis","SkinEpidermis","Skin_Epidermis"}, true},
    {"Crewed Mission - Tissue Targets", "EyeLens", "Eye Lens", "ShieldSim_EyeLens",
     "Custom crystalline-lens tissue proxy, density 1.07 g/cm3; cataract endpoint context only.",
     "Refs: [ICRP-110], [NASA-STD-3001]; not a geometric eye model", {"Lens","Eye","CrystallineLens","Eye Lens"}, true},
    {"Crewed Mission - Tissue Targets", "BFO", "BFO - Blood-Forming Organs", "ShieldSim_BFO_Tissue",
     "Custom soft-tissue proxy for BFO scoring; material only, depth is controlled by CLI thickness.",
     "Refs: [ICRU-44], [NASA-STD-3001]; common depth proxy is 5 cm = 50 mm", {"BloodFormingOrgans","Blood-Forming Organs","BFO_Tissue"}, true},
    {"Crewed Mission - Tissue Targets", "CNS", "CNS - Central Nervous System", "ShieldSim_CNS_Tissue",
     "Custom brain/CNS tissue proxy, density 1.04 g/cm3; slab proxy for CNS energy deposition.",
     "Refs: [ICRP-110], [NASA-STD-3001]; not a voxel phantom", {"Brain","BrainTissue","CentralNervousSystem","Hippocampus"}, true},
    {"Crewed Mission - Tissue Targets", "SoftTissue", "Soft Tissue / Muscle (ICRU 4-element)", "ShieldSim_SoftTissue_ICRU4",
     "Custom simplified ICRU-style 4-element tissue substitute, density 1.00 g/cm3.",
     "Ref: [ICRU-44]; useful general organ-dose proxy", {"Muscle","Tissue","ICRU4","ICRU-4","Soft Tissue"}, true},

    {"Electronics - Silicon Family", "Si", "Si - Silicon", "G4_Si",
     "Geant4/NIST elemental silicon; common scoring medium for CMOS, CCDs, solar cells, TID/DDD proxies.",
     "Refs: [G4-MAT], [NIST-PSTAR], [SEMICONDUCTOR]", {"Silicon","G4_Si"}, false},
    {"Electronics - Silicon Family", "SiO2", "SiO2 - Silicon Dioxide", "ShieldSim_SiliconDioxide",
     "Custom SiO2 oxide proxy, density 2.20 g/cm3; useful for gate-oxide TID studies.",
     "Refs: [SEMICONDUCTOR]; device-specific process oxide may differ", {"SiliconDioxide","Silica","Oxide","GateOxide"}, true},
    {"Electronics - Silicon Family", "SiC", "SiC - Silicon Carbide", "ShieldSim_SiC",
     "Custom dense stoichiometric SiC, density 3.21 g/cm3; wide-bandgap electronics proxy.",
     "Refs: [SIC], [SEMICONDUCTOR]; TID and DDD both may be relevant", {"SiliconCarbide","Silicon Carbide"}, true},

    {"Electronics - III-V and Compound Semiconductors", "GaAs", "GaAs - Gallium Arsenide", "ShieldSim_GaAs",
     "Custom stoichiometric GaAs, density 5.3176 g/cm3; RF/microwave ICs and multi-junction solar cells.",
     "Refs: [SEMICONDUCTOR]; use device-specific material if known", {"GalliumArsenide","Gallium Arsenide"}, true},
    {"Electronics - III-V and Compound Semiconductors", "InGaAs", "InGaAs - Indium Gallium Arsenide", "ShieldSim_InGaAs",
     "Custom In0.53Ga0.47As proxy, density 5.68 g/cm3; SWIR/NIR focal-plane arrays.",
     "Refs: [SEMICONDUCTOR]; DDD/dark-current metrics require post-processing", {"IndiumGalliumArsenide","Indium Gallium Arsenide","In0.53Ga0.47As"}, true},
    {"Electronics - III-V and Compound Semiconductors", "Ge", "Ge - Germanium", "G4_Ge",
     "Geant4/NIST elemental germanium; MWIR/thermal IR detector-array proxy.",
     "Refs: [G4-MAT], [NIST-PSTAR], [SEMICONDUCTOR]", {"Germanium","G4_Ge"}, false},
    {"Electronics - III-V and Compound Semiconductors", "H2O", "H2O - Water (ICRU reference)", "G4_WATER",
     "Geant4/NIST liquid water; useful ICRU reference medium and tissue-comparison baseline.",
     "Refs: [G4-MAT], [NIST-XCOM], [ICRU-44]", {"Water","G4_WATER","LiquidWater"}, false}
  };
  return entries;
}

std::string ResolveShieldMaterialName(const std::string& userName){
  if(userName.empty()) return userName;
  if(StartsWithG4(userName)) return userName;

  static const auto aliasMap = MakeAliasMapFor(ShieldMaterialCatalog());
  const auto key = NormalizeToken(userName);
  const auto it = aliasMap.find(key);
  if(it!=aliasMap.end()) return it->second;

  // Unknown names are passed through.  This preserves a useful extension path:
  // future code can create a G4Material elsewhere before DetectorConstruction is
  // built, then pass that material name through the CLI.
  return userName;
}

std::string ResolveDetectorMaterialName(const std::string& userName){
  if(userName.empty()) return userName;
  if(StartsWithG4(userName)) return userName;

  static const auto detectorAliasMap = MakeAliasMapFor(DetectorMaterialCatalog());
  const auto key = NormalizeToken(userName);
  const auto detIt = detectorAliasMap.find(key);
  if(detIt!=detectorAliasMap.end()) return detIt->second;

  // Scoring slabs are often made from the same material as the shield during
  // debugging or specialized detector studies.  Therefore, detector resolution
  // deliberately falls back to the shielding catalog before giving up.
  return ResolveShieldMaterialName(userName);
}

G4Material* FindOrBuildShieldMaterial(const std::string& userName){
  const std::string canonical = ResolveShieldMaterialName(userName);

  // First check whether Geant4 already has the material in its global material
  // table.  This catches custom materials created earlier in this run and any
  // NIST materials already requested by another part of the application.
  if(auto* m=G4Material::GetMaterial(canonical,false)) return m;

  // Then try shieldSim custom material builders.
  if(auto* m=BuildCustomMaterial(canonical)) return m;

  // Finally, ask the Geant4/NIST material manager.  This is where ordinary G4_*
  // material names are resolved.
  auto* nist=G4NistManager::Instance();
  auto* m=nist->FindOrBuildMaterial(canonical,false);
  if(!m){
    const std::string msg = "Shield material '" + userName + "' could not be resolved. "
                          + "Use --list-materials to see built-in aliases, or "
                          + "provide a valid Geant4 NIST material name such as G4_Al.";
    G4Exception("FindOrBuildShieldMaterial","BadMaterial",FatalException,msg.c_str());
  }
  return m;
}

G4Material* FindOrBuildDetectorMaterial(const std::string& userName){
  const std::string canonical = ResolveDetectorMaterialName(userName);

  // The detector material may already have been created as a custom material or
  // may have been requested by a shielding material.  Check the global Geant4
  // material table first to avoid duplicate definitions.
  if(auto* m=G4Material::GetMaterial(canonical,false)) return m;

  // Detector and shield custom materials share the same builder dispatcher.
  // This lets scoring slabs use both biological/electronics target materials
  // and ordinary shielding materials.
  if(auto* m=BuildCustomMaterial(canonical)) return m;

  // Finally, ask Geant4/NIST.  This supports any G4_* material and also catches
  // catalog aliases that intentionally resolve to a predefined NIST material.
  auto* nist=G4NistManager::Instance();
  auto* m=nist->FindOrBuildMaterial(canonical,false);
  if(!m){
    const std::string msg = "Detector/target material '" + userName + "' could not be resolved. "
                          + "Use --list-target-materials to see detector aliases, "
                          + "--list-materials to see shielding aliases, or provide a valid G4_* material name.";
    G4Exception("FindOrBuildDetectorMaterial","BadMaterial",FatalException,msg.c_str());
  }
  return m;
}

std::string ShieldMaterialCatalogText(){
  std::ostringstream out;
  std::string currentCategory;

  out<<"\n  Allowed shielding-material catalog keys\n";
  out<<"  ---------------------------------------\n";
  for(const auto& e : ShieldMaterialCatalog()){
    if(e.category!=currentCategory){
      currentCategory=e.category;
      out<<"\n  "<<currentCategory<<"\n";
    }
    out<<"    "<<std::left<<std::setw(16)<<e.key
       <<" -> "<<std::setw(32)<<e.displayName
       <<" canonical="<<std::setw(32)<<e.canonicalName
       <<(e.isCustom?" custom":" G4/NIST")<<"\n";
    out<<"       "<<e.description<<"\n";
    out<<"       "<<e.sourceNote<<"\n";
    if(!e.aliases.empty()){
      out<<"       aliases: ";
      for(std::size_t i=0;i<e.aliases.size();++i){
        if(i) out<<", ";
        out<<e.aliases[i];
      }
      out<<"\n";
    }
  }

  out<<"\n  Any Geant4/NIST material name beginning with G4_ is also accepted,\n";
  out<<"  for example G4_Al, G4_Cu, G4_WATER, G4_Si, G4_Pb, or G4_POLYETHYLENE.\n";

  out<<"\n  Shielding-material reference notes\n";
  out<<"  ----------------------------------\n";
  out<<"    [G4-MAT]          Geant4 material database / installed Geant4 NIST definitions.\n";
  out<<"    [NIST-XCOM]       NIST material constants and elemental-density tables.\n";
  out<<"    [NIST-KAPTON]     NIST STAR Kapton composition/density.\n";
  out<<"    [G4-PE]           Geant4 G4_POLYETHYLENE reference material.\n";
  out<<"    [FSRI-HDPE]       Measured HDPE density scale from FSRI material database.\n";
  out<<"    [DUPONT-KEVLAR]   Kevlar technical-guide density scale.\n";
  out<<"    [SIC]             SiC stoichiometry/density from materials references.\n";
  out<<"    [LUNAR-SOURCEBOOK] Lunar Sourcebook regolith composition/density context.\n";
  out<<"    [MARS-APXS]       Mars APXS literature for Martian soil/regolith chemistry.\n";
  out<<"\n  Full URLs and implementation details are documented in src/MaterialCatalog.cc.\n";
  out<<"  Custom composite/regolith definitions are approximate trade-study materials;\n";
  out<<"  replace them with measured project-specific material data for final analyses.\n";
  out<<"\n";
  return out.str();
}

std::string DetectorMaterialCatalogText(){
  std::ostringstream out;
  std::string currentCategory;

  out<<"\n  Allowed detector / absorber / target material catalog keys\n";
  out<<"  --------------------------------------------------------\n";
  for(const auto& e : DetectorMaterialCatalog()){
    if(e.category!=currentCategory){
      currentCategory=e.category;
      out<<"\n  "<<currentCategory<<"\n";
    }
    out<<"    "<<std::left<<std::setw(16)<<e.key
       <<" -> "<<std::setw(42)<<e.displayName
       <<" canonical="<<std::setw(32)<<e.canonicalName
       <<(e.isCustom?" custom":" G4/NIST")<<"\n";
    out<<"       "<<e.description<<"\n";
    out<<"       "<<e.sourceNote<<"\n";
    if(!e.aliases.empty()){
      out<<"       aliases: ";
      for(std::size_t i=0;i<e.aliases.size();++i){
        if(i) out<<", ";
        out<<e.aliases[i];
      }
      out<<"\n";
    }
  }

  out<<"\n  Any Geant4/NIST material name beginning with G4_ is also accepted.\n";
  out<<"  Detector materials are selected with --scoring=<M>:<mm>,... or\n";
  out<<"  the equivalent --target=<M>:<mm>,... option.\n";
  out<<"\n  Detector/target material reference notes\n";
  out<<"  ----------------------------------------\n";
  out<<"    [ICRU-44]        ICRU tissue substitutes / 4-element soft-tissue proxy context.\n";
  out<<"    [ICRP-110]       Reference tissue/organ context; ShieldSim uses simplified slab proxies.\n";
  out<<"    [NASA-STD-3001]  Crew health endpoint context for skin, lens, BFO, and CNS.\n";
  out<<"    [NIST-PSTAR]     Stopping-power table provenance for electronics/scoring media.\n";
  out<<"    [G4-MAT]         Geant4 material database / installed Geant4 NIST definitions.\n";
  out<<"    [NIST-XCOM]      NIST material constants and elemental-density tables.\n";
  out<<"    [SEMICONDUCTOR]  Standard semiconductor material-property references.\n";
  out<<"\n  Important: organ names here define material composition only.  ShieldSim does\n";
  out<<"  not apply quality factors, LET/RBE weighting, organ weighting, NASA limit\n";
  out<<"  checks, or device-response functions internally.  The computed TID, DDD,\n";
  out<<"  n_eq, and LET outputs are first-order slab/proxy quantities; replace the\n";
  out<<"  analytic NIEL/LET response functions with mission- or device-specific data\n";
  out<<"  when quantitative radiation-effects results are required.\n";
  out<<"\n";
  return out.str();
}

std::string DescribeShieldMaterial(const std::string& userName){
  const std::string canonical = ResolveShieldMaterialName(userName);
  if(const auto* e = FindCatalogEntryByCanonical(ShieldMaterialCatalog(),canonical)){
    return e->displayName + " [" + e->key + "; " + e->canonicalName + "]";
  }
  return userName;
}

std::string DescribeDetectorMaterial(const std::string& userName){
  const std::string canonical = ResolveDetectorMaterialName(userName);
  if(const auto* e = FindCatalogEntryByCanonical(DetectorMaterialCatalog(),canonical)){
    return e->displayName + " [" + e->key + "; " + e->canonicalName + "]";
  }
  if(const auto* e = FindCatalogEntryByCanonical(ShieldMaterialCatalog(),canonical)){
    return e->displayName + " [" + e->key + "; " + e->canonicalName + "]";
  }
  return userName;
}
