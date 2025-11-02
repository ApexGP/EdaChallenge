/*
#pragma once
#include "public.h"
#include "Input.hpp"
#include <fstream>
#include <charconv>
#include <system_error>
#include <string>
#include <vector>
#include <array>

class Output {
public:
    static constexpr size_t FILE_BUFFER_THRESHOLD = 128 * 1024;

    Output(Input& _input, std::string res_path, std::vector<int>& _component)
        : input(_input), component(_component) {
        file_buffer.reserve(FILE_BUFFER_THRESHOLD * 2);
        std::ofstream res_file(res_path);
        assert(res_file.is_open() && "Failed to open output file");
        stream_buffer.resize(FILE_BUFFER_THRESHOLD);
        res_file.rdbuf()->pubsetbuf(stream_buffer.data(), static_cast<std::streamsize>(stream_buffer.size()));
        printResult(res_file);
        flushBuffer(res_file);
    }

private:
    Input& input;
    std::vector<int>& component;

    std::string file_buffer;
    std::vector<char> stream_buffer;
    std::array<char, 32> int_buffer{};

    void flushBuffer(std::ofstream& res_file) {
        if (!file_buffer.empty()) {
            res_file.write(file_buffer.data(), static_cast<std::streamsize>(file_buffer.size()));
            file_buffer.clear();
        }
    }

    void appendRaw(std::ofstream& res_file, const char* data, size_t len) {
        if (len >= FILE_BUFFER_THRESHOLD) {
            flushBuffer(res_file);
            res_file.write(data, static_cast<std::streamsize>(len));
            return;
        }
        if (file_buffer.size() + len > FILE_BUFFER_THRESHOLD) {
            flushBuffer(res_file);
        }
        file_buffer.append(data, len);
    }

    void appendChar(std::ofstream& res_file, char c) {
        if (file_buffer.size() == FILE_BUFFER_THRESHOLD) {
            flushBuffer(res_file);
        }
        file_buffer.push_back(c);
    }

    void appendInt(std::ofstream& res_file, int value) {
        auto* begin = int_buffer.data();
        auto* end = begin + int_buffer.size();
        auto result = std::to_chars(begin, end, value);
        if (result.ec == std::errc()) {
            appendRaw(res_file, int_buffer.data(), static_cast<size_t>(result.ptr - int_buffer.data()));
        } else {
            const auto fallback = std::to_string(value);
            appendRaw(res_file, fallback.data(), fallback.size());
        }
    }

    void printResult(std::ofstream& res_file) {
        const int layer_num = static_cast<int>(input.polygon_id_range_in_layer.size());
        std::vector<std::vector<int>> layer_polygons(layer_num);
        layer_polygons.reserve(layer_num);

        auto& polygons = input.polygons;
        for (int poly_id : component) {
            int layer_id = polygons[poly_id].layer_id;
            layer_polygons[layer_id].emplace_back(poly_id);
        }

        for (int i = 0; i < layer_num; ++i) {
            const auto& ids = layer_polygons[i];
            if (ids.empty()) {
                continue;
            }

            const std::string& layer_name = input.layer_id_to_name[i];
            appendRaw(res_file, layer_name.data(), layer_name.size());
            appendChar(res_file, '\n');

            for (int poly_id : ids) {
                OutputPolygon(res_file, polygons[poly_id]);
            }
        }
    }

    void OutputPolygon(std::ofstream& res_file, Polygon& p) {
        const auto vertex_count = p.vertex.size();
        for (size_t j = 0; j < vertex_count; ++j) {
            appendChar(res_file, '(');
            appendInt(res_file, p.vertex[j].x);
            appendChar(res_file, ',');
            appendInt(res_file, p.vertex[j].y);
            appendChar(res_file, ')');
            if (j + 1 != vertex_count) {
                appendChar(res_file, ',');
            }
        }
        appendChar(res_file, '\n');
    }
};
*/

#pragma region another_implement
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
        
        // 设置大缓冲区（4MB）
        const size_t BUFFER_SIZE = 4 * 1024 * 1024;
        std::vector<char> file_buffer(BUFFER_SIZE);
        setvbuf(file, file_buffer.data(), _IOFBF, BUFFER_SIZE); // 将缓冲区关联到文件流,数据会先写入内存缓冲区，不会立即触发磁盘I/O,只有当缓冲区满、调用fflush()或fclose()时，才会真正写入磁盘
        
        if(thread_count == 1 || input.layer_name_to_id.size() == 1) // 若是单层也不用并行写，除非实现层内并行
            printResult(file);
        else
            printResultParallel(file);

        fclose(file);
    }

