# Data in the ScienceAccelBench CaMa-Flood snapshot

ScienceAccelBench imports source from
[`25b9caab93dc809d2d5580781c6bff31f185ebf8`](https://github.com/global-hydrodynamics/CaMa-Flood_v4/tree/25b9caab93dc809d2d5580781c6bff31f185ebf8).
The upstream `LICENSE` applies to the code. The data below retain their own
terms. This packaging notice does not relicense either the code or the data.

## Basic river map

`etc/sealev_boundary/moz_06min.tar.gz` contains the CaMa-Flood Mozambique
6-arcminute river map and 1-arcminute auxiliary grids. Dai Yamazaki and
contributors at the University of Tokyo provide CaMa-Flood basic river maps
under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), as stated in
the [publisher's licence section](https://global-hydrodynamics.github.io/CaMa-Flood/#license-to-use-cama-flood-model).
This archive contains no 3- or 15-arcsecond grids. The source files within
the archive retain their Apache-2.0 notices.

Attribution: Dai Yamazaki and CaMa-Flood contributors, University of Tokyo.
Please cite Yamazaki et al. (2011), *A physically based description of floodplain
inundation dynamics in a global river routing model*; Yamazaki et al. (2013),
*Improving computational efficiency in global river models by implementing the
local inertial flow equation and a vector-based river network map*; and
Yamazaki et al. (2019), *MERIT Hydro: A high-resolution global hydrography map
based on latest topography datasets*.

ScienceAccelBench preserves this archive without modification from the pin.

## ERA5-Land runoff

These two files contain ERA5-Land runoff for Japan in January 2000:

- `inp/download_ERA5/ori_japan/ERA5-Land_runoff_200001.nc`
- `inp/download_ERA5/runoff_japan_nc/runoff_200001.nc`

Attribution: Copernicus Climate Change Service (C3S), ECMWF,
*ERA5-Land hourly data from 1950 to present*,
[DOI 10.24381/cds.e2161bac](https://doi.org/10.24381/cds.e2161bac).
The [dataset catalogue](https://cds.climate.copernicus.eu/datasets/reanalysis-era5-land?tab=overview)
states the CC-BY licence. ECMWF records the
[change to CC BY 4.0 on 2 July 2025](https://forum.ecmwf.int/t/cc-by-licence-to-replace-licence-to-use-copernicus-products-on-02-july-2025/13464).
The first file's metadata records ECMWF as the institution and a GRIB conversion
on 19 May 2026.

Contains modified Copernicus Climate Change Service information (2000).
The upstream workflow selects a region and dates and converts runoff into
CaMa-Flood forcing. ScienceAccelBench makes no further changes to these two
files. Neither the European Commission nor ECMWF is responsible for use of
this information. The data remain under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

## Omitted samples

This is a source import with explicit data omissions, not a complete upstream
tree. ScienceAccelBench omits the Mozambique forcing/restart archive,
Mekong point metadata and five model-output series, three dummy CONUS
validation tables, and `map/src/src_param/dam_alloc_error.txt`, because it
could not establish a specific redistribution basis for those files.
The omitted series total eleven files. They are not necessary to compile
the model or run its self-contained common, physics and heatlink unit tests.

The example scripts remain upstream originals. Their presence does not mean
all their inputs ship here. In particular, the Mozambique sea-level example
needs the omitted forcing/restart archive, and the Mekong workflow needs
licensed maps and forcing before it can regenerate its output series.
No script in this import fetches the omitted files automatically.
The GTSM preprocessing notebook also has its stored outputs and execution
counts cleared; its code/Markdown cell sources and metadata are unchanged.

The repository's `codebase-reports/cama-flood/data-policy.json` and
`source-audit.json` record the path, hash, provenance and disposition of the
data assets. Users who supply other data must follow those providers' terms.
