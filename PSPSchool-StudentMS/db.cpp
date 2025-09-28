/*
-------------------------------------------------------------------------------
 db.cpp — SQLite persistence layer for PSPSchool Student Management System
-------------------------------------------------------------------------------
Purpose
  - Implements all database I/O for students, courses, and grades using SQLite3.
  - Exposes small, purpose-specific functions called by the UI/services layer.

Design notes
  - Each function returns a bool for success/failure. Callers can print messages
    and keep the in-memory DataStore in sync only when DB writes succeed.
  - Foreign key cascades are enabled per-connection (PRAGMA foreign_keys=ON).
  - Write ops use prepared statements with bound parameters to avoid SQL injection
    and handle quoting safely.
  - Reads that stream many rows use sqlite3_prepare_v2 / sqlite3_step loops.

Caveats / TODOs for contributors
  - NULL handling: sqlite3_column_text may return nullptr. This code assumes the
    columns are NOT NULL (or the seed data has values). If schema changes or
    NULLs are possible, guard conversions (e.g., fallback to empty string).
  - Transactions: Seed and multi-step operations currently run autocommit. If
    you add larger multi-statement updates, consider wrapping in BEGIN/COMMIT.
  - Error surfacing: exec_sql prints error text. Prepared statement paths return
    only false/true; extend to bubble up sqlite3_errmsg(db) when debugging.
-------------------------------------------------------------------------------
*/

#include "db.hpp"
#include <iostream>

// Small helper to run a raw SQL string with sqlite3_exec and report errors.
static bool exec_sql(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        if (err) { std::cerr << "SQL error: " << err << "\n"; sqlite3_free(err); }
        return false;
    }
    return true;
}

// Open (or create) the SQLite database file at `path` and enable FK constraints
// for this connection. Returns false if the DB cannot be opened.
bool db_open(sqlite3*& db, const std::string& path) {
    db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << "\n";
        return false;
    }
    // Enforce FK constraints for this connection
    exec_sql(db, "PRAGMA foreign_keys = ON;");
    return true;
}

// Close the database handle if non-null. Safe to call multiple times.
void db_close(sqlite3* db) {
    if (db) sqlite3_close(db);
}

// Create tables if they don't exist yet and seed some initial data the first
// time the app runs. Safe to call on every startup.
bool db_init_and_seed(sqlite3* db) {
    // 1) Create tables (idempotent). FK cascades delete dependent grade rows.
    const char* ddl =
        "PRAGMA foreign_keys = ON;"

        "CREATE TABLE IF NOT EXISTS students ("
        "  roll_no   TEXT PRIMARY KEY,"
        "  name      TEXT NOT NULL,"
        "  address   TEXT,"
        "  contact   TEXT"
        ");"

        "CREATE TABLE IF NOT EXISTS courses ("
        "  code        TEXT PRIMARY KEY,"
        "  title       TEXT NOT NULL,"
        "  description TEXT,"
        "  teacher     TEXT"
        ");"

        "CREATE TABLE IF NOT EXISTS grades ("
        "  roll_no       TEXT NOT NULL,"
        "  course_code   TEXT NOT NULL,"
        "  internal_mark REAL NOT NULL DEFAULT 0,"
        "  final_mark    REAL NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (roll_no, course_code),"
        "  FOREIGN KEY (roll_no) REFERENCES students(roll_no) ON DELETE CASCADE,"
        "  FOREIGN KEY (course_code) REFERENCES courses(code) ON DELETE CASCADE"
        ");";
    if (!exec_sql(db, ddl)) return false;

    // 2) Seed only when tables are empty. A fast existence check per table.
    auto table_empty = [&](const char* table)->bool {
        sqlite3_stmt* st = nullptr;
        std::string q = std::string("SELECT 1 FROM ") + table + " LIMIT 1;";
        bool empty = true;
        if (sqlite3_prepare_v2(db, q.c_str(), -1, &st, nullptr) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) empty = false;
        }
        sqlite3_finalize(st);
        return empty;
        };

    if (table_empty("students")) {
        const char* seed_students =
            "INSERT INTO students(roll_no,name,address,contact) VALUES"
            "('S001','Ava','12 Oak St','021-111'),"
            "('S002','Leo','34 Pine Ave','021-222'),"
            "('S003','Mia','56 Willow Rd','021-333');";
        if (!exec_sql(db, seed_students)) return false;
    }

    if (table_empty("courses")) {
        const char* seed_courses =
            "INSERT INTO courses(code,title,description,teacher) VALUES"
            "('MTH101','Maths','Numbers and algebra','Mr. King'),"
            "('SCI101','Science','Intro science','Ms. Ray'),"
            "('ENG101','English','Reading & writing','Mrs. Lee');";
        if (!exec_sql(db, seed_courses)) return false;
    }

    if (table_empty("grades")) {
        const char* seed_grades =
            "INSERT INTO grades(roll_no,course_code,internal_mark,final_mark) VALUES"
            "('S001','MTH101',75,88),"
            "('S001','SCI101',62,70),"
            "('S002','ENG101',80,92),"
            "('S003','MTH101',55,60);";
        if (!exec_sql(db, seed_grades)) return false;
    }

    return true;
}

