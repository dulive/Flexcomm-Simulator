#!/usr/bin/env python3

import argparse
import math
import os
import sys
import warnings

import matplotlib.gridspec as gridspec
import matplotlib.patches as patches
import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.lines import Line2D
from matplotlib.ticker import AutoMinorLocator, MaxNLocator

BLUE = "#3D85F7"
BLUE_LIGHT = "#5490FF"
PINK = "#C32E5A"
PINK_LIGHT = "#D34068"
BACKGROUND = "#F5F4EF"
GREY10 = "#1a1a1a"
GREY20 = "#333333"
GREY25 = "#404040"
GREY30 = "#4d4d4d"
GREY40 = "#666666"
GREY50 = "#7f7f7f"
GREY60 = "#999999"
GREY75 = "#bfbfbf"
GREY91 = "#e8e8e8"
GREY98 = "#fafafa"

# TODO: make this work asap


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-t", "--topology", type=str, required=True)
    parser.add_argument("-f", "--flexibility", type=str, required=True)
    parser.add_argument("-e", "--estimate", type=str, required=True)
    parser.add_argument(
        "-a", "--algorithms", type=str, action="extend", nargs="*", default=[]
    )
    parser.add_argument(
        "-b", "--bases", type=str, action="extend", nargs="*", default=[]
    )
    parser.add_argument(
        "-i", "--input-dir", type=str, required=False, default="outputs"
    )
    parser.add_argument(
        "-o", "--output-dir", type=str, required=False, default="graphs"
    )
    parser.add_argument(
        "-d", "--topologies-dir", type=str, required=False, default="topologies"
    )

    return parser.parse_args()


def power_drawn_multi_lines(values, output_dir):
    def plot(alg_values, i, ax_single, colors, sub_plots):
        node = alg_values[1]["Node"].unique()[i]
        data = alg_values[1][alg_values[1]["Node"] == node]
        ax_single.plot(data["Time"], data["Available"], color=colors(i), alpha=0.2)
        ax_single.plot(data["Time"], data["Power Drawn"], color=colors(i))

        ax_single.spines[["right", "top", "left", "bottom"]].set_visible(False)

        ax_single.set_facecolor(BACKGROUND)

        ax_single.set_xlim(sub_plots[i][1][0].get_xlim())
        ax_single.set_ylim(sub_plots[i][1][0].get_ylim())

        ## TODO: check this
        # ax_single.set_yticks([])
        ax_single.set_xticks([])

        ax_single.set_title(f"{node}", fontsize=12, fontweight="bold")

    num_subplots = len(values)
    nodes = len(next(iter(values.values()))[1]["Node"].unique())
    colors = plt.get_cmap("tab20", nodes)

    power_drawn_dir = os.path.join(output_dir, "power_drawn_multi_lines")
    os.makedirs(power_drawn_dir, exist_ok=True)

    sub_plots = [
        (fig, axs.flatten())
        for fig, axs in (
            plt.subplots(
                num_subplots,
                1,
                figsize=(6, 8),
                sharey=True,
                sharex=True,
            )
            for _ in range(nodes)
        )
    ]

    for i, (_, alg_values) in enumerate(values.items()):
        node_values = alg_values[1]
        for node, (_, axs) in zip(node_values["Node"].unique(), sub_plots):
            data = node_values[node_values["Node"] == node]
            ax = axs[i]
            ax.plot(data["Time"], data["Available"])
            ax.plot(data["Time"], data["Power Drawn"])

    plt.tight_layout()

    n_cols = math.ceil(math.sqrt(nodes))
    n_rows = math.ceil(nodes / n_cols)

    for alg, alg_values in values.items():
        fig_single = plt.figure(figsize=(n_cols * 4, n_rows * 4))

        gs = gridspec.GridSpec(n_rows, n_cols, figure=fig_single)
        for i in range(nodes - (nodes % n_cols)):
            row = i // n_cols
            col = i % n_cols
            ax_single = fig_single.add_subplot(gs[row, col])
            plot(alg_values, i, ax_single, colors, sub_plots)

        if nodes % n_cols != 0:
            last_row_plots = nodes % n_cols

            gs_last_row = gridspec.GridSpecFromSubplotSpec(
                1, last_row_plots, subplot_spec=gs[n_rows - 1, :]
            )

            for i in range(last_row_plots):
                ax_single = fig_single.add_subplot(gs_last_row[0, i])
                plot(alg_values, i, ax_single, colors, sub_plots)

        fig_single.set_facecolor(BACKGROUND)
        fig_single.suptitle(
            f"Energy Consumed Compared to Energy Available\n{alg}",
            fontsize=20,
            fontweight="bold",
            x=0.05,
            ha="left",
        )

        file_path = os.path.join(power_drawn_dir, f"{alg}.pdf")
        fig_single.savefig(file_path)
        plt.close(fig_single)

    for fig, _ in sub_plots:
        plt.close(fig)


