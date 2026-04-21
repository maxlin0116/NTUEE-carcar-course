import csv
import logging
import math
from collections import deque
from enum import IntEnum
from typing import List

import numpy as np
import pandas

from node import Direction, Node

log = logging.getLogger(__name__)

# The Action enum represents the possible actions the car can take based on its current direction and the direction to the next node.
class Action(IntEnum):
    ADVANCE = 1
    U_TURN = 2
    TURN_RIGHT = 3
    TURN_LEFT = 4
    HALT = 5

# The Maze class represents the maze structure and provides methods for pathfinding and action generation.
class Maze:
    @staticmethod
    def _parse_optional_int(value):
        if pandas.isna(value):
            return None
        if isinstance(value, str):
            value = value.strip()
            if not value:
                return None
        return int(float(value))

    # The constructor reads the maze structure from a CSV file and initializes the nodes and their connections.
    def __init__(self, filepath: str):
        self.raw_data = pandas.read_csv(filepath).values
        self.nodes = []
        self.node_dict = {}  # key: index, value: the corresponding node

        if len(self.raw_data) == 0:
            return

		# The direction_columns variable defines the columns in the CSV file that correspond to the neighboring nodes and their distances, along with the direction from the current node to the neighbor.
        direction_columns = (
            # neighbor_col, distance_col, direction
            (1, 5, Direction.NORTH),
            (2, 6, Direction.SOUTH),
            (3, 7, Direction.WEST),
            (4, 8, Direction.EAST),
        )
		
		# First, we create Node objects for each row in the CSV file and store them in a list and a dictionary for easy access.
        for row in self.raw_data:
            node_index = self._parse_optional_int(row[0])
            if node_index is None:
                raise ValueError("Maze CSV contains a row without a valid node index.")
            node = Node(node_index)
            self.nodes.append(node)
            self.node_dict[node_index] = node

		# Next, we iterate through the rows again to establish the connections between the nodes based on the neighboring node indices and their corresponding directions and distances.
        for row in self.raw_data:
            node_index = self._parse_optional_int(row[0])
            if node_index is None:
                continue
            node = self.node_dict[node_index]
            for neighbor_col, distance_col, direction in direction_columns:
                neighbor_index = self._parse_optional_int(row[neighbor_col])
                if neighbor_index is None:
                    continue

                distance = self._parse_optional_int(row[distance_col])
                node.set_successor(
                    self.node_dict[neighbor_index],
                    direction,
                    1 if distance is None else distance,
                )

	# The get_start_point method returns the starting node of the maze, which is assumed to be the node with index 1. If there are fewer than 2 nodes in the maze, it logs an error and returns 0.
    def get_start_point(self):
        if len(self.node_dict) < 2:
            log.error("Error: the start point is not included.")
            return 0
        return self.node_dict[1]

	# The get_node_dict method returns the dictionary of nodes, which can be used for quick access to nodes based on their indices.
    def get_node_dict(self):
        return self.node_dict

	# The BFS method returns the reachable nodes in breadth-first visitation order.
    def BFS(self, node: Node):
        if node is None:
            return []

        queue = deque([node])
        visited = {node}
        order = []

        while queue:
            current = queue.popleft()
            order.append(current)

            for successor, _, _ in current.get_successors():
                if successor in visited:
                    continue
                visited.add(successor)
                queue.append(successor)

        return order

	# The BFS_2 method performs a breadth-first search to find the shortest path from node_from to node_to. It uses a queue to explore the nodes and a set to keep track of visited nodes. The parent dictionary is used to reconstruct the path once node_to is found. If either node_from or node_to is None, it returns None. If node_from is the same as node_to, it returns a list containing just that node. If no path is found, it returns None.
    def BFS_2(self, node_from: Node, node_to: Node):
        if node_from is None or node_to is None:
            return None

        if node_from == node_to:
            return [node_from]

        queue = deque([node_from])
        visited = {node_from}
        parent = {node_from: None}

        while queue:
            current = queue.popleft()
            for successor, _, _ in current.get_successors():
                if successor in visited:
                    continue

                visited.add(successor)
                parent[successor] = current

                if successor == node_to:
                    path = [node_to]
                    while current is not None:
                        path.append(current)
                        current = parent[current]
                    path.reverse()
                    return path

                queue.append(successor)

        return None

	# The getAction method determines the action to take based on the current car direction and the next node.
    def getAction(self, car_dir, node_from: Node, node_to: Node):
        if not node_from.is_successor(node_to):
            print(
                f"Error: Node {node_to.get_index()} is not adjacent to Node {node_from.get_index()}."
            )
            return 0

        next_dir = node_from.get_direction(node_to)
        action_map = {
            Direction.NORTH: {
                Direction.NORTH: Action.ADVANCE,
                Direction.SOUTH: Action.U_TURN,
                Direction.WEST: Action.TURN_LEFT,
                Direction.EAST: Action.TURN_RIGHT,
            },
            Direction.SOUTH: {
                Direction.NORTH: Action.U_TURN,
                Direction.SOUTH: Action.ADVANCE,
                Direction.WEST: Action.TURN_RIGHT,
                Direction.EAST: Action.TURN_LEFT,
            },
            Direction.WEST: {
                Direction.NORTH: Action.TURN_RIGHT,
                Direction.SOUTH: Action.TURN_LEFT,
                Direction.WEST: Action.ADVANCE,
                Direction.EAST: Action.U_TURN,
            },
            Direction.EAST: {
                Direction.NORTH: Action.TURN_LEFT,
                Direction.SOUTH: Action.TURN_RIGHT,
                Direction.WEST: Action.U_TURN,
                Direction.EAST: Action.ADVANCE,
            },
        }
        return action_map[Direction(car_dir)][next_dir], next_dir

    def getActions(self, nodes: List[Node], start_dir=Direction.SOUTH):
        if len(nodes) < 2:
            return []

        actions = []
        car_dir = Direction(start_dir)

        for i in range(len(nodes) - 1):
            action, car_dir = self.getAction(car_dir, nodes[i], nodes[i + 1])
            actions.append(action)

        return actions

	# The path_to_str method converts a list of nodes into a string representation of the path, where each node index is separated by " -> ".
    def path_to_str(self, nodes: List[Node]):
        if not nodes:
            return ""
        return " -> ".join(str(node.get_index()) for node in nodes)

	# The actions_to_str method converts a list of actions into a string representation, where each action is represented by a character (f for advance, b for u-turn, r for turn right, l for turn left, and s for halt). It uses a mapping of action integers to characters to generate the string.
    def actions_to_str(self, actions):
        # cmds should be a string sequence like "fbrl....", use it as the input of BFS checklist #1
        cmd = "fbrls" # f for advance, b for u-turn, r for turn right, l for turn left, s for halt
        cmds = ""
        for action in actions:
            cmds += cmd[action - 1]
        log.info(cmds)
        return cmds

    def actions_to_car_cmds(self, actions):
        cmd = "FBRLS"
        return "".join(cmd[action - 1] for action in actions)

    def build_bfs_walk(self, start_node: Node):
        visit_order = self.BFS(start_node)
        if not visit_order:
            return [], []

        full_path = [visit_order[0]]
        current = visit_order[0]

        for target in visit_order[1:]:
            segment = self.BFS_2(current, target)
            if not segment:
                raise ValueError(
                    f"No path found while building BFS walk from node "
                    f"{current.get_index()} to node {target.get_index()}"
                )
            full_path.extend(segment[1:])
            current = target

        return visit_order, full_path

    def build_shortest_visit_walk(self, start_node: Node, exact_limit=16):
        reachable_nodes = self.BFS(start_node)
        if not reachable_nodes:
            return [], []

        if len(reachable_nodes) > exact_limit:
            log.warning(
                "Shortest full-map walk has %d reachable nodes; falling back to greedy ordering.",
                len(reachable_nodes),
            )
            return self._build_greedy_visit_walk(start_node, reachable_nodes)

        node_to_bit = {
            node: 1 << index
            for index, node in enumerate(reachable_nodes)
        }
        full_mask = (1 << len(reachable_nodes)) - 1
        start_state = (start_node, node_to_bit[start_node])
        queue = deque([start_state])
        parent = {start_state: None}
        end_state = None

        while queue:
            current, visited_mask = queue.popleft()
            if visited_mask == full_mask:
                end_state = (current, visited_mask)
                break

            for successor, _, _ in current.get_successors():
                next_state = (
                    successor,
                    visited_mask | node_to_bit[successor],
                )
                if next_state in parent:
                    continue
                parent[next_state] = (current, visited_mask)
                queue.append(next_state)

        if end_state is None:
            raise ValueError(
                f"No full-map walk found from node {start_node.get_index()}."
            )

        full_path = []
        state = end_state
        while state is not None:
            full_path.append(state[0])
            state = parent[state]
        full_path.reverse()

        seen = set()
        visit_order = []
        for node in full_path:
            if node in seen:
                continue
            seen.add(node)
            visit_order.append(node)

        return visit_order, full_path

    def _build_greedy_visit_walk(self, start_node: Node, reachable_nodes: List[Node]):
        unvisited = set(reachable_nodes)
        unvisited.discard(start_node)
        visit_order = [start_node]
        full_path = [start_node]
        current = start_node

        while unvisited:
            best_path = None
            best_target = None
            for target in sorted(unvisited, key=lambda node: node.get_index()):
                path = self.BFS_2(current, target)
                if not path:
                    continue
                sort_key = (len(path), target.get_index())
                if best_path is None or sort_key < (len(best_path), best_target.get_index()):
                    best_path = path
                    best_target = target

            if best_path is None:
                raise ValueError(
                    f"No path found while building greedy full-map walk from node "
                    f"{current.get_index()}."
                )

            full_path.extend(best_path[1:])
            for node in best_path[1:]:
                if node in unvisited:
                    unvisited.remove(node)
                    visit_order.append(node)
            current = best_target

        return visit_order, full_path

    def strategy(self, node: Node):
        return self.BFS(node)

    def strategy_2(self, node_from: Node, node_to: Node):
        return self.BFS_2(node_from, node_to)
