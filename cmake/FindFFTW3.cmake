# Include custom standard package
include(FindPackageStandard)

# Load using standard package finder
find_package_standard(
  NAMES fftw3 fftw3f
  HEADERS "fftw3.h"
  PATHS $ENV{FFTW} $ENV{FFTW3}
)