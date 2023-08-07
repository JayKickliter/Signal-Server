#include <bzlib.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <zlib.h>

#include <cassert>
#include <memory>

#include "common.hh"
#include "signal-server.hh"
#include "tiles.hh"

#define BZBUFFER 65536
#define GZBUFFER 32768

extern char *G_color_file;

int loadClutter(char *filename, double radius, struct site tx)
{
    /* This function reads a MODIS 17-class clutter file in ASCII Grid format.
       The nominal heights it applies to each value, eg. 5 (Mixed forest) = 15m are
       taken from ITU-R P.452-11.
       It doesn't have it's own matrix, instead it boosts the DEM matrix like point clutter
       AddElevation(lat, lon, height);
       If tiles are standard 2880 x 3840 then cellsize is constant at 0.004166
     */
    int x, y, z, h = 0, w = 0;
    double clh, xll, yll, cellsize, cellsize2, xOffset, yOffset, lat, lon;
    char line[100000];
    char *s, *pch = NULL;
    FILE *fd;

    if ((fd = fopen(filename, "rb")) == NULL) return errno;

    if (fgets(line, 19, fd) != NULL) {
        pch = strtok(line, " ");
        pch = strtok(NULL, " ");
        w = atoi(pch);
    }

    if (fgets(line, 19, fd) != NULL) {
        pch = strtok(line, " ");
        pch = strtok(NULL, " ");
        h = atoi(pch);
    }

    if (w == 2880 && h == 3840) {
        cellsize = 0.004167;
        cellsize2 = cellsize * 3;
    }
    else {
        if (G_debug) {
            fprintf(stderr, "\nError Loading clutter file, unsupported resolution %d x %d.\n", w, h);
            fflush(stderr);
            fclose(fd);
        }
        return 0;  // can't work with this yet
    }

    if (G_debug) {
        fprintf(stderr, "\nLoading clutter file \"%s\" %d x %d...\n", filename, w, h);
        fflush(stderr);
    }

    if (fgets(line, 25, fd) != NULL) {
        sscanf(pch, "%lf", &xll);
    }

    s = fgets(line, 25, fd);
    if (fgets(line, 25, fd) != NULL) {
        sscanf(pch, "%lf", &yll);
    }

    if (G_debug) {
        fprintf(stderr, "\nxll %.2f yll %.2f\n", xll, yll);
        fflush(stderr);
    }

    s = fgets(line, 25, fd);  // cellsize

    if (s) {
    }

    // loop over matrix
    for (y = h; y > 0; y--) {
        x = 0;
        if (fgets(line, sizeof(line) - 1, fd) != NULL) {
            pch = strtok(line, " ");
            while (pch != NULL && x < w) {
                z = atoi(pch);
                // Apply ITU-R P.452-11
                // Treat classes 0, 9, 10, 11, 15, 16 as water, (Water, savanna, grassland, wetland, snow, barren)
                clh = 0.0;

                // evergreen, evergreen, urban
                if (z == 1 || z == 2 || z == 13) clh = 20.0;
                // deciduous, deciduous, mixed
                if (z == 3 || z == 4 || z == 5) clh = 15.0;
                // woody shrublands & savannas
                if (z == 6 || z == 8) clh = 4.0;
                // shrublands, savannas, croplands...
                if (z == 7 || z == 9 || z == 10 || z == 12 || z == 14) clh = 2.0;

                if (clh > 1) {
                    xOffset = x * cellsize;  // 12 deg wide
                    yOffset = y * cellsize;  // 16 deg high

                    // make all longitudes positive
                    if (xll + xOffset > 0) {
                        lon = 360 - (xll + xOffset);
                    }
                    else {
                        lon = (xll + xOffset) * -1;
                    }
                    lat = yll + yOffset;

                    // bounding box
                    if (lat > tx.lat - radius && lat < tx.lat + radius && lon > tx.lon - radius && lon < tx.lon + radius) {
                        // not in near field
                        if ((lat > tx.lat + cellsize2 || lat < tx.lat - cellsize2) ||
                            (lon > tx.lon + cellsize2 || lon < tx.lon - cellsize2)) {
                            AddElevation(lat, lon, clh, 2);
                        }
                    }
                }

                x++;
                pch = strtok(NULL, " ");
            }  // while
        }
        else {
            fprintf(stderr, "Clutter error @ x %d y %d\n", x, y);
        }  // if
    }      // for

    fclose(fd);
    return 0;
}

/*
int averageHeight(int x, int y){
        int total = 0;
        int c=0;
        if(DEM_INDEX(&(G_dem[0]), x-1, y-1)>0){
                total+=DEM_INDEX(G_dem[0], x-1, y-1);
                c++;
        }
        if(DEM_INDEX(&G_dem[0], x+1, y+1)>0){
                total+=DEM_INDEX(G_dem[0], x+1, y+1);
                c++;
        }
        if(DEM_INDEX(&G_dem[0], x+1, y-1)>0){
                total+=DEM_INDEX(G_dem[0], x+1, y-1);
                c++;
        }
        if(DEM_INDEX(&G_dem[0], x-1, y+1)>0){
                total+=DEM_INDEX(G_dem[0], x-1, y+1);
                c++;
        }
        if(c>0){
                return (int)(total/c);
        }else{
                return 0;
        }
}*/

