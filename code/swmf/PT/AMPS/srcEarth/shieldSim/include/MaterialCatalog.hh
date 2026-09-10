#ifndef SHIELDSIM_MATERIAL_CATALOG_HH
#define SHIELDSIM_MATERIAL_CATALOG_HH

/* ============================================================================
 * MaterialCatalog.hh
 *
 * Centralized shielding and detector/target material catalogs for shieldSim
 * ------------------------------------------------------------------------
 * This header defines the small, user-facing material databases used by the
 * shieldSim command line.  The purpose of these catalogs is not to replace the
 * Geant4/NIST material database.  Instead, they give the user short and stable
 * material names.  The shielding catalog contains absorber/shield materials
 * such as
 *
 *     Al, HDPE, BPE, Kevlar, CFRP, LunarRegolith
 *
 * while the detector/target catalog contains scoring media such as
 *
 *     Skin, EyeLens, BFO, CNS, SoftTissue, Si, SiO2, SiC, GaAs, InGaAs, Ge
 *
 * Both catalogs still allow ordinary Geant4/NIST material names beginning with
 * "G4_", for example G4_Al, G4_WATER, G4_Si, or G4_Pb.
 *
 * Why these catalogs exist
 * -------------------------
 * Geant4 already contains many standard NIST materials.  However, for a
 * shielding/detector trade study it is useful to expose a smaller list of
 * engineering and biological scoring materials in the CLI and to define a few
 * approximate materials that are not guaranteed to exist as predefined Geant4
 * entries, for example borated polyethylene, carbon-fiber/epoxy, tissue
 * proxies, compound semiconductors, and regolith-like mixtures.
 *
 * Where material properties are defined
 * -------------------------------------
 * The actual material definitions are implemented in src/MaterialCatalog.cc.
 * Some catalog entries are simple aliases to Geant4/NIST materials; those are
 * marked with isCustom=false and their canonicalName is a G4_* material name.
 * Other entries are built explicitly by shieldSim; those are marked with
 * isCustom=true and have canonical names beginning with ShieldSim_*.
 *
 * IMPORTANT: custom entries are approximate trade-study materials.  For a final
 * mission or instrument analysis, replace density, composition, filler fraction,
 * composite layup, and regolith chemistry with measured project-specific values.
 *
 * How to add a new shielding material
 * -----------------------------------
 * Case A: the material already exists in Geant4/NIST
 *   1. Add a new MaterialCatalogEntry in ShieldMaterialCatalog() in
 *      src/MaterialCatalog.cc.
 *   2. Set canonicalName to the Geant4 material name, e.g. "G4_Pb".
 *   3. Set isCustom=false.
 *   4. Add aliases that users might type on the command line.
 *   5. Add a clear description and a source/reference note.
 *
 * Case B: the material must be built by shieldSim
 *   1. Add a new MaterialCatalogEntry in ShieldMaterialCatalog() with a unique
 *      canonicalName beginning with "ShieldSim_" and isCustom=true.
 *   2. Implement a builder in src/MaterialCatalog.cc, following the pattern of
 *      BuildHDPE(), BuildBPE(), BuildKevlar(), or BuildLunarRegolith().
 *      Use Geant4 units explicitly, e.g. 1.23*g/cm3.
 *   3. At the beginning of the builder, return an existing material if it has
 *      already been created:
 *
 *          const char* name = "ShieldSim_NewMaterial";
 *          if (auto* m = Existing(name)) return m;
 *
 *      This avoids duplicate material definitions when geometry is rebuilt in
 *      sweep mode.
 *   4. Add the builder to BuildCustomMaterial().
 *   5. For compounds or polymers, use AddElement(element, nAtoms).  For mixtures
 *      and composites, use AddElement/AddMaterial with mass fractions that sum
 *      to 1.0.
 *   6. Document the density, composition convention, and references in the
 *      comment above the builder and in the catalog entry.  The --help and
 *      --list-materials text is generated from the catalog, so no separate help
 *      table needs to be edited.
 *
 * How to add a new detector/target material
 * -----------------------------------------
 * Detector/target materials are the scoring media used downstream of the
 * shield.  They can represent tissue-equivalent organs, water, silicon devices,
 * gate oxides, compound semiconductors, or any other absorber in which dose,
 * energy deposition, or later NIEL-like metrics are to be estimated.
 *
 * Case A: the detector material already exists in Geant4/NIST
 *   1. Add a new MaterialCatalogEntry in DetectorMaterialCatalog() in
 *      src/MaterialCatalog.cc.
 *   2. Set canonicalName to the Geant4 material name, e.g. "G4_Si".
 *   3. Set isCustom=false.
 *   4. Add user-friendly aliases, a use-case description, and a reference note.
 *
 * Case B: the detector material must be built by shieldSim
 *   1. Add a new MaterialCatalogEntry in DetectorMaterialCatalog() with a
 *      unique canonicalName beginning with "ShieldSim_" and isCustom=true.
 *   2. Implement a builder in src/MaterialCatalog.cc.  Keep the density,
 *      composition convention, and references in the comment immediately above
 *      the builder so a future user can audit the material definition.
 *   3. Register the builder in BuildCustomMaterial().  This same dispatcher is
 *      shared by shielding and detector materials.
 *   4. Use FindOrBuildDetectorMaterial() when the material is intended for a
 *      scoring slab.  That routine first searches the detector/target catalog
 *      and then falls back to the shielding catalog and ordinary G4_* names so
 *      a shield material can also be used as a detector if needed.
 *
 * Common pitfalls when adding materials
 * -------------------------------------
 *   - Do not omit Geant4 units for density or thickness.
 *   - Do not create the same G4Material twice under the same name.
 *   - Be explicit about whether fractions are mass fractions or atom counts.
 *   - Avoid pretending that engineering composites/regolith are unique materials;
 *     their properties are sample- and process-dependent.
 * ========================================================================== */

