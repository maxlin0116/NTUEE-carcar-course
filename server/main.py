import argparse
import logging
import os
import sys
import time
from typing import Callable
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
REPO_ROOT = SERVER_DIR.parent
TEAM_NAME = os.getenv("TEAM_NAME", "WED1")
SERVER_URL = os.getenv("SERVER_URL", "http://carcar.ntuee.org/scoreboard")
MAZE_FILE = os.getenv("MAZE_FILE", str(SERVER_DIR / "maze.csv"))
QUADRANT_MAZE_FILE = os.getenv(
    "QUADRANT_MAZE_FILE",
    str(SERVER_DIR / "big_maze_114.csv"),
)
BT_PORT = os.getenv("BT_PORT", "COM11")
EXPECTED_BT_NAME = os.getenv("EXPECTED_BT_NAME", "HM10_G10")
FAKE_UID_FILE = os.getenv("FAKE_UID_FILE", str(SERVER_DIR / "fakeUID.csv"))
FAKE_GAME_SECONDS = float(os.getenv("FAKE_GAME_SECONDS", "70"))
DEFAULT_START_DIR = "south"
AUTO_NODE_SETTLE_DELAY = 0.06
AUTO_EVENT_DRAIN_SECONDS = 0.25
BT_EVENT_POLL_SECONDS = 0.01
AUTO_NODE_EVENT_COOLDOWN_SECONDS = {
    "F": 0.2,
    "L": 0.7,
    "R": 0.7,
    "B": 0.9,
    "C": 0.9,
    "S": 0.2,
}
if str(REPO_ROOT) not in sys.path:
    sys.path.append(str(REPO_ROOT))

from main_path_algorithm.main_path import (
    DEFAULT_START_NODE as QUADRANT_DEFAULT_START_NODE,
    compute_maze_analysis,
)

MODE_ALIASES = {
    "0": "treasure",
    "treasure": "treasure",
    "treasure-hunting": "treasure",
    "hunt": "treasure",
    "1": "self-test",
    "self-test": "self-test",
    "selftest": "self-test",
    "test": "self-test",
    "2": "uid",
    "uid": "uid",
    "score": "uid",
    "scoreboard": "uid",
    "3": "route",
    "route": "route",
    "bfs": "bfs",
    "go": "go",
    "drive": "go",
    "map": "map",
    "bfs-map": "map",
    "full-map": "map",
    "explore": "map",
    "map-recovery": "map-recovery",
    "recovery-map": "map-recovery",
    "map-r": "map-recovery",
    "rmap": "map-recovery",
    "quadrant-hunt": "quadrant-hunt",
    "quad-hunt": "quadrant-hunt",
    "quad": "quadrant-hunt",
    "qhunt": "quadrant-hunt",
    "qudrant-hunt": "quadrant-hunt",
    "qudrant": "quadrant-hunt",
    "manual": "manual",
}


