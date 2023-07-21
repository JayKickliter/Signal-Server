#include <stdio.h>
#include <string.h>

#include <fstream>
#include <iterator>

#include "signal-server.hh"

int main(int argc, char * argv[]) {
    if (argc == 1) {
        fprintf(
            stdout,
            "Version: Signal Server %s (Built for %d DEM tiles at %d pixels)\n",
            version(),
            MAXPAGES,
            IPPD);
        fprintf(stdout,
                "License: GNU General Public License (GPL) version 2\n\n");
        fprintf(stdout,
                "Radio propagation simulator by Alex Farrant QCVS, 2E0TDW\n");
        fprintf(stdout, "Based upon SPLAT! by John Magliacane, KD2BD\n");
        fprintf(stdout, "Some feature enhancements/additions by Aaron A. Collins, N9OZB\n\n");
        fprintf(stdout,
                "Usage: signalserver [data options] [input options] [antenna "
                "options] [output options] -o outputfile\n\n");
        fprintf(stdout, "Data:\n");
        fprintf(stdout, "     -sdf Directory containing SRTM derived .sdf DEM tiles (may be .gz or .bz2)\n");
        // fprintf(stdout, "     -lid ASCII grid tile (LIDAR) with dimensions
        // and resolution defined in header\n");
        fprintf(stdout,
                "     -udt User defined point clutter as decimal co-ordinates: "
                "'latitude,longitude,height'\n");
        fprintf(stdout, "     -clt MODIS 17-class wide area clutter in ASCII grid format\n");
        fprintf(stdout, "     -color File to pre-load .scf/.lcf/.dcf for Signal/Loss/dBm color palette\n");
        fprintf(stdout, "Input:\n");
        fprintf(stdout, "     -lat Tx Latitude (decimal degrees) -70/+70\n");
        fprintf(stdout, "     -lon Tx Longitude (decimal degrees) -180/+180\n");
        fprintf(stdout, "     -rla (Optional) Rx Latitude for PPA (decimal degrees) -70/+70\n");
        fprintf(stdout, "     -rlo (Optional) Rx Longitude for PPA (decimal degrees) -180/+180\n");
        fprintf(stdout, "     -f Tx Frequency (MHz) 20MHz to 100GHz (LOS after 20GHz)\n");
        fprintf(stdout,
                "     -erp Tx Total Effective Radiated Power in Watts (dBd) "
                "inc Tx+Rx gain. 2.14dBi = 0dBd\n");
        fprintf(stdout, "     -gc Random ground clutter (feet/meters)\n");
        fprintf(stdout, "     -m Metric units of measurement\n");
        fprintf(stdout, "     -te Terrain code 1-6 (optional - 1. Water, 2. Marsh, 3. Farmland,\n");
        fprintf(stdout, "          4. Mountain, 5. Desert, 6. Urban\n");
        fprintf(stdout,
                "     -terdic Terrain dielectric value 2-80 (optional)\n");
        fprintf(stdout,
                "     -tercon Terrain conductivity 0.01-0.0001 (optional)\n");
        fprintf(stdout, "     -cl Climate code 1-7 (optional - 1. Equatorial 2. Continental subtropical\n");
        fprintf(stdout, "          3. Maritime subtropical 4. Desert 5. Continental temperate\n");
        fprintf(stdout, "          6. Maritime temperate (Land) 7. Maritime temperate (Sea)\n");
        fprintf(stdout, "     -rel Reliability for ITM model (%% of 'time') 1 to 99 (optional, default 50%%)\n");
        fprintf(stdout,
                "     -conf Confidence for ITM model (%% of 'situations') 1 to "
                "99 (optional, default 50%%)\n");
        // fprintf(stdout, "     -resample Reduce Lidar resolution by specified factor (2 = 50%%)\n");
        fprintf(stdout, "Output:\n");
        fprintf(stdout, "     -o basename (Output file basename - required)\n");
        fprintf(stdout, "     -dbm Plot Rxd signal power instead of field strength in dBuV/m\n");
        fprintf(stdout, "     -rt Rx Threshold (dB / dBm / dBuV/m)\n");
        fprintf(stdout, "     -R Radius (miles/kilometers)\n");
        // fprintf(stdout, "     -res Pixels per tile. 300/600/1200/3600
        // (Optional. LIDAR res is within the tile)\n");
        fprintf(
            stdout,
            "     -pm Propagation model. 1: ITM, 2: LOS, 3: Hata, 4: ECC33,\n");
        fprintf(stdout, "          5: SUI, 6: COST-Hata, 7: FSPL, 8: ITWOM, 9: Ericsson,\n");
        fprintf(stdout,
                "          10: Plane earth, 11: Egli VHF/UHF, 12: Soil\n");
        fprintf(stdout, "     -pe Propagation model mode: 1=Urban,2=Suburban,3=Rural\n");
        fprintf(stdout,
                "     -ked Knife edge diffraction (Already on for ITM)\n");
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

    // these can stay globals
    G_gpsav           = 0;
    G_sdf_path[0]     = 0;
    G_fzone_clearance = 0.6;
    G_earthradius     = EARTHRADIUS;
    G_ippd            = IPPD; // default resolution
    // leave these as globals
    G_ppd  = (double)G_ippd;
    G_yppd = G_ppd;

    G_dpp       = 1 / G_ppd;
    G_mpi       = G_ippd - 1;
    bool daemon = false;

    int y = argc - 1;

    // parse all the global flags here
    for (int x = 1; x <= y; x++) {
        if (strcmp(argv[x], "-dbg") == 0) {
            G_debug = 1;
        }
        if (strcmp(argv[x], "-sdf") == 0) {
            int z = x + 1;

            if (z <= y && argv[z][0] && argv[z][0] != '-')
                strncpy(G_sdf_path, argv[z], 253);
        }

        if (strcmp(argv[x], "-daemon") == 0) {
            daemon = true;
        }
    }

    if (daemon) {
        return scan_stdin();
    }

    output out;
    int    ret = handle_args(argc - 1, argv + 1, out);

    if (!ret) {
        if (out.imagedata.size()) {
            std::ofstream                  png_out("out.png");
            std::ostream_iterator<uint8_t> png_out_iter(png_out);
            std::copy(out.imagedata.begin(), out.imagedata.end(), png_out_iter);
        }
    }
    return ret;
}
