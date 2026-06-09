import os
import re
import glob
import pandas as pd

def extract_info_from_txt(file_path):
    """
    从txt文件中提取信息，每个文件一行，多次执行的信息分开列
    """
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 提取文件名（不含扩展名）
    filename = os.path.splitext(os.path.basename(file_path))[0]

    # 初始化结果字典
    result = {'文件名': filename}

    # 1. 提取线程数
    thread_match = re.search(r'从线程配置文件读取到线程数:\s*(\d+)', content)
    if thread_match:
        result['线程数'] = int(thread_match.group(1))
    else:
        thread_match2 = re.search(r'准备运行编译后的C\+\+程序\d+次，使用(\d+)线程', content)
        result['线程数'] = int(thread_match2.group(1)) if thread_match2 else '未找到'

    # 2. 提取总执行时间
    total_time_match = re.search(r'总执行时间:\s*([\d.]+)秒', content)
    if total_time_match:
        result['总执行时间'] = float(total_time_match.group(1))
    else:
        total_time_match2 = re.search(r'提取的执行时间:\s*([\d.]+)\s*秒', content)
        result['总执行时间'] = float(total_time_match2.group(1)) if total_time_match2 else None

    # 3. 提取每次执行的详细时间（共6次）
    execution_times = re.findall(r'第(\d+)次执行时间:\s*([\d.]+)秒', content)
    for i, time in execution_times:
        result[f'第{i}次执行时间'] = float(time)

    # 4. 为每次运行提取各个步骤的时间
    # 分割每次运行的内容
    run_sections = re.split(r'第\d+次运行:', content)
    if run_sections and '第一步：运行ingestion.py' in run_sections[0]:
        run_sections = run_sections[1:]

    # 处理每次运行
    for i, run_content in enumerate(run_sections, 1):
        if i > 6:  # 只处理前6次运行
            break

        # 提取各个步骤的时间（针对本次运行）
        time_patterns = {
            'Input_Time': r'----- Starting Input -----.*?Use Time:\s*([\d.e+-]+)\s*s',
            'Space_Index_Time': r'----- Starting Space Index -----.*?Use Time:\s*([\d.e+-]+)\s*s',
            'Output_Time': r'----- Starting Output -----.*?Use Time:\s*([\d.e+-]+)\s*s',
            'Total_Time': r'----- Total Time:\s*([\d.e+-]+)\s*s',
            'Merge_Cutting_Time': r'----- Starting Merge and Cutting Polygon -----.*?Use Time:\s*([\d.e+-]+)\s*s'
        }

        for key, pattern in time_patterns.items():
            match = re.search(pattern, run_content, re.DOTALL)
            if match:
                try:
                    result[f'第{i}次{key}'] = float(match.group(1))
                except ValueError:
                    result[f'第{i}次{key}'] = match.group(1)  # 保留科学计数法字符串
            else:
                result[f'第{i}次{key}'] = None

        # 特殊处理Lazy_BFS_Time，因为可能有多个
        lazy_bfs_pattern = r'----- Starting Lazy BFS.*?Use Time:\s*([\d.e+-]+)\s*s'
        lazy_bfs_matches = re.findall(lazy_bfs_pattern, run_content, re.DOTALL)

        if lazy_bfs_matches:
            # 如果有多个Lazy_BFS_Time，用分号分隔
            lazy_bfs_times = []
            for j, time in enumerate(lazy_bfs_matches, 1):
                try:
                    lazy_bfs_times.append(str(float(time)))
                except ValueError:
                    lazy_bfs_times.append(time)  # 保留科学计数法字符串

            result[f'第{i}次Lazy_BFS_Time'] = '; '.join(lazy_bfs_times)
        else:
            result[f'第{i}次Lazy_BFS_Time'] = None

    return result

def main():
    # 获取当前目录下所有txt文件
    txt_files = glob.glob("*.txt")

    if not txt_files:
        print("当前目录下没有找到txt文件")
        return

    # 存储所有文件的结果
    all_results = []

    # 处理每个txt文件
    for txt_file in txt_files:
        print(f"正在处理文件: {txt_file}")
        try:
            result = extract_info_from_txt(txt_file)
            all_results.append(result)
            print(f"成功提取信息: {txt_file}")
        except Exception as e:
            print(f"处理文件 {txt_file} 时发生错误: {e}")

    if not all_results:
        print("没有成功提取到任何信息")
        return

    # 转换为DataFrame
    df = pd.DataFrame(all_results)

    # 重新排列列的顺序，使每次执行的执行时间和Total_Time相邻
    all_columns = df.columns.tolist()

    # 将文件名放在第一列
    if '文件名' in all_columns:
        all_columns.remove('文件名')
        all_columns = ['文件名'] + all_columns

    # 将线程数和总执行时间放在前面
    for col in ['线程数', '总执行时间']:
        if col in all_columns:
            all_columns.remove(col)
            all_columns.insert(1, col)  # 放在文件名后面

    # 为每次运行创建列组
    new_column_order = all_columns[:3]  # 文件名, 线程数, 总执行时间

    # 对每次运行i，将相关列按特定顺序排列
    for i in range(1, 7):
        # 找出与本次运行相关的所有列
        run_columns = [col for col in all_columns if f'第{i}次' in col]

        # 按特定顺序排列这些列
        preferred_order = [
            f'第{i}次执行时间',
            f'第{i}次Total_Time',
            f'第{i}次Input_Time',
            f'第{i}次Space_Index_Time',
            f'第{i}次Lazy_BFS_Time',
            f'第{i}次Output_Time',
            f'第{i}次Merge_Cutting_Time'
        ]

        # 只保留实际存在的列
        ordered_columns = [col for col in preferred_order if col in run_columns]

        # 添加剩余的列（如果有不在preferred_order中的列）
        remaining_columns = [col for col in run_columns if col not in ordered_columns]
        ordered_columns.extend(remaining_columns)

        # 将这些列添加到新的列顺序中
        new_column_order.extend(ordered_columns)

    # 添加不属于任何运行的列（如果有的话）
    remaining_columns = [col for col in all_columns if col not in new_column_order and not any(f'第{i}次' in col for i in range(1, 7))]
    new_column_order.extend(remaining_columns)

    # 应用新的列顺序
    df = df[new_column_order]

    # 保存到Excel文件
    excel_file = "result.xlsx"

    # 如果文件已存在，追加到现有文件
    if os.path.exists(excel_file):
        try:
            # 读取现有文件
            existing_df = pd.read_excel(excel_file)

            # 合并数据（基于文件名去重）
            combined_df = pd.concat([existing_df, df], ignore_index=True)
            combined_df = combined_df.drop_duplicates(subset=['文件名'], keep='last')

            # 保存回文件
            combined_df.to_excel(excel_file, index=False)
            print(f"已追加数据到现有文件: {excel_file}")

        except Exception as e:
            print(f"追加到现有文件失败，将创建新文件: {e}")
            df.to_excel(excel_file, index=False)
            print(f"已创建新文件: {excel_file}")
    else:
        df.to_excel(excel_file, index=False)
        print(f"已创建新文件: {excel_file}")

    # 打印统计信息
    print(f"\n处理完成！")
    print(f"共处理文件数: {len(all_results)}")
    print(f"输出文件: {excel_file}")

    # 显示提取的列信息
    print(f"\n提取的列信息:")
    for i, col in enumerate(df.columns, 1):
        print(f"{i:2d}. {col}")

    # 显示数据预览
    print(f"\n数据预览（前5行）:")
    print(df.head())

if __name__ == "__main__":
    main()