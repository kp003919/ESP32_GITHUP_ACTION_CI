pipeline {
    agent { label 'hil' }
    
    stages {

        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Install PlatformIO') {
            steps {
                sh 'pip3 install -U platformio'
            }
        }

        stage('Build HIL Firmware') {
            steps {
                sh 'pio run -e hil_test'
            }
        }

        stage('Upload Firmware') {
            steps {
                sh 'pio run -e hil_test -t upload'
                sleep 3
            }
        }

        stage('Run HIL Tests') {
            steps {
                sh 'pytest --junitxml=pytest-report.xml'
            }
        }
    }

    post {
        always {
            junit 'pytest-report.xml'
        }
    }
}
