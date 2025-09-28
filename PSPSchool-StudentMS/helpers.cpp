#include "helpers.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

/*
-------------------------------------------------------------------------------
 helpers.cpp — In‑memory helpers for DataStore
-------------------------------------------------------------------------------
These functions operate on the in-memory cache (DataStore) and are used by the
UI layer to check existence, apply edits, and remove entities without touching
SQLite directly. The DB remains the source of truth; writes happen in the DB
first, then these helpers mirror those changes locally.

Complexity notes
  - All operations here are linear in the size of the corresponding vectors
    (O(n)). For classroom-sized datasets this is fine; if the dataset grows,
    consider indexing by unordered_map for O(1) lookups.

Safety
  - Removal helpers also clean up related Grade rows to keep the cache
    consistent with the DB's ON DELETE CASCADE behavior.
-------------------------------------------------------------------------------
*/

// Return true if a student with the given roll exists in the cache.
bool exists_student(const DataStore& d, const std::string& roll) {
    for (const auto& s : d.all_students)
        if (s.roll_no == roll) return true;
    return false;
}

// Return true if a course with the given code exists in the cache.
bool exists_course(const DataStore& d, const std::string& code) {
    for (const auto& c : d.all_courses)
        if (c.code == code) return true;
    return false;
}

// Return true if the (roll, course) pair already has a grade/enrollment row.
bool already_enrolled(const DataStore& d,
    const std::string& roll,
    const std::string& code) {
    for (const auto& g : d.all_grades)
        if (g.roll_no == roll && g.course_code == code) return true;
    return false;
}

// Replace the student with matching roll_no by the provided updated object.
// Returns true if an element was replaced.
bool apply_student_update(DataStore& d, const Student& s) {
    for (auto& it : d.all_students)
        if (it.roll_no == s.roll_no) { it = s; return true; }
    return false;
}

// Replace the course with matching code by the provided updated object.
// Returns true if an element was replaced.
bool apply_course_update(DataStore& d, const Course& c) {
    for (auto& it : d.all_courses)
        if (it.code == c.code) { it = c; return true; }
    return false;
}

// Remove a student by roll and cascade-delete their grade rows in-memory.
// Returns true if at least one student was removed.
bool remove_student(DataStore& d, const std::string& roll) {
    // erase student
    auto s0 = d.all_students.size();
    d.all_students.erase(std::remove_if(d.all_students.begin(), d.all_students.end(),
        [&](const Student& s) { return s.roll_no == roll; }),
        d.all_students.end());

    // erase that student's grades (compose) — mirror DB ON DELETE CASCADE
    d.all_grades.erase(std::remove_if(d.all_grades.begin(), d.all_grades.end(),
        [&](const Grade& g) { return g.roll_no == roll; }),
        d.all_grades.end());

    return d.all_students.size() != s0;
}

// Remove a course by code and cascade-delete its grade rows in-memory.
// Returns true if at least one course was removed.
bool remove_course(DataStore& d, const std::string& code) {
    // erase course
    auto c0 = d.all_courses.size();
    d.all_courses.erase(std::remove_if(d.all_courses.begin(), d.all_courses.end(),
        [&](const Course& c) { return c.code == code; }),
        d.all_courses.end());

    // erase grades for that course (aggregate) — mirror DB ON DELETE CASCADE
    d.all_grades.erase(std::remove_if(d.all_grades.begin(), d.all_grades.end(),
        [&](const Grade& g) { return g.course_code == code; }),
        d.all_grades.end());

    return d.all_courses.size() != c0;
}

// Remove a single enrollment (grade row) by (roll, code).
// Returns true if at least one grade row was removed.
bool remove_enrollment(DataStore& d, const std::string& roll, const std::string& code) {
    auto g0 = d.all_grades.size();
    d.all_grades.erase(std::remove_if(d.all_grades.begin(), d.all_grades.end(),
        [&](const Grade& g) { return g.roll_no == roll && g.course_code == code; }),
        d.all_grades.end());
    return d.all_grades.size() != g0;
}

