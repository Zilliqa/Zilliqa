#!/usr/bin/env python3
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

"""
See the docstring for cli() for details.
"""

import click
import datetime
import glob
import ipaddress
import json
import os
import pathlib
import random
import re
import shutil
import string
import subprocess
import sys
import tarfile
import tempfile
import time
import xml.dom.minidom

ZILLIQA_DIR=os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SCILLA_DIR = os.path.join(ZILLIQA_DIR, "..", "scilla")
TESTNET_DIR = os.path.join(ZILLIQA_DIR, "..", "testnet")
KEEP_WORKSPACE = True

def get_progress_arg():
    if "NO_COLOR" in os.environ:
        return "--progress=plain"
    else:
        return "--progress=auto"

def is_osx():
    return sys.platform == "darwin"

def default_driver():
    return "docker"

def using_podman(config):
    return config.driver == "podman"

class Config:
    def __init__(self):
        self.default_env = { "LOCALDEV" : "1" , "FAST_BUILD" : "1"}
        self.cache_dir = os.path.join(pathlib.Path.home(), ".cache", "zilliqa_localdev");
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

def helm_remove_repository(config, repo):
    run_or_die(config, ["helm", "uninstall", repo], allow_failure = True)


def setup_podman(ctx, cpus, memory, disk_size):
    """
    Set up podman.
    """
    config = get_config(ctx)
    run_or_die(config, ["podman", "machine", "init" , "--cpus={}".format(cpus), "--memory={}".format(memory), "--disk-size={}".format(disk_size)])
    # This is necessary because Zilliqa requires various files in /proc/sys/net/core, which aren't exposed in rootless configurations.
    run_or_die(config, ["podman", "machine", "set", "--rootful" ])
    run_or_die(config, ["podman", "machine", "start"])

def setup_colima(ctx, cpus, memory, disk_size):
    """
    Sets up colima (on OS X only).
    """
    config = get_config(ctx)
    run_or_die(config, ["colima", "start", f"--cpu={cpus}", f"--memory={int(memory / 1024)}", f"--disk={disk_size}", "--runtime=docker"])

@click.command("teardown-podman")
@click.pass_context
def teardown_podman(ctx):
    """
    Tear down podman.
    """
    config = get_config(ctx)
    if is_osx():
        run_or_die(config, ["podman", "machine", "stop"])
        run_or_die(config, ["podman", "machine", "rm", "-f"])

def get_minikube_ip(config):
    # On OS X it's not possible to use the real minikube IP, and instead 127.0.0.1 must
    # be used as well as running 'minikube tunnel'.
    result = "127.0.0.1" if is_osx() else sanitise_output(run_or_die(config, ["minikube", "ip"], capture_output = True))
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
        host_names = [  "localdev-api",
                        "localdev-explorer",
                        "localdev-l2api",
                        "localdev-newapi",
                        "localdev-origin",
                        "localdev-origin-internal" ]
    hosts = "\n".join([ f"{ip} {host}.localdomain" for host in host_names ])
    print(f"Minikube is at {ip}")
    print(f"""Now run:
cat << EOF | sudo tee -a /etc/hosts
{hosts}
EOF
""");
    if is_osx():
        print("""And run:
sudo minikube tunnel

In a separate terminal
        """)


def minikube_env(config, driver):
    driver_env = os.environ.copy()
    if driver == "docker" or driver == "podman" or driver == "kvm2":
        for p in map(
            # Skip the 'export ' and split at '=' into a tuple
            lambda x: x[7:].split('='),
            re.findall(
                r'export [A-Z_]+="[^"]*"',
                run_or_die(config, ["minikube", ("podman" if driver == "podman" else "docker") + "-env"], capture_output=True).decode('utf-8'))):
            driver_env[p[0]] = p[1][1:-1]

    return driver_env

def adjust_config(config, driver):
    config.driver = driver

    if driver == "podman":
        config.docker_binary = "podman"
    else:
        config.docker_binary = "docker"

    config.driver_env = minikube_env(config, driver)

def setup_k8s(ctx, cpus, memory, disk_size, driver, container_runtime):
    """
    Set up a minikube cluster with appropriate containers and add-ons to run a local development version of Zilliqa
    """

    config = get_config(ctx)
    print("Creating minikube cluster .. ")
    run_or_die(config, ["minikube", "start", "--disk-size", "{}g".format(disk_size), "--cpus", str(cpus), "--memory", str(memory), "--driver", driver,
                        "--container-runtime", container_runtime])
    run_or_die(config, ["minikube", "addons", "enable", "ingress"])
    run_or_die(config, ["minikube", "addons", "enable", "ingress-dns"])
    run_or_die(config, ["kubectl", "config", "use-context", "minikube"])

    adjust_config(config, driver)

    print_config_advice(config)
    print("You can then run localdev up")


