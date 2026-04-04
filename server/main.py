import argparse
import logging
import os
import sys
import time

import numpy as np
import pandas
# from BTinterface import BTInterface
from maze import Action, Maze
from score import ScoreboardServer, ScoreboardFake

logging.basicConfig(
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s", level=logging.INFO
)

log = logging.getLogger(__name__)

TEAM_NAME = os.getenv("TEAM_NAME", "Wednesday first team")
SERVER_URL = os.getenv("SERVER_URL", "http://carcar.ntuee.org/scoreboard")
MAZE_FILE = os.getenv("MAZE_FILE", "maze.csv")
BT_PORT = os.getenv("BT_PORT", "COM11")


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", help="0: treasure-hunting, 1: self-testing", type=str)
    parser.add_argument("--maze-file", default=MAZE_FILE, help="Maze file", type=str)
    parser.add_argument("--bt-port", default=BT_PORT, help="Bluetooth port", type=str)
    parser.add_argument(
        "--team-name", default=TEAM_NAME, help="Your team name", type=str
    )
    parser.add_argument("--server-url", default=SERVER_URL, help="Server URL", type=str)
    return parser.parse_args()


def main(mode: int, bt_port: str, team_name: str, server_url: str, maze_file: str):
    maze = Maze(maze_file)
    point = ScoreboardServer(team_name, server_url)
    # point = ScoreboardFake("your team name", "data/fakeUID.csv") # for local testing

    ### Bluetooth connection haven't been implemented yet, we will update ASAP ###
    # interface = BTInterface(port=bt_port)
    current_score = point.get_current_score() or 0
    start_time = time.time()
    current_node = None
    current_action = None
    visited_nodes = set()
    planned_actions = []

    if mode == "0":
        log.info("Mode 0: For treasure-hunting")
        maze_df = pandas.read_csv(maze_file)
        score_columns = [col for col in ("ND", "SD", "WD", "ED") if col in maze_df.columns]

        if not score_columns or "index" not in maze_df.columns:
            log.warning("Maze data does not contain score metadata.")
            return

        score_table = (
            maze_df[["index", *score_columns]]
            .assign(
                best_score=lambda df: df[score_columns].fillna(0).max(axis=1),
                total_score=lambda df: df[score_columns].fillna(0).sum(axis=1),
            )
            .sort_values(["best_score", "total_score", "index"], ascending=[False, False, True])
        )

        planned_actions = score_table.loc[score_table["best_score"] > 0, "index"].astype(int).tolist()
        visited_nodes.update(planned_actions)

        if planned_actions:
            current_node = planned_actions[0]
            elapsed = time.time() - start_time
            projected_score = int(score_table.loc[score_table["best_score"] > 0, "best_score"].sum())
            log.info("Planned treasure route: %s", planned_actions)
            log.info(
                "Tracking %d treasure nodes, projected score %d, elapsed %.2fs, current score %d",
                len(planned_actions),
                projected_score,
                elapsed,
                current_score,
            )
        else:
            log.info("No treasure nodes found in the maze metadata.")

    elif mode == "1":
        log.info("Mode 1: Self-testing mode.")
        maze_df = pandas.read_csv(maze_file)
        log.info(
            "Loaded maze file '%s' with %d nodes.", maze_file, len(maze_df.index)
        )

        if maze_df.empty or "index" not in maze_df.columns:
            log.warning("Maze data is empty or missing the 'index' column.")
            return

        test_row = maze_df.iloc[0].replace({np.nan: None})
        neighbor_map = {
            direction: int(test_row[column])
            for direction, column in (
                ("North", "North"),
                ("South", "South"),
                ("West", "West"),
                ("East", "East"),
            )
            if column in test_row.index and test_row[column] is not None
        }
        score_map = {
            direction: int(test_row[column])
            for direction, column in (
                ("North", "ND"),
                ("South", "SD"),
                ("West", "WD"),
                ("East", "ED"),
            )
            if column in test_row.index and test_row[column] is not None
        }

        log.info("Testing node %d", int(test_row["index"]))
        log.info("Neighbors: %s", neighbor_map or "None")
        log.info("Treasure scores: %s", score_map or "None")
        log.info("Current score: %d", current_score)

    else:
        log.error("Invalid mode")
        sys.exit(1)


if __name__ == "__main__":
    args = parse_args()
    main(**vars(args))
