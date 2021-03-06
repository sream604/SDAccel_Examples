#!/usr/bin/env groovy

properties([buildDiscarder(logRotator(artifactDaysToKeepStr: '',
artifactNumToKeepStr: '', daysToKeepStr: '30', numToKeepStr: '5')), [$class:
'GithubProjectProperty', displayName: '', projectUrlStr:
'https://gitenterprise.xilinx.com/SDx-Hub/apps/'], pipelineTriggers([[$class:
'GitHubPushTrigger']])])

devices = ''
devices += ' xilinx:adm-pcie-7v3:1ddr:3.0'
devices += ' xilinx:xil-accel-rd-ku115:4ddr-xpr:3.2'
devices += ' xilinx:adm-pcie-ku3:2ddr-xpr:3.2'

version = '2016.3'

def buildExample(target, dir, devices, workdir) {
	return { ->
//		stage("${dir}-${target}") {
//			node('rhel6 && xsjrdevl') {
				/* Retry up to 3 times to get this to work */

				if ( target == "sw_emu" ) {
					cores = 1
					queue = "medium"
				} else {
					cores = 8
					queue = "long"
				}

				sh """#!/bin/bash -e

cd ${workdir}

. /tools/local/bin/modinit.sh > /dev/null 2>&1
module use.own /proj/picasso/modulefiles

module add vivado/${version}_daily
module add vivado_hls/${version}_daily
module add sdaccel/${version}_daily
module add opencv/vivado_hls
module add lsf

cd ${dir}

echo
echo "-----------------------------------------------"
echo "PWD: \$(pwd)"
echo "-----------------------------------------------"
echo

rsync -rL \$XILINX_SDX/Vivado_HLS/lnx64/tools/opencv/ lib/

bsub -I -q ${queue} -R "osdistro=rhel && osver==ws6" -n ${cores} -R "span[ptile=${cores}]" -J "\$(basename ${dir})-${target}" <<EOF
make -k TARGETS=${target} DEVICES=\"${devices}\" all
EOF

"""
			}
			retry(3) {
				sh """#!/bin/bash -e

cd ${workdir}

. /tools/local/bin/modinit.sh > /dev/null 2>&1
module use.own /proj/picasso/modulefiles

module add vivado/${version}_daily
module add vivado_hls/${version}_daily
module add sdaccel/${version}_daily
module add opencv/vivado_hls

module add proxy

cd ${dir}

echo
echo "-----------------------------------------------"
echo "PWD: \$(pwd)"
echo "-----------------------------------------------"
echo

export PYTHONUNBUFFERED=true

make -k TARGETS=${target} DEVICES=\"${devices}\" check

"""
			}
//		}
//	}
}

timestamps {
// Always build on the same host so that the workspace is reused
node('rhel6 && xsjrdevl && xsjrdevl110') {
try {
	Calendar now = Calendar.getInstance();

	if (now.get(Calendar.DAY_OF_WEEK) == Calendar.TUESDAY) {
		stage('clean') {
			sh 'echo "FIXME: Clean not implemented"'
		}
	}

	stage("checkout") {
		checkout scm
		step([$class: 'GitHubCommitStatusSetter'])
	}

	stage('pre-check') {
		sh """
. /tools/local/bin/modinit.sh > /dev/null 2>&1
module use.own /proj/picasso/modulefiles

module add vivado/${version}_daily
module add vivado_hls/${version}_daily
module add sdaccel/${version}_daily
module add opencv/vivado_hls

module add proxy

./utility/check_license.sh LICENSE.txt
./utility/check_readme.sh
"""
	}

	stage('configure') {
		sh 'git ls-files | grep description.json | sed -e \'s/\\.\\///\' -e \'s/\\/description.json//\' > examples.dat'
		examplesFile = readFile 'examples.dat'
		examples = examplesFile.split('\\n')

	}

	workdir = pwd()

	stage('sw_emu') {
		def swEmuSteps = [:]

		for(int i = 0; i < examples.size(); i++) {
			name = "${examples[i]}-sw_emu"
			swEmuSteps[name] = buildExample('sw_emu', examples[i], devices, workdir)
		}

		parallel swEmuSteps
	}

	stage('hw') {
		def hwSteps = [:]

		for(int i = 0; i < examples.size(); i++) {
			name = "${examples[i]}-hw"
			hwSteps[name] = buildExample('hw', examples[i], devices, workdir)
		}

		parallel hwSteps
	}

} catch (e) {
	currentBuild.result = "FAILED"
	throw e
} finally {
	step([$class: 'GitHubCommitStatusSetter'])
	step([$class: 'Mailer', notifyEveryUnstableBuild: true, recipients: 'spenserg@xilinx.com', sendToIndividuals: false])
	// Cleanup .Xil Files after run
	sh 'find . -name .Xil | xargs rm -rf'
} // try
} // node
} // timestamps


