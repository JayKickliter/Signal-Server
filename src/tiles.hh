#ifndef _TILES_HH_
#define _TILES_HH_

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
        float xll;
        float max_west;
    };
    union {
        float yll;
        float min_north;
    };
    union {
        float xur;
        float min_west;
    };
    union {
        float yur;
        float max_north;
    };
    float cellsize;
    long long datastart;
    short nodata;
    short max_el;
    short min_el;
    short * data;
    float precise_resolution;
    float resolution;
    float width_deg;
    float height_deg;
    int ppdx;
    int ppdy;
} tile_t, *ptile_t;

int tile_load_lidar(tile_t *, char *, struct output * out);
int tile_rescale(tile_t *, float);
void tile_destroy(tile_t *);

#endif