@click.command("setup")
@click.option("--driver",
              required=True,
              default=default_driver(),
              show_default=True,
              help="The minikube driver to use")
@click.option("--cpus",
              callback=lambda ctx, param, value: value if value else "max" if ctx.params["driver"] == "docker" and sys.platform != "darwin" else 8,
              help="The number of CPUs in the guest VM")
@click.option("--memory",
              default=12288,
              show_default=True,
              help="The amount of memory allocated to the guest VM (in MB)")
@click.option("--disk-size",
              default=128,
              show_default=True,
              help="The disk size (in GB) in the guest VM")
@click.option("--container-runtime",
              callback=lambda ctx, param, value: value if value else "cri-o" if ctx.params["driver"] == "podman" else "docker",
              help="The minikube container runtime to use")
@click.pass_context
def setup(ctx, driver, cpus, memory, disk_size, container_runtime):
    """
    Sets up minikube & the virtualization environment
    """

    adjust_minikube_specs = False
    if driver == "podman":
        setup_podman(ctx, cpus, memory, disk_size)
        adjust_minikube_specs = True
    elif driver == "docker" and sys.platform == "darwin":
        adjust_minikube_specs = True
        setup_colima(ctx, cpus, memory, disk_size)

    memory = int(memory)
    disk_size = int(disk_size)

    # If the driver is podman or docker on OS X, minikube will be created inside the
    # podman/colima VM so we need to reduce the memory & disk size it's allocated.
    if adjust_minikube_specs:
        try:
            memory = int(memory * 0.8)
        except:
            pass
        disk_size = int(disk_size * 0.75)

    setup_k8s(ctx, cpus, memory, disk_size, driver, container_runtime)


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

def pull_container(config, container):
    run_or_die(config, [ config.docker_binary, "pull" , container], env=config.driver_env)

def wait_for_helm_pod(config, pod_partial_name):
    while True:
        pods = subprocess.Popen([ "kubectl", "get", "pod", "-o", "json" ], env=config.driver_env, stdout=subprocess.PIPE)
        pod_name = sanitise_output(
            subprocess.check_output([ "jq", "-r", f".items[] | select(.metadata.name | test(\"{pod_partial_name}\")) | select(.status.phase == \"Running\").metadata.name" ], env=config.driver_env, stdin=pods.stdout)).strip(' ')
        pods.wait()

        if len(pod_name) == 0:
            print(f"Waiting for pod to be ready...")
            time.sleep(2)
        else:
            break

    return pod_name

def localstack_up(config):
    """ Let helm deploy localstack """
    run_or_die(config, ["helm", "upgrade", "--install", "localstack", "localstack/localstack"])
    localstack_pod_name = wait_for_helm_pod(config, "localstack-")

    # Port forward localstack so we can talk to it
    run_or_die(config, ["kubectl", "port-forward", "deployment/localstack", "4566:4566"], in_background=True)

    bucket_name = 'zilliqa-devnet'
    run_or_die(config, ['kubectl', 'exec', '-it', localstack_pod_name, '--', 'awslocal', 's3', 'mb', f's3://{bucket_name}'])
    run_or_die(config, ['kubectl', 'exec', '-it', localstack_pod_name, '--', 'awslocal', 's3', 'mb', f's3://tempo'])

def localstack_down(config):
    """ Let helm undeploy localstack """
    helm_remove_repository(config, 'localstack')

def grafana_up(config, testnet_name):
    """ Let helm deploy grafana """

    conf = f"""
datasources:
  datasources.yaml:
    apiVersion: 1
    datasources:
    - name: Prometheus
      type: prometheus
      url: http://prometheus-server.default.svc.cluster.local
      access: proxy
      isDefault: true
    - name: Tempo
      type: tempo
      url: http://tempo.default.svc.cluster.local:3100
      access: proxy
      isDefault: false
ingress:
  enabled: true
  hosts:
    - "{testnet_name}-grafana.localdomain"
persistence:
  enabled: true
  storageClassName: "standard"
  size: 1Gi
adminUser: admin
adminPassword: admin
            """

    with tempfile.NamedTemporaryFile() as tmpfile:
        tmpfile.write(conf.encode('utf-8'))
        tmpfile.flush()
        run_or_die(config, ["helm", "upgrade", "--install", "grafana", "grafana/grafana", "-f", tmpfile.name])
        wait_for_helm_pod(config, "grafana-")

