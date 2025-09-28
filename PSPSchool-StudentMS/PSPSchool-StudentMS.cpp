/*
-------------------------------------------------------------------------------
 PSPSchool-StudentMS.cpp
-------------------------------------------------------------------------------
 Purpose:
   Console-based Student Management System for the PSPSchool project.
   This is the main entry point (contains main()), driving a menu workflow
   that reads/writes to an SQLite database and mirrors that state in-memory.

 Data flow (very important):
   - Persistent store: SQLite (via db.hpp functions)
   - In-memory cache: DataStore (services.hpp)
   - Pattern on writes: Attempt DB change first; if success, apply the same
     change to the in-memory DataStore (keeps both in sync). If either side
     fails, we print a message and abort that operation.

 User input model:
   - All text fields are validated with helpers in validation.hpp
   - Numeric entry uses prompt_number_or_back; text uses prompt_until_valid_or_back
   - Most prompts support special control responses from InputCtl:
       * Back  -> cancel current action and return to the menu
       * Exit  -> exit the app immediately (we set choice = 0 and break)

 Conventions & Notes for contributors:
   - Keep UI copy short and consistent; prefer full words over abbreviations.
   - Keep the DB and DataStore changes paired and ordered: DB first, then
     in-memory. This ensures DB remains source of truth.
   - If you add new menu items, maintain the ASCII banner width (45-45-45 lines)
     and adjust the counters line if you display live counts.
   - Validation rules live in validation.hpp; please reuse them to maintain
     consistent constraints across the app.
   - Long-term TODOs are marked with "TODO:" comments below.

 Build:
   - Requires SQLite3 dev headers/libs and a C++17 (or later) compiler.

 Author:
   DHONA OBINA (coursework)
-------------------------------------------------------------------------------
*/

#include <iostream>
#include <limits>
#include <iomanip>
#include "services.hpp"     // DataStore, Student, Course, Grade, add/modify helpers
#include "db.hpp"           // SQLite bridge: open/init/CRUD functions
#include "validation.hpp"   // Input validation helpers and InputCtl enum
#include "helpers.hpp"      // Prompt utilities (prompt_until_valid_or_back, etc.)
using namespace std;         // OK for this small console app; avoid in headers

// Prints the big ASCII art welcome banner once at startup.
static void showWelcome() {
    cout << "=====================================================\n";
    cout << "                        WELCOME                      \n";
    cout << "=====================================================\n";
    cout << "                Student Management System            \n";
    cout << "-----------------------------------------------------\n";
    cout << "             Developed for PSPSchool Project         \n";
    cout << "-----------------------------------------------------\n";
    cout << "                    By: DHONA OBINA                  \n";
    cout << "=====================================================\n\n";
}

