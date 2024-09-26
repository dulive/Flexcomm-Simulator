#!/bin/bash

BASES=("SimpleController" "ReactiveLoadController1" "ReactiveLoadController2" "ReactiveLoadController3")
DIR="$(dirname "$(realpath "$0")")/"

while [[ ${#} -gt 0 ]]; do
  case ${1} in
  -t | --topology)
    TOPO="${2}"
    shift 2
    ;;
  -b | --bases)
    BASES+=("${2}")
    shift 2
    ;;
  *)
    echo "Unkown option ${1}"
    exit 1
    ;;
  esac
done

ESTI_DIR="$(realpath "${DIR}/../topologies/${TOPO}/estimate_files")"
OUT_DIR="$(realpath "${DIR}/../outputs/${TOPO}")"

mkdir -p "${ESTI_DIR}"

(
  cd "${ESTI_DIR}" || exit
  for alg in "${BASES[@]}"; do
    dir="${OUT_DIR}/${alg}"
    if [[ -d "${dir}" ]]; then
      python3 "${DIR}/gen_estimate.py" "${dir}/ecofen-trace.csv" -o "${alg}_estimate.json"
    fi
  done
)