def grafana_down(config):
    """ Let helm undeploy grafana """
    helm_remove_repository(config, 'grafana')

def prometheus_up(config, testnet_name, count = 8):
    """ Let helm deploy prometheus """
    ips = []
    while True:
        pods = subprocess.Popen([ "kubectl", "get", "pod", "-o", "json" ], env=config.driver_env, stdout=subprocess.PIPE)
        output = sanitise_output(
            subprocess.check_output([ "jq", "-r", f".items[] | select(.metadata.name | test(\"{testnet_name}-\")) | select(.status.phase == \"Running\") | .metadata.name, .status.podIP" ], env=config.driver_env, stdin=pods.stdout)).strip(' ').split('\n')
        pods.wait()

        ips = []
        # Iterate the output until IPs have been assigned to all the testnet pods
        for pod_name, pod_ip in zip(output[::2], output[1::2]):
            if pod_name == 'null':
                break

            # Skip the origin/explorer/multiplier pods so we can count the IPs correctly
            if pod_name.find('-origin-') != -1 or pod_name.find('-explorer-') != -1 or pod_name.find('-multiplier-') != -1:
                continue

            # Parse the IP to make sure it's valid
            try:
                ipaddress.IPv4Address(pod_ip)
            except:
                break

            ips.append(pod_ip)

        if len(ips) != count:
            print(f"Waiting for all pods to be assigned an IP...")
            time.sleep(2)
        else:
            break

    conf = """
serverFiles:
  prometheus.yml:
    scrape_configs:
      - job_name: prometheus
        static_configs:
        - targets:
""" + '\n'.join(['            - ' + ip + ':8090' for ip in ips])

    print(conf)
    with tempfile.NamedTemporaryFile() as tmpfile:
        tmpfile.write(conf.encode('utf-8'))
        tmpfile.flush()
        run_or_die(config, ["helm", "upgrade", "--install", "prometheus", "prometheus-community/prometheus", "-f", tmpfile.name])
        wait_for_helm_pod(config, "prometheus-")

def prometheus_down(config):
    """ Let helm undeploy prometheus """
    helm_remove_repository(config, 'prometheus')

def tempo_up(config, testnet_name):
    """ Let helm deploy tempo """
    conf = """
tempo:
  storage:
    trace:
      backend: s3
      s3:
        bucket: tempo
        endpoint: localstack.default.svc.cluster.local:4566
        access_key: test
        secret_key: test
        insecure: true
  receivers:
    jaeger:
    opencensus:
    otlp:
      protocols:
        grpc:
          endpoint: "0.0.0.0:4317"
        http:
          endpoint: "0.0.0.0:4318"
"""

    print(conf)
    with tempfile.NamedTemporaryFile() as tmpfile:
        tmpfile.write(conf.encode('utf-8'))
        tmpfile.flush()
        run_or_die(config, ["helm", "upgrade", "--install", "tempo", "grafana/tempo", "-f", tmpfile.name])
        wait_for_helm_pod(config, "tempo-")

def tempo_down(config):
    """ Let helm undeploy tempo """
    helm_remove_repository(config, "tempo")


@click.command("up")
@click.pass_context
@click.option("--driver",
              required=True,
              default=default_driver(),
              show_default=True,
              help="The minikube driver to use")
@click.option("--zilliqa-image",
              help="The zilliqz image to use when building the zilliqa image. If none is specified scilla & zillqa will be built with a new tag and used to bring up the test network.")
@click.option("--testnet-name",
              required=True,
              default='localdev',
              show_default=True,
              help="The test network's name")
@click.option("--isolated-server-accounts",
              is_flag=True,
              default=True,
              show_default=True,
              help="Use isolated_server_accounts.json to create accounts when zilliqa is up")