private:
    Input& input;
    std::vector<int>& component;
    char num_buffer[32]; // 整数转换缓冲区
    
    // 超快速整数转字符串
    char* ultraFastIntToStr(int value, char* end) {
        char* ptr = end;
        bool negative = value < 0;
        unsigned int n = negative ? -value : value;
        
        do {
            *--ptr = '0' + (n % 10);
            n /= 10;
        } while (n > 0);
        
        if (negative) {
            *--ptr = '-';
        }
        
        return ptr;
    }
    
    void printResult(FILE* file) {
        const int layer_num = static_cast<int>(input.polygon_id_range_in_layer.size());
        
        // 直接使用哈希表避免预分配
        std::vector<int>* layer_polygons = new std::vector<int>[layer_num]();
        
        // 单次遍历分组
        for (int poly_id : component) {
            int layer_id = input.polygons[poly_id].layer_id;
            layer_polygons[layer_id].push_back(poly_id);
        }
        
        // 超大缓冲区用于构建输出行
        const size_t LINE_BUFFER_SIZE = 64 * 1024; // 64KB行缓冲区
        std::vector<char> line_buffer(LINE_BUFFER_SIZE);
        
        for (int i = 0; i < layer_num; ++i) {
            const auto& ids = layer_polygons[i];
            if (ids.empty()) continue;
            
            const std::string& layer_name = input.layer_id_to_name[i];
            
            // 写入图层名称
            fwrite(layer_name.data(), 1, layer_name.size(), file);
            fputc('\n', file);
            
            // 批量处理每个多边形
            for (int poly_id : ids) {
                outputPolygon(file, input.polygons[poly_id], line_buffer);
            }
        }
        
        delete[] layer_polygons;
    }
    
    void outputPolygon(FILE* file, const Polygon& p, std::vector<char>& line_buffer) {
        char* ptr = line_buffer.data();
        const char* end = ptr + line_buffer.size();
        const size_t vertex_count = p.vertex.size();
        
        for (size_t j = 0; j < vertex_count; ++j) {
            if (ptr + 64 >= end) { // 预留足够空间
                fwrite(line_buffer.data(), 1, ptr - line_buffer.data(), file);
                ptr = line_buffer.data();
            }
            
            *ptr++ = '(';
            
            // 手动整数转换
            char* num_start = ultraFastIntToStr(p.vertex[j].x, num_buffer + sizeof(num_buffer) - 1);
            char* num_ptr = num_start;
            while (num_ptr < num_buffer + sizeof(num_buffer) && *num_ptr) {
                *ptr++ = *num_ptr++;
            }
            
            *ptr++ = ',';
            
            // 第二个整数
            num_start = ultraFastIntToStr(p.vertex[j].y, num_buffer + sizeof(num_buffer) - 1);
            num_ptr = num_start;
            while (num_ptr < num_buffer + sizeof(num_buffer) && *num_ptr) {
                *ptr++ = *num_ptr++;
            }
            
            *ptr++ = ')';
            
            if (j + 1 != vertex_count) {
                *ptr++ = ',';
            }
        }
        
        *ptr++ = '\n';
        
        // 写入剩余数据
        if (ptr > line_buffer.data()) {
            fwrite(line_buffer.data(), 1, ptr - line_buffer.data(), file);
        }
    }

#pragma region parallel_implement
    // 并行版本
    void printResultParallel(FILE* file) {
        const int layer_num = static_cast<int>(input.polygon_id_range_in_layer.size());
        
        // 按图层分组
        std::vector<std::vector<int>> layer_polys(layer_num);
        for (int poly_id : component) {
            int layer_id = input.polygons[poly_id].layer_id;
            layer_polys[layer_id].push_back(poly_id);
        }
        
        // 并行处理每个图层
        #pragma omp parallel for schedule(dynamic)
        for (int layer_id = 0; layer_id < layer_num; ++layer_id) {
            if (layer_polys[layer_id].empty()) continue;
            
            // 每个线程有自己的num_buffer
            char num_buffer[32];
            std::string poly_result;
            
            std::string layer_content;
            layer_content.reserve(layer_polys[layer_id].size() * 256);
            layer_content.append(input.layer_id_to_name[layer_id]); // 层名
            layer_content.push_back('\n');
            
            // 层的每个多边形
            for (int poly_id : layer_polys[layer_id]) {
                layer_content.append(polygonToString(input.polygons[poly_id], num_buffer, poly_result));
            }
            
            #pragma omp critical
            {
                fwrite(layer_content.data(), 1, layer_content.size(), file);
            }
        }
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

#pragma endregion