#if 0
int loadLIDAR(char *filenames, int resample, struct output *out)
{
    char *filename;
    char *files[900];  // 20x20=400, 16x16=256 tiles
    int indx = 0, fc = 0, success;
    double avgCellsize = 0, smCellsize = 0;
    tile_t *tiles;

    // Initialize global variables before processing files
    out->min_west = 361;  // any value will be lower than this
    out->max_west = 0;    // any value will be higher than this

    // test for multiple files
    filename = strtok(filenames, " ,");
    while (filename != NULL) {
        files[fc] = filename;
        filename = strtok(NULL, " ,");
        fc++;
    }

    /* Allocate the tile array */
    if ((tiles = (tile_t *)calloc(fc + 1, sizeof(tile_t))) == NULL) {
        if (G_debug) fprintf(stderr, "Could not allocate %d\n tiles", fc + 1);
        return ENOMEM;
    }

    /* Load each tile in turn */
    for (indx = 0; indx < fc; indx++) {
        /* Grab the tile metadata */
        if ((success = tile_load_lidar(&tiles[indx], files[indx], out)) != 0) {
            fprintf(stderr, "Failed to load LIDAR tile %s\n", files[indx]);
            fflush(stderr);
            free(tiles);
            return success;
        }

        if (G_debug) {
            fprintf(stderr, "Loading \"%s\" into page %d with width %d...\n", files[indx], indx, tiles[indx].width);
            fflush(stderr);
        }

        // Increase the "average" cell size
        avgCellsize += tiles[indx].cellsize;
        // Update the smallest cell size
        if (smCellsize == 0 || tiles[indx].cellsize < smCellsize) {
            smCellsize = tiles[indx].cellsize;
        }

        // Update a bunch of globals
        if (tiles[indx].max_el > out->max_elevation) out->max_elevation = tiles[indx].max_el;
        if (tiles[indx].min_el < out->min_elevation) out->min_elevation = tiles[indx].min_el;

        if (out->max_north == -90 || tiles[indx].max_north > out->max_north) out->max_north = tiles[indx].max_north;

        if (out->min_north == 90 || tiles[indx].min_north < out->min_north) out->min_north = tiles[indx].min_north;

        // Meridian switch. max_west=0
        if (abs(tiles[indx].max_west - out->max_west) < 180 || tiles[indx].max_west < 360) {
            if (tiles[indx].max_west > out->max_west) out->max_west = tiles[indx].max_west;  // update highest value
        }
        else {
            if (tiles[indx].max_west < out->max_west) out->max_west = tiles[indx].max_west;
        }
        if (fabs(tiles[indx].min_west - out->min_west) < 180.0 || tiles[indx].min_west <= 360) {
            if (tiles[indx].min_west < out->min_west) out->min_west = tiles[indx].min_west;
        }
        else {
            if (tiles[indx].min_west > out->min_west) out->min_west = tiles[indx].min_west;
        }
        // Handle tile with 360 XUR
        if (out->min_west > 359) out->min_west = 0.0;
    }

    /* Iterate through all of the tiles to find the smallest resolution. We will
     * need to rescale every tile from here on out to this value */
    float smallest_res = 0;
    for (size_t i = 0; i < (unsigned)fc; i++) {
        if (smallest_res == 0 || tiles[i].resolution < smallest_res) {
            smallest_res = tiles[i].resolution;
        }
    }

    /* Now we need to rescale all tiles the the lowest resolution or the requested resolution. ie if we have
     * one 1m lidar and one 2m lidar, resize the 2m to fake 1m */
    float desired_resolution = resample != 0 && smallest_res < resample ? resample : smallest_res;

    if (resample > 1) {
        desired_resolution = smallest_res * resample;
    }

    // Don't resize large 1 deg tiles in large multi-degree plots as it gets messy
    if (tiles[0].width != 3600) {
        for (size_t i = 0; i < (unsigned)fc; i++) {
            float rescale = tiles[i].resolution / (float)desired_resolution;
            if (G_debug) {
                fprintf(stderr, "res %.5f desired_res %.5f\n", tiles[i].resolution, (float)desired_resolution);
                fflush(stderr);
            }
            if (rescale != 1) {
                if ((success = tile_rescale(&tiles[i], rescale) != 0)) {
                    fprintf(stderr, "Error resampling tiles\n");
                    fflush(stderr);
                    return success;
                }
            }
        }
    }

    /* Now we work out the size of the giant lidar tile. */
    if (G_debug) {
        fprintf(stderr, "mw:%lf Mnw:%lf\n", out->max_west, out->min_west);
    }
    double total_width =
        out->max_west - out->min_west >= 0 ? out->max_west - out->min_west : out->max_west + (360 - out->min_west);
    double total_height = out->max_north - out->min_north;
    if (G_debug) {
        fprintf(stderr, "totalh: %.7f - %.7f = %.7f totalw: %.7f - %.7f = %.7f fc: %d\n", out->max_north, out->min_north,
                total_height, out->max_west, out->min_west, total_width, fc);
    }

    // detect problematic layouts eg. vertical rectangles
    //  1x2
    if (fc >= 2 && desired_resolution < 28 && total_height > total_width * 1.5) {
        tiles[fc].max_north = out->max_north;
        tiles[fc].min_north = out->min_north;
        out->westoffset = out->westoffset - (total_height - total_width);  // WGS84 for stdout only
        out->max_west = out->max_west + (total_height - total_width);      // Positive westing
        tiles[fc].max_west = out->max_west;                                // Positive westing
        tiles[fc].min_west = out->max_west;
        tiles[fc].ppdy = tiles[fc - 1].ppdy;
        tiles[fc].ppdy = tiles[fc - 1].ppdx;
        tiles[fc].width = (total_height - total_width);
        tiles[fc].height = total_height;
        tiles[fc].data = tiles[fc - 1].data;
        fc++;

        // calculate deficit

        if (G_debug) {
            fprintf(stderr, "deficit: %.4f cellsize: %.9f tiles needed to square: %.1f, desired_resolution %f\n",
                    total_width - total_height, avgCellsize, (total_width - total_height) / avgCellsize,
                    (float)desired_resolution);
            fflush(stderr);
        }
    }
    // 2x1
    if (fc >= 2 && desired_resolution < 28 && total_width > total_height * 1.5) {
        tiles[fc].max_north = out->max_north + (total_width - total_height);
        tiles[fc].min_north = out->max_north;
        tiles[fc].max_west = out->max_west;                              // Positive westing
        out->max_north = out->max_north + (total_width - total_height);  // Positive westing
        tiles[fc].min_west = out->min_west;
        tiles[fc].ppdy = tiles[fc - 1].ppdy;
        tiles[fc].ppdy = tiles[fc - 1].ppdx;
        tiles[fc].width = total_width;
        tiles[fc].height = (total_width - total_height);
        tiles[fc].data = tiles[fc - 1].data;
        fc++;

        // calculate deficit

        if (G_debug) {
            fprintf(stderr, "deficit: %.4f cellsize: %.9f tiles needed to square: %.1f\n", total_width - total_height,
                    avgCellsize, (total_width - total_height) / avgCellsize);
            fflush(stdout);
        }
    }
    size_t new_height = 0;
    size_t new_width = 0;
    for (size_t i = 0; i < (unsigned)fc; i++) {
        double north_offset = out->max_north - tiles[i].max_north;
        double west_offset = out->max_west - tiles[i].max_west >= 0 ? out->max_west - tiles[i].max_west
                                                                    : out->max_west + (360 - tiles[i].max_west);
        size_t north_pixel_offset = north_offset * tiles[i].ppdy;
        size_t west_pixel_offset = west_offset * tiles[i].ppdx;

        if (west_pixel_offset + tiles[i].width > new_width) new_width = west_pixel_offset + tiles[i].width;
        if (north_pixel_offset + tiles[i].height > new_height) new_height = north_pixel_offset + tiles[i].height;
        if (G_debug)
            fprintf(stderr, "north_pixel_offset %zu west_pixel_offset %zu, %zu x %zu\n", north_pixel_offset, west_pixel_offset,
                    new_height, new_width);

        // sanity check!
        if (new_width > 39e3 || new_height > 39e3) {
            fprintf(stdout, "Not processing a tile with these dimensions: %zu x %zu\n", new_width, new_height);
            exit(1);
        }
    }

    size_t new_tile_alloc = new_width * new_height;
    short *new_tile = (short *)calloc(new_tile_alloc, sizeof(short));

    if (new_tile == NULL) {
        if (G_debug) {
            fprintf(stderr, "Could not allocate %zu bytes\n", new_tile_alloc);
            fflush(stderr);
        }
        free(tiles);
        return ENOMEM;
    }
    if (G_debug) {
        fprintf(stderr, "Lidar tile dimensions w:%lf(%zu) h:%lf(%zu)\n", total_width, new_width, total_height, new_height);
        fflush(stderr);
    }

    /* ...If we wanted a value other than sea level here, we would
       need to initialize the array... */

    /* Fill out the array one tile at a time */
    for (size_t i = 0; i < (unsigned)fc; i++) {
        double north_offset = out->max_north - tiles[i].max_north;
        double west_offset = out->max_west - tiles[i].max_west >= 0 ? out->max_west - tiles[i].max_west
                                                                    : out->max_west + (360 - tiles[i].max_west);
        size_t north_pixel_offset = north_offset * tiles[i].ppdy;
        size_t west_pixel_offset = west_offset * tiles[i].ppdx;

        if (G_debug) {
            fprintf(stderr, "mn: %lf mw:%lf globals: %lf %lf\n", tiles[i].max_north, tiles[i].max_west, out->max_north,
                    out->max_west);
            fprintf(stderr, "Offset n:%zu(%lf) w:%zu(%lf)\n", north_pixel_offset, north_offset, west_pixel_offset, west_offset);
            fprintf(stderr, "Height: %d\n", tiles[i].height);
            fflush(stderr);
        }

        /* Copy it row-by-row from the tile */
        for (size_t h = 0; h < (unsigned)tiles[i].height; h++) {
            short *dest_addr = &new_tile[(north_pixel_offset + h) * new_width + west_pixel_offset];
            short *src_addr = &tiles[i].data[h * tiles[i].width];
            // Check if we might overflow
            if (dest_addr + tiles[i].width > new_tile + new_tile_alloc || dest_addr < new_tile) {
                if (G_debug) {
                    fprintf(stderr, "Overflow %zu\n", i);
                    fflush(stderr);
                }
                continue;
            }
            memcpy(dest_addr, src_addr, tiles[i].width * sizeof(short));
        }
    }

    // SUPER tile
    MAXPAGES = 1;
    IPPD = MAX(new_width, new_height);
    G_ippd = IPPD;

    ARRAYSIZE = (MAXPAGES * IPPD) + 10;

    out->height = new_height;
    out->width = new_width;

    if (G_debug) {
        fprintf(stderr, "Setting IPPD to %d height %d width %d\n", IPPD, out->height, out->width);
        fflush(stderr);
    }

    /* Load the data into the global dem array */
    G_dem[0].max_north = out->max_north;
    G_dem[0].min_west = out->min_west;
    G_dem[0].min_north = out->min_north;
    G_dem[0].max_west = out->max_west;
    G_dem[0].max_el = out->max_elevation;
    G_dem[0].min_el = out->min_elevation;

    /*
     * Copy the lidar tile data into the dem array. The dem array is then rotated
     * 90 degrees(!)...it's a legacy thing.
     */
    int y = new_height - 1;
    for (size_t h = 0; h < new_height; h++, y--) {
        int x = new_width - 1;
        for (size_t w = 0; w < new_width; w++, x--) {
            G_dem[0].data[DEM_INDEX(G_dem[0].ippd, x, y)] = new_tile[h * new_width + w];
        }
    }

    // Polyfilla for warped tiles
    y = new_height - 2;
    for (size_t h = 0; h < new_height - 2; h++, y--) {
        int x = new_width - 2;
        for (size_t w = 0; w < new_width - 2; w++, x--) {
            /*if(G_dem[0].data[DEM_INDEX(G_dem[0], x, y)]<=0){
                    G_dem[0].data[DEM_INDEX(G_dem[0], x, y)] = averageHeight(x,y);
            }*/
        }
    }
    if (out->width > 3600 * 8) {
        fprintf(stdout, "DEM fault. Contact system administrator: %d\n", out->width);
        fflush(stderr);
        exit(1);
    }

    if (G_debug) {
        fprintf(stderr, "LIDAR LOADED %d x %d\n", out->width, out->height);
        fprintf(stderr, "fc %d WIDTH %d HEIGHT %d ippd %d minN %.5f maxN %.5f minW %.5f maxW %.5f avgCellsize %.5f\n", fc,
                out->width, out->height, G_ippd, out->min_north, out->max_north, out->min_west, out->max_west, avgCellsize);
        fflush(stderr);
    }

    if (tiles != NULL)
        for (size_t i = 0; i < (unsigned)fc - 1; i++) tile_destroy(&tiles[i]);
    free(tiles);

    return 0;
}
#endif

