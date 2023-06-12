double version = 3.3;
/****************************************************************************\
*  Signal Server: Radio propagation simulator by Alex Farrant QCVS, 2E0TDW   *
******************************************************************************
*    SPLAT! Project started in 1997 by John A. Magliacane, KD2BD             *
******************************************************************************
*         Please consult the SPLAT! documentation for a complete list of     *
*         individuals who have contributed to this project.                  *
******************************************************************************
*                                                                            *
*  This program is free software; you can redistribute it and/or modify it   *
*  under the terms of the GNU General Public License as published by the     *
*  Free Software Foundation; either version 2 of the License or any later    *
*  version.                                                                  *
*                                                                            *
*  This program is distributed in the hope that it will useful, but WITHOUT  *
*  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or     *
*  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License     *
*  for more details.                                                         *
*                                                                            *
\****************************************************************************/

#include <bzlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include <vector>

#include "common.hh"
#include "image.hh"
#include "inputs.hh"
#include "models/itwom3.0.hh"
#include "models/los.hh"
#include "models/pel.hh"
#include "outputs.hh"

int MAXPAGES = 10 * 10;
int IPPD = 1200;
int ARRAYSIZE = (MAXPAGES * IPPD) + 10;

char G_sdf_path[255], opened = 0, G_gpsav = 0, ss_name[16], dashes[80], *color_file = NULL;

double G_earthradius, forced_erp, G_dpp, G_ppd, G_yppd,
    G_fzone_clearance = 0.6, forced_freq, lat, lon, txh, tercon, terdic, G_north, G_east, G_south, G_west, G_dBm, G_loss,
    G_field_strength, G_min_north = 90, G_max_north = -90, G_min_west = 360, G_max_west = -1, G_westoffset = 180,
    G_eastoffset = -180, G_delta = 0, rxGain = 0, antenna_rotation, antenna_downtilt, antenna_dt_direction, G_cropLat = -70,
    G_cropLon = 0, cropLonNeg = 0;

int G_ippd, G_mpi, G_max_elevation = -32768, G_min_elevation = 32767, bzerror, gzerr, pred, pblue, pgreen, ter,
                   multiplier = 256, G_debug = 0, G_loops = 100, G_jgets = 0, G_MAXRAD, hottest = 0, G_height, G_width,
                   resample = 0, bzbuf_empty = 1, gzbuf_empty = 1;

long bzbuf_pointer = 0L, bzbytes_read, gzbuf_pointer = 0L, gzbytes_read;

unsigned char G_got_elevation_pattern, G_got_azimuth_pattern;

bool to_stdout = false, cropping = true;

__thread double *G_elev;
__thread struct path G_path;
struct site tx_site[2];

std::vector<struct dem> G_dem;

struct LR LR;
struct region G_region;

double arccos(double x, double y)
{
    /* This function implements the arc cosine function,
       returning a value between 0 and TWOPI. */

    double result = 0.0;

    if (y > 0.0) result = acos(x / y);

    if (y < 0.0) result = PI + acos(x / y);

    return result;
}

int ReduceAngle(double angle)
{
    /* This function normalizes the argument to
       an integer angle between 0 and 180 degrees */

    double temp;

    temp = acos(cos(angle * DEG2RAD));

    return (int)rint(temp / DEG2RAD);
}

double LonDiff(double lon1, double lon2)
{
    /* This function returns the short path longitudinal
       difference between longitude1 and longitude2
       as an angle between -180.0 and +180.0 degrees.
       If lon1 is west of lon2, the result is positive.
       If lon1 is east of lon2, the result is negative. */

    double diff;

    diff = lon1 - lon2;

    if (diff <= -180.0) diff += 360.0;

    if (diff >= 180.0) diff -= 360.0;

    return diff;
}

void *dec2dms(double decimal, char *string)
{
    /* Converts decimal degrees to degrees, minutes, seconds,
       (DMS) and returns the result as a character string. */

    char sign;
    int degrees, minutes, seconds;
    double a, b, c, d;

    if (decimal < 0.0) {
        decimal = -decimal;
        sign = -1;
    }

    else
        sign = 1;

    a = floor(decimal);
    b = 60.0 * (decimal - a);
    c = floor(b);
    d = 60.0 * (b - c);

    degrees = (int)a;
    minutes = (int)c;
    seconds = (int)d;

    if (seconds < 0) seconds = 0;

    if (seconds > 59) seconds = 59;

    string[0] = 0;
    snprintf(string, 250, "%d%c %d\' %d\"", degrees * sign, 176, minutes, seconds);
    return (string);
}

int PutMask(std::vector<dem_output> *v, double lat, double lon, int value)
{
    /* Lines, text, markings, and coverage areas are stored in a
       mask that is combined with topology data when topographic
       maps are generated by ss.  This function sets and resets
       bits in the mask based on the latitude and longitude of the
       area pointed to. */

    int x = 0, y = 0;
    struct dem_output *out;
    char found = 0;

    for (auto &i : *v) {
        x = (int)rint(G_ppd * (lat - i.min_north));
        y = G_mpi - (int)rint(G_yppd * (LonDiff(i.max_west, lon)));

        if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
            out = &i;
            found = 1;
            break;
        }
    }

    // if we couldn't find it in the output vector, find the right DEM
    // and create a corresponding output vector entry
    if (!found) {
        for (auto &dem : G_dem) {
            x = (int)rint(G_ppd * (lat - dem.min_north));
            y = G_mpi - (int)rint(G_yppd * (LonDiff(dem.max_west, lon)));

            if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
                struct dem_output tmp;
                tmp.min_north = dem.min_north;
                tmp.max_north = dem.max_north;
                tmp.min_west = dem.min_west;
                tmp.max_west = dem.max_west;
                tmp.dem = &dem;
                tmp.mask = new unsigned char[dem.ippd * dem.ippd];
                tmp.signal = new unsigned char[dem.ippd * dem.ippd];
                v->push_back(tmp);
                out = &v->back();
                found = 1;
                break;
            }
        }
    }

    if (found) {
        out->mask[DEM_INDEX(out->dem->ippd, x, y)] = value;
        return ((int)out->mask[DEM_INDEX(out->dem->ippd, x, y)]);
    }

    else
        return -1;
}

int OrMask(std::vector<dem_output> *v, double lat, double lon, int value)
{
    /* Lines, text, markings, and coverage areas are stored in a
       mask that is combined with topology data when topographic
       maps are generated by ss.  This function sets bits in
       the mask based on the latitude and longitude of the area
       pointed to. */

    int x = 0, y = 0;
    struct dem_output *out;
    char found = 0;

    for (auto &i : *v) {
        x = (int)rint(G_ppd * (lat - i.min_north));
        y = G_mpi - (int)rint(G_yppd * (LonDiff(i.max_west, lon)));

        if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
            out = &i;
            found = 1;
            break;
        }
    }

    // if we couldn't find it in the output vector, find the right DEM
    // and create a corresponding output vector entry
    if (!found) {
        for (auto &dem : G_dem) {
            x = (int)rint(G_ppd * (lat - dem.min_north));
            y = G_mpi - (int)rint(G_yppd * (LonDiff(dem.max_west, lon)));

            if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
                struct dem_output tmp;
                tmp.min_north = dem.min_north;
                tmp.max_north = dem.max_north;
                tmp.min_west = dem.min_west;
                tmp.max_west = dem.max_west;
                tmp.dem = &dem;
                tmp.mask = new unsigned char[dem.ippd * dem.ippd];
                bzero(tmp.mask, dem.ippd * dem.ippd);
                tmp.signal = new unsigned char[dem.ippd * dem.ippd];
                bzero(tmp.signal, dem.ippd * dem.ippd);
                v->push_back(tmp);
                out = &v->back();
                found = 1;
                break;
            }
        }
    }

    if (found) {
        out->mask[DEM_INDEX(out->dem->ippd, x, y)] |= value;
        return ((int)out->mask[DEM_INDEX(out->dem->ippd, x, y)]);
    }

    else
        return -1;
}

int GetMask(std::vector<dem_output> *v, double lat, double lon)
{
    /* This function returns the mask bits based on the latitude
       and longitude given. */
    int x = 0, y = 0;
    struct dem_output *out;
    char found = 0;

    for (auto &i : *v) {
        x = (int)rint(G_ppd * (lat - i.min_north));
        y = G_mpi - (int)rint(G_yppd * (LonDiff(i.max_west, lon)));

        if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
            out = &i;
            found = 1;
            break;
        }
    }

    if (found) {
        return ((int)out->mask[DEM_INDEX(out->dem->ippd, x, y)]);
    }
    else {
        return 0;
    }
}

