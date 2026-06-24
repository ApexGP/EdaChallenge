#!/usr/bin/env python3
import sys
import subprocess
import argparse
import shutil
import time
from pathlib import Path

def resolve_existing_path(raw_path, label, project_root, original_cwd):
    """Resolve an input path before the script changes working directories."""
    path = Path(raw_path).expanduser()
    candidates = [path] if path.is_absolute() else [original_cwd / path, project_root / path]
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    searched = ', '.join(str(candidate) for candidate in candidates)
    raise FileNotFoundError(f"{label} not found: {raw_path}. Searched: {searched}")

def resolve_output_path(raw_path, project_root, original_cwd):
    """Resolve an output path, preferring an existing parent directory."""
    path = Path(raw_path).expanduser()
    if path.is_absolute():
        resolved = path
    else:
        candidates = [original_cwd / path, project_root / path]
        resolved = next((candidate for candidate in candidates if candidate.parent.exists()), candidates[0])
    resolved = resolved.resolve()
    resolved.parent.mkdir(parents=True, exist_ok=True)
    return resolved

def find_executable(candidates, label):
    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return candidate.resolve()
    searched = ', '.join(str(candidate) for candidate in candidates)
    raise FileNotFoundError(f"{label} executable not found. Searched: {searched}")

def find_optional_executable(candidates):
    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return candidate.resolve()
    return None

def print_checker_diagnostics(checker_exe):
    print("⚠️  Checker file exists but could not be executed.")
    print("⚠️  On Linux this often means the ELF interpreter or a dynamic library is missing.")
    if not sys.platform.startswith('win'):
        if shutil.which('file'):
            subprocess.run(['file', str(checker_exe)], check=False)
        if shutil.which('ldd'):
            subprocess.run(['ldd', str(checker_exe)], check=False)
    print("⚠️  You can rerun with --skip-check, or install/copy the missing checker dependencies.")

def build_project(project_root, args):
    if args.skip_build:
        print("⏭️  === Skipping build step ===")
        return

    if args.build_preset:
        print(f"🔧 === Configuring and building preset: {args.build_preset} ===")
        subprocess.run(['cmake', '--preset', args.build_preset], cwd=project_root, check=True)
        subprocess.run(['cmake', '--build', '--preset', args.build_preset], cwd=project_root, check=True)
        print("✅ Build completed successfully!")
        return

    build_dir = project_root / 'build'
    print(f"🧹 === Cleaning build directory: {build_dir} ===")
    if build_dir.exists():
        shutil.rmtree(build_dir)
    build_dir.mkdir(exist_ok=True)

    print("🔧 === Configuring and building project ===")
    subprocess.run(['cmake', '..'], cwd=build_dir, check=True)
    subprocess.run(['cmake', '--build', '.'], cwd=build_dir, check=True)
    print("✅ Build completed successfully!")

