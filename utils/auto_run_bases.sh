#!/bin/bash

TOPO=$1
ALGS=("SimpleController" "ReactiveLoadController1" "ReactiveLoadController2" "ReactiveLoadController3")
DIR="$(dirname "$(realpath "$0")")/"
OUT_DIR="$(realpath "${DIR}/../outputs/${TOPO}")"

for alg in "${ALGS[@]}"; do
	make run TOPO="$TOPO" CONTROLLER="ns3::${alg}"

	mkdir -p "${OUT_DIR}/${alg}"

	mv "${OUT_DIR}"/* "${OUT_DIR}/${alg}"
done
