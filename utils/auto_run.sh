#!/bin/bash

stop_server() {
	if [[ -n ${SERVER_PID} ]]; then
		echo "### Stopping extenal energy server"
		kill -KILL "${SERVER_PID}"
		unset SERVER_PID
	fi
}

trap stop_server EXIT

while [[ ${#} -gt 0 ]]; do
	case ${1} in
	-t | --topology)
		TOPO=${2}
		shift 2
		;;
	-f | --flex-files)
		FLEXES+=("${2}")
		shift 2
		;;
	-e | --esti-files)
		ESTIS+=("${2}")
		shift 2
		;;
	-a | --algorithms)
		ALGS+=("${2}")
		shift 2
		;;
	*)
		echo "Unkown option ${1}"
		exit 1
		;;
	esac
done

if [[ -z ${TOPO} ]]; then
	echo "a topology is needed" 1>&2
	exit 1
fi

if [[ -z ${ALGS[*]} ]]; then
	echo "at least one algorithm is needed" 1>&2
	exit 1
fi

DIR="$(dirname "$(realpath "$0")")/"
OUT_DIR="$(realpath "${DIR}/../outputs/${TOPO}")"
FLEX_DIR="$(realpath "${DIR}/../topologies/${TOPO}/flex_files")"
ESTI_DIR="$(realpath "${DIR}/../topologies/${TOPO}/estimate_files")"
SERVER="$(realpath "${DIR}/../ns-3.35/utils/energy-api-server.py")"

recursive_flex_run() {
	if [[ -d "${FLEX_DIR}/${3}" ]]; then
		for flex_file in "${FLEX_DIR}/${3}"/*; do
			flex="${flex_file##"${FLEX_DIR}/"}"
			recursive_flex_run "${1}" "${2}" "${flex}"
		done
	elif [[ -f "${FLEX_DIR}/${3}" ]]; then
		if [[ "${1}" == "External" ]]; then
			echo "### Starting extenal energy server"
			${SERVER} -f "${FLEX_DIR}/${3}" -e "${ESTI_DIR}/${2}" &
			SERVER_PID=$!
			alg="${1}"
		else
			alg="ns3::${1}"
		fi
		echo "### Using flex file: ${3}; Using esti file: ${2}"
		make run TOPO="${TOPO}" CONTROLLER="${alg}" FLEXFILE="flex_files/${3}" ESTIFILE="estimate_files/${2}"
		dir_name="${OUT_DIR}/${1}/${2%%.json}/${3%%.json}"
		mkdir -p "${dir_name}"
		mv "${OUT_DIR}"/*.* "${dir_name}"
		if [[ "${1}" == "External" ]]; then
			stop_server
			read -n 1 -s -r -p "[Continue]"
			echo
		fi
	else
		echo "Invalid flex ${3}" 1>&2
	fi
}

do_flex_run() {
	if [[ -z ${FLEXES[*]} ]]; then
		recursive_flex_run "${1}" "${2}" ""
	else
		for flex in "${FLEXES[@]}"; do
			recursive_flex_run "${1}" "${2}" "${flex}"
		done
	fi
}

for alg in "${ALGS[@]}"; do
	if [[ -z ${ESTIS[*]} ]]; then
		for esti_file in "${ESTI_DIR}"/*; do
			do_flex_run "${alg}" "$(basename "${esti_file}")"
		done
	else
		for esti in "${ESTIS[@]}"; do
			do_flex_run "${alg}" "${esti}"
		done
	fi
done