def parse_args():
    parser = argparse.ArgumentParser(
        description="NTUEE CarCar command-line entry point.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  python server\\main.py manual\n"
            "  python server\\main.py go --from-node 1 --to-node 10 --start-dir south\n"
            "  python server\\main.py bfs --start-node 1 --goal-node 10 --start-dir south\n"
            "  python server\\main.py bfs --start-node 1 --goal-node 10 --drive-bt\n"
            "  python server\\main.py map --start-node 1 --start-dir south --drive-bt\n"
            "  python server\\main.py map-recovery --start-node 1 --start-dir south\n"
            "  python server\\main.py quadrant-hunt --start-dir south\n"
            "  python server\\main.py uid --uid 10BA617E --fake-scoreboard"
        ),
    )
    parser.add_argument(
        "mode",
        help=(
            "Mode name or legacy number: treasure/0, self-test/1, uid/2, "
            "route/3, bfs, go, map, map-recovery, quadrant-hunt, manual"
        ),
        type=str,
    )
    parser.add_argument("--maze-file", default=MAZE_FILE, help="Maze file", type=str)
    parser.add_argument(
        "--quadrant-maze-file",
        default=QUADRANT_MAZE_FILE,
        help="48-node maze file used by quadrant-hunt mode",
        type=str,
    )
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
        help="Listen for UID:<8/14/20 hex> messages from Bluetooth and forward them to the server",
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
    parser.add_argument(
        "--start-node",
        "--from-node",
        dest="start_node",
        help="Route, map, or quadrant-hunt start node index",
        type=int,
    )
    parser.add_argument(
        "--goal-node",
        "--to-node",
        dest="goal_node",
        help="Point-to-point BFS goal node index",
        type=int,
    )
    parser.add_argument(
        "--start-dir",
        default=None,
        choices=("north", "south", "west", "east"),
        help="Car heading at the start node for BFS route execution. If omitted, prompt interactively.",
        type=str,
    )
    parser.add_argument(
        "--drive-mode",
        choices=("manual", "bfs"),
        help="Skip the manual/bfs prompt in route mode",
        type=str,
    )
    parser.add_argument(
        "--drive-bt",
        action="store_true",
        help="In route/map mode, send commands to the car through Bluetooth automatically",
    )
    args = parser.parse_args()
    try:
        args.mode = normalize_mode(args.mode)
    except ValueError as exc:
        parser.error(str(exc))
    return args


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


def print_uid_submission_result(point, result: UIDSubmissionResult):
    response_message = format_checklist_message(result)
    print(f"Server response: {response_message}", flush=True)

    if result.message and result.message != response_message:
        log.info("Raw server response: %s", result.message)

    try:
        current_score = point.get_current_score()
    except Exception as exc:
        current_score = None
        log.error("Failed to fetch current score after UID submission: %s", exc)

    if current_score is None:
        print("Current score: unavailable", flush=True)
    else:
        print(f"Current score: {current_score}", flush=True)

    print(
        f"Time remaining: {format_seconds_left(result.time_remaining)} seconds",
        flush=True,
    )


def submit_uid(point, uid: str):
    try:
        result = point.submit_UID(uid.upper())
    except ValueError as exc:
        print(str(exc), flush=True)
        log.error(str(exc))
        return

    print_uid_submission_result(point, result)


def handle_uid_event(uid: str, point=None):
    print(f"RFID: {uid}", flush=True)
    if point is None:
        return

    log.info("Forwarding UID %s to the scoreboard.", uid)
    submit_uid(point, uid)


def parse_direction(direction_name: str) -> Direction:
    direction_map = {
        "north": Direction.NORTH,
        "south": Direction.SOUTH,
        "west": Direction.WEST,
        "east": Direction.EAST,
    }
    return direction_map[direction_name.lower()]


def normalize_mode(mode_name: str) -> str:
    normalized = MODE_ALIASES.get(mode_name.strip().lower())
    if normalized is None:
        valid_modes = ", ".join(sorted(MODE_ALIASES))
        raise ValueError(f"Unknown mode '{mode_name}'. Valid modes: {valid_modes}")
    return normalized


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
    car_cmds = maze.path_to_car_cmds(path, start_dir)
    return path, actions, car_cmds


def build_bfs_map_plan(
    maze: Maze,
    start_node: int,
    start_dir: Direction,
):
    node_dict = maze.get_node_dict()
    node_from = node_dict.get(start_node)

    if node_from is None:
        raise ValueError(f"Unknown start node index: {start_node}")

    visit_order, path = maze.build_shortest_visit_walk(node_from)
    if not visit_order:
        raise ValueError(f"No reachable nodes found from node {start_node}")

    actions = maze.getActions(path, start_dir)
    car_cmds = maze.path_to_car_cmds(path, start_dir)
    return visit_order, path, actions, car_cmds


