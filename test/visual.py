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

def visualize_polygons(data, show_labels=False):
    """
    可视化多层多边形
    show_labels: 是否在多边形内部显示层名和编号
    """
    if not data:
        print("No valid polygon data found.")
        return
        
    fig, ax = plt.subplots(figsize=(12, 10))
    ax.set_aspect('equal')
    ax.grid(True)
    ax.set_title('Multi-layer Polygons Visualization')
    
    # 为不同层生成颜色
    layers = list(data.keys())
    colors = plt.cm.tab10(np.linspace(0, 1, len(layers)))
    
    # 创建图例的代理对象
    legend_proxies = []
    
    # 绘制每个多边形
    for i, layer in enumerate(layers):
        patches = []
        for j, vertices in enumerate(data[layer]):
            polygon = Polygon(vertices, closed=True)
            patches.append(polygon)
            
            # 如果需要显示标签
            if show_labels:
                # 找到合适的文本位置
                text_x, text_y = find_text_position(vertices)
                
                # 创建文本内容（层名和编号）
                text_content = f"{layer}\n#{j+1}"
                
                # 添加文本
                ax.text(
                    text_x, text_y, 
                    text_content, 
                    fontsize=10, 
                    fontweight='bold',
                    ha='center', 
                    va='center',
                    bbox=dict(boxstyle='round', 
                              facecolor='white', 
                              edgecolor='black', 
                              alpha=0.7,
                              pad=0.5)
                )
        
        # 创建带透明度的集合
        collection = PatchCollection(
            patches,
            facecolor=colors[i],
            alpha=0.5,          # 50% 透明度
            edgecolor='black',  # 黑色边界
            linewidth=1.5       # 边界线宽
        )
        ax.add_collection(collection)
        
        # 为图例创建代理对象
        proxy = Rectangle((0, 0), 1, 1, fc=colors[i], alpha=0.5, ec='black', linewidth=1.5)
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
        padding = max((x_max - x_min), (y_max - y_min)) * 0.1  # 10% padding
        ax.set_xlim(x_min - padding, x_max + padding)
        ax.set_ylim(y_min - padding, y_max + padding)
    
    # 添加图例（始终显示）
    ax.legend(legend_proxies, layers, loc='best', title="Layers")
    
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Visualize multi-layer polygons from a file.')
    parser.add_argument('filename', type=str, help='Path to the polygon data file')
    parser.add_argument('--labels', '-l', action='store_true', 
                        help='Show layer names and polygon numbers inside each polygon')
    
    args = parser.parse_args()
    
    try:
        # 解析文件并可视化
        polygon_data = parse_polygon_file(args.filename)
        if polygon_data:
            visualize_polygons(polygon_data, show_labels=args.labels)
        else:
            print("No valid polygon data found in the file.")
    except FileNotFoundError:
        print(f"Error: File '{args.filename}' not found.")
    except Exception as e:
        print(f"An error occurred: {str(e)}")