def difference_multi_lines(values, output_dir):
    def plot(alg_values, i, ax_single, colors, ax):
        node = alg_values[1]["Node"].unique()[i]
        data = alg_values[1][alg_values[1]["Node"] == node]
        ax_single.plot(data["Time"], data["Difference"], color=colors(i))

        other_nodes = alg_values[1]["Node"].unique()[
            alg_values[1]["Node"].unique() != node
        ]
        for other_node in other_nodes:
            data = alg_values[1][alg_values[1]["Node"] == other_node]

            ax_single.plot(data["Time"], data["Difference"], color=colors(i), alpha=0.2)

        ax_single.spines[["right", "top", "left", "bottom"]].set_visible(False)

        ax_single.set_facecolor(BACKGROUND)

        ax_single.set_xlim(ax.get_xlim())
        ax_single.set_ylim(ax.get_ylim())

        ## TODO: check this
        ax_single.set_yticks([])
        ax_single.set_xticks([])

        ax_single.set_title(f"{node}", fontsize=12, fontweight="bold")

    num_subplots = len(values)
    nodes = len(next(iter(values.values()))[1]["Node"].unique())
    colors = plt.get_cmap("tab20", nodes)

    difference_dir = os.path.join(output_dir, "difference_multi_lines")
    os.makedirs(difference_dir, exist_ok=True)

    fig, axs = plt.subplots(num_subplots, 1, figsize=(6, 8), sharey=True, sharex=True)

    for (_, alg_values), ax in zip(values.items(), axs):
        for node in alg_values[1]["Node"].unique():
            data = alg_values[1][alg_values[1]["Node"] == node]
            ax.plot(data["Time"], data["Difference"])

    plt.tight_layout()

    n_cols = math.ceil(math.sqrt(nodes))
    n_rows = math.ceil(nodes / n_cols)

    for alg, alg_values in values.items():
        fig_single = plt.figure(figsize=(n_cols * 3, n_rows * 3))

        gs = gridspec.GridSpec(n_rows, n_cols, figure=fig_single)

        for i in range(nodes - (nodes % n_cols)):
            row = i // n_cols
            col = i % n_cols
            ax_single = fig_single.add_subplot(gs[row, col])
            plot(alg_values, i, ax_single, colors, axs[0])

        if nodes % n_cols != 0:
            last_row_plots = nodes % n_cols

            gs_last_row = gridspec.GridSpecFromSubplotSpec(
                1, last_row_plots, subplot_spec=gs[n_rows - 1, :]
            )

            for i in range(last_row_plots):
                ax_single = fig_single.add_subplot(gs_last_row[0, i])
                plot(alg_values, i, ax_single, colors, axs[0])

        fig_single.set_facecolor(BACKGROUND)
        fig_single.suptitle(
            "Energy Consumed Compared to Energy Available",
            fontsize=20,
            fontweight="bold",
            x=0.05,
            ha="left",
        )

        file_path = os.path.join(difference_dir, f"{alg}.pdf")
        fig_single.savefig(file_path)
        plt.close(fig_single)

    plt.close(fig)


def difference_lines(values, output_dir):
    num_subplots = len(values)
    nodes = len(next(iter(values.values()))[1]["Node"].unique())
    colors = plt.get_cmap("tab20", nodes)

    difference_dir = os.path.join(output_dir, "difference_lines")
    os.makedirs(difference_dir, exist_ok=True)

    fig, axs = plt.subplots(num_subplots, 1, figsize=(6, 8), sharey=True, sharex=True)

    axs = axs.flatten()

    for (_, alg_values), ax in zip(values.items(), axs):
        for node in alg_values[1]["Node"].unique():
            data = alg_values[1][alg_values[1]["Node"] == node]
            ax.plot("Time", "Difference", data=data)

    plt.tight_layout()

    for alg, alg_values in values.items():
        fig_single, ax_single = plt.subplots(figsize=(12, 8))

        for i, node in enumerate(alg_values[1]["Node"].unique()):
            data = alg_values[1][alg_values[1]["Node"] == node]
            color = colors(i)
            ax_single.plot("Time", "Difference", color=color, lw=1.8, data=data)

        fig_single.patch.set_facecolor(GREY98)
        ax_single.set_facecolor(GREY98)

        ax_single.hlines(y=0, xmin=0, xmax=24, color=GREY60, lw=0.8)

        ax_single.set_ylim(axs[0].get_ylim())
        ax_single.set_xlim(axs[0].get_xlim())  # add + 3 to upper limit

        ax_single.spines[["left", "bottom"]].set_color(GREY91)
        ax_single.spines[["right", "top"]].set_color("none")

        file_path = os.path.join(difference_dir, f"{alg}.pdf")
        fig_single.savefig(file_path)
        plt.close(fig_single)

    plt.close(fig)