def shortest_path_length(maze: Maze, node_from: int, node_to: int) -> int:
    node_dict = maze.get_node_dict()
    from_node = node_dict.get(node_from)
    to_node = node_dict.get(node_to)
    if from_node is None or to_node is None:
        raise ValueError(f"Unknown node index in route: {node_from} -> {node_to}")

    path = maze.strategy_2(from_node, to_node)
    if not path:
        raise ValueError(f"No path found from node {node_from} to node {node_to}")
    return max(len(path) - 1, 0)


def order_targets_by_shortest_distance(
    maze: Maze,
    start_node: int,
    targets,
    score_by_node=None,
):
    remaining_targets = sorted(set(targets))
    score_by_node = score_by_node or {}
    ordered_targets = []
    current_node = start_node

    while remaining_targets:
        best_target = None
        best_key = None
        for target_node in remaining_targets:
            distance = shortest_path_length(maze, current_node, target_node)
            sort_key = (
                distance,
                -score_by_node.get(target_node, 0),
                target_node,
            )
            if best_key is None or sort_key < best_key:
                best_key = sort_key
                best_target = target_node

        ordered_targets.append(best_target)
        remaining_targets.remove(best_target)
        current_node = best_target

    return ordered_targets


def build_multi_goal_plan(
    maze: Maze,
    start_node: int,
    goal_nodes,
    start_dir: Direction,
):
    node_dict = maze.get_node_dict()
    current_node = node_dict.get(start_node)
    if current_node is None:
        raise ValueError(f"Unknown start node index: {start_node}")

    path = [current_node]
    for goal_node in goal_nodes:
        target_node = node_dict.get(goal_node)
        if target_node is None:
            raise ValueError(f"Unknown goal node index: {goal_node}")

        segment = maze.strategy_2(current_node, target_node)
        if not segment:
            raise ValueError(
                f"No path found while building route from node "
                f"{current_node.get_index()} to node {goal_node}"
            )

        path.extend(segment[1:])
        current_node = target_node

    actions = maze.getActions(path, start_dir)
    car_cmds = maze.path_to_car_cmds(path, start_dir)
    return path, actions, car_cmds


def direction_to_name(direction: Direction) -> str:
    return direction.name.lower()


def build_segment_execution_steps(
    maze: Maze,
    start_node: int,
    goal_nodes,
    goal_chunks,
    start_dir: Direction,
):
    node_dict = maze.get_node_dict()
    current_node = node_dict.get(start_node)
    if current_node is None:
        raise ValueError(f"Unknown start node index: {start_node}")

    current_dir = start_dir
    segment_steps = []

    for sequence, (goal_node, chunk_name) in enumerate(zip(goal_nodes, goal_chunks), start=1):
        target_node = node_dict.get(goal_node)
        if target_node is None:
            raise ValueError(f"Unknown goal node index: {goal_node}")

        segment_path = maze.strategy_2(current_node, target_node)
        if not segment_path:
            raise ValueError(
                f"No path found while building segment from node "
                f"{current_node.get_index()} to node {goal_node}"
            )

        segment_start_dir = current_dir
        segment_actions = []
        for path_index in range(len(segment_path) - 1):
            action, current_dir = maze.getAction(
                current_dir,
                segment_path[path_index],
                segment_path[path_index + 1],
            )
            segment_actions.append(action)

        segment_steps.append(
            {
                "sequence": sequence,
                "chunk_name": chunk_name,
                "from_node": current_node.get_index(),
                "to_node": goal_node,
                "path": segment_path,
                "actions": segment_actions,
                "actions_str": maze.actions_to_str(segment_actions),
                "car_cmds": maze.path_to_car_cmds(segment_path, segment_start_dir),
                "start_dir": direction_to_name(segment_start_dir),
                "end_dir": direction_to_name(current_dir),
            }
        )
        current_node = target_node

    return segment_steps


