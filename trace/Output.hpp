#pragma once
#include "public.h"
#include "Input.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>

class Output {
public:
    Output(Input& _input, const std::string& res_path, std::vector<int>& _component, int thread_count = 1)
        : input(_input), component(_component) {

        // 使用C风格文件操作，最高性能
        FILE* file = fopen(res_path.c_str(), "wb");
        if (!file) {
            throw std::runtime_error("Failed to open output file");
        }

        // 设置文件大缓冲区（4MB）
        const size_t BUFFER_SIZE = 4 * 1024 * 1024;
        std::vector<char> file_buffer(BUFFER_SIZE);
        setvbuf(file, file_buffer.data(), _IOFBF, BUFFER_SIZE); // 将缓冲区关联到文件流,数据会先写入内存缓冲区，不会立即触发磁盘I/O,只有当缓冲区满、调用fflush()或fclose()时，才会真正写入磁盘

        if(thread_count == 1)
            printResult(file);
        else
            printResultParallel(file, thread_count);

        fclose(file);
    }

private:
    Input& input;
    std::vector<int>& component;
    char num_buffer[32]; // 整数转换缓冲区

    // 预计算数字查找表
    static inline const char digits_table[201] =
        "0001020304050607080910111213141516171819"
        "2021222324252627282930313233343536373839"
        "4041424344454647484950515253545556575859"
        "6061626364656667686970717273747576777879"
        "8081828384858687888990919293949596979899";

    // 超快速整数转字符串
    char* ultraFastIntToStr(int value, char* end) {
        char* ptr = end;

        // 特殊处理INT_MIN
        if (value == INT_MIN) {
            // 直接写入"-2147483648"
            *--ptr = '8'; *--ptr = '4'; *--ptr = '6'; *--ptr = '3';
            *--ptr = '8'; *--ptr = '4'; *--ptr = '7'; *--ptr = '4';
            *--ptr = '1'; *--ptr = '2'; *--ptr = '-';
            return ptr;
        }

        unsigned int n;
        if (value < 0) {
            n = -value;
        } else {
            n = value;
        }

        // 处理0的特殊情况
        if (n == 0) {
            *--ptr = '0';
            if (value < 0) *--ptr = '-';
            return ptr;
        }

        // 每次处理2位数字
        while (n >= 100) {
            unsigned int index = (n % 100) * 2;
            n /= 100;
            *--ptr = digits_table[index + 1];
            *--ptr = digits_table[index];
        }

        // 处理剩余的数字（0-99）
        if (n < 10) {
            *--ptr = '0' + n;
        } else {
            unsigned int index = n * 2;
            *--ptr = digits_table[index + 1];
            *--ptr = digits_table[index];
        }

        if (value < 0) {
            *--ptr = '-';
        }

        return ptr;
    }