def difference_area(values, output_dir):
    num_subplots = len(values)

    difference_dir = os.path.join(output_dir, "difference_area")
    os.makedirs(difference_dir, exist_ok=True)

    fig, axs = plt.subplots(num_subplots, 1, figsize=(6, 8), sharey=True, sharex=True)

    axs = axs.flatten()

    for (_, alg_values), ax in zip(values.items(), axs):
        TIME = alg_values[0]["Time"].values[1:]
        POSITIVE_DIFFERENCE = alg_values[0]["Positive Difference"].values[1:]
        NEGATIVE_DIFFERENCE = alg_values[0]["Negative Difference"].values[1:]

        ax.plot(TIME, POSITIVE_DIFFERENCE)
        ax.plot(TIME, NEGATIVE_DIFFERENCE)

    plt.tight_layout()

    for alg, alg_values in values.items():
        fig_single, ax_single = plt.subplots(figsize=(6, 4))

        TIME = alg_values[0]["Time"].values[1:]
        POSITIVE_DIFFERENCE = alg_values[0]["Positive Difference"].values[1:]
        NEGATIVE_DIFFERENCE = alg_values[0]["Negative Difference"].values[1:]

        ax_single.plot(TIME, POSITIVE_DIFFERENCE, color=BLUE)
        ax_single.plot(TIME, NEGATIVE_DIFFERENCE, color=PINK)
        ax_single.fill_between(
            TIME,
            POSITIVE_DIFFERENCE,
            NEGATIVE_DIFFERENCE,
            where=(POSITIVE_DIFFERENCE > NEGATIVE_DIFFERENCE),
            interpolate=True,
            color=BLUE_LIGHT,
            alpha=0.3,
        )
        ax_single.fill_between(
            TIME,
            POSITIVE_DIFFERENCE,
            NEGATIVE_DIFFERENCE,
            where=(POSITIVE_DIFFERENCE <= NEGATIVE_DIFFERENCE),
            interpolate=True,
            color=PINK_LIGHT,
            alpha=0.3,
        )

        ax_single.set_facecolor(BACKGROUND)
        fig_single.set_facecolor(BACKGROUND)

        ax_single.set_xlim(axs[0].get_xlim())
        ax_single.set_ylim(axs[0].get_ylim())

        ax_single.xaxis.set_major_locator(MaxNLocator(integer=True))
        ax_single.xaxis.set_minor_locator(AutoMinorLocator())
        ax_single.yaxis.set_major_locator(MaxNLocator(integer=True))
        ax_single.yaxis.set_minor_locator(AutoMinorLocator())

        ax_single.minorticks_on()

        ax_single.tick_params(axis="both", which="major", labelcolor=GREY40)
        ax_single.tick_params(axis="both", which="both", length=0)

        ax_single.grid(which="minor", lw=0.4, alpha=0.4)
        ax_single.grid(which="major", lw=0.8, alpha=0.4)

        ax_single.spines[["right", "top", "left", "bottom"]].set_color("none")

        handles = [
            Line2D([], [], c=color, lw=1.2, label=label)
            for label, color in zip(["Positive", "Negative"], [BLUE, PINK])
        ]

        fig_single.legend(
            handles=handles,
            loc=(0.1, 0.01),
            ncol=2,
            columnspacing=1,
            handlelength=1.2,
            frameon=False,
        )

        positive = patches.Patch(
            facecolor=BLUE_LIGHT, alpha=0.3, label="energy surplus"
        )
        negative = patches.Patch(
            facecolor=PINK_LIGHT, alpha=0.3, label="energy shortage"
        )

        fig_single.legend(
            handles=[positive, negative],
            loc=(0.5, 0.01),
            ncol=2,
            columnspacing=1,
            handlelength=2,
            handleheight=2,
            frameon=False,
        )

        fig_single.text(
            x=0.05,
            y=0.975,
            s="Difference",
            color=GREY25,
            fontsize=26,
            fontweight="bold",
            ha="left",
            va="top",
            ma="left",
        )

        file_path = os.path.join(difference_dir, f"{alg}.pdf")
        fig_single.savefig(file_path)
        plt.close(fig_single)

    plt.close(fig)