void PutSignal(std::vector<dem_output> *v, double lat, double lon, unsigned char signal)
{
    int x = 0, y = 0;
    char found = 0, dotfile[260], basename[255];
    struct dem_output *out;

    /* This function writes a signal level (0-255)
       at the specified location for later recall. */

    snprintf(basename, 255, "%s", tx_site[0].filename);
    strcpy(dotfile, basename);
    strcat(dotfile, ".dot");

    if (signal > hottest)  // dBm, dBuV
        hottest = signal;

    // lookup x/y for this co-ord
    for (auto &i : *v) {
        x = (int)rint(G_ppd * (lat - i.min_north));
        y = G_mpi - (int)rint(G_yppd * (LonDiff(i.max_west, lon)));

        if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
            out = &i;
            found = 1;
            break;
        }
    }

    // if we couldn't find it in the output vector, find the right DEM
    // and create a corresponding output vector entry
    if (!found) {
        for (auto &dem : G_dem) {
            x = (int)rint(G_ppd * (lat - dem.min_north));
            y = G_mpi - (int)rint(G_yppd * (LonDiff(dem.max_west, lon)));

            if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
                struct dem_output tmp;
                tmp.min_north = dem.min_north;
                tmp.max_north = dem.max_north;
                tmp.min_west = dem.min_west;
                tmp.max_west = dem.max_west;
                tmp.dem = &dem;
                tmp.mask = new unsigned char[dem.ippd * dem.ippd];
                bzero(tmp.mask, dem.ippd * dem.ippd);
                tmp.signal = new unsigned char[dem.ippd * dem.ippd];
                bzero(tmp.signal, dem.ippd * dem.ippd);
                v->push_back(tmp);
                out = &v->back();
                found = 1;
                break;
            }
        }
    }

    if (found) {  // Write values to file
        out->signal[DEM_INDEX(out->dem->ippd, x, y)] = signal;
        // return (dem[indx].signal[x][y]);
        return;
    }
    else
        // return 0;
        return;
}

unsigned char GetSignal(std::vector<dem_output> *v, double lat, double lon)
{
    /* This function reads the signal level (0-255) at the
       specified location that was previously written by the
       complimentary PutSignal() function. */

    int x = 0, y = 0;
    char found = 0;
    struct dem_output *out = NULL;

    // lookup x/y for this co-ord
    for (auto &i : *v) {
        x = (int)rint(G_ppd * (lat - i.min_north));
        y = G_mpi - (int)rint(G_yppd * (LonDiff(i.max_west, lon)));

        if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
            out = &i;
            found = 1;
            break;
        }
    }

    if (found)
        return (out->signal[DEM_INDEX(out->dem->ippd, x, y)]);
    else
        return 0;
}

double GetElevation(struct site location)
{
    /* This function returns the elevation (in feet) of any location
       represented by the digital elevation model data in memory.
       Function returns -5000.0 for locations not found in memory. */

    int x = 0, y = 0;
    double elevation;
    struct dem *found = NULL;

    for (auto &dem : G_dem) {
        x = (int)rint(G_ppd * (location.lat - dem.min_north));
        y = G_mpi - (int)rint(G_yppd * (LonDiff(dem.max_west, location.lon)));

        if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
            found = &dem;
            break;
        }
    }

    if (found)
        elevation = 3.28084 * found->data[DEM_INDEX(found->ippd, x, y)];
    else
        elevation = -5000.0;

    return elevation;
}

int AddElevation(double lat, double lon, double height, int size)
{
    /* This function adds a user-defined terrain feature
       (in meters AGL) to the digital elevation model data
       in memory.  Does nothing and returns 0 for locations
       not found in memory. */

    struct dem *found;
    int i, j, x = 0, y = 0;

    for (auto &dem : G_dem) {
        x = (int)rint(G_ppd * (lat - dem.min_north));
        y = G_mpi - (int)rint(G_yppd * (LonDiff(dem.max_west, lon)));

        if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi) {
            found = &dem;
            break;
        }
    }

    if (found && size < 2) found->data[DEM_INDEX(found->ippd, x, y)] += (short)rint(height);

    // Make surrounding area bigger for wide area landcover. Should enhance 3x3 pixels including c.p
    if (found && size > 1) {
        for (i = size * -1; i <= size; i = i + 1) {
            for (j = size * -1; j <= size; j = j + 1) {
                if (x + j >= 0 && x + j <= IPPD && y + i >= 0 && y + i <= IPPD)
                    // coordinates are swapped for some reason?
                    found->data[DEM_INDEX(found->ippd, y + i, x + j)] += (short)rint(height);
            }
        }
    }

    return found != NULL;
}

double dist(double lat1, double lon1, double lat2, double lon2)
{
    // ENHANCED HAVERSINE FORMULA WITH RADIUS SLIDER
    double dx, dy, dz;
    int polarRadius = 6357;
    int equatorRadius = 6378;
    int delta = equatorRadius - polarRadius;  // 21km
    float earthRadius = equatorRadius - ((lat1 / 100) * delta);
    lon1 -= lon2;
    lon1 *= DEG2RAD, lat1 *= DEG2RAD, lat2 *= DEG2RAD;

    dz = sin(lat1) - sin(lat2);
    dx = cos(lon1) * cos(lat1) - cos(lat2);
    dy = sin(lon1) * cos(lat1);
    return asin(sqrt(dx * dx + dy * dy + dz * dz) / 2) * 2 * earthRadius;
}

double Distance(struct site site1, struct site site2)
{
    /* This function returns the great circle distance
       in miles between any two site locations. */

    double lat1, lon1, lat2, lon2, distance;

    lat1 = site1.lat * DEG2RAD;
    lon1 = site1.lon * DEG2RAD;
    lat2 = site2.lat * DEG2RAD;
    lon2 = site2.lon * DEG2RAD;

    distance = 3959.0 * acos(sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos((lon1) - (lon2)));

    return distance;
}

double Azimuth(struct site source, struct site destination)
{
    /* This function returns the azimuth (in degrees) to the
       destination as seen from the location of the source. */

    double dest_lat, dest_lon, src_lat, src_lon, beta, azimuth, diff, num, den, fraction;

    dest_lat = destination.lat * DEG2RAD;
    dest_lon = destination.lon * DEG2RAD;

    src_lat = source.lat * DEG2RAD;
    src_lon = source.lon * DEG2RAD;

    /* Calculate Surface Distance */

    beta = acos(sin(src_lat) * sin(dest_lat) + cos(src_lat) * cos(dest_lat) * cos(src_lon - dest_lon));

    /* Calculate Azimuth */

    num = sin(dest_lat) - (sin(src_lat) * cos(beta));
    den = cos(src_lat) * sin(beta);
    fraction = num / den;

    /* Trap potential problems in acos() due to rounding */

    if (fraction >= 1.0) fraction = 1.0;

    if (fraction <= -1.0) fraction = -1.0;

    /* Calculate azimuth */

    azimuth = acos(fraction);

    /* Reference it to True North */

    diff = dest_lon - src_lon;

    if (diff <= -PI) diff += TWOPI;

    if (diff >= PI) diff -= TWOPI;

    if (diff > 0.0) azimuth = TWOPI - azimuth;

    return (azimuth / DEG2RAD);
}

double ElevationAngle(struct site source, struct site destination)
{
    /* This function returns the angle of elevation (in degrees)
       of the destination as seen from the source location.
       A positive result represents an angle of elevation (uptilt),
       while a negative result represents an angle of depression
       (downtilt), as referenced to a normal to the center of
       the earth. */

    double a, b, dx;

    a = GetElevation(destination) + destination.alt + G_earthradius;
    b = GetElevation(source) + source.alt + G_earthradius;

    dx = FEET_PER_MILE * Distance(source, destination);

    /* Apply the Law of Cosines */

    return ((180.0 * (acos(((b * b) + (dx * dx) - (a * a)) / (2.0 * b * dx))) / PI) - 90.0);
}

