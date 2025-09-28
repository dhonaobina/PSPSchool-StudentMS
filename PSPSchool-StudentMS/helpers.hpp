#pragma once
#include <string>
#include "services.hpp"   // brings in DataStore, Student, Course, Grade
#include "db.hpp"

/*
-------------------------------------------------------------------------------
 helpers.hpp — In-memory cache helpers
-------------------------------------------------------------------------------
These functions operate on the DataStore vectors (students, courses, grades)
without touching the SQLite database. They are always called *after* the DB
operation has succeeded to keep the cache consistent with the DB.

Naming convention:
  - exists_*   -> read-only check for presence.
  - apply_*    -> update in place (by key).
  - remove_*   -> erase entity and any dependent grades (mirrors DB cascades).

Return values:
  - For checkers (exists_*), true if the entity/enrollment exists.
  - For apply_* and remove_* helpers, true if at least one element was updated
    or erased.

Usage reminder:
  - Call DB functions (db_add_*, db_update_*, db_delete_*) first.
  - Only if DB call returns true, call the corresponding helper here.
-------------------------------------------------------------------------------
*/

// ==========================
// Existence checks
// ==========================

/// True if a student with given roll exists in DataStore.
bool exists_student(const DataStore& d, const std::string& roll);

/// True if a course with given code exists in DataStore.
bool exists_course(const DataStore& d, const std::string& code);

/// True if a (student, course) enrollment already exists in DataStore.
bool already_enrolled(const DataStore& d,
    const std::string& roll,
    const std::string& code);

// ==========================
// Updates
// ==========================

/// Replace the student with matching roll_no by new Student data.
/// Returns true if updated.
bool apply_student_update(DataStore& d, const Student& s);

/// Replace the course with matching code by new Course data.
/// Returns true if updated.
bool apply_course_update(DataStore& d, const Course& c);

// ==========================
// Removals (mirror DB cascades)
// ==========================

/// Remove student and cascade delete their grades.
/// Returns true if removed.
bool remove_student(DataStore& d, const std::string& roll);

/// Remove course and cascade delete grades for that course.
/// Returns true if removed.
bool remove_course(DataStore& d, const std::string& code);

/// Remove a single enrollment (grade row).
/// Returns true if removed.
bool remove_enrollment(DataStore& d, const std::string& roll, const std::string& code);



