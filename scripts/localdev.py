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
Docs!
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
import random
import time
import string

ZILLIQA_DIR=os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SCILLA_DIR = os.path.join(ZILLIQA_DIR, "..", "scilla")
TESTNET_DIR = os.path.join(ZILLIQA_DIR, "..", "testnet")
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
            self.using_podman = True
            self.using_local_registry = True
            self.docker_binary = "podman"
            self.is_osx = False
        self.keep_workspace = True # "KEEP_WORKSPACE" in os.environ
        self.testnet_name = "localdev"
        self.strip_binaries = False

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


def get_minikube_ip():
    result = sanitise_output(run_or_die(["minikube", "ip"], capture_output = True))
    return result

def gen_tag():
    """
    Generate a short random tag for builds
    """
    gen_from = string.ascii_lowercase + string.digits
    result = "".join([ random.choice(gen_from) for _ in range(0,8) ])
    return result

def print_config_advice():
    ip = get_minikube_ip()
    host_names = run_or_die(["kubectl", "get", "ingress", "-o", "jsonpath={.items[*].metadata.name}"], capture_output = True)
    host_names = sanitise_output(host_names).split()
    hosts = "\n".join([ f"{ip} {host}.localdomain" for host in host_names ])
    print("Minikube is at {ip}")
    print(f"""Please add

[Resolver]
DNS={ip}
Domains=~localdomain

    to your /etc/systemd/resolved.conf
    And run

systemctl restart systemd-resolved

    Or add

{hosts}

    to your /etc/hosts.
""")
          
def setup():
    print("Creating minikube cluster .. ")
    run_or_die(["minikube", "start", "--cpus", "max", "--memory", "max", "--driver", "kvm2",
                "--insecure-registry", "192.168.39.0/24", "--container-runtime", "cri-o"])
    run_or_die(["minikube", "addons", "enable", "registry"])
    run_or_die(["minikube", "addons", "enable", "ingress"])
    run_or_die(["minikube", "addons", "enable", "ingress-dns"])
    run_or_die(["kubectl", "config", "use-context", "minikube"])
    # Pre-emptively grab busybox and nginx
    for container in [ 'nginx', 'busybox' ]:
        pull_container(container)
        push_to_local_registry(container)
    print_config_advice()
    print("You can then run localdev up")



def pull_container(container):
    run_or_die([ CONFIG.docker_binary, "pull" , container])

def push_to_local_registry(tag):
    if CONFIG.using_podman:
        extra_flags = [ "--tls-verify=false" ]
    else:
        extra_flags = [ ]
    if tag.find('/') != -1:
        print(f"> Pushing to local registry")
        cmd = [ CONFIG.docker_binary, "push", tag ]
        cmd.extend(extra_flags)
        run_or_die(cmd, in_dir = ZILLIQA_DIR, capture_output = False)

def teardown():
    print("Destroying minikube cluster .. ")
    run_or_die(["minikube", "delete"])

def go_up():
    minikube = get_minikube_ip()
    tag = gen_tag()
    #tag = "w5fnelo8"
    tag_name = f"{minikube}:5000/zilliqa:{tag}"
    build_tag(tag_name)
    write_testnet_configuration(tag_name, CONFIG.testnet_name)
    start_testnet(CONFIG.testnet_name)
    start_proxy(CONFIG.testnet_name)
    show_proxy(CONFIG.testnet_name)

def start_testnet(testnet_name):
    run_or_die(["./testnet.sh", "up"], in_dir=os.path.join(TESTNET_DIR, testnet_name))

def go_down():
    stop_testnet(CONFIG.testnet_name)
    stop_proxy(CONFIG.testnet_name)

def stop_testnet(testnet_name):
    run_or_die(["sh", "-c", "echo localdev | ./testnet.sh down"], in_dir=os.path.join(TESTNET_DIR, testnet_name))

def build_tag(tag_name):
    print(f"Building Zilliqa to container {tag_name} .. ")
    build_lite(tag_name)

def write_testnet_configuration(tag_name, testnet_name):

    instance_dir = os.path.join(TESTNET_DIR, testnet_name)
    if os.path.exists(instance_dir):
        print(f"Removing old testnet configuration ..")
        shutil.rmtree(instance_dir)
    print(f"Generating testnet configuration .. ")
    run_or_die(["./bootstrap.py", testnet_name, "--clusters", "minikube", "--constants-from-file",
                os.path.join(ZILLIQA_DIR, "constants_local.xml"),
                "--image", tag_name,
                "--localdev", "true",
                "-n", "20",
                "-d", "5",
                "-l", "1",
                "--guard", "4/10",
                "--gentxn", "false",
                "--multiplier-fanout", "2",
                "--host-network", "false",
                "--https", "localdomain",
                "--seed-multiplier", "true",
                "-f", "--bucket", "none"],
               in_dir = TESTNET_DIR)