@click.option("--persistence", help="A persistence directory to start the network with. Has no effect without also passing `--key-file`.")
@click.option("--key-file", help="A `.tar.gz` generated by `./testnet.sh back-up auto` containing the keys used to start this network. Has no effect without also passing `--persistence`.")
@click.option("--monitoring", help="Start monitoring - when set to false, skips grafana, prometheus, and tempo", default=True,show_default=True)
@click.option("--chain-id", help="Set the chain id", default=None)
def up_cmd(ctx, driver, zilliqa_image, testnet_name, isolated_server_accounts, persistence, key_file, monitoring, chain_id):
    """
    Build Zilliqa (via a process equivalent to the build-zilliqa & build-scilla commands), write configuration files for a
    testnet named localdev, run `localdev/config.sh up`, and start a proxy to allow the user to monitor traffic
    on the API ports.
    """
    config = get_config(ctx)
    if not zilliqa_image:
        zilliqa_image = build_zilliqa(config, driver, None, None)
    else:
        adjust_config(config, driver)

    up(config, zilliqa_image, testnet_name, isolated_server_accounts, persistence, key_file, monitoring, chain_id = chain_id)

def up(config, zilliqa_image, testnet_name, isolated_server_accounts, persistence, key_file, monitoring, chain_id = None):
    minikube = get_minikube_ip(config)
    write_testnet_configuration(config, zilliqa_image, testnet_name, isolated_server_accounts, persistence, key_file, chain_id)
    localstack_up(config)
    if monitoring:
        grafana_up(config, testnet_name)
    start_testnet(config, testnet_name, persistence)
    if monitoring:
        prometheus_up(config, testnet_name)
        tempo_up(config, testnet_name)
    restart_ingress(config);
    print("Ingress restarted; you should be ready to go...");

@click.command("isolated")
@click.option("--enable-evm", is_flag = True, help="Disable the EVM so you can start it yourself - instructions will appear in the log")
@click.option("--disable-evm", is_flag = True, help="Disable the EVM so you can start it yourself - instructions will appear in the log")
@click.option("--block-time-ms", help="Block time in ms", default=10000)
@click.option("--chain-id", help="Set the chain id", default = None)
@click.pass_context
def isolated_cmd(ctx, enable_evm, disable_evm, block_time_ms):
    """
    Run an isolated server
    """
    config = get_config(ctx)
    isolated(config, enable_evm = enable_evm or not disable_evm, block_time_ms = block_time_ms, chain_id = chain_id)

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

def isolated(config, enable_evm = True, block_time_ms = None, chain_id = None):
    build_native_to_workspace(config)
    #workspace = os.path.join(ZILLIQA_DIR, "_localdev", "isolated")
    config_file = xml.dom.minidom.parse(os.path.join(ZILLIQA_DIR, "constants.xml"))
    xml_replace_element(config_file, config_file.documentElement, "LAUNCH_EVM_DAEMON", "true" if enable_evm else "false")
    xml_replace_element(config_file, config_file.documentElement, "LOOKUP_NODE_MODE", "true")
    xml_replace_element(config_file, config_file.documentElement, "ENABLE_SC", "true")
    xml_replace_element(config_file, config_file.documentElement, "ENABLE_SCILLA_MULTI_VERSION", "false")
    xml_replace_element(config_file, config_file.documentElement, "SCILLA_ROOT", "scilla")
    xml_replace_element(config_file, config_file.documentElement, "SCILLA_LIB", "stdlib")
    xml_replace_element(config_file, config_file.documentElement, "ENABLE_SCILLA_MULTI_VERSION", "false")
    xml_replace_element(config_file, config_file.documentElement, "ENABLE_EVM", "true")
    xml_replace_element(config_file, config_file.documentElement, "EVM_SERVER_BINARY", "evm-ds/evm-ds")
    xml_replace_element(config_file, config_file.documentElement, "EVM_LOG_CONFIG", "evm-ds/log4rs-local.yml")
    xml_replace_element(config_file, config_file.documentElement, "EVM_SERVER_SOCKET_PATH", "/tmp/evm-server.sock")
    if chain_id is not None:
        xml_replace_element(config_file, config_file.documentElement, "CHAIN_ID", chain_id)
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
    if is_osx():
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
    # This used to contain DYLD_LIBRARY_PATH - and it probably still should,
    # but I don't have a Mac to hand - rrw 2023-06-22
    for var in [ "LD_LIBRARY_PATH" ]:
        old_path = new_env.get(var, "")
        new_env[var] = f"{target_workspace}/lib:{old_path}"
    cmd = [ "./bin/isolatedServer", "-f", "isolated-server-accounts.json", "-u", "999" ]
    if block_time_ms is not None:
        cmd.extend(["--time", str(block_time_ms)])
    print(f"Running isolated server in {target_workspace} with ${new_env}.. ")
    print(f"EVM logs will appear in /tmp/evm.log")
    run_or_die(config, cmd, in_dir = target_workspace, env = new_env)

    # print(f"> Using workspace {workspace}")
    # try:
    #     shutil.rmtree(workspace)
    # except:
    #     pass

