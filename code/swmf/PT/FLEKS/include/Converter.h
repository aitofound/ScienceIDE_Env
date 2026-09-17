#ifndef _CONVERTER_H_
#define _CONVERTER_H_

#include "DataWriter.h"

class Converter {
public:
  Converter(const std::string& in, FileType sType, FileType dType)
      : sourceFile(in), sourceType(sType), destType(dType) {

    if (sourceType == FileType::UNSET)
      sourceType = find_source_type();

    switch (sourceType) {
      case FileType::AMREX: {
        amrex::Geometry gm;
        amrex::AmrInfo info;
        amrex::RealBox domain;
        amrex::Box cellBox;

        { // Read simulation domain, and cell number from header.
          std::string headerName = sourceFile + "/Header";

          int nVar, nDim, finest_lev;
          amrex::Real time;
          amrex::Vector<std::string> varNames;

          AMReXDataContainer::read_header(headerName, nVar, nDim, time,
                                          finest_lev, domain, cellBox,
                                          varNames);

          info.max_level = finest_lev;
          info.blocking_factor.clear();
          for (int iLev = 0; iLev <= info.max_level; iLev++) {
            info.blocking_factor.push_back(amrex::IntVect(1));
          }
        }

        gm.define(cellBox, &domain);

        dc = std::make_unique<AMReXDataContainer>(sourceFile, gm, info);

        break;
      }
      case FileType::IDL: {
        dc = std::make_unique<IDLDataContainer>(sourceFile);
        break;
      }
      case FileType::TEC: {
        amrex::Abort("Error: TEC input is not supported yet!");
        break;
      }
      case FileType::VTK: {
        amrex::Abort("Error: VTK input is not supported yet!");
        break;
      }
      case FileType::UNSET: {
        amrex::Abort("Error: set source file format with -s option!");
        break;
      }
      case FileType::UNKNOWN: {
        amrex::Abort("Error: source file format is unknown!");
        break;
      }
    }

    dc->print();

    switch (destType) {
      case FileType::TEC: {
        writer = std::make_unique<TECWriter>(dc.get(), sourceFile);
        break;
      }
      case FileType::VTK: {
        writer = std::make_unique<VTKWriter>(dc.get(), sourceFile);
        break;
      }
      case FileType::IDL: {
        amrex::Abort("Error: IDL output is not supported yet!");
        break;
      }
      case FileType::AMREX: {
        amrex::Abort("Error: AMREX output is not supported yet!");
        break;
      }
      case FileType::UNSET: {
        amrex::Abort("Error: set destination file format with -d option!");
        break;
      }
      case FileType::UNKNOWN: {
        amrex::Abort("Error: destination file format is unknown!");
        break;
      }
    }
    writer->print();
  }

  FileType find_source_type() {
    if (sourceFile.size() > 6 &&
        sourceFile.substr(sourceFile.size() - 6, 6) == "_amrex") {
      // *_amrex
      return FileType::AMREX;
    } else if (sourceFile.size() > 4 &&
               sourceFile.substr(sourceFile.size() - 4, 4) == ".dat") {
      // *.dat
      return FileType::TEC;
    } else if (sourceFile.size() > 4 &&
               sourceFile.substr(sourceFile.size() - 4, 4) == ".out") {
      // *.out
      return FileType::IDL;
    } else {
      amrex::Abort("Unknown sourceFile type");
    }

    return FileType::UNKNOWN;
  }
  int read() { return dc->read(); }
  int write() { return writer->write(); }

  void smooth(int nSmooth) { dc->smooth(nSmooth); }

private:
  std::string sourceFile;
  FileType sourceType;
  std::unique_ptr<DataContainer> dc;

  FileType destType;
  std::unique_ptr<DataWriter> writer;
};

#endif
