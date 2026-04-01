import pandas as pd
from collections import deque

# 讀取 CSV
df = pd.read_csv("maze.csv")

# 建立 graph
graph = {}

for _, row in df.iterrows():

    node = int(row["index"])
    graph[node] = {}

    if not pd.isna(row["North"]):
        graph[node][int(row["North"])] = "N"

    if not pd.isna(row["East"]):
        graph[node][int(row["East"])] = "E"

    if not pd.isna(row["South"]):
        graph[node][int(row["South"])] = "S"

    if not pd.isna(row["West"]):
        graph[node][int(row["West"])] = "W"


# 絕對方向 → 相對動作
def get_action(heading, move):

    order = ["N", "E", "S", "W"]

    h = order.index(heading)
    m = order.index(move)

    diff = (m - h) % 4

    if diff == 0:
        return "F"
    elif diff == 1:
        return "R"
    elif diff == 3:
        return "L"
    else:
        return "B"


# BFS 找最短路
def shortest_path(start, goal, start_heading="N"):

    queue = deque([(start, [start], [], start_heading)])
    visited = set()

    while queue:

        node, node_path, action_path, heading = queue.popleft()

        if node == goal:
            return node_path, action_path

        if node in visited:
            continue

        visited.add(node)

        for nxt in graph[node]:

            move_dir = graph[node][nxt]

            action = get_action(heading, move_dir)

            queue.append(
                (
                    nxt,
                    node_path + [nxt],
                    action_path + [action],
                    move_dir,
                )
            )

    return None, None


# ===== 設定起點終點 =====
start = 1
goal = 11
start_heading = "N"

nodes, actions = shortest_path(start, goal, start_heading)

print("Node path:")
print(nodes)

print("FRLB actions:")
print("".join(actions))