# Include custom standard package
include(FindPackageStandard)

# Load using standard package finder
find_package_standard(
  NAMES ipp
  HEADERS "ipp.h"
  PATHS ${IPP} $ENV{IPP}
)