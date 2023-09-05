#ifndef _MAIN_HH_
#define _MAIN_HH_

#include <stdio.h>

#include "common.hh"

int ReduceAngle(float angle);
float LonDiff(float lon1, float lon2);
void * dec2dms(float decimal, char * string);
int PutMask(struct output * out, float lat, float lon, int value);
int OrMask(struct output * out, float lat, float lon, int value);
int GetMask(struct output * out, float lat, float lon);
void PutSignal(struct output * out, float lat, float lon, unsigned char signal);
unsigned char GetSignal(struct output * out, float lat, float lon);
float GetElevation(site const & location);
int AddElevation(float lat, float lon, float height, int size);
float Distance(site const & site1, site const & site2);
float Azimuth(site const & source, site const & destination);
float ElevationAngle(site const & source, site const & destination);
float ElevationAngle2(Path const & path,
                       site const & source,
                       site const & destination,
                       float er,
                       LR const & lr);
float ReadBearing(char * input);
void ObstructionAnalysis(Path const & path,
                         site const & xmtr,
                         site const & rcvr,
                         float f,
                         FILE * outfile,
                         LR const & lr);
void resize_elev(struct output & out);
int handle_args(int argc, char * argv[], output & ret_out);
int scan_stdin();
const char * version();

#endif /* _MAIN_HH_ */
