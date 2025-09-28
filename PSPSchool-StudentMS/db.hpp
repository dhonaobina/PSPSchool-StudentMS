#pragma once
#include <string>
#include "sqlite3.h"
#include "models.hpp"
#include "services.hpp"   // for DataStore (holds vectors)

/*
-------------------------------------------------------------------------------
 db.hpp — Public interface to SQLite persistence layer
-------------------------------------------------------------------------------

This header declares all functions that interact with the SQLite database.
They provide a clean, minimal API so higher-level code (UI, services) doesn’t
need to deal with raw sqlite3_* calls.

Design:
  - Each function returns `bool` to indicate success/failure.
  - Callers should update the in-memory `DataStore` only when DB ops succeed.
  - `DbCounts` provides live counts (students, courses, enrolments) for menus.

Usage convention:
  - Call `db_open` once at startup, then `db_init_and_seed`.
  - Use `db_load_all` to populate `DataStore` cache after opening.
  - Always call `db_close` before exiting.
-------------------------------------------------------------------------------
*/

/// Opens (creates if not exists) the SQLite DB file at path.
/// Returns true on success, false on failure. On failure, `db` is set to nullptr.
bool db_open(sqlite3*& db, const std::string& path);

/// Close DB (safe if db==nullptr). Call once at shutdown.
void db_close(sqlite3* db);

/// Create tables if missing and insert a small set of dummy data (only if empty).
/// Safe to call on every startup.
bool db_init_and_seed(sqlite3* db);

/// Load all rows from DB into the in-memory DataStore vectors.
/// Clears the vectors first to avoid duplicates.
bool db_load_all(sqlite3* db, DataStore& store);

// ==========================
// INSERT operations
// ==========================

bool db_add_student(sqlite3* db, const Student& s);
bool db_add_course(sqlite3* db, const Course& c);

/// Enroll a student in a course: creates a row in `grades` with marks=0.
bool db_enroll(sqlite3* db, const std::string& roll_no, const std::string& course_code);

// ==========================
// UPDATE operations
// ==========================

/// Enter/update marks for an existing enrollment.
bool db_enter_marks(sqlite3* db, const std::string& roll_no, const std::string& course_code,
    double internal_mark, double final_mark);

/// Update student fields (by roll_no).
bool db_update_student(sqlite3* db, const Student& s);

/// Update course fields (by code).
bool db_update_course(sqlite3* db, const Course& c);

// ==========================
// DELETE operations
// ==========================

/// Delete student (cascades to their grades).
bool db_delete_student(sqlite3* db, const std::string& roll);

/// Delete course (cascades to its grades).
bool db_delete_course(sqlite3* db, const std::string& code);

/// Delete one enrollment (grade row).
bool db_delete_enrollment(sqlite3* db, const std::string& roll, const std::string& code);

// ==========================
// Counts (for dashboards/menus)
// ==========================

/// Simple struct with live counts from DB.
struct DbCounts {
    int students = 0;
    int courses = 0;
    int enrolments = 0; // rows in grades (i.e., enrollments)
};

/// Populate `out` with counts of students, courses, and enrolments.
/// Returns true on success.
bool db_get_counts(sqlite3* db, DbCounts& out);

