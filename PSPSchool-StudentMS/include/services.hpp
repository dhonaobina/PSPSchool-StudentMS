#pragma once
#include <vector>
#include <algorithm>
#include <iostream>
#include "models.hpp"

/*
-------------------------------------------------------------------------------
 services.hpp - In-memory "service" helpers and simple store
-------------------------------------------------------------------------------
This header defines:
  - DataStore: a simple in-memory cache of students, courses, and grades.
  - Small helper functions that operate on DataStore for common actions the
    UI needs (add/show/enroll/enter marks/report).

Design notes
  - DataStore mirrors the SQLite database. DB remains the source of truth.
    Callers should first perform the DB write; only if that succeeds should
    they call the matching in-memory helper to keep the cache consistent.
  - All helpers are inline and operate in O(n) time using std::find_if. For
    larger datasets, consider replacing vectors with maps or adding indexes.
  - All output is written to std::cout to keep the UI minimal for the console.

Conventions
  - Functions return bool to indicate success/failure where appropriate.
  - Mark ranges are validated to be in [0, 100].
-------------------------------------------------------------------------------
*/

// Our simple "database" / in-memory cache
struct DataStore {
    std::vector<Student> all_students;
    std::vector<Course>  all_courses;
    std::vector<Grade>   all_grades;
};

// ==========================
// STUDENTS
// ==========================

// Add a student if roll_no is unique. Returns true on success.
inline bool add_student(DataStore& data, const Student& s) {
    auto it = std::find_if(data.all_students.begin(), data.all_students.end(),
        [&](const Student& x) { return x.roll_no == s.roll_no; });
    if (it != data.all_students.end()) return false; // already exists
    data.all_students.push_back(s);
    return true;
}

// Print a simple list of students to stdout.
inline void show_students(const DataStore& data) {
    if (data.all_students.empty()) {
        std::cout << "No students enrolled.\n";
        return;
    }
    std::cout << "--- ********************** ---\n";
    std::cout << "        View Students         \n";
    std::cout << "--- ********************** ---\n";
    for (const auto& s : data.all_students) {
        std::cout << s.roll_no << " - "
            << s.name << " - "
            << s.address << " - "
            << s.contact << "\n";
    }
}

// ==========================
// COURSES
// ==========================

// Add a course if code is unique. Returns true on success.
inline bool add_course(DataStore& data, const Course& c) {
    auto it = std::find_if(data.all_courses.begin(), data.all_courses.end(),
        [&](const Course& x) { return x.code == c.code; });
    if (it != data.all_courses.end()) return false;
    data.all_courses.push_back(c);
    return true;
}

// Print a simple list of courses to stdout.
inline void show_courses(const DataStore& data) {
    if (data.all_courses.empty()) { std::cout << "No courses.\n"; return; }
    for (const auto& c : data.all_courses)
        std::cout << c.code << " - " << c.title << " - " << c.teacher << "\n";
}

// ==========================
// ENROLLMENT
// ==========================

// Enroll a student in a course by creating a Grade row with 0 marks.
// Returns false if student/course does not exist or duplicate enrollment.
inline bool enroll_student(DataStore& data, const std::string& roll_no, const std::string& course_code) {
    auto s = std::find_if(data.all_students.begin(), data.all_students.end(),
        [&](const Student& x) { return x.roll_no == roll_no; });
    if (s == data.all_students.end()) return false;

    auto c = std::find_if(data.all_courses.begin(), data.all_courses.end(),
        [&](const Course& x) { return x.code == course_code; });
    if (c == data.all_courses.end()) return false;

    auto dup = std::find_if(data.all_grades.begin(), data.all_grades.end(),
        [&](const Grade& g) { return g.roll_no == roll_no && g.course_code == course_code; });
    if (dup != data.all_grades.end()) return false;

    data.all_grades.push_back(Grade{ roll_no, course_code, 0.0, 0.0 });
    return true;
}

// ==========================
// MARKS
// ==========================

// Enter or replace marks for an existing enrollment. Returns true on success.
inline bool enter_marks(DataStore& data, const std::string& roll_no, const std::string& course_code,
    double internal, double final) {
    if (internal < 0 || internal > 100 || final < 0 || final > 100) return false;
    auto it = std::find_if(data.all_grades.begin(), data.all_grades.end(),
        [&](const Grade& g) { return g.roll_no == roll_no && g.course_code == course_code; });
    if (it == data.all_grades.end()) return false;
    it->internal_mark = internal;
    it->final_mark = final;
    return true;
}

// ==========================
// REPORTING
// ==========================

// Print a simple per-student report: lists each enrolled course and marks.
inline void student_report(const DataStore& data, const std::string& roll_no) {
    auto s = std::find_if(data.all_students.begin(), data.all_students.end(),
        [&](const Student& x) { return x.roll_no == roll_no; });
    if (s == data.all_students.end()) { std::cout << "Student not found.\n"; return; }

    std::cout << "Student: " << s->name << " (" << s->roll_no << ")\n";
    bool any = false;
    for (const auto& g : data.all_grades) {
        if (g.roll_no != roll_no) continue;
        any = true;
        auto c = std::find_if(data.all_courses.begin(), data.all_courses.end(),
            [&](const Course& x) { return x.code == g.course_code; });
        std::string title = (c == data.all_courses.end()) ? g.course_code : c->title;
        std::cout << " - " << title
            << " | internal=" << g.internal_mark
            << " final=" << g.final_mark
            << " grade=" << g.weighted() << "\n";
    }

    double total = 0.0;
    int n = 0, passed = 0;
    for (const auto& g : data.all_grades) {
        if (g.roll_no != roll_no) continue;
        double w = g.weighted();
        total += w; ++n;
        if (w >= 50.0) ++passed; // choose your pass threshold
    }
    if (n > 0) {
        std::cout << "Overall average: " << (total / n)
            << " | Courses: " << n
            << " | Passed: " << passed << "/" << n << "\n";
    }

    if (!any) std::cout << "No courses enrolled.\n";
}

// ==========================
// ENROLLMENTS (list all)
// ==========================
inline void show_enrollments(const DataStore& data) {
    if (data.all_grades.empty()) {
        std::cout << "No enrollments.\n";
        return;
    }
    for (const auto& g : data.all_grades) {
        std::cout << g.roll_no << " -> " << g.course_code
            << " | internal=" << g.internal_mark
            << " final=" << g.final_mark
            << " weighted=" << g.weighted() << "\n";
    }
}

