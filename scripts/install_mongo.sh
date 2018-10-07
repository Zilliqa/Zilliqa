function install_libmongoc() {
# install libmongoc-1.13.0
# see http://mongoc.org/libmongoc/current/installing.html
curl -OL https://github.com/mongodb/mongo-c-driver/releases/download/1.13.0/mongo-c-driver-1.13.0.tar.gz
tar xzf mongo-c-driver-1.13.0.tar.gz
cd mongo-c-driver-1.13.0
mkdir cmake-build
cd cmake-build
cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF ..
make
make install
}

function install_libmongocxx() {
# install libmongocxx-3.3.1
# see https://mongodb.github.io/mongo-cxx-driver/mongocxx-v3/installation/
curl -OL https://github.com/mongodb/mongo-cxx-driver/archive/r3.3.1.tar.gz
tar -xzf r3.3.1.tar.gz
cd mongo-cxx-driver-r3.3.1/build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
make EP_mnmlstc_core
make -j -l4
make install
}



echo "Installing Mongoc"
install_libmongoc;
sleep 2
echo "Installing Mongocxx"
install_libmongocxx;
