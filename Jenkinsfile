def podDefinition = """
apiVersion: "v1"
kind: "Pod"
spec:
  nodeSelector:
    beta.kubernetes.io/os: "linux"
  containers:
  - name: "scilla"
    image: "zilliqa/scilla:456db82c"
    command:
    - cat
    tty: true
  - name: "ubuntu"
    image: "648273915458.dkr.ecr.us-west-2.amazonaws.com/zilliqa:v8.3.0-deps"
    command:
    - cat
    tty: true
    resources:
      requests:
        ephemeral-storage: "20Gi"
      limits:
        ephemeral-storage: "20Gi"
"""

String skipciMessage = 'Aborting because commit message contains [skip ci]'

timestamps {
  ansiColor('gnome-terminal') {
    podTemplate(yaml: podDefinition) {
      timeout(time: 60, unit: 'MINUTES') {
        node(POD_LABEL) {
          try {
            stage('Checkout scm') {
                checkout scm
                def pr_skipci = "0"
                try {
                  if (env.CHANGE_TITLE != null && env.CHANGE_TITLE != "") {
                    pr_skipci = sh(script: "echo ${env.CHANGE_TITLE.replace("(","").replace(")","").replace("'","")} | fgrep -ie '[skip ci]' -e '[ci skip]' | wc -l", returnStdout: true).trim()
                  }
                } catch (err) {
                  println err.getMessage()
                  error("Error reading the Pull Request title, please check and eventually remove special characters")
                }
                def skipci = sh(script: "git log -1 --pretty=%B | fgrep -ie '[skip ci]' -e '[ci skip]' | wc -l", returnStdout: true).trim()
                if (skipci != "0" || pr_skipci != "0") {
                  error(skipciMessage)
                }
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
                    sh "apt update -y && apt install -y patchelf chrpath"
                    sh "chmod u+w /scilla/0/_build/install/default/bin/*" 
                    sh "patchelf --set-rpath \"\$(pwd)/build/vcpkg_installed/x64-linux-dynamic/lib\" /scilla/0/_build/install/default/bin/*" 
                }
            }
            container('ubuntu') {
                env.VCPKG_ROOT="/vcpkg"
                stage('Configure environment') {
                    sh "./scripts/setup_environment.sh"
                    // sh "git clone https://github.com/microsoft/vcpkg ${env.VCPKG_ROOT}"
                    // sh "export VCPKG_FORCE_SYSTEM_BINARIES=1 && cd ${env.VCPKG_ROOT} && git checkout 2022.09.27 && ${env.VCPKG_ROOT}/bootstrap-vcpkg.sh"
                }
                stage('Build') {
                    sh "git config --global --add safe.directory '*'"
                    sh "export VCPKG_ROOT=${env.VCPKG_ROOT} && export PATH=\$(pwd)/.local/bin:\$PATH && ./scripts/ci_build.sh"
                }
                stage('Integration test') {
                    sh "scripts/integration_test.sh --setup-env"
                }
                stage('Integration test JS') {
                    sh "scripts/integration_test_js.sh --setup-env"
                }
                stage('Report coverage') {
                    // Code coverage is currently only implemented for GCC builds, so OSX is currently excluded from reporting
                    sh "./scripts/ci_report_coverage.sh"
                }
            }
          } catch (err) {
            if (err.getMessage() == skipciMessage)
              currentBuild.result = 'SUCCESS'
            else
              throw err
          }
        }
      }
    }
  }
}
