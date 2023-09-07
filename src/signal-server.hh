#ifndef _MAIN_HH_
#define _MAIN_HH_

#include <stdio.h>

#include "common.hh"

int ReduceAngle(SsFloat angle);
SsFloat LonDiff(SsFloat lon1, SsFloat lon2);
void * dec2dms(SsFloat decimal, char * string);
int PutMask(struct output * out, SsFloat lat, SsFloat lon, int value);
int OrMask(struct output * out, SsFloat lat, SsFloat lon, int value);
int GetMask(struct output * out, SsFloat lat, SsFloat lon);
void PutSignal(struct output * out, SsFloat lat, SsFloat lon, unsigned char signal);
unsigned char GetSignal(struct output * out, SsFloat lat, SsFloat lon);
SsFloat GetElevation(site const & location);
int AddElevation(SsFloat lat, SsFloat lon, SsFloat height, int size);
SsFloat Distance(site const & site1, site const & site2);
SsFloat Azimuth(site const & source, site const & destination);
SsFloat ElevationAngle(site const & source, site const & destination);
SsFloat ElevationAngle2(Path const & path,
                        site const & source,
                        site const & destination,
                        SsFloat er,
                        LR const & lr);
SsFloat ReadBearing(char * input);
void ObstructionAnalysis(Path const & path,
                         site const & xmtr,
                         site const & rcvr,
                         SsFloat f,
                         FILE * outfile,
                         LR const & lr);
void resize_elev(struct output & out);
int handle_args(int argc, char * argv[], output & ret_out);
int scan_stdin();
const char * version();

#endif /* _MAIN_HH_ */
