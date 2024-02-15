import argparse
import csv
import json
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-a", "--accumulated", type=str, required=True)
    parser.add_argument("-p", "--production", type=str, required=True)
    parser.add_argument("-o", "--out-dir", type=str, required=False, default="")

    return parser.parse_args()


def normalize(values, a, b):
    x_max = max(values)
    x_min = min(values)
    return [a + ((x - x_min) * (b - a)) / (x_max - x_min) for x in values]


def calculate_range(values):
    return min(map(min, values)), max(map(max, values))


def load_ren_prod(prod):
    ren_data = {}
    with open(prod, "r") as csv_file:
        reader = csv.reader(csv_file, delimiter=";")
        for date_hour, hydro, eolic, solar, bio, waves, *_ in list(reader)[3:]:
            date = date_hour.split(" ")[0]

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

            ren_data[date]["hydro"].append(float(hydro))
            ren_data[date]["eolic"].append(float(eolic))
            ren_data[date]["solar"].append(float(solar))
            ren_data[date]["hydro_eolic"].append(float(hydro) + float(eolic))
            ren_data[date]["hydro_solar"].append(float(hydro) + float(solar))
            ren_data[date]["eolic_solar"].append(float(eolic) + float(solar))
            ren_data[date]["hydro_eolic_solar"].append(
                float(hydro) + float(eolic) + float(solar)
            )
            ren_data[date]["renewable"].append(
                float(hydro) + float(eolic) + float(solar) + float(bio) + float(waves)
            )

    return ren_data


def disjointed_prod(prod_dict, switches, groups_ranges, out_dir):
    for prod_type, prod_vals in prod_dict.items():
        global_prod = {
            sw: normalize(list(prod_vals), *groups_ranges["global_range"])
            for sw in switches
        }
        switch_prod = {
            sw: normalize(list(prod_vals), *groups_ranges["switch_range"][sw])
            for sw in switches
        }

        with open(f"{out_dir}/{prod_type}_global_prod.json", "w") as flex_file:
            json.dump(global_prod, flex_file, indent=4)

        with open(f"{out_dir}/{prod_type}_per_device_prod.json", "w") as flex_file:
            json.dump(switch_prod, flex_file, indent=4)


def grouped_prod(
    prod_dict, filename, groups_ranges, global_range=False, switch_range=False
):
    hydro_prod = {
        sw: normalize(
            list(prod_dict["hydro"]),
            *(
                groups_ranges["global_range"]
                if global_range
                else (
                    groups_ranges["switch_range"][sw]
                    if switch_range
                    else groups_ranges["hydro_range"]
                )
            ),
        )
        for sw in groups_ranges["hydro_group"]
    }
    solar_prod = {
        sw: normalize(
            list(prod_dict["solar"]),
            *(
                groups_ranges["global_range"]
                if global_range
                else (
                    groups_ranges["switch_range"][sw]
                    if switch_range
                    else groups_ranges["solar_range"]
                )
            ),
        )
        for sw in groups_ranges["solar"]
    }
    eolic_prod = {
        sw: normalize(
            list(prod_dict["eolic"]),
            *(
                groups_ranges["global_range"]
                if global_range
                else (
                    groups_ranges["switch_range"][sw]
                    if switch_range
                    else groups_ranges["eolic_range"]
                )
            ),
        )
        for sw in groups_ranges["eolic_group"]
    }

    with open(filename, "w") as flex_file:
        json.dump(
            hydro_prod | solar_prod | eolic_prod,
            flex_file,
            indent=4,
        )


def main():
    args = parse_args()

    with open(args.accumulated) as acc_file:
        accumulated = json.load(acc_file)

    sw_sorted = sorted(accumulated)
    n_elems = round(len(sw_sorted) / 3)

    groups_ranges = {
        "global_range": calculate_range(accumulated.values()),
        "switch_range": {sw: (min(acc), max(acc)) for sw, acc in accumulated.items()},
        "hydro_group": sw_sorted[:n_elems],
        "solar_group": sw_sorted[n_elems : n_elems * 2],
        "eolic_group": sw_sorted[n_elems * 2 :],
    }
    groups_ranges["hydro_range"] = calculate_range(
        [accumulated[sw] for sw in groups_ranges["hydro_group"]]
    )
    groups_ranges["solar_range"] = calculate_range(
        [accumulated[sw] for sw in groups_ranges["solar_group"]]
    )
    groups_ranges["eolic_range"] = calculate_range(
        [accumulated[sw] for sw in groups_ranges["eolic_group"]]
    )

    for date, prod_dict in load_ren_prod(args.production).items():
        out_dir = f"{date}/{args.out_dir}"
        Path(out_dir).mkdir(exist_ok=True, parents=True)

        disjointed_prod(prod_dict, sw_sorted, groups_ranges, out_dir)

        grouped_prod(prod_dict, f"{out_dir}/grouped_prod.json", groups_ranges)
        grouped_prod(
            prod_dict,
            f"{out_dir}/grouped_global_prod.json",
            groups_ranges,
            global_range=True,
        )
        grouped_prod(
            prod_dict,
            f"{out_dir}/grouped_per_device_prod.json",
            groups_ranges,
            switch_range=True,
        )


if __name__ == "__main__":
    main()