def respecting_area(values, output_dir):
    num_subplots = len(values)

    respecting_dir = os.path.join(output_dir, "respecting_area")
    os.makedirs(respecting_dir, exist_ok=True)

    fig, axs = plt.subplots(num_subplots, 1, figsize=(6, 8), sharey=True, sharex=True)

    axs = axs.flatten()

    for (_, alg_values), ax in zip(values.items(), axs):
        TIME = alg_values[0]["Time"].values[1:]
        RESPECTING = alg_values[0]["Respecting"].values[1:]
        NOT_RESPECTING = alg_values[0]["Not Respecting"].values[1:]

        ax.plot(TIME, RESPECTING)
        ax.plot(TIME, NOT_RESPECTING)

    plt.tight_layout()

    for alg, alg_values in values.items():
        fig_single, ax_single = plt.subplots(figsize=(6, 4))

        TIME = alg_values[0]["Time"].values[1:]
        RESPECTING = alg_values[0]["Respecting"].values[1:]
        NOT_RESPECTING = alg_values[0]["Not Respecting"].values[1:]

        ax_single.plot(TIME, RESPECTING, color=BLUE)
        ax_single.plot(TIME, NOT_RESPECTING, color=PINK)
        ax_single.fill_between(
            TIME,
            RESPECTING,
            NOT_RESPECTING,
            where=(RESPECTING > NOT_RESPECTING),
            interpolate=True,
            color=BLUE_LIGHT,
            alpha=0.3,
        )
        ax_single.fill_between(
            TIME,
            RESPECTING,
            NOT_RESPECTING,
            where=(RESPECTING <= NOT_RESPECTING),
            interpolate=True,
            color=PINK_LIGHT,
            alpha=0.3,
        )

        ax_single.set_facecolor(BACKGROUND)
        fig_single.set_facecolor(BACKGROUND)

        ax_single.set_xlim(axs[0].get_xlim())
        ax_single.set_ylim(axs[0].get_ylim())

        ax_single.xaxis.set_major_locator(MaxNLocator(integer=True))
        ax_single.xaxis.set_minor_locator(AutoMinorLocator())
        ax_single.yaxis.set_major_locator(MaxNLocator(integer=True))
        ax_single.yaxis.set_minor_locator(AutoMinorLocator())

        ax_single.minorticks_on()

        ax_single.tick_params(axis="both", which="major", labelcolor=GREY40)
        ax_single.tick_params(axis="both", which="both", length=0)

        ax_single.grid(which="minor", lw=0.4, alpha=0.4)
        ax_single.grid(which="major", lw=0.8, alpha=0.4)

        ax_single.spines[["right", "top", "left", "bottom"]].set_color("none")

        handles = [
            Line2D([], [], c=color, lw=1.2, label=label)
            for label, color in zip(["Respecting", "Not Respecting"], [BLUE, PINK])
        ]

        fig_single.legend(
            handles=handles,
            loc=(0.1, 0.01),
            ncol=2,
            columnspacing=1,
            handlelength=1.2,
            frameon=False,
        )

        respecting = patches.Patch(
            facecolor=BLUE_LIGHT, alpha=0.3, label="more respecting"
        )
        not_respecting = patches.Patch(
            facecolor=PINK_LIGHT, alpha=0.3, label="more not respecting"
        )

        fig_single.legend(
            handles=[respecting, not_respecting],
            loc=(0.45, 0.01),
            ncol=2,
            columnspacing=1,
            handlelength=2,
            handleheight=2,
            frameon=False,
        )

        fig_single.text(
            x=0.05,
            y=0.975,
            s="Switches Respecting",
            color=GREY25,
            fontsize=26,
            fontweight="bold",
            ha="left",
            va="top",
            ma="left",
        )

        file_path = os.path.join(respecting_dir, f"{alg}.pdf")
        fig_single.savefig(file_path)
        plt.close(fig_single)

    plt.close(fig)


## TODO: check if this is needed
def power_drawn():
    pass


