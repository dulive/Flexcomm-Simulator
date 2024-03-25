#!/usr/bin/env python3

import matplotlib.pylab as plt
import argparse
import csv

from sortedcontainers import SortedSet
from tabulate import tabulate
from common import colors, markers


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("file", type=str)
    parser.add_argument("--plot", "-p", dest="drawPlot", action="store_true")
    parser.set_defaults(drawPlot=False)
    return parser.parse_args()


def report_values(consumptions, power_drawn):
    table = []
    for node in consumptions:
        table.append([node, max(consumptions[node]), power_drawn[node][-1]])
    print(
        tabulate(
            table,
            headers=["Nodes", "Max Consumption (W)", "Power Drawn (Wh)"],
            tablefmt="pretty",
        )
    )
    print(
        "Total Max Consumption: {} kW".format(
            max(map(sum, zip(*consumptions.values()))) / 1000
        )
    )
    print(
        "Total Power Drawn: {} kWh".format(
            sum(p[-1] for p in power_drawn.values()) / 1000
        )
    )


def draw_plots(title, x, y, ylabel):
    plt.figure(title)
    color_index = 0

    for node in y:
        plt.plot(
            x,
            y[node],
            label=node,
            color=colors[color_index % len(colors)],
            marker=markers[color_index % len(markers)],
            linestyle="none",
        )
        color_index += 1

    aggregated = []
    for i in range(len(x)):
        aggregated.append(0.0)
        for node in y:
            aggregated[i] += y[node][i]

    plt.plot(
        x,
        aggregated,
        "ro",
        label="Aggregated",
    )

    plt.legend()
    plt.xlabel("Time Interval (s)")
    plt.ylabel(ylabel)


def read_file(file_path, drawPlot):
    with open(file_path) as file:
        data = csv.reader(file, delimiter=";")
        data = list(data)[1:]

        # Parse File
        x = SortedSet()
        y_consumption = {}
        y_power_drawn = {}
        for row in data:
            time = float(row[0])
            node = row[1]
            conso = float(row[2])
            power_drawn = float(row[3])
            x.add(time)
            if node not in y_consumption:
                y_consumption[node] = [conso]
                y_power_drawn[node] = [power_drawn]
            else:
                y_consumption[node].append(conso)
                y_power_drawn[node].append(power_drawn)

        report_values(y_consumption, y_power_drawn)
        if drawPlot:
            draw_plots("Consumption over time", x, y_consumption, "Consumption (W)")
            draw_plots("Power drawn over time", x, y_power_drawn, "Power drawn (Wh)")
            plt.show()


if __name__ == "__main__":
    args = parse_args()
    read_file(args.file, args.drawPlot)
