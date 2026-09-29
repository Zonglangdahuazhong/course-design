
import matplotlib.pyplot as plt


def draw_map(filename):
    # =========================
    # 1. 读取 map.txt
    # =========================

    with open(filename, "r", encoding="utf-8") as f:

        # 第一行：节点数、边数
        first_line = f.readline().strip()

        node_count, edge_count = map(
            int,
            first_line.split()
        )

        print("节点数量：", node_count)
        print("边数量：", edge_count)

        # =========================
        # 2. 读取节点
        # =========================

        nodes = {}

        for _ in range(node_count):

            line = f.readline().strip()

            node_id, lon, lat = line.split()

            node_id = int(node_id)
            lon = float(lon)
            lat = float(lat)

            nodes[node_id] = (lon, lat)

        print("节点读取完成")

        # =========================
        # 3. 读取道路
        # =========================

        edges = []

        for _ in range(edge_count):

            line = f.readline().strip()

            u, v, distance = line.split()

            u = int(u)
            v = int(v)
            distance = float(distance)

            edges.append((u, v))

        print("道路读取完成")

    # =========================
    # 4. 开始画图
    # =========================

    plt.figure(figsize=(12, 10))

    for u, v in edges:

        if u not in nodes or v not in nodes:
            continue

        lon1, lat1 = nodes[u]
        lon2, lat2 = nodes[v]

        plt.plot(
            [lon1, lon2],
            [lat1, lat2],
            linewidth=0.5
        )

    # =========================
    # 5. 设置图像
    # =========================

    plt.xlabel("Longitude")
    plt.ylabel("Latitude")

    plt.title("Road Network")

    plt.axis("equal")

    plt.tight_layout()

    plt.savefig("map.png", dpi=200)

print("地图已经保存到：map.png")


if __name__ == "__main__":

    draw_map("map.txt")

