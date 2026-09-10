#include "sep_debug_fieldline_datum.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace SEP {
namespace Debug {

namespace {

// Values close to DBL_MAX are almost always numerical artifacts in the SEP
// turbulence model.  validate_numeric() will also catch non-finite values, but
// the explicit threshold below lets us print a detailed diagnostic *before* the
// generic validator aborts the run.  The threshold is intentionally very large so
// it does not reject physically meaningful SEP/turbulence values; it is only a
// context-printing trigger for values that are already effectively overflowed.
const double ContextPrintAbsThreshold = 1.0e100;

inline bool needs_context_print(double value) {
  return (!std::isfinite(value)) || (std::fabs(value) > ContextPrintAbsThreshold);
}

} // anonymous namespace

void ValidateFieldLineDatum(
    PIC::Datum::cDatumStored& Datum,
    const char* datum_name,
    const char* context,
    long int iteration,
    int caller_line,
    const char* caller_file) {

  // Keep the function essentially free in non-debugger runs.  This routine is
  // called at the beginning of every main iteration, and some datums, especially
  // the wave-number-resolved spectrum, can have many components per segment.
  // The check is therefore enabled only in AMPS debugger mode.
  if (_PIC_DEBUGGER_MODE_ != _PIC_DEBUGGER_MODE_ON_) return;

  const char* safe_datum_name = (datum_name != nullptr) ? datum_name : "<unnamed datum>";
  const char* safe_context    = (context    != nullptr) ? context    : "<no context>";
  const char* safe_file       = (caller_file!= nullptr) ? caller_file: "<unknown file>";

  const int datum_length = Datum.length;

  // A non-positive length is not expected for a cDatumStored that is registered
  // with field-line segments.  Treat it as a diagnostic condition rather than
  // silently returning, because an incorrect length would also break packing and
  // unpacking of the datum.
  if (datum_length <= 0) {
    if (PIC::ThisThread == 0) {
      std::fprintf(stderr,
          "SEP::Debug::ValidateFieldLineDatum: datum '%s' has invalid length %d. "
          "context='%s', iteration=%ld, called from %s:%d\n",
          safe_datum_name, datum_length, safe_context, iteration, safe_file, caller_line);
      std::fflush(stderr);
    }
    return;
  }

  for (int iFieldLine = 0; iFieldLine < PIC::FieldLine::nFieldLine; ++iFieldLine) {
    PIC::FieldLine::cFieldLine& field_line = PIC::FieldLine::FieldLinesAll[iFieldLine];

    int iSegment = 0;
    for (PIC::FieldLine::cFieldLineSegment* segment = field_line.GetFirstSegment();
         segment != nullptr;
         segment = segment->GetNext(), ++iSegment) {

      double* data = segment->GetDatum_ptr(Datum);
      if (data == nullptr) continue;

      for (int iDatum = 0; iDatum < datum_length; ++iDatum) {
        const double value = data[iDatum];

        // Print detailed context before calling validate_numeric().  This is the
        // key purpose of the helper: when validate_numeric() aborts, the normal
        // AMPS field-line MPI code reports only the pack/unpack line.  The print
        // below identifies the actual SEP datum component and the field-line
        // location that produced the suspicious value.
        if (needs_context_print(value)) {
          std::fprintf(stderr,
              "\nSEP FIELD-LINE DATUM VALIDATION WARNING\n"
              "  datum       : %s\n"
              "  context     : %s\n"
              "  iteration   : %ld\n"
              "  MPI rank    : %d\n"
              "  field line  : %d\n"
              "  segment     : %d\n"
              "  component   : %d of %d\n"
              "  value       : %.17e\n"
              "  segment ptr : %p\n"
              "  caller      : %s:%d\n",
              safe_datum_name,
              safe_context,
              iteration,
              PIC::ThisThread,
              iFieldLine,
              iSegment,
              iDatum,
              datum_length,
              value,
              static_cast<void*>(segment),
              safe_file,
              caller_line);
          std::fflush(stderr);
        }

        // Use the same AMPS validation mechanism as the field-line MPI unpacking
        // code.  Passing the caller location makes the abort point identify the
        // place in the SEP driver where the pre-iteration validation was invoked,
        // while the diagnostic printed above identifies the exact datum element.
        validate_numeric(value, caller_line, safe_file);
      }
    }
  }
}

} // namespace Debug
} // namespace SEP
