mod sigserve;
use sigserve::call_sigserve;

fn main() {
    call_sigserve("-dbg -sdf /home/jay/forks/Signal-Server/data -lat 41.490298 -lon -81.699936 -txh 4 -f 900 -erp 20 -rxh 2 -rt -70 -dbm -m -o cleveland -R 20 -t -pm 4 -lol").unwrap();
}
