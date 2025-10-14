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
            int layer_id = polygons[poly_id]->layer_id;
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
                OutputPolygon(res_file, *polygons[poly_id]);
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