void ReadPath(struct site source, struct site destination)
{
    /* This function generates a sequence of latitude and
       longitude positions between source and destination
       locations along a great circle path, and stores
       elevation and distance information for points
       along that path in the "path" structure. */

    int c;
    double azimuth, distance, lat1, lon1, beta, den, num, lat2, lon2, total_distance, dx, dy, path_length, miles_per_sample,
        samples_per_radian = 68755.0;
    struct site tempsite;

    lat1 = source.lat * DEG2RAD;
    lon1 = source.lon * DEG2RAD;
    lat2 = destination.lat * DEG2RAD;
    lon2 = destination.lon * DEG2RAD;
    samples_per_radian = G_ppd * 57.295833;
    azimuth = Azimuth(source, destination) * DEG2RAD;

    total_distance = Distance(source, destination);

    if (total_distance > (30.0 / G_ppd)) {
        dx = samples_per_radian * acos(cos(lon1 - lon2));
        dy = samples_per_radian * acos(cos(lat1 - lat2));
        path_length = sqrt((dx * dx) + (dy * dy));
        miles_per_sample = total_distance / path_length;
    }

    else {
        c = 0;
        dx = 0.0;
        dy = 0.0;
        path_length = 0.0;
        miles_per_sample = 0.0;
        total_distance = 0.0;

        lat1 = lat1 / DEG2RAD;
        lon1 = lon1 / DEG2RAD;

        G_path.lat[c] = lat1;
        G_path.lon[c] = lon1;
        G_path.elevation[c] = GetElevation(source);
        G_path.distance[c] = 0.0;
    }

    for (distance = 0.0, c = 0; (total_distance != 0.0 && distance <= total_distance && c < ARRAYSIZE);
         c++, distance = miles_per_sample * (double)c) {
        beta = distance / 3959.0;
        lat2 = asin(sin(lat1) * cos(beta) + cos(azimuth) * sin(beta) * cos(lat1));
        num = cos(beta) - (sin(lat1) * sin(lat2));
        den = cos(lat1) * cos(lat2);

        if (azimuth == 0.0 && (beta > HALFPI - lat1))
            lon2 = lon1 + PI;

        else if (azimuth == HALFPI && (beta > HALFPI + lat1))
            lon2 = lon1 + PI;

        else if (fabs(num / den) > 1.0)
            lon2 = lon1;

        else {
            if ((PI - azimuth) >= 0.0)
                lon2 = lon1 - arccos(num, den);
            else
                lon2 = lon1 + arccos(num, den);
        }

        while (lon2 < 0.0) lon2 += TWOPI;

        while (lon2 > TWOPI) lon2 -= TWOPI;

        lat2 = lat2 / DEG2RAD;
        lon2 = lon2 / DEG2RAD;

        G_path.lat[c] = lat2;
        G_path.lon[c] = lon2;
        tempsite.lat = lat2;
        tempsite.lon = lon2;
        G_path.elevation[c] = GetElevation(tempsite);
        // fix for tile gaps in multi-tile LIDAR plots
        if (G_path.elevation[c] == 0 && G_path.elevation[c - 1] > 10) G_path.elevation[c] = G_path.elevation[c - 1];
        G_path.distance[c] = distance;
    }

    /* Make sure exact destination point is recorded at path.length-1 */

    if (c < ARRAYSIZE) {
        G_path.lat[c] = destination.lat;
        G_path.lon[c] = destination.lon;
        G_path.elevation[c] = GetElevation(destination);
        G_path.distance[c] = total_distance;
        c++;
    }

    if (c < ARRAYSIZE)
        G_path.length = c;
    else
        G_path.length = ARRAYSIZE - 1;
}

double ElevationAngle2(struct site source, struct site destination, double er)
{
    /* This function returns the angle of elevation (in degrees)
       of the destination as seen from the source location, UNLESS
       the path between the sites is obstructed, in which case, the
       elevation angle to the first obstruction is returned instead.
       "er" represents the earth radius. */

    int x;
    char block = 0;
    double source_alt, destination_alt, cos_xmtr_angle, cos_test_angle, test_alt, elevation, distance, source_alt2,
        first_obstruction_angle = 0.0;
    struct path temp;

    temp = G_path;

    ReadPath(source, destination);

    distance = FEET_PER_MILE * Distance(source, destination);
    source_alt = er + source.alt + GetElevation(source);
    destination_alt = er + destination.alt + GetElevation(destination);
    source_alt2 = source_alt * source_alt;

    /* Calculate the cosine of the elevation angle of the
       destination (receiver) as seen by the source (transmitter). */

    cos_xmtr_angle =
        ((source_alt2) + (distance * distance) - (destination_alt * destination_alt)) / (2.0 * source_alt * distance);

    /* Test all points in between source and destination locations to
       see if the angle to a topographic feature generates a higher
       elevation angle than that produced by the destination.  Begin
       at the source since we're interested in identifying the FIRST
       obstruction along the path between source and destination. */

    for (x = 2, block = 0; x < G_path.length && block == 0; x++) {
        distance = FEET_PER_MILE * G_path.distance[x];

        test_alt = G_earthradius + (G_path.elevation[x] == 0.0 ? G_path.elevation[x] : G_path.elevation[x] + LR.clutter);

        cos_test_angle = ((source_alt2) + (distance * distance) - (test_alt * test_alt)) / (2.0 * source_alt * distance);

        /* Compare these two angles to determine if
           an obstruction exists.  Since we're comparing
           the cosines of these angles rather than
           the angles themselves, the sense of the
           following "if" statement is reversed from
           what it would be if the angles themselves
           were compared. */

        if (cos_xmtr_angle >= cos_test_angle) {
            block = 1;
            first_obstruction_angle = ((acos(cos_test_angle)) / DEG2RAD) - 90.0;
        }
    }

    if (block)
        elevation = first_obstruction_angle;

    else
        elevation = ((acos(cos_xmtr_angle)) / DEG2RAD) - 90.0;

    G_path = temp;

    return elevation;
}

double ReadBearing(char *input)
{
    /* This function takes numeric input in the form of a character
       string, and returns an equivalent bearing in degrees as a
       decimal number (double).  The input may either be expressed
       in decimal format (40.139722) or degree, minute, second
       format (40 08 23).  This function also safely handles
       extra spaces found either leading, trailing, or
       embedded within the numbers expressed in the
       input string.  Decimal seconds are permitted. */

    double seconds, bearing = 0.0;
    char string[20];
    int a, b, length, degrees, minutes;

    /* Copy "input" to "string", and ignore any extra
       spaces that might be present in the process. */

    string[0] = 0;
    length = strlen(input);

    for (a = 0, b = 0; a < length && a < 18; a++) {
        if ((input[a] != 32 && input[a] != '\n') || (input[a] == 32 && input[a + 1] != 32 && input[a + 1] != '\n' && b != 0)) {
            string[b] = input[a];
            b++;
        }
    }

    string[b] = 0;

    /* Count number of spaces in the clean string. */

    length = strlen(string);

    for (a = 0, b = 0; a < length; a++)
        if (string[a] == 32) b++;

    if (b == 0) /* Decimal Format (40.139722) */
        sscanf(string, "%lf", &bearing);

    if (b == 2) { /* Degree, Minute, Second Format (40 08 23.xx) */
        sscanf(string, "%d %d %lf", &degrees, &minutes, &seconds);

        bearing = fabs((double)degrees);
        bearing += fabs(((double)minutes) / 60.0);
        bearing += fabs(seconds / 3600.0);

        if ((degrees < 0) || (minutes < 0) || (seconds < 0.0)) bearing = -bearing;
    }

    /* Anything else returns a 0.0 */

    if (bearing > 360.0 || bearing < -360.0) bearing = 0.0;

    return bearing;
}