def start_testnet(config, testnet_name, persistence):
    run_or_die(config, ["./testnet.sh", "up"], in_dir=os.path.join(TESTNET_DIR, testnet_name))

    if persistence is not None:
        # Create a tarball of persistence.
        with tarfile.open(f"_{testnet_name}/{testnet_name}-persistence.tar.gz", "w:gz") as tar:
            tar.add(persistence, arcname="persistence")

        # Wait for localstack to be running
        run_or_die(config, ["kubectl", "rollout", "status", "deployment", "localstack"])

        with open(f"_{testnet_name}/.currentTxBlk", "w") as f:
            f.write("123") # FIXME: Set a real value?
        def aws(cmd):
            run_or_die(config, ["aws", "--endpoint-url=http://localhost:4566"] + cmd, env={"PATH": os.environ["PATH"], "AWS_ACCESS_KEY_ID": "test", "AWS_SECRET_ACCESS_KEY": "test"})

        # Copy persistence to S3 in localstack. Each of the subdirectories in S3 are meant to contain only a subset of
        # persistence, but we choose to just copy everything into everywhere.
        bucket_name = "zilliqa-devnet"
        aws(["s3", "sync", f"{persistence}/", f"s3://{bucket_name}/blockchain-data/{testnet_name}/"])
        aws(["s3", "sync", f"{persistence}/", f"s3://{bucket_name}/incremental/localdev/persistence/"])
        aws(["s3", "cp", f"_{testnet_name}/.currentTxBlk", f"s3://{bucket_name}/incremental/{testnet_name}/"])
        aws(["s3", "cp", f"{testnet_name}-persistence.tar.gz", f"s3://{bucket_name}/persistence/"])

def wait_for_termination(config, keep_persistence):
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
        if len(podx) == 0 or len(pods) == 0 or (keep_persistence and len(pods) == 1 and pods[0].startswith('localstack')):
            break
        else:
            print(f": {' '.join(pods)}")
            time.sleep(2)

@click.command("down")
@click.pass_context
@click.option("--testnet-name",
              required=True,
              default='localdev',
              show_default=True,
              help="The test network's name")
@click.option("--keep-persistence",
              is_flag=True,
              default=False,
              show_default=True,
              help="A flag indicating whether to delete the persistence or not")
def down_cmd(ctx, testnet_name, keep_persistence):
    """ Bring the testnet down and stop the proxies """
    config = get_config(ctx)
    down(config, testnet_name, keep_persistence)

def down(config, testnet_name, keep_persistence):
    stop_testnet(config, testnet_name)
    stop_proxy(config, testnet_name)
    tempo_down(config)
    prometheus_down(config)
    grafana_down(config)
    if not keep_persistence:
        localstack_down(config)

    wait_for_termination(config, keep_persistence)

def stop_testnet(config, testnet_name):
    # tediously, testnet.sh has a habit of returning non-zero error codes when eg. the testnet has already been destroyed :-(
    run_or_die(config, ["sh", "-c", "echo localdev | ./testnet.sh down"], in_dir=os.path.join(TESTNET_DIR, testnet_name), allow_failure = True)

