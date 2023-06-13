#ifndef _OUTPUT_HH_
#define _OUTPUT_HH_

void DoPathLoss(struct output *out, char *filename, unsigned char geo, unsigned char kml, unsigned char ngs,
                struct site *xmtr, struct LR LR);
int DoSigStr(struct output *out, char *filename, unsigned char kml, unsigned char ngs, struct site *xmtr,
             const struct LR LR);
void DoRxdPwr(struct output *out, char *filename, unsigned char kml, unsigned char ngs, struct site *xmtr,
              const struct LR LR);
void DoLOS(struct output *out, char *filename, unsigned char kml, unsigned char ngs, struct site *xmtr);
void PathReport(struct site source, struct site destination, char *name, char graph_it, int propmodel, int pmenv, double rxGain,
                struct output *out, const struct LR LR);
void SeriesData(struct site source, struct site destination, char *name, unsigned char fresnel_plot, unsigned char normalised,
                struct output *out, const struct LR LR);

#endif /* _OUTPUT_HH_ */