void ObstructionAnalysis(struct site xmtr, struct site rcvr, double f, FILE *outfile)
{
    /* Perform an obstruction analysis along the
       path between receiver and transmitter. */

    int x;
    struct site site_x;
    double h_r, h_t, h_x, h_r_orig, cos_tx_angle, cos_test_angle, cos_tx_angle_f1, cos_tx_angle_fpt6, d_tx, d_x, h_r_f1,
        h_r_fpt6, h_f, h_los, lambda = 0.0;
    char string[255], string_fpt6[255], string_f1[255];

    ReadPath(xmtr, rcvr);
    h_r = GetElevation(rcvr) + rcvr.alt + G_earthradius;
    h_r_f1 = h_r;
    h_r_fpt6 = h_r;
    h_r_orig = h_r;
    h_t = GetElevation(xmtr) + xmtr.alt + G_earthradius;
    d_tx = FEET_PER_MILE * Distance(rcvr, xmtr);
    cos_tx_angle = ((h_r * h_r) + (d_tx * d_tx) - (h_t * h_t)) / (2.0 * h_r * d_tx);
    cos_tx_angle_f1 = cos_tx_angle;
    cos_tx_angle_fpt6 = cos_tx_angle;

    if (f) lambda = 9.8425e8 / (f * 1e6);

    if (LR.clutter > 0.0) {
        fprintf(outfile, "Terrain has been raised by");

        if (LR.metric)
            fprintf(outfile, " %.2f meters", METERS_PER_FOOT * LR.clutter);
        else
            fprintf(outfile, " %.2f feet", LR.clutter);

        fprintf(outfile, " to account for ground clutter.\n\n");
    }

    /* At each point along the path calculate the cosine
       of a sort of "inverse elevation angle" at the receiver.
       From the antenna, 0 deg. looks at the ground, and 90 deg.
       is parallel to the ground.

       Start at the receiver.  If this is the lowest antenna,
       then terrain obstructions will be nearest to it.  (Plus,
       that's the way ppa!'s original los() did it.)

       Calculate cosines only.  That's sufficient to compare
       angles and it saves the extra computational burden of
       acos().  However, note the inverted comparison: if
       acos(A) > acos(B), then B > A. */

    for (x = G_path.length - 1; x > 0; x--) {
        site_x.lat = G_path.lat[x];
        site_x.lon = G_path.lon[x];
        site_x.alt = 0.0;

        h_x = GetElevation(site_x) + G_earthradius + LR.clutter;
        d_x = FEET_PER_MILE * Distance(rcvr, site_x);

        /* Deal with the LOS path first. */

        cos_test_angle = ((h_r * h_r) + (d_x * d_x) - (h_x * h_x)) / (2.0 * h_r * d_x);

        if (cos_tx_angle > cos_test_angle) {
            if (h_r == h_r_orig)
                fprintf(outfile, "Between %s and %s, obstructions were detected at:\n\n", rcvr.name, xmtr.name);

            if (site_x.lat >= 0.0) {
                if (LR.metric)
                    fprintf(outfile, "   %8.4f N,%9.4f W, %5.2f kilometers, %6.2f meters AMSL\n", site_x.lat, site_x.lon,
                            KM_PER_MILE * (d_x / FEET_PER_MILE), METERS_PER_FOOT * (h_x - G_earthradius));
                else
                    fprintf(outfile, "   %8.4f N,%9.4f W, %5.2f miles, %6.2f feet AMSL\n", site_x.lat, site_x.lon,
                            d_x / FEET_PER_MILE, h_x - G_earthradius);
            }

            else {
                if (LR.metric)
                    fprintf(outfile, "   %8.4f S,%9.4f W, %5.2f kilometers, %6.2f meters AMSL\n", -site_x.lat, site_x.lon,
                            KM_PER_MILE * (d_x / FEET_PER_MILE), METERS_PER_FOOT * (h_x - G_earthradius));
                else
                    fprintf(outfile, "   %8.4f S,%9.4f W, %5.2f miles, %6.2f feet AMSL\n", -site_x.lat, site_x.lon,
                            d_x / FEET_PER_MILE, h_x - G_earthradius);
            }
        }

        while (cos_tx_angle > cos_test_angle) {
            h_r += 1;
            cos_test_angle = ((h_r * h_r) + (d_x * d_x) - (h_x * h_x)) / (2.0 * h_r * d_x);
            cos_tx_angle = ((h_r * h_r) + (d_tx * d_tx) - (h_t * h_t)) / (2.0 * h_r * d_tx);
        }

        if (f) {
            /* Now clear the first Fresnel zone... */

            cos_tx_angle_f1 = ((h_r_f1 * h_r_f1) + (d_tx * d_tx) - (h_t * h_t)) / (2.0 * h_r_f1 * d_tx);
            h_los = sqrt(h_r_f1 * h_r_f1 + d_x * d_x - 2 * h_r_f1 * d_x * cos_tx_angle_f1);
            h_f = h_los - sqrt(lambda * d_x * (d_tx - d_x) / d_tx);

            while (h_f < h_x) {
                h_r_f1 += 1;
                cos_tx_angle_f1 = ((h_r_f1 * h_r_f1) + (d_tx * d_tx) - (h_t * h_t)) / (2.0 * h_r_f1 * d_tx);
                h_los = sqrt(h_r_f1 * h_r_f1 + d_x * d_x - 2 * h_r_f1 * d_x * cos_tx_angle_f1);
                h_f = h_los - sqrt(lambda * d_x * (d_tx - d_x) / d_tx);
            }

            /* and clear the 60% F1 zone. */

            cos_tx_angle_fpt6 = ((h_r_fpt6 * h_r_fpt6) + (d_tx * d_tx) - (h_t * h_t)) / (2.0 * h_r_fpt6 * d_tx);
            h_los = sqrt(h_r_fpt6 * h_r_fpt6 + d_x * d_x - 2 * h_r_fpt6 * d_x * cos_tx_angle_fpt6);
            h_f = h_los - G_fzone_clearance * sqrt(lambda * d_x * (d_tx - d_x) / d_tx);

            while (h_f < h_x) {
                h_r_fpt6 += 1;
                cos_tx_angle_fpt6 = ((h_r_fpt6 * h_r_fpt6) + (d_tx * d_tx) - (h_t * h_t)) / (2.0 * h_r_fpt6 * d_tx);
                h_los = sqrt(h_r_fpt6 * h_r_fpt6 + d_x * d_x - 2 * h_r_fpt6 * d_x * cos_tx_angle_fpt6);
                h_f = h_los - G_fzone_clearance * sqrt(lambda * d_x * (d_tx - d_x) / d_tx);
            }
        }
    }

    if (h_r > h_r_orig) {
        if (LR.metric)
            snprintf(string, 150,
                     "\nAntenna at %s must be raised to at least %.2f meters AGL\nto clear all obstructions detected.\n",
                     rcvr.name, METERS_PER_FOOT * (h_r - GetElevation(rcvr) - G_earthradius));
        else
            snprintf(string, 150,
                     "\nAntenna at %s must be raised to at least %.2f feet AGL\nto clear all obstructions detected.\n",
                     rcvr.name, h_r - GetElevation(rcvr) - G_earthradius);
    }

    else
        snprintf(string, 150, "\nNo obstructions to LOS path due to terrain were detected\n");

    if (f) {
        if (h_r_fpt6 > h_r_orig) {
            if (LR.metric)
                snprintf(
                    string_fpt6, 150,
                    "\nAntenna at %s must be raised to at least %.2f meters AGL\nto clear %.0f%c of the first Fresnel zone.\n",
                    rcvr.name, METERS_PER_FOOT * (h_r_fpt6 - GetElevation(rcvr) - G_earthradius), G_fzone_clearance * 100.0,
                    37);

            else
                snprintf(
                    string_fpt6, 150,
                    "\nAntenna at %s must be raised to at least %.2f feet AGL\nto clear %.0f%c of the first Fresnel zone.\n",
                    rcvr.name, h_r_fpt6 - GetElevation(rcvr) - G_earthradius, G_fzone_clearance * 100.0, 37);
        }

        else
            snprintf(string_fpt6, 150, "\n%.0f%c of the first Fresnel zone is clear.\n", G_fzone_clearance * 100.0, 37);

        if (h_r_f1 > h_r_orig) {
            if (LR.metric)
                snprintf(string_f1, 150,
                         "\nAntenna at %s must be raised to at least %.2f meters AGL\nto clear the first Fresnel zone.\n",
                         rcvr.name, METERS_PER_FOOT * (h_r_f1 - GetElevation(rcvr) - G_earthradius));

            else
                snprintf(string_f1, 150,
                         "\nAntenna at %s must be raised to at least %.2f feet AGL\nto clear the first Fresnel zone.\n",
                         rcvr.name, h_r_f1 - GetElevation(rcvr) - G_earthradius);
        }

        else
            snprintf(string_f1, 150, "\nThe first Fresnel zone is clear.\n");
    }

    fprintf(outfile, "%s", string);

    if (f) {
        fprintf(outfile, "%s", string_f1);
        fprintf(outfile, "%s", string_fpt6);
    }
}

void free_elev(void) { delete[] G_elev; }

void free_path(void)
{
    delete[] G_path.lat;
    delete[] G_path.lon;
    delete[] G_path.elevation;
    delete[] G_path.distance;
}

void alloc_elev(void) { G_elev = new double[ARRAYSIZE + 10]; }

void alloc_path(void)
{
    G_path.lat = new double[ARRAYSIZE];
    G_path.lon = new double[ARRAYSIZE];
    G_path.elevation = new double[ARRAYSIZE];
    G_path.distance = new double[ARRAYSIZE];
}

