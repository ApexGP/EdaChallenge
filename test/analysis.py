import argparse
import csv
import re
from pathlib import Path

REPEAT_SUFFIX_RE = re.compile(r'_r(\d+)$')
THREAD_SUFFIX_RE = re.compile(r'_t(\d+)(?:_r\d+)?$')

TIME_COLUMNS = [
    'Trace_Wall_Time',
    'Pipeline_Time',
    'Total_Time',
    'Input_Time',
    'Space_Index_Time',
    'Lazy_BFS_Time',
    'Output_Time',
    'Merge_Cutting_Time',
]

CSV_COLUMNS = ['文件名', '用例', '重复序号', '线程数'] + TIME_COLUMNS


def parse_log_name(stem):
    repeat_index = ''
    repeat_match = REPEAT_SUFFIX_RE.search(stem)
    if repeat_match:
        repeat_index = int(repeat_match.group(1))
        stem_without_repeat = stem[:repeat_match.start()]
    else:
        stem_without_repeat = stem

    thread = ''
    thread_match = THREAD_SUFFIX_RE.search(stem)
    if thread_match:
        thread = int(thread_match.group(1))
        suffix_pos = stem_without_repeat.rfind(f'_t{thread}')
        case_name = stem_without_repeat[:suffix_pos] if suffix_pos != -1 else stem_without_repeat
    else:
        case_name = stem_without_repeat
    return case_name, thread, repeat_index


def parse_float(pattern, content, default=None):
    match = re.search(pattern, content, re.DOTALL)
    return float(match.group(1)) if match else default


def parse_thread(content, thread_from_name):
    command_match = re.search(r'Command:.*?(?:^|\s)-thread\s+(\d+)(?:\s|$)', content)
    if command_match:
        return int(command_match.group(1))
    if thread_from_name:
        return thread_from_name
    if re.search(r'Command:.*?\btrace(?:\.exe)?\b', content):
        return 1
    return ''


def extract_info_from_txt(file_path):
    file_path = Path(file_path).resolve()
    content = file_path.read_text(encoding='utf-8')
    case_name, thread_from_name, repeat_index = parse_log_name(file_path.stem)

    return {
        '文件名': file_path.stem,
        '用例': case_name,
        '重复序号': repeat_index,
        '线程数': parse_thread(content, thread_from_name),
        'Trace_Wall_Time': parse_float(r'Trace execution time:\s*([\d.e+-]+)\s*seconds', content),
        'Pipeline_Time': parse_float(r'Total pipeline time:\s*([\d.e+-]+)\s*seconds', content),
        'Total_Time': parse_float(r'----- Total Time:\s*([\d.e+-]+)\s*s', content),
        'Input_Time': parse_float(r'----- Starting Input -----.*?Use Time:\s*([\d.e+-]+)\s*s', content),
        'Space_Index_Time': parse_float(r'----- Starting Space Index -----.*?Use Time:\s*([\d.e+-]+)\s*s', content),
        'Lazy_BFS_Time': parse_float(r'----- Starting Lazy BFS.*?Use Time:\s*([\d.e+-]+)\s*s', content),
        'Output_Time': parse_float(r'----- Starting Output -----.*?Use Time:\s*([\d.e+-]+)\s*s', content),
        'Merge_Cutting_Time': parse_float(r'----- Starting Merge and Cutting Polygon -----.*?Use Time:\s*([\d.e+-]+)\s*s', content),
    }


def collect_txt_files(paths, recursive=False):
    txt_files = []
    for raw_path in paths:
        path = Path(raw_path).expanduser()
        if not path.is_absolute():
            path = Path.cwd() / path
        path = path.resolve()

        if path.is_file():
            if path.suffix.lower() == '.txt':
                txt_files.append(path)
            continue
        if path.is_dir():
            txt_files.extend(sorted(path.glob('**/*.txt' if recursive else '*.txt')))
            continue
        print(f"路径不存在，已跳过: {path}")
    return sorted(dict.fromkeys(txt_files))


def numeric_values(rows, column):
    values = []
    for row in rows:
        value = row.get(column)
        if value in (None, ''):
            continue
        values.append(float(value))
    return values


def append_average_row(rows):
    avg_row = {column: '' for column in CSV_COLUMNS}
    first = rows[0]
    avg_row['文件名'] = 'AVG'
    avg_row['用例'] = first.get('用例', '')
    avg_row['线程数'] = first.get('线程数', '')
    avg_row['重复序号'] = 'AVG'

    for column in TIME_COLUMNS:
        values = numeric_values(rows, column)
        if values:
            avg_row[column] = sum(values) / len(values)

    return rows + [avg_row]


def write_csv(csv_file, rows):
    csv_file.parent.mkdir(parents=True, exist_ok=True)
    with csv_file.open('w', encoding='utf-8-sig', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=CSV_COLUMNS, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description='Extract per-round trace timing metrics into one CSV.')
    parser.add_argument('paths', nargs='*', default=['.'],
                        help='txt files or directories to scan. Default: current directory')
    parser.add_argument('-r', '--recursive', action='store_true',
                        help='Recursively scan txt files when a directory is provided')
    parser.add_argument('-o', '--output', default='result.csv',
                        help='Output CSV path. Default: result.csv in current directory')
    args = parser.parse_args()

    txt_files = collect_txt_files(args.paths, recursive=args.recursive)
    if not txt_files:
        print("未找到txt文件")
        return

    rows = []
    for txt_file in txt_files:
        try:
            rows.append(extract_info_from_txt(txt_file))
        except Exception as exc:
            print(f"处理文件 {txt_file} 时发生错误: {exc}")

    if not rows:
        print("没有成功提取到任何信息")
        return

    rows = append_average_row(rows)
    csv_file = Path(args.output).expanduser()
    if not csv_file.is_absolute():
        csv_file = Path.cwd() / csv_file
    csv_file = csv_file.resolve()
    write_csv(csv_file, rows)

    print(f"处理完成，共处理日志数: {len(txt_files)}")
    print(f"输出文件: {csv_file}")


if __name__ == "__main__":
    main()