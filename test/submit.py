#!/usr/bin/env python3
"""
submit.py - 打包脚本
编译项目并将生成的可执行文件打包成zip
"""

import os
import sys
import zipfile
import argparse
import subprocess
import shutil
from pathlib import Path

def compile_project(base_dir):
    """编译项目"""
    build_dir = base_dir / "build"

    # 删除旧的build目录
    if build_dir.exists():
        print(f"删除旧的构建目录: {build_dir}")
        shutil.rmtree(build_dir)

    # 创建新的build目录
    build_dir.mkdir()
    print(f"✓ 创建新的构建目录: {build_dir}")

    try:
        # 进入build目录
        original_dir = os.getcwd()
        os.chdir(build_dir)

        # 运行cmake
        print("运行 cmake ...")
        result = subprocess.run(["cmake", ".."], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"✗ cmake 失败: {result.stderr}")
            return False
        print("✓ cmake 成功")

        # 运行make
        print("运行 make ...")
        result = subprocess.run(["make", "-j4"], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"✗ make 失败: {result.stderr}")
            return False
        print("✓ make 成功")

        # 返回原目录
        os.chdir(original_dir)
        return True

    except Exception as e:
        print(f"✗ 编译过程出错: {e}")
        # 确保返回原目录
        os.chdir(original_dir)
        return False

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

def find_executable(bin_dir):
    """在bin目录中查找可执行文件"""
    if not bin_dir.exists():
        print(f"⚠ 警告: {bin_dir} 目录不存在")
        return []

    executables = []
    # 查找所有可执行文件
    for file_path in bin_dir.iterdir():
        if file_path.is_file() and os.access(file_path, os.X_OK):
            executables.append(file_path)

    return executables

def create_zip(zip_filename, base_dir, submit_dir, thread_num):
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
            # 1. 添加可执行文件
            bin_dir = base_dir / "bin"
            executables = find_executable(bin_dir)

            if executables:
                for exe in executables:
                    # 可执行文件放在压缩包根目录
                    arcname = exe.name
                    zipf.write(exe, arcname)
                    print(f"✓ 添加可执行文件: {arcname}")
            else:
                print("⚠ 警告: 未找到可执行文件")

            # 2. 添加线程数文件
            thread_file = submit_dir / "thread.txt"
            if thread_file.exists():
                arcname = "thread.txt"
                zipf.write(thread_file, arcname)
                print(f"✓ 添加: {arcname}")

            # 3. 添加 submit 文件夹下的其他非.zip文件
            if submit_dir.exists():
                for file_path in submit_dir.iterdir():
                    # 排除.zip文件（避免包含自己）和thread.txt（已单独处理）
                    if (file_path.is_file() and
                        file_path.suffix != '.zip' and
                        file_path.name != 'thread.txt'):
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
    parser = argparse.ArgumentParser(description='编译并打包项目')
    parser.add_argument('zip_name', help='压缩文件名（必需）')
    parser.add_argument('thread_num', nargs='?', type=int, default=1,
                       help='线程数（默认: 1）')

    args = parser.parse_args()

    # 获取路径
    current_dir = Path(__file__).parent  # test/ 文件夹
    base_dir = current_dir.parent        # 项目根目录
    submit_dir = base_dir / "submit"

    print("=" * 50)
    print("开始编译和打包操作")
    print(f"压缩文件名: {args.zip_name}")
    print(f"线程数: {args.thread_num}")
    print(f"项目根目录: {base_dir}")
    print(f"提交目录: {submit_dir}")
    print("=" * 50)

    # 检查 submit 目录是否存在
    if not submit_dir.exists():
        print(f"✗ 错误: 提交目录 {submit_dir} 不存在")
        sys.exit(1)

    # 1. 编译项目
    print("步骤 1/3: 编译项目...")
    if not compile_project(base_dir):
        print("✗ 编译失败，请检查项目配置")
        sys.exit(1)

    # 2. 写入线程数
    print("步骤 2/3: 写入线程数配置...")
    if not write_thread_number(args.thread_num, submit_dir):
        sys.exit(1)

    # 3. 创建压缩包
    print("步骤 3/3: 创建压缩包...")
    if not create_zip(args.zip_name, base_dir, submit_dir, args.thread_num):
        sys.exit(1)

    print("=" * 50)
    print("编译和打包操作完成！")
    print("=" * 50)

if __name__ == "__main__":
    main()