def select_preferred_quadrant(
    maze: Maze,
    analysis,
    start_node: int,
):
    second_score = analysis["chunk_scores"].get("Chunk 2", 0)
    fourth_score = analysis["chunk_scores"].get("Chunk 4", 0)
    if second_score > fourth_score:
        return "Chunk 2"
    if fourth_score > second_score:
        return "Chunk 4"

    candidate_chunks = []
    for chunk_name in ("Chunk 2", "Chunk 4"):
        chunk_targets = [item["node"] for item in analysis["chunk_results"].get(chunk_name, [])]
        if not chunk_targets:
            continue
        nearest_distance = min(
            shortest_path_length(maze, start_node, target_node)
            for target_node in chunk_targets
        )
        candidate_chunks.append((nearest_distance, chunk_name))

    if candidate_chunks:
        candidate_chunks.sort()
        return candidate_chunks[0][1]
    return "Chunk 2"


def select_quadrant_search_order(analysis):
    right_half_score = (
        analysis["chunk_scores"].get("Chunk 2", 0)
        + analysis["chunk_scores"].get("Chunk 3", 0)
    )
    left_half_score = (
        analysis["chunk_scores"].get("Chunk 4", 0)
        + analysis["chunk_scores"].get("Chunk 1", 0)
    )

    if right_half_score >= left_half_score:
        quadrant_order = ["Chunk 2", "Chunk 3", "Chunk 4", "Chunk 1"]
    else:
        quadrant_order = ["Chunk 4", "Chunk 1", "Chunk 2", "Chunk 3"]

    return {
        "right_half": round(right_half_score, 2),
        "left_half": round(left_half_score, 2),
        "quadrant_order": quadrant_order,
    }


def build_quadrant_hunt_plan(
    maze: Maze,
    analysis_maze_file: str,
    start_node: int,
    start_dir: Direction,
):
    analysis = compute_maze_analysis(analysis_maze_file, start_node=start_node)
    half_summary = select_quadrant_search_order(analysis)
    quadrant_order = half_summary["quadrant_order"]

    ordered_targets = []
    ordered_target_chunks = []
    phase_orders = {}
    current_node = start_node

    for chunk_name in quadrant_order:
        chunk_results = [
            item
            for item in analysis["chunk_results"].get(chunk_name, [])
            if item["score"] > 0
        ]
        score_by_node = {
            item["node"]: item["score"]
            for item in chunk_results
        }
        chunk_targets = [item["node"] for item in chunk_results]
        ordered_chunk_targets = order_targets_by_shortest_distance(
            maze=maze,
            start_node=current_node,
            targets=chunk_targets,
            score_by_node=score_by_node,
        )
        phase_orders[chunk_name] = ordered_chunk_targets
        ordered_targets.extend(ordered_chunk_targets)
        ordered_target_chunks.extend([chunk_name] * len(ordered_chunk_targets))
        if ordered_chunk_targets:
            current_node = ordered_chunk_targets[-1]

    path, actions, car_cmds = build_multi_goal_plan(
        maze=maze,
        start_node=start_node,
        goal_nodes=ordered_targets,
        start_dir=start_dir,
    )
    segment_steps = build_segment_execution_steps(
        maze=maze,
        start_node=start_node,
        goal_nodes=ordered_targets,
        goal_chunks=ordered_target_chunks,
        start_dir=start_dir,
    )
    return {
        "analysis": analysis,
        "half_summary": half_summary,
        "quadrant_order": quadrant_order,
        "phase_orders": phase_orders,
        "goal_nodes": ordered_targets,
        "goal_chunks": ordered_target_chunks,
        "segment_steps": segment_steps,
        "path": path,
        "actions": actions,
        "car_cmds": car_cmds,
    }


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


def resolve_start_direction(start_direction: Optional[str]) -> Direction:
    if start_direction:
        return parse_direction(start_direction)
    return prompt_for_start_direction(DEFAULT_START_DIR)


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


