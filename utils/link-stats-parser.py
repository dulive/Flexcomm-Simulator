#!/usr/bin/env python3

import matplotlib.pylab as plt
import argparse
import csv

from tabulate import tabulate
from sortedcontainers import SortedSet
from common import colors, markers


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("file", type=str)
    parser.add_argument("--plot", "-p", dest="drawPlot", action="store_true")
    parser.set_defaults(drawPlot=False)
    return parser.parse_args()


def report_values(y_usage, y_load):
    table = []
    for link, usages in y_usage.items():
        table.append([link, max(usages), sum(usages) / len(usages), max(y_load[link])])

    print(
        tabulate(
            table,
            headers=["Link", "Max Usage (%)", "Average Usage (%)", "Max Load (bps)"],
            tablefmt="pretty",
        )
    )


def draw_plot(title, x, y, ylabel):
    plt.figure(title)
    color_index = 0
    for link, usages in y.items():
        plt.plot(
            x,
            usages,
            color=colors[color_index % len(colors)],
            marker=markers[color_index % len(markers)],
            linestyle="none",
            label=link,
        )
        color_index += 1

    plt.legend()
    plt.xlabel("Time Interval (s)")
    plt.ylabel(ylabel)


def read_file(file_path, drawPlot):
    with open(file_path) as file:
        data = csv.reader(file, delimiter=";")

        x = SortedSet()
        y_usage = {}
        y_load = {}
        for row in data:
            time = float(row[0])
            link = row[1]
            utilization = float(row[2])
            load = float(row[3])

            x.add(time)
            if link not in y_usage:
                y_usage[link] = [utilization]
                y_load[link] = [load]
            else:
                y_usage[link].append(utilization)
                y_load[link].append(load)

        report_values(y_usage, y_load)
        if drawPlot:
            draw_plot("Usage over time", x, y_usage, "Usage (%)")
            draw_plot("Load over time", x, y_load, "Load (bps)")
            plt.show()


if __name__ == "__main__":
    args = parse_args()
    read_file(args.file, args.drawPlot)
