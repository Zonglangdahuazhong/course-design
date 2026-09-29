import xml.etree.ElementTree as ET
import math
import sys


# ============================================================
# 配置：需要保留的道路类型
# ============================================================

ALLOWED_HIGHWAYS = {
    "motorway",
    "trunk",
    "primary",
    "secondary",
    "tertiary",
    "unclassified",
    "residential",
    "living_street",
    "service",
}


# ============================================================
# 计算两个经纬度之间的距离
# 使用 Haversine 公式
# 返回：米
# ============================================================

def haversine(lon1, lat1, lon2, lat2):

    R = 6371000.0

    lon1 = math.radians(lon1)
    lat1 = math.radians(lat1)

    lon2 = math.radians(lon2)
    lat2 = math.radians(lat2)

    dlon = lon2 - lon1
    dlat = lat2 - lat1

    a = (
        math.sin(dlat / 2) ** 2
        +
        math.cos(lat1)
        * math.cos(lat2)
        * math.sin(dlon / 2) ** 2
    )

    c = 2 * math.atan2(
        math.sqrt(a),
        math.sqrt(1 - a)
    )

    return R * c


# ============================================================
# OSM → map.txt
# ============================================================

def convert_osm(input_file, output_file):

    print("=" * 60)
    print("OSM → map.txt")
    print("=" * 60)

    print(f"正在读取：{input_file}")

    # --------------------------------------------------------
    # 读取 XML
    # --------------------------------------------------------

    tree = ET.parse(input_file)
    root = tree.getroot()

    # --------------------------------------------------------
    # 第一阶段
    # 读取所有 OSM node
    #
    # osm_nodes:
    #
    # OSM ID → (longitude, latitude)
    # --------------------------------------------------------

    osm_nodes = {}

    for element in root:

        if element.tag != "node":
            continue

        osm_id = int(element.attrib["id"])

        lat = float(element.attrib["lat"])
        lon = float(element.attrib["lon"])

        osm_nodes[osm_id] = (lon, lat)

    print(f"OSM 原始节点：{len(osm_nodes)}")

    # --------------------------------------------------------
    # 第二阶段
    # 读取道路 way
    # --------------------------------------------------------

    ways = []

    for element in root:

        if element.tag != "way":
            continue

        tags = {}
        node_refs = []

        for child in element:

            # 道路使用了哪些节点
            if child.tag == "nd":

                ref = int(child.attrib["ref"])

                node_refs.append(ref)

            # 读取 tag
            elif child.tag == "tag":

                key = child.attrib.get("k")
                value = child.attrib.get("v")

                tags[key] = value

        # highway 类型
        highway_type = tags.get("highway")

        # 不是我们需要的道路
        if highway_type not in ALLOWED_HIGHWAYS:
            continue

        # 至少需要两个节点才能形成道路
        if len(node_refs) < 2:
            continue

        ways.append(node_refs)

    print(f"保留道路数量：{len(ways)}")

    # --------------------------------------------------------
    # 第三阶段
    # 找出真正参与道路网络的节点
    # --------------------------------------------------------

    used_nodes = set()

    for node_refs in ways:

        for node_id in node_refs:

            if node_id in osm_nodes:
                used_nodes.add(node_id)

    print(f"道路节点数量：{len(used_nodes)}")

    # --------------------------------------------------------
    # 第四阶段
    # 重新编号
    #
    # OSM ID 可能是：
    #
    # 123456789
    # 987654321
    #
    # 我们转换成：
    #
    # 1
    # 2
    # 3
    #
    # 方便 C 程序使用数组
    # --------------------------------------------------------

    id_mapping = {}

    for new_id, old_id in enumerate(
        sorted(used_nodes),
        start=1
    ):

        id_mapping[old_id] = new_id

    # --------------------------------------------------------
    # 第五阶段
    # 建立道路边
    #
    # 默认全部双向
    # --------------------------------------------------------

    edges = []

    for node_refs in ways:

        for i in range(len(node_refs) - 1):

            old_from = node_refs[i]
            old_to = node_refs[i + 1]

            # OSM 中可能存在异常引用
            if old_from not in osm_nodes:
                continue

            if old_to not in osm_nodes:
                continue

            # 经纬度
            lon1, lat1 = osm_nodes[old_from]
            lon2, lat2 = osm_nodes[old_to]

            # 计算道路长度
            distance = haversine(
                lon1,
                lat1,
                lon2,
                lat2
            )

            # 转换成新的节点编号
            new_from = id_mapping[old_from]
            new_to = id_mapping[old_to]

            # ------------------------------------------------
            # 默认双向
            # ------------------------------------------------

            edges.append(
                (new_from, new_to, distance)
            )

            edges.append(
                (new_to, new_from, distance)
            )

    print(f"原始边数量：{len(edges)}")

    # --------------------------------------------------------
    # 第六阶段
    # 删除重复边
    # --------------------------------------------------------

    unique_edges = {}

    for u, v, distance in edges:

        key = (u, v)

        # 同一方向重复出现
        # 保留较短的一条
        if key not in unique_edges:

            unique_edges[key] = distance

        else:

            unique_edges[key] = min(
                unique_edges[key],
                distance
            )

    edges = [
        (u, v, distance)
        for (u, v), distance
        in unique_edges.items()
    ]

    print(f"去重后边数量：{len(edges)}")

    # --------------------------------------------------------
    # 第七阶段
    # 输出 map.txt
    # --------------------------------------------------------

    print(f"正在生成：{output_file}")

    with open(
        output_file,
        "w",
        encoding="utf-8"
    ) as f:

        # 第一行：
        # 节点数量 边数量

        f.write(
            f"{len(id_mapping)} {len(edges)}\n"
        )

        # ----------------------------------------------------
        # 节点
        # ----------------------------------------------------

        for old_id, new_id in sorted(
            id_mapping.items(),
            key=lambda x: x[1]
        ):

            lon, lat = osm_nodes[old_id]

            f.write(
                f"{new_id} "
                f"{lon:.8f} "
                f"{lat:.8f}\n"
            )

        # ----------------------------------------------------
        # 边
        # ----------------------------------------------------

        for u, v, distance in edges:

            f.write(
                f"{u} {v} {distance:.2f}\n"
            )

    print()
    print("=" * 60)
    print("转换完成！")
    print("=" * 60)

    print(f"节点数量：{len(id_mapping)}")
    print(f"边数量：{len(edges)}")
    print(f"输出文件：{output_file}")


# ============================================================
# 程序入口
# ============================================================

def main():

    # 必须提供：
    #
    # python osm_to_map.py 输入.osm 输出.txt

    if len(sys.argv) != 3:

        print()
        print("使用方法：")
        print()
        print("python osm_to_map.py input.osm output.txt")
        print()

        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    try:

        convert_osm(
            input_file,
            output_file
        )

    except FileNotFoundError:

        print()
        print("错误：找不到输入文件。")
        print(f"请检查：{input_file}")
        sys.exit(1)

    except ET.ParseError:

        print()
        print("错误：OSM 文件不是合法的 XML 文件。")
        sys.exit(1)

    except Exception as e:

        print()
        print("转换过程中发生错误：")
        print(e)
        sys.exit(1)


if __name__ == "__main__":
    main()