import files;
import unix;
import sys;

string workspace = argv("workspace");

app (file out, file err) flamestore_run_master (string workspace) {
    "flamestore" "run" "--master" "--debug" "--workspace " workspace @stdout=out @stderr=err
}

app (file out, file err) flamestore_run_storage (string workspace, string storagespace, string size) {
    "./run-storage.sh" workspace storagespace size @stdout=out @stderr=err
}

app (file out, file err) flamestore_shutdown (string workspace) {
    "flamestore" "shutdown" "--workspace" workspace "--debug" @stdout=out @stderr=err
}

file o1<"out-master.txt">;
file e1<"err-master.txt">;
o1, e1 = flamestore_run_master(workspace);

file o2<"out-storage.txt">;
file e2<"err-storage.txt">;
o2, e2 = flamestore_run_storage(workspace, "/dev/shm", "1GB");

file o3<"out-storage.txt">;
file e3<"err-storage.txt">;
sleep(60) => o3, e3 = flamestore_shutdown(workspace);
