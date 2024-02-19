#!/bin/bash

TOPO=${1}
ALGS=("SimpleController" "ReactiveLoadController1" "ReactiveLoadController2" "ReactiveLoadController3")
DIR="$(dirname "$(realpath "$0")")/"
ESTI_DIR="$(realpath "${DIR}/../topologies/${TOPO}/estimate_files")"
OUT_DIR="$(realpath "${DIR}/../outputs/${TOPO}")"

mkdir -p "${ESTI_DIR}"

(
	cd "${ESTI_DIR}" || exit
	for alg in "${ALGS[@]}"; do
		dir="${OUT_DIR}/${alg}"
		if [[ -d "${dir}" ]]; then
			python3 "${DIR}/gen_estimate.py" "${dir}/ecofen-trace.csv" -o "${alg}_estimate.json"
		fi
	done
)