int LoadSDF_BSDF(char *name, struct output *out)
{
    /* This function reads uncompressed ss Data Files (.sdf)
       containing digital elevation model data into memory.
       Elevation data, maximum and minimum elevations, and
       quadrangle limits are stored in the first available
       dem[] structure.
       NOTE: On error, this function returns a negative errno */

    int x, minlat, minlon, maxlat, maxlon;
    char found = 0, sdf_file[255], path_plus_name[PATH_MAX];

    int fd;

    for (x = 0; name[x] != '.' && name[x] != 0 && x < 249; x++) sdf_file[x] = name[x];

    sdf_file[x] = 0;

    /* Parse filename for minimum latitude and longitude values */

    if (sscanf(sdf_file, "%d:%d:%d:%d", &minlat, &maxlat, &minlon, &maxlon) != 4) return -EINVAL;

    sdf_file[x] = '.';
    sdf_file[x + 1] = 'b';
    sdf_file[x + 2] = 's';
    sdf_file[x + 3] = 'd';
    sdf_file[x + 4] = 'f';
    sdf_file[x + 5] = 0;

    /* Is it already in memory? */
    {
        /* Hold read lock on G_dem while iterating through the vec. */
        std::shared_lock r_lock(G_dem_mtx);

        for (auto const &dem : G_dem) {
            if (minlat == dem->min_north && minlon == dem->min_west && maxlat == dem->max_north && maxlon == dem->max_west) {
                found = 1;

                if (out) {
                    // TODO copy-pasted from below, dedup
                    if (dem->min_el < out->min_elevation) out->min_elevation = dem->min_el;

                    if (dem->max_el > out->max_elevation) out->max_elevation = dem->max_el;

                    if (out->max_north == -90)
                        out->max_north = dem->max_north;

                    else if (dem->max_north > out->max_north)
                        out->max_north = dem->max_north;

                    if (out->min_north == 90)
                        out->min_north = dem->min_north;

                    else if (dem->min_north < out->min_north)
                        out->min_north = dem->min_north;

                    if (out->max_west == -1)
                        out->max_west = dem->max_west;

                    else {
                        if (abs(dem->max_west - out->max_west) < 180) {
                            if (dem->max_west > out->max_west) out->max_west = dem->max_west;
                        }

                        else {
                            if (dem->max_west < out->max_west) out->max_west = dem->max_west;
                        }
                    }

                    if (out->min_west == 360)
                        out->min_west = dem->min_west;

                    else {
                        if (fabs(dem->min_west - out->min_west) < 180.0) {
                            if (dem->min_west < out->min_west) out->min_west = dem->min_west;
                        }

                        else {
                            if (dem->min_west > out->min_west) out->min_west = dem->min_west;
                        }
                    }
                }
                break;
            }
        }
    }

    if (found == 0) {
        /* Search for SDF file in current working directory first */

        strncpy(path_plus_name, sdf_file, sizeof(path_plus_name) - 1);

        if ((fd = open(path_plus_name, O_RDONLY)) == -1) {
            /* Next, try loading SDF file from path specified
               in $HOME/.ss_path file or by -d argument */

            strncpy(path_plus_name, G_sdf_path, sizeof(path_plus_name) - 1);
            strncat(path_plus_name, sdf_file, sizeof(path_plus_name) - 1);
            if ((fd = open(path_plus_name, O_RDONLY)) == -1) {
                return -errno;
            }
        }

        if (G_debug == 1) {
            fprintf(stderr, "Loading \"%s\"...\n", path_plus_name);
            fflush(stderr);
        }

        /*
         * ┌─────────────────────────────────────┐
         * │                                     │
         * │                                     │
         * │                                     │
         * │                                     │
         * │                                     │
         * │    IPPD^2 i16 elevation samples     │
         * │           (little-endian)           │
         * │                                     │
         * │                                     │
         * │                                     │
         * │                                     │
         * │                                     │
         * │                                     │
         * ├─────────────────────────────────────┤
         * │              u16 IPPD               │
         * │           (little-endian)           │
         * ├─────────────────────────────────────┤
         * │        i16 minimum elevation        │
         * │           (little-endian)           │
         * ├─────────────────────────────────────┤
         * │        i16 maximum elevation        │
         * │           (little-endian)           │
         * ├─────────────────────────────────────┤
         * │             u16 Version             │
         * │           (little-endian)           │
         * └─────────────────────────────────────┘
         */

        struct FooterV0 {
            uint16_t ippd;
            int16_t min_el;
            int16_t max_el;

            /** Parses this from `fd`.
             *
             * @returns 0 on success
             * @returns a negative value on error
             */
            int read(int fd)
            {
                uint16_t version = UINT16_MAX;
                if (lseek(fd, -2, SEEK_END) == -1) {
                    return -errno;
                }
                if (::read(fd, &version, sizeof(version)) != sizeof(version)) {
                    return -errno;
                }
                if (version != 0) {
                    return -1;
                }
                if (lseek(fd, -8, SEEK_END) == -1) {
                    return -errno;
                }
                if (::read(fd, &this->ippd, sizeof(this->ippd)) != sizeof(this->ippd)) {
                    return -errno;
                }
                assert(this->ippd == 1200 || this->ippd == 3600);
                if (::read(fd, &this->min_el, sizeof(this->min_el)) != sizeof(this->min_el)) {
                    return -errno;
                }
                if (::read(fd, &this->max_el, sizeof(this->max_el)) != sizeof(this->max_el)) {
                    return -errno;
                }
                return 0;
            }
        };

        int parse_res;
        FooterV0 footer;
        if ((parse_res = footer.read(fd)) != 0) {
            close(fd);
            return parse_res;
        }

        struct dem dem;
        dem.min_north = minlat;
        dem.min_west = minlon;
        dem.max_north = maxlat;
        dem.max_west = maxlon;
        dem.ippd = footer.ippd;
        dem.min_el = footer.min_el;
        dem.max_el = footer.max_el;

        /* TODO: need to seek before mapping? */
        lseek(fd, 0, SEEK_SET);
        dem.data = (short *)mmap(NULL, sizeof(int16_t) * dem.ippd * dem.ippd, PROT_READ, MAP_PRIVATE, fd, 0);

        close(fd);
        if (out) {
            if (dem.min_el < out->min_elevation) out->min_elevation = dem.min_el;

            if (dem.max_el > out->max_elevation) out->max_elevation = dem.max_el;

            if (out->max_north == -90)
                out->max_north = dem.max_north;

            else if (dem.max_north > out->max_north)
                out->max_north = dem.max_north;

            if (out->min_north == 90)
                out->min_north = dem.min_north;

            else if (dem.min_north < out->min_north)
                out->min_north = dem.min_north;

            if (out->max_west == -1)
                out->max_west = dem.max_west;

            else {
                if (abs(dem.max_west - out->max_west) < 180) {
                    if (dem.max_west > out->max_west) out->max_west = dem.max_west;
                }

                else {
                    if (dem.max_west < out->max_west) out->max_west = dem.max_west;
                }
            }

            if (out->min_west == 360)
                out->min_west = dem.min_west;

            else {
                if (fabs(dem.min_west - out->min_west) < 180.0) {
                    if (dem.min_west < out->min_west) out->min_west = dem.min_west;
                }

                else {
                    if (dem.min_west > out->min_west) out->min_west = dem.min_west;
                }
            }
        }

        std::unique_lock lock(G_dem_mtx);
        std::shared_ptr<const struct dem> s_dem(new struct dem(dem));
        G_dem.push_back(s_dem);

        return 1;
    }

    else
        return found;
}

