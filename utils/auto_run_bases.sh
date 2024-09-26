#!/bin/bash

BASES=("SimpleController" "ReactiveLoadController1" "ReactiveLoadController2" "ReactiveLoadController3")
DIR="$(dirname "$(realpath "$0")")/"
USE_BASES=true

while [[ ${#} -gt 0 ]]; do
  case ${1} in
  -t | --topology)
    TOPO="${2}"
    shift 2
    ;;
  -e | --external-bases)
    EXTERNAL_BASES+=("${2}")
    shift 2
    ;;
  -s | --skip-bases)
    USE_BASES=false
    shift 2
    ;;
  *)
    echo "Unkown option ${1}"
    exit 1
    ;;
  esac
done

if [[ -z ${TOPO} ]]; then
  echo "A topology is needed" 1>&2
  exit 1
fi

OUT_DIR="$(realpath "${DIR}/../outputs/${TOPO}")"

if $USE_BASES; then
  for base in "${BASES[@]}"; do
    make run TOPO="$TOPO" CONTROLLER="ns3::${base}"

    mkdir -p "${OUT_DIR}/${base}"

    mv "${OUT_DIR}"/*.* "${OUT_DIR}/${base}"
  done
fi

for base in "${EXTERNAL_BASES[@]}"; do
  read -n 1 -s -r -p "Starting external base ${base} [Continue]"
  echo

  make run TOPO="$TOPO" CONTROLLER="External"

  mkdir -p "${OUT_DIR}/${base}"

  mv "${OUT_DIR}"/*.* "${OUT_DIR}/${base}"
done
