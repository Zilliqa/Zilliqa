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

It should work on Linux or (recent) OS X/amd64; OS X/arm64 is untested.

You can currently do a number of things, mostly just automating what's in docs/local-network.md

podman-setup    - Setup podman machine for OS X
podman-teardown - Tear down podman machine on OS X
setup           - create a kind cluster and set it up for development
teardown        - destroy the cluster
up              - bring the cluster up with the latest build (equiv to tilt up)
down            - take the cluster down (equiv to tilt down)
reup            - Down, then up - useful when you've changed source and want the results to be reflected.

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

For kubectl context:

 kubectl config use-context kind-zqdev

"""

import os
import sys
import shutil
import re
import tempfile
import subprocess
import time
import pathlib
import json
import glob
import json
import time

ZILLIQA_DIR=os.path.join(os.path.dirname(__file__), "..")
SCILLA_DIR = os.path.join(ZILLIQA_DIR, "..", "scilla")
KEEP_WORKSPACE = True
CONFIG = None

class Config:
    def __init__(self):
        self.default_env = { "LOCALDEV" : "1" , "FAST_BUILD" : "1"}
        self.cache_dir = os.path.join(pathlib.Path.home(), ".cache", "zilliqa_localdev");
        self.pods_to_start = {
            "dsguard": 4,
            "lookup": 2,
            "multiplier": 1,
            "normal": 4,
            "seedpub": 1
        }
        if sys.platform == "darwin":
            print(f"You are running on OS X .. using podman by setting \nexport KIND_EXPERIMENTAL_PROVIDER=podman\n");
            self.default_env['KIND_EXPERIMENTAL_PROVIDER'] = "podman"
            # This allows localstack to start - otherwise it gets weird chwon / subuid errors.
            self.default_env["PODMAN_USERNS"] = "host"
            self.default_env["BUILDAH_LAYERS"] = "true"
            self.using_podman = True
            self.using_local_registry = True
            self.docker_binary = "podman"
            self.is_osx = True
        else:
            self.using_podman = False
            self.using_local_registry = True
            self.docker_binary = "docker"
            self.is_osx = False
        self.keep_workspace = True # "KEEP_WORKSPACE" in os.environ

    def setup(self):
        """
        State modifying initialisation for the configuration structure.
        """
        try:
            os.makedirs(self.cache_dir, 0o755)
        except:
            pass

class GiveUp(Exception):
    pass

class Pidfile:
    def __init__(self, name):
        self._name = name

    def get_file_name(self):
        return os.path.join(CONFIG.cache_dir, f"{self._name}.pid")

    def put(self, pidval):
        file_name = self.get_file_name()
        with open(file_name, 'w') as f:
            f.write(f"{pidval}")

    def get(self):
        file_name = self.get_file_name()
        if os.path.exists(file_name):
            with open(file_name, 'r') as f:
                val = f.read()
            if val is not None and len(val) > 0:
                return int(val)
        else:
            return None

    def zap(self):
        try:
            os.unlink(self.get_file_name())
        except:
            pass

def run_or_die(cmd, in_dir = None, env = None, in_background = False, pid_name = None, capture_output = False, allow_failure = False):
    if env is None:
        env_to_use = os.environ.copy()
        env_to_use.update(CONFIG.default_env)
    else:
        env_to_use = CONFIG.default_env.copy()
        env_to_use.update(env)

    try:
        if in_background:
            print(">&" + " ".join(cmd))
            proc = subprocess.Popen(cmd, cwd = in_dir, env = env_to_use)
            if pid_name is not None:
                pid_obj = Pidfile(pid_name)
                print(f">& process {pid_name} has pid {proc.pid}");
                pid_obj.put(proc.pid)
            return None
        else:
            print("> " + " ".join(cmd))
            if capture_output:
                data = subprocess.check_output(cmd, cwd = in_dir, env = env_to_use)
                return data
            else:
                # Magic to let us stream the output..
                subprocess.check_call(cmd, cwd = in_dir, env = env_to_use)
                return None
    except Exception as e:
        if allow_failure:
            return None
        else:
            raise e


def build_pod_services():
    TEMPLATE = """