void do_allocs(void)
{
    alloc_elev();
    alloc_path();
}

int handle_args(int argc, char *argv[])
{
    /* Scan for command line arguments */
    int x, y, z = 0, propmodel, knifeedge = 0, ppa = 0, normalise = 0, haf = 0, pmenv = 1, lidar = 0, result;

    double min_lat, min_lon, max_lat, max_lon, rxlat, rxlon, txlat, txlon, west_min, west_max, nortRxHin, nortRxHax;

    bool use_threads = false;

    unsigned char LRmap = 0, txsites = 0, topomap = 0, geo = 0, kml = 0, area_mode = 0, max_txsites, ngs = 0;

    char mapfile[255], ano_filename[255], lidar_tiles[27000], clutter_file[255], antenna_file[255];
    char *az_filename, *el_filename, *udt_file = NULL;

    double altitude = 0.0, altitudeLR = 0.0, tx_range = 0.0, rx_range = 0.0, deg_range = 0.0, deg_limit = 0.0, deg_range_lon;

    kml = 0;
    geo = 0;
    mapfile[0] = 0;
    clutter_file[0] = 0;
    forced_erp = -1.0;
    forced_freq = 0.0;
    udt_file = NULL;
    color_file = NULL;
    max_txsites = 30;
    resample = 0;

    struct LR LR;

    // all these need to be made per-request variables
    LR.max_range = 1.0;
    LR.contour_threshold = 0;
    LR.clutter = 0.0;
    LR.dbm = 0;
    LR.metric = 0;
    LR.eps_dielect = 15.0;        // Farmland
    LR.sgm_conductivity = 0.005;  // Farmland
    LR.eno_ns_surfref = 301.0;
    LR.frq_mhz = 19.0;     // Deliberately too low
    LR.radio_climate = 5;  // continental
    LR.pol = 1;            // vert
    LR.conf = 0.50;
    LR.rel = 0.50;
    LR.erp = 0.0;  // will default to Path Loss

    ano_filename[0] = 0;
    propmodel = 1;  // ITM
    lat = 0;
    lon = 0;
    txh = 0;
    ngs = 1;  // no terrain background
    kml = 1;
    LRmap = 1;
    area_mode = 1;

    antenna_rotation = -1;    // unique defaults to test usage
    antenna_downtilt = 99.0;  // don't mess with them!
    antenna_dt_direction = -1;
    antenna_file[0] = '\0';

    tx_site[0].lat = 91.0;
    tx_site[0].lon = 361.0;
    tx_site[1].lat = 91.0;
    tx_site[1].lon = 361.0;

    y = argc - 1;

    for (x = 0; x <= y; x++) {
        if (strcmp(argv[x], "-R") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &LR.max_range);
            }
        }

        if (strcmp(argv[x], "-gc") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &LR.clutter);

                if (LR.clutter < 0.0) LR.clutter = 0.0;
            }
        }

        if (strcmp(argv[x], "-clt") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                strncpy(clutter_file, argv[z], 253);
            }
        }

        if (strcmp(argv[x], "-ant") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                strncpy(antenna_file, argv[z], 253);
            }
        }

        if (strcmp(argv[x], "-rot") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &antenna_rotation);

                if (antenna_rotation < 0.0) antenna_rotation = 0.0;
                if (antenna_rotation > 359.0) antenna_rotation = 0.0;
            }
        }

        if (strcmp(argv[x], "-dt") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) { /* A minus argument is legal here */
                sscanf(argv[z], "%lf", &antenna_downtilt);
                if (antenna_downtilt < -10.0) antenna_downtilt = -10.0;
                if (antenna_downtilt > 90.0) antenna_downtilt = 90.0;
            }
        }

        if (strcmp(argv[x], "-dtdir") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &antenna_dt_direction);

                if (antenna_dt_direction < 0.0) antenna_dt_direction = 0.0;
                if (antenna_dt_direction > 359.0) antenna_dt_direction = 0.0;
            }
        }

        if (strcmp(argv[x], "-o") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                strncpy(mapfile, argv[z], 253);
                strncpy(tx_site[0].name, "Tx", 2);
                strncpy(tx_site[0].filename, argv[z], 253);
                /* Antenna pattern files have the same basic name as the output file
                 * but with a different extension. If they exist, load them now */
                if ((az_filename = (char *)calloc(strlen(argv[z]) + strlen(AZ_FILE_SUFFIX) + 1, sizeof(char))) == NULL)
                    return ENOMEM;
                if (antenna_file[0] != '\0')
                    strcpy(az_filename, antenna_file);
                else
                    strcpy(az_filename, argv[z]);
                strcat(az_filename, AZ_FILE_SUFFIX);

                if ((el_filename = (char *)calloc(strlen(argv[z]) + strlen(EL_FILE_SUFFIX) + 1, sizeof(char))) == NULL) {
                    free(az_filename);
                    return ENOMEM;
                }
                if (antenna_file[0] != '\0')
                    strcpy(el_filename, antenna_file);
                else
                    strcpy(el_filename, argv[z]);
                strcat(el_filename, EL_FILE_SUFFIX);

                if ((result = LoadPAT(az_filename, el_filename, &LR)) != 0) {
                    fprintf(stderr, "Permissions error reading antenna pattern file\n");
                    free(az_filename);
                    free(el_filename);
                    exit(result);
                }
                free(az_filename);
                free(el_filename);
            }
            else if (z <= y && argv[z][0] && argv[z][0] == '-' && argv[z][1] == '\0') {
                /* Handle writing image data to stdout */
                to_stdout = true;
                mapfile[0] = '\0';
                strncpy(tx_site[0].name, "Tx", 2);
                tx_site[0].filename[0] = '\0';
                fprintf(stderr, "Writing to stdout\n");
            }
        }

        if (strcmp(argv[x], "-so") == 0) {
            z = x + 1;
            if (image_set_library(argv[z]) != 0) {
                fprintf(stderr, "Error configuring image processor\n");
                exit(EINVAL);
            }
        }

        if (strcmp(argv[x], "-rt") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) /* A minus argument is legal here */
                sscanf(argv[z], "%d", &LR.contour_threshold);
        }

        if (strcmp(argv[x], "-m") == 0) {
            LR.metric = 1;
        }

        if (strcmp(argv[x], "-t") == 0) {
            ngs = 0;  // greyscale background
        }

        if (strcmp(argv[x], "-dbm") == 0) LR.dbm = 1;

        if (strcmp(argv[x], "-lid") == 0) {
            z = x + 1;
            lidar = 1;
            if (z <= y && argv[z][0] && argv[z][0] != '-') strncpy(lidar_tiles, argv[z], 27000);  // 900 tiles!
        }

        // TODO we need to handle resolution better
        // or make resolution explicit in the data
        // XXX assume 1200 for now
        /*if (strcmp(argv[x], "-res") == 0) {
                z = x + 1;

                if (!lidar &&
                    z <= y &&
                    argv[z][0] &&
                    argv[z][0] != '-') {
                        sscanf(argv[z], "%d", &G_ippd);

                        switch (G_ippd) {
                        case 300:
                                G_MAXRAD = 500;
                                G_jgets = 3; // 3 dummy reads
                                break;
                        case 600:
                                G_MAXRAD = 500;
                                G_jgets = 1;
                                break;
                        case 1200:
                                G_MAXRAD = 200;
                                G_ippd = 1200;
                                break;
                        case 3600:
                                G_MAXRAD = 100;
                                G_ippd = 3600;
                                break;
                        default:
                                G_MAXRAD = 200;
                                G_ippd = 1200;
                                break;
                        }
                }
        }*/

        if (strcmp(argv[x], "-resample") == 0) {
            z = x + 1;

            if (!lidar) {
                fprintf(stderr, "Error, this should only be used with LIDAR tiles.\n");
                return -1;
            }

            sscanf(argv[z], "%d", &resample);
        }

        if (strcmp(argv[x], "-lat") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) {
                tx_site[0].lat = ReadBearing(argv[z]);
            }
        }
        if (strcmp(argv[x], "-lon") == 0) {
            z = x + 1;
            if (z <= y && argv[z][0]) {
                tx_site[0].lon = ReadBearing(argv[z]);
                tx_site[0].lon *= -1;
                if (tx_site[0].lon < 0.0) tx_site[0].lon += 360.0;
            }
        }
        // Switch to Path Profile Mode if Rx co-ords specified
        if (strcmp(argv[x], "-rla") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) {
                ppa = 1;
                tx_site[1].lat = ReadBearing(argv[z]);
            }
        }
        if (strcmp(argv[x], "-rlo") == 0) {
            z = x + 1;
            if (z <= y && argv[z][0]) {
                tx_site[1].lon = ReadBearing(argv[z]);
                tx_site[1].lon *= -1;
                if (tx_site[1].lon < 0.0) tx_site[1].lon += 360.0;
            }
        }

        if (strcmp(argv[x], "-txh") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%f", &tx_site[0].alt);
            }
            txsites = 1;
        }

        if (strcmp(argv[x], "-rxh") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &altitudeLR);
                sscanf(argv[z], "%f", &tx_site[1].alt);
            }
        }

        if (strcmp(argv[x], "-rxg") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &rxGain);
            }
        }

        if (strcmp(argv[x], "-f") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &LR.frq_mhz);
            }
        }

        if (strcmp(argv[x], "-erp") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &LR.erp);
            }
        }

        if (strcmp(argv[x], "-cl") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%d", &LR.radio_climate);
            }
        }
        if (strcmp(argv[x], "-te") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%d", &ter);

                switch (ter) {
                    case 1:  // Water
                        terdic = 80;
                        tercon = 0.010;
                        break;

                    case 2:  // Marsh
                        terdic = 12;
                        tercon = 0.007;
                        break;

                    case 3:  // Farmland
                        terdic = 15;
                        tercon = 0.005;
                        break;

                    case 4:  // Mountain
                        terdic = 13;
                        tercon = 0.002;
                        break;
                    case 5:  // Desert
                        terdic = 13;
                        tercon = 0.002;
                        break;
                    case 6:  // Urban
                        terdic = 5;
                        tercon = 0.001;
                        break;
                }
                LR.eps_dielect = terdic;
                LR.sgm_conductivity = tercon;
            }
        }

        if (strcmp(argv[x], "-terdic") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &terdic);

                LR.eps_dielect = terdic;
            }
        }

        if (strcmp(argv[x], "-tercon") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') {
                sscanf(argv[z], "%lf", &tercon);

                LR.sgm_conductivity = tercon;
            }
        }

        if (strcmp(argv[x], "-hp") == 0) {
            // Horizontal polarisation (0)
            LR.pol = 0;
        }

        /*UDT*/
        if (strcmp(argv[x], "-udt") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) {
                udt_file = (char *)calloc(PATH_MAX + 1, sizeof(char));
                if (udt_file == NULL) return ENOMEM;
                strncpy(udt_file, argv[z], 253);
            }
        }

        /*Prop model */

        if (strcmp(argv[x], "-pm") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) {
                sscanf(argv[z], "%d", &propmodel);
            }
        }
        // Prop model variant eg. urban/suburban
        if (strcmp(argv[x], "-pe") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) {
                sscanf(argv[z], "%d", &pmenv);
            }
        }
        // Knife edge diffraction
        if (strcmp(argv[x], "-ked") == 0) {
            z = x + 1;
            knifeedge = 1;
        }

        // Normalise Path Profile chart
        if (strcmp(argv[x], "-ng") == 0) {
            z = x + 1;
            normalise = 1;
        }
        // Halve the problem
        if (strcmp(argv[x], "-haf") == 0) {
            z = x + 1;
            if (z <= y && argv[z][0]) {
                sscanf(argv[z], "%d", &haf);
            }
        }

        // Disable threads
        if (strcmp(argv[x], "-nothreads") == 0) {
            z = x + 1;
            printf("disabling threads\n");
            use_threads = false;
        }

        // Reliability % for ITM model
        if (strcmp(argv[x], "-rel") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) {
                sscanf(argv[z], "%lf", &LR.rel);
                LR.rel = LR.rel / 100;
            }
        }
        // Confidence % for ITM model
        if (strcmp(argv[x], "-conf") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) {
                sscanf(argv[z], "%lf", &LR.conf);
                LR.conf = LR.conf / 100;
            }
        }
        // LossColors for the -scf, -dcf and -lcf, depending on mode
        if (strcmp(argv[x], "-color") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0]) {
                color_file = (char *)calloc(PATH_MAX + 1, sizeof(char));
                if (color_file == NULL) return ENOMEM;
                strncpy(color_file, argv[z], 253);
            }
        }
    }

    /* ERROR DETECTION */
    if (tx_site[0].lat > 90 || tx_site[0].lat < -90) {
        fprintf(stderr, "ERROR: Either the lat was missing or out of range!");
        exit(EINVAL);
    }
    if (tx_site[0].lon > 360 || tx_site[0].lon < 0) {
        fprintf(stderr, "ERROR: Either the lon was missing or out of range!");
        exit(EINVAL);
    }
    if (LR.frq_mhz < 20 || LR.frq_mhz > 100000) {
        fprintf(stderr, "ERROR: Either the Frequency was missing or out of range!");
        exit(EINVAL);
    }
    if (LR.erp > 500000000) {
        fprintf(stderr, "ERROR: Power was out of range!");
        exit(EINVAL);
    }
    if (LR.eps_dielect > 80 || LR.eps_dielect < 0.1) {
        fprintf(stderr, "ERROR: Ground Dielectric value out of range!");
        exit(EINVAL);
    }
    if (LR.sgm_conductivity > 0.01 || LR.sgm_conductivity < 0.000001) {
        fprintf(stderr, "ERROR: Ground conductivity out of range!");
        exit(EINVAL);
    }

    if (tx_site[0].alt < 0 || tx_site[0].alt > 60000) {
        fprintf(stderr, "ERROR: Tx altitude above ground was too high: %f", tx_site[0].alt);
        exit(EINVAL);
    }
    if (altitudeLR < 0 || altitudeLR > 60000) {
        fprintf(stderr, "ERROR: Rx altitude above ground was too high!");
        exit(EINVAL);
    }

    if (!lidar) {
        if (G_ippd < 300 || G_ippd > 10000) {
            fprintf(stderr, "ERROR: resolution out of range!");
            exit(EINVAL);
        }
    }

    if (LR.contour_threshold < -200 || LR.contour_threshold > 240) {
        fprintf(stderr, "ERROR: Receiver threshold out of range (-200 / +240)");
        exit(EINVAL);
    }
    if (propmodel > 2 && propmodel < 7 && LR.frq_mhz < 150) {
        fprintf(stderr, "ERROR: Frequency too low for Propagation model");
        exit(EINVAL);
    }

    if (to_stdout == true && ppa != 0) {
        fprintf(stderr, "ERROR: Cannot write to stdout in ppa mode");
        exit(EINVAL);
    }

    if (resample > 10) {
        fprintf(stderr, "ERROR: Cannot resample higher than a factor of 10");
        exit(EINVAL);
    }
    if (LR.metric) {
        altitudeLR /= METERS_PER_FOOT; /* 10ft * 0.3 = 3.3m */
        LR.max_range /= KM_PER_MILE;   /* 10 / 1.6 = 7.5 */
        altitude /= METERS_PER_FOOT;
        tx_site[0].alt /= METERS_PER_FOOT; /* Feet to metres */
        tx_site[1].alt /= METERS_PER_FOOT; /* Feet to metres */
        LR.clutter /= METERS_PER_FOOT;     /* Feet to metres */
    }

    /* Ensure a trailing '/' is present in sdf_path */

    if (G_sdf_path[0]) {
        x = strlen(G_sdf_path);

        if (G_sdf_path[x - 1] != '/' && x != 0) {
            G_sdf_path[x] = '/';
            G_sdf_path[x + 1] = 0;
        }
    }

    x = 0;
    y = 0;

    min_lat = 70;
    max_lat = -70;

    min_lon = (double)floor(tx_site[0].lon);
    max_lon = (double)floor(tx_site[0].lon);

    txlat = (int)floor(tx_site[0].lat);
    txlon = (int)floor(tx_site[0].lon);

    if (txlat < min_lat) min_lat = txlat;

    if (txlat > max_lat) max_lat = txlat;

    if (LonDiff(txlon, min_lon) < 0.0) min_lon = txlon;

    if (LonDiff(txlon, max_lon) >= 0.0) max_lon = txlon;

    if (ppa == 1) {
        rxlat = (int)floor(tx_site[1].lat);
        rxlon = (int)floor(tx_site[1].lon);

        if (rxlat < min_lat) min_lat = rxlat;

        if (rxlat > max_lat) max_lat = rxlat;

        if (LonDiff(rxlon, min_lon) < 0.0) min_lon = rxlon;

        if (LonDiff(rxlon, max_lon) >= 0.0) max_lon = rxlon;
    }

    /* Load the required tiles */
    if (lidar) {
        if ((result = loadLIDAR(lidar_tiles, resample)) != 0) {
            fprintf(stderr,
                    "Couldn't find one or more of the "
                    "lidar files. Please ensure their paths are "
                    "correct and try again.\n");
            fprintf(stderr, "Error %d: %s\n", result, strerror(result));
            exit(result);
        }

        G_ppd = ((double)G_height / (G_max_north - G_min_north));
        G_yppd = G_ppd;

        if (G_debug) {
            fprintf(stderr, "ppd %lf, yppd %lf, %.4lf,%.4lf,%.4lf,%.4lf,%d x %d\n", G_ppd, G_yppd, G_max_north, G_min_west,
                    G_min_north, G_max_west, G_width, G_height);
            fflush(stderr);
        }

        if (G_yppd < G_ppd / 4) {
            fprintf(stderr, "yppd is bad! Check longitudes\n");
            fflush(stderr);
            exit(1);
        }

        if (G_delta > 0) {
            tx_site[0].lon += G_delta;
        }
    }
    else {
        // DEM first
        if (G_debug) {
            fprintf(stderr, "%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf\n", G_max_north, G_min_west, G_min_north, G_max_west, max_lon,
                    min_lon);
        }

        // max_lon-=3;

        if ((result = LoadTopoData(max_lon, min_lon, max_lat, min_lat)) != 0) {
            // This only fails on errors loading SDF tiles
            fprintf(stderr, "Error loading topo data\n");
            return result;
        }

        if (area_mode || topomap) {
            for (z = 0; z < txsites && z < max_txsites; z++) {
                /* "Ball park" estimates used to load any additional
                   SDF files required to conduct this analysis. */

                tx_range = sqrt(1.5 * (tx_site[z].alt + GetElevation(tx_site[z])));

                if (LRmap)
                    rx_range = sqrt(1.5 * altitudeLR);
                else
                    rx_range = sqrt(1.5 * altitude);

                /* deg_range determines the maximum
                   amount of topo data we read */

                deg_range = (tx_range + rx_range) / 57.0;

                /* max_range regulates the size of the
                   analysis.  A small, non-zero amount can
                   be used to shrink the size of the analysis
                   and limit the amount of topo data read by
                   ss  A large number will increase the
                   width of the analysis and the size of
                   the map. */

                if (LR.max_range == 0.0) LR.max_range = tx_range + rx_range;

                deg_range = LR.max_range / 57.0;

                // No more than 8 degs
                deg_limit = 3.5;

                if (fabs(tx_site[z].lat) < 70.0)
                    deg_range_lon = deg_range / cos(DEG2RAD * tx_site[z].lat);
                else
                    deg_range_lon = deg_range / cos(DEG2RAD * 70.0);

                /* Correct for squares in degrees not being square in miles */

                if (deg_range > deg_limit) deg_range = deg_limit;

                if (deg_range_lon > deg_limit) deg_range_lon = deg_limit;

                nortRxHin = (int)floor(tx_site[z].lat - deg_range);
                nortRxHax = (int)floor(tx_site[z].lat + deg_range);

                west_min = (int)floor(tx_site[z].lon - deg_range_lon);

                while (west_min < 0) west_min += 360;

                while (west_min >= 360) west_min -= 360;

                west_max = (int)floor(tx_site[z].lon + deg_range_lon);

                while (west_max < 0) west_max += 360;

                while (west_max >= 360) west_max -= 360;

                if (nortRxHin < min_lat) min_lat = nortRxHin;

                if (nortRxHax > max_lat) max_lat = nortRxHax;

                if (LonDiff(west_min, min_lon) < 0.0) min_lon = west_min;

                if (LonDiff(west_max, max_lon) >= 0.0) max_lon = west_max;
            }

            /* Load any additional SDF files, if required */

            if ((result = LoadTopoData(max_lon, min_lon, max_lat, min_lat)) != 0) {
                // This only fails on errors loading SDF tiles
                fprintf(stderr, "Error loading topo data\n");
                return result;
            }
        }
        G_ppd = (double)G_ippd;
        G_yppd = G_ppd;

        G_width = (unsigned)(G_ippd * ReduceAngle(G_max_west - G_min_west));
        G_height = (unsigned)(G_ippd * ReduceAngle(G_max_north - G_min_north));
    }

    G_dpp = 1 / G_ppd;
    G_mpi = G_ippd - 1;

    // User defined clutter file
    if (udt_file != NULL && (result = LoadUDT(udt_file)) != 0) {
        fprintf(stderr, "Error loading clutter file\n");
        return result;
    }

    // Enrich with Clutter
    if (strlen(clutter_file) > 1) {
        /*
        Clutter tiles cover 16 x 12 degs but we only need a fraction of that area.
        Limit by max_range / miles per degree (at equator)
        */
        if ((result = loadClutter(clutter_file, LR.max_range / 45, tx_site[0])) != 0) {
            fprintf(stderr, "Error, invalid or clutter file not found\n");
            return result;
        }
    }

    std::vector<struct dem_output> v;

    if (ppa == 0) {
        if (propmodel == 2) {  // Model 2 = LOS
            cropping = false;  // TODO: File is written in DoLOS() so this needs moving to PlotPropagation() to allow styling,
                               // cropping etc
            PlotLOSMap(&v, tx_site[0], altitudeLR, ano_filename, use_threads);
            DoLOS(&v, mapfile, kml, ngs, tx_site);
        }
        else {
            // 90% of effort here
            PlotPropagation(&v, tx_site[0], altitudeLR, ano_filename, propmodel, knifeedge, haf, pmenv, use_threads, LR);

            if (G_debug) {
                fprintf(stderr, "Finished PlotPropagation()\n");
                fflush(stderr);
            }

            if (cropping) {
                // CROPPING Factor determined in propPathLoss().
                // cropLon is the circle radius in pixels at it's widest (east/west)
                G_cropLon *= G_dpp;                       // pixels to degrees
                G_max_north = G_cropLat;                  // degrees
                G_max_west = G_cropLon + tx_site[0].lon;  // degrees west (positive)
                G_cropLat -= tx_site[0].lat;              // angle from tx to edge

                if (G_debug) {
                    fprintf(stderr, "Cropping 1: max_west: %.4f cropLat: %.4f cropLon: %.4f longitude: %.5f dpp %.7f\n",
                            G_max_west, G_cropLat, G_cropLon, tx_site[0].lon, G_dpp);
                    fflush(stderr);
                }
                G_width = (int)((G_cropLon * G_ppd) * 2);
                G_height = (int)((G_cropLat * G_ppd) * 2);

                if (G_debug) {
                    fprintf(stderr, "Cropping 2: max_west: %.4f cropLat: %.4f cropLon: %.7f longitude: %.5f width %d\n",
                            G_max_west, G_cropLat, G_cropLon, tx_site[0].lon, G_width);
                    fflush(stderr);
                }
                if (G_width > 3600 * 10 || G_cropLon < 0) {
                    fprintf(stderr, "FATAL BOUNDS! max_west: %.4f cropLat: %.4f cropLon: %.7f longitude: %.5f\n", G_max_west,
                            G_cropLat, G_cropLon, tx_site[0].lon);
                    return 0;
                }
            }

            // Write bitmap
            if (LR.erp == 0.0)
                DoPathLoss(&v, mapfile, geo, kml, ngs, tx_site, LR);
            else if (LR.dbm)
                DoRxdPwr(&v, (to_stdout == true ? NULL : mapfile), kml, ngs, tx_site, LR);
            else if ((result = DoSigStr(&v, mapfile, kml, ngs, tx_site, LR)) != 0)
                return result;
        }
        /*if(lidar){
                east=eastoffset;
                west=westoffset;
        }*/

        if (tx_site[0].lon > 0.0) tx_site[0].lon *= -1;

        if (tx_site[0].lon < -180.0) tx_site[0].lon += 360;

        if (cropping) {
            fprintf(stderr, "|%.6f", tx_site[0].lat + G_cropLat);
            fprintf(stderr, "|%.6f", tx_site[0].lon + G_cropLon);
            fprintf(stderr, "|%.6f", tx_site[0].lat - G_cropLat);
            fprintf(stderr, "|%.6f|", tx_site[0].lon - G_cropLon);
        }
        else {
            fprintf(stderr, "|%.6f", G_max_north);
            fprintf(stderr, "|%.6f", G_east);
            fprintf(stderr, "|%.6f", G_min_north);
            fprintf(stderr, "|%.6f|", G_west);
        }
        fprintf(stderr, "\n");
    }
    else {
        strncpy(tx_site[0].name, "Tx", 3);
        strncpy(tx_site[1].name, "Rx", 3);
        PlotPath(&v, tx_site[0], tx_site[1], 1, LR);
        PathReport(tx_site[0], tx_site[1], tx_site[0].filename, 0, propmodel, pmenv, rxGain, LR);
        // Order flipped for benefit of graph. Makes no difference to data.
        SeriesData(tx_site[1], tx_site[0], tx_site[0].filename, 1, normalise, LR);
    }
    fflush(stderr);

    return 0;
}

