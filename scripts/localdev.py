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
See the docstring for cli() for details.
"""

import os
import sys
import shutil
import re
import tarfile
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
import click
import datetime
import xml.dom.minidom

ZILLIQA_DIR=os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SCILLA_DIR = os.path.join(ZILLIQA_DIR, "..", "scilla")
TESTNET_DIR = os.path.join(ZILLIQA_DIR, "..", "testnet")
KEEP_WORKSPACE = True

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
        self.persistence = None
        self.key_file = None
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

    def set_persistence(self, persistence, key_file):
        """Inherit testnet persistence"""
        self.persistence = persistence
        self.key_file = key_file

class GiveUp(Exception):
    pass

class Pidfile:
    def __init__(self, config, name):
        self._name = name
        self._config = config

    def get_file_name(self):
        return os.path.join(self._config.cache_dir, f"{self._name}.pid")

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

def get_config(ctx):
    """ Get config from the click context """
    return ctx.obj

def run_or_die(config, cmd, in_dir = None, env = None, in_background = False, pid_name = None, capture_output = False, allow_failure = False):
    if env is None:
        env_to_use = os.environ.copy()
        env_to_use.update(config.default_env)
    else:
        env_to_use = config.default_env.copy()
        env_to_use.update(env)

    try:
        if in_background:
            print(">&" + " ".join(cmd))
            proc = subprocess.Popen(cmd, cwd = in_dir, env = env_to_use)
            if pid_name is not None:
                pid_obj = Pidfile(config, pid_name)
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


@click.command("setup-podman")
@click.pass_context
def setup_podman(ctx):
    """
    Set up podman on OS X machines.
    """
    config = get_config(ctx)
    if config.is_osx:
        run_or_die(config, ["podman", "machine", "init" , "--cpus=8", "--memory=16384", "--disk-size=196"])
        # This is necessary because Zilliqa requires various files in /proc/sys/net/core, which aren't exposed in rootless configurations.
        run_or_die(config, ["podman", "machine", "set", "--rootful" ])
        run_or_die(config, ["podman", "machine", "start"])
    else:
        print("No need to setup podman on non-OS X machines")

@click.command("teardown-podman")
@click.pass_context
def teardown_podman(ctx):
    """
    Tear down podman (on OS X only)
    """
    config = get_config(ctx)
    if config.is_osx:
        run_or_die(config, ["podman", "machine", "stop"])
        run_or_die(config, ["podman", "machine", "rm", "-f"])

def get_minikube_ip(config):
    result = sanitise_output(run_or_die(config, ["minikube", "ip"], capture_output = True))
    return result

def gen_tag():
    """
    Generate a short random tag for builds
    """
    gen_from = string.ascii_lowercase + string.digits
    result = "".join([ random.choice(gen_from) for _ in range(0,8) ])
    return result

def print_config_advice(config):
    ip = get_minikube_ip(config)
    host_names = run_or_die(config, ["kubectl", "get", "ingress", "-o", "jsonpath={.items[*].metadata.name}"], capture_output = True)
    host_names = sanitise_output(host_names).split()
    # May have to be hardcoded, since the net isn't running at this point...
    if len(host_names) == 0:
        host_names = [  "localdev-api.localdomain",
                        "localdev-explorer.localdomain",
                        "localdev-l2api.localdomain",
                        "localdev-newapi.localdomain",
                        "localdev-origin.localdomain",
                        "localdev-origin-internal.localdomain" ]
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


def start_k8s(config):
    print("Starting minikube .. ")
    run_or_die(config, ["minikube", "start", "--driver", "kvm2"])

@click.command("start-k8s")
@click.pass_context
def start_k8s_cmd(ctx):
    """
    Restart minikube after a reboot or after you've stopped it for some other reason
    """
    config = get_config(ctx)
    start_k8s(config)
    pull_containers(config)

@click.command("setup-k8s")
@click.pass_context
def setup_k8s(ctx):
    """
    Set up a minikube cluster with appropriate containers and add-ons to run a local development version of Zilliqa
    """
    config = get_config(ctx)
    print("Creating minikube cluster .. ")
    run_or_die(config, ["minikube", "start", "--disk-size", "100g", "--cpus", "max", "--memory", "max", "--driver", "kvm2",
                "--insecure-registry", "192.168.39.0/24", "--container-runtime", "cri-o"])
    run_or_die(config, ["minikube", "addons", "enable", "registry"])
    run_or_die(config, ["minikube", "addons", "enable", "ingress"])
    run_or_die(config, ["minikube", "addons", "enable", "ingress-dns"])
    run_or_die(config, ["kubectl", "config", "use-context", "minikube"])
    wait_for_running_pod(config, "registry", "kube-system")
    wait_for_running_pod(config, "registry-proxy", "kube-system")
    wait_for_local_registry(config)
    pull_containers(config)
    print_config_advice(config)
    print("You can then run localdev up")

def wait_for_local_registry(config):
    """
    Wait for the local registry to be up
    """
    ip = get_minikube_ip(config)
    print(f"Waiting for http://{ip}:5000/v2 .. ")
    while True:
        try:
            run_or_die(config, [ "curl", f"http://{ip}:5000/v2" ] )
            print(".. OK")
            break
        except:
            print("...")
            time.sleep(5)

def wait_for_running_pod(config, podname_prefix, namespace):
    """
    Wait for a pod with a name starting with podman_prefix to be running
    """
    print(f"Waiting for {podname_prefix} in {namespace}. .. ")
    while True:
        pod_data = run_or_die(config, [ "kubectl",
                                        "get",
                                        "pod",
                                        "-n", namespace,
                                        "-o", "jsonpath={ range .items[*] }{.metadata.name}:{.status.phase}{\"\\n\"}{end}" ],
                              capture_output = True )
        pods = sanitise_output(pod_data).split('\n')
        hr = ""
        for p in pods:
            line = p.split(':')
            if (line[0].startswith(podname_prefix) and line[1].lower() == "running"):
                print(f": {line[0]} is running")
                return True
        print(f": {' '.join(pods)}")
        time.sleep(2)


def pull_containers(config):
    # Pre-emptively grab busybox and nginx
    ip = get_minikube_ip(config)
    remote_registry = f"{ip}:5000"
    for container in [ 'docker.io/localstack/localstack:latest',
                       'docker.io/library/nginx:latest',
                       'docker.io/library/busybox:latest',
                       'docker.io/zilliqa/devex:a532d82' ]:
        pull_container(config, container)
        if container.startswith('docker.io/library'):
            local_tag = container.split('/')[-1]
        elif container.startswith('docker.io/'):
            local_tag = '/'.join(container.split('/')[1:])
        local_tag = f"{remote_registry}/{local_tag}"
        print(f"Retagging {container} as {local_tag} .. ")
        run_or_die(config, [config.docker_binary, "tag", container, local_tag])
        push_to_local_registry(config, local_tag)



def pull_container(config, container):
    run_or_die(config, [ config.docker_binary, "pull" , container])

def push_to_local_registry(config, tag):
    if config.using_podman:
        extra_flags = [ "--tls-verify=false" ]
    else:
        extra_flags = [ ]
    if tag.find('/') != -1:
        print(f"> Pushing to local registry")
        cmd = [ config.docker_binary, "push", tag ]
        cmd.extend(extra_flags)
        run_or_die(config, cmd, in_dir = ZILLIQA_DIR, capture_output = False)

@click.command("teardown-k8s")
@click.pass_context
def teardown_k8s(ctx):
    """ Tear down k8s cluster; when you restart it you will need to rewrite your host lookups """
    config = get_config(ctx)
    print("Destroying minikube cluster .. ")
    run_or_die(config, ["minikube", "delete"])

@click.command("up")
@click.pass_context
@click.option("--persistence", help="A persistence directory to start the network with. Has no effect without also passing `--key-file`.")
@click.option("--key-file", help="A `.tar.gz` generated by `./testnet.sh back-up auto` containing the keys used to start this network. Has no effect without also passing `--persistence`.")
def up_cmd(ctx, persistence, key_file):
    """
    Build Zilliqa (via a process equivalent to the build_lite command), write configuration files for a
    testnet named localdev, run `localdev/config.sh up`, and start a proxy to allow the user to monitor traffic
    on the API ports.
    """
    config = get_config(ctx)
    config.set_persistence(persistence,key_file)
    up(config)

def up(config):
    minikube = get_minikube_ip(config)
    tag = gen_tag()
    #tag = "w5fnelo8"
    tag_name = f"{minikube}:5000/zilliqa:{tag}"
    build_lite(config, tag_name)
    write_testnet_configuration(config, tag_name, config.testnet_name)
    start_testnet(config, config.testnet_name)
    start_proxy(config, config.testnet_name)
    show_proxy(config, config.testnet_name)
    restart_ingress(config);
    print("Ingress restarted; you should be ready to go...");

@click.command("isolated")
@click.pass_context
def isolated_cmd(ctx):
    """
    Run an isolated server
    """
    config = get_config(ctx)
    isolated(config)

def xml_get_element(doc, parent, name):
    elems = parent.getElementsByTagName(name)
    if elems is None or len(elems) < 1:
        raise GiveUp(f"XML: Cannot find element {name} in {parent}")
    elif len(elems) > 1:
        raise GiveUp(f"XML: More than one child named {name} in {parent}")
    return elems[0]

def xml_replace_element(doc, parent, name, new_value):
    elem = xml_get_element(doc, parent, name)
    # Remove all the nodes
    while elem.firstChild is not None:
        elem.removeChild(elem.firstChild)
    elem.appendChild(doc.createTextNode(new_value))

def copy_everything_in_dir(src, dest):
    """ Copy all files in src to dest """
    the_files = os.listdir(src)
    for f in the_files:
        full_src = os.path.join(src, f)
        if os.path.isfile(full_src):
            shutil.copy(full_src, os.path.join(dest, f))

def isolated(config):
    build_native_to_workspace(config)
    #workspace = os.path.join(ZILLIQA_DIR, "_localdev", "isolated")
    config_file = xml.dom.minidom.parse(os.path.join(ZILLIQA_DIR, "constants.xml"))
    xml_replace_element(config_file, config_file.documentElement, "LOOKUP_NODE_MODE", "true")
    xml_replace_element(config_file, config_file.documentElement, "ENABLE_SC", "true")
    xml_replace_element(config_file, config_file.documentElement, "ENABLE_SCILLA_MULTI_VERSION", "false")
    xml_replace_element(config_file, config_file.documentElement, "SCILLA_ROOT", "scilla")
    xml_replace_element(config_file, config_file.documentElement, "SCILLA_LIB", "stdlib")
    xml_replace_element(config_file, config_file.documentElement, "ENABLE_SCILLA_MULTI_VERSION", "false")
    xml_replace_element(config_file, config_file.documentElement, "ENABLE_EVM", "true")
    xml_replace_element(config_file, config_file.documentElement, "EVM_SERVER_BINARY", "evm-ds/evm-ds")
    xml_replace_element(config_file, config_file.documentElement, "EVM_LOG_CONFIG", "evm-ds/log4rs.yml")
    xml_replace_element(config_file, config_file.documentElement, "EVM_SERVER_SOCKET_PATH", "/tmp/evm-server.sock")
    # Now assemble an isolated server release.
    target_workspace = os.path.join(ZILLIQA_DIR, "_localdev", "isolated")
    src_workspace = os.path.join(ZILLIQA_DIR, "_localdev")
    subdirs = [ "lib", "scilla", "bin" ]
    for s in subdirs:
        try:
            os.makedirs(os.path.join(target_workspace,s), 0o755)
        except:
            pass
    # Copy scilla recursively
    shutil.copytree(os.path.join(src_workspace, "scilla"), os.path.join(target_workspace, "scilla"), dirs_exist_ok = True)
    # Now, on OS X we need to patch rpath for the scilla executables ..
    if config.is_osx:
        print("> On OS X, patching rpath for Scilla .. ")
        tgt_bin = os.path.join(target_workspace, "scilla", "bin")
        tgt_lib = os.path.join(target_workspace,  "lib")
        bins = os.listdir(tgt_bin)
        for binary in bins:
            full_path = os.path.join(tgt_bin, binary)
            if os.path.isfile(full_path):
                print(f".. patching rpath for {binary}")
                run_or_die(config, ["chmod", "u+w",  full_path])
                run_or_die(config, ["install_name_tool", "-add_rpath", tgt_lib, full_path])

    shutil.copytree(os.path.join(src_workspace, "zilliqa", "evm-ds"), os.path.join(target_workspace, "evm-ds"), dirs_exist_ok = True)
    # Copy zilliqa in...
    copy_everything_in_dir(os.path.join(src_workspace, "zilliqa", "lib"), os.path.join(target_workspace, "lib"))
    copy_everything_in_dir(os.path.join(src_workspace, "zilliqa", "build", "bin"),
                           os.path.join(target_workspace, "bin"))
    copy_everything_in_dir(os.path.join(src_workspace, "zilliqa", "build", "lib"),
                           os.path.join(target_workspace, "lib"))
    shutil.copy(os.path.join(ZILLIQA_DIR, "isolated-server-accounts.json"),
                os.path.join(target_workspace, "isolated-server-accounts.json"))
    output_config = config_file.toprettyxml(newl='')
    with open(os.path.join(target_workspace, 'constants.xml'), 'w') as f:
        f.write(output_config)
    new_env = os.environ.copy()
    #old_path = new_env.get("DYLD_LIBRARY_PATH", "")
    #new_env["DYLD_LIBRARY_PATH"] = f"{target_workspace}/lib:{old_path}"
    cmd = [ "./bin/isolatedServer", "-f", "isolated-server-accounts.json", "-u", "999" ]
    print(f"Running isolated server in {target_workspace} with ${new_env}.. ")
    run_or_die(config, cmd, in_dir = target_workspace, env = new_env)

    # print(f"> Using workspace {workspace}")
    # try:
    #     shutil.rmtree(workspace)
    # except:
    #     pass

def start_testnet(config, testnet_name):
    run_or_die(config, ["./testnet.sh", "up"], in_dir=os.path.join(TESTNET_DIR, testnet_name))
    if config.persistence is not None:
        # Create a tarball of persistence.
        with tarfile.open("_localdev/devnet-persistence.tar.gz", "w:gz") as tar:
            tar.add(config.persistence, arcname="persistence")

        # Wait for localstack to be running
        run_or_die(config, ["kubectl", "rollout", "status", "deployment", "localstack"])
        # Port forward localstack so we can talk to it
        run_or_die(config, ["kubectl", "port-forward", "deployment/localstack", "4566:4566"], in_background=True)

        with open(f"_localdev/.currentTxBlk", "w") as f:
            f.write("123") # FIXME: Set a real value?
        def aws(cmd):
            run_or_die(config, ["aws", "--endpoint-url=http://localhost:4566"] + cmd, env={"PATH": os.environ["PATH"], "AWS_ACCESS_KEY_ID": "test", "AWS_SECRET_ACCESS_KEY": "test"})

        # Copy persistence to S3 in localstack. Each of the subdirectories in S3 are meant to contain only a subset of
        # persistence, but we choose to just copy everything into everywhere.
        aws(["s3", "sync", f"{config.persistence}/", "s3://devnet-bucket/blockchain-data/localdev/"])
        aws(["s3", "sync", f"{config.persistence}/", "s3://devnet-bucket/incremental/localdev/persistence/"])
        aws(["s3", "cp", "_localdev/.currentTxBlk", "s3://devnet-bucket/incremental/localdev/"])
        aws(["s3", "cp", "devnet-persistence.tar.gz", "s3://devnet-bucket/persistence/"])

def wait_for_termination(config):
    """
    Wait for the localdev network to die so we can restart it
    """
    print(f"Waiting for all pods in the main namespace to die .. ")
    while True:
        pod_data = run_or_die(config, [ "kubectl", "get", "pod",
                                        "-o", "jsonpath={ .items[*].metadata.name }" ],
                              capture_output = True)
        podx = sanitise_output(pod_data)
        pods = podx.split(" ")
        # Annoyingly, splitting a zero-length list causes an array of length 1 ..
        if len(podx) == 0 or len(pods) == 0:
            break
        else:
            print(f": {' '.join(pods)}")
            time.sleep(2)

@click.command("down")
@click.pass_context
def down_cmd(ctx):
    """ Bring the testnet down and stop the proxies """
    config = get_config(ctx)
    down(config)

def down(config):
    stop_testnet(config, config.testnet_name)
    stop_proxy(config, config.testnet_name)
    wait_for_termination(config)

def stop_testnet(config, testnet_name):
    # tediously, testnet.sh has a habit of returning non-zero error codes when eg. the testnet has already been destroyed :-(
    run_or_die(config, ["sh", "-c", "echo localdev | ./testnet.sh down"], in_dir=os.path.join(TESTNET_DIR, testnet_name), allow_failure = True)

def build_tag(config, tag_name):
    print(f"Building Zilliqa to container {tag_name} .. ")
    build_lite(config, tag_name)

def write_testnet_configuration(config, tag_name, testnet_name):
    instance_dir = os.path.join(TESTNET_DIR, testnet_name)
    minikube_ip = get_minikube_ip(config)
    if os.path.exists(instance_dir):
        print(f"Removing old testnet configuration ..")
        shutil.rmtree(instance_dir)
    print(f"Generating testnet configuration .. ")
    cmd = ["./bootstrap.py", testnet_name, "--clusters", "minikube", "--constants-from-file",
        os.path.join(ZILLIQA_DIR, "constants_local.xml"),
        "--image", tag_name,
        "--k8s-logs", "true",
        "--local-repo", f"{minikube_ip}:5000",
        "--localdev", "true",
        "--isolated-server-accounts", os.path.join(ZILLIQA_DIR, "isolated-server-accounts.json"),
        "-n", "20",
        "-d", "5",
        "-l", "1",
        "--guard", "4/10",
        "--gentxn", "false",
        "--multiplier-fanout", "2",
        "--host-network", "false",
        "--https", "localdomain",
        "--seed-multiplier", "true",
        "-f",
    ]
    if config.persistence is not None and config.key_file is not None:
        cmd.extend([
            "--bucket", "devnet-bucket",
            "--recover-from-s3", "s3://devnet-bucket/persistence/devnet-persistence.tar.gz",
            "--recover-key-files", config.key_file,
        ])
    run_or_die(config, cmd, in_dir = TESTNET_DIR)

def kill_mitmweb(config, pidfile_name):
    pidfile = Pidfile(config, pidfile_name)
    pid = pidfile.get()
    if pid is not None:
        print(f"> Found pid {pid}")
        try:
            os.kill(pid, 9)
        except Exception as e:
            print(f"> cannot kill - {e}")
            pidfile.zap()
            pass

def sanitise_output(some_output):
    """Strip and decode process output and return something sane"""
    if some_output is None:
        return None
    else:
        try:
            return some_output.decode('utf-8').strip()
        except:
            # Not UTF-8! Let's try latin-1 as a random codec that accepts
            # any code sequence.
            return some_output.decode('latin_1').strip()

def get_mitm_instances(testnet_name):
    return { "explorer" : { "host" : f"{testnet_name}-explorer.localdomain", "port" : 5300 },
             "api" : { "host" : f"{testnet_name}-api.localdomain", "port" : 5301 },
             "l2api" : { "host" : f"{testnet_name}-l2api.localdomain", "port" : 5302 },
             "newapi" : { "host" : f"{testnet_name}-newapi.localdomain", "port" : 5303 },
             "origin" : { "host" : f"{testnet_name}-origin.localdomain", "port" : 5304 } }

def stop_proxy(config, testnet_name):
    mitm_instances = get_mitm_instances(testnet_name)
    for m in mitm_instances.keys():
        kill_mitmweb(config, f"mitmweb_{m}")

def start_proxy(config, testnet_name):
    print(f"> Wait for loadbalancer .. ")
    while True:
        lb_addr = run_or_die(config, ["kubectl",
                              "get",
                              "ingress",
                              "-o",
                              "jsonpath={.items[].status.loadBalancer.ingress[].ip}"],
                             capture_output = True)
        if lb_addr is None or len(lb_addr) == 0:
            print(".")
            sys.stdout.flush()
            time.sleep(2)
        else:
            break
    lb_addr = lb_addr.decode('utf-8')
    print(f"> Load balancer ip {lb_addr}")

    if lb_addr is None or len(lb_addr) is None:
        raise GiveUp("Cannot find IP address of LB: give up");
    print("Killing old mitmweb instances .. ")
    stop_proxy(config, testnet_name)
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
        run_or_die(config, mitm_cmd, in_dir = ZILLIQA_DIR, in_background = True ,pid_name = f"mitmweb_{k}")

def show_proxy(config, testnet_name):
    info = { }
    mitm_instances = get_mitm_instances(testnet_name)
    for (k,v) in mitm_instances.items():
        port = v['port']
        info[k] = { "comm" : f"http://localhost:{port}", "monitor" : f"http://localhost:{port+3000}" }
    print(json.dumps(info))

@click.command("build-lite")
@click.argument("tag")
@click.pass_context
def build_lite_cmd(ctx, tag):
    """
    Builds a Zilliqa container using docker/Dockerfile.lite
    This uses your local compiler and OS, so you need to set that up as described in README.md and have vcpkg available.
    It copies in (but does not build) scilla, so if you are making changes to Scilla you will need to rebuild it yourself.
    Dockerfile.lite contains just enough mechanism to let Zilliqa run.

    TAG is the container tag to build - eg. 'zilliqa:v1'
    """
    config = get_config(ctx)
    build_lite(config, tag)

def build_native_to_workspace(config):
    workspace = os.path.join(ZILLIQA_DIR, "_localdev")
    print(f"> Using workspace {workspace}")
    print(f"> Startup: removing workspace to avoid pollution ...")
    try:
        shutil.rmtree(workspace)
    except:
        pass
    # Let's start off by building Scilla, in case it breaks.
    run_or_die(config, ["make"], in_dir = SCILLA_DIR)
    run_or_die(config, ["./build.sh"], in_dir = ZILLIQA_DIR)
    run_or_die(config, ["cargo", "build", "--release", "--package", "evm-ds"], in_dir =
               os.path.join(ZILLIQA_DIR, "evm-ds"))
    # OK. That worked. Now copy the relevant bits to our workspace
    tgt_bin_dir = os.path.join(workspace, "zilliqa", "build", "bin")
    tgt_lib_dir = os.path.join(workspace, "zilliqa", "build", "lib")
    for the_dir in [ tgt_bin_dir, tgt_lib_dir ]:
        try:
            os.makedirs(the_dir, 0o755)
        except:
            pass

    # Scilla is horrid - we need to copy out the dynlibs.
    triplet_script = os.path.join(SCILLA_DIR, "scripts", "vcpkg_triplet.sh")
    triplet = sanitise_output(run_or_die(config, [ triplet_script ], capture_output = True))
    copy_everything_in_dir( os.path.join(SCILLA_DIR, "vcpkg_installed", triplet, "lib"),
                            tgt_lib_dir )
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
    if config.strip_binaries:
        print("Stripping binaries for a smaller container .. ")
        for strip_dir in [ tgt_bin_dir, tgt_lib_dir ]:
            all_files = glob.glob(strip_dir + r'/*')
            for f in all_files:
                run_or_die(config, ["strip", f])

    lib_tgt = os.path.join(workspace, "zilliqa", "lib")
    try:
        os.makedirs(lib_tgt, 0o755)
    except:
        pass
    src_name = os.path.join(ZILLIQA_DIR, "build", "vcpkg_installed", "x64-linux-dynamic", "lib")
    in_files = glob.glob(src_name + r'/*.so*')
    print("Copying libraries .. ")
    for f in in_files:
        file_name = os.path.split(f)[1]
        full_tgt = os.path.join(lib_tgt, file_name)
        # This is _horrid_, but it is fast and mostly works.
        if not os.path.exists(full_tgt):
            shutil.copyfile(f, full_tgt)
            if config.strip_binaries and os.path.isfile(full_tgt):
                run_or_die(config, ["strip", full_tgt])
            # Avoid printing too much to stdout - rrw 2023-03-28
            #print(file_name)

    evm_ds_tgt = os.path.join(workspace, "zilliqa", "evm-ds")
    try:
        os.makedirs(evm_ds_tgt, 0o755)
    except:
        pass
    shutil.copyfile(os.path.join(ZILLIQA_DIR, "evm-ds", "target", "release", "evm-ds"),
                    os.path.join(evm_ds_tgt, "evm-ds"))
    if config.strip_binaries:
        run_or_die(config, ["strip", os.path.join(evm_ds_tgt, "evm-ds")])
    os.chmod(os.path.join(evm_ds_tgt, "evm-ds"), 0o755)
    # This exists because it is a parent of evm_ds_tgt
    shutil.copyfile(os.path.join(ZILLIQA_DIR, "evm-ds", "log4rs.yml"),
                    os.path.join(workspace, "zilliqa", "evm-ds", "log4rs.yml"))

def get_pod_names(config, node_type = None):
    cmd =  ["kubectl",
            "get",
            "pod",
            "-o", "jsonpath={.items[*].metadata.name}" ]
    if node_type is not None:
        cmd.append(f"-l type={node_type}")
    pod_op = run_or_die(config,cmd, capture_output = True)
    pod_names = sanitise_output(pod_op).split()
    return pod_names

def get_rfc3339_recency(config,recency):
    recency_num = int(recency)
    from_date = datetime.datetime.now() - datetime.timedelta(seconds=recency_num)
    utc_time = from_date.astimezone(datetime.timezone.utc)
    rfc_time = utc_time.strftime('%Y-%m-%dT%H:%M:%SZ')
    return rfc_time

def log_snapshot(config, recency):
    pod_names = get_pod_names(config)
    log_name = "/tmp/" + "".join([random.choice(string.digits) for _ in range(0,10) ])
    os.makedirs(log_name, 0o755)
    rfc_time = get_rfc3339_recency(config, recency)
    # Get a lookup table in case we want it (just because I wrote the command line .. )
    lookup_file = os.path.join(log_name, "_ips.json")
    ip_table = run_or_die(config, ["kubectl", "get", "pod","-o",
                                   'jsonpath={ range .items[*]}{@.metadata.name}{" "}{@.status.podIP}{"\\n"}{end}'],
                          capture_output = True)
    ip_table = sanitise_output(ip_table)
    ips = { }
    for line in ip_table.split('\n'):
        (name,value) = line.split(' ')
        ips[name] = value
    with open(lookup_file, 'w') as f:
        f.write(json.dumps(ips, indent=2))

    for n in pod_names:
        print(f"..{n}")
        logs = run_or_die(config, ["kubectl", "logs", f"--since-time={rfc_time}", n],
                          capture_output = True)
        logs = sanitise_output(logs)
        ip = ips.get(n, "unknown")
        output_file = os.path.join(log_name, f"{n}_{ip}")
        with open(output_file, 'w') as f:
            f.write(logs)
    print(f"Logs in {log_name}")

def build_lite(config, tag):
    workspace = build_native_to_workspace(config)
    new_env = os.environ.copy()
    new_env["DOCKER_BUILDKIT"] = "1"
    run_or_die(config, [config.docker_binary, "build", ".", "-t", tag, "-f", os.path.join(ZILLIQA_DIR, "docker", "Dockerfile.lite")], in_dir = ZILLIQA_DIR, env = new_env,
               capture_output = False)
    push_to_local_registry(config, tag)
    print(f"> Built in workspace {workspace}")
    if not config.keep_workspace:
        print(f"> Removing workspace")
        shutil.rmtree(workspace)


def get_pod_names(config, node_type = None):
    cmd =  ["kubectl",
            "get",
            "pod",
            "-o", "jsonpath={.items[*].metadata.name}" ]
    if node_type is not None:
        cmd.append(f"-l type={node_type}")
    pod_op = run_or_die(config,cmd, capture_output = True)
    pod_names = sanitise_output(pod_op).split()
    return pod_names

def get_rfc3339_recency(config,recency):
    recency_num = int(recency)
    from_date = datetime.datetime.now() - datetime.timedelta(seconds=recency_num)
    utc_time = from_date.astimezone(datetime.timezone.utc)
    rfc_time = utc_time.strftime('%Y-%m-%dT%H:%M:%SZ')
    return rfc_time

def which_pod_said(config, node_type, recency, what):
    pod_names = get_pod_names(config, node_type)
    expression = re.compile(what)
    rfc_time = get_rfc3339_recency(config, recency)
    pods_said = [ ]
    print("Pods: {' '.join(pod_names)}")
    for n in pod_names:
        print(f".. {n}")
        logs = run_or_die(config, ["kubectl","logs", f"--since-time={rfc_time}", n ],
                          capture_output = True)
        logs = sanitise_output(logs)
        if expression.search(logs):
            pods_said.append(n)

    print("----")
    print(" ".join(pods_said))

def restart_ingress(config):
    print("Restarting ingress .. ")
    run_or_die(config, ["kubectl", "rollout", "restart", "deployment",
                        "ingress-nginx-controller", "-n", "ingress-nginx" ])

@click.command("restart-ingress")
@click.pass_context
def restart_ingress_cmd(ctx):
    """Restart the k8s ingress as it sometimes sticks connections"""
    config = get_config(ctx)
    restart_ingress(config)

@click.command("reup")
@click.pass_context
@click.option("--persistence", help="A persistence directory to start the network with. Has no effect without also passing `--key-file`.")
@click.option("--key-file", help="A `.tar.gz` generated by `./testnet.sh back-up auto` containing the keys used to start this network. Has no effect without also passing `--persistence`.")
def reup_cmd(ctx, persistence, key_file):
    """
    Equivalent to `localdev down && localdev up`
    """
    config = get_config(ctx)
    config.set_persistence(persistence, key_file)
    down(config)
    up(config)

@click.command("pull-containers")
@click.pass_context
def pull_containers_cmd(ctx):
    """
    Pull external containers and push them to the k8s registry for loading
    (up may not work if you don't do this)
    This command is executed implicitly by setup-k8s
    """
    config = get_config(ctx)
    pull_containers(config)

@click.command("show-proxy")
@click.pass_context
def show_proxy_cmd(ctx):
    """
    Show proxy settings
    """
    config = get_config(ctx)
    show_proxy(config, config.testnet_name)

@click.command("log-snapshot")
@click.pass_context
@click.argument("recency")
def log_snapshot_cmd(ctx, recency):
    """
    Grab logs for the last RECENCY seconds for all pods and save them in /tmp
    """
    config = get_config(ctx)
    log_snapshot(config, recency)

@click.command("which-pod-said")
@click.pass_context
@click.argument("nodetype")
@click.argument("recency")
@click.argument("term")
def which_pod_said_cmd(ctx, nodetype, recency, term):
    """
    Find out which pods said something

    NODETYPE is the type of pod (normal, dsguard, .. )
    RECENCY is the number of seconds back to check.
    TERM is the term to check for
    """
    config = get_config(ctx)
    which_pod_said(config, nodetype, recency, term)

@click.command("start-proxy")
@click.pass_context
def start_proxy_cmd(ctx):
    """
    Start the mitm proxies
    """
    config = get_config(ctx)
    start_proxy(config, config.testnet_name)
    show_proxy(config, config.testnet_name)

@click.command("write-testnet-config")
@click.pass_context
@click.argument("tag")
def write_testnet_config_cmd(ctx, tag):
    """
    Write config for the testnet

    TAG is the tag for the Zilliqa docker image to run.
    """
    config = get_config(ctx)
    write_testnet_configuration(config, tag, config.testnet_name)

@click.command("print-config-advice")
@click.pass_context
def print_config_advice_cmd(ctx):
    config = get_config(ctx)
    print_config_advice(config)

@click.command("wait-for-local-registry")
@click.pass_context
def wait_for_local_registry_cmd(ctx):
    """
    Wait for the local container registry to be running
    """
    config = get_config(ctx)
    wait_for_local_registry(config)
    
@click.command("wait-for-running-pod")
@click.pass_context
@click.argument("prefix")
@click.argument("namespace")
def wait_for_running_pod_cmd(ctx, prefix, namespace):
    """
    Wait for a running pod.

    PREFIX is the prefix of the pod name to wait for.
    NAMESPACE is the namespace to look for it in.
    """
    config = get_config(ctx)
    wait_for_running_pod(config,prefix,namespace)

@click.command("wait-for-termination")
@click.pass_context
def wait_for_termination_cmd(ctx):
    """
    Wait for running pods to terminate so we can start a new network
    """
    config = get_config(ctx)
    wait_for_termination(config)

@click.command("rfc3339")
@click.argument("recency")
@click.pass_context
def rfc3339_cmd(ctx, recency):
    """ Print an RFC3339 date RECENCY seconds ago """
    config = get_config(ctx)
    print(get_rfc3339_recency(config, recency))

@click.group()
@click.pass_context
def debug(ctx):
    """
    Debugging commands - used to test parts of localdev in isolation
    """
    pass

debug.add_command(start_proxy_cmd)
debug.add_command(write_testnet_config_cmd)
debug.add_command(print_config_advice_cmd)
debug.add_command(pull_containers_cmd)
debug.add_command(wait_for_running_pod_cmd)
debug.add_command(wait_for_termination_cmd)
debug.add_command(wait_for_local_registry_cmd)
debug.add_command(rfc3339_cmd)

@click.group()
@click.pass_context
def cli(ctx):
    """localdev.py sets up local development environments for Zilliqa.

    It is for internal use only and requires the use of the `testnet`
    private repository. We'll hope to relax this restriction when we can.

    You will need a directory structure that looks like this:

    scilla/        - from git@github.com:zilliqa/scilla
    zilliqa/       - from git@github.com:zilliqa/zilliqa
    testnet/       - from git@github.com:zilliqa/testnet

    You may need the ZIL_5135_localdev branch of testnet if it hasn't yet
    been merged.

    You will need to have built scilla.

    localdev.py runs in stages:
     setup-podman    - on OS X only, sets up podman
     setup-k8s       - Sets up k8s (currently via minikube)
     up              - Brings the system up.
     teardown-k8s    - Bring down k8s
     teardown-podman - On OS X only, tears down podman

    If you reboot your machine, `localdev.py start-k8s` will restart
    minikube for you.

    There are also commands to collect logs, and one to restart the
    ingress, since it sometimes sticks.

    WARNING: Only tested so far on Ubuntu 22.04 . OS X MAY NOT WORK.

    """
    ctx.obj = Config()
    ctx.obj.setup()

cli.add_command(build_lite_cmd)
cli.add_command(setup_podman)
cli.add_command(teardown_podman)
cli.add_command(setup_k8s)
cli.add_command(start_k8s_cmd)
cli.add_command(teardown_k8s)
cli.add_command(up_cmd)
cli.add_command(down_cmd)
cli.add_command(show_proxy_cmd)
cli.add_command(which_pod_said_cmd)
cli.add_command(debug)
cli.add_command(pull_containers_cmd)
cli.add_command(reup_cmd)
cli.add_command(log_snapshot_cmd)
cli.add_command(restart_ingress_cmd)
cli.add_command(isolated_cmd)

if __name__ == "__main__":
    cli()
