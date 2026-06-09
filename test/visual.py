import matplotlib.pyplot as plt
from matplotlib.collections import PatchCollection
from matplotlib.patches import Polygon, Rectangle
import numpy as np
import argparse
import re

def parse_polygon_file(filename):
    """
    解析多边形文件格式，支持任意层名
    格式要求：
    - 层名独立一行
    - 每个多边形一行
    - 多边形由多个(x,y)点组成
    """
    data = {}
    current_layer = None

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            # 检查是否是层标识（非空行且不包含括号）
            if '(' not in line and ')' not in line:
                current_layer = line
                if current_layer not in data:
                    data[current_layer] = []
                continue

            # 解析多边形顶点
            if current_layer:
                # 匹配所有 (x,y) 对
                points = re.findall(r'\(([-+]?\d*\.?\d+)\s*,\s*([-+]?\d*\.?\d+)\)', line)
                if points:
                    polygon = [(float(x), float(y)) for x, y in points]
                    data[current_layer].append(polygon)

    return data

def find_text_position(vertices):
    """为哈曼顿多边形找到合适的文本位置（内部）"""
    # 计算多边形的边界框
    xs = [v[0] for v in vertices]
    ys = [v[1] for v in vertices]
    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)

    # 尝试找到内部点（避免边界）
    center_x = (min_x + max_x) / 2
    center_y = (min_y + max_y) / 2

    return (center_x, center_y)

def calculate_polygon_area(vertices):
    """使用鞋带公式计算多边形面积"""
    n = len(vertices)
    area = 0.0
    for i in range(n):
        j = (i + 1) % n
        area += vertices[i][0] * vertices[j][1]
        area -= vertices[j][0] * vertices[i][1]
    return abs(area) / 2.0

def visualize_polygons(title, data, show_labels=False, save_file=None):
    if not data:
        print("No valid polygon data found.")
        return

    # 高对比度颜色方案
    high_contrast_colors = [
        '#FF0000', '#0000FF', '#00FF00', '#FF00FF',  # 红、蓝、绿、品红
        '#FFFF00', '#00FFFF', '#FF8000', '#8000FF',  # 黄、青、橙、紫
        '#008000', '#000080', '#800000', '#808000',  # 深绿、深蓝、深红、橄榄绿
        '#008080', '#800080', '#808080', '#C0C0C0'   # 青蓝、紫红、灰、银
    ]

    # 创建图形
    fig, ax = plt.subplots(figsize=(14, 12))
    ax.set_aspect('equal')

    # 设置高对比度背景和网格
    ax.set_facecolor('#FFFFFF')  # 白色背景
    ax.grid(color='#AAAAAA', linestyle='--', linewidth=0.5, alpha=0.5)  # 浅灰网格

    # 设置高对比度文本颜色
    ax.set_title(title + ' Polygons Visualization', color='#000000', fontsize=14, fontweight='bold')
    ax.tick_params(colors='#000000')

    # 为不同层分配颜色
    layers = list(data.keys())
    colors = high_contrast_colors * (len(layers) // len(high_contrast_colors) + 1)
    colors = colors[:len(layers)]

    # 创建图例的代理对象
    legend_proxies = []

    # 按层深度排序（从深到浅绘制） 多边形多的在浅层（因为小而多）
    # sorted_layers = sorted(layers, key=lambda l: len(data[l]))
    sorted_layers = layers  # 保持原始顺序

    # 绘制每个多边形（从大到小，从深到浅）
    for i, layer in enumerate(sorted_layers):
        patches = []
        layer_polygons = data[layer]

        # 按多边形面积排序（从大到小）
        layer_polygons.sort(
            key=lambda poly: calculate_polygon_area(poly),
            reverse=True
        )

        for j, vertices in enumerate(layer_polygons):
            polygon = Polygon(vertices, closed=True)
            patches.append(polygon)

            # 如果需要显示标签
            if show_labels:
                # 找到合适的文本位置
                text_x, text_y = find_text_position(vertices)

                # 创建文本内容（层名和编号）
                text_content = f"{layer}\n#{j+1}"

                # 添加高对比度文本
                ax.text(
                    text_x, text_y,
                    text_content,
                    fontsize=6,
                    fontweight='bold',
                    color='#000000',  # 黑色文本
                    ha='center',
                    va='center',
                    # bbox=dict(
                    #     boxstyle='round',
                    #     facecolor='#FFFFFF',  # 白色背景
                    #     edgecolor='#000000',  # 黑色边界
                    #     alpha=0.9,           # 高不透明度
                    #     pad=0.5
                    # )
                )

        # 创建多边形集合
        collection = PatchCollection(
            patches,
            facecolor=colors[i],
            alpha=0.4,          # 动态透明度
            # edgecolor='#000000',  # 黑色边界
            # linewidth=2,        # 边界线宽
            hatch=None            # 不使用填充图案（避免冲突）
        )
        ax.add_collection(collection)

        # 为图例创建代理对象
        proxy = Rectangle((0, 0), 1, 1, fc=colors[i], alpha=0.4, ec='#000000', linewidth=1.5)
        legend_proxies.append(proxy)

    # 自动调整坐标轴范围
    all_points = []
    for polygons in data.values():
        for poly in polygons:
            for point in poly:
                all_points.append(point)

    if all_points:
        all_points = np.array(all_points)
        x_min, y_min = np.min(all_points, axis=0)
        x_max, y_max = np.max(all_points, axis=0)
        padding = max((x_max - x_min), (y_max - y_min)) * 0.15
        ax.set_xlim(x_min - padding, x_max + padding)
        ax.set_ylim(y_min - padding, y_max + padding)

    # 统一刻度间隔
    x_min, x_max = ax.get_xlim()
    y_min, y_max = ax.get_ylim()
    range_size = max(x_max - x_min, y_max - y_min)
    tick_interval = range_size / 8
    ax.xaxis.set_major_locator(plt.MultipleLocator(tick_interval))
    ax.yaxis.set_major_locator(plt.MultipleLocator(tick_interval))

    # 添加高对比度图例
    ax.legend(
        legend_proxies,
        sorted_layers,
        loc='best',
        title="Layers",
        frameon=True,
        framealpha=1.0,
        edgecolor='#000000',
        fontsize=10
    )

    plt.tight_layout()
    if save_file:
        plt.savefig(save_file, dpi=300)
    else:
        plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Visualize multi-layer polygons from a file.')
    parser.add_argument('filename', type=str, help='Path to the polygon data file')
    parser.add_argument('--labels', '-l', action='store_true',
                        help='Show layer names and polygon numbers inside each polygon')
    parser.add_argument('--save', '-s', type=str, default=None,
                        help='Path to save the output image (e.g., output.png). If not provided, display on screen.')


    args = parser.parse_args()

    try:
        # 解析文件并可视化
        polygon_data = parse_polygon_file(args.filename)
        if polygon_data:
            title = args.filename.split('/')[-1].split('.')[0]
            visualize_polygons(title, polygon_data, show_labels=args.labels, save_file=args.save)
        else:
            print("No valid polygon data found in the file.")
    except FileNotFoundError:
        print(f"Error: File '{args.filename}' not found.")
    except Exception as e:
        print(f"An error occurred: {str(e)}")