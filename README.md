# 🎓 Student Management System (C++ / SQLite)

A console-based **Student Management System** built in C++ with SQLite for persistent storage.  
This project was developed as part of a software development assessment to demonstrate **object-oriented design, CRUD operations, and report generation**.  

---

## ✨ Features
- **Students**
  - Add, View, Edit, Delete student records  
- **Courses**
  - Add, View, Edit, Delete course records  
- **Enrollments & Grades**
  - Enroll students into courses  
  - Enter / Edit assessment marks (internal & final)  
  - Calculate weighted grades (30% internal, 70% final)  
  - Delete enrollments  
- **Reports**
  - Generate detailed student reports with courses, marks, weighted grades  
  - Show overall average and pass count for each student  
- **Validation**
  - Input validation for roll numbers, course codes, names, phone numbers, and marks  
  - Error handling for duplicates and invalid operations  

---

## 🛠️ Tech Stack
- **Language:** C++17  
- **Database:** SQLite3  
- **Design:** Based on UML class diagrams and OOP principles  

---

## 📂 Project Structure
- `models.hpp` — Core data structures (Student, Course, Grade)  
- `services.hpp` — In-memory operations and reporting  
- `helpers.hpp` — Utilities for existence checks and data updates  
- `db.hpp / db.cpp` — SQLite persistence layer  
- `validation.hpp` — Input validation and prompts  
- `PSPSchool-StudentMS.cpp` — Main application entry point  

---

## 🚀 How to Run
1. Clone the repository  
2. Build with a C++ compiler (g++ / clang++) linking `sqlite3`:  
   ```bash
   g++ PSPSchool-StudentMS.cpp db.cpp -lsqlite3 -o sms
