#include "sigserve.h"

#include <algorithm>
#include <cmath>

#include "../../../src/common.hh"
#include "rfprop/src/sigserve.rs.h"

extern int init(const char *sdf_path, bool debug);
extern int handle_args(int argc, char *argv[], output &ret_out);
extern int LoadTopoData(double max_lon, double min_lon, double max_lat, double min_lat, struct output *out);
extern double LonDiff(double lon1, double lon2);

namespace sigserve_wrapper {

int init(const char *sdf_path, bool debug) { return ::init(sdf_path, debug); };

Report handle_args(int argc, char *argv[])
{
    Report report;
    output out;

    report.retcode = ::handle_args(argc, argv, out);
    if (report.retcode) {
        return report;
    }

    report.dbm = out.dBm;
    report.loss = out.loss;
    report.field_strength = out.field_strength;

    report.cluttervec.reserve(out.cluttervec.size());
    std::copy(out.cluttervec.begin(), out.cluttervec.end(), std::back_inserter(report.cluttervec));

    report.line_of_sight.reserve(out.line_of_sight.size());
    std::copy(out.line_of_sight.begin(), out.line_of_sight.end(), std::back_inserter(report.line_of_sight));

    report.fresnelvec.reserve(out.fresnelvec.size());
    std::copy(out.fresnelvec.begin(), out.fresnelvec.end(), std::back_inserter(report.fresnelvec));

    report.fresnel60vec.reserve(out.fresnel60vec.size());
    std::copy(out.fresnel60vec.begin(), out.fresnel60vec.end(), std::back_inserter(report.fresnel60vec));

    report.curvaturevec.reserve(out.curvaturevec.size());
    std::copy(out.curvaturevec.begin(), out.curvaturevec.end(), std::back_inserter(report.curvaturevec));

    report.profilevec.reserve(out.profilevec.size());
    std::copy(out.profilevec.begin(), out.profilevec.end(), std::back_inserter(report.profilevec));

    report.distancevec.reserve(out.distancevec.size());
    std::copy(out.distancevec.begin(), out.distancevec.end(), std::back_inserter(report.distancevec));

    report.image_data.reserve(out.imagedata.size());
    std::copy(out.imagedata.begin(), out.imagedata.end(), std::back_inserter(report.image_data));

    return report;
}

TerrainProfile terrain_profile(double tx_lat, double tx_lon, double tx_antenna_alt, double rx_lat, double rx_lon,
                               double rx_antenna_alt, double freq_hz, bool normalize, bool metric)
{
    // The following conversions are confusing, and I'm still not
    // entirely sure why negating longitudes works.
    //
    // But here's what I know: internally, uses normal +/- notation
    // for latitude. However, for longitude, it uses [0,360) to
    // represent longitude.
    //
    // An example mapping using actual input parameters to this function:
    //
    // tx_lat: 68.49 (68.49° N)
    // tx_lon: 179.9 (179.9° E)
    // rx_lat: 68.49 (68.49° N)
    // rx_lon: -179.9 (179.9° W)
    //
    // Translates to the following:
    //
    // _min_lat: 68.000000
    // _max_lat: 68.000000
    // _tx_lon: -180.000000
    // _rx_lon: 179.000000
    // _min_lon: 179.000000
    // _max_lon: -180.000000
    //
    // Which causes the following files to be sources in LoadTopoData:
    //
    // 68:69:179:180.bsdf
    // 68:69:180:181.bsdf
    //
    // And calls the terrain profile with the following sites with the
    // original-but-negated longitudes:
    //
    // tx_site { lat: 68.49, lon: -179.9 }
    // rx_site { lat: 68.49, lon: 179.9 }

    double _min_lat = std::floor(std::min(tx_lat, rx_lat));
    double _max_lat = std::floor(std::max(tx_lat, rx_lat));
    double _tx_lon = std::floor(-tx_lon);
    double _rx_lon = std::floor(-rx_lon);
    double _min_lon = LonDiff(_tx_lon, _rx_lon) < 0.0 ? _tx_lon : _rx_lon;
    double _max_lon = LonDiff(_tx_lon, _rx_lon) < 0.0 ? _rx_lon : _tx_lon;

    printf("_min_lat: %f, _max_lat: %f, _tx_lon: %f, _rx_lon: %f, _min_lon: %f, _max_lon: %f\n", _min_lat, _max_lat, _tx_lon,
           _rx_lon, _min_lon, _max_lon);

    LoadTopoData(_max_lon, _min_lon, _max_lat, _min_lat, nullptr);

    site tx_site;
    tx_site.lat = tx_lat;
    tx_site.lon = -tx_lon;
    tx_site.alt = metric ? tx_antenna_alt * FEET_PER_METER : tx_antenna_alt;

    site rx_site;
    rx_site.lat = rx_lat;
    rx_site.lon = -rx_lon;
    rx_site.alt = metric ? rx_antenna_alt * FEET_PER_METER : rx_antenna_alt;

    Path path(tx_site, rx_site);

    /* Call sigserve's TerrainProfile constructor */
    ::TerrainProfile p2p(tx_site, rx_site, path, freq_hz, normalize, metric);

    TerrainProfile report;

    report.distance.reserve(p2p._distance.size());
    std::copy(p2p._distance.begin(), p2p._distance.end(), std::back_inserter(report.distance));

    report.los.reserve(p2p._los.size());
    std::copy(p2p._los.begin(), p2p._los.end(), std::back_inserter(report.los));

    report.fresnel.reserve(p2p._fresnel.size());
    std::copy(p2p._fresnel.begin(), p2p._fresnel.end(), std::back_inserter(report.fresnel));

    report.fresnel60.reserve(p2p._fresnel60.size());
    std::copy(p2p._fresnel60.begin(), p2p._fresnel60.end(), std::back_inserter(report.fresnel60));

    report.curvature.reserve(p2p._curvature.size());
    std::copy(p2p._curvature.begin(), p2p._curvature.end(), std::back_inserter(report.curvature));

    report.terrain.reserve(p2p._terrain.size());
    std::copy(p2p._terrain.begin(), p2p._terrain.end(), std::back_inserter(report.terrain));

    return report;
}

}  // namespace sigserve_wrapper
