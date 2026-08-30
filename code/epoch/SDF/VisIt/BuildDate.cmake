string(TIMESTAMP DATE "%a %d %b %H:%M:%S %Y")
file(WRITE build_date.h "#define SDF_READER_BUILD_DATE \"${DATE}\"\n")
