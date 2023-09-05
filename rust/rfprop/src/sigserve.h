#ifndef SIGSERVE_DB020C9E
#define SIGSERVE_DB020C9E
#include "rust/cxx.h"

namespace sigserve_wrapper {

struct Report;
struct TerrainProfile;

int init(const char *sdf_path, bool debug);
Report handle_args(int argc, char *argv[]);
TerrainProfile terrain_profile(float tx_lat, float tx_lon, float tx_antenna_alt_m, float rx_lat, float rx_lon,
                               float rx_antenna_alt_m, float freq_hz, bool normalize, bool metric);

}  // namespace sigserve_wrapper

#endif /* SIGSERVE_DB020C9E */
