# 
# Copyright (C) 2023 Zilliqa
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