int scan_stdin()
{
    char buffer[1024];
    while (fgets(buffer, 1024, stdin)) {
        int argc = 0;
        char *argv[64];

        char *p2 = strtok(buffer, " ");
        while (p2 && argc < 64 - 1) {
            argv[argc++] = p2;
            p2 = strtok(NULL, " ");
        }
        argv[argc] = NULL;
        handle_args(argc, argv);
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int x, y, z = 0, lidar = 0, daemon = 0;

    if (strstr(argv[0], "signalserverHD")) {
        MAXPAGES = 32;       // was 9
        ARRAYSIZE = 115210;  // was 32410
        IPPD = 3600;
    }

    if (strstr(argv[0], "signalserverLIDAR")) {
        MAXPAGES = 100;  // 10x10
        lidar = 1;
        IPPD = 6000;  // will be overridden based upon file header...
    }

    strncpy(ss_name, "Signal Server\0", 14);

    if (argc == 1) {
        fprintf(stdout, "Version: %s %.2f (Built for %d DEM tiles at %d pixels)\n", ss_name, version, MAXPAGES, IPPD);
        fprintf(stdout, "License: GNU General Public License (GPL) version 2\n\n");
        fprintf(stdout, "Radio propagation simulator by Alex Farrant QCVS, 2E0TDW\n");
        fprintf(stdout, "Based upon SPLAT! by John Magliacane, KD2BD\n");
        fprintf(stdout, "Some feature enhancements/additions by Aaron A. Collins, N9OZB\n\n");
        fprintf(stdout,
                "Usage: signalserver [data options] [input options] [antenna options] [output options] -o outputfile\n\n");
        fprintf(stdout, "Data:\n");
        fprintf(stdout, "     -sdf Directory containing SRTM derived .sdf DEM tiles (may be .gz or .bz2)\n");
        fprintf(stdout, "     -lid ASCII grid tile (LIDAR) with dimensions and resolution defined in header\n");
        fprintf(stdout, "     -udt User defined point clutter as decimal co-ordinates: 'latitude,longitude,height'\n");
        fprintf(stdout, "     -clt MODIS 17-class wide area clutter in ASCII grid format\n");
        fprintf(stdout, "     -color File to pre-load .scf/.lcf/.dcf for Signal/Loss/dBm color palette\n");
        fprintf(stdout, "Input:\n");
        fprintf(stdout, "     -lat Tx Latitude (decimal degrees) -70/+70\n");
        fprintf(stdout, "     -lon Tx Longitude (decimal degrees) -180/+180\n");
        fprintf(stdout, "     -rla (Optional) Rx Latitude for PPA (decimal degrees) -70/+70\n");
        fprintf(stdout, "     -rlo (Optional) Rx Longitude for PPA (decimal degrees) -180/+180\n");
        fprintf(stdout, "     -f Tx Frequency (MHz) 20MHz to 100GHz (LOS after 20GHz)\n");
        fprintf(stdout, "     -erp Tx Total Effective Radiated Power in Watts (dBd) inc Tx+Rx gain. 2.14dBi = 0dBd\n");
        fprintf(stdout, "     -gc Random ground clutter (feet/meters)\n");
        fprintf(stdout, "     -m Metric units of measurement\n");
        fprintf(stdout, "     -te Terrain code 1-6 (optional - 1. Water, 2. Marsh, 3. Farmland,\n");
        fprintf(stdout, "          4. Mountain, 5. Desert, 6. Urban\n");
        fprintf(stdout, "     -terdic Terrain dielectric value 2-80 (optional)\n");
        fprintf(stdout, "     -tercon Terrain conductivity 0.01-0.0001 (optional)\n");
        fprintf(stdout, "     -cl Climate code 1-7 (optional - 1. Equatorial 2. Continental subtropical\n");
        fprintf(stdout, "          3. Maritime subtropical 4. Desert 5. Continental temperate\n");
        fprintf(stdout, "          6. Maritime temperate (Land) 7. Maritime temperate (Sea)\n");
        fprintf(stdout, "     -rel Reliability for ITM model (%% of 'time') 1 to 99 (optional, default 50%%)\n");
        fprintf(stdout, "     -conf Confidence for ITM model (%% of 'situations') 1 to 99 (optional, default 50%%)\n");
        fprintf(stdout, "     -resample Reduce Lidar resolution by specified factor (2 = 50%%)\n");
        fprintf(stdout, "Output:\n");
        fprintf(stdout, "     -o basename (Output file basename - required)\n");
        fprintf(stdout, "     -dbm Plot Rxd signal power instead of field strength in dBuV/m\n");
        fprintf(stdout, "     -rt Rx Threshold (dB / dBm / dBuV/m)\n");
        fprintf(stdout, "     -R Radius (miles/kilometers)\n");
        fprintf(stdout, "     -res Pixels per tile. 300/600/1200/3600 (Optional. LIDAR res is within the tile)\n");
        fprintf(stdout, "     -pm Propagation model. 1: ITM, 2: LOS, 3: Hata, 4: ECC33,\n");
        fprintf(stdout, "          5: SUI, 6: COST-Hata, 7: FSPL, 8: ITWOM, 9: Ericsson,\n");
        fprintf(stdout, "          10: Plane earth, 11: Egli VHF/UHF, 12: Soil\n");
        fprintf(stdout, "     -pe Propagation model mode: 1=Urban,2=Suburban,3=Rural\n");
        fprintf(stdout, "     -ked Knife edge diffraction (Already on for ITM)\n");
        fprintf(stdout, "Antenna:\n");
        fprintf(stdout, "     -ant (antenna pattern file basename+path for .az and .el files)\n");
        fprintf(stdout, "     -txh Tx Height (above ground)\n");
        fprintf(stdout, "     -rxh Rx Height(s) (optional. Default=0.1)\n");
        fprintf(stdout, "     -rxg Rx gain dBd (optional for PPA text report)\n");
        fprintf(stdout, "     -hp Horizontal Polarisation (default=vertical)\n");
        fprintf(stdout, "     -rot  (  0.0 - 359.0 degrees, default 0.0) Antenna Pattern Rotation\n");
        fprintf(stdout, "     -dt   ( -10.0 - 90.0 degrees, default 0.0) Antenna Downtilt\n");
        fprintf(stdout, "     -dtdir ( 0.0 - 359.0 degrees, default 0.0) Antenna Downtilt Direction\n");
        fprintf(stdout, "Debugging:\n");
        fprintf(stdout, "     -t Terrain greyscale background\n");
        fprintf(stdout, "     -dbg Verbose debug messages\n");
        fprintf(stdout, "     -ng Normalise Path Profile graph\n");
        fprintf(stdout, "     -haf Halve 1 or 2 (optional)\n");
        fprintf(stdout, "     -nothreads Turn off threaded processing\n");

        fflush(stdout);

        return 1;
    }

    /*
     * If we're not called as signalserverLIDAR we can allocate various
     * memory now. For LIDAR we need to wait until we've parsed
     * the headers in the .asc file to know how much memory to allocate...
     */
    if (!lidar) do_allocs();

    // these can stay globals
    G_gpsav = 0;
    G_sdf_path[0] = 0;
    G_path.length = 0;
    G_fzone_clearance = 0.6;
    G_earthradius = EARTHRADIUS;
    G_ippd = IPPD;  // default resolution

    y = argc - 1;

    // parse all the global flags here
    for (x = 1; x <= y; x++) {
        if (strcmp(argv[x], "-dbg") == 0) {
            G_debug = 1;
        }
        if (strcmp(argv[x], "-sdf") == 0) {
            z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-') strncpy(G_sdf_path, argv[z], 253);
        }

        if (strcmp(argv[x], "-daemon") == 0) {
            daemon = 1;
        }
    }

    if (daemon) {
        return scan_stdin();
    }

    return handle_args(argc - 1, argv + 1);
}
