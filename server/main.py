import argparse
import logging
import os
import sys
import time
from pathlib import Path
from typing import Optional

import numpy as np
import pandas
from bt_interface import BTInterface
from maze import Direction, Maze
from score import ScoreboardFake, ScoreboardServer, UIDSubmissionResult

logging.basicConfig(
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s", level=logging.INFO
)

log = logging.getLogger(__name__)

SERVER_DIR = Path(__file__).resolve().parent
TEAM_NAME = os.getenv("TEAM_NAME", "WED3")
SERVER_URL = os.getenv("SERVER_URL", "http://carcar.ntuee.org/scoreboard")
MAZE_FILE = os.getenv("MAZE_FILE", "maze.csv")
BT_PORT = os.getenv("BT_PORT", "COM11")
EXPECTED_BT_NAME = os.getenv("EXPECTED_BT_NAME", "HM10_G6")
FAKE_UID_FILE = os.getenv("FAKE_UID_FILE", str(SERVER_DIR / "fakeUID.csv"))
FAKE_GAME_SECONDS = float(os.getenv("FAKE_GAME_SECONDS", "70"))


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "mode",
        help="0: treasure-hunting, 1: self-testing, 2: server UID integration, 3: BFS route integration",
        type=str,
    )
    parser.add_argument("--maze-file", default=MAZE_FILE, help="Maze file", type=str)
    parser.add_argument("--bt-port", default=BT_PORT, help="Bluetooth port", type=str)
    parser.add_argument(
        "--expected-bt-name",
        default=EXPECTED_BT_NAME,
        help="Expected HM-10 device name for ESP32 bridge",
        type=str,
    )
    parser.add_argument(
        "--team-name", default=TEAM_NAME, help="Your team name", type=str
    )
    parser.add_argument("--server-url", default=SERVER_URL, help="Server URL", type=str)
    parser.add_argument("--uid", help="Submit a single UID directly to the server", type=str)
    parser.add_argument(
        "--listen-bt",
        action="store_true",
        help="Listen for UID:<8 hex> messages from Bluetooth and forward them to the server",
    )
    parser.add_argument(
        "--fake-scoreboard",
        action="store_true",
        help="Use local fakeUID.csv instead of the live scoreboard server",
    )
    parser.add_argument(
        "--fake-uid-file",
        default=FAKE_UID_FILE,
        help="Local fake UID score table for --fake-scoreboard",
        type=str,
    )
    parser.add_argument(
        "--fake-game-seconds",
        default=FAKE_GAME_SECONDS,
        help="Countdown duration for --fake-scoreboard",
        type=float,
    )
    parser.add_argument("--start-node", help="BFS route start node index", type=int)
    parser.add_argument("--goal-node", help="BFS route goal node index", type=int)
    parser.add_argument(
        "--start-dir",
        default="south",
        choices=("north", "south", "west", "east"),
        help="Car heading at the start node for BFS route execution",
        type=str,
    )
    parser.add_argument(
        "--drive-bt",
        action="store_true",
        help="In BFS mode, send commands to the car through Bluetooth automatically",
    )
    return parser.parse_args()


def format_seconds_left(seconds_left: float) -> str:
    rounded = round(float(seconds_left), 1)
    if rounded.is_integer():
        return str(int(rounded))
    return f"{rounded:.1f}"


def format_checklist_message(result: UIDSubmissionResult) -> str:
    if result.score > 0:
        return (
            f"Added {result.score} Points at "
            f"{format_seconds_left(result.time_remaining)} seconds left."
        )
    return "uid not found"


def submit_uid(point, uid: str):
    try:
        result = point.submit_UID(uid.upper())
    except ValueError as exc:
        print(str(exc))
        log.error(str(exc))
        return

    checklist_message = format_checklist_message(result)
    print(checklist_message)

    if result.message and result.message != checklist_message:
        log.info("Server response: %s", result.message)


def parse_direction(direction_name: str) -> Direction:
    direction_map = {
        "north": Direction.NORTH,
        "south": Direction.SOUTH,
        "west": Direction.WEST,
        "east": Direction.EAST,
    }
    return direction_map[direction_name.lower()]


def build_bfs_plan(
    maze: Maze,
    start_node: int,
    goal_node: int,
    start_dir: Direction,
):
    node_dict = maze.get_node_dict()
    node_from = node_dict.get(start_node)
    node_to = node_dict.get(goal_node)

    if node_from is None or node_to is None:
        raise ValueError(f"Unknown node index in route: {start_node} -> {goal_node}")

    path = maze.strategy_2(node_from, node_to)
    if not path:
        raise ValueError(f"No path found from node {start_node} to node {goal_node}")

    actions = maze.getActions(path, start_dir)
    car_cmds = maze.actions_to_car_cmds(actions)
    return path, actions, car_cmds