def print_pending_events(interface: BTInterface, point=None):
    for event_type, payload in interface.read_events():
        if event_type == "uid":
            handle_uid_event(payload, point)
        elif event_type == "node":
            print("Event: NODE", flush=True)


def drain_pending_events(
    interface: BTInterface,
    duration: float = AUTO_EVENT_DRAIN_SECONDS,
    point=None,
):
    deadline = time.monotonic() + duration
    while time.monotonic() < deadline:
        for event_type, payload in interface.read_events():
            if event_type == "uid":
                handle_uid_event(payload, point)
        time.sleep(BT_EVENT_POLL_SECONDS)


def node_event_cooldown_for_command(command: str) -> float:
    return AUTO_NODE_EVENT_COOLDOWN_SECONDS.get(command, 0.3)


def run_manual_control(interface: BTInterface, point=None):
    print("Manual control mode.")
    print(
        "Commands: G run, H halt, F forward, L left, R right, B/C u-turn, "
        "S stop, P recovery tracking, O original tracking, quit exit"
    )

    valid_commands = {"G", "H", "F", "L", "R", "B", "C", "S", "P", "O"}
    while True:
        print_pending_events(interface, point)
        command = input("Manual command> ").strip().upper()
        if not command:
            continue
        if command in {"QUIT", "EXIT", "Q"}:
            return
        if command == "HELP":
            print(
                "Commands: G run, H halt, F forward, L left, R right, B/C u-turn, "
                "S stop, P recovery tracking, O original tracking, quit exit"
            )
            continue
        if len(command) != 1 or command not in valid_commands:
            print("Invalid command. Use G/H/F/L/R/B/C/S/P/O or quit.")
            continue

        interface.send_command(command)
        print(f"Sent manual command: {command}", flush=True)
        time.sleep(0.05)
        print_pending_events(interface, point)


def prepare_bt_interface(bt_port: str, expected_bt_name: str) -> BTInterface:
    print(f"Checking Bluetooth on {bt_port}...")
    try:
        interface = BTInterface(port=bt_port)
    except Exception as exc:
        raise RuntimeError(
            f"Failed to open {bt_port}. Another program may already be using this port "
            f"(Arduino Serial Monitor, PlatformIO monitor, or another Python script). "
            f"Original error: {exc}"
        ) from exc

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


def execute_car_command_plan(
    car_cmds: str,
    drive_bt: bool,
    interface: Optional[BTInterface],
    completion_message: str,
    point=None,
    recovery_tracking: bool = False,
):
    if not drive_bt:
        return

    if not car_cmds:
        print("No Bluetooth commands need to be sent for this plan.")
        return

    if interface is None:
        raise RuntimeError("Bluetooth interface is not ready for auto-drive.")

    print("Assumption: the car is already placed on the start node and facing the selected start direction.")
    if not wait_for_start_command():
        print("Canceled before sending Bluetooth commands.")
        return

    drain_pending_events(interface, point=point)
    print("BFS auto-drive started.", flush=True)

    if recovery_tracking:
        interface.send_command("P")
        print("Sent tracking mode command: P (recovery PID)", flush=True)
        time.sleep(0.05)

    interface.send_command("G")
    print("Sent run command: G", flush=True)
    print("Waiting for EVENT:NODE before sending the first node command.", flush=True)

    next_index = 0
    next_node_event_allowed_at = 0.0
    try:
        while True:
            events = interface.read_events()
            handled_node_event = False
            for event_type, payload in events:
                if event_type == "uid":
                    handle_uid_event(payload, point)
                elif event_type == "node":
                    now = time.monotonic()
                    if now < next_node_event_allowed_at:
                        continue
                    if handled_node_event:
                        continue
                    handled_node_event = True
                    print("Event: NODE", flush=True)
                    if next_index < len(car_cmds):
                        # Mirror manual control more closely: let the car settle
                        # on the node before sending the next turn/forward command.
                        time.sleep(AUTO_NODE_SETTLE_DELAY)
                        command = car_cmds[next_index]
                        interface.send_command(command)
                        next_index += 1
                        next_node_event_allowed_at = (
                            time.monotonic() + node_event_cooldown_for_command(command)
                        )
                        print(f"Sent node command {next_index}/{len(car_cmds)}: {command}", flush=True)
                    else:
                        interface.send_command("H")
                        print(completion_message, flush=True)
                        return
            time.sleep(BT_EVENT_POLL_SECONDS)
    except KeyboardInterrupt:
        print()
        interface.send_command("H")
        log.info("BFS auto-drive interrupted. Sent halt command.")


