#!/bin/bash

DIR="$(dirname "$(realpath "$0")")/"
INPUT="outputs"
OUTPUT="graphs"
TOPOLOGIES="topologies"

while [[ ${#} -gt 0 ]]; do
	case ${1} in
	-t | --topology)
		TOPOS+=("${2}")
		shift 2
		;;
	-f | --flexibilities)
		FLEXES+=("${2}")
		shift 2
		;;
	-e | --estimates)
		ESTIS+=("${2}")
		shift 2
		;;
	-o | --output-dir)
		OUTPUT=${2}
		shift 2
		;;
	-a | --algorithms)
		ALGS+=("${2}")
		shift 2
		;;
	-b | --bases)
		BASES+=("${2}")
		shift 2
		;;
	-i | --input-dir)
		INPUT=${2}
		shift 2
		;;
	-d | --topologies-dir)
		TOPOLOGIES=${2}
		shift 2
		;;
	*)
		echo "Unkown option ${1}"
		exit 1
		;;
	esac
done

TOPO_DIR="$(realpath "${DIR}/../${TOPOLOGIES}")"
INPUT_DIR="$(realpath "${DIR}/../${INPUT}")"

gen_graphs() {
    python ./utils/ecofen-graphs.py -t "${1}" -e "${2}" -f "${3}" -o "${OUTPUT}" -a "${ALGORITHMS[@]}" -b "${BASES[@]}" -i "${INPUT}" -d "${TOPOLOGIES}"
}

recursive_flex() {
    if [[ -d "${3}" ]]; then
        for c in "${3}"/*; do
            recursive_flex "${1}" "${2}" "${c}"
        done
    elif [[ -f "${3}" ]]; then
        flex_name="${3##${TOPO_DIR}/${1}/flex_files/}"
        flex_name="${flex_name%%.*}"
        gen_graphs "${1}" "${2}" "${flex_name}"
    else
        echo "Invalid flexibility ${3}" 1>&2
    fi
}

loop_flexes() {
    flex_dir="${TOPO_DIR}/${1}/flex_files"
    if [[ -z "${FLEX_FILES[*]}" ]]; then
		recursive_flex "${1}" "${2}" "${flex_dir}"
	else
		for flex in "${FLEX_FILES[@]}"; do
			if [[ -f "${flex_dir}/${flex}.json" ]]; then
				gen_graphs "${1}" "${2}" "${flex}"
			else
				recursive_flex "${1}" "${2}" "${flex_dir}/${flex}"
			fi
		done
	fi
}

loop_estimates() {
    esti_dir="${TOPO_DIR}/${1}/estimate_files"
	if [[ -z "${ESTIS[*]}" ]]; then
	    for esti_file in "${esti_dir}"/*; do
			esti="${esti_file%%.*}"
			esti="${esti##*/}"
			loop_flexes "${1}" "${esti}"
		done
	else
		for esti in "${ESTIS[@]}"; do
			if [[ -f "${esti_dir}/${esti}.json" ]]; then
    			loop_flexes "${1}" "${esti}"
            fi
		done
	fi
}

if [[ -z ${TOPOS[*]} ]]; then
	for dir in "${INPUT_DIR}"/*/; do
		loop_estimates "$(basename "${dir}")"
	done
else
	for topo in "${TOPOS[@]}"; do
		if [[ -d "${INPUT_DIR}/${topo}" ]]; then
			loop_estimates "${topo}"
		else
			echo "invalid topology ${topo}"
		fi
	done
fi