def parse_traces(alg_dir, estimate, flexibility):
    trace_path = os.path.join(alg_dir, "ecofen-trace.csv")
    with open(trace_path) as trace_file:
        trace = pd.read_csv(trace_file, sep=";")

    nodes = trace["NodeName"].unique()
    # NOTE: h * 60 = minutes in hours
    # 8 * 60 = 480
    # 24 * 60 = 1440
    global_values = {
        "Time": [x / 60 for x in range(480)],
        "Positive Difference": [0] * 480,
        "Negative Difference": [0] * 480,
        "Respecting": [0] * 480,
        "Not Respecting": [0] * 480,
    }
    per_switch_values = {
        "Time": [],
        "Node": [],
        "Available": [],
        "Power Drawn": [],
        "Difference": [],
    }

    last_acc = {sw: 0 for sw in nodes}
    last_acc_15 = {sw: 0 for sw in nodes}

    for _, row in trace.iterrows():
        time = row["Time"]
        if (time % 60) == 59:
            nodeName = row["NodeName"]
            index = time // 900
            max_acc = estimate[nodeName][index] + flexibility[nodeName][index]
            diff = (max_acc / 15) - (row["PowerDrawn"] - last_acc[nodeName])
            index = int(time // 60)
            if diff >= 0:
                global_values["Positive Difference"][index] += diff
                global_values["Respecting"][index] += 1
            else:
                global_values["Negative Difference"][index] -= diff
                global_values["Not Respecting"][index] += 1
            if (time % 900) == 899:
                power_drawn = row["PowerDrawn"] - last_acc_15[nodeName]
                per_switch_values["Time"].append((time // 900) / 4)
                per_switch_values["Node"].append(nodeName)
                per_switch_values["Available"].append(max_acc)
                per_switch_values["Power Drawn"].append(power_drawn)
                per_switch_values["Difference"].append(max_acc - power_drawn)
                last_acc_15[nodeName] = int(row["PowerDrawn"])
            last_acc[nodeName] = int(row["PowerDrawn"])

    return pd.DataFrame(global_values), pd.DataFrame.from_dict(per_switch_values)


def main():
    args = parse_args()

    if not os.path.isdir(args.input_dir):
        warnings.warn("Input directory does not exist")
        sys.exit(1)

    dir = os.path.dirname(os.path.realpath(__file__))

    topo_input_dir = os.path.realpath(
        os.path.join(dir, "..", args.input_dir, args.topology)
    )
    topo_dir = os.path.realpath(os.path.join(dir, "..", args.topologies_dir))
    esti_path = (
        os.path.join(topo_dir, args.topology, "estimate_files", args.estimate) + ".json"
    )
    flex_path = (
        os.path.join(topo_dir, args.topology, "flex_files", args.flexibility) + ".json"
    )

    if not os.path.exists(topo_input_dir):
        warnings.warn("Topology input does not exist")
        sys.exit(1)
    if not os.path.exists(esti_path):
        warnings.warn("Estimate does not exist")
        sys.exit(1)
    if not os.path.exists(flex_path):
        warnings.warn("Flexibility does not exist")
        sys.exit(1)

    values = dict()

    bases = list(
        set(os.listdir(topo_input_dir))
        & set(
            [
                "ReactiveLoadController1",
                "ReactiveLoadController2",
                "ReactiveLoadController3",
                "SimpleController",
            ]
            + args.bases
        )
    )
    with open(esti_path) as esti_file, open(flex_path) as flex_file:
        flexibility = pd.read_json(flex_file)
        estimate = pd.read_json(esti_file)

        for base in bases:
            base_dir = os.path.join(topo_input_dir, base)
            values[base] = parse_traces(base_dir, estimate, flexibility)

        algorithms = (
            list(set(os.listdir(topo_input_dir)) & set(args.algorithms))
            if args.algorithms
            else list(set(os.listdir(topo_input_dir)) - set(bases))
        )
        for alg in algorithms:
            alg_input_dir = os.path.join(
                topo_input_dir, alg, args.estimate, args.flexibility
            )

            if os.path.exists(alg_input_dir):
                values[alg] = parse_traces(alg_input_dir, estimate, flexibility)

    output_dir = os.path.realpath(
        os.path.join(
            dir, "..", args.output_dir, args.topology, args.estimate, args.flexibility
        )
    )
    respecting_area(values, output_dir)
    difference_area(values, output_dir)
    difference_lines(values, output_dir)
    power_drawn_multi_lines(values, output_dir)
    difference_multi_lines(values, output_dir)


if __name__ == "__main__":
    main()
