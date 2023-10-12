# native

Ziliiqa Native Driver

Description

Create a program that will generate the required environment to launch a native version of the Zilliqa blockchain that will run on a local or server machine environment.

The Zilliqa binaries will be built using the build.sh script and will support native debugging at the source code level using local development tools of your choice.

Initial version is to run on a single host but can be extended later to run across any hosts if required.

The initial version is independent of any other Zilliqa technologies and has no dependency on any other Zilliqa toolsets.

The driver supports both the constants.xml configuration format and can utilize additional contract accounts as defined in isolated-server-accounts.json

To run the tool simply run the native.py with the following arguments or variations of:

-n 6 -d 5 -l 1--port 30303 --websocket= --transaction-sender=0
--ds-guard=4
--shard-guard=0
--bucket=zilliqa-devnet
--origin-server=/<yourdir>/dev/native/tmp
--multiplier-fanout=1
--out-dir=/<yourdir>/dev/native/tmp
--build-dir=/<yourdir>/Zilliqa/build
--skip-non-guard-ds
--isolated-server-accounts

A simple startup script

Startall.sh is provided as a first cut for launching the initial version


Prerequisites.

Create a read/write directory /etc/zilliqa on your local machine (Annoying but hardcoded entry in zilliqa source)

Out-dir is the path where the filesystem with binaries will be created, you need to run a web server there.   python3 -m http.server

Currently you need to run a localstack instance.

You can install localstack directly on your machine and start with localstack start


TODO

Fixup the hardhcoded path in the start.sh command, i usually run it inside by dir, I hope this is the last remaining defect with paths.