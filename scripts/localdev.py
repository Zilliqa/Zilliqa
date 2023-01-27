#!/usr/bin/env python3
# Copyright (C) 2019 Zilliqa
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

"""
This script supports development builds on Zilliqa.

You can currently do a number of things, mostly just automating what's in docs/local-network.md

setup       - create a kind cluster and set it up for development
teardown    - destroy the cluster
up          - bring the cluster up with the latest build (equiv to tilt up)
down        - take the cluster down (equiv to tilt down)

Explorer is at port 8110, HTTP proxy at 8080.

build <tag> - this builds a quick & dirty Zilliqa container image for local development

It's assumed that you have Scilla checked out as a sibling to Zilliqa.
We:

 - Create a temporary directory
 - Copy some stuff from "Docker" into it
 - Build Scilla
 - Copy scilla into it.
 - Build Zilliqa

We use a magic dockerfile which does very bad things to Python, but is rather faster to build
than the ordinary dockerfile.

This is called by Tiltfile.dev, which uses it to build a (cached) container of your latest sources
to run for localdev builds.
"""

import os
import sys
import shutil
import re
import tempfile
import subprocess
import time

ZILLIQA_DIR=os.path.join(os.path.dirname(__file__), "..")
SCILLA_DIR = os.path.join(ZILLIQA_DIR, "..", "scilla")
KEEP_WORKSPACE = False

class GiveUp(Exception):
    pass

def run_or_die(cmd, in_dir = None, env = None, in_background = False):
    if in_background:
        print(">&" + " ".join(cmd))
        subprocess.Popen(cmd)
        return None
    else:
        print("> " + " ".join(cmd))
        data = subprocess.check_output(cmd, cwd = in_dir, env = env)
        return data

def setup():
    run_or_die(["kind", "create", "cluster", "--name", "zqdev", "--config", "infra/kind-cluster.yaml"],
               in_dir = ZILLIQA_DIR)
    run_or_die(["kubectl", "config", "use-context", "kind-zqdev"])
    run_or_die(["kubectl", "apply", "-f", "https://raw.githubusercontent.com/kubernetes/ingress-nginx/main/deploy/static/provider/kind/deploy.yaml"])
    # we need to wait a bit for the resource to exist.
    done = False
    for i in range(0,5):
        try:
            run_or_die(["kubectl", "wait", "--namespace", "ingress-nginx", "--for=condition=ready","pod", "--selector=app.kubernetes.io/component=controller",
                        "--timeout=300s"])
            done = True
            break
        except:
            time.sleep(2)
    if not done:
        raise GiveUp("Couldn't build ingress")

def teardown():
    run_or_die(["kind", "delete", "cluster", "-n", "kind-zqdev"])

def run(which):
    run_or_die(["kubectl", "config", "use-context", "kind-zqdev"])
    kind_ip_addr = run_or_die(["docker", "container", "inspect", "zqdev-control-plane",
                               "--format", "{{ .NetworkSettings.Networks.kind.IPAddress }}"]).decode('utf-8').strip()

    # Oh, how I hate this .. but pidfiles are too easily corrupted.
    try:
        run_or_die(["killall", "mitmweb"])
    except:
        # Probably nothing was running.
        pass
    if which == "up":
        mitm_cmd = ["mitmweb",
                    "--mode",
                    f"reverse:http://{kind_ip_addr}",
                    "--modify-headers",
                    "/~q/Host/l2api.local.z7a.xyz",
                    "--no-web-open-browser"
                    ]
        run_or_die(mitm_cmd, in_dir = ZILLIQA_DIR, in_background = True)
    tilt_cmd = ["tilt", "--context", "kind-zqdev", "-f", "Tiltfile.dev", which]
    if which == "up":
        print("*** Monitor http requests via mitmweb at http://localhost:8081 ****")
        print("*** Monitor tilt at http://localhost/http requests via mitmweb at http://localhost:10350 ****")
        os.execvp('tilt', tilt_cmd)
    else:
        run_or_die(tilt_cmd, in_dir = ZILLIQA_DIR)

def build(args):
    if len(args) != 1:
        raise GiveUp("Need a single argument - the tag to build")
    tag = args[0]
    KEEP_WORKSACE = "KEEP_WORKSPACE" in os.environ
    workspace = os.path.join(ZILLIQA_DIR, "_localdev")
    print(f"> Using workspace {workspace}")
    # Let's start off by building Scilla, in case it breaks.
    run_or_die(["make"], in_dir = SCILLA_DIR)
    # OK. That worked. Now copy the relevant bits to our workspace
    os.makedirs(os.path.join(workspace, "scilla"), 0o755)
    shutil.copytree(os.path.join(SCILLA_DIR, "bin"),
                    os.path.join(workspace, "scilla", "bin"))
    shutil.copytree(os.path.join(SCILLA_DIR, "_build", "install", "default", "lib", "scilla", "stdlib"),
                    os.path.join(workspace, "scilla", "stdlib"))
    new_env = os.environ.copy()
    new_env["DOCKER_BUILDKIT"] = "1"
    run_or_die(["docker", "build", ".", "-t", tag, "-f", os.path.join(ZILLIQA_DIR, "docker", "Dockerfile.dev")], in_dir = ZILLIQA_DIR, env = new_env)
    print(f"> Built in workspace {workspace}")
    if not KEEP_WORKSPACE:
        print(f"> Removing workspace")
        shutil.rmtree(workspace)

if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) < 1:
        print(__doc__, file=sys.stderr)
        raise GiveUp("Syntax: localdev [cmd] <args..>")
    cmd = args[0]
    if cmd == "build":
        build(args[1:])
    elif cmd == "up":
        run("up")
    elif cmd == "down":
        run("down")
    elif cmd == "setup":
        setup()
    elif cmd == "teardown":
        teardown()
    else:
        raise GiveUp(f"Invalid command {cmd}")
