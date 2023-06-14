#include "los.hh"

#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include <limits>
#include <vector>

#include "../signal-server.hh"
#include "cost.hh"
#include "ecc33.hh"
#include "egli.hh"
#include "ericsson.hh"
#include "fspl.hh"
#include "hata.hh"
#include "itwom3.0.hh"
#include "pel.hh"
#include "soil.hh"
#include "sui.hh"

#define NUM_SECTIONS 4

namespace {
pthread_t threads[NUM_SECTIONS];
unsigned int thread_count = 0;
pthread_mutex_t maskMutex;
bool ***processed;
bool has_init_processed = true;  // XXX hacked for now because we don't use threads

struct propagationRange {
    double min_west, max_west, min_north, max_north;
    double altitude;
    bool eastwest, los, use_threads;
    site source;
    unsigned char mask_value;
    FILE *fd;
    int propmodel, knifeedge, pmenv;
    struct output *out;
    const struct LR *LR;
};

void *rangePropagation(void *parameters)
{
    propagationRange *v = (propagationRange *)parameters;
    /*if (v->use_threads) {
        alloc_elev();
        alloc_path();
    }*/

    double minwest = G_dpp + (double)v->min_west;
    double lon = v->eastwest ? minwest : v->min_west;
    double lat = v->min_north;
    int y = 0;

    do {
        if (lon >= 360.0) lon -= 360.0;

        site edge;
        edge.lat = lat;
        edge.lon = lon;
        edge.alt = v->altitude;

        if (v->los)
            PlotLOSPath(v->out, v->source, edge, v->mask_value, v->LR);
        else
            PlotPropPath(v->out, v->source, edge, v->mask_value, v->fd, v->propmodel, v->knifeedge, v->pmenv, v->LR);

        ++y;
        if (v->eastwest)
            lon = minwest + (G_dpp * (double)y);
        else
            lat = (double)v->min_north + (G_dpp * (double)y);

    } while (v->eastwest ? (LonDiff(lon, (double)v->max_west) <= 0.0) : (lat < (double)v->max_north));

    /*if (v->use_threads) {
        free_elev();
        free_path();
    }*/
    return NULL;
}

void init_processed()
{
    // XXX hacked for now
    /*int i;
    int x;
    int y;

    processed = new bool **[MAXPAGES];
    for (i = 0; i < MAXPAGES; i++) {
            processed[i] = new bool *[G_ippd];
            for (x = 0; x < G_ippd; x++)
                    processed[i][x] = new bool [G_ippd];
    }

    for (i = 0; i < MAXPAGES; i++) {
            for (x = 0; x < G_ippd; x++) {
                    for (y = 0; y < G_ippd; y++)
                            processed[i][x][y] = false;
            }
    }*/

    has_init_processed = true;
}

bool can_process(double lat, double lon)
{
    /* Lines, text, markings, and coverage areas are stored in a
    mask that is combined with topology data when topographic
    maps are generated by ss.  This function sets bits in
    the mask based on the latitude and longitude of the area
    pointed to. */

    int x, y, indx;
    char found;
    bool rtn = false;

    return true;  // XXX hacked for now because we don't use threads

    for (indx = 0, found = 0; indx < MAXPAGES && found == 0;) {
        x = (int)rint(G_ppd * (lat - G_dem[indx].min_north));
        y = G_mpi - (int)rint(G_yppd * (LonDiff(G_dem[indx].max_west, lon)));

        if (x >= 0 && x <= G_mpi && y >= 0 && y <= G_mpi)
            found = 1;
        else
            indx++;
    }

    if (found) {
        /* As long as we only set this without resetting it we can
        check outside a mutex first without worrying about race
        conditions. But we must lock the mutex before updating the
        value. */

        if (!processed[indx][x][y]) {
            pthread_mutex_lock(&maskMutex);

            if (!processed[indx][x][y]) {
                rtn = true;
                processed[indx][x][y] = true;
            }

            pthread_mutex_unlock(&maskMutex);
        }
    }
    return rtn;
}

void beginThread(void *arg)
{
    if (!has_init_processed) init_processed();

    int rc = pthread_create(&threads[thread_count], NULL, rangePropagation, arg);
    if (rc)
        fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
    else
        ++thread_count;
}

void finishThreads()
{
    void *status;
    for (unsigned int i = 0; i < thread_count; i++) {
        int rc = pthread_join(threads[i], &status);
        if (rc) fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
    }
    thread_count = 0;
}
}  // namespace

