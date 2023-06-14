#include "sigserve.h"

#include "../../../src/common.hh"
#include "signal-server/src/sigserve.rs.h"

extern int init(const char *sdf_path, bool debug);
extern int handle_args(int argc, char *argv[], output *ret_out);

namespace sigserve_wrapper {

int init(const char *sdf_path, bool debug) { return ::init(sdf_path, debug); };
Report handle_args(int argc, char *argv[])
{
    Report report;
    report.dbm = 0.0;
    report.loss = 0.0;
    report.field_strength = 0.0;

    output out;

    report.retcode = ::handle_args(argc, argv, &out);
    if (report.retcode) {
        return report;
    }

    report.dbm = out.dBm;
    report.loss = out.loss;
    report.field_strength = out.field_strength;

    for (auto &[a, b] : out.cluttervec) {
        report.cluttervec.push_back(a);
        report.cluttervec.push_back(b);
    }

    for (auto &[a, b] : out.referencevec) {
        report.referencevec.push_back(a);
        report.referencevec.push_back(b);
    }

    for (auto &[a, b] : out.fresnelvec) {
        report.fresnelvec.push_back(a);
        report.fresnelvec.push_back(b);
    }

    for (auto &[a, b] : out.fresnel60vec) {
        report.fresnel60vec.push_back(a);
        report.fresnel60vec.push_back(b);
    }

    for (auto &[a, b] : out.curvaturevec) {
        report.curvaturevec.push_back(a);
        report.curvaturevec.push_back(b);
    }

    for (auto &[a, b] : out.profilevec) {
        report.profilevec.push_back(a);
        report.profilevec.push_back(b);
    }

    return report;
}

}  // namespace sigserve_wrapper
