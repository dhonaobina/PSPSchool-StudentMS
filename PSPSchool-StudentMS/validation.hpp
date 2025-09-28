#pragma once
#include <string>
#include <regex>
#include <algorithm>
#include <iostream>
#include <limits>
#include <cctype>   // for std::isspace

/*
-------------------------------------------------------------------------------
 validation.hpp - Input validation and console prompt helpers (ASCII only)
-------------------------------------------------------------------------------
What this file provides:
  - trim: basic whitespace trimming helper.
  - Validators: roll number, name, phone, course code, short non-empty text.
  - Prompt helpers for interactive console:
      * prompt_until_valid            -> simple loop until validator passes
      * prompt_until_valid_or_back    -> like above, but supports Back/Exit
      * prompt_number_or_back         -> numeric with range and Back/Exit
      * prompt_edit_string            -> edit in-place with default value
      * confirm_or_back               -> yes/no confirmation (Back on no)

Conventions:
  - Special inputs:
      Back: "0", "b", "B"
      Exit: "x", "X", "q", "Q"
  - All characters are plain ASCII (no Unicode dashes).
-------------------------------------------------------------------------------
*/

// Trim leading and trailing whitespace.
inline std::string trim(std::string s) {
    auto ws = [](int ch) { return std::isspace(ch); };
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), ws));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), ws).base(), s.end());
    return s;
}

// e.g. S001, S12345  (S + 3-6 digits)
inline bool is_valid_roll(const std::string& x) {
    static const std::regex re("^S\\d{3,6}$");
    return std::regex_match(x, re);
}

// letters, spaces, hyphen, apostrophe; 2..40 chars
inline bool is_valid_name(const std::string& x) {
    if (x.size() < 2 || x.size() > 40) return false;
    static const std::regex re("^[A-Za-z '\\-]+$");
    return std::regex_match(x, re);
}

// optional but simple NZ-style mobile check (021/022/027/029 etc)
inline bool is_valid_phone(const std::string& x) {
    static const std::regex re("^0(2[0-9]|[3-9][0-9])[- ]?\\d{3}[- ]?\\d{3,4}$|^021[- ]?\\d{3}[- ]?\\d{3,4}$");
    return std::regex_match(x, re);
}

// 3 letters + 3 digits, e.g. ENG101, MTH101
inline bool is_valid_course_code(const std::string& x) {
    static const std::regex re("^[A-Z]{3}\\d{3}$");
    return std::regex_match(x, re);
}

// non-empty, max 60
inline bool is_non_empty_short(const std::string& x) {
    return !trim(x).empty() && x.size() <= 60;
}

// ---- generic prompt helper ----
inline std::string prompt_until_valid(
    const std::string& label,
    bool (*validator)(const std::string&),
    const std::string& error_msg)
{
    std::string v;
    for (;;) {
        std::cout << label;
        if (!std::getline(std::cin >> std::ws, v)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        v = trim(v);
        if (validator(v)) return v;
        std::cout << "  -> " << error_msg << "\n";
    }
}

// ---- back / exit aware prompts ----
enum class InputCtl { Ok, Back, Exit };

// String prompt that accepts Back/Exit keywords.
// Back: "0", "b", "B"   Exit: "x","X","q","Q"
inline InputCtl prompt_until_valid_or_back(
    const std::string& label,
    std::string& out,
    bool (*validator)(const std::string&),
    const std::string& error_msg)
{
    for (;;) {
        std::string v;
        std::cout << label << " (0=Back, x=Exit): ";
        if (!std::getline(std::cin >> std::ws, v)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        v = trim(v);
        if (v == "0" || v == "b" || v == "B") return InputCtl::Back;
        if (v == "x" || v == "X" || v == "q" || v == "Q") return InputCtl::Exit;
        if (validator(v)) { out = v; return InputCtl::Ok; }
        std::cout << "  -> " << error_msg << "\n";
    }
}

// Number prompt with range + Back/Exit
inline InputCtl prompt_number_or_back(
    const std::string& label,
    double& out,
    double lo, double hi)
{
    for (;;) {
        std::string v;
        std::cout << label << " [" << lo << "-" << hi << "] (0=Back, x=Exit): ";
        if (!std::getline(std::cin >> std::ws, v)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        v = trim(v);
        if (v == "0" || v == "b" || v == "B") return InputCtl::Back;
        if (v == "x" || v == "X" || v == "q" || v == "Q") return InputCtl::Exit;
        try {
            double d = std::stod(v);
            if (d < lo || d > hi) { std::cout << "  -> Must be between " << lo << " and " << hi << ".\n"; continue; }
            out = d; return InputCtl::Ok;
        }
        catch (...) {
            std::cout << "  -> Please enter a number.\n";
        }
    }
}

// Edit-friendly prompt: show current value, Enter = keep,
// 0/b = Back, x/q = Exit, otherwise validate new value.
inline InputCtl prompt_edit_string(
    const std::string& label,
    const std::string& current,
    std::string& out,
    bool (*validator)(const std::string&),
    const std::string& error_msg)
{
    for (;;) {
        std::cout << label << " [" << current << "] (Enter=keep, 0=Back, x=Exit): ";
        std::string v;
        if (!std::getline(std::cin >> std::ws, v)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        v = trim(v);
        if (v.empty()) { out = current; return InputCtl::Ok; }
        if (v == "0" || v == "b" || v == "B") return InputCtl::Back;
        if (v == "x" || v == "X" || v == "q" || v == "Q") return InputCtl::Exit;
        if (validator(v)) { out = v; return InputCtl::Ok; }
        std::cout << "  -> " << error_msg << "\n";
    }
}

// Yes/No confirmation. Empty or "n" is treated as cancel (Back).
inline InputCtl confirm_or_back(const std::string& msg) {
    for (;;) {
        std::string v;
        std::cout << msg << " [y/N] (0=Back, x=Exit): ";
        if (!std::getline(std::cin >> std::ws, v)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        v = trim(v);
        if (v.empty() || v == "n" || v == "N") return InputCtl::Back; // treat as cancel
        if (v == "0" || v == "b" || v == "B") return InputCtl::Back;
        if (v == "x" || v == "X" || v == "q" || v == "Q") return InputCtl::Exit;
        if (v == "y" || v == "Y") return InputCtl::Ok;
        std::cout << "  -> Please enter y or n.\n";
    }
}



