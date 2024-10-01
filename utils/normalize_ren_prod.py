import argparse
import pandas as pd
import json
from pathlib import Path
from itertools import permutations, product
from statistics import quantiles


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-a", "--accumulated", type=str, required=True)
    parser.add_argument("-p", "--production", type=str, required=True)
    parser.add_argument("-o", "--out-dir", type=str, required=False, default="")
    parser.add_argument("-s", "--start", type=int, required=False, default=0)
    parser.add_argument("-e", "--end", type=int, required=False, default=96)

    return parser.parse_args()


def normalize(values, a, b, reduced=False):
    if reduced:
        parts = [4, 10, 100]
        for n in parts:
            x_min, *_, x_max = quantiles(values, n=n)
            if x_min != x_max:
                break
    else:
        x_max = max(values)
        x_min = min(values)
    return [a + ((x - x_min) * (b - a)) / (x_max - x_min) for x in values]


def calculate_range(values):
    return min(map(min, values)), max(map(max, values))


def load_ren_prod(prod):
    ren_data = {}
    with open(prod, "r") as csv_file:
        data = pd.read_csv(csv_file, sep=";", skiprows=2)
        for _, row in data.iterrows():
            date = row["Data e Hora"].split(" ")[0]

            if date not in ren_data:
                ren_data[date] = {
                    "hydro": [],
                    "eolic": [],
                    "solar": [],
                    "hydro_eolic": [],
                    "hydro_solar": [],
                    "eolic_solar": [],
                    "hydro_eolic_solar": [],
                    "renewable": [],
                }

            ren_data[date]["hydro"].append(row["Hídrica"])
            ren_data[date]["eolic"].append(row["Eólica"])
            ren_data[date]["solar"].append(row["Solar"])
            ren_data[date]["hydro_eolic"].append(row["Hídrica"] + row["Eólica"])
            ren_data[date]["hydro_solar"].append(row["Hídrica"] + row["Solar"])
            ren_data[date]["eolic_solar"].append(row["Eólica"] + row["Solar"])
            ren_data[date]["hydro_eolic_solar"].append(
                row["Hídrica"] + row["Eólica"] + row["Solar"]
            )
            ren_data[date]["renewable"].append(
                row["Hídrica"]
                + row["Eólica"]
                + row["Solar"]
                + row["Biomassa"]
                + row["Ondas"]
            )

    return ren_data


def disjointed_prod(
    prod_dict, switches, ranges, start, end, out_dir, pre_range=True, reduced=False
):

    for prod_type, prod_vals in prod_dict.items():
        if pre_range:
            prod_vals = prod_vals[start:end]

        global_prod = {
            sw: normalize(prod_vals, *ranges["global"], reduced=reduced)
            for sw in switches
        }
        switch_prod = {
            sw: normalize(prod_vals, *ranges["per_switch"][sw], reduced=reduced)
            for sw in switches
        }

        if not pre_range:
            global_prod = {sw: values[start:end] for sw, values in global_prod.items()}
            switch_prod = {sw: values[start:end] for sw, values in switch_prod.items()}

        with open(f"{out_dir}/{prod_type}_global_prod.json", "w") as flex_file:
            json.dump(global_prod, flex_file, indent=4)

        with open(f"{out_dir}/{prod_type}_per_device_prod.json", "w") as flex_file:
            json.dump(switch_prod, flex_file, indent=4)


def grouped_prod(
    prod_dict,
    ranges,
    groups,
    start,
    end,
    out_dir,
    pre_range=True,
    reduced=False,
    range_selection=None,
):
    energies = ("hydro", "solar", "eolic")
    production = {}

    for index, energy in enumerate(energies):
        prod_vals = prod_dict[energy]

        if pre_range:
            prod_vals = prod_vals[start:end]

        production |= {
            sw: normalize(
                prod_vals,
                *(
                    ranges["global"]
                    if range_selection == "global"
                    else (
                        ranges["per_switch"][sw]
                        if range_selection == "per_switch"
                        else ranges[energy]
                    )
                ),
                reduced=reduced,
            )
            for sw in groups[index]
        }

    if not pre_range:
        production = {sw: values[start:end] for sw, values in production.items()}

    filename = "grouped"
    if range_selection == "global":
        filename += "_global"
    elif range_selection == "per_switch":
        filename += "_per_device"

    with open(f"{out_dir}/{filename}_prod.json", "w") as flex_file:
        json.dump(
            production,
            flex_file,
            indent=4,
        )


def main():
    args = parse_args()

    with open(args.accumulated) as acc_file:
        accumulated = json.load(acc_file)

    sw_sorted = sorted(accumulated)
    n_elems = round(len(sw_sorted) / 3)

    ranges = {
        "global": calculate_range(accumulated.values()),
        "per_switch": {sw: (min(acc), max(acc)) for sw, acc in accumulated.items()},
    }

    switch_groups = [
        sw_sorted[:n_elems],
        sw_sorted[n_elems : n_elems * 2],
        sw_sorted[n_elems * 2 :],
    ]

    for date, prod_dict in load_ren_prod(args.production).items():

        for pre_range, reduced in product((True, False), repeat=2):
            out_dir = f"{args.out_dir}/{date}/"
            if pre_range:
                out_dir += "pre_range"
            else:
                out_dir += "pos_range"

            if reduced:
                out_dir += "_reduced"

            Path(out_dir).mkdir(exist_ok=True, parents=True)

            disjointed_prod(
                prod_dict,
                sw_sorted,
                ranges,
                args.start,
                args.end,
                out_dir,
                pre_range=pre_range,
                reduced=reduced,
            )

            for index, groups in enumerate(permutations(switch_groups)):
                grouped_dir = out_dir + f"/grouped_permutation{index}"
                Path(grouped_dir).mkdir(exist_ok=True, parents=True)

                ranges["hydro"] = calculate_range([accumulated[sw] for sw in groups[0]])
                ranges["solar"] = calculate_range([accumulated[sw] for sw in groups[1]])
                ranges["eolic"] = calculate_range([accumulated[sw] for sw in groups[2]])

                grouped_prod(
                    prod_dict,
                    ranges,
                    groups,
                    args.start,
                    args.end,
                    grouped_dir,
                    pre_range=pre_range,
                    reduced=reduced,
                )
                grouped_prod(
                    prod_dict,
                    ranges,
                    groups,
                    args.start,
                    args.end,
                    grouped_dir,
                    range_selection="global",
                    pre_range=pre_range,
                    reduced=reduced,
                )
                grouped_prod(
                    prod_dict,
                    ranges,
                    groups,
                    args.start,
                    args.end,
                    grouped_dir,
                    range_selection="per_switch",
                    pre_range=pre_range,
                    reduced=reduced,
                )


if __name__ == "__main__":
    main()
