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


class Action(IntEnum):
    ADVANCE = 1
    U_TURN = 2
    TURN_RIGHT = 3
    TURN_LEFT = 4
    HALT = 5


class Maze:
    def __init__(self, filepath: str):
        self.raw_data = pandas.read_csv(filepath).values
        self.nodes = []
        self.node_dict = {}  # key: index, value: the corresponding node

        if len(self.raw_data) == 0:
            return

        direction_columns = (
            (1, 5, Direction.NORTH),
            (2, 6, Direction.SOUTH),
            (3, 7, Direction.WEST),
            (4, 8, Direction.EAST),
        )

        for row in self.raw_data:
            node_index = int(row[0])
            node = Node(node_index)
            self.nodes.append(node)
            self.node_dict[node_index] = node

        for row in self.raw_data:
            node = self.node_dict[int(row[0])]
            for neighbor_col, distance_col, direction in direction_columns:
                neighbor_index = row[neighbor_col]
                if pandas.isna(neighbor_index):
                    continue

                distance = row[distance_col]
                node.set_successor(
                    self.node_dict[int(neighbor_index)],
                    direction,
                    1 if pandas.isna(distance) else int(distance),
                )

    def get_start_point(self):
        if len(self.node_dict) < 2:
            log.error("Error: the start point is not included.")
            return 0
        return self.node_dict[1]

    def get_node_dict(self):
        return self.node_dict

    def BFS(self, node: Node):
        queue = deque([node])
        visited = {node}
        parent = {node: None}
        # Tips : return a sequence of nodes from the node to the nearest unexplored deadend
        return None

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

    def getActions(self, nodes: List[Node]):
        if len(nodes) < 2:
            return []

        actions = []
        car_dir = Direction.SOUTH

        for i in range(len(nodes) - 1):
            action, car_dir = self.getAction(car_dir, nodes[i], nodes[i + 1])
            actions.append(action)

        return actions

    def path_to_str(self, nodes: List[Node]):
        if not nodes:
            return ""
        return " -> ".join(str(node.get_index()) for node in nodes)

    def actions_to_str(self, actions):
        # cmds should be a string sequence like "fbrl....", use it as the input of BFS checklist #1
        cmd = "fbrls"
        cmds = ""
        for action in actions:
            cmds += cmd[action - 1]
        log.info(cmds)
        return cmds

    def strategy(self, node: Node):
        return self.BFS(node)

    def strategy_2(self, node_from: Node, node_to: Node):
        return self.BFS_2(node_from, node_to)
