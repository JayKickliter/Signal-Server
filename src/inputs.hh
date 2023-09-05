#ifndef _INPUTS_HH_
#define _INPUTS_HH_

#include "common.hh"
extern char scf_file[255];

/* Resample input tiles to new resolution */
int resample_data(int scaling_factor);
int resize_data(int resolution);

int LoadSDF_SDF(char * name, int winfiles);
char * BZfgets(char * output, BZFILE * bzfd, unsigned length);
int LoadSDF_GZ(char * name);
char * GZfgets(char * output, gzFile gzfd, unsigned length);
int LoadSDF_BZ(char * name);
int LoadSDF(char * name, int winfiles);
int LoadPAT(char * az_filename, char * el_filename, struct LR & lr);
int LoadSignalColors(struct site xmtr);
int LoadLossColors(struct site xmtr);
int LoadDBMColors(struct site xmtr);
int LoadTopoData(float max_lon,
                 float min_lon,
                 float max_lat,
                 float min_lat,
                 struct output * out);
int LoadUDT(char * filename);
int loadLIDAR(char * filename, int resample, struct output * out);
int loadClutter(char * filename, float radius, struct site tx);
int averageHeight(int h, int w, int x, int y);
static const char AZ_FILE_SUFFIX[] = ".az";
static const char EL_FILE_SUFFIX[] = ".el";

#endif /* _INPUTS_HH_ */
