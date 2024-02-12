#pragma once

#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>
#include <type_traits>

// minimalist JSON writer

enum class json_state
{
    before_first,
    after_key,
    after_element,
};

static std::string json_output;
static size_t json_depth               = 0;
static constexpr size_t json_pad_width = 2;
static json_state json_cur_state       = json_state::before_first;
static constexpr std::string_view json_padding =
    "                                                                ";
static constexpr size_t json_max_depth = json_padding.size() / json_pad_width;

inline void json_append(std::string_view data) { json_output += data; }
inline void json_append(char data) { json_output += data; }
inline void json_newline()
{
    json_append("\n");
    json_append(json_padding.substr(0, json_pad_width * std::min(json_max_depth, json_depth)));
}
inline void json_comma()
{
    if (json_cur_state == json_state::after_element)
    {
        json_append(", ");
        json_newline();
    }
}
inline void json_quoted(std::string_view str)
{
    json_append("\"");
    constexpr std::string_view escape = "\\\"";

    size_t p = str.find_first_of(escape);
    while (p != std::string_view::npos)
    {
        json_append(str.substr(0, p));
        json_append("\\");
        json_append(str[p]);
        str = str.substr(p + 1);
        p   = str.find_first_of(escape);
    }
    json_append(str);
    json_append("\"");
}
inline void json_string(std::string_view str)
{
    json_comma();
    json_quoted(str);
    json_cur_state = json_state::after_element;
}
inline void json_bool(bool value)
{
    json_comma();
    if (value)
        json_append("true");
    else
        json_append("false");
    json_cur_state = json_state::after_element;
}
inline void json_open_array()
{
    json_comma();
    ++json_depth;
    json_append("[");
    json_newline();
    json_cur_state = json_state::before_first;
}
inline void json_close_array()
{
    --json_depth;
    json_newline();
    json_append("]");
    json_cur_state = json_state::after_element;
}
inline void json_open_object()
{
    json_comma();
    ++json_depth;
    json_append("{");
    json_newline();
    json_cur_state = json_state::before_first;
}
inline void json_close_object()
{
    --json_depth;
    json_newline();
    json_append("}");
    json_cur_state = json_state::after_element;
}
inline void json_key(std::string_view key)
{
    json_comma();
    json_quoted(key);
    json_append(": ");
    json_cur_state = json_state::after_key;
}
template <typename T>
inline void json_number(T number)
{
    json_comma();
    if constexpr (std::is_floating_point_v<T>)
    {
        int e = 12 - ceil(log10(number));
        e     = std::max(e, 0);
        e     = std::min(e, 20);
        char buf[128];
        size_t wr = std::snprintf(buf, sizeof(buf), "%.*f", e, number);
        json_append(std::string_view(std::begin(buf), std::min(wr, sizeof(buf))));
    }
    else
    {
        json_append(std::to_string(number));
    }
    json_cur_state = json_state::after_element;
}