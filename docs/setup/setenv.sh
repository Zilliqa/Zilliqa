if [ -f ~/.cargo/env ]; then
    source ~/.cargo/env
fi
if [ -f ~/.opam/opam-init/init.sh ]; then
    . ~/.opam/opam-init/init.sh
fi

export RUSTC_WRAPPER=~/.cargo/bin/sccache
export CMAKE_VERSION=3.24.2
export CCACHE_DIR=~/ccache
export VCPKG_ROOT=~/vcpkg
export PATH=$PATH:~/.local/bin
