#ifndef _ITWOM30_HH_
#define _ITWOM30_HH_

void point_to_point_ITM(SsFloat tht_m,
                        SsFloat rht_m,
                        SsFloat eps_dielect,
                        SsFloat sgm_conductivity,
                        SsFloat eno_ns_surfref,
                        SsFloat frq_mhz,
                        int radio_climate,
                        int pol,
                        SsFloat conf,
                        SsFloat rel,
                        SsFloat & dbloss,
                        char * strmode,
                        SsFloat * elev,
                        int & errnum);
void point_to_point(SsFloat tht_m,
                    SsFloat rht_m,
                    SsFloat eps_dielect,
                    SsFloat sgm_conductivity,
                    SsFloat eno_ns_surfref,
                    SsFloat frq_mhz,
                    int radio_climate,
                    int pol,
                    SsFloat conf,
                    SsFloat rel,
                    SsFloat & dbloss,
                    char * strmode,
                    SsFloat * elev,
                    int & errnum);

#endif /* _ITWOM30_HH_ */
