#!/usr/bin/env python3

import argparse
from operator import attrgetter
from statistics import quantiles
import os
import re
import tomli_w
from xml.dom import minidom
from decimal import Decimal
from random import choices, randrange

# maybe do this relatively to timestep
FLOW_DURATION = [[120, 240], [30, 120], [5, 30]]
FLOW_DURATION_WEIGHTS = [5, 25, 70]

FLOW_RATES_WEIGHTS = [35, 65]


class App:
    def __init__(self, value, init, end) -> None:
        self.value = value
        self.init = init
        self.end = end

    def __str__(self):
        return f"({self.value}, {self.init}, {self.end})"

    def __repr__(self):
        return self.__str__()

    def __add__(self, other):
        return self.value + other

    def __radd__(self, other):
        return other + self.value

    def __sub__(self, other):
        return self.value - other

    def __rsub__(self, other):
        return other - self.value


def equal_value(lapps):
    for app in lapps:
        app.end += timestep


def more_value(id, lapps, value):
    equal_value(lapps)
    apps[id]["apps"].append(App(value, current_time, current_time + timestep))


def less_value(id, lapps, total, value):
    while total > value:
        total -= lapps[0]
        lapps = lapps[1:]

    if total == value:
        equal_value(lapps)
    else:
        more_value(id, lapps, value - total)


def pre_parse_xml(xml):
    for demand in xml.getElementsByTagName("demand"):
        id = demand.getAttribute("id")

        # reads Mbps, transforms to bps
        value = (
            Decimal(demand.getElementsByTagName("demandValue")[0].firstChild.nodeValue)
            * 1000000
            / ratio
        )

        if id in apps:
            lapps = [app for app in apps[id]["apps"] if app.end == current_time]

            total = sum(lapps)
            if total == value:
                equal_value(lapps)

            elif total < value:
                more_value(id, lapps, value - total)

            else:  # total > value
                less_value(id, lapps, total, value)

        else:
            source = demand.getElementsByTagName("source")[0].firstChild.nodeValue
            target = demand.getElementsByTagName("target")[0].firstChild.nodeValue
            apps[id] = {
                "source": source,
                "target": target,
                "apps": [App(value, current_time, current_time + timestep)],
            }


def gen_times(start_time, stop_time):
    if start_time is None:
        start = 0
        while start <= 30:
            start = current_time - randrange(timestep)
    else:
        start = start_time

    if stop_time is None:
        time_interval = choices(FLOW_DURATION, cum_weights=FLOW_DURATION_WEIGHTS).pop()
        duration = randrange(time_interval[0], time_interval[1] + 1) * 60
        stop = start + duration
    else:
        stop = stop_time

    return start, stop


def random_more_value(id, total_bps, start_time, stop_time):
    if total_bps < FLOW_RATES[1][1]:
        start, stop = gen_times(start_time, stop_time)
        apps[id]["apps"].append(App(total_bps, start, stop))

    else:
        range_bps = choices(FLOW_RATES, cum_weights=FLOW_RATES_WEIGHTS).pop()

        if total_bps < range_bps[1]:
            start, stop = gen_times(start_time, stop_time)
            apps[id]["apps"].append(App(total_bps, start, stop))

        else:
            rest = 0
            bps = 0
            while rest < FLOW_RATES[1][0]:
                bps = randrange(range_bps[0], range_bps[1] + 1)
                rest = total_bps - bps

            start, stop = gen_times(start_time, stop_time)
            apps[id]["apps"].append(App(bps, start, stop))

            random_more_value(id, rest, start_time, stop_time)


def fix_app_end(app):
    prev_time = current_time - timestep
    lower_limit = app.init + 1 if app.init > prev_time else prev_time + 1
    app.end = randrange(lower_limit, current_time)
    assert app.init < app.end


def equal_value_app(lapps, total, value):
    diff = total - value
    for app in lapps:
        if app.value == diff:
            total -= app
            fix_app_end(app)
            return True

    return False


def random_less_value(id, lapps, total, value, start_time, stop_time):
    while total > value:
        if equal_value_app(lapps, total, value):
            break

        removed = False
        for index in range(len(lapps)):
            app = lapps[index]
            new_total = total - app
            if new_total > value or (value - new_total) >= FLOW_RATES[1][0]:
                fix_app_end(app)
                lapps.pop(index)
                total = new_total
                removed = True
                break

        if not removed:
            oldest_app = lapps.pop(0)
            fix_app_end(oldest_app)
            total -= oldest_app

    if total < value:
        while lapps and (value - total) < FLOW_RATES[1][0]:
            oldest_app = lapps.pop(0)
            fix_app_end(oldest_app)
            total -= oldest_app

        random_more_value(id, value - total, start_time, stop_time)


