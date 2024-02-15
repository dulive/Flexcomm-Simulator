#!/usr/bin/env python3

import argparse
import json


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("acc_file1", type=str)
    parser.add_argument("acc_file2", type=str)
    parser.add_argument("-o", "--output", type=str, required=False, default="flex.json")
    return parser.parse_args()


def main():
    args = parse_args()
    zones = {}

    with open(args.acc_file1) as acc_file1, open(args.acc_file2) as acc_file2:
        acc_dict1 = json.load(acc_file1)
        acc_dict2 = json.load(acc_file2)

        for sw in acc_dict1.keys():
            zones[sw] = []

            for acc1, acc2 in zip(acc_dict1[sw], acc_dict2[sw]):
                zones[sw].append(acc1 - acc2)

    with open(args.output, "w") as flex_file:
        json.dump(zones, flex_file, indent=4)


if __name__ == "__main__":
    main()
