#ifndef SIGSERVE_DB020C9E
#define SIGSERVE_DB020C9E
#include "rust/cxx.h"

namespace sigserve_wrapper {

struct Report;
struct PointToPointReport;

int init(const char *sdf_path, bool debug);
Report handle_args(int argc, char *argv[]);
PointToPointReport point_to_point(double tx_lat, double tx_lon, double tx_antenna_alt_m, double rx_lat, double rx_lon,
                                  double rx_antenna_alt_m, double freq_hz, bool normalize, bool metric);

}  // namespace sigserve_wrapper

#endif /* SIGSERVE_DB020C9E */
