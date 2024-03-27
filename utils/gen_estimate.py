#!/usr/bin/env python3

import argparse
import json
import pandas as pd


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=str)
    parser.add_argument(
        "-o", "--output", type=str, required=False, default="estimate.json"
    )
    return parser.parse_args()


def main():
    args = parse_args()
    values = {}
    last_power_drawn = {}

    with open(args.trace, "r") as trace:
        data = pd.read_csv(trace, sep=";")

        for _, row in data.iterrows():
            if row["Time"] == 0:
                values[row["NodeName"]] = []
                last_power_drawn[row["NodeName"]] = 0
            elif (row["Time"] % 900) == 899:
                values[row["NodeName"]].append(
                    row["PowerDrawn"] - last_power_drawn[row["NodeName"]]
                )
                last_power_drawn[row["NodeName"]] = row["PowerDrawn"]

    with open(args.output, "w") as out:
        json.dump(values, out, indent=4)


if __name__ == "__main__":
    main()
