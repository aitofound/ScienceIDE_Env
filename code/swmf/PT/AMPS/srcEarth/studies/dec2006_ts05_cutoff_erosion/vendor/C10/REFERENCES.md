# C10 POES/MetOp SEM-2 dataset and method references

This bibliography separates **dataset/instrument documentation** from
**scientific cutoff-boundary methodology**.  The references explain how to
interpret and process the measurements, but they do **not** collectively provide
the complete numerical list of December-2006 pass-level cutoff crossings needed
by C10.  The reference solution must therefore be regenerated from the archived
daily Level-2 files, with the papers and manuals used to define the algorithm and
quality controls.

## Can the published references themselves be used as the numerical reference?

No.  Dmitriev et al. (2010) provide the boundary definition, event analysis,
figures, fitted models, and physical interpretation.  They do not publish every
satellite/pass/channel crossing with time, MLT, hemisphere, uncertainty, and
spacecraft identity.  The NOAA manuals likewise define the product and its
fields but contain no event-specific measurement table.  A digitized figure or
a value generated from the Dmitriev empirical model would be a secondary/model
reference, not a measurement reference.  C10 therefore reads the original
NOAA/NCEI daily Level-2 archive and records checksums for every source file.

## Ten primary references

### 1. NOAA/NCEI POES/MetOp Space Environment Monitor product page

National Centers for Environmental Information, **POES/MetOp Space Environment
Monitor**.  Official product description, satellite coverage, archive access,
processing notices, SEM-2 manuals, and Level-2 column-document links.

- Product page: https://www.ncei.noaa.gov/products/poes-metop-space-environment-monitor
- Role in C10: authoritative archive entry point and processing-notice source.
- Contains event numerical boundaries? **No.**

### 2. NOAA/NCEI dataset metadata record

National Centers for Environmental Information, **Space Environment Monitor
(SEM) data from NOAA POES and MetOp**, dataset identifier
`gov.noaa.ngdc.sem:poes_sem_g00188`.

- Metadata: https://www.ncei.noaa.gov/access/metadata/landing-page/bin/iso?id=gov.noaa.ngdc.sem%3Apoes_sem_g00188
- Role in C10: dataset citation, period of record, formats, stewardship, and
  direct-download linkage.
- Contains event numerical boundaries? **No.**

### 3. Green (2013), External Users Manual

Green, J. (2013), **External Users Manual: POES/MetOp SEM-2 Processing**, Version
1.0, NOAA National Geophysical Data Center.

- Manual: https://www.ngdc.noaa.gov/stp/satellite/poes/docs/NGDC/External_Users_Manual_POES_MetOp_SEM-2_processing_V1.pdf
- Role in C10: processed-product purpose, directory/file naming, instrument
  overview, physical units, and variable descriptions.
- Contains event numerical boundaries? **No.**

### 4. Evans and Greer (2006), SEM-2 instrument/archive documentation

Evans, D. S., and M. S. Greer (2006), **Polar Orbiting Environmental Satellite
Space Environment Monitor-2: Instrument Descriptions and Archive Data
Documentation**, Version 2.0, NOAA Space Environment Center.

- Documentation family is linked from the NCEI product page above.
- Role in C10: SEM-2 packet/archive definition, detector channels, conversion
  information, data flags, and the P8/P9 alternating readout behavior.
- Contains event numerical boundaries? **No.**

### 5. NOAA/NCEI Level-2 16-second ASCII column description

National Centers for Environmental Information, **Column descriptions for the
Level-2 16-sec averaged POES/MetOp SEM data in ASCII format**.

- Documentation link is provided under “Additional Documentation” on the NCEI
  product page.
- Role in C10: exact historical ASCII field names, including time, sub-satellite
  position, magnetic quantities, and `mepomp6` through `mepomp9`; timestamps are
  at the centers of the 16-second averages.
- Contains event numerical boundaries? **No**, but it defines the fields from
  which they are extracted.