int LoadSDF(char *name, struct output *out)
{
    /* This function loads the requested SDF file from the filesystem.
       It first tries to invoke the LoadSDF_SDF() function to load an
       uncompressed SDF file (since uncompressed files load slightly
       faster).  If that attempt fails, then it tries to load a
       compressed SDF file by invoking the LoadSDF_BZ() function.
       If that attempt fails, then it tries again to load a
       compressed SDF file by invoking the LoadSDF_GZ() function.
       If that fails, then we can assume that no elevation data
       exists for the region requested, and that the region
       requested must be entirely over water. */

    int minlat, minlon, maxlat, maxlon;
    char found = 0;
    int return_value = -1;

    return_value = LoadSDF_BSDF(name, out);

    /* Try to load an uncompressed SDF first. */

    // return_value = LoadSDF_SDF(name);

    /* If that fails, try loading a BZ2 compressed SDF. */

    // if ( return_value <= 0 )
    //         return_value = LoadSDF_BZ(name);

    /* If that fails, try loading a gzip compressed SDF. */

    // if ( return_value <= 0 )
    //         return_value = LoadSDF_GZ(name);

    /* If no file format can be found, then assume the area is water. */

    if (return_value <= 0) {
        sscanf(name, "%d:%d:%d:%d", &minlat, &maxlat, &minlon, &maxlon);

        /* Is it already in memory? */
        {
            /* Hold an RAII read lock on G_dem while iterating through the vec. */
            std::shared_lock r_lock(G_dem_mtx);
            for (auto const &dem : G_dem) {
                if (minlat == dem->min_north && minlon == dem->min_west && maxlat == dem->max_north &&
                    maxlon == dem->max_west) {
                    found = 1;
                    break;
                }
            }
        }

        if (found == 0) {
            if (G_debug == 1) {
                fprintf(stderr, "Region  \"%s\" assumed as sea-level...\n", name);
                fflush(stderr);
            }

            struct dem dem;
            dem.ippd = 1200;
            dem.max_west = maxlon;
            dem.min_north = minlat;
            dem.min_west = minlon;
            dem.max_north = maxlat;
            dem.min_el = 0;
            dem.max_el = 0;

            /* Fill DEM with sea-level topography */
            dem.data = new short[IPPD * IPPD];
            bzero(dem.data, IPPD * IPPD * sizeof(short));

            if (out) {
                if (dem.min_el > 0) dem.min_el = 0;

                if (dem.min_el < out->min_elevation) out->min_elevation = dem.min_el;

                if (dem.max_el > out->max_elevation) out->max_elevation = dem.max_el;

                if (out->max_north == -90)
                    out->max_north = dem.max_north;

                else if (dem.max_north > out->max_north)
                    out->max_north = dem.max_north;

                if (out->min_north == 90)
                    out->min_north = dem.min_north;

                else if (dem.min_north < out->min_north)
                    out->min_north = dem.min_north;

                if (out->max_west == -1)
                    out->max_west = dem.max_west;

                else {
                    if (abs(dem.max_west - out->max_west) < 180) {
                        if (dem.max_west > out->max_west) out->max_west = dem.max_west;
                    }

                    else {
                        if (dem.max_west < out->max_west) out->max_west = dem.max_west;
                    }
                }

                if (out->min_west == 360)
                    out->min_west = dem.min_west;

                else {
                    if (abs(dem.min_west - out->min_west) < 180) {
                        if (dem.min_west < out->min_west) out->min_west = dem.min_west;
                    }

                    else {
                        if (dem.min_west > out->min_west) out->min_west = dem.min_west;
                    }
                }
            }

            std::unique_lock lock(G_dem_mtx);
            std::shared_ptr<const struct dem> s_dem(new struct dem(dem));
            G_dem.push_back(s_dem);

            return_value = 1;
        }
    }

    return return_value;
}

