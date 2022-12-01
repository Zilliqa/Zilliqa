def podDefinition = """
apiVersion: "v1"
kind: "Pod"
spec:
  nodeSelector:
    beta.kubernetes.io/os: "linux"
  containers:
  - name: "scilla"
    image: "zilliqa/scilla:v0.13.1-alpha"
    command:
    - cat
    tty: true
  - name: "ubuntu"
    image: "zilliqa/zilliqa-ccache:v8.4.0"
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
                stage('Build') {
                    sh "update-alternatives --install /usr/bin/python python \$(which python3) 1"
                    sh "git config --global --add safe.directory '*'"
                    // Hack: since Jenkins checks out the branch to a different directory each time and given
                    //       that ccache is very sensitive to code coverage & debug info, we checkout out
                    //       to the same directoy as the ccache compilation and build there to benefit from the cache. 
                    sh "git clone file://\"\$(pwd)\" /root/zilliqa"
                    sh "git -C /root/zilliqa checkout \$(git rev-parse HEAD)"
                    sh "ln -s /root/zilliqa/build build"
                    sh "cd /root/zilliqa && VCPKG_ROOT=${env.VCPKG_ROOT} CCACHE_BASEDIR=\"\$(pwd)\" ./scripts/ci_build.sh"
                }
                stage('Integration test') {
                    sh "scripts/integration_test.sh --setup-env"
                }
                stage('Integration test JS') {
                    sh "scripts/integration_test_js.sh --setup-env"
                }
                stage('Report coverage') {
                    // Code coverage is currently only implemented for GCC builds, so OSX is currently excluded from reporting
                    sh "cd /root/zilliqa && scripts/ci_report_coverage.sh"
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