def parse_xml(file, start_time=None, stop_time=None):
    xml = minidom.parse(file)
    for demand in xml.getElementsByTagName("demand"):
        id = demand.getAttribute("id")

        # reads Mbps, transforms to bps
        value = (
            Decimal(demand.getElementsByTagName("demandValue")[0].firstChild.nodeValue)
            * 1000000
            / ratio
        )

        lapps = [
            app
            for app in apps[id]["apps"]
            if app.init <= current_time and app.end > current_time
        ]

        total = sum(lapps)
        if total < value:
            while lapps and (value - total) < FLOW_RATES[1][0]:
                oldest_app = lapps.pop(0)
                fix_app_end(oldest_app)
                total -= oldest_app

            random_more_value(id, value - total, start_time, stop_time)

        elif total > value:
            random_less_value(id, lapps, total, value, start_time, stop_time)

        if id not in prev_value:
            prev_value[id] = []

        prev_value[id].append((value, current_time))
        for val, time in prev_value[id]:
            total = sum(
                [app for app in apps[id]["apps"] if app.init <= time and app.end > time]
            )
            assert (
                total == val
            ), f"{id}, {time}, {val}, {total}, {current_time}, {value}, {apps[id]['apps']}, {prev_value[id]}"


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--demands", type=str, required=True)
    parser.add_argument("-m", "--max", type=int, required=False)
    parser.add_argument("-t", "--timestep", type=int, required=False)
    parser.add_argument("-r", "--ratio", type=int, required=False, default=1)
    parser.add_argument("-s", "--start", type=int, required=False)
    parser.add_argument("-e", "--end", type=int, required=False)

    return parser.parse_args()


def main():
    args = parse_args()

    global apps, current_time, timestep, FLOW_RATES, MEDIAN, ratio, prev_value

    prev_value = {}
    apps = {}
    current_time = 0
    ratio = args.ratio

    files = sorted(os.listdir(args.demands))[: args.max]

    if not args.timestep:
        xml = minidom.parse(f"{args.demands}/{files[0]}")

        res = re.match(
            r"([0-9]+)([a-z]+)",
            xml.getElementsByTagName("granularity")[0].firstChild.nodeValue,
        )

        if res:  # always true
            time, unit = res.groups()
        else:
            exit(-1)

        if unit == "day":
            timestep = 86400  # 24 * 60 * 60

        elif unit == "month":
            timestep = 2592000  # 60 * 60 * 24 * 30

        else:
            timestep = int(time) * 60

    else:
        timestep = args.timestep * 60

    for file in files:
        xml = minidom.parse(f"{args.demands}/{file}")
        pre_parse_xml(xml)
        current_time += timestep

    data_rate_values = []
    for lapps in apps.values():
        data_rate_values.extend(map(attrgetter("value"), lapps["apps"]))
        lapps["apps"] = []

    sorted_values = sorted(data_rate_values)

    quart = quantiles(sorted_values, n=4)
    iqr = quart[2] - quart[0]

    MEDIAN = quart[1]
    FLOW_RATES = [
        [int(quart[2]), int(quart[2] + (Decimal(1.5) * iqr))],
        [int(quart[0]), int(quart[2])],
    ]

    current_time = 30
    parse_xml(f"{args.demands}/{files[0]}", start_time=30)
    current_time = timestep
    for file in files[1:-1]:
        parse_xml(f"{args.demands}/{file}")
        current_time += timestep
    parse_xml(f"{args.demands}/{files[-1]}", stop_time=86400)

    toml_apps = dict()
    counter = 0
    for appGroup in apps.values():
        for app in appGroup["apps"]:
            if (
                int(app.value) == 0
                or (args.start is not None and app.end <= args.start)
                or (args.end is not None and app.init >= args.end)
            ):
                continue

            startTime = app.init
            stopTime = app.end
            if args.start is not None:
                startTime -= args.start
                stopTime -= args.start
                if startTime < 30:
                    startTime = 30

            toml_apps[f"app{counter}"] = {
                "type": "constSend",
                "host": f"{appGroup['source']}Host",
                "remote": f"{appGroup['target']}Host",
                "startTime": f"{startTime}s",
                "stopTime": f"{stopTime}s",
                "dataRate": f"{int(app.value)}bps",
            }
            counter += 1

    sorted_toml_apps = {
        k: v
        for k, v in sorted(
            toml_apps.items(), key=lambda item: int(item[1]["startTime"][:-1])
        )
    }

    print(f"Total apps: {len(sorted_toml_apps)}")
    with open("applications.toml", "wb") as output:
        tomli_w.dump(sorted_toml_apps, output)


if __name__ == "__main__":
    main()
