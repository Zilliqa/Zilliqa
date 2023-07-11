#! /usr/bin/bash
# This is a shell script because one of the things it might want to do is upgrade python

TARGET_DIR=~/src/zilliqa
APT="sudo DEBIAN_FRONTEND=noninteractive apt"
APT_INSTALL="${APT} install -yqq"
APT_REMOVE="${APT} remove -yqq"
VERSION=0.1.5
CMAKE_VERSION=3.24.2
echo Installme ${VERSION}

# Install the key right off, so that the user can go and have a cup of tea whilst
# the rest of the build runs.
if [ ! -f ~/.ssh/id_rsa.pub ]; then 
    ssh-keygen -b 4096 -f ~/.ssh/id_rsa -N ""
    echo Install your pubkey into github and then press return
    cat ~/.ssh/id_rsa.pub
    read
fi

source ~/setenv.sh
export DEBIAN_FRONTEND=noninteractive
set -e
${APT} update
${APT} -yqq dist-upgrade
# Technically not required, but I wanted an editor to write this script in.
${APT_INSTALL} --no-install-recommends \
     emacs \
     software-properties-common \
     apache2-utils \
     autoconf \
    bison \
    build-essential \
    ca-certificates \
    ccache \
    clang-format \
    clang-tidy \
    cron \
    curl \
    dnsutils \
    gawk \
    gdb \
    git \
    htop \
    iproute2 \
    jq \
    lcov \
    libcurl4-openssl-dev \
    libffi-dev \
    libgmp-dev \
    libpcre3-dev \
    libsecp256k1-dev \
    libssl-dev \
    libtool \
    libxml2-utils \
    logrotate \
    net-tools \
    ninja-build \
    nload \
    ocaml \
    ocaml-dune \
    ocl-icd-opencl-dev \
    opam \
    openssh-client \
    patchelf \
    pigz \
    pkg-config \
    protobuf-compiler \
    python3 \
    python3-dev \
    python3-pip \
    rsync \
    rsyslog \
    tar \
    trickle \
    unzip \
    vim \
    wget \
    zip \
    zlib1g-dev

echo STEP 2
${APT_REMOVE} cmake python2

if ! cargo version >/dev/null 2>&1; then
    # Install rust
    echo Installing rust.
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs -o install_script.sh
    sh install_script.sh -yqq
fi
source ~/.cargo/env
(unset RUSTC_WRAPPER; cargo install sccache)

if ! ~/.local/bin/cmake --version >/dev/null 2>&1; then
    echo Installing CMake
    rm -f cmake-${CMAKE_VERSION}-*
    wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.sh
    chmod +x cmake-${CMAKE_VERSION}-Linux-x86_64.sh
    echo "739d372726cb23129d57a539ce1432453448816e345e1545f6127296926b6754 cmake-${CMAKE_VERSION}-Linux-x86_64.sh" | sha256sum -c 
    mkdir -p "${HOME}"/.local
    bash ./cmake-${CMAKE_VERSION}-Linux-x86_64.sh --skip-license --prefix="${HOME}"/.local/ 
    "${HOME}"/.local/bin/cmake --version \
    rm cmake-${CMAKE_VERSION}-Linux-x86_64.sh
fi

# Setup ccacche
sudo ln -fs "$(which ccache)" /usr/local/bin/gcc
sudo ln -fs "$(which ccache)" /usr/local/bin/g++
sudo ln -fs "$(which ccache)" /usr/local/bin/cc
sudo ln -fs "$(which ccache)" /usr/local/bin/c++

if [ ! -f /usr/local/bin/kubectl ]; then
    curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
    chmod +x kubectl
    sudo cp kubectl /usr/local/bin
fi


if [ ! -f ${VCPKG_ROOT}/vcpkg ]; then
  echo Installing vcpkg
  git clone https://github.com/microsoft/vcpkg ${VCPKG_ROOT}
  git -C ${VCPKG_ROOT} checkout ${VCPKG_COMMIT_OR_TAG}
  ${VCPKG_ROOT}/bootstrap-vcpkg.sh
fi


if ! dpkg -l docker-ce >/dev/null 2>&1; then
    echo Install docker
    for pkg in docker.io docker-doc docker-compose podman-docker containerd runc; do ${APT_REMOVE} $pkg; done
    ${APT_INSTALL} ca-certificates curl gnupg
    if [ ! -f /etc/apt/keyrings/docker.gpg ]; then 
	sudo install -m 0755 -d /etc/apt/keyrings
	curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
	sudo chmod a+r /etc/apt/keyrings/docker.gpg
    fi
    if [ ! -f /etc/apt/sources.list.d/docker.list ]; then 
	echo \
	    "deb [arch="$(dpkg --print-architecture)" signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
	    	  "$(. /etc/os-release && echo "$VERSION_CODENAME")" stable" | \
	    sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
    fi
    ${APT} update
    ${APT_INSTALL} docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
fi
sudo usermod -aG docker $USER


if [ ! -f /etc/apt/keyrings/helm.gpg ]; then
    curl https://baltocdn.com/helm/signing.asc | gpg --dearmor | sudo tee /etc/apt/keyrings/helm.gpg > /dev/null
    ${APT_INSTALL} apt-transport-https
fi
if [ ! -f /etc/apt/sources.list.d/helm-stable-debian.list ]; then
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/helm.gpg] https://baltocdn.com/helm/stable/debian/ all main" | sudo tee /etc/apt/sources.list.d/helm-stable-debian.list
fi

${APT} update
${APT_INSTALL} -yqq helm
if ! minikube version > /dev/null 2>&1; then
     echo Installing minikube
     curl -LO https://storage.googleapis.com/minikube/releases/latest/minikube_latest_amd64.deb
     sudo DEBIAN_FRONTEND=noninteractive dpkg -i minikube_latest_amd64.deb
fi

helm repo add localstack https://localstack.github.io/helm-charts
helm repo add grafana https://grafana.github.io/helm-charts 
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts

if [ ! -d ~/ccache ]; then
    mkdir ~/ccache
fi


# Yuck! But it works .. 
ssh-keygen -F github.com || ssh-keyscan github.com >>~/.ssh/known_hosts
mkdir -p ${TARGET_DIR}
cd ${TARGET_DIR}
if [ ! -d zilliqa ]; then
    git clone git@github.com:zilliqa/zilliqa
fi
if [ ! -d testnet ]; then
    git clone git@github.com:zilliqa/testnet
    (cd testnet; git checkout local-dev-minikube)
fi
if [ ! -d scilla ]; then
    git clone git@github.com:zilliqa/scilla
fi
export SCILLA_REPO_ROOT=${TARGET_DIR}/scilla
opam init </dev/null 2>&1 || true
eval $(opam env)
opam switch create 4.12.0 || true
opam switch 4.12.0 || true
(cd scilla; ./scripts/build_deps.sh && make opamdep-ci)


pip3 install -r ${TARGET_DIR}/testnet/requirements.txt
# (cd ${TARGET_DIR}; ./scripts/localdev.py isolated)
echo <<EOF
Now log out and back in again (or run newgrp docker) and:

cd ${TARGET_DIR}/zilliqa
./scripts/localdev.py setup
./scripts/localdev.py up

EOF


echo Installme ${VERSION} done.