/*
 * Acute Angle from Rx point to an obstacle of height (opp) and
 * distance (adj)
 */
static double incidenceAngle(double opp, double adj) { return atan2(opp, adj) * 180 / PI; }

/*
 * Knife edge diffraction:
 * This is based upon a recognised formula like Huygens, but trades
 * thoroughness for increased speed which adds a proportional diffraction
 * effect to obstacles.
 */
static double ked(double freq, double rxh, double dkm, double *elev)
{
    double obh, obd, rxobaoi = 0, d;

    obh = 0;  // Obstacle height
    obd = 0;  // Obstacle distance

    dkm = dkm * 1000;  // KM to metres

    // walk along path
    for (int n = 2; n < (dkm / elev[1]); n++) {
        d = (n - 2) * elev[1];  // no of points * delta = km

        // Find dip(s)
        if (elev[n] < obh) {
            // Angle from Rx point to obstacle
            rxobaoi = incidenceAngle((obh - (elev[n] + rxh)), d - obd);
        }
        else {
            // Line of sight or higher
            rxobaoi = 0;
        }

        // note the highest point
        if (elev[n] > obh) {
            obh = elev[n];
            obd = d;
        }
    }

    if (rxobaoi >= 0) {
        return (rxobaoi / (300 / freq)) + 3;  // Diffraction angle divided by wavelength (m)
    }
    else {
        return 1;
    }
}

void PlotLOSPath(struct output *out, struct site source, struct site destination, char mask_value, const struct LR *LR)
{
    /* This function analyzes the path between the source and
       destination locations. It determines which points along

       the path have line-of-sight visibility to the source.
       Points with line-of-sight visibility to the source are
           stored by setting bit 1 in the mask[][] array, which are
           displayed in green when PPM maps are later generated by ss. */

    bool bStop;
    int x, iCounter;
    double cos_angle, cos_test_angle, cos_horizon_angle, cos_limit_angle, rx_alt2;
    double distance, rx_alt, tx_alt, limit_alt, distance2, tx_alt2, test_alt, test_alt2, limit_alt2;

    ReadPath(source, destination, out);

    distance = 0.0;
    tx_alt = 0.0;
    distance2 = 0.0;
    tx_alt2 = 0.0;
    rx_alt2 = 0.0;
    test_alt = 0.0;
    test_alt2 = 0.0;
    limit_alt2 = 0.0;
    cos_horizon_angle = 1.0;
    bStop = false;
    iCounter = 0;

    /* altitude limit of 32808 feets or 10000 meters */
    limit_alt = G_earthradius + 32808.0;
    limit_alt2 = limit_alt * limit_alt;

    tx_alt = G_earthradius + source.alt + out->path.elevation[0];
    tx_alt2 = tx_alt * tx_alt;

    for (x = 0; (bStop == false) && (x < (out->path.length - 1)) && (out->path.distance[x] <= LR->max_range); x++) {
        if (x > 0) {
            distance = FEET_PER_MILE * out->path.distance[x];
            distance2 = distance * distance;

            rx_alt = G_earthradius + destination.alt + out->path.elevation[x];
            rx_alt2 = rx_alt * rx_alt;

            /* Calculate the cosine of the elevation between
               transmitter and receiver. */

            cos_angle = (distance2 + tx_alt2 - rx_alt2) / (2.0 * distance * tx_alt);

            if (cos_angle > 1.0) {
                cos_angle = 1.0;
            }

            if (cos_angle < -1.0) {
                cos_angle = -1.0;
            }

            test_alt =
                G_earthradius + (out->path.elevation[x] == 0.0 ? out->path.elevation[x] : out->path.elevation[x] + LR->clutter);
            test_alt2 = test_alt * test_alt;

            /* Calculate the cosine of the elevation between
               transmitter and this test point. */

            cos_test_angle = (distance2 + tx_alt2 - test_alt2) / (2.0 * distance * tx_alt);
        }
        else {
            cos_angle = -1.0;
            cos_test_angle = 1.0;
        }

        /* Compare these two angles to determine if
           an obstruction exists.
           Mark this point only if it hasn't been already marked */

        if ((cos_horizon_angle >= cos_angle) && ((GetMask(out, out->path.lat[x], out->path.lon[x]) & mask_value) == 0) &&
            (can_process(out->path.lat[x], out->path.lon[x]))) {
            OrMask(out, out->path.lat[x], out->path.lon[x], mask_value);
        }

        if (cos_test_angle < cos_horizon_angle) {
            cos_horizon_angle = cos_test_angle;
        }

        if ((x > 0) && (cos_horizon_angle < 0.0)) {
            if (iCounter > 10) {
                /* Check for Mount Everest in line-of-sight visibility */

                /* Calculate the cosine of the elevation between
                   transmitter and altitude limit. */

                cos_limit_angle = (distance2 + tx_alt2 - limit_alt2) / (2.0 * distance * tx_alt);

                if (cos_limit_angle > 1.0) {
                    cos_limit_angle = 1.0;
                }

                if (cos_limit_angle < -1.0) {
                    cos_limit_angle = -1.0;
                }

                if (cos_limit_angle > cos_horizon_angle) {
                    bStop = true;
                }

                iCounter = 0;
            }
            else {
                iCounter++;
            }
        }
    }
}

