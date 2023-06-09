#ifndef _LOS_HH_
#define _LOS_HH_

#include <stdio.h>

#include "../common.hh"

void PlotLOSPath(std::vector<dem_output> *v, struct site source, struct site destination, char mask_value, FILE *fd);
void PlotPropPath(std::vector<dem_output> *v, struct site source, struct site destination, unsigned char mask_value, FILE *fd, int propmodel, int knifeedge,
                  int pmenv);
void PlotLOSMap(std::vector<dem_output> *v, struct site source, double altitude, char *plo_filename, bool use_threads);
void PlotPropagation(std::vector<dem_output> *v, struct site source, double altitude, char *plo_filename, int propmodel, int knifeedge, int haf, int pmenv,
                     bool use_threads);
void PlotPath(std::vector<dem_output> *v, struct site source, struct site destination, char mask_value);

#endif /* _LOS_HH_ */