int LoadPAT(char *az_filename, char *el_filename, struct LR &lr)
{
    /* This function reads and processes antenna pattern (.az
       and .el) files that may correspond in name to previously
       loaded ss .lrp files or may be user-supplied by cmdline.  */

    int a, b, w, x, y, z, last_index, next_index, span;
    char string[255], *pointer = NULL;
    float az, xx, elevation, amplitude, rotation, valid1, valid2, delta, azimuth[361], azimuth_pattern[361], el_pattern[10001],
        elevation_pattern[361][1001], slant_angle[361], tilt, mechanical_tilt = 0.0, tilt_azimuth, tilt_increment, sum;
    FILE *fd = NULL;
    unsigned char read_count[10001];

    rotation = 0.0;

    G_got_azimuth_pattern = 0;
    G_got_elevation_pattern = 0;

    /* Load .az antenna pattern file */

    if (az_filename != NULL && (fd = fopen(az_filename, "r")) == NULL && errno != ENOENT)
        /* Any error other than file not existing is an error */
        return errno;

    if (fd != NULL) {
        if (G_debug) {
            fprintf(stderr, "\nAntenna Pattern Azimuth File = [%s]\n", az_filename);
            fflush(stderr);
        }

        /* Clear azimuth pattern array */
        for (x = 0; x <= 360; x++) {
            azimuth[x] = 0.0;
            read_count[x] = 0;
        }

        /* Read azimuth pattern rotation
           in degrees measured clockwise
           from true North. */

        if (fgets(string, 254, fd) == NULL) {
            // fprintf(stderr,"Azimuth read error\n");
            // exit(0);
        }
        pointer = strchr(string, ';');

        if (pointer != NULL) *pointer = 0;

        if (lr.antenna_rotation != -1)  // If cmdline override
            rotation = (float)lr.antenna_rotation;
        else
            sscanf(string, "%f", &rotation);

        if (G_debug) {
            fprintf(stderr, "Antenna Pattern Rotation = %f\n", rotation);
            fflush(stderr);
        }
        /* Read azimuth (degrees) and corresponding
           normalized field radiation pattern amplitude
           (0.0 to 1.0) until EOF is reached. */

        if (fgets(string, 254, fd) == NULL) {
            // fprintf(stderr,"Azimuth read error\n");
            // exit(0);
        }
        pointer = strchr(string, ';');

        if (pointer != NULL) *pointer = 0;

        sscanf(string, "%f %f", &az, &amplitude);

        do {
            x = (int)rintf(az);

            if (x >= 0 && x <= 360 && fd != NULL) {
                azimuth[x] += amplitude;
                read_count[x]++;
            }

            if (fgets(string, 254, fd) == NULL) {
                // fprintf(stderr,"Azimuth read error\n");
                //  exit(0);
            }
            pointer = strchr(string, ';');

            if (pointer != NULL) *pointer = 0;

            sscanf(string, "%f %f", &az, &amplitude);

        } while (feof(fd) == 0);

        fclose(fd);
        fd = NULL;

        /* Handle 0=360 degree ambiguity */

        if ((read_count[0] == 0) && (read_count[360] != 0)) {
            read_count[0] = read_count[360];
            azimuth[0] = azimuth[360];
        }

        if ((read_count[0] != 0) && (read_count[360] == 0)) {
            read_count[360] = read_count[0];
            azimuth[360] = azimuth[0];
        }

        /* Average pattern values in case more than
           one was read for each degree of azimuth. */

        for (x = 0; x <= 360; x++) {
            if (read_count[x] > 1) azimuth[x] /= (float)read_count[x];
        }

        /* Interpolate missing azimuths
           to completely fill the array */

        last_index = -1;
        next_index = -1;

        for (x = 0; x <= 360; x++) {
            if (read_count[x] != 0) {
                if (last_index == -1)
                    last_index = x;
                else
                    next_index = x;
            }

            if (last_index != -1 && next_index != -1) {
                valid1 = azimuth[last_index];
                valid2 = azimuth[next_index];

                span = next_index - last_index;
                delta = (valid2 - valid1) / (float)span;

                for (y = last_index + 1; y < next_index; y++) azimuth[y] = azimuth[y - 1] + delta;

                last_index = y;
                next_index = -1;
            }
        }

        /* Perform azimuth pattern rotation
           and load azimuth_pattern[361] with
           azimuth pattern data in its final form. */

        for (x = 0; x < 360; x++) {
            y = x + (int)rintf(rotation);

            if (y >= 360) y -= 360;

            azimuth_pattern[y] = azimuth[x];
        }

        azimuth_pattern[360] = azimuth_pattern[0];

        G_got_azimuth_pattern = 255;
    }

    /* Read and process .el file */

    if (el_filename != NULL && (fd = fopen(el_filename, "r")) == NULL && errno != ENOENT)
        /* Any error other than file not existing is an error */
        return errno;

    if (fd != NULL) {
        if (G_debug) {
            fprintf(stderr, "Antenna Pattern Elevation File = [%s]\n", el_filename);
            fflush(stderr);
        }

        /* Clear azimuth pattern array */

        for (x = 0; x <= 10000; x++) {
            el_pattern[x] = 0.0;
            read_count[x] = 0;
        }

        /* Read mechanical tilt (degrees) and
           tilt azimuth in degrees measured
           clockwise from true North. */

        if (fgets(string, 254, fd) == NULL) {
            // fprintf(stderr,"Tilt read error\n");
            // exit(0);
        }
        pointer = strchr(string, ';');

        if (pointer != NULL) *pointer = 0;

        sscanf(string, "%f %f", &mechanical_tilt, &tilt_azimuth);

        if (lr.antenna_downtilt != 99.0) {      // If Cmdline override
            if (lr.antenna_dt_direction == -1)  // dt_dir not specified
                tilt_azimuth = rotation;        // use rotation value
            mechanical_tilt = (float)lr.antenna_downtilt;
        }

        if (lr.antenna_dt_direction != -1)  // If Cmdline override
            tilt_azimuth = (float)lr.antenna_dt_direction;

        if (G_debug) {
            fprintf(stderr, "Antenna Pattern Mechamical Downtilt = %f\n", mechanical_tilt);
            fprintf(stderr, "Antenna Pattern Mechanical Downtilt Direction = %f\n\n", tilt_azimuth);
            fflush(stderr);
        }

        /* Read elevation (degrees) and corresponding
           normalized field radiation pattern amplitude
           (0.0 to 1.0) until EOF is reached. */

        if (fgets(string, 254, fd) == NULL) {
            // fprintf(stderr,"Ant elevation read error\n");
            // exit(0);
        }
        pointer = strchr(string, ';');

        if (pointer != NULL) *pointer = 0;

        sscanf(string, "%f %f", &elevation, &amplitude);

        while (feof(fd) == 0) {
            /* Read in normalized radiated field values
               for every 0.01 degrees of elevation between
               -10.0 and +90.0 degrees */

            x = (int)rintf(100.0 * (elevation + 10.0));

            if (x >= 0 && x <= 10000) {
                el_pattern[x] += amplitude;
                read_count[x]++;
            }

            if (fgets(string, 254, fd) != NULL) {
                pointer = strchr(string, ';');
            }
            if (pointer != NULL) *pointer = 0;

            sscanf(string, "%f %f", &elevation, &amplitude);
        }

        fclose(fd);

        /* Average the field values in case more than
           one was read for each 0.01 degrees of elevation. */

        for (x = 0; x <= 10000; x++) {
            if (read_count[x] > 1) el_pattern[x] /= (float)read_count[x];
        }

        /* Interpolate between missing elevations (if
           any) to completely fill the array and provide
           radiated field values for every 0.01 degrees of
           elevation. */

        last_index = -1;
        next_index = -1;

        for (x = 0; x <= 10000; x++) {
            if (read_count[x] != 0) {
                if (last_index == -1)
                    last_index = x;
                else
                    next_index = x;
            }

            if (last_index != -1 && next_index != -1) {
                valid1 = el_pattern[last_index];
                valid2 = el_pattern[next_index];

                span = next_index - last_index;
                delta = (valid2 - valid1) / (float)span;

                for (y = last_index + 1; y < next_index; y++) el_pattern[y] = el_pattern[y - 1] + delta;

                last_index = y;
                next_index = -1;
            }
        }

        /* Fill slant_angle[] array with offset angles based
           on the antenna's mechanical beam tilt (if any)
           and tilt direction (azimuth). */

        if (mechanical_tilt == 0.0) {
            for (x = 0; x <= 360; x++) slant_angle[x] = 0.0;
        }

        else {
            tilt_increment = mechanical_tilt / 90.0;

            for (x = 0; x <= 360; x++) {
                xx = (float)x;
                y = (int)rintf(tilt_azimuth + xx);

                while (y >= 360) y -= 360;

                while (y < 0) y += 360;

                if (x <= 180) slant_angle[y] = -(tilt_increment * (90.0 - xx));

                if (x > 180) slant_angle[y] = -(tilt_increment * (xx - 270.0));
            }
        }

        slant_angle[360] = slant_angle[0]; /* 360 degree wrap-around */

        for (w = 0; w <= 360; w++) {
            tilt = slant_angle[w];

            /** Convert tilt angle to
                    an array index offset **/

            y = (int)rintf(100.0 * tilt);

            /* Copy shifted el_pattern[10001] field
               values into elevation_pattern[361][1001]
               at the corresponding azimuth, downsampling
               (averaging) along the way in chunks of 10. */

            for (x = y, z = 0; z <= 1000; x += 10, z++) {
                for (sum = 0.0, a = 0; a < 10; a++) {
                    b = a + x;

                    if (b >= 0 && b <= 10000) sum += el_pattern[b];
                    if (b < 0) sum += el_pattern[0];
                    if (b > 10000) sum += el_pattern[10000];
                }

                elevation_pattern[w][z] = sum / 10.0;
            }
        }

        G_got_elevation_pattern = 255;

        for (x = 0; x <= 360; x++) {
            for (y = 0; y <= 1000; y++) {
                if (G_got_elevation_pattern)
                    elevation = elevation_pattern[x][y];
                else
                    elevation = 1.0;

                if (G_got_azimuth_pattern)
                    az = azimuth_pattern[x];
                else
                    az = 1.0;

                lr.ant_pat(x, y) = az * elevation;
            }
        }
    }
    return 0;
}