def prompt_for_node(name: str) -> int:
    while True:
        value = input(f"{name} node> ").strip()
        try:
            return int(value)
        except ValueError:
            print("Please enter an integer node index.")


def prompt_for_start_direction(default_direction: str) -> Direction:
    choices = {"north", "south", "west", "east"}
    prompt = f"Start direction [{default_direction}]> "
    while True:
        value = input(prompt).strip().lower()
        if not value:
            return parse_direction(default_direction)
        if value in choices:
            return parse_direction(value)
        print("Please enter north, south, west, or east.")


def wait_for_start_command() -> bool:
    print("Type G and press Enter to start the car. Type anything else to cancel.")
    command = input("Start command> ").strip().upper()
    return command == "G"


def prompt_drive_mode() -> str:
    print("Select drive mode:")
    print("  manual - manual remote control")
    print("  bfs    - BFS auto-drive")
    while True:
        choice = input("Drive mode [manual/bfs]> ").strip().lower()
        if choice in {"manual", "bfs"}:
            return choice
        print("Please enter manual or bfs.")


def print_pending_events(interface: BTInterface):
    for event_type, payload in interface.read_events():
        if event_type == "uid":
            print(f"RFID: {payload}")
        elif event_type == "node":
            print("Event: NODE")


def run_manual_control(interface: BTInterface):
    print("Manual control mode.")
    print("Commands: G run, H halt, F forward, L left, R right, B u-turn, S stop, quit exit")

    valid_commands = {"G", "H", "F", "L", "R", "B", "S"}
    while True:
        print_pending_events(interface)
        command = input("Manual command> ").strip().upper()
        if not command:
            continue
        if command in {"QUIT", "EXIT", "Q"}:
            return
        if command == "HELP":
            print("Commands: G run, H halt, F forward, L left, R right, B u-turn, S stop, quit exit")
            continue
        if len(command) != 1 or command not in valid_commands:
            print("Invalid command. Use G/H/F/L/R/B/S or quit.")
            continue

        interface.send_command(command)
        print(f"Sent manual command: {command}")
        time.sleep(0.05)
        print_pending_events(interface)


def prepare_bt_interface(bt_port: str, expected_bt_name: str) -> BTInterface:
    print(f"Checking Bluetooth on {bt_port}...")
    interface = BTInterface(port=bt_port)

    current_name = interface.get_hm10_name()
    if current_name != expected_bt_name:
        print(f"Target mismatch. Current: {current_name}, Expected: {expected_bt_name}")
        print(f"Updating target name to {expected_bt_name}...")
        if not interface.set_hm10_name(expected_bt_name):
            interface.close()
            raise RuntimeError("Failed to set HM-10 target name on ESP32.")
        print("Name updated successfully. Resetting ESP32...")
        if not interface.reset():
            interface.close()
            raise RuntimeError("Failed to reset ESP32 bridge after updating target name.")
        interface.close()
        interface = BTInterface(port=bt_port)

    status = interface.get_status()
    if status != "CONNECTED":
        interface.close()
        raise RuntimeError(
            f"ESP32 is {status}. Please ensure HM-10 is advertising and connected."
        )

    print(f"Ready! Connected to {expected_bt_name}")
    return interface


def run_bfs_route(
    maze: Maze,
    start_node: int,
    goal_node: int,
    start_dir: Direction,
    drive_bt: bool,
    interface: Optional[BTInterface] = None,
):
    path, actions, car_cmds = build_bfs_plan(maze, start_node, goal_node, start_dir)

    print(f"Path: {maze.path_to_str(path)}")
    print(f"Actions: {maze.actions_to_str(actions)}")
    print(f"Car commands: {car_cmds or '(already at goal)'}")

    if not drive_bt:
        return

    if not car_cmds:
        print("Already at goal node. No Bluetooth commands sent.")
        return

    if interface is None:
        raise RuntimeError("Bluetooth interface is not ready for auto-drive.")

    print("Assumption: the car is already placed on the start node and facing the selected start direction.")
    if not wait_for_start_command():
        print("Canceled before sending Bluetooth commands.")
        return

    print("BFS auto-drive started.")

    interface.send_command("G")
    time.sleep(0.1)
    interface.send_command(car_cmds[0])
    print(f"Sent run command: G")
    print(f"Sent node command 1/{len(car_cmds)}: {car_cmds[0]}")

    next_index = 1
    try:
        while True:
            events = interface.read_events()
            for event_type, payload in events:
                if event_type == "uid":
                    print(f"RFID: {payload}")
                elif event_type == "node":
                    if next_index < len(car_cmds):
                        command = car_cmds[next_index]
                        interface.send_command(command)
                        next_index += 1
                        print(f"Sent node command {next_index}/{len(car_cmds)}: {command}")
                    else:
                        interface.send_command("H")
                        print(f"Reached goal node {goal_node}. Sent halt command: H")
                        return
            time.sleep(0.05)
    except KeyboardInterrupt:
        print()
        interface.send_command("H")
        log.info("BFS auto-drive interrupted. Sent halt command.")


