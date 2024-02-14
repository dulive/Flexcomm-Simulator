#!/usr/bin/env python3

import argparse
import json
import csv


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
        data = csv.reader(trace, delimiter=";")

        for row in data:
            node = row[1]
            if row[0] == "0.0":
                values[node] = []
                last_power_drawn[node] = 0
            elif (float(row[0]) % 900) == 899:
                power_drawn = float(row[3])
                values[node].append(float(power_drawn - last_power_drawn[node]))
                last_power_drawn[node] = power_drawn

    with open(args.output, "w") as out:
        json.dump(values, out, indent=4)


if __name__ == "__main__":
    main()
