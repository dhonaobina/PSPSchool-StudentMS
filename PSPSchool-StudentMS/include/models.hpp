#pragma once
#include <string>

/*
-------------------------------------------------------------------------------
 models.hpp — Core domain structs
-------------------------------------------------------------------------------
Defines plain data structures for the three main entities in the system:
  - Student
  - Course
  - Grade (enrollment + marks)

These are simple value types with public fields, suitable for storage in both
SQLite tables and in-memory vectors (DataStore).
-------------------------------------------------------------------------------
*/

// A student record
struct Student {
    std::string roll_no;   // primary key-like, e.g. S001
    std::string name;
    std::string address;
    std::string contact;
};

// A course record
struct Course {
    std::string code;        // primary key-like, e.g. MTH101
    std::string title;
    std::string description;
    std::string teacher;
};

// One grade record linking a student and a course
struct Grade {
    std::string roll_no;     // foreign key -> Student
    std::string course_code; // foreign key -> Course
    double internal_mark{ 0.0 }; // 0..100, typically coursework/tests
    double final_mark{ 0.0 };    // 0..100, final exam

    // Compute weighted grade using 30% internal, 70% final
    double weighted() const {
        return 0.3 * internal_mark + 0.7 * final_mark;
    }
};