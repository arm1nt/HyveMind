#! /bin/sh
# Based on the script provided by the Limine bootloader

set -e

INSTALL_PATH="$1"

usage() {
    echo "Usage: $0 <TARGET_INSTALL_DIRECTORY>"
    exit 1
}

check_install_path() {
    if test -z "$INSTALL_PATH"; then
        usage
    fi

    if test ! -d "$INSTALL_PATH"; then
        echo "Error: The specified installation directory '$INSTALL_PATH' does not exist!"
        exit 1
    fi
}

clone_repo_commit() {
    local repo_link="$1"
    local target_dir="$2"
    local commit_hash="$3"

    if test -d "$target_dir/.git"; then
        git -C "$target_dir" reset --hard
        git -C "$target_dir" clean -fd
        if ! git -C "$target_dir" -c advice.detachedHead=false checkout $commit_hash; then
            rm -rf "$target_dir"
        fi
    else
        if test -d "$target_dir"; then
            echo "error: '$target_dir' is not a Git repository" 1>&2
            exit 1
        fi
    fi
    if ! test -d "$target_dir"; then
        git clone $repo_link "$target_dir"
        if ! git -C "$target_dir" -c advice.detachedHead=false checkout $commit_hash; then
            rm -rf "$target_dir"
            exit 1
        fi
    fi
}


check_install_path

rm -f "$INSTALL_PATH"/.deps-installed

clone_repo_commit \
    https://github.com/osdev0/freestnd-c-hdrs-0bsd.git \
    "$INSTALL_PATH"/freestnd-c-hdrs \
    097259a899d30f0a4b7a694de2de5fdda942e923

clone_repo_commit \
    https://github.com/osdev0/cc-runtime.git \
    "$INSTALL_PATH"/cc-runtime \
    dae79833b57a01b9fd3e359ee31def69f5ae899b

clone_repo_commit \
    https://github.com/Limine-Bootloader/limine-protocol.git \
    "$INSTALL_PATH"/limine-protocol \
    80ef54bed402b8c0b672a707c1df4c532f3428ad

touch "$INSTALL_PATH"/.deps-installed

