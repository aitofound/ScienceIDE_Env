#include "OutputUtils.hh"

#include "ComputedQuantities.hh"
#include "MaterialCatalog.hh"

#include <G4SystemOfUnits.hh>
#include <G4ios.hh>

#include <fstream>
#include <iomanip>

void WriteDoseSweepTecplot(const std::vector<SweepPoint>& data,
                           const std::vector<std::string>& matNames,
                           const std::string& shieldMat,
                           const Options& opts)
{
  const std::string fname=opts.outputPrefix+"_dose_sweep.dat";
  std::ofstream out(fname);
  if(!out){ G4cerr<<"Cannot write "<<fname<<G4endl; return; }

  out<<"TITLE = \"Dose vs Shield Thickness - Geant4/"<<opts.physicsList<<"\"\n";
  out<<"# Physics list: "<<opts.physicsList<<"\n";
  out<<"# Dose_perPrimary columns are Gy/primary.\n";
  out<<"# DoseRate columns are Gy/s if the input source spectrum units are physical.\n";
  out<<"# DDD columns, when enabled, are spectrum-folded displacement-damage-dose proxies in MeV/g.\n";
  out<<"# n_eq columns, when enabled, are 1-MeV-neutron-equivalent fluence proxies in cm^-2.\n";
  out<<"# H100/10, when enabled, is J(>100 MeV)/J(>10 MeV) for transmitted p+alpha+n and is repeated for each target.\n";
  out<<"# Beam mode assumes the source spectrum columns are differential beam rates [particles/s/MeV].\n";
  out<<"# Isotropic mode assumes the source spectrum columns are differential intensities [particles/(cm2 s sr MeV)].\n";
  out<<"# In isotropic mode the code applies the pi angular factor and multiplies by the finite source-plane area.\n";
  out<<"# Shield material: "<<DescribeShieldMaterial(shieldMat)<<"\n";
  out<<"# Source mode: "<<opts.sourceMode<<"\n";
  out<<"# Events per point: "<<opts.nEvents<<"\n";
  out<<"# Proton energy range: ["<<opts.eMinProton<<", "<<opts.eMaxProton<<"] MeV total kinetic energy\n";
  out<<"# Alpha  energy range: ["<<opts.eMinAlpha <<", "<<opts.eMaxAlpha <<"] MeV total kinetic energy per alpha\n";
  out<<"# Scoring volumes:";
  for(const auto& n:matNames) out<<" "<<n;
  out<<"\n";
  out<<"# Scoring material descriptions:";
  for(const auto& n:matNames) out<<" "<<DescribeDetectorMaterial(n);
  out<<"\n";

  out<<"VARIABLES = \"Thickness [mm]\" \"Areal_Density [g/cm2]\"";
  if(opts.calcTID){
    for(const auto& n:matNames)
      out<<" \"Dose_"<<n<<"_perPrimary [Gy]\"";
    for(const auto& n:matNames)
      out<<" \"DoseRate_"<<n<<" [Gy/s]\"";
  }
  if(opts.calcDDD){
    for(const auto& n:matNames)
      out<<" \"DDD_"<<n<<"_perPrimary [MeV/g]\"";
    for(const auto& n:matNames)
      out<<" \"DDDRate_"<<n<<" [MeV/g/s]\"";
  }
  if(opts.calcNEq){
    for(const auto& n:matNames)
      out<<" \"n_eq_"<<n<<"_perPrimary [cm-2]\"";
    for(const auto& n:matNames)
      out<<" \"n_eq_rate_"<<n<<" [cm-2/s]\"";
  }
  if(opts.calcHardness)
    out<<" \"H100_10\"";
  out<<"\n";

  out<<"ZONE T=\""<<DescribeShieldMaterial(shieldMat)<<" shield | source-mode="<<opts.sourceMode<<" | "
     <<opts.nEvents<<" evt/pt\", I="<<data.size()<<", DATAPACKING=POINT\n";

  out<<std::scientific<<std::setprecision(6);
  for(const auto& pt:data){
    out<<std::setw(14)<<pt.thickMM
       <<std::setw(14)<<pt.arealDensity;
    if(opts.calcTID){
      for(G4double d:pt.dose_Gy)
        out<<std::setw(14)<<d/gray;
      for(G4double r:pt.doseRate_Gy_s)
        out<<std::setw(14)<<r/gray;
    }
    if(opts.calcDDD){
      for(G4double d:pt.ddd_MeV_g_perPrimary)
        out<<std::setw(14)<<d;
      for(G4double r:pt.dddRate_MeV_g_s)
        out<<std::setw(14)<<r;
    }
    if(opts.calcNEq){
      for(G4double n:pt.neq_cm2_perPrimary)
        out<<std::setw(14)<<n;
      for(G4double r:pt.neqRate_cm2_s)
        out<<std::setw(14)<<r;
    }
    if(opts.calcHardness)
      out<<std::setw(14)<<pt.hardness_H100_10;
    out<<"\n";
  }
  out.close();
  G4cout<<"\nDose sweep written: "<<fname
        <<"  ("<<data.size()<<" thickness points)"<<G4endl;
}
