#ifndef _ITWOM30_HH_
#define _ITWOM30_HH_

void point_to_point_ITM(float tht_m,
                        float rht_m,
                        float eps_dielect,
                        float sgm_conductivity,
                        float eno_ns_surfref,
                        float frq_mhz,
                        int radio_climate,
                        int pol,
                        float conf,
                        float rel,
                        float & dbloss,
                        char * strmode,
                        float * elev,
                        int & errnum);
void point_to_point(float tht_m,
                    float rht_m,
                    float eps_dielect,
                    float sgm_conductivity,
                    float eno_ns_surfref,
                    float frq_mhz,
                    int radio_climate,
                    int pol,
                    float conf,
                    float rel,
                    float & dbloss,
                    char * strmode,
                    float * elev,
                    int & errnum);

#endif /* _ITWOM30_HH_ */