def run_bfs_route(
    maze: Maze,
    start_node: int,
    goal_node: int,
    start_dir: Direction,
    drive_bt: bool,
    interface: Optional[BTInterface] = None,
    point=None,
):
    path, actions, car_cmds = build_bfs_plan(maze, start_node, goal_node, start_dir)

    print(f"Path: {maze.path_to_str(path)}")
    print(f"Actions: {maze.actions_to_str(actions)}")
    print(f"Car commands: {car_cmds or '(already at goal)'}")
    execute_car_command_plan(
        car_cmds=car_cmds,
        drive_bt=drive_bt,
        interface=interface,
        completion_message=f"Reached goal node {goal_node}. Sent halt command: H",
        point=point,
    )


def run_bfs_map(
    maze: Maze,
    start_node: int,
    start_dir: Direction,
    drive_bt: bool,
    interface: Optional[BTInterface] = None,
    point=None,
    recovery_tracking: bool = False,
):
    visit_order, path, actions, car_cmds = build_bfs_map_plan(
        maze, start_node, start_dir
    )

    print(f"Shortest full-map visit order: {maze.path_to_str(visit_order)}")
    print(f"Drive path: {maze.path_to_str(path)}")
    print(f"Actions: {maze.actions_to_str(actions)}")
    print(f"Car commands: {car_cmds or '(already at start)'}")
    execute_car_command_plan(
        car_cmds=car_cmds,
        drive_bt=drive_bt,
        interface=interface,
        completion_message="Finished shortest full-map traversal. Sent halt command: H",
        point=point,
        recovery_tracking=recovery_tracking,
    )


def run_quadrant_hunt(
    maze: Maze,
    analysis_maze_file: str,
    start_node: int,
    start_dir: Direction,
    drive_bt: bool,
    interface: Optional[BTInterface] = None,
    point=None,
):
    plan = build_quadrant_hunt_plan(
        maze=maze,
        analysis_maze_file=analysis_maze_file,
        start_node=start_node,
        start_dir=start_dir,
    )
    analysis = plan["analysis"]
    half_summary = plan["half_summary"]

    print(f"Start node: {start_node}")
    print(
        "Quadrant scores: "
        f"Q1={analysis['chunk_scores'].get('Chunk 1', 0)}, "
        f"Q2={analysis['chunk_scores'].get('Chunk 2', 0)}, "
        f"Q3={analysis['chunk_scores'].get('Chunk 3', 0)}, "
        f"Q4={analysis['chunk_scores'].get('Chunk 4', 0)}"
    )
    print(
        "Half scores: "
        f"Q2+Q3={half_summary['right_half']}, "
        f"Q4+Q1={half_summary['left_half']}"
    )
    print(f"Quadrant search order: {' -> '.join(plan['quadrant_order'])}")
    print("Quadrant target order:")
    for chunk_name in plan["quadrant_order"]:
        print(
            f"  {chunk_name}: "
            f"{' -> '.join(map(str, plan['phase_orders'].get(chunk_name, []))) or '(none)'}"
        )
    print(
        f"Target nodes: {' -> '.join(map(str, plan['goal_nodes'])) or '(already complete)'}"
    )
    print("Execution steps:")
    for step in plan["segment_steps"]:
        print(
            f"  {step['sequence']}. [{step['chunk_name']}] "
            f"{step['from_node']} -> {step['to_node']} "
            f"({step['start_dir']} -> {step['end_dir']})"
        )
        print(f"     Path: {maze.path_to_str(step['path'])}")
        print(f"     Actions: {step['actions_str'] or '(none)'}")
        print(f"     Car commands: {step['car_cmds'] or '(none)'}")
    print(f"Drive path: {maze.path_to_str(plan['path'])}")
    print(f"Actions: {maze.actions_to_str(plan['actions'])}")
    print(f"Car commands: {plan['car_cmds'] or '(already at all targets)'}")

    execute_car_command_plan(
        car_cmds=plan["car_cmds"],
        drive_bt=drive_bt,
        interface=interface,
        completion_message="Finished quadrant hunt. Sent halt command: H",
        point=point,
    )


