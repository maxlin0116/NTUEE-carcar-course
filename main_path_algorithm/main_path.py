import argparse
import collections
import os
from pathlib import Path

import pandas as pd


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MAZE_FILE = REPO_ROOT / "server" / "big_maze_114.csv"
DEFAULT_START_NODE = 25

CHUNK_ORDER = ("Chunk 1", "Chunk 2", "Chunk 3", "Chunk 4")
UNCLASSIFIED_CHUNK = "Unclassified"
CHUNKS_DEFINITION = {
    "Chunk 1": [1, 2, 3, 7, 8, 9, 13, 14, 15, 19, 20, 21],
    "Chunk 2": [25, 26, 27, 31, 32, 33, 37, 38, 39, 43, 44, 45],
    "Chunk 3": [28, 29, 30, 34, 35, 36, 40, 41, 42, 46, 47, 48],
    "Chunk 4": [4, 5, 6, 10, 11, 12, 16, 17, 18, 22, 23, 24],
}

# Each lane is defined by entering an anchor node and blocking the edge back to
# the main road. The remaining connected component is the lane's searchable area.
LANE_DEFINITIONS = {
    "23巷線": {"entry": 23, "blocked_neighbor": 22},
    "14巷線": {"entry": 14, "blocked_neighbor": 8},
}


def _load_adjacency(filepath):
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Maze file not found: {filepath}")

    df = pd.read_csv(filepath, skipinitialspace=True)
    df.columns = df.columns.str.strip()

    required_columns = ["index", "North", "South", "West", "East"]
    missing_columns = [column for column in required_columns if column not in df.columns]
    if missing_columns:
        raise ValueError(f"Maze CSV is missing required columns: {', '.join(missing_columns)}")

    adjacency = {}
    for _, row in df.iterrows():
        node_index = int(row["index"])
        adjacency[node_index] = [
            int(row[column])
            for column in ("North", "South", "West", "East")
            if pd.notna(row[column])
        ]

    return adjacency


def shortest_path(adjacency, start_node, goal_node):
    if start_node not in adjacency or goal_node not in adjacency:
        return []

    queue = collections.deque([start_node])
    parent = {start_node: None}

    while queue:
        current = queue.popleft()
        if current == goal_node:
            path = []
            while current is not None:
                path.append(current)
                current = parent[current]
            return list(reversed(path))

        for neighbor in adjacency.get(current, []):
            if neighbor in parent:
                continue
            parent[neighbor] = current
            queue.append(neighbor)

    return []