def main():
    # 设置命令行参数解析
    parser = argparse.ArgumentParser(description='Trace Processing Pipeline')
    parser.add_argument('-layout', required=True, help='Path to layout file')
    parser.add_argument('-rule', required=True, help='Path to rule file')
    parser.add_argument('-thread', type=int, help='Number of threads for parallel computation')
    parser.add_argument('-output', required=True, help='Path to output result file')
    parser.add_argument('-ans', help='Path to answer file')
    parser.add_argument('--build-preset', help='CMake preset to configure/build, e.g. gcc-release or msvc-release')
    parser.add_argument('--skip-build', action='store_true', help='Skip build and use existing bin/trace[.exe]')
    parser.add_argument('--skip-check', action='store_true', help='Skip checker validation')
    args = parser.parse_args()
    pipeline_start = time.perf_counter()

    # 获取项目根目录
    project_root = Path(__file__).parent.parent.resolve()
    original_cwd = Path.cwd().resolve()

    # 在任何 chdir/build 之前解析路径，避免相对路径被 build/bin/checker 工作目录污染
    layout_path = resolve_existing_path(args.layout, 'layout', project_root, original_cwd)
    rule_path = resolve_existing_path(args.rule, 'rule', project_root, original_cwd)
    if not args.skip_check and not args.ans:
        parser.error('-ans is required unless --skip-check is specified')
    ans_path = resolve_existing_path(args.ans, 'answer', project_root, original_cwd) if args.ans else None
    output_path = resolve_output_path(args.output, project_root, original_cwd)

    build_project(project_root, args)

    trace_exe = find_executable(
        [project_root / 'bin' / 'trace.exe', project_root / 'bin' / 'trace'],
        'trace'
    )

    # 构建trace命令，确保-thread参数在-rule后面
    trace_cmd = [
        str(trace_exe),
        '-layout', str(layout_path),
        '-rule', str(rule_path)
    ]

    # 添加线程参数（如果提供），放在-rule后面
    if args.thread:
        trace_cmd.extend(['-thread', str(args.thread)])

    # 添加-output参数
    trace_cmd.extend(['-output', str(output_path)])

    # 运行trace程序
    print("\n🚀 === Running trace program ===")
    print("🔧 Command:", ' '.join(trace_cmd))
    trace_start = time.perf_counter()
    subprocess.run(trace_cmd, check=True)
    trace_elapsed = time.perf_counter() - trace_start
    print("✅ Trace execution completed!")
    print(f"⏱️ Trace execution time: {trace_elapsed:.6f} seconds")

    if args.skip_check:
        print("⏭️  === Skipping answer checker ===")
        print(f"⏱️ Total pipeline time: {time.perf_counter() - pipeline_start:.6f} seconds")
        return

    # 查找答案检查器。自有服务器上可能只跑 trace，不一定同步 checker 二进制。
    checker_dir = project_root / 'checker'
    checker_candidates = [
        checker_dir / 'checker.exe',
        checker_dir / 'checker',
        checker_dir / 'AnswerChecker.exe',
        checker_dir / 'AnswerChecker',
    ]
    checker_exe = find_optional_executable(checker_candidates)
    if checker_exe is None:
        searched = ', '.join(str(candidate) for candidate in checker_candidates)
        print("\n⚠️  === Answer checker not found; skipping validation ===")
        print(f"⚠️  Searched: {searched}")
        print("⚠️  If you need correctness validation, copy checker/AnswerChecker to this server or run checker manually.")
        print(f"⏱️ Total pipeline time: {time.perf_counter() - pipeline_start:.6f} seconds")
        return

    if not sys.platform.startswith('win'):
        try:
            checker_exe.chmod(checker_exe.stat().st_mode | 0o111)
        except OSError as exc:
            print(f"⚠️  Could not chmod checker executable: {exc}")

    answer_checker = checker_dir / 'AnswerChecker'
    if not sys.platform.startswith('win') and answer_checker.exists():
        try:
            answer_checker.chmod(answer_checker.stat().st_mode | 0o111)
        except OSError as exc:
            print(f"⚠️  Could not chmod AnswerChecker executable: {exc}")

    # 运行答案检查器
    print("\n🔍 === Running answer checker ===")
    checker_cmd = [
        str(checker_exe),
        '-ans', str(ans_path),
        '-res', str(output_path)
    ]
    print("🔧 Command:", ' '.join(checker_cmd))
    checker_start = time.perf_counter()
    try:
        subprocess.run(checker_cmd, cwd=checker_dir, check=True)
    except FileNotFoundError as exc:
        print(f"\n⚠️  === Answer checker could not start; skipping validation ===")
        print(f"⚠️  {exc}")
        print_checker_diagnostics(checker_exe)
        print(f"⏱️ Total pipeline time: {time.perf_counter() - pipeline_start:.6f} seconds")
        return
    except PermissionError as exc:
        print(f"\n⚠️  === Answer checker is not executable; skipping validation ===")
        print(f"⚠️  {exc}")
        print_checker_diagnostics(checker_exe)
        print(f"⏱️ Total pipeline time: {time.perf_counter() - pipeline_start:.6f} seconds")
        return
    checker_elapsed = time.perf_counter() - checker_start
    print("✅ Answer check completed!")
    print(f"⏱️ Checker execution time: {checker_elapsed:.6f} seconds")
    print(f"⏱️ Total pipeline time: {time.perf_counter() - pipeline_start:.6f} seconds")

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