    void printResult(FILE* file) {
        INFO_INSTR(Timer stage;)
        const int layer_num = static_cast<int>(input.polygon_id_range_in_layer.size());

        // 分组
        std::vector<std::vector<int>> layer_polygons(layer_num);
        for (int poly_id : component) {
            int layer_id = input.polygons[poly_id].layer_id;
            layer_polygons[layer_id].push_back(poly_id);
        }
        INFO_INSTR(double group_time = stage.FromLastCallElapsed();)

        // 图层级缓冲区（1MB）
        const size_t BUFFER_SIZE = 1024 * 1024;
        std::vector<char> buffer(BUFFER_SIZE);
        char* buffer_ptr = buffer.data();
        const char* buffer_end = buffer.data() + BUFFER_SIZE;

        for (int i = 0; i < layer_num; ++i) {
            if (layer_polygons[i].empty()) continue;

            const auto& polygon_ids = layer_polygons[i];
            const std::string& layer_name = input.layer_id_to_name[i];

            // 写入图层头到缓冲区
            const char* layer_name_data = layer_name.data();
            size_t layer_name_size = layer_name.size();

            if (buffer_ptr + layer_name_size + 1 >= buffer_end) {
                fwrite(buffer.data(), 1, buffer_ptr - buffer.data(), file);
                buffer_ptr = buffer.data();
            }

            memcpy(buffer_ptr, layer_name_data, layer_name_size);
            buffer_ptr += layer_name_size;
            *buffer_ptr++ = '\n';

            // 批量处理当前图层的所有多边形
            for (int poly_id : polygon_ids) {
                const Polygon& p = input.polygons[poly_id];
                const size_t vertex_count = p.vertex.size();

                // 估计当前多边形所需空间（保守估计）
                size_t estimated_size = vertex_count * 32 + 2; // 每个顶点约32字节

                if (buffer_ptr + estimated_size >= buffer_end) {
                    // 缓冲区不足，先刷新
                    fwrite(buffer.data(), 1, buffer_ptr - buffer.data(), file);
                    buffer_ptr = buffer.data();
                }

                // 写入多边形数据
                for (size_t j = 0; j < vertex_count; ++j) {
                    // 检查缓冲区空间
                    if (buffer_ptr + 64 >= buffer_end) {
                        fwrite(buffer.data(), 1, buffer_ptr - buffer.data(), file);
                        buffer_ptr = buffer.data();
                    }

                    *buffer_ptr++ = '(';

                    // 转换x坐标
                    char* num_str = ultraFastIntToStr(p.vertex[j].x, num_buffer + sizeof(num_buffer) - 1);
                    while (*num_str) {
                        *buffer_ptr++ = *num_str++;
                    }

                    *buffer_ptr++ = ',';

                    // 转换y坐标
                    num_str = ultraFastIntToStr(p.vertex[j].y, num_buffer + sizeof(num_buffer) - 1);
                    while (*num_str) {
                        *buffer_ptr++ = *num_str++;
                    }

                    *buffer_ptr++ = ')';

                    if (j + 1 != vertex_count) {
                        *buffer_ptr++ = ',';
                    }
                }

                *buffer_ptr++ = '\n';
            }
        }

        // 写入剩余数据
        if (buffer_ptr > buffer.data()) {
            fwrite(buffer.data(), 1, buffer_ptr - buffer.data(), file);
        }
        fflush(file);
        INFO_INSTR(double output_time = stage.FromLastCallElapsed();)

        INFO_MSG( "[Output][Timing] group: " << group_time
                  << " s, output: " << output_time
                  << " s, total: " << stage.Elapsed() << " s" )
    }

#pragma region parallel_implement
    // 并行版本
    void printResultParallel(FILE* file, int thread_count) {
        INFO_INSTR(Timer stage;)
        const int layer_num = static_cast<int>(input.polygon_id_range_in_layer.size());

        // 按图层分组
        std::vector<std::vector<int>> layer_polys(layer_num);
        for (int poly_id : component) {
            int layer_id = input.polygons[poly_id].layer_id;
            layer_polys[layer_id].push_back(poly_id);
        }
        INFO_INSTR(double group_time = stage.FromLastCallElapsed();)

        const int chunk_min_polygons = 10000;
        // 层间串行处理，避免输出交叉，大图层层内并行，小图层层内串行
        for (int layer_id = 0; layer_id < layer_num; ++layer_id) {
            if (layer_polys[layer_id].empty()) continue;

            const auto& polygons = layer_polys[layer_id];
            const int total_polygons = polygons.size();

            // 写入图层头
            std::string layer_header = input.layer_id_to_name[layer_id] + "\n";
            fwrite(layer_header.data(), 1, layer_header.size(), file);

            if (total_polygons > chunk_min_polygons * 2) {
            // 大图层：使用独立的并行区域
            #pragma omp parallel num_threads(thread_count)
            {
                // 线程局部变量
                char num_buffer[32];
                std::string poly_result;
                std::string thread_buffer;
                thread_buffer.reserve(1024 * 1024); // 线程局部缓存区，预分配1MB

                int chunk_size = std::max(chunk_min_polygons, total_polygons / (thread_count * 4));

                // 分块并行
                #pragma omp for schedule(dynamic, 1)
                for (int chunk_start = 0; chunk_start < total_polygons; chunk_start += chunk_size) {
                    int chunk_end = std::min(chunk_start + chunk_size, total_polygons);

                    thread_buffer.clear();
                    for (int i = chunk_start; i < chunk_end; ++i) {
                        int poly_id = polygons[i];
                        thread_buffer.append(polygonToString(input.polygons[poly_id], num_buffer, poly_result));
                    }

                    #pragma omp critical
                    {
                        fwrite(thread_buffer.data(), 1, thread_buffer.size(), file);
                    }
                }
            }
            } else {
                // 小图层：串行处理，避免并行开销
                char num_buffer[32];
                std::string poly_result;
                for (int poly_id : polygons) {
                    std::string poly_str = polygonToString(input.polygons[poly_id], num_buffer, poly_result);
                    fwrite(poly_str.data(), 1, poly_str.size(), file);
                }
            }
            // 刷新文件缓冲区
            fflush(file);
        }
        INFO_INSTR(double output_time = stage.FromLastCallElapsed();)

        INFO_MSG( "[Output][Timing] group: " << group_time
          << " s, output: " << output_time
          << " s, total: " << stage.Elapsed() << " s" )
    }

    std::string polygonToString(const Polygon& p, char* num_buffer, std::string& poly_result) {
        auto &result = poly_result;
        result.clear();
        const size_t vertex_count = p.vertex.size();
        result.reserve(vertex_count * 20);

        for (size_t j = 0; j < vertex_count; ++j) {
            result.push_back('(');

            // 使用传入的num_buffer，避免竞争
            char* num_start = ultraFastIntToStr(p.vertex[j].x, num_buffer + sizeof(num_buffer) - 1);
            char* num_ptr = num_start;
            while (num_ptr < num_buffer + sizeof(num_buffer) && *num_ptr) {
                result.push_back(*num_ptr++);
            }

            result.push_back(',');

            num_start = ultraFastIntToStr(p.vertex[j].y, num_buffer + sizeof(num_buffer) - 1);
            num_ptr = num_start;
            while (num_ptr < num_buffer + sizeof(num_buffer) && *num_ptr) {
                result.push_back(*num_ptr++);
            }

            result.push_back(')');

            if (j + 1 != vertex_count) {
                result.push_back(',');
            }
        }
        result.push_back('\n');

        return result;
    }

#pragma endregion
};