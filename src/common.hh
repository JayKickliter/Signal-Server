#ifndef _COMMON_HH_
#define _COMMON_HH_

#define GAMMA 		2.5

#ifndef PI
  #define PI		3.141592653589793
#endif

#ifndef TWOPI
  #define TWOPI		6.283185307179586
#endif

#ifndef HALFPI
  #define HALFPI	1.570796326794896
#endif

#define DEG2RAD		1.74532925199e-02
#define	EARTHRADIUS	20902230.97
#define	METERS_PER_MILE 1609.344
#define	METERS_PER_FOOT 0.3048
#define	KM_PER_MILE	1.609344
#define	FEET_PER_MILE	5280.0
#define FOUR_THIRDS	1.3333333333333

#define MAX(x,y)((x)>(y)?(x):(y))

struct dem {
	float min_north;
	float max_north;
	float min_west;
	float max_west;
	long min_x, max_x, min_y, max_y;
	int max_el;
	int min_el;
	short **data;
	unsigned char **mask;
	unsigned char **signal;
};

struct site {
	double lat;
	double lon;
	float alt;
	char name[50];
	char filename[255];
};

struct path {
	double *lat;
	double *lon;
	double *elevation;
	double *distance;
	int length;
};

struct LR {
	double eps_dielect;
	double sgm_conductivity;
	double eno_ns_surfref;
	double frq_mhz;
	double conf;
	double rel;
	double erp;
	int radio_climate;
	int pol;
	float antenna_pattern[361][1001];
};

struct region {
	unsigned char color[128][3];
	int level[128];
	int levels;
};

extern int MAXPAGES;
extern int ARRAYSIZE;
extern int IPPD;

extern double G_min_north;
extern double G_max_north;
extern double G_min_west;
extern double G_max_west;
extern int G_ippd;
extern int G_MAXRAD;
extern int G_mpi;
extern int G_max_elevation;
extern int G_min_elevation;
extern int G_contour_threshold;
extern int G_loops;
extern int G_jgets;
extern int G_width;
extern int G_height;

extern double G_earthradius;
extern double G_north;
extern double G_east;
extern double G_south;
extern double G_west;
extern double G_max_range;
extern double G_dpp;
extern double G_ppd;
extern double G_yppd;
extern double G_fzone_clearance;
extern double G_clutter;
extern double G_dBm;
extern double G_loss;
extern double G_field_strength;
extern __thread double *G_elev;
extern double G_westoffset;
extern double G_eastoffset;
extern double G_delta;
extern double G_cropLat;
extern double G_cropLon;

extern char G_string[];
extern char G_sdf_path[];
extern char G_gpsav;

extern unsigned char G_got_elevation_pattern;
extern unsigned char G_got_azimuth_pattern;
extern unsigned char G_metric;
extern unsigned char G_dbm;

extern struct dem *G_dem;
extern __thread struct path G_path;
extern struct LR G_LR;
extern struct region G_region;

extern int G_debug;

#endif /* _COMMON_HH_ */