def compute_maze_analysis(filepath, start_node=DEFAULT_START_NODE):
    adjacency = _load_adjacency(filepath)

    degrees = {node_index: len(neighbors) for node_index, neighbors in adjacency.items()}
    queue = collections.deque(
        node_index
        for node_index, degree in degrees.items()
        if degree == 1 and node_index != start_node
    )
    removed = set()

    while queue:
        current = queue.popleft()
        removed.add(current)
        for neighbor in adjacency.get(current, []):
            if neighbor in removed:
                continue
            degrees[neighbor] -= 1
            if degrees[neighbor] == 1 and neighbor != start_node:
                queue.append(neighbor)

    main_roads_set = {node_index for node_index in adjacency if node_index not in removed}
    node_to_chunk = {
        node_index: chunk_name
        for chunk_name, nodes in CHUNKS_DEFINITION.items()
        for node_index in nodes
    }

    dist_from_origin = {start_node: 0}
    bfs_queue = collections.deque([start_node])
    while bfs_queue:
        current = bfs_queue.popleft()
        for neighbor in adjacency.get(current, []):
            if neighbor in dist_from_origin:
                continue
            dist_from_origin[neighbor] = dist_from_origin[current] + 1
            bfs_queue.append(neighbor)

    dead_ends = sorted(
        node_index for node_index, neighbors in adjacency.items() if len(neighbors) == 1
    )
    chunk_results = {chunk_name: [] for chunk_name in CHUNK_ORDER}
    chunk_results[UNCLASSIFIED_CHUNK] = []

    for dead_end in dead_ends:
        nearest_main = dead_end
        distance_to_main = 0
        if dead_end not in main_roads_set:
            search_queue = collections.deque([(dead_end, 0)])
            visited = {dead_end}
            while search_queue:
                current, distance = search_queue.popleft()
                if current in main_roads_set:
                    nearest_main = current
                    distance_to_main = distance
                    break
                for neighbor in adjacency.get(current, []):
                    if neighbor in visited:
                        continue
                    visited.add(neighbor)
                    search_queue.append((neighbor, distance + 1))

        distance_from_origin = dist_from_origin.get(dead_end, 0)
        score = round(distance_from_origin / distance_to_main, 2) if distance_to_main > 0 else 0

        chunk_name = node_to_chunk.get(nearest_main, UNCLASSIFIED_CHUNK)
        chunk_results[chunk_name].append(
            {
                "node": dead_end,
                "origin_dist": distance_from_origin,
                "main_node": nearest_main,
                "main_dist": distance_to_main,
                "score": score,
            }
        )

    ordered_chunk_names = list(CHUNK_ORDER) + [UNCLASSIFIED_CHUNK]
    chunk_scores = {
        chunk_name: round(
            sum(result["score"] for result in chunk_results.get(chunk_name, [])),
            2,
        )
        for chunk_name in ordered_chunk_names
    }
    total_score = round(sum(chunk_scores.values()), 2)

    all_results = [
        result
        for chunk_name in ordered_chunk_names
        for result in chunk_results.get(chunk_name, [])
    ]

    return {
        "filepath": filepath,
        "start_node": start_node,
        "adjacency": adjacency,
        "main_roads_set": main_roads_set,
        "dist_from_origin": dist_from_origin,
        "dead_ends": dead_ends,
        "chunk_results": chunk_results,
        "chunk_scores": chunk_scores,
        "chunk_order": ordered_chunk_names,
        "all_results": all_results,
        "total_score": total_score,
    }


def lane_nodes(adjacency, entry_node, blocked_neighbor):
    if entry_node not in adjacency:
        raise ValueError(f"Lane entry node does not exist: {entry_node}")

    blocked_edge = {entry_node, blocked_neighbor}
    queue = collections.deque([entry_node])
    visited = {entry_node}

    while queue:
        current = queue.popleft()
        for neighbor in adjacency.get(current, []):
            if {current, neighbor} == blocked_edge:
                continue
            if neighbor in visited:
                continue
            visited.add(neighbor)
            queue.append(neighbor)

    return visited


def order_targets_by_distance(adjacency, start_node, targets):
    remaining = sorted(set(targets))
    ordered = []
    current = start_node

    while remaining:
        best_target = None
        best_path = None
        for target in remaining:
            path = shortest_path(adjacency, current, target)
            if not path:
                continue
            key = (len(path), target)
            if best_path is None or key < (len(best_path), best_target):
                best_target = target
                best_path = path

        if best_target is None:
            raise ValueError(f"No reachable target left from node {current}.")

        ordered.append(best_target)
        remaining.remove(best_target)
        current = best_target

    return ordered


def build_search_path(adjacency, start_node, targets):
    if not targets:
        return [start_node]

    full_path = [start_node]
    current = start_node
    for target in targets:
        segment = shortest_path(adjacency, current, target)
        if not segment:
            raise ValueError(f"No path from node {current} to node {target}.")
        full_path.extend(segment[1:])
        current = target

    return full_path