void PlotPropPath(struct output *out, struct site source, struct site destination, unsigned char mask_value, FILE *fd,
                  int propmodel, int knifeedge, int pmenv, const struct LR *LR)
{
    int x, y, ifs, ofs, errnum;
    char block = 0, strmode[100];
    double loss, azimuth, pattern = 0.0, xmtr_alt, dest_alt, xmtr_alt2, dest_alt2, cos_rcvr_angle, cos_test_angle = 0.0,
                          test_alt, elevation = 0.0, distance = 0.0, four_thirds_earth, field_strength = 0.0, rxp, dBm,
                          diffloss;
    struct site temp;
    float dkm;

    ReadPath(source, destination, out);

    four_thirds_earth = FOUR_THIRDS * EARTHRADIUS;

    for (x = 1; x < out->path.length - 1; x++)
        out->elev[x + 2] = (out->path.elevation[x] == 0.0 ? out->path.elevation[x] * METERS_PER_FOOT
                                                          : (LR->clutter + out->path.elevation[x]) * METERS_PER_FOOT);

    /* Copy ending points without clutter */

    out->elev[2] = out->path.elevation[0] * METERS_PER_FOOT;

    out->elev[out->path.length + 1] = out->path.elevation[out->path.length - 1] * METERS_PER_FOOT;

    /* Since the only energy the Longley-Rice model considers
       reaching the destination is based on what is scattered
       or deflected from the first obstruction along the path,
       we first need to find the location and elevation angle
       of that first obstruction (if it exists).  This is done
       using a 4/3rds Earth radius to match the model used by
       Longley-Rice.  This information is required for properly
       integrating the antenna's elevation pattern into the
       calculation for overall path loss. */
    // if(debug)
    //	fprintf(stderr,"four_thirds_earth %.1f source.alt %.1f path.elevation[0]
    //%.1f\n",four_thirds_earth,source.alt,path.elevation[0]);
    for (y = 2; (y < (out->path.length - 1) && out->path.distance[y] <= LR->max_range); y++) {
        /* Process this point only if it
           has not already been processed. */

        if ((GetMask(out, out->path.lat[y], out->path.lon[y]) & 248) != (mask_value << 3) &&
            can_process(out->path.lat[y], out->path.lon[y])) {
            char fd_buffer[64];
            int buffer_offset = 0;

            distance = FEET_PER_MILE * out->path.distance[y];
            xmtr_alt = four_thirds_earth + source.alt + out->path.elevation[0];
            dest_alt = four_thirds_earth + destination.alt + out->path.elevation[y];
            dest_alt2 = dest_alt * dest_alt;
            xmtr_alt2 = xmtr_alt * xmtr_alt;

            /* Calculate the cosine of the elevation of
               the receiver as seen by the transmitter. */

            cos_rcvr_angle = ((xmtr_alt2) + (distance * distance) - (dest_alt2)) / (2.0 * xmtr_alt * distance);

            if (cos_rcvr_angle > 1.0) cos_rcvr_angle = 1.0;

            if (cos_rcvr_angle < -1.0) cos_rcvr_angle = -1.0;

            if (G_got_elevation_pattern || fd != NULL) {
                /* Determine the elevation angle to the first obstruction
                   along the path IF elevation pattern data is available
                   or an output (.ano) file has been designated. */

                for (x = 2, block = 0; (x < y && block == 0); x++) {
                    distance = FEET_PER_MILE * out->path.distance[x];

                    test_alt = four_thirds_earth +
                               (out->path.elevation[x] == 0.0 ? out->path.elevation[x] : out->path.elevation[x] + LR->clutter);

                    /* Calculate the cosine of the elevation
                       angle of the terrain (test point)
                       as seen by the transmitter. */

                    cos_test_angle =
                        ((xmtr_alt2) + (distance * distance) - (test_alt * test_alt)) / (2.0 * xmtr_alt * distance);

                    if (cos_test_angle > 1.0) cos_test_angle = 1.0;

                    if (cos_test_angle < -1.0) cos_test_angle = -1.0;

                    /* Compare these two angles to determine if
                       an obstruction exists.  Since we're comparing
                       the cosines of these angles rather than
                       the angles themselves, the sense of the
                       following "if" statement is reversed from
                       what it would be if the angles themselves
                       were compared. */

                    if (cos_rcvr_angle >= cos_test_angle) block = 1;
                }

                if (block)
                    elevation = ((acos(cos_test_angle)) / DEG2RAD) - 90.0;
                else
                    elevation = ((acos(cos_rcvr_angle)) / DEG2RAD) - 90.0;
            }

            /* Determine attenuation for each point along the
               path using a prop model starting at y=2 (number_of_points = 1), the
               shortest distance terrain can play a role in
               path loss. */

            out->elev[0] = y - 1; /* (number of points - 1) */

            /* Distance between elevation samples */

            out->elev[1] = METERS_PER_MILE * (out->path.distance[y] - out->path.distance[y - 1]);

            if (out->path.elevation[y] < 1) {
                out->path.elevation[y] = 1;
            }

            dkm = (out->elev[1] * out->elev[0]) / 1000;  // km

            switch (propmodel) {
                case 1:
                    // Longley Rice ITM
                    point_to_point_ITM(source.alt * METERS_PER_FOOT, destination.alt * METERS_PER_FOOT, LR->eps_dielect,
                                       LR->sgm_conductivity, LR->eno_ns_surfref, LR->frq_mhz, LR->radio_climate, LR->pol,
                                       LR->conf, LR->rel, loss, strmode, out->elev, errnum);
                    break;
                case 3:
                    // HATA 1, 2 & 3
                    loss = HATApathLoss(LR->frq_mhz, source.alt * METERS_PER_FOOT,
                                        (out->path.elevation[y] * METERS_PER_FOOT) + (destination.alt * METERS_PER_FOOT), dkm,
                                        pmenv);
                    break;
                case 4:
                    // ECC33
                    loss = ECC33pathLoss(LR->frq_mhz, source.alt * METERS_PER_FOOT,
                                         (out->path.elevation[y] * METERS_PER_FOOT) + (destination.alt * METERS_PER_FOOT), dkm,
                                         pmenv);
                    break;
                case 5:
                    // SUI
                    loss = SUIpathLoss(LR->frq_mhz, source.alt * METERS_PER_FOOT,
                                       (out->path.elevation[y] * METERS_PER_FOOT) + (destination.alt * METERS_PER_FOOT), dkm,
                                       pmenv);
                    break;
                case 6:
                    // COST231-Hata
                    loss = COST231pathLoss(LR->frq_mhz, source.alt * METERS_PER_FOOT,
                                           (out->path.elevation[y] * METERS_PER_FOOT) + (destination.alt * METERS_PER_FOOT),
                                           dkm, pmenv);
                    break;
                case 7:
                    // ITU-R P.525 Free space path loss
                    loss = FSPLpathLoss(LR->frq_mhz, dkm, false);
                    break;
                case 8:
                    // ITWOM 3.0
                    point_to_point(source.alt * METERS_PER_FOOT, destination.alt * METERS_PER_FOOT, LR->eps_dielect,
                                   LR->sgm_conductivity,

                                   LR->eno_ns_surfref, LR->frq_mhz, LR->radio_climate, LR->pol, LR->conf, LR->rel, loss,
                                   strmode, out->elev, errnum);
                    break;
                case 9:
                    // Ericsson
                    loss = EricssonpathLoss(LR->frq_mhz, source.alt * METERS_PER_FOOT,
                                            (out->path.elevation[y] * METERS_PER_FOOT) + (destination.alt * METERS_PER_FOOT),
                                            dkm, pmenv);
                    break;
                case 10:
                    // Plane earth
                    loss = PlaneEarthLoss(dkm, source.alt * METERS_PER_FOOT,
                                          (out->path.elevation[y] * METERS_PER_FOOT) + (destination.alt * METERS_PER_FOOT));
                    break;
                case 11:
                    // Egli VHF/UHF
                    loss = EgliPathLoss(LR->frq_mhz, source.alt * METERS_PER_FOOT,
                                        (out->path.elevation[y] * METERS_PER_FOOT) + (destination.alt * METERS_PER_FOOT), dkm);
                    break;
                case 12:
                    // Soil
                    loss = SoilPathLoss(LR->frq_mhz, dkm, LR->eps_dielect);
                    break;

                default:
                    point_to_point_ITM(source.alt * METERS_PER_FOOT, destination.alt * METERS_PER_FOOT, LR->eps_dielect,
                                       LR->sgm_conductivity, LR->eno_ns_surfref, LR->frq_mhz, LR->radio_climate, LR->pol,
                                       LR->conf, LR->rel, loss, strmode, out->elev, errnum);
            }

            if (knifeedge == 1 && propmodel > 1) {
                diffloss = ked(LR->frq_mhz, destination.alt * METERS_PER_FOOT, dkm, out->elev);
                loss += (diffloss);  // ;)
            }
            // Key stage. Link dB for p2p is returned as 'loss'.

            temp.lat = out->path.lat[y];
            temp.lon = out->path.lon[y];
            temp.alt = std::numeric_limits<float>::min();

            azimuth = (Azimuth(source, temp));

            if (fd != NULL)
                buffer_offset += sprintf(fd_buffer + buffer_offset, "%.7f, %.7f, %.3f, %.3f, ", out->path.lat[y],
                                         out->path.lon[y], azimuth, elevation);

            /* If ERP==0, write path loss to alphanumeric
               output file.  Otherwise, write field strength
               or received power level (below), as appropriate. */

            if (fd != NULL && LR->erp == 0.0) buffer_offset += sprintf(fd_buffer + buffer_offset, "%.2f", loss);

            /* Integrate the antenna's radiation
               pattern into the overall path loss. */

            x = (int)rint(10.0 * (10.0 - elevation));

            if (x >= 0 && x <= 1000) {
                azimuth = rint(azimuth);

                pattern = (double)LR->antenna_pattern[(int)azimuth][x];

                if (pattern != 0.0) {
                    pattern = 20.0 * log10(pattern);
                    loss -= pattern;
                }
            }

            if (LR->erp != 0.0) {
                if (LR->dbm) {
                    /* dBm is based on EIRP (ERP + 2.14) */

                    rxp = LR->erp / (pow(10.0, (loss - 2.14) / 10.0));

                    dBm = 10.0 * (log10(rxp * 1000.0));

                    if (fd != NULL) buffer_offset += sprintf(fd_buffer + buffer_offset, "%.3f", dBm);

                    /* Scale roughly between 0 and 255 */

                    ifs = 200 + (int)rint(dBm);

                    if (ifs < 0) ifs = 0;

                    if (ifs > 255) ifs = 255;

                    ofs = GetSignal(out, out->path.lat[y], out->path.lon[y]);

                    if (ofs > ifs) ifs = ofs;

                    PutSignal(out, out->path.lat[y], out->path.lon[y], (unsigned char)ifs);
                }

                else {
                    field_strength = (139.4 + (20.0 * log10(LR->frq_mhz)) - loss) + (10.0 * log10(LR->erp / 1000.0));

                    ifs = 100 + (int)rint(field_strength);

                    if (ifs < 0) ifs = 0;

                    if (ifs > 255) ifs = 255;

                    ofs = GetSignal(out, out->path.lat[y], out->path.lon[y]);

                    if (ofs > ifs) ifs = ofs;

                    PutSignal(out, out->path.lat[y], out->path.lon[y], (unsigned char)ifs);

                    if (fd != NULL) buffer_offset += sprintf(fd_buffer + buffer_offset, "%.3f", field_strength);
                }
            }

            else {
                if (loss > 255)
                    ifs = 255;
                else
                    ifs = (int)rint(loss);

                ofs = GetSignal(out, out->path.lat[y], out->path.lon[y]);

                if (ofs < ifs && ofs != 0) ifs = ofs;

                PutSignal(out, out->path.lat[y], out->path.lon[y], (unsigned char)ifs);
            }

            if (fd != NULL) {
                if (block) buffer_offset += sprintf(fd_buffer + buffer_offset, " *");
                fprintf(fd, "%s\n", fd_buffer);
            }

            /* Mark this point as having been analyzed */

            PutMask(out, out->path.lat[y], out->path.lon[y],
                    (GetMask(out, out->path.lat[y], out->path.lon[y]) & 7) + (mask_value << 3));
        }
    }

    if (out->path.lat[y] > out->cropLat) out->cropLat = out->path.lat[y];

    if (y > out->cropLon) out->cropLon = y;

    // if(cropLon>180)
    //	cropLon-=360;
}

