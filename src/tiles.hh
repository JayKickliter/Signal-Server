#ifndef _TILES_HH_
#define _TILES_HH_

#include "common.hh"

typedef struct _tile_t {
    char * filename;
    union {
        int cols;
        int width;
    };
    union {
        int rows;
        int height;
    };
    union {
        SsFloat xll;
        SsFloat max_west;
    };
    union {
        SsFloat yll;
        SsFloat min_north;
    };
    union {
        SsFloat xur;
        SsFloat min_west;
    };
    union {
        SsFloat yur;
        SsFloat max_north;
    };
    SsFloat cellsize;
    long long datastart;
    short nodata;
    short max_el;
    short min_el;
    short * data;
    float precise_resolution;
    float resolution;
    SsFloat width_deg;
    SsFloat height_deg;
    int ppdx;
    int ppdy;
} tile_t, *ptile_t;

int tile_load_lidar(tile_t *, char *, struct output * out);
int tile_rescale(tile_t *, float);
void tile_destroy(tile_t *);

#endif
