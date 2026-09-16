from ndsl.constants import CONST_VERSION, ConstantVersions

# By selecting the GEOS constant using (NDSL_CONSTANTS env var)
# we select the GEOS flavor of numerics
IS_GEOS = ConstantVersions.GEOS == CONST_VERSION
