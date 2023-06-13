#ifndef _LOS_HH_
#define _LOS_HH_

#include <stdio.h>

#include "../common.hh"

void PlotLOSPath(struct output *out, struct site source, struct site destination, char mask_value, const struct LR LR);
void PlotPropPath(struct output *out, struct site source, struct site destination, unsigned char mask_value, FILE *fd,
                  int propmodel, int knifeedge, int pmenv, const struct LR LR);
void PlotLOSMap(struct output *out, struct site source, double altitude, char *plo_filename, bool use_threads, const struct LR LR);
void PlotPropagation(struct output *out, struct site source, double altitude, char *plo_filename, int propmodel,
                     int knifeedge, int haf, int pmenv, bool use_threads, const struct LR LR);
void PlotPath(struct output *out, struct site source, struct site destination, char mask_value, const struct LR LR);

#endif /* _LOS_HH_ */