// Load full tables into the in-memory DataStore (used by the UI/reporting).
// Clears the vectors first to avoid duplicates.
bool db_load_all(sqlite3* db, DataStore& store) {
    store.all_students.clear();
    store.all_courses.clear();
    store.all_grades.clear();

    // --- load students ------------------------------------------------------
    {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT roll_no,name,address,contact FROM students;", -1, &st, nullptr) != SQLITE_OK)
            return false;
        while (sqlite3_step(st) == SQLITE_ROW) {
            Student s;
            // NOTE: if schema ever allows NULLs here, guard against nullptrs.
            s.roll_no = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
            s.name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
            s.address = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
            s.contact = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
            store.all_students.push_back(s);
        }
        sqlite3_finalize(st);
    }

    // --- load courses -------------------------------------------------------
    {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT code,title,description,teacher FROM courses;", -1, &st, nullptr) != SQLITE_OK)
            return false;
        while (sqlite3_step(st) == SQLITE_ROW) {
            Course c;
            c.code = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
            c.title = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
            c.description = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
            c.teacher = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
            store.all_courses.push_back(c);
        }
        sqlite3_finalize(st);
    }

    // --- load grades --------------------------------------------------------
    {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT roll_no,course_code,internal_mark,final_mark FROM grades;", -1, &st, nullptr) != SQLITE_OK)
            return false;
        while (sqlite3_step(st) == SQLITE_ROW) {
            Grade g;
            g.roll_no = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
            g.course_code = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
            g.internal_mark = sqlite3_column_double(st, 2);
            g.final_mark = sqlite3_column_double(st, 3);
            store.all_grades.push_back(g);
        }
        sqlite3_finalize(st);
    }

    return true;
}

/* =========================
   Persistence helpers (DB)
   ========================= */

   // INSERT student row.
bool db_add_student(sqlite3* db, const Student& s) {
    const char* sql = "INSERT INTO students(roll_no,name,address,contact) VALUES(?,?,?,?);";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, s.roll_no.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, s.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, s.address.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, s.contact.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

// INSERT course row.
bool db_add_course(sqlite3* db, const Course& c) {
    const char* sql = "INSERT INTO courses(code,title,description,teacher) VALUES(?,?,?,?);";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, c.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, c.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, c.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, c.teacher.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

// ENROLL: create a grades row with default marks for (roll_no, course_code).
bool db_enroll(sqlite3* db, const std::string& roll_no, const std::string& course_code) {
    const char* sql = "INSERT INTO grades(roll_no,course_code,internal_mark,final_mark) VALUES(?,?,0,0);";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, roll_no.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, course_code.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

// UPDATE marks for an existing enrollment. Returns false if no row was updated.
bool db_enter_marks(sqlite3* db, const std::string& roll_no, const std::string& course_code,
    double internal_mark, double final_mark) {
    const char* sql = "UPDATE grades SET internal_mark=?, final_mark=? WHERE roll_no=? AND course_code=?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_double(st, 1, internal_mark);
    sqlite3_bind_double(st, 2, final_mark);
    sqlite3_bind_text(st, 3, roll_no.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, course_code.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    bool ok = (rc == SQLITE_DONE) && (sqlite3_changes(db) > 0);
    sqlite3_finalize(st);
    return ok;
}

// Edit helpers ---------------------------------------------------------------

// UPDATE student fields by roll_no.
bool db_update_student(sqlite3* db, const Student& s) {
    const char* sql =
        "UPDATE students SET name=?, address=?, contact=? WHERE roll_no=?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, s.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, s.address.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, s.contact.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, s.roll_no.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    bool ok = (rc == SQLITE_DONE && sqlite3_changes(db) > 0);
    sqlite3_finalize(st);
    return ok;
}

// UPDATE course fields by code.
bool db_update_course(sqlite3* db, const Course& c) {
    const char* sql =
        "UPDATE courses SET title=?, description=?, teacher=? WHERE code=?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, c.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, c.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, c.teacher.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, c.code.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    bool ok = (rc == SQLITE_DONE && sqlite3_changes(db) > 0);
    sqlite3_finalize(st);
    return ok;
}

// Delete helpers -------------------------------------------------------------

// Delete a student by roll; cascades remove their grade rows.
bool db_delete_student(sqlite3* db, const std::string& roll) {
    const char* sql = "DELETE FROM students WHERE roll_no=?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, roll.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    bool ok = (rc == SQLITE_DONE && sqlite3_changes(db) > 0);
    sqlite3_finalize(st);
    return ok; // cascades will delete grades for this student
}

// Delete a course by code; cascades remove its grade rows.
bool db_delete_course(sqlite3* db, const std::string& code) {
    const char* sql = "DELETE FROM courses WHERE code=?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, code.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    bool ok = (rc == SQLITE_DONE && sqlite3_changes(db) > 0);
    sqlite3_finalize(st);
    return ok; // cascades will delete grades for this course
}

// Delete a single enrollment (grade row) by composite key.
bool db_delete_enrollment(sqlite3* db, const std::string& roll, const std::string& code) {
    const char* sql = "DELETE FROM grades WHERE roll_no=? AND course_code=?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, roll.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, code.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    bool ok = (rc == SQLITE_DONE && sqlite3_changes(db) > 0);
    sqlite3_finalize(st);
    return ok;
}

// Quick counts for live dashboard/menu. One round-trip using scalar subqueries.
bool db_get_counts(sqlite3* db, DbCounts& out) {
    static const char* SQL =
        "SELECT "
        " (SELECT COUNT(*) FROM students) AS s, "
        " (SELECT COUNT(*) FROM courses)  AS c, "
        " (SELECT COUNT(*) FROM grades)   AS g;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, SQL, -1, &st, nullptr) != SQLITE_OK) {
        return false;
    }

    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out.students = sqlite3_column_int(st, 0);
        out.courses = sqlite3_column_int(st, 1);
        out.enrolments = sqlite3_column_int(st, 2);
        ok = true;
    }
    sqlite3_finalize(st);
    return ok;
}