def analyze_lanes(analysis):
    adjacency = analysis["adjacency"]
    score_by_node = {result["node"]: result for result in analysis["all_results"]}
    lane_analysis = {}

    for lane_name, definition in LANE_DEFINITIONS.items():
        nodes = lane_nodes(
            adjacency,
            entry_node=definition["entry"],
            blocked_neighbor=definition["blocked_neighbor"],
        )
        results = [
            score_by_node[node]
            for node in sorted(nodes)
            if node in score_by_node
        ]
        target_nodes = [result["node"] for result in results if result["score"] > 0]
        total_score = round(sum(result["score"] for result in results), 2)

        lane_analysis[lane_name] = {
            "nodes": nodes,
            "results": results,
            "target_nodes": target_nodes,
            "total_score": total_score,
        }

    lane_order = sorted(
        lane_analysis,
        key=lambda lane_name: (
            -lane_analysis[lane_name]["total_score"],
            LANE_DEFINITIONS[lane_name]["entry"],
        ),
    )
    selected_lane = lane_order[0] if lane_order else None

    ordered_targets = []
    ordered_target_lanes = []
    current_node = analysis["start_node"]
    for lane_name in lane_order:
        lane_targets = order_targets_by_distance(
            adjacency,
            current_node,
            lane_analysis[lane_name]["target_nodes"],
        )
        ordered_targets.extend(lane_targets)
        ordered_target_lanes.extend([lane_name] * len(lane_targets))
        if lane_targets:
            current_node = lane_targets[-1]

    search_path = build_search_path(adjacency, analysis["start_node"], ordered_targets)

    return {
        "lanes": lane_analysis,
        "lane_order": lane_order,
        "selected_lane": selected_lane,
        "ordered_targets": ordered_targets,
        "ordered_target_lanes": ordered_target_lanes,
        "search_path": search_path,
    }


def analyze_maze_with_totals(filepath, start_node=DEFAULT_START_NODE):
    analysis = compute_maze_analysis(filepath, start_node=start_node)

    print(f"分析檔案: {filepath}")
    print(f"起點: {start_node}")
    print("=" * 95)

    for chunk_name in analysis["chunk_order"]:
        chunk_results = analysis["chunk_results"].get(chunk_name, [])
        if not chunk_results:
            continue

        print(f"【{chunk_name}】")
        header = (
            f"{'死路點':<10} | {'到起點距離':<12} | {'最近主幹道點':<15} | "
            f"{'到主幹道距離':<12} | {'分數':<8}"
        )
        print(header)
        print("-" * 90)

        for result in sorted(chunk_results, key=lambda item: item["node"]):
            score_display = result["score"] if result["main_dist"] > 0 else "0 (主幹道)"
            print(
                f"{result['node']:<10} | {result['origin_dist']:<12} | "
                f"{result['main_node']:<15} | {result['main_dist']:<12} | "
                f"{score_display:<8}"
            )

        print("-" * 90)
        print(f">>> {chunk_name} 總分 (Total Score): {analysis['chunk_scores'][chunk_name]}")
        print("=" * 95)
        print()

    print(f"整個迷宮的累積總分: {analysis['total_score']}")
    print()

    lane_summary = analyze_lanes(analysis)
    print("【23 巷線 vs 14 巷線】")
    for lane_name, lane in lane_summary["lanes"].items():
        result_text = ", ".join(
            f"{result['node']}({result['score']})"
            for result in lane["results"]
        ) or "none"
        print(f"{lane_name} 節點: {' -> '.join(map(str, sorted(lane['nodes'])))}")
        print(f"{lane_name} 寶藏分數: {result_text}")
        print(f"{lane_name} 加總: {lane['total_score']}")

    selected_lane = lane_summary["selected_lane"]
    print(f"搜尋巷線順序: {' -> '.join(lane_summary['lane_order'])}")
    print(f"優先搜尋: {selected_lane}")
    print(f"搜尋目標順序: {' -> '.join(map(str, lane_summary['ordered_targets'])) or '(none)'}")
    print(f"搜尋路徑: {' -> '.join(map(str, lane_summary['search_path']))}")

    return analysis


def parse_args():
    parser = argparse.ArgumentParser(description="Analyze maze dead-end scores and lane priority.")
    parser.add_argument(
        "--maze-file",
        default=str(DEFAULT_MAZE_FILE),
        help="Maze CSV file to analyze.",
    )
    parser.add_argument(
        "--start-node",
        default=DEFAULT_START_NODE,
        type=int,
        help="Start node used for d_origin scoring.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    analyze_maze_with_totals(args.maze_file, start_node=args.start_node)