def listen_for_uids(point, interface: BTInterface, bt_port: str):
    log.info("Listening for RFID UIDs on %s", bt_port)
    print("Game Started.")

    try:
        while True:
            uid = interface.read_uid()
            if uid:
                log.info("Received UID from Bluetooth: %s", uid)
                submit_uid(point, uid)
            time.sleep(0.1)
    except KeyboardInterrupt:
        print()
        log.info("Stopped Bluetooth listening.")


def build_scoreboard(
    team_name: str,
    server_url: str,
    fake_scoreboard: bool = False,
    fake_uid_file: str = FAKE_UID_FILE,
    fake_game_seconds: float = FAKE_GAME_SECONDS,
):
    if fake_scoreboard:
        log.info(
            "Using fake scoreboard data from %s with %.1f seconds",
            fake_uid_file,
            fake_game_seconds,
        )
        return ScoreboardFake(team_name, fake_uid_file, game_duration=fake_game_seconds)
    return ScoreboardServer(team_name, server_url)


def main(
    mode: int,
    bt_port: str,
    team_name: str,
    server_url: str,
    maze_file: str,
    uid: str = None,
    listen_bt: bool = False,
    fake_scoreboard: bool = False,
    fake_uid_file: str = FAKE_UID_FILE,
    fake_game_seconds: float = FAKE_GAME_SECONDS,
    start_node: Optional[int] = None,
    goal_node: Optional[int] = None,
    start_dir: str = "south",
    drive_bt: bool = False,
    expected_bt_name: str = EXPECTED_BT_NAME,
):
    start_time = time.time()
    current_node = None
    current_action = None
    visited_nodes = set()
    planned_actions = []

    if mode == "0":
        maze = Maze(maze_file)
        point = build_scoreboard(
            team_name,
            server_url,
            fake_scoreboard,
            fake_uid_file,
            fake_game_seconds,
        )
        current_score = point.get_current_score() or 0
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
        maze = Maze(maze_file)
        current_score = 0
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

    elif mode == "2":
        point = build_scoreboard(
            team_name,
            server_url,
            fake_scoreboard,
            fake_uid_file,
            fake_game_seconds,
        )
        log.info("Mode 2: Server UID integration.")

        if uid:
            print("Game Started.")
            submit_uid(point, uid)
            return

        if listen_bt:
            interface = prepare_bt_interface(bt_port, expected_bt_name)
            try:
                listen_for_uids(point, interface, bt_port)
            finally:
                interface.close()
            return

        print("Game Started.")
        print("Enter UID (8 hex digits). Type 'quit' to exit.")
        while True:
            try:
                manual_uid = input("UID> ").strip()
            except EOFError:
                print()
                return

            if manual_uid.lower() in {"quit", "exit"}:
                return
            if not manual_uid:
                continue

            submit_uid(point, manual_uid)

    elif mode == "3":
        interface = None
        selected_mode = "bfs"
        if drive_bt:
            interface = prepare_bt_interface(bt_port, expected_bt_name)
            if start_node is None and goal_node is None:
                selected_mode = prompt_drive_mode()
            if selected_mode == "manual":
                try:
                    run_manual_control(interface)
                finally:
                    interface.close()
                return

        if start_node is None:
            start_node = prompt_for_node("Start")
        if goal_node is None:
            goal_node = prompt_for_node("Goal")

        chosen_start_dir = prompt_for_start_direction(start_dir)

        maze = Maze(maze_file)
        log.info(
            "Mode 3: BFS route integration from node %d to node %d.", start_node, goal_node
        )
        try:
            run_bfs_route(
                maze=maze,
                start_node=start_node,
                goal_node=goal_node,
                start_dir=chosen_start_dir,
                drive_bt=drive_bt,
                interface=interface,
            )
        finally:
            if interface is not None:
                interface.close()

    else:
        log.error("Invalid mode")
        sys.exit(1)


if __name__ == "__main__":
    args = parse_args()
    main(**vars(args))
