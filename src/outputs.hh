#ifndef _OUTPUT_HH_
#define _OUTPUT_HH_

void
DoPathLoss(struct output * out,
           unsigned char   geo,
           unsigned char   kml,
           unsigned char   ngs,
           struct site *   xmtr,
           LR const &      lr);
int
DoSigStr(struct output * out,
         unsigned char   kml,
         unsigned char   ngs,
         struct site *   xmtr,
         LR const &      lr);
void
DoRxdPwr(struct output * out,
         unsigned char   kml,
         unsigned char   ngs,
         struct site *   xmtr,
         LR const &      lr);
void
DoLOS(struct output * out, unsigned char kml, unsigned char ngs, struct site * xmtr);
void
PathReport(struct site     source,
           struct site     destination,
           char *          name,
           char            graph_it,
           int             propmodel,
           int             pmenv,
           double          rxGain,
           struct output * out,
           LR const &      lr);
void
SeriesData(struct site     source,
           struct site     destination,
           unsigned char   fresnel_plot,
           unsigned char   normalised,
           struct output * out,
           LR const &      lr);

#endif /* _OUTPUT_HH_ */