def write_testnet_configuration(config, zilliqa_image, testnet_name, isolated_server_accounts, persistence, key_file, chain_id = None):
    instance_dir = os.path.join(TESTNET_DIR, testnet_name)
    minikube_ip = get_minikube_ip(config)

    if os.path.exists(instance_dir):
        print(f"Removing old testnet configuration ..")
        shutil.rmtree(instance_dir)
    print(f"Generating testnet configuration .. ")
    cmd = ["./bootstrap.py", testnet_name, "--clusters", "minikube", "--constants-from-file",
           os.path.join(ZILLIQA_DIR, "constants.xml"),
           "--image", zilliqa_image,
           "-n", "20",
           "-d", "5",
           "-l", "1",
           "--guard", "4/10",
           "--gentxn", "false",
           "--multiplier-fanout", "1",
           "--host-network", "false",
           "--https", "localdomain",
           "--seed-multiplier", "true",
           "--skip-non-guard-ds", "true",
           "--localstack", "true"]
    cmd = cmd + ([ "--isolated-server-accounts", os.path.join(ZILLIQA_DIR, "isolated-server-accounts.json") ] if isolated_server_accounts else [])
    cmd = cmd + [ "-f" ]
    if persistence is not None and key_file is not None:
        bucket_name = "zilliqa-devnet"
        cmd.extend([
            "--bucket", bucket_name,
            "--recover-from-s3", f"s3://{bucket_name}/persistence/{testnet_name}-persistence.tar.gz",
            "--recover-key-files", key_file,
        ])
    run_or_die(config, cmd, in_dir = TESTNET_DIR)

    constants_xml_target_path = os.path.join(TESTNET_DIR, f"{testnet_name}/configmap/constants.xml")
    config_file = xml.dom.minidom.parse(constants_xml_target_path)
    xml_replace_element(config_file, config_file.documentElement, "METRIC_ZILLIQA_HOSTNAME", "0.0.0.0")
    xml_replace_element(config_file, config_file.documentElement, "METRIC_ZILLIQA_PORT", "8090")
    xml_replace_element(config_file, config_file.documentElement, "METRIC_ZILLIQA_PROVIDER", "PROMETHEUS")
    xml_replace_element(config_file, config_file.documentElement, "METRIC_ZILLIQA_MASK", "ALL")
    xml_replace_element(config_file, config_file.documentElement, "TRACE_ZILLIQA_HOSTNAME", "tempo.default.svc.cluster.local")
    xml_replace_element(config_file, config_file.documentElement, "TRACE_ZILLIQA_PORT", "4317")
    xml_replace_element(config_file, config_file.documentElement, "TRACE_ZILLIQA_PROVIDER", "OTLPGRPC")
    xml_replace_element(config_file, config_file.documentElement, "TRACE_ZILLIQA_MASK", "ALL")
    if chain_id is not None:
        xml_replace_element(config_file, config_file.documentElement, "CHAIN_ID", chain_id)
    output_config = config_file.toprettyxml(newl='')
    with open(constants_xml_target_path, 'w') as f:
        f.write(output_config)

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
             "origin" : { "host" : f"{testnet_name}-origin.localdomain", "port" : 5304 },
             "grafana" : { "host" : f"{testnet_name}-grafana.localdomain", "port" : 5305 } }

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
                    f"reverse:http://127.0.0.1:3500",
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

def build_native_to_workspace(config):
    workspace = os.path.join(ZILLIQA_DIR, "_localdev")
    print(f"> Using workspace {workspace}")
    print(f"> Startup: removing workspace to avoid pollution ...")
    try:
        shutil.rmtree(workspace)
    except:
        pass
    build_env = os.environ.copy()
    build_env['SCILLA_REPO_ROOT'] = SCILLA_DIR
    # Let's start off by building Scilla, in case it breaks.
    run_or_die(config, ["make"], in_dir = SCILLA_DIR, env = build_env)
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
    shutil.copyfile(os.path.join(ZILLIQA_DIR, "evm-ds", "log4rs-local.yml"),
                    os.path.join(evm_ds_tgt, "log4rs-local.yml"))
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

def build_scilla(config, driver, tag):
    adjust_config(config, driver)

    build_env = config.driver_env.copy()
    build_env["DOCKER_BUILDKIT"] = "1"

    image_name = "scilla:" + (tag if tag else gen_tag())
    progress_arg = get_progress_arg()
    run_or_die(config, [config.docker_binary, "build", progress_arg, ".", "-t", image_name, "-f", os.path.join(SCILLA_DIR, "docker", "Dockerfile")], in_dir = SCILLA_DIR, env = build_env,
               capture_output = False)
    return image_name