def listen_for_uids(
    interface: BTInterface,
    bt_port: str,
    scoreboard_factory: Optional[Callable[[], object]] = None,
):
    log.info("Listening for RFID UIDs on %s", bt_port)
    print("Game Started.", flush=True)
    point = None
    scoreboard_connect_failed = False

    try:
        while True:
            for event_type, payload in interface.read_events():
                if event_type == "uid":
                    print(f"RFID: {payload}", flush=True)
                    if point is None and scoreboard_factory is not None and not scoreboard_connect_failed:
                        try:
                            point = scoreboard_factory()
                            print("Scoreboard connected.", flush=True)
                        except Exception as exc:
                            scoreboard_connect_failed = True
                            print(
                                "Failed to connect to the scoreboard server. UID is shown locally only.",
                                flush=True,
                            )
                            print(
                                "Use --fake-scoreboard to test locally, or check your network/server URL.",
                                flush=True,
                            )
                            log.error("Scoreboard connection failed: %s", exc)
                    if point is not None:
                        log.info("Forwarding UID %s to the scoreboard.", payload)
                        submit_uid(point, payload)
            time.sleep(BT_EVENT_POLL_SECONDS)
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
    mode: str,
    bt_port: str,
    team_name: str,
    server_url: str,
    maze_file: str,
    quadrant_maze_file: str,
    uid: str = None,
    listen_bt: bool = False,
    fake_scoreboard: bool = False,
    fake_uid_file: str = FAKE_UID_FILE,
    fake_game_seconds: float = FAKE_GAME_SECONDS,
    start_node: Optional[int] = None,
    goal_node: Optional[int] = None,
    start_dir: Optional[str] = None,
    drive_mode: Optional[str] = None,
    drive_bt: bool = False,
    expected_bt_name: str = EXPECTED_BT_NAME,
):
    mode = normalize_mode(mode)
    recovery_tracking = False
    if mode == "manual":
        mode = "route"
        drive_mode = "manual"
        drive_bt = True
    elif mode == "bfs":
        mode = "route"
        drive_mode = drive_mode or "bfs"
    elif mode == "go":
        mode = "route"
        drive_mode = "bfs"
        drive_bt = True
    elif mode == "map":
        drive_bt = True
    elif mode == "map-recovery":
        mode = "map"
        drive_bt = True
        recovery_tracking = True
    elif mode == "quadrant-hunt":
        drive_bt = True

    start_time = time.time()
    current_node = None
    current_action = None
    visited_nodes = set()
    planned_actions = []

    if mode == "treasure":
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

    elif mode == "self-test":
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

    elif mode == "uid":
        log.info("Mode 2: Server UID integration.")

        if uid:
            point = build_scoreboard(
                team_name,
                server_url,
                fake_scoreboard,
                fake_uid_file,
                fake_game_seconds,
            )
            print("Game Started.")
            submit_uid(point, uid)
            return

        if listen_bt:
            interface = prepare_bt_interface(bt_port, expected_bt_name)
            try:
                listen_for_uids(
                    interface=interface,
                    bt_port=bt_port,
                    scoreboard_factory=lambda: build_scoreboard(
                        team_name,
                        server_url,
                        fake_scoreboard,
                        fake_uid_file,
                        fake_game_seconds,
                    ),
                )
            finally:
                interface.close()
            return

        point = build_scoreboard(
            team_name,
            server_url,
            fake_scoreboard,
            fake_uid_file,
            fake_game_seconds,
        )
        print("Game Started.")
        print("Enter UID (8, 14, or 20 hex digits). Type 'quit' to exit.")
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

    elif mode == "route":
        interface = None
        point = None
        selected_mode = drive_mode or "bfs"
        if selected_mode == "manual":
            drive_bt = True

        if drive_bt:
            interface = prepare_bt_interface(bt_port, expected_bt_name)
            point = build_scoreboard(
                team_name,
                server_url,
                fake_scoreboard,
                fake_uid_file,
                fake_game_seconds,
            )
            if drive_mode is None and start_node is None and goal_node is None:
                selected_mode = prompt_drive_mode()
            if selected_mode == "manual":
                print("UID forwarding is enabled during manual Bluetooth control.")
                try:
                    run_manual_control(interface, point)
                finally:
                    interface.close()
                return
            print("UID forwarding is enabled during BFS route execution.")

        if start_node is None:
            start_node = prompt_for_node("Start")
        if goal_node is None:
            goal_node = prompt_for_node("Goal")

        chosen_start_dir = resolve_start_direction(start_dir)

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
                point=point,
            )
        finally:
            if interface is not None:
                interface.close()

    elif mode == "map":
        interface = None
        point = None
        if drive_bt:
            interface = prepare_bt_interface(bt_port, expected_bt_name)
            point = build_scoreboard(
                team_name,
                server_url,
                fake_scoreboard,
                fake_uid_file,
                fake_game_seconds,
            )
            if recovery_tracking:
                print("UID forwarding is enabled during recovery shortest full-map traversal.")
                print("Recovery tracking will be enabled automatically before G.")
            else:
                print("UID forwarding is enabled during shortest full-map traversal.")

        if start_node is None:
            start_node = prompt_for_node("Start")
        if goal_node is not None:
            log.warning("Ignoring goal node %d in shortest full-map mode.", goal_node)

        chosen_start_dir = resolve_start_direction(start_dir)
        maze = Maze(maze_file)
        log.info("Shortest full-map traversal from node %d.", start_node)
        try:
            run_bfs_map(
                maze=maze,
                start_node=start_node,
                start_dir=chosen_start_dir,
                drive_bt=drive_bt,
                interface=interface,
                point=point,
                recovery_tracking=recovery_tracking,
            )
        finally:
            if interface is not None:
                interface.close()

    elif mode == "quadrant-hunt":
        interface = None
        point = None
        if drive_bt:
            interface = prepare_bt_interface(bt_port, expected_bt_name)
            point = build_scoreboard(
                team_name,
                server_url,
                fake_scoreboard,
                fake_uid_file,
                fake_game_seconds,
            )
            print("UID forwarding is enabled during quadrant hunt.")

        if start_node is None:
            start_node = QUADRANT_DEFAULT_START_NODE
            print(f"Using default quadrant-hunt start node: {start_node}")
        if goal_node is not None:
            log.warning("Ignoring goal node %d in quadrant-hunt mode.", goal_node)

        chosen_start_dir = resolve_start_direction(start_dir)
        maze = Maze(quadrant_maze_file)
        log.info(
            "Quadrant hunt from node %d using analysis file '%s'.",
            start_node,
            quadrant_maze_file,
        )
        try:
            run_quadrant_hunt(
                maze=maze,
                analysis_maze_file=quadrant_maze_file,
                start_node=start_node,
                start_dir=chosen_start_dir,
                drive_bt=drive_bt,
                interface=interface,
                point=point,
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
