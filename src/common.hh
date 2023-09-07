#ifndef _COMMON_HH_
#define _COMMON_HH_

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <vector>

typedef float SsFloat;

#define GAMMA 2.5

#ifndef PI
#define PI 3.141592653589793
#endif

#ifndef TWOPI
#define TWOPI 6.283185307179586
#endif

#ifndef HALFPI
#define HALFPI SsFloat(1.570796326794896)
#endif

#define DEG2RAD 1.74532925199e-02
#define EARTHRADIUS_FT 20902230.97
#define METERS_PER_MILE 1609.344
#define METERS_PER_FOOT 0.3048
#define FEET_PER_METER 3.28084
#define KM_PER_MILE 1.609344
#define FEET_PER_MILE 5280.0
#define FOUR_THIRDS 1.3333333333333

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define DEM_INDEX(ippd, x, y) (((y)*ippd) + x)

struct dem {
    int ippd;
    float min_north;
    float max_north;
    float min_west;
    float max_west;
    short max_el;
    short min_el;
    short * data;
};

struct dem_output {
    std::shared_ptr<const struct dem> dem;
    float min_north;
    float max_north;
    float min_west;
    float max_west;
    std::vector<unsigned char> mask;
    std::vector<unsigned char> signal;
};

struct site {
    SsFloat lat;
    SsFloat lon;
    float alt;
    /* TODO: remove the following fields. They use a huge amount of
       stack and conflate IO with baseness logic. */
    char name[50];
    char filename[255];
};

struct Path {
    std::vector<SsFloat> lat;
    std::vector<SsFloat> lon;
    std::vector<SsFloat> elevation;
    std::vector<SsFloat> distance;
    ssize_t ssize() const;
    size_t size() const;
    Path() = default;
    Path(site const & src, site const & dst);
};

struct TerrainProfile {
    std::vector<SsFloat> _curvature;
    std::vector<SsFloat> _distance;
    std::vector<SsFloat> _fresnel60;
    std::vector<SsFloat> _fresnel;
    std::vector<SsFloat> _los;
    std::vector<SsFloat> _terrain;
    bool _tx_site_over_water;
    TerrainProfile(site const & src,
                   site const & dst,
                   Path const & path,
                   SsFloat freq_hz,
                   bool normalised,
                   bool metric) noexcept;
};

class antenna_pattern {
    std::vector<float> elems;

  public:
    antenna_pattern()
        : elems(361 * 1001, 0.0) {}
    float const & operator()(int azimuth, int elevation) const {
        return elems[(azimuth * 1001) + elevation];
    }
    float & operator()(int azimuth, int elevation) {
        return elems[(azimuth * 1001) + elevation];
    }
};

// TODO what does LR mean
// essentially this struct holds the
// read-only configuration options
struct LR {
    SsFloat max_range;
    SsFloat clutter;
    int contour_threshold;
    char dbm;
    unsigned char metric;
    SsFloat eps_dielect;
    SsFloat sgm_conductivity;
    SsFloat eno_ns_surfref;
    SsFloat frq_mhz;
    SsFloat conf;
    SsFloat rel;
    SsFloat erp;
    int radio_climate;
    int pol;
    antenna_pattern ant_pat;
    SsFloat antenna_downtilt;
    SsFloat antenna_dt_direction;
    SsFloat antenna_rotation;
};

struct output {
    std::vector<dem_output> dem_out;
    int width;
    int height;
    int min_elevation;
    int max_elevation;
    SsFloat min_north;
    SsFloat max_north;
    SsFloat min_west;
    SsFloat max_west;
    std::vector<SsFloat> elev;
    SsFloat north;
    SsFloat east;
    SsFloat south;
    SsFloat west;
    SsFloat westoffset;
    SsFloat eastoffset;
    SsFloat cropLat;
    SsFloat cropLon;
    SsFloat dBm;
    SsFloat loss;
    SsFloat field_strength;
    int hottest;
    struct site tx_site[2];
    std::vector<SsFloat> distancevec;
    std::vector<SsFloat> cluttervec;
    std::vector<SsFloat> line_of_sight;
    std::vector<SsFloat> fresnelvec;
    std::vector<SsFloat> fresnel60vec;
    std::vector<SsFloat> curvaturevec;
    std::vector<SsFloat> profilevec;
    std::vector<char> imagedata;
};

struct region {
    unsigned char color[128][3];
    int level[128];
    int levels;
};

extern int MAXPAGES;
extern int ARRAYSIZE;
extern int IPPD;

extern int G_ippd;
extern int G_mpi;

extern SsFloat G_earthradius_ft;
extern SsFloat G_dpp;
extern SsFloat G_ppd;
extern SsFloat G_yppd;
extern SsFloat G_fzone_clearance;
extern SsFloat G_delta;

extern char G_string[];
extern char G_sdf_path[];
extern char G_gpsav;

extern unsigned char G_got_elevation_pattern;
extern unsigned char G_got_azimuth_pattern;

extern std::vector<std::shared_ptr<const struct dem>> G_dem;
extern std::shared_mutex G_dem_mtx;
extern struct region G_region;

extern int G_debug;

#endif /* _COMMON_HH_ */