### 6. NOAA/NCEI MEPED OMNI processing ATBD

National Centers for Environmental Information, **MEPED OMNI Processing
Algorithm Theoretical Basis Document**, Version 1.

- ATBD: https://www.ncei.noaa.gov/data/poes-metop-space-environment-monitor/doc/atbd/MEPED_OMNI_processing%20ATBD_V1.pdf
- Role in C10: processing and interpretation of the omnidirectional proton
  channels used for P6–P9.
- Contains event numerical boundaries? **No.**

### 7. Dmitriev, Jayachandran, and Tsai (2010)

Dmitriev, A. V., P. T. Jayachandran, and L.-C. Tsai (2010), **Elliptical model of
cutoff boundaries for the solar energetic particles measured by POES satellites
in December 2006**, *Journal of Geophysical Research: Space Physics*, 115,
A12244, https://doi.org/10.1029/2010JA015380.

- Role in C10: event selection; five-satellite measurement strategy; cutoff as
  the invariant latitude where flux reaches 50% of the polar-cap mean; polar-cap
  region; uncertainty scale; two-hour windows stepped by one hour; and boundary
  morphology.
- Contains event numerical boundaries? **Only partially.** Figures and fitted
  model information are available, but not the complete pass-level table needed
  for a reproducible measurement reference.

### 8. Yando et al. (2011)

Yando, K., R. M. Millan, J. C. Green, and D. S. Evans (2011), **A Monte Carlo
simulation of the NOAA POES Medium Energy Proton and Electron Detector
instrument**, *Journal of Geophysical Research: Space Physics*, 116, A10231,
https://doi.org/10.1029/2011JA016671.

- Role in C10: detector response, geometric factors, cross-species response, and
  limitations of treating a detector channel as a monoenergetic rigidity.
- Contains event numerical boundaries? **No.**

### 9. Birch et al. (2005)

Birch, M. J., J. K. Hargreaves, A. Senior, and B. J. I. Bromage (2005), **Variations
in cutoff latitude during selected solar energetic proton events**, *Journal of
Geophysical Research: Space Physics*, 110, A07221,
https://doi.org/10.1029/2004JA010833.

- Role in C10: observational cutoff-latitude methods using POES particle data and
  event-scale interpretation.
- Contains the December-2006 numerical reference? **No.**

### 10. Asikainen and Mursula (2011)

Asikainen, T., and K. Mursula (2011), **Recalibration of NOAA/MEPED energetic
proton measurements**, *Journal of Atmospheric and Solar-Terrestrial Physics*,
73, 335–347, https://doi.org/10.1016/j.jastp.2009.12.011.

- Role in C10: long-term detector degradation and calibration considerations.
  For a short December-2006 boundary test, relative half-plateau crossings are
  less sensitive to absolute gain than absolute-flux studies, but calibration
  and channel health still require review.
- Contains event numerical boundaries? **No.**

## Useful supplementary references

- Leske, R. A., et al. (2001), geomagnetic cutoff determinations and uncertainty
  methodology, https://doi.org/10.1029/2000JA000212.
- Sandanger, M. I., et al. (2015), MEPED proton detector degradation correction,
  https://doi.org/10.1002/2015JA021388.
- Smart, D. F., and M. A. Shea, geomagnetic cutoff and trajectory-tracing work,
  for broader physical and numerical context.

## Dataset citation used in generated manifests

When publishing results, cite the archive as recommended by NCEI and identify
the exact subset, for example:

> NOAA National Centers for Environmental Information, Space Environment
> Monitor (SEM) data from NOAA POES and MetOp, subset: NOAA-15, NOAA-16,
> NOAA-17, NOAA-18, and MetOp-02, 5–16 December 2006, Level-2 16-second
> processed data; accessed [date].

The generated `download_manifest.json` and `C10_reference_manifest.json` add the
file names, source URLs, byte sizes, and SHA-256 values needed to make that
citation computationally reproducible.
