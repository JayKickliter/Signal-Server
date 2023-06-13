#ifndef _MAIN_HH_
#define _MAIN_HH_

#include <stdio.h>

#include <vector>

#include "common.hh"

int ReduceAngle(double angle);
double LonDiff(double lon1, double lon2);
void *dec2dms(double decimal, char *string);
int PutMask(struct output *out, double lat, double lon, int value);
int OrMask(struct output *out, double lat, double lon, int value);
int GetMask(struct output *out, double lat, double lon);
void PutSignal(struct output *out, double lat, double lon, unsigned char signal);
unsigned char GetSignal(struct output *out, double lat, double lon);
double GetElevation(struct site location);
int AddElevation(double lat, double lon, double height, int size);
double Distance(struct site site1, struct site site2);
double Azimuth(struct site source, struct site destination);
double ElevationAngle(struct site source, struct site destination);
void ReadPath(struct site source, struct site destination, struct output *out);
double ElevationAngle2(struct site source, struct site destination, double er, struct output *out, const struct LR LR);
double ReadBearing(char *input);
void ObstructionAnalysis(struct site xmtr, struct site rcvr, double f, FILE *outfile, struct output *out, struct LR LR);
void free_elev(void);
void free_path(void);
void alloc_elev(void);
void alloc_path(void);
int handle_args(int argc, char *argv[]);
int scan_stdin();
const char *version();

#endif /* _MAIN_HH_ */
