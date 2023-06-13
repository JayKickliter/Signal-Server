mod sigserve;
use sigserve::call_sigserve;

fn main() {
    call_sigserve("-dbg -sdf data -lat 41.490298 -lon -81.699936 -txh 4 -f 900 -erp 20 -rxh 2 -rt -70 -dbm -m -o cleveland -R 5 -pm 4");
}