def build_native_scilla(config):
    build_env = os.environ.copy()
    build_env.update(config.default_env)

    if not build_env.get("VCPKG_ROOT"):
        raise GiveUp("Environment variable VCPKG_ROOT must be defined to point to vcpkg")

    vcpkg_triplet = run_or_die(config, ["scripts/vcpkg_triplet.sh"], in_dir = SCILLA_DIR, env = build_env,
               capture_output = True).decode('utf-8')

    build_env["SCILLA_REPO_ROOT"] = SCILLA_DIR
    build_env["PKG_CONFIG_PATH"] = f"{os.path.join(SCILLA_DIR, 'vcpkg_installed', vcpkg_triplet, 'lib', 'pkg-config')}" + f":{build_env['PKG_CONFIG_PATH']}" if build_env.get('PKG_CONFIG_PATH') else ''

    # Cleanup
    shutil.rmtree(os.path.join(SCILLA_DIR, "vcpkg_installed"), ignore_errors = True)
    shutil.rmtree(os.path.join(SCILLA_DIR, "_build"), ignore_errors = True)

    output = run_or_die(config, ["grep", "OCAML_VERSION_RECOMMENDED=", "Makefile"], in_dir = SCILLA_DIR, env = build_env,
               capture_output = True).decode('utf-8')
    result = re.match(r'^OCAML_VERSION_RECOMMENDED=([0-9\.]+)$', output)
    if len(result.groups()) != 1:
        raise GiveUp("Couldn't infer OCaml recommended version for Scilla")

    # Build OCaml
    run_or_die(config, ["opam", "init", f"--compiler=ocaml-base-compiler.{result.group(1)}", "--yes"], in_dir = SCILLA_DIR, env = build_env,
               capture_output = False)
    run_or_die(config, ["./scripts/build_deps.sh"], in_dir = SCILLA_DIR, env = build_env,
               capture_output = False)

    for p in map(
        # Skip the 'export ' and split at '=' into a tuple
        lambda x: x.split('='),
        filter(
            lambda x: x.find('export') == -1,
            re.split(";\n?",
            run_or_die(config, ["opam", "env"], capture_output=True).decode('utf-8')))):
        if len(p) > 1:
            build_env[p[0]] = p[1][1:-1]

    # Set PWD explicitly to get around usage in Scilla's Makefile; in sub-shells
    # this will resolve to the wrong directly which could mess up rpath.
    build_env["PWD"] = SCILLA_DIR
    run_or_die(config, ["make", "opamdep"], in_dir = SCILLA_DIR, env = build_env,
               capture_output = False)
    run_or_die(config, ["make"], in_dir = SCILLA_DIR, env = build_env,
               capture_output = False)

@click.command("build-scilla")
@click.option("--driver",
              required=False,
              help="The minikube driver to use")
@click.option("--tag",
              help="The scilla image tag. Will be generated if not given.")
@click.option("--native",
              is_flag=True,
              default=False,
              show_default=True,
              help="Build Scilla natively.")
@click.pass_context
def build_scilla_cmd(ctx, driver, tag, native):
    """
    Builds a scilla image.
    """
    if native:
        if tag:
            raise GiveUp("--native and --tag can't be specified together")
        elif ctx.params["driver"]:
            print("ignoring --driver since --native is specified; building natively...")
    else:
        if not driver:
            driver = default_driver()
        ctx.params["driver"] = driver


    config = get_config(ctx)
    if native:
       build_native_scilla(config)
    else:
       build_scilla(config, driver, tag)

def build_zilliqa(config, driver, scilla_image, tag):
    if not scilla_image:
        scilla_image = build_scilla(config, driver, None)
    else:
        adjust_config(config, driver)

    build_env = config.driver_env.copy()
    build_env["DOCKER_BUILDKIT"] = "1"

    image_name = "zilliqa:" + (tag if tag else gen_tag())
    progress_arg = get_progress_arg();
    run_or_die(config, [config.docker_binary, "build", progress_arg, ".", "--build-arg", f"SCILLA_IMAGE={scilla_image}", "-t", image_name, "-f", os.path.join(ZILLIQA_DIR, "docker", "Dockerfile")], in_dir = ZILLIQA_DIR, env = build_env,
               capture_output = False)
    return image_name

@click.command("build-zilliqa")
@click.option("--driver",
              required=True,
              default=default_driver(),
              show_default=True,
              help="The minikube driver to use")
@click.option("--scilla-image",
              required=True,
              help="the scilla image to use when building the zilliqa image (i.e. scilla:<tag>)")
@click.option("--tag",
              help="The zilliqa image tag. Will be generated if not given.")
@click.pass_context
def build_zilliqa_cmd(ctx, driver, scilla_image, tag):
    """
    Builds a zilliqa image.
    """
    config = get_config(ctx)
    build_zilliqa(config, driver, scilla_image, tag)

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
@click.option("--driver",
              required=True,
              default=default_driver(),
              show_default=True,
              help="The minikube driver to use")
@click.option("--zilliqa-image",
              help="The zilliqz image to use when building the zilliqa image. If none is specified scilla & zillqa will be built with a new tag and used to bring up the test network.")