void PlotLOSMap(struct output *out, struct site source, double altitude, char *plo_filename, bool use_threads,
                const struct LR *LR)
{
    /* This function performs a 360 degree sweep around the
       transmitter site (source location), and plots the
       line-of-sight coverage of the transmitter on the ss
       generated topographic map based on a receiver located
       at the specified altitude (in feet AGL).  Results
       are stored in memory, and written out in the form
       of a topographic map when the WritePPM() function
       is later invoked. */

    static __thread unsigned char mask_value = 1;
    FILE *fd = NULL;

    if (plo_filename && plo_filename && plo_filename[0] != 0) fd = fopen(plo_filename, "wb");

    if (fd != NULL) {
        fprintf(fd, "%.3f, %.3f\t; max_west, min_west\n%.3f, %.3f\t; max_north, min_north\n", out->max_west, out->min_west,
                out->max_north, out->min_north);
    }

    // Four sections start here
    // Process north edge east/west, east edge north/south,
    // south edge east/west, west edge north/south
    double range_min_west[] = {out->min_west, out->min_west, out->min_west, out->max_west};
    double range_min_north[] = {out->max_north, out->min_north, out->min_north, out->min_north};
    double range_max_west[] = {out->max_west, out->min_west, out->max_west, out->max_west};
    double range_max_north[] = {out->max_north, out->max_north, out->min_north, out->max_north};
    propagationRange *r[NUM_SECTIONS];

    for (int i = 0; i < NUM_SECTIONS; ++i) {
        propagationRange *range = new propagationRange;
        r[i] = range;
        range->los = true;

        range->eastwest = (range_min_west[i] == range_max_west[i] ? false : true);
        range->min_west = range_min_west[i];
        range->max_west = range_max_west[i];
        range->min_north = range_min_north[i];
        range->max_north = range_max_north[i];

        range->use_threads = use_threads;
        range->altitude = altitude;
        range->source = source;
        range->mask_value = mask_value;
        range->fd = fd;
        range->out = out;
        range->LR = LR;

        if (use_threads)
            beginThread(range);
        else {
            if (!has_init_processed) init_processed();
            rangePropagation(range);
        }
    }

    if (use_threads) finishThreads();

    for (int i = 0; i < NUM_SECTIONS; ++i) {
        delete r[i];
    }

    switch (mask_value) {
        case 1:
            mask_value = 8;
            break;

        case 8:
            mask_value = 16;
            break;

        case 16:
            mask_value = 32;
    }
}

