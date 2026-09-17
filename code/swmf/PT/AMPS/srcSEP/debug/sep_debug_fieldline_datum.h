#ifndef SEP_DEBUG_FIELDLINE_DATUM_H
#define SEP_DEBUG_FIELDLINE_DATUM_H

#include "pic.h"

namespace SEP {
namespace Debug {

// -----------------------------------------------------------------------------
// ValidateFieldLineDatum()
// -----------------------------------------------------------------------------
// Generic debugger helper for any field-line segment datum.
//
// The AMPS field-line MPI synchronization routines already validate values during
// unpacking, but when the error is detected on the receiving side it can be hard
// to determine which SEP datum, field line, segment, and component produced the
// bad value.  This helper performs the same validate_numeric() check directly on
// the field-line datum storage and prints the full context before validation for
// clearly non-finite or extremely large values.
//
// Datum is intentionally a parameter so the same routine can be used for
// different stored quantities, for example CellIntegratedWaveEnergy,
// WaveEnergyDensity, G_plus_streaming, G_minus_streaming, and the
// wave-number-resolved SpectralWaveEnergy datum.
// -----------------------------------------------------------------------------
void ValidateFieldLineDatum(
    PIC::Datum::cDatumStored& Datum,
    const char* datum_name,
    const char* context,
    long int iteration,
    int caller_line,
    const char* caller_file);

} // namespace Debug
} // namespace SEP

#endif // SEP_DEBUG_FIELDLINE_DATUM_H
