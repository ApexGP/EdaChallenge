#!/usr/bin/env python3
"""
submit.py - 打包脚本
将 ../trace 文件夹、../CMakeLists.txt 和 ../submit 下的非.zip文件打包成 zip
submit文件夹下的文件直接放在压缩包根目录，不包含submit文件夹
"""

import os
import sys
import zipfile
import argparse
from pathlib import Path

def write_thread_number(thread_num, submit_dir):
    """将线程数写入 thread.txt 文件"""
    thread_file = submit_dir / "thread.txt"
    try:
        with open(thread_file, 'w') as f:
            f.write(str(thread_num))
        print(f"✓ 已将线程数 {thread_num} 写入 {thread_file}")
        return True
    except Exception as e:
        print(f"✗ 写入 thread.txt 失败: {e}")
        return False

def create_zip(zip_filename, base_dir, submit_dir):
    """创建压缩包"""
    # 确保 zip 文件名有 .zip 扩展名
    if not zip_filename.endswith('.zip'):
        zip_filename += '.zip'
    
    zip_path = submit_dir / zip_filename
    
    # 检查是否已存在同名文件
    if zip_path.exists():
        response = input(f"文件 {zip_path} 已存在，是否覆盖？(y/N): ")
        if response.lower() != 'y':
            print("操作已取消")
            return False
    
    try:
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            # 添加 trace 文件夹
            trace_dir = base_dir / "trace"
            if trace_dir.exists():
                for file_path in trace_dir.rglob('*'):
                    if file_path.is_file() and not str(file_path).endswith(".zip"):
                        # 在 zip 中保持相对路径结构
                        arcname = file_path.relative_to(base_dir)
                        zipf.write(file_path, arcname)
                        print(f"✓ 添加: {arcname}")
            else:
                print(f"⚠ 警告: {trace_dir} 不存在")
            
            # 添加 CMakeLists.txt
            cmake_file = base_dir / "CMakeLists.txt"
            if cmake_file.exists():
                arcname = cmake_file.relative_to(base_dir)
                zipf.write(cmake_file, arcname)
                print(f"✓ 添加: {arcname}")
            else:
                print(f"⚠ 警告: {cmake_file} 不存在")
            
            # 添加 submit 文件夹下的非.zip文件（直接放在压缩包根目录）
            if submit_dir.exists():
                for file_path in submit_dir.iterdir():
                    # 排除.zip文件（避免包含自己）
                    if file_path.is_file() and file_path.suffix != '.zip':
                        # 文件直接放在zip根目录，不包含submit文件夹
                        arcname = file_path.name
                        zipf.write(file_path, arcname)
                        print(f"✓ 添加: {arcname}")
            
        print(f"✓ 压缩包创建成功: {zip_path}")
        print(f"✓ 压缩包大小: {zip_path.stat().st_size / 1024:.2f} KB")
        
        # 显示压缩包内容
        print("\n压缩包内容:")
        with zipfile.ZipFile(zip_path, 'r') as zipf:
            for name in zipf.namelist():
                print(f"  - {name}")
                
        return True
        
    except Exception as e:
        print(f"✗ 创建压缩包失败: {e}")
        return False

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='打包提交文件')
    parser.add_argument('zip_name', help='压缩文件名（必需）')
    parser.add_argument('thread_num', nargs='?', type=int, default=1, 
                       help='线程数（默认: 1）')
    
    args = parser.parse_args()
    
    # 获取路径
    current_dir = Path(__file__).parent  # test/ 文件夹
    base_dir = current_dir.parent        # 项目根目录
    submit_dir = base_dir / "submit"
    
    print("=" * 50)
    print("开始打包操作")
    print(f"压缩文件名: {args.zip_name}")
    print(f"线程数: {args.thread_num}")
    print(f"项目根目录: {base_dir}")
    print(f"提交目录: {submit_dir}")
    print("=" * 50)
    
    # 检查 submit 目录是否存在
    if not submit_dir.exists():
        print(f"✗ 错误: 提交目录 {submit_dir} 不存在")
        sys.exit(1)
    
    # 1. 写入线程数
    if not write_thread_number(args.thread_num, submit_dir):
        sys.exit(1)
    
    # 2. 创建压缩包
    if not create_zip(args.zip_name, base_dir, submit_dir):
        sys.exit(1)
    
    print("=" * 50)
    print("打包操作完成！")
    print("=" * 50)

if __name__ == "__main__":
    main()