int LoadSignalColors(struct site xmtr)
{
    int x, y, ok, val[4];
    char filename[255], string[80], *pointer = NULL, *s;
    FILE *fd = NULL;

    if (G_color_file != NULL && G_color_file[0] != 0)
        for (x = 0; G_color_file[x] != '.' && G_color_file[x] != 0 && x < 250; x++) filename[x] = G_color_file[x];
    else
        for (x = 0; xmtr.filename[x] != '.' && xmtr.filename[x] != 0 && x < 250; x++) filename[x] = xmtr.filename[x];

    filename[x] = '.';
    filename[x + 1] = 's';
    filename[x + 2] = 'c';
    filename[x + 3] = 'f';
    filename[x + 4] = 0;

    /* Default values */

    G_region.level[0] = 128;
    G_region.color[0][0] = 255;
    G_region.color[0][1] = 0;
    G_region.color[0][2] = 0;

    G_region.level[1] = 118;
    G_region.color[1][0] = 255;
    G_region.color[1][1] = 165;
    G_region.color[1][2] = 0;

    G_region.level[2] = 108;
    G_region.color[2][0] = 255;
    G_region.color[2][1] = 206;
    G_region.color[2][2] = 0;

    G_region.level[3] = 98;
    G_region.color[3][0] = 255;
    G_region.color[3][1] = 255;
    G_region.color[3][2] = 0;

    G_region.level[4] = 88;
    G_region.color[4][0] = 184;
    G_region.color[4][1] = 255;
    G_region.color[4][2] = 0;

    G_region.level[5] = 78;
    G_region.color[5][0] = 0;
    G_region.color[5][1] = 255;
    G_region.color[5][2] = 0;

    G_region.level[6] = 68;
    G_region.color[6][0] = 0;
    G_region.color[6][1] = 208;
    G_region.color[6][2] = 0;

    G_region.level[7] = 58;
    G_region.color[7][0] = 0;
    G_region.color[7][1] = 196;
    G_region.color[7][2] = 196;

    G_region.level[8] = 48;
    G_region.color[8][0] = 0;
    G_region.color[8][1] = 148;
    G_region.color[8][2] = 255;

    G_region.level[9] = 38;
    G_region.color[9][0] = 80;
    G_region.color[9][1] = 80;
    G_region.color[9][2] = 255;

    G_region.level[10] = 28;
    G_region.color[10][0] = 0;
    G_region.color[10][1] = 38;
    G_region.color[10][2] = 255;

    G_region.level[11] = 18;
    G_region.color[11][0] = 142;
    G_region.color[11][1] = 63;
    G_region.color[11][2] = 255;

    G_region.level[12] = 8;
    G_region.color[12][0] = 140;
    G_region.color[12][1] = 0;
    G_region.color[12][2] = 128;

    G_region.levels = 13;

    /* Don't save if we don't have an output file */
    if ((fd = fopen(filename, "r")) == NULL && xmtr.filename[0] == '\0') return 0;

    if (fd == NULL) {
        if ((fd = fopen(filename, "w")) == NULL) return errno;

        for (x = 0; x < G_region.levels; x++)
            fprintf(fd, "%3d: %3d, %3d, %3d\n", G_region.level[x], G_region.color[x][0], G_region.color[x][1],
                    G_region.color[x][2]);

        fclose(fd);
    }

    else {
        x = 0;
        s = fgets(string, 80, fd);

        if (s) {
        }

        while (x < 128 && feof(fd) == 0) {
            pointer = strchr(string, ';');

            if (pointer != NULL) *pointer = 0;

            ok = sscanf(string, "%d: %d, %d, %d", &val[0], &val[1], &val[2], &val[3]);

            if (ok == 4) {
                if (G_debug) {
                    fprintf(stderr, "\nLoadSignalColors() %d: %d, %d, %d\n", val[0], val[1], val[2], val[3]);
                    fflush(stderr);
                }

                for (y = 0; y < 4; y++) {
                    if (val[y] > 255) val[y] = 255;

                    if (val[y] < 0) val[y] = 0;
                }

                G_region.level[x] = val[0];
                G_region.color[x][0] = val[1];
                G_region.color[x][1] = val[2];
                G_region.color[x][2] = val[3];
                x++;
            }

            s = fgets(string, 80, fd);
        }

        fclose(fd);
        G_region.levels = x;
    }
    return 0;
}

