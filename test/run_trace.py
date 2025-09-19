#!/usr/bin/env python3
import os
import sys
import subprocess
import argparse
import shutil
from pathlib import Path

def main():
    # 设置命令行参数解析
    parser = argparse.ArgumentParser(description='Trace Processing Pipeline')
    parser.add_argument('-layout', required=True, help='Path to layout file')
    parser.add_argument('-rule', required=True, help='Path to rule file')
    parser.add_argument('-thread', type=int, help='Number of threads for parallel computation')
    parser.add_argument('-output', required=True, help='Path to output result file')
    parser.add_argument('-ans', required=True, help='Path to answer file')
    args = parser.parse_args()

    # 获取项目根目录
    project_root = Path(__file__).parent.parent.resolve()
    
    # 清空并重新创建build目录
    build_dir = project_root / 'build'
    print(f"🧹 === Cleaning build directory: {build_dir} ===")
    if build_dir.exists():
        shutil.rmtree(build_dir)
    build_dir.mkdir(exist_ok=True)
    os.chdir(build_dir)
    
    # 执行CMake和Make
    print("🔧 === Configuring and building project ===")
    subprocess.run(['cmake', '..'], check=True)
    subprocess.run(['make'], check=True)
    print("✅ Build completed successfully!")
    
    # 构建trace命令，确保-thread参数在-rule后面
    trace_cmd = [
        './trace',
        '-layout', str(Path(args.layout).resolve()),
        '-rule', str(Path(args.rule).resolve())
    ]
    
    # 添加线程参数（如果提供），放在-rule后面
    if args.thread:
        trace_cmd.extend(['-thread', str(args.thread)])
    
    # 添加-output参数
    trace_cmd.extend(['-output', str(Path(args.output).resolve())])
    
    # 运行trace程序
    print("\n🚀 === Running trace program ===")
    print("🔧 Command:", ' '.join(trace_cmd))
    subprocess.run(trace_cmd, check=True)
    print("✅ Trace execution completed!")
    
    # 切换到答案检查器目录
    checker_dir = project_root / 'trace_answer_checker'
    os.chdir(checker_dir)
    
    # 运行答案检查器
    print("\n🔍 === Running answer checker ===")
    checker_cmd = [
        './checker',
        '-ans', str(Path(args.ans).resolve()),
        '-res', str(Path(args.output).resolve())
    ]
    print("🔧 Command:", ' '.join(checker_cmd))
    subprocess.run(checker_cmd, check=True)
    print("✅ Answer check completed!")

if __name__ == '__main__':
    try:
        main()
        print("\n🎉 === Processing completed successfully ===")
    except subprocess.CalledProcessError as e:
        print(f"\n❌ !!! Error occurred during processing: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ !!! Unexpected error: {e}")
        sys.exit(2)