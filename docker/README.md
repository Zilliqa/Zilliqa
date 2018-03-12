#Zilliqa container

##Overview
This repository contains a Dockerfile that can built into a Docker image containing the Zilliqa binaries.

##Building the Docker image
Executing the following command will start building a Docker image of Zilliqa:

> docker build --rm -t zilliqa .

This build step will fetch the Zilliqa sources, gather it's dependencies and build Zilliqa from source. The result is a Docker image that can be used to start up a Docker container.

> Note: by default the 'master' branch of the Zilliqa repo will be built in 'Debug' mode. You can specify an other branch and/or configuration by providing build arguments like so:
> docker build --build-arg BRANCH=<branchname> --build-arg CONFIG=<Debug/Release> --rm -t zilliqa . 

##Running a Docker container
To start up a Docker container from the previously built Docker image, you can run the following command:

> docker run --rm -i -t zilliqa

This will spawn a Docker container (running Ubuntu 16.04) which contains the entire source- and buildtree of Zilliqa. From there, you can run the deamon, test scripts, etc...