def kill_mitmweb(pidfile_name):
    if CONFIG.is_osx:
        print("> killall doesn't work on OS X .. using pidfile")
        pidfile = Pidfile(pidfile_name)
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

def get_mitm_instances(testnet_name):
    return { "explorer" : { "host" : f"{testnet_name}-explorer.localdomain", "port" : 5300 },
             "api" : { "host" : f"{testnet_name}-api.localdomain", "port" : 5301 },
             "l2api" : { "host" : f"{testnet_name}-l2api.localdomain", "port" : 5302 },
             "newapi" : { "host" : f"{testnet_name}-newapi.localdomain", "port" : 5303 },
             "origin" : { "host" : f"{testnet_name}-origin.localdomain", "port" : 5304 } }

def stop_proxy(testnet_name):
    mitm_instances = get_mitm_instances(testnet_name)
    for m in mitm_instances.keys():
        kill_mitmweb(f"mitmweb_{m}")

def start_proxy(testnet_name):
    print(f"> Wait for loadbalancer .. ")
    while True:
        lb_addr = run_or_die(["kubectl",
                              "get",
                              "ingress",
                              "-o",
                              "jsonpath={.items[].status.loadBalancer.ingress[].ip}"],
                             capture_output = True)
        if lb_addr is None or len(lb_addr) == 0:
            print(".")
            std.stdout.flush()
            time.sleep(2)
        else:
            break
    lb_addr = lb_addr.decode('utf-8')
    print(f"> Load balancer ip {lb_addr}")

    if lb_addr is None or len(lb_addr) is None:
        raise GiveUp("Cannot find IP address of LB: give up");
    print("Killing old mitmweb instances .. ")
    stop_proxy(testnet_name)
    mitm_instances = get_mitm_instances(testnet_name)
    print("Starting new mitmweb instances .. ")
    for (k,v) in mitm_instances.items():
        port = v['port']
        print(f"Starting {k} on port {port}, webserver {port+3000} .. ")
        mitm_cmd = ["mitmweb",
                    "--mode",
                    f"reverse:http://{lb_addr}:80",
                    "--modify-headers",
                    f"/~q/Host/{v['host']}",
                    "--no-web-open-browser",
                    "--listen-port", str(port),
                    "--web-port", str(port+3000)
                    ]
        run_or_die(mitm_cmd, in_dir = ZILLIQA_DIR, in_background = True ,pid_name = f"mitmweb_{k}")

def show_proxy(testnet_name):
    info = []
    for (k,v) in mitm_instances.items():
        port = v['port']
        info[k] = { "comm" : f"http://localhost:{port}", "monitor" : f"http://localhost:{port+3000}" }
    
    print(json.dumps(info))

def build_lite(tag):
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
    if CONFIG.strip_binaries:
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
    push_to_local_registry(tag)
    #cmd = [ "kind", "load", "docker-image", tag, "-n", "zqdev" ]
    #run_or_die(cmd, in_dir = ZILLIQA_DIR, env = new_env, capture_output = False)
    print(f"> Built in workspace {workspace}")
    if not CONFIG.keep_workspace:
        print(f"> Removing workspace")
        shutil.rmtree(workspace)


def which_pod_said(node_type, recency, what):
    pod_op = run_or_die(["kubectl",
                            "get",
                            "pod",
                            f"-l type={node_type}",
                            "-o", "jsonpath={.items[*].metadata.name}" ], capture_output = True)
    pod_names = sanitise_output(pod_op).split()
    expression = re.compile(what)
    pods_said = [ ]
    print("Pods: {' '.join(pod_names)}")
    for n in pod_names:
        print(f".. {n}")
        logs = run_or_die(["kubectl","logs", f"--tail={recency}", n ],
                          capture_output = True)
        logs = sanitise_output(logs)
        if expression.search(logs):
            pods_said.append(n)

    print("----")
    print(" ".join(pods_said))
    
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
        build_lite(args[1])
    elif cmd == "podman-setup":
        setup_podman()
    elif cmd == "podman-teardown":
        teardown_podman()
    elif cmd == "start-proxy":
        start_proxy(CONFIG.testnet_name)
        show_proxy(CONFIG.testnet_name)
    elif cmd == "show-proxy":
        display_proxy()
    elif cmd == "up":
        go_up()
    elif cmd == "down":
        go_down()
    elif cmd == "write_testnet_config":
        write_testnet_configuration(args[1], CONFIG.testnet_name)
    elif cmd == "reup":
        run("down")
        time.sleep(4.0)
        run("up")
    elif cmd == "setup":
        setup()
    elif cmd == "teardown":
        teardown()
    elif cmd == "which-pod-said":
        which_pod_said(args[1], args[2], args[3])
    elif cmd == "print-config-advice":
        print_config_advice()
    else:
        raise GiveUp(f"Invalid command {cmd}")