@click.option("--testnet-name",
              required=True,
              default='localdev',
              show_default=True,
              help="The test network's name")
@click.option("--isolated-server-accounts",
              is_flag=True,
              default=True,
              show_default=True,
              help="Use isolated_server_accounts.json to create accounts when zilliqa is up")
@click.option("--keep-persistence",
              is_flag=True,
              default=False,
              show_default=True,
              help="A flag indicating whether to delete the persistence or not")
@click.option("--persistence", help="A persistence directory to start the network with. Has no effect without also passing `--key-file`.")
@click.option("--key-file", help="A `.tar.gz` generated by `./testnet.sh back-up auto` containing the keys used to start this network. Has no effect without also passing `--persistence`.")
def reup_cmd(ctx, driver, zilliqa_image, testnet_name, isolated_server_accounts, keep_persistence, persistence, key_file):
    """
    Equivalent to `localdev down && localdev up`
    """
    config = get_config(ctx)
    down(config, testnet_name, keep_persistence)
    if not zilliqa_image:
        zilliqa_image = build_zilliqa(config, driver, None, None)
    else:
        adjust_config(config, driver)
    up(config, zilliqa_image, testnet_name, isolated_server_accounts, persistence, key_file)

@click.command("show-proxy")
@click.pass_context
@click.option("--testnet-name",
              required=True,
              default='localdev',
              show_default=True,
              help="The test network's name")
def show_proxy_cmd(ctx, testnet_name):
    """
    Show proxy settings
    """
    config = get_config(ctx)
    show_proxy(config, testnet_name)

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
@click.option("--testnet-name",
              required=True,
              default='localdev',
              show_default=True,
              help="The test network's name")
def start_proxy_cmd(ctx, testnet_name):
    """
    Start the mitm proxies
    """
    config = get_config(ctx)
    start_proxy(config, testnet_name)
    show_proxy(config, testnet_name)

@click.command("write-testnet-config")
@click.pass_context
@click.option("--zilliqa-image",
              help="The zilliqz image to use when building the zilliqa image. If none is specified scilla & zillqa will be built with a new tag and used to bring up the test network.")
@click.option("--testnet-name",
              required=True,
              default='localdev',
              show_default=True,
              help="The test network's name")
@click.option("--isolated-server-accounts",
              is_flag=True,
              default=False,
              show_default=True,
              help="Use isolated_server_accounts.json to create accounts when zilliqa is up")
def write_testnet_config_cmd(ctx, zilliqa_image, testnet_name, isolated_server_accounts):
    """
    Write config for the testnet

    TAG is the tag for the Zilliqa docker image to run.
    """
    config = get_config(ctx)
    write_testnet_configuration(config, zilliqa_image, testnet_name, isolated_server_accounts)

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
@click.option("--keep-persistence",
              is_flag=True,
              default=False,
              show_default=True,
              help="A flag indicating whether to delete the persistence or not")
def wait_for_termination_cmd(ctx, keep_persistence):
    """
    Wait for running pods to terminate so we can start a new network
    """
    config = get_config(ctx)
    wait_for_termination(config, keep_persistence)

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

    You need the local-dev-minikube branch of testnet if it hasn't yet
    been merged.

    You will need to have built scilla.

    localdev.py runs in stages:
     setup           - Sets up k8s (through minikube, and colima on OS X)
     up              - Brings the system up (compiling images if needed).
     down            - Brings the system down.
     teardown-podman - On OS X only, tears down podman

    If you reboot your machine, remember to: [`colima start` (on OS X)] `minikube start`.
    If you want to delete the environment or create a new one, do: `minikube delete`.

    There are also commands to collect logs, and one to restart the
    ingress, since it sometimes sticks.

    WARNING: Only tested so far on Ubuntu 22.04 . OS X MAY NOT WORK.

    """
    ctx.obj = Config()
    ctx.obj.setup()

cli.add_command(teardown_podman)
cli.add_command(setup)
cli.add_command(build_scilla_cmd)
cli.add_command(build_zilliqa_cmd)
cli.add_command(up_cmd)
cli.add_command(down_cmd)
cli.add_command(show_proxy_cmd)
cli.add_command(which_pod_said_cmd)
cli.add_command(debug)
cli.add_command(reup_cmd)
cli.add_command(log_snapshot_cmd)
cli.add_command(restart_ingress_cmd)
cli.add_command(isolated_cmd)

if __name__ == "__main__":
    cli()