void PlotPropagation(struct output *out, struct site source, double altitude, char *plo_filename, int propmodel, int knifeedge,
                     int haf, int pmenv, bool use_threads, const struct LR *LR)
{
    static __thread unsigned char mask_value = 1;
    FILE *fd = NULL;

    if (LR->erp == 0.0 && G_debug)
        fprintf(stderr, "path loss");
    else {
        if (G_debug) {
            if (LR->dbm)
                fprintf(stderr, "signal power level");
            else
                fprintf(stderr, "field strength");
        }
    }
    if (G_debug) {
        fprintf(stderr, " contours of \"%s\" out to a radius of %.2f %s with Rx antenna(s) at %.2f %s AGL\n", source.name,
                LR->metric ? LR->max_range * KM_PER_MILE : LR->max_range, LR->metric ? "kilometers" : "miles",
                LR->metric ? altitude * METERS_PER_FOOT : altitude, LR->metric ? "meters" : "feet");
    }

    if (LR->clutter > 0.0 && G_debug)
        fprintf(stderr, "\nand %.2f %s of ground clutter", LR->metric ? LR->clutter * METERS_PER_FOOT : LR->clutter,
                LR->metric ? "meters" : "feet");

    if (plo_filename && plo_filename && plo_filename[0] != 0) fd = fopen(plo_filename, "wb");

    if (fd != NULL) {
        fprintf(fd, "%.3f, %.3f\t; max_west, min_west\n%.3f, %.3f\t; max_north, min_north\n", out->max_west, out->min_west,
                out->max_north, out->min_north);
    }

    // Four sections start here
    // Process north edge east/west, east edge north/south,
    // south edge east/west, west edge north/south
    double range_min_west[] = {out->min_west, out->min_west, out->min_west, out->max_west};
    double range_min_north[] = {out->max_north, out->min_north, out->min_north, out->min_north};
    double range_max_west[] = {out->max_west, out->min_west, out->max_west, out->max_west};
    double range_max_north[] = {out->max_north, out->max_north, out->min_north, out->max_north};
    propagationRange *r[NUM_SECTIONS];

    for (int i = 0; i < NUM_SECTIONS; ++i) {
        propagationRange *range = new propagationRange;
        r[i] = range;
        range->los = false;

        // Only process correct half
        if ((NUM_SECTIONS - i) <= (NUM_SECTIONS / 2) && haf == 1) continue;
        if ((NUM_SECTIONS - i) > (NUM_SECTIONS / 2) && haf == 2) continue;

        range->eastwest = (range_min_west[i] == range_max_west[i] ? false : true);
        range->min_west = range_min_west[i];
        range->max_west = range_max_west[i];
        range->min_north = range_min_north[i];
        range->max_north = range_max_north[i];

        range->use_threads = use_threads;
        range->altitude = altitude;
        range->source = source;
        range->mask_value = mask_value;
        range->fd = fd;
        range->propmodel = propmodel;
        range->knifeedge = knifeedge;
        range->pmenv = pmenv;
        range->out = out;
        range->LR = LR;

        if (use_threads)
            beginThread(range);
        else {
            if (!has_init_processed) init_processed();
            rangePropagation(range);
        }
    }

    if (use_threads) finishThreads();

    for (int i = 0; i < NUM_SECTIONS; ++i) {
        delete r[i];
    }

    if (fd != NULL) fclose(fd);

    if (mask_value < 30) mask_value++;
}