#include <string>
#include <vector>

class G4Material;

struct MaterialCatalogEntry {
  std::string category;       // UI grouping, e.g. "Structural Metals".
  std::string key;            // Recommended CLI key, e.g. "Al" or "HDPE".
  std::string displayName;    // Human-readable label shown in help text.
  std::string canonicalName;  // Geant4/NIST name or ShieldSim_* custom name.
  std::string description;    // Density/composition/meaning note.
  std::string sourceNote;     // Short source/reference note shown in help text.
  std::vector<std::string> aliases; // Alternative CLI spellings.
  bool isCustom = false;      // True if shieldSim builds the material itself.
};

// Shielding-material catalog in the order used by --help, --list-materials, and
// the README.  Keeping one authoritative list avoids inconsistencies between
// the CLI parser, geometry builder, and documentation.
const std::vector<MaterialCatalogEntry>& ShieldMaterialCatalog();

// Detector/absorber/target-material catalog in the order used by --help,
// --list-target-materials, and the README.  These entries are intended for
// downstream scoring slabs selected with --scoring / --target.
const std::vector<MaterialCatalogEntry>& DetectorMaterialCatalog();

// Resolve a user-provided shield material name or alias to the canonical
// material name.  Names beginning with "G4_" are passed through unchanged so
// arbitrary Geant4/NIST materials remain supported.
std::string ResolveShieldMaterialName(const std::string& userName);

// Resolve a user-provided detector/target material name or alias to the
// canonical material name.  The detector resolver searches the detector catalog
// first and then falls back to shielding aliases so any shielding material can
// also be used as a scoring slab if needed.
std::string ResolveDetectorMaterialName(const std::string& userName);

// Build or return the Geant4 material corresponding to a shielding catalog key,
// alias, or ordinary NIST material name.
G4Material* FindOrBuildShieldMaterial(const std::string& userName);

// Build or return the Geant4 material corresponding to a detector/target catalog
// key, alias, shielding alias, or ordinary NIST material name.  Use this for
// scoring slabs so biological and electronics detector materials work directly
// from the CLI.
G4Material* FindOrBuildDetectorMaterial(const std::string& userName);

// User-facing tables for --help and list commands.  The returned strings list
// allowed catalog keys, aliases, material descriptions, and reference notes.
std::string ShieldMaterialCatalogText();
std::string DetectorMaterialCatalogText();

// One-line material descriptions for configuration printouts and metadata.
std::string DescribeShieldMaterial(const std::string& userName);
std::string DescribeDetectorMaterial(const std::string& userName);

#endif // SHIELDSIM_MATERIAL_CATALOG_HH
