use std::path::Path;

const ARGS: &str = "-lat 41.490298 -lon -81.699936 -txh 4 -f 900 -erp 20 -rxh 2 -rt -70 -dbm -m -o cleveland -R 20 -t -pm 4 -lol";

fn main() {
    signal_server::init(&Path::new("/home/jay/forks/Signal-Server/data"), false).unwrap();
    println!("Calling signal-server with: {ARGS}");
    let report = signal_server::call_sigserve(ARGS);
    println!("{report:#?}");
}