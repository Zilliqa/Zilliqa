def podDefinition = """
apiVersion: "v1"
kind: "Pod"
spec:
  nodeSelector:
    beta.kubernetes.io/os: "linux"
  containers:
  - name: "scilla"
    image: "zilliqa/scilla:v0.11.0"
    command:
    - cat
    tty: true
  - name: "ubuntu"
    image: "648273915458.dkr.ecr.us-west-2.amazonaws.com/zilliqa:v8.2.0-deps"
    command:
    - cat
    tty: true
"""

timestamps {
  ansiColor('gnome-terminal') {
    podTemplate(yaml: podDefinition) {
      node(POD_LABEL) {
        stage('Checkout scm') {
            checkout scm
        }
        stage('Import Scilla') {
            container('scilla') {
                sh "mkdir ./moving_folder"
                sh "cp -r /scilla ./moving_folder/scilla"
            }
            container('ubuntu') {
                sh "cp -r ./moving_folder/scilla /scilla"
                sh "rm -rf ./moving_folder"
                sh "ls -la /scilla/0"
            }
        }
        container('ubuntu') {
            env.VCPKG_ROOT="/vcpkg"
            stage('Configure environment') {
                sh "./scripts/setup_environment.sh"
                // sh "git clone https://github.com/microsoft/vcpkg ${env.VCPKG_ROOT}"
                // sh "export VCPKG_FORCE_SYSTEM_BINARIES=1 && cd ${env.VCPKG_ROOT} && git checkout 2022.07.25 && ${env.VCPKG_ROOT}/bootstrap-vcpkg.sh"
            }
            stage('Build') {
                sh "git config --global --add safe.directory '*'"
                sh "export VCPKG_ROOT=${env.VCPKG_ROOT} && ./scripts/ci_build.sh"
            }
            stage('Report coverage') {
                // Code coverage is currently only implemented for GCC builds, so OSX is currently excluded from reporting
                sh "./scripts/ci_report_coverage.sh"
            }
        }
      }
    }
  }
}