void PlotPath(struct output *out, struct site source, struct site destination, char mask_value, const struct LR *LR)
{
    /* This function analyzes the path between the source and
       destination locations.  It determines which points along
       the path have line-of-sight visibility to the source.
       Points along with path having line-of-sight visibility
       to the source at an AGL altitude equal to that of the
       destination location are stored by setting bit 1 in the
       mask[][] array, which are displayed in green when PPM
       maps are later generated by SPLAT!. */

    char block;
    int x, y;
    double cos_xmtr_angle, cos_test_angle, test_alt;
    double distance, rx_alt, tx_alt;

    ReadPath(source, destination, out);

    for (y = 0; y < out->path.length; y++) {
        /* Test this point only if it hasn't been already
           tested and found to be free of obstructions. */

        if ((GetMask(out, out->path.lat[y], out->path.lon[y]) & mask_value) == 0) {
            distance = FEET_PER_MILE * out->path.distance[y];
            tx_alt = G_earthradius + source.alt + out->path.elevation[0];
            rx_alt = G_earthradius + destination.alt + out->path.elevation[y];

            /* Calculate the cosine of the elevation of the
               transmitter as seen at the temp rx point. */

            cos_xmtr_angle = ((rx_alt * rx_alt) + (distance * distance) - (tx_alt * tx_alt)) / (2.0 * rx_alt * distance);

            for (x = y, block = 0; x >= 0 && block == 0; x--) {
                distance = FEET_PER_MILE * (out->path.distance[y] - out->path.distance[x]);
                test_alt = G_earthradius +
                           (out->path.elevation[x] == 0.0 ? out->path.elevation[x] : out->path.elevation[x] + LR->clutter);

                cos_test_angle =
                    ((rx_alt * rx_alt) + (distance * distance) - (test_alt * test_alt)) / (2.0 * rx_alt * distance);

                /* Compare these two angles to determine if
                   an obstruction exists.  Since we're comparing
                   the cosines of these angles rather than
                   the angles themselves, the following "if"
                   statement is reversed from what it would
                   be if the actual angles were compared. */

                if (cos_xmtr_angle >= cos_test_angle) block = 1;
            }

            if (block == 0) OrMask(out, out->path.lat[y], out->path.lon[y], mask_value);
        }
    }
}
