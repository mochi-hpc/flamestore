import files;

app (file out, file err) flamestore_run_master (string workspace) {
    "flamestore" "run" "--master" "--debug" "--workspace" workspace @stdout=out @stderr=err
}

app (file out, file err) flamestore_run_storage (string workspace, string storagepath) {
    "flamestore" "run" "--storage" "--debug" "--workspace" workspace "--path" storagepath @stdout=out @stderr=err
}

app (file out, file err) flamestore_format_and_run_storage (string workspace, string storagepath, int size) {
    "flamestore" "run" "--storage" "--debug" "--workspace" workspace "--path" storagepath "--size" size "--override" @stdout=out @stderr=err
}

app (file out, file err) flamestore_format_storage (string storagepath, int size) {
    "flamestore" "format" "--path" storagepath "--size" size "--override" @stdout=out @stderr=err
}

app (file out, file err) flamestore_shutdown (string workspace) {
    "flamestore" "shutdown" "--workspace" workspace "--debug" @stdout=out @stderr=err
}
