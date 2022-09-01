def podDefinition = """
apiVersion: "v1"
kind: "Pod"
spec:
  nodeSelector:
    kubernetes.io/os: "linux"
  containers:
  - name: "scilla"
    image: "zilliqa/scilla:v0.11.0"
    command:
    - cat
    tty: true
  - name: "ubuntu"
    image: "ubuntu:bionic"
    command:
    - cat
    tty: true
"""

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
        stage('Configure environment') {
            sh "./scripts/setup_environment.sh"
        }
        stage('Build') {
            sh "git config --global --add safe.directory '*'"
            sh "sleep 7000"
            sh "./scripts/ci_build.sh"
        }
        stage('Report coverage') {
            // Code coverage is currently only implemented for GCC builds, so OSX is currently excluded from reporting
            sh "./scripts/ci_report_coverage.sh"
        }
    }
  }
}
