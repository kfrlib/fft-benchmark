# Include custom standard package
include(FindPackageStandard)

# Load using standard package finder
find_package_standard(
  NAMES mkl
  HEADERS "mkl.h"
  PATHS ${MKL} $ENV{MKL}
)