int LoadLossColors(struct site xmtr)
{
    int x, y, ok, val[4];
    char filename[255], string[80], *pointer = NULL, *s;
    FILE *fd = NULL;

    if (G_color_file != NULL && G_color_file[0] != 0)
        for (x = 0; G_color_file[x] != '.' && G_color_file[x] != 0 && x < 250; x++) filename[x] = G_color_file[x];
    else
        for (x = 0; xmtr.filename[x] != '.' && xmtr.filename[x] != 0 && x < 250; x++) filename[x] = xmtr.filename[x];

    filename[x] = '.';
    filename[x + 1] = 'l';
    filename[x + 2] = 'c';
    filename[x + 3] = 'f';
    filename[x + 4] = 0;

    /* Default values */

    G_region.level[0] = 80;
    G_region.color[0][0] = 255;
    G_region.color[0][1] = 0;
    G_region.color[0][2] = 0;

    G_region.level[1] = 90;
    G_region.color[1][0] = 255;
    G_region.color[1][1] = 128;
    G_region.color[1][2] = 0;

    G_region.level[2] = 100;
    G_region.color[2][0] = 255;
    G_region.color[2][1] = 165;
    G_region.color[2][2] = 0;

    G_region.level[3] = 110;
    G_region.color[3][0] = 255;
    G_region.color[3][1] = 206;
    G_region.color[3][2] = 0;

    G_region.level[4] = 120;
    G_region.color[4][0] = 255;
    G_region.color[4][1] = 255;
    G_region.color[4][2] = 0;

    G_region.level[5] = 130;
    G_region.color[5][0] = 184;
    G_region.color[5][1] = 255;
    G_region.color[5][2] = 0;

    G_region.level[6] = 140;
    G_region.color[6][0] = 0;
    G_region.color[6][1] = 255;
    G_region.color[6][2] = 0;

    G_region.level[7] = 150;
    G_region.color[7][0] = 0;
    G_region.color[7][1] = 208;
    G_region.color[7][2] = 0;

    G_region.level[8] = 160;
    G_region.color[8][0] = 0;
    G_region.color[8][1] = 196;
    G_region.color[8][2] = 196;

    G_region.level[9] = 170;
    G_region.color[9][0] = 0;
    G_region.color[9][1] = 148;
    G_region.color[9][2] = 255;

    G_region.level[10] = 180;
    G_region.color[10][0] = 80;
    G_region.color[10][1] = 80;
    G_region.color[10][2] = 255;

    G_region.level[11] = 190;
    G_region.color[11][0] = 0;
    G_region.color[11][1] = 38;
    G_region.color[11][2] = 255;

    G_region.level[12] = 200;
    G_region.color[12][0] = 142;
    G_region.color[12][1] = 63;
    G_region.color[12][2] = 255;

    G_region.level[13] = 210;
    G_region.color[13][0] = 196;
    G_region.color[13][1] = 54;
    G_region.color[13][2] = 255;

    G_region.level[14] = 220;
    G_region.color[14][0] = 255;
    G_region.color[14][1] = 0;
    G_region.color[14][2] = 255;

    G_region.level[15] = 230;
    G_region.color[15][0] = 255;
    G_region.color[15][1] = 194;
    G_region.color[15][2] = 204;

    G_region.levels = 16;
    /*	region.levels = 120; // 240dB max PL */

    /*	for(int i=0; i<region.levels;i++){
                    region.level[i] = i*2;
                    region.color[i][0] = i*2;
                    region.color[i][1] = i*2;
                    region.color[i][2] = i*2;
            }
    */
    /* Don't save if we don't have an output file */
    if ((fd = fopen(filename, "r")) == NULL && xmtr.filename[0] == '\0') return 0;

    if (fd == NULL) {
        if ((fd = fopen(filename, "w")) == NULL) return errno;

        for (x = 0; x < G_region.levels; x++)
            fprintf(fd, "%3d: %3d, %3d, %3d\n", G_region.level[x], G_region.color[x][0], G_region.color[x][1],
                    G_region.color[x][2]);

        fclose(fd);

        if (G_debug) {
            fprintf(stderr, "loadLossColors: fopen fail: %s\n", filename);
            fflush(stderr);
        }
    }

    else {
        x = 0;
        s = fgets(string, 80, fd);

        if (s) {
        }

        while (x < 128 && feof(fd) == 0) {
            pointer = strchr(string, ';');

            if (pointer != NULL) *pointer = 0;

            ok = sscanf(string, "%d: %d, %d, %d", &val[0], &val[1], &val[2], &val[3]);

            if (ok == 4) {
                if (G_debug) {
                    fprintf(stderr, "\nLoadLossColors() %d: %d, %d, %d\n", val[0], val[1], val[2], val[3]);
                    fflush(stderr);
                }

                for (y = 0; y < 4; y++) {
                    if (val[y] > 255) val[y] = 255;

                    if (val[y] < 0) val[y] = 0;
                }

                G_region.level[x] = val[0];
                G_region.color[x][0] = val[1];
                G_region.color[x][1] = val[2];
                G_region.color[x][2] = val[3];
                x++;
            }

            s = fgets(string, 80, fd);
        }

        fclose(fd);
        G_region.levels = x;
    }
    return 0;
}

int LoadDBMColors(struct site xmtr)
{
    int x, y, ok, val[4];
    char filename[255], string[80], *pointer = NULL, *s;
    FILE *fd = NULL;

    if (G_color_file != NULL && G_color_file[0] != 0)
        for (x = 0; G_color_file[x] != '.' && G_color_file[x] != 0 && x < 250; x++) filename[x] = G_color_file[x];
    else {
        for (x = 0; xmtr.filename[x] != '.' && xmtr.filename[x] != 0 && x < 250; x++) filename[x] = xmtr.filename[x];

        filename[x] = '.';
        filename[x + 1] = 'd';
        filename[x + 2] = 'c';
        filename[x + 3] = 'f';
        filename[x + 4] = 0;
    }

    /* Default values */

    G_region.level[0] = 0;
    G_region.color[0][0] = 255;
    G_region.color[0][1] = 0;
    G_region.color[0][2] = 0;

    G_region.level[1] = -10;
    G_region.color[1][0] = 255;
    G_region.color[1][1] = 128;
    G_region.color[1][2] = 0;

    G_region.level[2] = -20;
    G_region.color[2][0] = 255;
    G_region.color[2][1] = 165;
    G_region.color[2][2] = 0;

    G_region.level[3] = -30;
    G_region.color[3][0] = 255;
    G_region.color[3][1] = 206;
    G_region.color[3][2] = 0;

    G_region.level[4] = -40;
    G_region.color[4][0] = 255;
    G_region.color[4][1] = 255;
    G_region.color[4][2] = 0;

    G_region.level[5] = -50;
    G_region.color[5][0] = 184;
    G_region.color[5][1] = 255;
    G_region.color[5][2] = 0;

    G_region.level[6] = -60;
    G_region.color[6][0] = 0;
    G_region.color[6][1] = 255;
    G_region.color[6][2] = 0;

    G_region.level[7] = -70;
    G_region.color[7][0] = 0;
    G_region.color[7][1] = 208;
    G_region.color[7][2] = 0;

    G_region.level[8] = -80;
    G_region.color[8][0] = 0;
    G_region.color[8][1] = 196;
    G_region.color[8][2] = 196;

    G_region.level[9] = -90;
    G_region.color[9][0] = 0;
    G_region.color[9][1] = 148;
    G_region.color[9][2] = 255;

    G_region.level[10] = -100;
    G_region.color[10][0] = 80;
    G_region.color[10][1] = 80;
    G_region.color[10][2] = 255;

    G_region.level[11] = -110;
    G_region.color[11][0] = 0;
    G_region.color[11][1] = 38;
    G_region.color[11][2] = 255;

    G_region.level[12] = -120;
    G_region.color[12][0] = 142;
    G_region.color[12][1] = 63;
    G_region.color[12][2] = 255;

    G_region.level[13] = -130;
    G_region.color[13][0] = 196;
    G_region.color[13][1] = 54;
    G_region.color[13][2] = 255;

    G_region.level[14] = -140;
    G_region.color[14][0] = 255;
    G_region.color[14][1] = 0;
    G_region.color[14][2] = 255;

    G_region.level[15] = -150;
    G_region.color[15][0] = 255;
    G_region.color[15][1] = 194;
    G_region.color[15][2] = 204;

    G_region.levels = 16;

    /* Don't save if we don't have an output file */
    if ((fd = fopen(filename, "r")) == NULL && xmtr.filename[0] == '\0') return 0;

    if (fd == NULL) {
        if ((fd = fopen(filename, "w")) == NULL) return errno;

        for (x = 0; x < G_region.levels; x++)
            fprintf(fd, "%+4d: %3d, %3d, %3d\n", G_region.level[x], G_region.color[x][0], G_region.color[x][1],
                    G_region.color[x][2]);

        fclose(fd);
    }

    else {
        x = 0;
        s = fgets(string, 80, fd);

        if (s) {
        }

        while (x < 128 && feof(fd) == 0) {
            pointer = strchr(string, ';');

            if (pointer != NULL) *pointer = 0;

            ok = sscanf(string, "%d: %d, %d, %d", &val[0], &val[1], &val[2], &val[3]);

            if (ok == 4) {
                if (G_debug) {
                    fprintf(stderr, "\nLoadDBMColors() %d: %d, %d, %d\n", val[0], val[1], val[2], val[3]);
                    fflush(stderr);
                }

                if (val[0] < -200) val[0] = -200;

                if (val[0] > +40) val[0] = +40;

                G_region.level[x] = val[0];

                for (y = 1; y < 4; y++) {
                    if (val[y] > 255) val[y] = 255;

                    if (val[y] < 0) val[y] = 0;
                }

                G_region.color[x][0] = val[1];
                G_region.color[x][1] = val[2];
                G_region.color[x][2] = val[3];
                x++;
            }

            s = fgets(string, 80, fd);
        }

        fclose(fd);
        G_region.levels = x;
    }
    return 0;
}

