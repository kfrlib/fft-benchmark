# Include custom standard package
include(FindPackageStandard)

# Load using standard package finder
find_package_standard(
  NAMES kfr_io kfr_capi kfr_dft
  HEADERS "kfr/kfr.h"
  PATHS ${KFR} $ENV{KFR}
)