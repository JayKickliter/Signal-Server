#ifndef _OUTPUT_HH_
#define _OUTPUT_HH_

#include <cstdbool>

void DoPathLoss(struct output * out,
                unsigned char geo,
                unsigned char kml,
                unsigned char ngs,
                struct site * xmtr,
                LR const & lr);
int DoSigStr(struct output * out,
             unsigned char kml,
             unsigned char ngs,
             struct site * xmtr,
             LR const & lr);
void DoRxdPwr(struct output * out,
              unsigned char kml,
              unsigned char ngs,
              struct site * xmtr,
              LR const & lr);
void DoLOS(struct output * out,
           unsigned char kml,
           unsigned char ngs,
           struct site * xmtr);
void PathReport(Path const & path,
                struct site source,
                struct site destination,
                const char * name,
                char graph_it,
                int propmodel,
                int pmenv,
                double rxGain,
                struct output * out,
                LR const & lr);
void SeriesData(Path const & path,
                site const & src,
                site const & dst,
                bool fresnel_plot,
                bool normalised,
                struct output * out,
                LR const & lr);

#endif /* _OUTPUT_HH_ */