int LoadTopoData(double max_lon, double min_lon, double max_lat, double min_lat, struct output *out)
{
    /* This function loads the SDF files required
       to cover the limits of the region specified. */

    int x, y, width, ymin, ymax;
    int success;
    char basename[255] = {0};
    char string[258] = {0};

    width = ReduceAngle(max_lon - min_lon);

    if ((max_lon - min_lon) <= 180.0) {
        for (y = 0; y <= width; y++)
            for (x = min_lat; x <= (int)max_lat; x++) {
                ymin = (int)(min_lon + (double)y);

                while (ymin < 0) ymin += 360;

                while (ymin >= 360) ymin -= 360;

                ymax = ymin + 1;

                while (ymax < 0) ymax += 360;

                while (ymax >= 360) ymax -= 360;

                snprintf(basename, 255, "%d:%d:%d:%d", x, x + 1, ymin, ymax);
                strcpy(string, basename);

                if (G_ippd == 3600) strcat(string, "-hd");

                if ((success = LoadSDF(string, out)) < 0) return -success;
            }
    }
    else {
        for (y = 0; y <= width; y++)
            for (x = (int)min_lat; x <= (int)max_lat; x++) {
                ymin = (int)(max_lon + (double)y);

                while (ymin < 0) ymin += 360;

                while (ymin >= 360) ymin -= 360;

                ymax = ymin + 1;

                while (ymax < 0) ymax += 360;

                while (ymax >= 360) ymax -= 360;

                snprintf(string, 255, "%d:%d:%d:%d", x, x + 1, ymin, ymax);
                strcpy(string, basename);

                if (G_ippd == 3600) strcat(string, "-hd");

                if ((success = LoadSDF(string, out)) < 0) return -success;
            }
    }
    return 0;
}

int LoadUDT(char *filename)
{
    /* This function reads a file containing User-Defined Terrain
       features for their addition to the digital elevation model
       data used by SPLAT!.  Elevations in the UDT file are evaluated
       and then copied into a temporary file under /tmp.  Then the
       contents of the temp file are scanned, and if found to be unique,
       are added to the ground elevations described by the digital
       elevation data already loaded into memory. */

    int i, x, y, z, ypix, xpix, tempxpix, tempypix, fd = 0, n = 0;
    char input[80], str[3][80], tempname[15], *pointer = NULL, *s = NULL;
    double latitude, longitude, height, tempheight, old_longitude = 0.0, old_latitude = 0.0;
    FILE *fd1 = NULL, *fd2 = NULL;

    strcpy(tempname, "/tmp/XXXXXX");

    if ((fd1 = fopen(filename, "r")) == NULL) return errno;

    if ((fd = mkstemp(tempname)) == -1) {
        fclose(fd1);
        return errno;
    };

    if ((fd2 = fdopen(fd, "w")) == NULL) {
        fclose(fd1);
        close(fd);
        return errno;
    }

    s = fgets(input, 78, fd1);

    if (s) {
    }

    pointer = strchr(input, ';');

    if (pointer != NULL) *pointer = 0;

    while (feof(fd1) == 0) {
        // Parse line for latitude, longitude, height

        for (x = 0, y = 0, z = 0; x < 78 && input[x] != 0 && z < 3; x++) {
            if (input[x] != ',' && y < 78) {
                str[z][y] = input[x];
                y++;
            }

            else {
                str[z][y] = 0;
                z++;
                y = 0;
            }
        }

        old_latitude = latitude = ReadBearing(str[0]);
        old_longitude = longitude = ReadBearing(str[1]);

        latitude = fabs(latitude);  // Clip if negative
        longitude = fabs(longitude);

        // Remove <CR> and/or <LF> from antenna height string

        for (i = 0; str[2][i] != 13 && str[2][i] != 10 && str[2][i] != 0; i++)
            ;

        str[2][i] = 0;

        /* The terrain feature may be expressed in either
           feet or meters.  If the letter 'M' or 'm' is
           discovered in the string, then this is an
           indication that the value given is expressed
           in meters.  Otherwise the height is interpreted
           as being expressed in feet.  */

        for (i = 0; str[2][i] != 'M' && str[2][i] != 'm' && str[2][i] != 0 && i < 48; i++)
            ;

        if (str[2][i] == 'M' || str[2][i] == 'm') {
            str[2][i] = 0;
            height = rint(atof(str[2]));
        }

        else {
            str[2][i] = 0;
            height = rint(METERS_PER_FOOT * atof(str[2]));
        }

        if (height > 0.0) fprintf(fd2, "%d, %d, %f\n", (int)rint(latitude / G_dpp), (int)rint(longitude / G_dpp), height);

        s = fgets(input, 78, fd1);

        pointer = strchr(input, ';');

        if (pointer != NULL) *pointer = 0;
    }

    fclose(fd1);
    fclose(fd2);

    if ((fd1 = fopen(tempname, "r")) == NULL) return errno;

    if ((fd2 = fopen(tempname, "r")) == NULL) {
        fclose(fd1);
        return errno;
    }

    y = 0;

    n = fscanf(fd1, "%d, %d, %lf", &xpix, &ypix, &height);

    if (n) {
    }

    do {
        x = 0;
        z = 0;

        n = fscanf(fd2, "%d, %d, %lf", &tempxpix, &tempypix, &tempheight);

        do {
            if (x > y && xpix == tempxpix && ypix == tempypix) {
                z = 1;  // Dupe Found!

                if (tempheight > height) height = tempheight;
            }

            else {
                n = fscanf(fd2, "%d, %d, %lf", &tempxpix, &tempypix, &tempheight);
                x++;
            }

        } while (feof(fd2) == 0 && z == 0);

        if (z == 0) {
            // No duplicate found
            if (G_debug) {
                fprintf(stderr, "Adding UDT Point: %lf, %lf, %lf\n", old_latitude, old_longitude, height);
                fflush(stderr);
            }
            AddElevation((double)xpix * G_dpp, (double)ypix * G_dpp, height, 1);
        }

        fflush(stderr);

        n = fscanf(fd1, "%d, %d, %lf", &xpix, &ypix, &height);
        y++;

        rewind(fd2);

    } while (feof(fd1) == 0);

    fclose(fd1);
    fclose(fd2);
    unlink(tempname);
    return 0;
}
