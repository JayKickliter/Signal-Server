#ifndef _OUTPUT_HH_
#define _OUTPUT_HH_

void DoPathLoss(std::vector<dem_output> *v, char *filename, unsigned char geo, unsigned char kml, unsigned char ngs, struct site *xmtr,
                unsigned char txsites);
int DoSigStr(std::vector<dem_output> *v, char *filename, unsigned char geo, unsigned char kml, unsigned char ngs, struct site *xmtr, unsigned char txsites);
void DoRxdPwr(std::vector<dem_output> *v, char *filename, unsigned char geo, unsigned char kml, unsigned char ngs, struct site *xmtr,
              unsigned char txsites);
void DoLOS(std::vector<dem_output> *v, char *filename, unsigned char geo, unsigned char kml, unsigned char ngs, struct site *xmtr, unsigned char txsites);
void PathReport(struct site source, struct site destination, char *name, char graph_it, int propmodel, int pmenv,
                double rxGain);
void SeriesData(struct site source, struct site destination, char *name, unsigned char fresnel_plot, unsigned char normalised);

#endif /* _OUTPUT_HH_ */