--- 
apiVersion: v1
kind: Service
metadata:
  name: svc-devnet-{name}-{index}
  labels:
    testnet: devnet
    type: {name}
spec:
  ports:
    - port: 30303
      name: zilliqa-app
  selector:
    statefulset.kubernetes.io/pod-name: devnet-{name}-{index}
"""
    a_file = tempfile.NamedTemporaryFile(suffix = "nodeservices", prefix="localdev", delete = False)
    name_to_load = a_file.name
    print(f"> Writing node service resources to {name_to_load} .. ")
    for (k,v) in CONFIG.pods_to_start.items():
        for i in range(0,v):
            data = TEMPLATE.format(name = k, index = i)
            a_file.write(data.encode('utf-8'))
    a_file.close()
    print("> Applying {name_to_load}")
    run_or_die(["kubectl", "apply", "-f", name_to_load], in_dir = ZILLIQA_DIR)

def refine_tag_name(tag):
    return tag
#    if tag.startswith("localhost:5001/") or tag.startswith("127.0.0.1:5001/"):
#        return tag
#    else:
#        return f"localhost:5001/{tag}"

def setup_podman():
    run_or_die(["podman", "machine", "init" , "--cpus=8", "--memory=16384", "--disk-size=196"])
    # This is necessary because Zilliqa requires various files in /proc/sys/net/core, which aren't exposed in rootless configurations.
    run_or_die(["podman", "machine", "set", "--rootful" ])
    run_or_die(["podman", "machine", "start"])

def teardown_podman():
    run_or_die(["podman", "machine", "stop"])
    run_or_die(["podman", "machine", "rm", "-f"])

def start_local_registry():
    result = sanitise_output(run_or_die([ CONFIG.docker_binary, "inspect", "-f={{.State.Running}}", "kind-registry"], allow_failure = True, capture_output = True))
    if result != "true":
        print("Registry not running - starting it .. ")
        run_or_die([ CONFIG.docker_binary, "run", "-d", "--restart=always", "-p", "127.0.0.1:5001:5000", "--name", "kind-registry", "registry:2"])

def configure_local_registry():
    result = sanitise_output(run_or_die([ CONFIG.docker_binary , "inspect", "-f={{json .NetworkSettings.Networks.kind}}", "kind-registry"], capture_output = True))
    if result == 'null':
        print("Connect registry to cluster network ..");
        run_or_die([ CONFIG.docker_binary, "network", "connect", "kind", "kind-registry" ])
    # Now document it.
    # run_or_die(["kubectl", "apply", "-f", "infra/k8s/local-registry.yaml"], in_dir = ZILLIQA_DIR)
    # .. and tell kind about it.
    # Faster if we do this locally - so only do this on OS X
    if CONFIG.using_local_registry:
        some_node_data = sanitise_output(run_or_die(["kind", "get", "nodes", "-n", "zqdev" ], capture_output = True))
        some_nodes = some_node_data.split()
        for node in some_nodes:
            print(f"Registering local repo for node {node}")
            run_or_die(["kubectl", "annotate", "node", node,  "kind.x-k8s.io/registry=localhost:5001" ])

def setup():
    start_local_registry()
    if CONFIG.using_local_registry:
        create_config = "kind-cluster-local-registry.yaml"
    else:
        create_config = "kind-cluster.yaml"
    run_or_die(["kind", "create", "cluster", "--name", "zqdev", "--config", f"infra/{create_config}"],
               in_dir = ZILLIQA_DIR)
    run_or_die(["kubectl", "config", "use-context", "kind-zqdev"])
    if CONFIG.using_local_registry:
        configure_local_registry()
    if False:
        run_or_die(["kubectl", "apply", "-f", "infra/k8s/nginx/deploy.yaml"], in_dir = ZILLIQA_DIR)
        # we need to wait a bit for the resource to exist.
        done = False
        # 0,5 turned out to be too small if you're pulling containers on creation - rrw 2023-02-13
        for i in range(0,20):
            try:
                run_or_die(["kubectl", "wait", "--namespace", "ingress-nginx", "--for=condition=ready","pod", "--selector=app.kubernetes.io/component=controller",
                            "--timeout=300s"])
                done = True
                break
            except:
                time.sleep(2)
        if not done:
            raise GiveUp("Couldn't build ingress")
    build_pod_services()
    print("All done.  You can now run localdev up");

def teardown():
    run_or_die(["kind", "delete", "cluster", "-n", "zqdev"])

def kill_mitmweb():
    if CONFIG.is_osx:
        print("> killall doesn't work on OS X .. using pidfile")
        pidfile = Pidfile("mitmweb")
        pid = pidfile.get()
        if pid is not None:
            print(f"> Found pid {pid}")
            try:
                os.kill(pid, 9)
            except Exception as e:
                print(f"> cannot kill - {e}")
                pidfile.zap()
                pass
    else:
        try:
            run_or_die(["killall", "mitmweb"])
        except:
            # Probably nothing was running.
            pass

def sanitise_output(some_output):
    """Strip and decode process output and return something sane"""
    if some_output is None:
        return None
    else:
        return some_output.decode('utf-8').strip()

def log(rest):
    logfile = rest[0]
    with open(logfile, 'r') as f:
        result = json.load(f)
    logs = result['view']['logList']['segments']
    filters = [*map(re.compile, rest[1:])]

    def match_filters(span):
        if len(filters) == 0:
            return True
        for f in filters:
            if f.search(span):
                return True
        return False
    
    for v in logs:
        spanId = v.get('spanId')
        if spanId is None:
            continue
        if match_filters(spanId):
            time = v.get('time')
            text = v.get('text').strip()
            
            print(f"{spanId:20} {time} {text}")

def run(which):
    run_or_die(["kubectl", "config", "use-context", "kind-zqdev"])
    kind_ip_addr = sanitise_output(run_or_die([CONFIG.docker_binary, "container", "inspect", "zqdev-control-plane",
                               "--format", "{{ .NetworkSettings.Networks.kind.IPAddress }}"], capture_output = True))

    print(f"> kind ip address {kind_ip_addr}")
    if kind_ip_addr is None or len(kind_ip_addr) == 0:
        raise GiveUp("Cannot find IP address of kind cluster: give up");
    kill_mitmweb()
    if which == "up":
        # Try to work out if there are any pods left.
        while True:
            result = run_or_die(["kubectl", "get", "pod", "-o", "json"], capture_output = True)
            some_json = json.loads(result)
            if len(some_json['items']) == 0:
                print("Pods have died. Continuing")
                break
            else:
                the_pods = [ val['metadata']['name'] for val in some_json['items'] ]
                for pod in the_pods:
                    print(f"Killing pod {pod}");
                    run_or_die(["kubectl", "delete", "pod", pod, "--grace-period", "0", "--force"], allow_failure = True)
                print(f"Waiting for pods to die: {' '.join(the_pods)}")
                time.sleep(2.0)

        mitm_cmd = ["mitmweb",
                    "--mode",
                    f"reverse:http://{kind_ip_addr}:5300",
                    "--modify-headers",
                    "/~q/Host/l2api.local.z7a.xyz",
                    "--no-web-open-browser"
                    ]
        run_or_die(mitm_cmd, in_dir = ZILLIQA_DIR, in_background = True ,pid_name = "mitmweb")
    tilt_cmd = ["tilt", "--context", "kind-zqdev", "-f", "Tiltfile", which]
    if which == "up":
        info = {
            "endpoints" : {
                "api" : "http://localhost:5302" ,
                "mitwmweb" : "http://localhost:8081",
                "tilt" : "http://localhost:10350",
                "devex" : "http://localhost:8110"
            }
        }
        print(json.dumps(info))
        #print("*** Send API requests to http://localhost:5302 ***")
        #print("*** Monitor http requests via mitmweb at http://localhost:8081 ****")
        #print("*** Monitor tilt at http://localhost/http requests via mitmweb at http://localhost:10350 ****")
        env_to_use = os.environ.copy()
        env_to_use.update(CONFIG.default_env)
        os.execvpe('tilt', tilt_cmd, env_to_use )
    else:
        run_or_die(tilt_cmd, in_dir = ZILLIQA_DIR)

def build_lite(args):
    if len(args) != 1:
        raise GiveUp("Need a single argument - the tag to build")
    tag = refine_tag_name(args[0])
    workspace = os.path.join(ZILLIQA_DIR, "_localdev")
    print(f"> Using workspace {workspace}")
    print(f"> Startup: removing workspace to avoid pollution ...")
    try:
        shutil.rmtree(workspace)
    except:
        pass
    # Let's start off by building Scilla, in case it breaks.
    run_or_die(["make"], in_dir = SCILLA_DIR)
    run_or_die(["./build.sh"], in_dir = ZILLIQA_DIR)
    run_or_die(["cargo", "build", "--release", "--package", "evm-ds"], in_dir =
               os.path.join(ZILLIQA_DIR, "evm-ds"))
    # OK. That worked. Now copy the relevant bits to our workspace
    try:
        os.makedirs(os.path.join(workspace, "scilla"), 0o755)
    except:
        pass
    shutil.copytree(os.path.join(SCILLA_DIR, "bin"),
                    os.path.join(workspace, "scilla", "bin"), dirs_exist_ok = True)
    shutil.copytree(os.path.join(SCILLA_DIR, "_build", "install", "default", "lib", "scilla", "stdlib"),
                    os.path.join(workspace, "scilla", "stdlib"), dirs_exist_ok = True)
    shutil.copytree(os.path.join(ZILLIQA_DIR, "scripts"),
                    os.path.join(workspace, "zilliqa", "scripts"), dirs_exist_ok = True)
    shutil.copytree(os.path.join(ZILLIQA_DIR, "build", "bin"),
                    os.path.join(workspace, "zilliqa", "build", "bin"), dirs_exist_ok = True)
    shutil.copytree(os.path.join(ZILLIQA_DIR, "build","lib"),
                    os.path.join(workspace, "zilliqa", "build", "lib"), dirs_exist_ok = True)
    tgt_bin_dir = os.path.join(workspace, "zilliqa", "build", "bin")
    tgt_lib_dir = os.path.join(workspace, "zilliqa", "build", "lib")
    for strip_dir in [ tgt_bin_dir, tgt_lib_dir ]:
        all_files = glob.glob(strip_dir + r'/*')
        for f in all_files:
            print(f)
            run_or_die(["strip", f])

    lib_tgt = os.path.join(workspace, "zilliqa", "lib")
    try:
        os.makedirs(lib_tgt, 0o755)
    except:
        pass
    src_name = os.path.join(ZILLIQA_DIR, "build", "vcpkg_installed", "x64-linux-dynamic", "lib")
    in_files = glob.glob(src_name + r'/*.so*')
    for f in in_files:
        file_name = os.path.split(f)[1]
        full_tgt = os.path.join(lib_tgt, file_name)
        # This is _horrid_, but it is fast and mostly works.
        if not os.path.exists(full_tgt):
            shutil.copyfile(f, full_tgt)
            if os.path.isfile(full_tgt):
                run_or_die(["strip", full_tgt])
            print(file_name)

    evm_ds_tgt = os.path.join(workspace, "zilliqa", "evm-ds")
    try:
        os.makedirs(evm_ds_tgt, 0o755)
    except:
        pass
    shutil.copyfile(os.path.join(ZILLIQA_DIR, "evm-ds", "target", "release", "evm-ds"),
                    os.path.join(evm_ds_tgt, "evm-ds"))
    run_or_die(["strip", os.path.join(evm_ds_tgt, "evm-ds")])
    os.chmod(os.path.join(evm_ds_tgt, "evm-ds"), 0o755)
    # This exists because it is a parent of evm_ds_tgt
    shutil.copyfile(os.path.join(ZILLIQA_DIR, "evm-ds", "log4rs.yml"),
                    os.path.join(workspace, "zilliqa", "evm-ds", "log4rs.yml"))

    new_env = os.environ.copy()
    new_env["DOCKER_BUILDKIT"] = "1"
    run_or_die([CONFIG.docker_binary, "build", ".", "-t", tag, "-f", os.path.join(ZILLIQA_DIR, "docker", "Dockerfile.lite")], in_dir = ZILLIQA_DIR, env = new_env,
               capture_output = False)
    if CONFIG.using_podman:
        extra_flags = [ "--tls-verify=false" ]
    else:
        extra_flags = [ ]
    if tag.find('/') != -1:
        print(f"> Pushing to local registry")
        cmd = [ CONFIG.docker_binary, "push", tag ]
        cmd.extend(extra_flags)
        run_or_die(cmd, in_dir = ZILLIQA_DIR, env = new_env, capture_output = False)

    #cmd = [ "kind", "load", "docker-image", tag, "-n", "zqdev" ]
    #run_or_die(cmd, in_dir = ZILLIQA_DIR, env = new_env, capture_output = False)
    print(f"> Built in workspace {workspace}")
    if not CONFIG.keep_workspace:
        print(f"> Removing workspace")
        shutil.rmtree(workspace)

def build(args):
    if len(args) != 1:
        raise GiveUp("Need a single argument - the tag to build")
    tag = refine_tag_name(args[0])
    workspace = os.path.join(ZILLIQA_DIR, "_localdev")
    print(f"> Using workspace {workspace}")
    print(f"> Startup: removing workspace to avoid pollution ...")
    try:
        shutil.rmtree(workspace)
    except:
        pass
    # Let's start off by building Scilla, in case it breaks.
    run_or_die(["make"], in_dir = SCILLA_DIR)
    # OK. That worked. Now copy the relevant bits to our workspace
    try:
        os.makedirs(os.path.join(workspace, "scilla"), 0o755)
    except:
        pass
    shutil.copytree(os.path.join(SCILLA_DIR, "bin"),
                    os.path.join(workspace, "scilla", "bin"), dirs_exist_ok = True)
    shutil.copytree(os.path.join(SCILLA_DIR, "_build", "install", "default", "lib", "scilla", "stdlib"),
                    os.path.join(workspace, "scilla", "stdlib"), dirs_exist_ok = True)
    new_env = os.environ.copy()
    new_env["DOCKER_BUILDKIT"] = "1"
    run_or_die([CONFIG.docker_binary, "build", ".", "-t", tag, "-f", os.path.join(ZILLIQA_DIR, "docker", "Dockerfile.dev")], in_dir = ZILLIQA_DIR, env = new_env,
               capture_output = False)
    if CONFIG.using_podman:
        extra_flags = [ "--tls-verify=false" ]
    else:
        extra_flags = [ ]
    if tag.find('/') != -1:
        cmd = [ CONFIG.docker_binary, "push", tag ]
        cmd.extend(extra_flags)
        run_or_die(cmd, in_dir = ZILLIQA_DIR, env = new_env, capture_output = False)
    print(f"> Built in workspace {workspace}")
    if not CONFIG.keep_workspace:
        print(f"> Removing workspace")
        shutil.rmtree(workspace)

if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) < 1:
        print(__doc__, file=sys.stderr)
        raise GiveUp("Syntax: localdev [cmd] <args..>")
    cmd = args[0]
    CONFIG = Config()
    CONFIG.setup()
    if cmd == "build":
        build(args[1:])
    elif cmd == "build-lite":
        build_lite(args[1:])
    elif cmd == "podman-setup":
        setup_podman()
    elif cmd == "podman-teardown":
        teardown_podman()
    elif cmd == "up":
        run("up")
    elif cmd == "down":
        run("down")
    elif cmd == "reup":
        run("down")
        time.sleep(4.0)
        run("up")
    elif cmd == "setup":
        setup()
    elif cmd == "teardown":
        teardown()
    elif cmd == "log":
        log(args[1:])
    else:
        raise GiveUp(f"Invalid command {cmd}")
