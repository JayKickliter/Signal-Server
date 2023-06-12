#ifndef _OUTPUT_HH_
#define _OUTPUT_HH_

void DoPathLoss(std::vector<dem_output> *v, char *filename, unsigned char geo, unsigned char kml, unsigned char ngs, struct site *xmtr,
                unsigned char txsites, struct LR LR);
int DoSigStr(std::vector<dem_output> *v, char *filename, unsigned char geo, unsigned char kml, unsigned char ngs, struct site *xmtr, unsigned char txsites, const struct LR LR);
void DoRxdPwr(std::vector<dem_output> *v, char *filename, unsigned char geo, unsigned char kml, unsigned char ngs, struct site *xmtr,
              unsigned char txsites, const struct LR LR);
void DoLOS(std::vector<dem_output> *v, char *filename, unsigned char geo, unsigned char kml, unsigned char ngs, struct site *xmtr, unsigned char txsites);
void PathReport(struct site source, struct site destination, char *name, char graph_it, int propmodel, int pmenv,
                double rxGain, const struct LR LR);
void SeriesData(struct site source, struct site destination, char *name, unsigned char fresnel_plot, unsigned char normalised, const struct LR LR);

#endif /* _OUTPUT_HH_ */
