pipeline {
    agent { label 'hil' }

    environment {
        VENV = "${WORKSPACE}/venv"
        PIO = "${WORKSPACE}/venv/bin/platformio"
        PYTEST = "${WORKSPACE}/venv/bin/pytest"
    }

    stages {

        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Setup PlatformIO Environment') {
            steps {
                sh '''
                    if [ ! -d "$VENV" ]; then
                        python3 -m venv $VENV
                        $VENV/bin/pip install --upgrade pip
                        $VENV/bin/pip install platformio pytest
                    fi
                '''
            }
        }

        stage('Build HIL Firmware') {
            steps {
                sh '''
                    $PIO run -e hil_test
                '''
            }
        }

        stage('Upload Firmware') {
            steps {
                sh '''
                    $PIO run -e hil_test -t upload
                '''
                sleep 3
            }
        }

        stage('Run HIL Tests') {
            steps {
                sh '''
                    $PYTEST --junitxml=pytest-report.xml
                '''
            }
        }
    }

    post {
        always {
            // Ensure junit runs INSIDE the agent context
            node('hil') {
                junit 'pytest-report.xml'
            }
        }
    }
}
