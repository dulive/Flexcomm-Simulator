#!/bin/bash

DIR="$(dirname "$(realpath "$0")")/"
FLEXES="flex_files"
PRODS="prod_files"
NORMALIZER="${DIR}/normalize_ren_prod.py"
BASES=("ReactiveLoadController1" "ReactiveLoadController2" "ReactiveLoadController3" "SimpleController")

while [[ ${#} -gt 0 ]]; do
  case ${1} in
  -t | --topology)
    TOPO=${2}
    shift 2
    ;;
  -r | --ren-prods)
    REN_PRODS+=("$(realpath "${2}")")
    shift 2
    ;;
  -s | --start)
    START=${2}
    shift 2
    ;;
  -e | --end)
    END=${2}
    shift 2
    ;;
  -f | --flex-dir)
    FLEXES="${2}"
    shift 2
    ;;
  -p | --prod-dir)
    PRODS="${2}"
    shift 2
    ;;
  -n | --normalizer)
    NORMALIZER="$(realpath "${2}")"
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

if [[ -z ${TOPO} ]]; then
  echo "a topology is needed" 1>&2
  exit 1
fi

FLEX_DIR="$(realpath "${DIR}/../topologies/${TOPO}/${FLEXES}")"
PROD_DIR="$(realpath "${DIR}/../topologies/${TOPO}/${PRODS}")"
ESTI_DIR="$(realpath "${DIR}/../topologies/${TOPO}/estimate_files")"

recursive_ren_norm() {
  if [[ -d "${1}" ]]; then
    for prod in "${1}"/*; do
      recursive_ren_norm "${prod}"
    done
  elif [[ -f "${1}" ]]; then
    for alg in "${BASES[@]}"; do
      python3 "${NORMALIZER}" -a "${ESTI_DIR}/${alg}_estimate.json" -p "${1}" -o "${alg}_based" -s "${START}" -e "${END}"
    done
  else
    echo "Invalid production ${1}" 1>&2
  fi
}

recursive_ren_flex() {
  if [[ -d "${1}" ]]; then
    dir_name="$(basename "${1}")"
    mkdir -p "${dir_name}"
    pushd "${dir_name}" >/dev/null || exit
    for prod in "${1}"/*; do
      recursive_ren_flex "${prod}" "${2}"
    done
    popd >/dev/null || exit
  elif [[ -f "${1}" ]]; then
    file_name="$(basename "${1}")"
    flex_name="${file_name%%_prod.json}"
    python3 "${DIR}/gen_flex.py" "${1}" "${ESTI_DIR}/${2}_estimate.json" -o "${flex_name}_flex.json"
  else
    echo "Invalid production ${1}" 1>&2
  fi
}

ren_flex() {
  mkdir -p "${1}_estimate"
  pushd "${1}_estimate" >/dev/null || exit
  for prod in "${PROD_DIR}"/*; do
    recursive_ren_flex "${prod}" "${1}"
  done
  popd >/dev/null || exit
}

mkdir -p "${PROD_DIR}"

if [[ -n ${REN_PRODS[*]} ]]; then
  (
    cd "${PROD_DIR}" || exit
    for prod in "${REN_PRODS[@]}"; do
      recursive_ren_norm "${prod}"
    done
  )
fi

mkdir -p "${FLEX_DIR}"

(
  cd "${FLEX_DIR}" || exit
  for base in "${BASES[@]:3}"; do
    mkdir -p "${base}_reactive"
    pushd "${base}_reactive" >/dev/null || exit
    for reactive in "${BASES[@]:0:3}"; do
      python3 "${DIR}/gen_flex.py" "${ESTI_DIR}/${reactive}_estimate.json" "${ESTI_DIR}/${base}_estimate.json" -o "${alg}_flex.json"
    done
    popd >/dev/null || exit
  done

  mkdir -p "ren_normalized"
  cd "ren_normalized" || exit
  for alg in "${BASES[@]}"; do
    ren_flex "${alg}"
  done
)
