#ifndef _COMMON_HH_
#define _COMMON_HH_

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <vector>

#define GAMMA 2.5

#ifndef PI
#define PI 3.141592653589793
#endif

#ifndef TWOPI
#define TWOPI 6.283185307179586
#endif

#ifndef HALFPI
#define HALFPI 1.570796326794896
#endif

#define DEG2RAD 1.74532925199e-02
#define EARTHRADIUS_FT 20902230.97
#define METERS_PER_MILE 1609.344
#define METERS_PER_FOOT 0.3048
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
    short *data;
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
    double lat;
    double lon;
    float alt;
    /* TODO: remove the following fields. They use a huge amount of
       stack and conflate IO with baseness logic. */
    char name[50];
    char filename[255];
};

struct Path {
    std::vector<double> lat;
    std::vector<double> lon;
    std::vector<double> elevation;
    std::vector<double> distance;
    ssize_t ssize() const;
    Path() = default;
    Path(site const &src, site const &dst);
};

class antenna_pattern {
    std::vector<float> elems;

   public:
    antenna_pattern() : elems(361 * 1001, 0.0) {}
    float const &operator()(int azimuth, int elevation) const { return elems[(azimuth * 1001) + elevation]; }
    float &operator()(int azimuth, int elevation) { return elems[(azimuth * 1001) + elevation]; }
};

// TODO what does LR mean
// essentially this struct holds the
// read-only configuration options
struct LR {
    double max_range;
    double clutter;
    int contour_threshold;
    char dbm;
    unsigned char metric;
    double eps_dielect;
    double sgm_conductivity;
    double eno_ns_surfref;
    double frq_mhz;
    double conf;
    double rel;
    double erp;
    int radio_climate;
    int pol;
    antenna_pattern ant_pat;
    double antenna_downtilt;
    double antenna_dt_direction;
    double antenna_rotation;
};

struct output {
    std::vector<dem_output> dem_out;
    int width;
    int height;
    int min_elevation;
    int max_elevation;
    double min_north;
    double max_north;
    double min_west;
    double max_west;
    std::vector<double> elev;
    double north;
    double east;
    double south;
    double west;
    double westoffset;
    double eastoffset;
    double cropLat;
    double cropLon;
    double dBm;
    double loss;
    double field_strength;
    int hottest;
    struct site tx_site[2];
    std::vector<double> distancevec;
    std::vector<double> cluttervec;
    std::vector<double> line_of_sight;
    std::vector<double> fresnelvec;
    std::vector<double> fresnel60vec;
    std::vector<double> curvaturevec;
    std::vector<double> profilevec;
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

extern double G_earthradius_ft;
extern double G_dpp;
extern double G_ppd;
extern double G_yppd;
extern double G_fzone_clearance;
extern double G_delta;

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
