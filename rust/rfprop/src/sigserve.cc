#include "sigserve.h"

#include "../../../src/common.hh"
#include "rfprop/src/sigserve.rs.h"

extern int
init(const char * sdf_path, bool debug);
extern int
handle_args(int argc, char * argv[], output & ret_out);

namespace sigserve_wrapper {

    int init(const char * sdf_path, bool debug) {
        return ::init(sdf_path, debug);
    };

    Report handle_args(int argc, char * argv[]) {
        Report report;
        output out;

        report.retcode = ::handle_args(argc, argv, out);
        if (report.retcode) {
            return report;
        }

        report.dbm            = out.dBm;
        report.loss           = out.loss;
        report.field_strength = out.field_strength;

        report.cluttervec.reserve(out.cluttervec.size());
        std::copy(out.cluttervec.begin(),
                  out.cluttervec.end(),
                  std::back_inserter(report.cluttervec));

        report.referencevec.reserve(out.referencevec.size());
        std::copy(out.referencevec.begin(),
                  out.referencevec.end(),
                  std::back_inserter(report.referencevec));

        report.fresnelvec.reserve(out.fresnelvec.size());
        std::copy(out.fresnelvec.begin(),
                  out.fresnelvec.end(),
                  std::back_inserter(report.fresnelvec));

        report.fresnel60vec.reserve(out.fresnel60vec.size());
        std::copy(out.fresnel60vec.begin(),
                  out.fresnel60vec.end(),
                  std::back_inserter(report.fresnel60vec));

        report.curvaturevec.reserve(out.curvaturevec.size());
        std::copy(out.curvaturevec.begin(),
                  out.curvaturevec.end(),
                  std::back_inserter(report.curvaturevec));

        report.profilevec.reserve(out.profilevec.size());
        std::copy(out.profilevec.begin(),
                  out.profilevec.end(),
                  std::back_inserter(report.profilevec));

        report.distancevec.reserve(out.distancevec.size());
        std::copy(out.distancevec.begin(),
                  out.distancevec.end(),
                  std::back_inserter(report.distancevec));

        report.image_data.reserve(out.imagedata.size());
        std::copy(out.imagedata.begin(),
                  out.imagedata.end(),
                  std::back_inserter(report.image_data));

        return report;
    }

} // namespace sigserve_wrapper