//-----------------------------------------
int main() {
    showWelcome();

    // In-memory mirror of the database. "data" must be kept in sync with DB
    // changes; we always write to DB first, then update this cache.
    DataStore data;

    // --- Database bootstrap -------------------------------------------------
    sqlite3* db = nullptr;

    // Open or create the SQLite file. If this fails, we cannot continue.
    if (!db_open(db, "school.db")) {
        std::cout << "Could not open database.\n";
        return 1;
    }

    // Initialize schema and seed sample data on first run. If this fails,
    // bail out to avoid running with a partial/unknown schema.
    if (!db_init_and_seed(db)) {
        std::cout << "Could not initialize database.\n";
        db_close(db);
        return 1;
    }

    // Load all rows into the in-memory cache (DataStore) so reads are fast
    // and we can render reports without hitting the DB each time.
    db_load_all(db, data);

    // --- Menu loop ----------------------------------------------------------
    int choice = -1;

    // Utility to reset the cin state and discard the rest of the current line.
    // Use this any time an extraction (operator>>) fails.
    auto clear_input = [] {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        };

    // Main interaction loop. Each branch is documented below.
    while (choice != 0) {
        // NOTE: The counters line is currently hardcoded in the original code.
        // For dynamic counts, you could compute sizes from DataStore.
        std::cout
            << "=====================================================\n"
            << "                      MAIN MENU                      \n"
            << "=====================================================\n"
            << "    Students: 05   Courses: 07   Enrolments:  12     \n"  // TODO: make dynamic
            << "-----------------------------------------------------\n"
            << "  [1]  Add student       [2]  View students          \n"
            << "  [3]  Add course        [4]  View courses           \n"
            << "  [5]  Enroll student    [6]  Enter marks            \n"
            << "  [7]  Student report    [13] View enrollments/grades\n"
            << "-----------------------------------------------------\n"
            << " EDIT:                                               \n"
            << "  [8]  Edit student    [9]  Edit course              \n"
            << "-----------------------------------------------------\n"
            << " DELETE:                                             \n"
            << "  [10] Delete student   [11] Delete course           \n"
            << "  [12] Delete enrolment (student from course)        \n"
            << "-----------------------------------------------------\n"
            << "  [0]  EXIT                                          \n"
            << "=====================================================\n"
            << "  CHOICE: ";

        // If reading the integer fails, flush and redisplay the menu.
        if (!(std::cin >> choice)) { clear_input(); continue; }

        // Always clear the trailing newline before using getline()-style prompts.
        clear_input();

        // ---- 1) Add student ------------------------------------------------
        if (choice == 1) {
            Student s;

            // Roll number (primary key-like). We reject duplicates up front by
            // checking the in-memory mirror, then rely on DB to enforce too.
            auto r1 = prompt_until_valid_or_back(
                "Roll No (e.g. S001)", s.roll_no, is_valid_roll,
                "Invalid roll no. Use S + 3–6 digits (e.g. S001)."
            );
            if (r1 == InputCtl::Back) continue;
            if (r1 == InputCtl::Exit) { choice = 0; break; }
            if (exists_student(data, s.roll_no)) {
                std::cout << "That roll is already used.\n"; continue;
            }

            auto r2 = prompt_until_valid_or_back(
                "Name", s.name, is_valid_name,
                "Invalid name. Letters/spaces only (2–40)."
            );
            if (r2 == InputCtl::Back) continue;
            if (r2 == InputCtl::Exit) { choice = 0; break; }

            auto r3 = prompt_until_valid_or_back(
                "Address", s.address, is_non_empty_short,
                "Address required (max 60 chars)."
            );
            if (r3 == InputCtl::Back) continue;
            if (r3 == InputCtl::Exit) { choice = 0; break; }

            auto r4 = prompt_until_valid_or_back(
                "Contact (NZ phone)", s.contact, is_valid_phone,
                "Invalid NZ phone."
            );
            if (r4 == InputCtl::Back) continue;
            if (r4 == InputCtl::Exit) { choice = 0; break; }

            // Persist to DB first, then mirror in-memory.
            if (db_add_student(db, s) && add_student(data, s))
                std::cout << "Student added (saved to DB).\n";
            else
                std::cout << "Could not add student (duplicate or DB error).\n";
        }

        // ---- 2) View students ---------------------------------------------
        else if (choice == 2) {
            // Prints a table of students from the in-memory cache.
            show_students(data);
        }

        // ---- 3) Add course -------------------------------------------------
        else if (choice == 3) {
            Course c;

            auto a = prompt_until_valid_or_back(
                "Code (e.g. ENG101)", c.code, is_valid_course_code,
                "Invalid code. 3 letters + 3 digits."
            );
            if (a == InputCtl::Back) continue;
            if (a == InputCtl::Exit) { choice = 0; break; }
            if (exists_course(data, c.code)) { std::cout << "Course code already exists.\n"; continue; }

            auto b = prompt_until_valid_or_back(
                "Title", c.title, is_non_empty_short,
                "Title required (max 60)."
            );
            if (b == InputCtl::Back) continue;
            if (b == InputCtl::Exit) { choice = 0; break; }

            auto d = prompt_until_valid_or_back(
                "Description", c.description, is_non_empty_short,
                "Description required (max 60)."
            );
            if (d == InputCtl::Back) continue;
            if (d == InputCtl::Exit) { choice = 0; break; }

            auto e = prompt_until_valid_or_back(
                "Teacher", c.teacher, is_valid_name,
                "Letters/spaces only."
            );
            if (e == InputCtl::Back) continue;
            if (e == InputCtl::Exit) { choice = 0; break; }

            if (db_add_course(db, c) && add_course(data, c))
                std::cout << "Course added (saved to DB).\n";
            else
                std::cout << "Could not add course (duplicate or DB error).\n";
        }

        // ---- 4) View courses ----------------------------------------------
        else if (choice == 4) {
            show_courses(data);
        }

        // ---- 5) Enroll student in course ----------------------------------
        else if (choice == 5) {
            std::string r, code;

            auto p1 = prompt_until_valid_or_back("Roll No", r, is_valid_roll, "Invalid roll.");
            if (p1 == InputCtl::Back) continue;
            if (p1 == InputCtl::Exit) { choice = 0; break; }

            auto p2 = prompt_until_valid_or_back("Course Code", code, is_valid_course_code, "Invalid code.");
            if (p2 == InputCtl::Back) continue;
            if (p2 == InputCtl::Exit) { choice = 0; break; }

            if (!exists_student(data, r)) { std::cout << "Student does not exist.\n"; continue; }
            if (!exists_course(data, code)) { std::cout << "Course does not exist.\n"; continue; }
            if (already_enrolled(data, r, code)) { std::cout << "Already enrolled.\n"; continue; }

            if (db_enroll(db, r, code) && enroll_student(data, r, code))
                std::cout << "Enrollment success (saved to DB).\n";
            else
                std::cout << "Failed to enroll.\n";
        }

        // ---- 6) Enter marks ------------------------------------------------
        else if (choice == 6) {
            std::string r, code;
            double im = 0, fm = 0; // internal & final marks (0..100)

            auto p1 = prompt_until_valid_or_back("Roll No", r, is_valid_roll, "Invalid roll.");
            if (p1 == InputCtl::Back) continue;
            if (p1 == InputCtl::Exit) { choice = 0; break; }

            auto p2 = prompt_until_valid_or_back("Course Code", code, is_valid_course_code, "Invalid code.");
            if (p2 == InputCtl::Back) continue;
            if (p2 == InputCtl::Exit) { choice = 0; break; }

            if (!already_enrolled(data, r, code)) { std::cout << "Not enrolled in that course.\n"; continue; }

            // Both marks are constrained to [0,100] via the number prompt.
            auto n1 = prompt_number_or_back("Internal mark", im, 0, 100);
            if (n1 == InputCtl::Back) continue;
            if (n1 == InputCtl::Exit) { choice = 0; break; }

            auto n2 = prompt_number_or_back("Final mark", fm, 0, 100);
            if (n2 == InputCtl::Back) continue;
            if (n2 == InputCtl::Exit) { choice = 0; break; }

            if (db_enter_marks(db, r, code, im, fm) && enter_marks(data, r, code, im, fm))
                std::cout << "Marks saved (persisted to DB).\n";
            else
                std::cout << "Failed to save marks.\n";
        }

        // ---- 7) Student report --------------------------------------------
        else if (choice == 7) {
            // Simple report driven entirely from the in-memory cache.
            std::string r; std::cout << "Roll No: "; std::getline(std::cin, r);
            student_report(data, r);
        }

        // ---- 8) Edit student ----------------------------------------------
        else if (choice == 8) {
            std::string roll;
            auto s1 = prompt_until_valid_or_back("Roll No to edit", roll, is_valid_roll, "Invalid roll.");
            if (s1 == InputCtl::Back) continue;
            if (s1 == InputCtl::Exit) { choice = 0; break; }

            // Find the current record in the in-memory cache.
            Student cur; bool found = false;
            for (const auto& st : data.all_students)
                if (st.roll_no == roll) { cur = st; found = true; break; }
            if (!found) { std::cout << "Student not found.\n"; continue; }

            // Begin with a copy and selectively update changed fields.
            Student upd = cur;

            auto r2 = prompt_edit_string("Name", cur.name, upd.name, is_valid_name, "Letters/spaces only (2–40).");
            if (r2 == InputCtl::Back) continue; if (r2 == InputCtl::Exit) { choice = 0; break; }

            auto r3 = prompt_edit_string("Address", cur.address, upd.address, is_non_empty_short, "Required (max 60).");
            if (r3 == InputCtl::Back) continue; if (r3 == InputCtl::Exit) { choice = 0; break; }

            auto r4 = prompt_edit_string("Contact (NZ phone)", cur.contact, upd.contact, is_valid_phone, "Invalid NZ phone.");
            if (r4 == InputCtl::Back) continue; if (r4 == InputCtl::Exit) { choice = 0; break; }

            if (db_update_student(db, upd) && apply_student_update(data, upd))
                std::cout << "Student updated (saved to DB).\n";
            else
                std::cout << "Update failed (DB error or not found).\n";
        }

        // ---- 9) Edit course ------------------------------------------------
        else if (choice == 9) {
            std::string code;
            auto c1 = prompt_until_valid_or_back("Course Code to edit", code, is_valid_course_code, "Invalid code.");
            if (c1 == InputCtl::Back) continue;
            if (c1 == InputCtl::Exit) { choice = 0; break; }

            Course cur; bool found = false;
            for (const auto& cc : data.all_courses)
                if (cc.code == code) { cur = cc; found = true; break; }
            if (!found) { std::cout << "Course not found.\n"; continue; }

            Course upd = cur;

            auto e1 = prompt_edit_string("Title", cur.title, upd.title, is_non_empty_short, "Required (max 60).");
            if (e1 == InputCtl::Back) continue; if (e1 == InputCtl::Exit) { choice = 0; break; }

            auto e2 = prompt_edit_string("Description", cur.description, upd.description, is_non_empty_short, "Required (max 60).");
            if (e2 == InputCtl::Back) continue; if (e2 == InputCtl::Exit) { choice = 0; break; }

            auto e3 = prompt_edit_string("Teacher", cur.teacher, upd.teacher, is_valid_name, "Letters/spaces only.");
            if (e3 == InputCtl::Back) continue; if (e3 == InputCtl::Exit) { choice = 0; break; }

            if (db_update_course(db, upd) && apply_course_update(data, upd))
                std::cout << "Course updated (saved to DB).\n";
            else
                std::cout << "Update failed (DB error or not found).\n";
        }

        // ---- 10) Delete student -------------------------------------------
        else if (choice == 10) {
            std::string roll;
            auto p = prompt_until_valid_or_back("Roll No to delete", roll, is_valid_roll, "Invalid roll.");
            if (p == InputCtl::Back) continue;
            if (p == InputCtl::Exit) { choice = 0; break; }

            if (!exists_student(data, roll)) { std::cout << "Student not found.\n"; continue; }

            // Defensive confirmation: warn that grades/enrolments will cascade.
            auto c = confirm_or_back("Delete student and all their grades?");
            if (c == InputCtl::Back) continue;
            if (c == InputCtl::Exit) { choice = 0; break; }

            if (db_delete_student(db, roll) && remove_student(data, roll))
                std::cout << "Student deleted (DB + local grades removed).\n";
            else
                std::cout << "Delete failed (DB error or not found).\n";
        }

        // ---- 11) Delete course --------------------------------------------
        else if (choice == 11) {
            std::string code;
            auto p = prompt_until_valid_or_back("Course Code to delete", code, is_valid_course_code, "Invalid code.");
            if (p == InputCtl::Back) continue;
            if (p == InputCtl::Exit) { choice = 0; break; }

            if (!exists_course(data, code)) { std::cout << "Course not found.\n"; continue; }

            auto c = confirm_or_back("Delete course and all associated grades?");
            if (c == InputCtl::Back) continue;
            if (c == InputCtl::Exit) { choice = 0; break; }

            if (db_delete_course(db, code) && remove_course(data, code))
                std::cout << "Course deleted (DB + local grades removed).\n";
            else
                std::cout << "Delete failed (DB error or not found).\n";
        }

        // ---- 12) Delete enrollment (student from course) -------------------
        else if (choice == 12) {
            std::string r, code;

            auto p1 = prompt_until_valid_or_back("Roll No", r, is_valid_roll, "Invalid roll.");
            if (p1 == InputCtl::Back) continue;
            if (p1 == InputCtl::Exit) { choice = 0; break; }

            auto p2 = prompt_until_valid_or_back("Course Code", code, is_valid_course_code, "Invalid code.");
            if (p2 == InputCtl::Back) continue;
            if (p2 == InputCtl::Exit) { choice = 0; break; }

            if (!already_enrolled(data, r, code)) { std::cout << "Not enrolled in that course.\n"; continue; }

            auto c = confirm_or_back("Delete this enrollment?");
            if (c == InputCtl::Back) continue;
            if (c == InputCtl::Exit) { choice = 0; break; }

            if (db_delete_enrollment(db, r, code) && remove_enrollment(data, r, code))
                std::cout << "Enrollment deleted (DB).\n";
            else
                std::cout << "Delete failed (DB error or not found).\n";
        }

        // ---- 13) View enrollments/grades ----------------------------------
            else if (choice == 13) {
                show_enrollments(data);
}

        // ---- Unknown option guard -----------------------------------------
        else if (choice != 0) {
            std::cout << "Unknown option.\n";
        }
    }

    // --- Shutdown -----------------------------------------------------------
    db_close(db);   // Always close the DB before exiting the program.
    return 0;
}
