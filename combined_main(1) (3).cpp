/*
 * ============================================================
 *  ЭЛЕКТРОННЫЙ ОБРАЗ ПРЕДПРИЯТИЯ — COMBINED EDITION
 *  Лучшее из двух проектов:
 *    ✓ nlohmann/json         (ZNA)       — надёжная сериализация
 *    ✓ Полный CRUD           (мой)       — добавление/редактирование/удаление
 *    ✓ Entity* полиморфизм   (мой)       — настоящий C++ полиморфизм
 *    ✓ DataRepository        (мой)       — изолированное хранилище
 *    ✓ Структура предприятия (мой)       — дерево отделов
 *    ✓ Валидация ввода       (мой)       — защита от кривого ввода
 *    ✓ Поиск по 3 полям      (мой)       — имя + отдел + должность
 *    ✓ 4 режима сортировки   (мой)       — ФИО, зарплата ↑↓, дата
 *    ✓ showInfo() стиль      (ZNA)       — чистый вывод как в ZNA
 *    ✓ generateId() int      (ZNA)       — простой счётчик ID как в ZNA
 * ============================================================
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>
#include <climits>
#include <sstream>

using namespace std;
using json = nlohmann::json;

// ─────────────────────────────────────────────
//  Утилиты: ввод с валидацией, форматирование
// ─────────────────────────────────────────────
namespace Utils {

    string trim(const string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        return (a == string::npos) ? "" : s.substr(a, b - a + 1);
    }

    string toLower(string s) {
        transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }

    string formatMoney(double n) {
        ostringstream ss;
        ss << fixed << setprecision(0) << n;
        string s = ss.str();
        int pos = (int)s.size() - 3;
        while (pos > 0) { s.insert(pos, " "); pos -= 3; }
        return s + " руб.";
    }

    string inputLine(const string& prompt) {
        cout << prompt;
        string s;
        getline(cin, s);
        return trim(s);
    }

    // Безопасный ввод int — защита от нечислового ввода (проблема ZNA)
    int inputInt(const string& prompt, int minV = 0, int maxV = INT_MAX) {
        while (true) {
            cout << prompt;
            string raw;
            getline(cin, raw);
            try {
                int v = stoi(trim(raw));
                if (v >= minV && v <= maxV) return v;
                cout << "  Введите число от " << minV << " до " << maxV << "\n";
            } catch (...) {
                cout << "  Некорректный ввод, попробуйте снова\n";
            }
        }
    }

    void separator(char c = '-', int w = 64) { cout << string(w, c) << "\n"; }

    void title(const string& s) {
        separator('=');
        int pad = max(0, (64 - (int)s.size()) / 2);
        cout << string(pad, ' ') << s << "\n";
        separator('=');
    }

    void subtitle(const string& s) {
        separator('-');
        cout << "  " << s << "\n";
        separator('-');
    }
}

// ═══════════════════════════════════════════════════════════════
//  КЛАСС 1: Entity — абстрактный базовый класс
//  Взято из обоих проектов; добавлен чистый виртуальный describe()
// ═══════════════════════════════════════════════════════════════
class Entity {
protected:
    int id;

public:
    Entity(int id = 0) : id(id) {}
    virtual ~Entity() = default;

    int getId() const { return id; }

    // Из ZNA: showInfo() — читаемый вывод
    virtual void showInfo() const = 0;

    // Из моего: describe() — полиморфизм через Entity*
    virtual string describe() const = 0;
};

// ═══════════════════════════════════════════════════════════════
//  КЛАСС 2: Position — должность
// ═══════════════════════════════════════════════════════════════
class Position : public Entity {
private:
    string title;
    string category;
    string level;  // junior | middle | senior | lead | director

public:
    Position(int id = 0, string title = "",
             string category = "Технический персонал",
             string level = "middle")
        : Entity(id), title(title), category(category), level(level) {}

    string getTitle()    const { return title; }
    string getCategory() const { return category; }
    string getLevel()    const { return level; }

    string getLevelLabel() const {
        map<string,string> m = {
            {"junior","Junior"}, {"middle","Middle"}, {"senior","Senior"},
            {"lead","Руководитель"}, {"director","Директор"}
        };
        auto it = m.find(level);
        return it != m.end() ? it->second : level;
    }

    void setTitle(string v)    { title = v; }
    void setCategory(string v) { category = v; }
    void setLevel(string v)    { level = v; }

    // showInfo() — стиль ZNA
    void showInfo() const override {
        cout << "Должность: " << title
             << " [" << getLevelLabel() << "] — " << category << "\n";
    }

    // describe() — полиморфизм через Entity* (мой проект)
    string describe() const override {
        return "Должность: «" + title + "» | Уровень: " + getLevelLabel()
               + " | Категория: " + category;
    }

    json toJson() const {
        return {{"id", id}, {"title", title},
                {"category", category}, {"level", level}};
    }

    static Position fromJson(const json& j) {
        return Position(
            j.at("id"),
            j.value("title", ""),
            j.value("category", "Технический персонал"),
            j.value("level", "middle")
        );
    }
};

// ═══════════════════════════════════════════════════════════════
//  КЛАСС 3: Department — отдел
// ═══════════════════════════════════════════════════════════════
class Department : public Entity {
private:
    string name;
    string code;
    int    headId;  // id руководителя (0 = не назначен)

public:
    Department(int id = 0, string name = "", string code = "", int headId = 0)
        : Entity(id), name(name), code(code), headId(headId) {}

    string getName()  const { return name; }
    string getCode()  const { return code; }
    int    getHeadId()const { return headId; }

    void setName(string v)  { name = v; }
    void setCode(string v)  { code = v; }
    void setHeadId(int v)   { headId = v; }

    // showInfo() — стиль ZNA
    void showInfo() const override {
        cout << "[" << code << "] " << name << "\n";
    }

    // describe() — полиморфизм через Entity*
    string describe() const override {
        return "Отдел: «" + name + "» (" + code + ")"
               + " | Руководитель: " + (headId ? "назначен" : "не назначен");
    }

    json toJson() const {
        return {{"id", id}, {"name", name}, {"code", code}, {"headId", headId}};
    }

    static Department fromJson(const json& j) {
        return Department(
            j.at("id"),
            j.value("name", ""),
            j.value("code", ""),
            j.value("headId", 0)
        );
    }
};

// ═══════════════════════════════════════════════════════════════
//  КЛАСС 4: Employee — сотрудник
// ═══════════════════════════════════════════════════════════════
class Employee : public Entity {
private:
    string name;
    int    age;
    string email;
    double salary;
    int    departmentId;
    int    positionId;
    string hireDate;
    string status;  // active | vacation | remote

public:
    Employee(int id = 0, string name = "", int age = 0,
             string email = "", double salary = 0,
             int departmentId = 0, int positionId = 0,
             string hireDate = "2024-01-01", string status = "active")
        : Entity(id), name(name), age(age), email(email), salary(salary),
          departmentId(departmentId), positionId(positionId),
          hireDate(hireDate), status(status) {}

    string getName()         const { return name; }
    int    getAge()          const { return age; }
    string getEmail()        const { return email; }
    double getSalary()       const { return salary; }
    int    getDepartmentId() const { return departmentId; }
    int    getPositionId()   const { return positionId; }
    string getHireDate()     const { return hireDate; }
    string getStatus()       const { return status; }

    string getStatusLabel() const {
        if (status == "active")   return "Активен";
        if (status == "vacation") return "В отпуске";
        if (status == "remote")   return "Удалённо";
        return status;
    }

    void setName(string v)        { name = v; }
    void setAge(int v)            { age = v; }
    void setEmail(string v)       { email = v; }
    void setSalary(double v)      { salary = v; }
    void setDepartmentId(int v)   { departmentId = v; }
    void setPositionId(int v)     { positionId = v; }
    void setHireDate(string v)    { hireDate = v; }
    void setStatus(string v)      { status = v; }

    // showInfo() — стиль ZNA (читаемый)
    void showInfo() const override {
        cout << "ID: "       << id
             << " | Имя: "   << name
             << " | Возраст: "<< age
             << " | Email: "  << email
             << " | Зарплата: " << Utils::formatMoney(salary)
             << " | Статус: " << getStatusLabel()
             << "\n";
    }

    // describe() — полиморфизм через Entity*
    string describe() const override {
        return "Сотрудник: " + name
               + " | Статус: " + getStatusLabel()
               + " | Зарплата: " + Utils::formatMoney(salary)
               + " | Принят: " + hireDate;
    }

    json toJson() const {
        return {
            {"id", id}, {"name", name}, {"age", age},
            {"email", email}, {"salary", salary},
            {"departmentId", departmentId}, {"positionId", positionId},
            {"hireDate", hireDate}, {"status", status}
        };
    }

    static Employee fromJson(const json& j) {
        return Employee(
            j.at("id"), j.value("name",""), j.value("age", 0),
            j.value("email",""), j.value("salary", 0.0),
            j.value("departmentId", 0), j.value("positionId", 0),
            j.value("hireDate","2024-01-01"), j.value("status","active")
        );
    }
};

// ═══════════════════════════════════════════════════════════════
//  КЛАСС 5: DataRepository — хранилище (из моего проекта)
//  Использует nlohmann/json (из ZNA) — лучшее из двух
// ═══════════════════════════════════════════════════════════════
class DataRepository {
private:
    string filename;

public:
    explicit DataRepository(const string& fn) : filename(fn) {}

    bool save(const vector<Department>& depts,
              const vector<Position>&   positions,
              const vector<Employee>&   employees,
              int nextId) const
    {
        json j;
        for (auto& d : depts)     j["departments"].push_back(d.toJson());
        for (auto& p : positions) j["positions"].push_back(p.toJson());
        for (auto& e : employees) j["employees"].push_back(e.toJson());
        j["nextId"] = nextId;

        ofstream file(filename);
        if (!file.is_open()) return false;
        file << setw(4) << j;
        return true;
    }

    bool load(vector<Department>& depts,
              vector<Position>&   positions,
              vector<Employee>&   employees,
              int& nextId) const
    {
        ifstream file(filename);
        if (!file.is_open()) return false;

        json j;
        file >> j;

        if (j.contains("departments"))
            for (auto& item : j["departments"])
                depts.push_back(Department::fromJson(item));

        if (j.contains("positions"))
            for (auto& item : j["positions"])
                positions.push_back(Position::fromJson(item));

        if (j.contains("employees"))
            for (auto& item : j["employees"])
                employees.push_back(Employee::fromJson(item));

        nextId = j.value("nextId", 1);
        return true;
    }

    string getFilename() const { return filename; }
};

// ═══════════════════════════════════════════════════════════════
//  КЛАСС 6: EnterpriseManager — главный контроллер
//  Полный CRUD из моего + int ID как в ZNA
// ═══════════════════════════════════════════════════════════════
class EnterpriseManager {
private:
    vector<Employee>   employees;
    vector<Department> departments;
    vector<Position>   positions;
    DataRepository     repo;
    int                nextId;

    void persist() {
        repo.save(departments, positions, employees, nextId);
    }

    void seedData() {
        addDepartment("Development",  "DEV");
        addDepartment("Engineering",  "ENG");
        addDepartment("HR",           "HR");
        addDepartment("Finance",      "FIN");

        addPosition("Генеральный директор",  "Управленческий персонал", "director");
        addPosition("Senior Developer",      "Технический персонал",    "senior");
        addPosition("Middle Developer",      "Технический персонал",    "middle");
        addPosition("HR Manager",            "HR и обучение",           "lead");
        addPosition("Financial Analyst",     "Финансовый персонал",     "senior");

        addEmployee("Alex Ivanov",    30, "alex@corp.com",  150000, 1, 6, "2021-03-15", "active");
        addEmployee("Maria Petrova",  27, "maria@corp.com", 110000, 1, 7, "2022-06-01", "active");
        addEmployee("Ivan Smirnov",   35, "ivan@corp.com",   95000, 2, 7, "2022-09-12", "remote");
        addEmployee("Anna Volkova",   29, "anna@corp.com",   80000, 3, 8, "2020-11-20", "vacation");
        addEmployee("Denis Orlov",    33, "denis@corp.com", 120000, 4, 9, "2023-01-09", "active");

        // Назначить руководителей (id отделов 1-4, id сотрудников 6-10)
        auto* dev = findDept(1); if (dev) dev->setHeadId(6);
        auto* hr  = findDept(3); if (hr)  hr->setHeadId(9);
        persist();
    }

public:
    explicit EnterpriseManager(const string& dataFile)
        : repo(dataFile), nextId(1)
    {
        if (!repo.load(departments, positions, employees, nextId))
            seedData();
    }

    // ─── generateId() — стиль ZNA (простой счётчик) ───
    int generateId() { return nextId++; }

    // ─── Поиск по id ───
    Department* findDept(int id) {
        for (auto& d : departments) if (d.getId() == id) return &d;
        return nullptr;
    }
    Position* findPosition(int id) {
        for (auto& p : positions) if (p.getId() == id) return &p;
        return nullptr;
    }
    Employee* findEmployee(int id) {
        for (auto& e : employees) if (e.getId() == id) return &e;
        return nullptr;
    }

    vector<Department>& getDepartments() { return departments; }
    vector<Position>&   getPositions()   { return positions; }
    vector<Employee>&   getEmployees()   { return employees; }

    // ─── CRUD: Сотрудники ───
    void addEmployee(string name, int age, string email, double salary,
                     int deptId, int posId, string hireDate, string status)
    {
        employees.push_back(Employee(generateId(), name, age, email,
                                     salary, deptId, posId, hireDate, status));
        persist();
    }

    bool updateEmployee(int id, Employee updated) {
        auto* e = findEmployee(id);
        if (!e) return false;
        *e = updated;
        persist();
        return true;
    }

    bool deleteEmployee(int id) {
        auto it = remove_if(employees.begin(), employees.end(),
            [id](const Employee& e){ return e.getId() == id; });
        if (it == employees.end()) return false;
        employees.erase(it, employees.end());
        for (auto& d : departments)
            if (d.getHeadId() == id) d.setHeadId(0);
        persist();
        return true;
    }

    // ─── CRUD: Отделы ───
    void addDepartment(string name, string code) {
        departments.push_back(Department(generateId(), name, code, 0));
        persist();
    }

    bool updateDepartment(int id, Department updated) {
        auto* d = findDept(id);
        if (!d) return false;
        *d = updated;
        persist();
        return true;
    }

    bool deleteDepartment(int id) {
        auto it = remove_if(departments.begin(), departments.end(),
            [id](const Department& d){ return d.getId() == id; });
        if (it == departments.end()) return false;
        departments.erase(it, departments.end());
        for (auto& e : employees)
            if (e.getDepartmentId() == id) e.setDepartmentId(0);
        persist();
        return true;
    }

    // ─── CRUD: Должности ───
    void addPosition(string title, string category, string level) {
        positions.push_back(Position(generateId(), title, category, level));
        persist();
    }

    bool deletePosition(int id) {
        auto it = remove_if(positions.begin(), positions.end(),
            [id](const Position& p){ return p.getId() == id; });
        if (it == positions.end()) return false;
        positions.erase(it, positions.end());
        for (auto& e : employees)
            if (e.getPositionId() == id) e.setPositionId(0);
        persist();
        return true;
    }

    // ─── Поиск по 3 полям (из моего) ───
    vector<Employee*> searchEmployees(const string& query) {
        string q = Utils::toLower(query);
        vector<Employee*> result;
        for (auto& e : employees) {
            bool matchName = Utils::toLower(e.getName()).find(q) != string::npos;
            bool matchEmail= Utils::toLower(e.getEmail()).find(q) != string::npos;
            auto* d = findDept(e.getDepartmentId());
            auto* p = findPosition(e.getPositionId());
            bool matchDept = d && Utils::toLower(d->getName()).find(q) != string::npos;
            bool matchPos  = p && Utils::toLower(p->getTitle()).find(q) != string::npos;
            if (matchName || matchEmail || matchDept || matchPos)
                result.push_back(&e);
        }
        return result;
    }

    // ─── 4 режима сортировки (из моего) ───
    vector<Employee> sortEmployees(const string& by) {
        vector<Employee> sorted = employees;
        if (by == "name")
            sort(sorted.begin(), sorted.end(),
                [](const Employee& a, const Employee& b){ return a.getName() < b.getName(); });
        else if (by == "salary_asc")
            sort(sorted.begin(), sorted.end(),
                [](const Employee& a, const Employee& b){ return a.getSalary() < b.getSalary(); });
        else if (by == "salary_desc")
            sort(sorted.begin(), sorted.end(),
                [](const Employee& a, const Employee& b){ return a.getSalary() > b.getSalary(); });
        else if (by == "hire")
            sort(sorted.begin(), sorted.end(),
                [](const Employee& a, const Employee& b){ return a.getHireDate() < b.getHireDate(); });
        return sorted;
    }

    // ─── Статистика (стиль ZNA + расширение) ───
    void showStatistics() const {
        Utils::title("СТАТИСТИКА ПРЕДПРИЯТИЯ");
        double total = 0;
        int active = 0, vacation = 0, remote = 0;
        for (auto& e : employees) {
            total += e.getSalary();
            if (e.getStatus() == "active")   active++;
            if (e.getStatus() == "vacation") vacation++;
            if (e.getStatus() == "remote")   remote++;
        }
        cout << "  Сотрудников:       " << employees.size()   << "\n";
        cout << "    — активных:      " << active              << "\n";
        cout << "    — в отпуске:     " << vacation            << "\n";
        cout << "    — удалённо:      " << remote              << "\n";
        cout << "  Отделов:           " << departments.size()  << "\n";
        cout << "  Должностей:        " << positions.size()    << "\n";
        Utils::separator();
        cout << "  Фонд зарплаты:     " << Utils::formatMoney(total) << "/мес\n";
        if (!employees.empty())
            cout << "  Средняя зарплата:  "
                 << Utils::formatMoney(total / employees.size()) << "\n";
        cout << "  Файл данных:       " << repo.getFilename() << "\n\n";
    }

    // ─── Полиморфная демонстрация через Entity* (из моего) ───
    void showOopDemo() const {
        Utils::title("ДЕМОНСТРАЦИЯ ПОЛИМОРФИЗМА");
        cout << "  Вызов describe() на объектах разных классов через Entity*:\n";
        Utils::separator();

        // Собираем все объекты под общим интерфейсом Entity*
        vector<const Entity*> all;
        for (auto& d : departments) all.push_back(&d);
        for (auto& p : positions)   all.push_back(&p);
        for (auto& e : employees)   all.push_back(&e);

        for (size_t i = 0; i < all.size(); i++)
            cout << "  " << setw(3) << (i+1) << ". " << all[i]->describe() << "\n";
        cout << "\n";
    }

    string getDataFile() const { return repo.getFilename(); }
};

// ═══════════════════════════════════════════════════════════════
//  КЛАСС 7: ConsoleUI — интерфейс
//  Структура из ZNA (do-while, стиль вывода) +
//  вложенные подменю, CRUD и структура из моего проекта
// ═══════════════════════════════════════════════════════════════
class ConsoleUI {
private:
    EnterpriseManager manager;

    // Выбрать отдел из списка
    int selectDept() {
        auto& depts = manager.getDepartments();
        if (depts.empty()) { cout << "  (Нет отделов)\n"; return 0; }
        for (auto& d : depts)
            cout << "  " << d.getId() << ". " << d.getName()
                 << " (" << d.getCode() << ")\n";
        cout << "  0. — без отдела\n";
        return Utils::inputInt("  ID отдела: ", 0, 99999);
    }

    // Выбрать должность из списка
    int selectPos() {
        auto& pos = manager.getPositions();
        if (pos.empty()) { cout << "  (Нет должностей)\n"; return 0; }
        for (auto& p : pos)
            cout << "  " << p.getId() << ". " << p.getTitle()
                 << " [" << p.getLevelLabel() << "]\n";
        cout << "  0. — без должности\n";
        return Utils::inputInt("  ID должности: ", 0, 99999);
    }

    // Выбрать статус
    string selectStatus() {
        cout << "  1. Активен\n  2. В отпуске\n  3. Удалённо\n";
        int ch = Utils::inputInt("  Выбор: ", 1, 3);
        if (ch == 1) return "active";
        if (ch == 2) return "vacation";
        return "remote";
    }

    void printEmpHeader() {
        cout << "\n  " << left
             << setw(5)  << "ID"
             << setw(22) << "Имя"
             << setw(8)  << "Возраст"
             << setw(20) << "Email"
             << setw(14) << "Зарплата"
             << setw(12) << "Статус"
             << "\n";
        Utils::separator('-', 81);
    }

    void printEmpRow(const Employee& e) {
        cout << "  " << left
             << setw(5)  << e.getId()
             << setw(22) << e.getName().substr(0, 20)
             << setw(8)  << e.getAge()
             << setw(20) << e.getEmail().substr(0, 18)
             << setw(14) << (to_string((int)e.getSalary()) + " р.")
             << setw(12) << e.getStatusLabel()
             << "\n";
    }

public:
    explicit ConsoleUI(const string& dataFile) : manager(dataFile) {}

    // ════════════ ГЛАВНОЕ МЕНЮ (стиль ZNA) ════════════
    void run() {
        int choice;
        do {
            Utils::title("ZNA Technologies — Электронный образ предприятия");
            cout << "  1. Сотрудники\n";
            cout << "  2. Отделы\n";
            cout << "  3. Должности\n";
            cout << "  4. Структура предприятия\n";
            cout << "  5. Статистика\n";
            cout << "  6. Демонстрация ООП\n";
            cout << "  0. Выход\n";
            Utils::separator();
            choice = Utils::inputInt("  Выбор: ", 0, 6);
            switch (choice) {
                case 1: menuEmployees();              break;
                case 2: menuDepartments();            break;
                case 3: menuPositions();              break;
                case 4: showStructure();              break;
                case 5: manager.showStatistics();     break;
                case 6: manager.showOopDemo();        break;
                case 0: cout << "\n  Выход...\n\n";  break;
                default: cout << "  Неверный выбор.\n";
            }
        } while (choice != 0);
    }

    // ════════════ МЕНЮ СОТРУДНИКОВ ════════════
    void menuEmployees() {
        int choice;
        do {
            Utils::subtitle("СОТРУДНИКИ");
            cout << "  1. Показать всех\n";
            cout << "  2. Добавить\n";
            cout << "  3. Редактировать\n";
            cout << "  4. Удалить\n";
            cout << "  5. Поиск\n";
            cout << "  6. Сортировка\n";
            cout << "  0. Назад\n";
            Utils::separator('-');
            choice = Utils::inputInt("  Выбор: ", 0, 6);
            switch (choice) {
                case 1: showEmployees(manager.getEmployees()); break;
                case 2: addEmployee();    break;
                case 3: editEmployee();   break;
                case 4: deleteEmployee(); break;
                case 5: searchEmployee(); break;
                case 6: sortEmployees();  break;
            }
        } while (choice != 0);
    }

    void showEmployees(const vector<Employee>& list) {
        if (list.empty()) { cout << "\n  Сотрудников нет.\n"; return; }
        printEmpHeader();
        for (auto& e : list) printEmpRow(e);
        cout << "\n  Всего: " << list.size() << " чел.\n";
    }

    void addEmployee() {
        Utils::subtitle("ДОБАВИТЬ СОТРУДНИКА");
        string name  = Utils::inputLine("  Имя:            ");
        int    age   = Utils::inputInt ("  Возраст:        ", 16, 100);
        string email = Utils::inputLine("  Email:          ");
        double sal   = Utils::inputInt ("  Зарплата (р.):  ", 0, 10000000);
        string hd    = Utils::inputLine("  Дата найма (ГГГГ-ММ-ДД): ");

        cout << "\n  Отделы:\n";
        int dId = selectDept();
        cout << "\n  Должности:\n";
        int pId = selectPos();
        string st = selectStatus();

        manager.addEmployee(name, age, email, sal, dId, pId, hd, st);
        cout << "\n  ✓ Сотрудник добавлен.\n";
    }

    void editEmployee() {
        Utils::subtitle("РЕДАКТИРОВАТЬ СОТРУДНИКА");
        auto& emps = manager.getEmployees();
        if (emps.empty()) { cout << "  Нет сотрудников.\n"; return; }
        showEmployees(emps);

        int id = Utils::inputInt("  ID сотрудника (0 — отмена): ", 0, 99999);
        if (id == 0) return;
        auto* ptr = manager.findEmployee(id);
        if (!ptr) { cout << "  Сотрудник не найден.\n"; return; }

        Employee e = *ptr;
        cout << "  (Enter — оставить без изменений)\n";

        auto ask = [&](const string& p, const string& cur) {
            cout << "  " << p << " [" << cur << "]: ";
            string s; getline(cin, s); s = Utils::trim(s);
            return s.empty() ? cur : s;
        };

        e.setName (ask("Имя",      e.getName()));
        string ageStr = ask("Возраст", to_string(e.getAge()));
        try { e.setAge(stoi(ageStr)); } catch(...) {}
        e.setEmail(ask("Email",    e.getEmail()));
        string salStr = ask("Зарплата", to_string((int)e.getSalary()));
        try { e.setSalary(stod(salStr)); } catch(...) {}
        e.setHireDate(ask("Дата найма", e.getHireDate()));

        cout << "  Изменить отдел? (1-да/0-нет): ";
        string yn; getline(cin, yn);
        if (Utils::trim(yn) == "1") {
            cout << "  Отделы:\n"; e.setDepartmentId(selectDept());
        }
        cout << "  Изменить должность? (1-да/0-нет): ";
        getline(cin, yn);
        if (Utils::trim(yn) == "1") {
            cout << "  Должности:\n"; e.setPositionId(selectPos());
        }
        cout << "  Изменить статус? (1-да/0-нет): ";
        getline(cin, yn);
        if (Utils::trim(yn) == "1") e.setStatus(selectStatus());

        manager.updateEmployee(id, e);
        cout << "\n  ✓ Данные обновлены.\n";
    }

    void deleteEmployee() {
        Utils::subtitle("УДАЛИТЬ СОТРУДНИКА");
        auto& emps = manager.getEmployees();
        if (emps.empty()) { cout << "  Нет сотрудников.\n"; return; }
        showEmployees(emps);
        int id = Utils::inputInt("  ID для удаления (0 — отмена): ", 0, 99999);
        if (id == 0) return;
        auto* e = manager.findEmployee(id);
        if (!e) { cout << "  Не найден.\n"; return; }
        cout << "  Удалить «" << e->getName() << "»? (1-да/0-нет): ";
        string yn; getline(cin, yn);
        if (Utils::trim(yn) == "1") {
            manager.deleteEmployee(id);
            cout << "  ✓ Удалён.\n";
        }
    }

    void searchEmployee() {
        string q = Utils::inputLine("  Поиск (имя / email / отдел / должность): ");
        auto results = manager.searchEmployees(q);
        cout << "\n  Найдено: " << results.size() << "\n";
        if (!results.empty()) {
            printEmpHeader();
            for (auto* e : results) printEmpRow(*e);
        }
    }

    void sortEmployees() {
        Utils::subtitle("СОРТИРОВКА");
        cout << "  1. По имени (А-Я)\n";
        cout << "  2. По зарплате ↑\n";
        cout << "  3. По зарплате ↓\n";
        cout << "  4. По дате найма\n";
        int ch = Utils::inputInt("  Выбор: ", 1, 4);
        string by[] = {"name","salary_asc","salary_desc","hire"};
        showEmployees(manager.sortEmployees(by[ch-1]));
    }

    // ════════════ МЕНЮ ОТДЕЛОВ ════════════
    void menuDepartments() {
        int choice;
        do {
            Utils::subtitle("ОТДЕЛЫ");
            cout << "  1. Показать все\n";
            cout << "  2. Добавить\n";
            cout << "  3. Редактировать\n";
            cout << "  4. Удалить\n";
            cout << "  0. Назад\n";
            Utils::separator('-');
            choice = Utils::inputInt("  Выбор: ", 0, 4);
            switch (choice) {
                case 1: showDepartments(); break;
                case 2: addDept();         break;
                case 3: editDept();        break;
                case 4: deleteDept();      break;
            }
        } while (choice != 0);
    }

    void showDepartments() {
        auto& depts = manager.getDepartments();
        if (depts.empty()) { cout << "\n  Нет отделов.\n"; return; }
        cout << "\n  " << left << setw(6) << "ID" << setw(24) << "Название"
             << setw(8) << "Код" << setw(20) << "Руководитель"
             << setw(8) << "Сотр." << "\n";
        Utils::separator('-', 66);
        for (auto& d : depts) {
            int cnt = 0;
            for (auto& e : manager.getEmployees())
                if (e.getDepartmentId() == d.getId()) cnt++;
            auto* head = manager.findEmployee(d.getHeadId());
            cout << "  " << left
                 << setw(6)  << d.getId()
                 << setw(24) << d.getName().substr(0,22)
                 << setw(8)  << d.getCode()
                 << setw(20) << (head ? head->getName().substr(0,18) : "— не назначен —")
                 << setw(8)  << cnt << "\n";
        }
    }

    void addDept() {
        Utils::subtitle("ДОБАВИТЬ ОТДЕЛ");
        string name = Utils::inputLine("  Название: ");
        string code = Utils::inputLine("  Код (напр. DEV): ");
        manager.addDepartment(name, code);
        cout << "  ✓ Отдел добавлен.\n";
    }

    void editDept() {
        auto& depts = manager.getDepartments();
        if (depts.empty()) { cout << "  Нет отделов.\n"; return; }
        showDepartments();
        int id = Utils::inputInt("  ID отдела (0 — отмена): ", 0, 99999);
        if (id == 0) return;
        auto* ptr = manager.findDept(id);
        if (!ptr) { cout << "  Не найден.\n"; return; }
        Department d = *ptr;

        auto ask = [&](const string& p, const string& cur) {
            cout << "  " << p << " [" << cur << "]: ";
            string s; getline(cin, s); s = Utils::trim(s);
            return s.empty() ? cur : s;
        };
        d.setName(ask("Название", d.getName()));
        d.setCode(ask("Код",      d.getCode()));

        cout << "  Назначить руководителя? (1-да/0-нет): ";
        string yn; getline(cin, yn);
        if (Utils::trim(yn) == "1") {
            showEmployees(manager.getEmployees());
            d.setHeadId(Utils::inputInt("  ID сотрудника: ", 0, 99999));
        }
        manager.updateDepartment(id, d);
        cout << "  ✓ Отдел обновлён.\n";
    }

    void deleteDept() {
        auto& depts = manager.getDepartments();
        if (depts.empty()) { cout << "  Нет отделов.\n"; return; }
        showDepartments();
        int id = Utils::inputInt("  ID для удаления (0 — отмена): ", 0, 99999);
        if (id == 0) return;
        auto* d = manager.findDept(id);
        if (!d) { cout << "  Не найден.\n"; return; }
        cout << "  Удалить «" << d->getName() << "»? (1-да/0-нет): ";
        string yn; getline(cin, yn);
        if (Utils::trim(yn) == "1") {
            manager.deleteDepartment(id);
            cout << "  ✓ Отдел удалён.\n";
        }
    }

    // ════════════ МЕНЮ ДОЛЖНОСТЕЙ ════════════
    void menuPositions() {
        int choice;
        do {
            Utils::subtitle("ДОЛЖНОСТИ");
            cout << "  1. Показать все\n";
            cout << "  2. Добавить\n";
            cout << "  3. Удалить\n";
            cout << "  0. Назад\n";
            Utils::separator('-');
            choice = Utils::inputInt("  Выбор: ", 0, 3);
            switch (choice) {
                case 1: showPositions();  break;
                case 2: addPosition();    break;
                case 3: deletePosition(); break;
            }
        } while (choice != 0);
    }

    void showPositions() {
        auto& pos = manager.getPositions();
        if (pos.empty()) { cout << "\n  Нет должностей.\n"; return; }
        cout << "\n  " << left << setw(6) << "ID" << setw(26) << "Название"
             << setw(14) << "Уровень" << setw(26) << "Категория"
             << setw(6)  << "Сотр." << "\n";
        Utils::separator('-', 78);
        for (auto& p : pos) {
            int cnt = 0;
            for (auto& e : manager.getEmployees())
                if (e.getPositionId() == p.getId()) cnt++;
            cout << "  " << left
                 << setw(6)  << p.getId()
                 << setw(26) << p.getTitle().substr(0,24)
                 << setw(14) << p.getLevelLabel()
                 << setw(26) << p.getCategory().substr(0,24)
                 << setw(6)  << cnt << "\n";
        }
    }

    void addPosition() {
        Utils::subtitle("ДОБАВИТЬ ДОЛЖНОСТЬ");
        string title = Utils::inputLine("  Название: ");
        cout << "  Категория:\n"
             << "  1. Технический персонал\n  2. Управленческий персонал\n"
             << "  3. Финансовый персонал\n  4. Маркетинг и продажи\n"
             << "  5. HR и обучение\n";
        int cc = Utils::inputInt("  Выбор: ", 1, 5);
        string cats[] = {"Технический персонал","Управленческий персонал",
                          "Финансовый персонал","Маркетинг и продажи","HR и обучение"};
        cout << "  Уровень: 1.Junior  2.Middle  3.Senior  4.Lead  5.Директор\n";
        int lc = Utils::inputInt("  Выбор: ", 1, 5);
        string lvls[] = {"junior","middle","senior","lead","director"};
        manager.addPosition(title, cats[cc-1], lvls[lc-1]);
        cout << "  ✓ Должность добавлена.\n";
    }

    void deletePosition() {
        auto& pos = manager.getPositions();
        if (pos.empty()) { cout << "  Нет должностей.\n"; return; }
        showPositions();
        int id = Utils::inputInt("  ID для удаления (0 — отмена): ", 0, 99999);
        if (id == 0) return;
        auto* p = manager.findPosition(id);
        if (!p) { cout << "  Не найдена.\n"; return; }
        cout << "  Удалить «" << p->getTitle() << "»? (1-да/0-нет): ";
        string yn; getline(cin, yn);
        if (Utils::trim(yn) == "1") {
            manager.deletePosition(id);
            cout << "  ✓ Должность удалена.\n";
        }
    }

    // ════════════ СТРУКТУРА ПРЕДПРИЯТИЯ (из моего) ════════════
    void showStructure() {
        Utils::title("СТРУКТУРА ПРЕДПРИЯТИЯ");
        auto& depts = manager.getDepartments();
        if (depts.empty()) { cout << "  Нет отделов.\n"; return; }

        for (auto& d : depts) {
            auto* head = manager.findEmployee(d.getHeadId());
            cout << "\n  ┌─ [" << d.getCode() << "] " << d.getName();
            if (head) cout << "   Рук.: " << head->getName();
            cout << "\n";

            vector<Employee*> dEmps;
            for (auto& e : manager.getEmployees())
                if (e.getDepartmentId() == d.getId()) dEmps.push_back(&e);

            for (size_t i = 0; i < dEmps.size(); i++) {
                auto* e = dEmps[i];
                auto* p = manager.findPosition(e->getPositionId());
                bool last = (i == dEmps.size() - 1);
                cout << "  " << (last ? "└──" : "├──")
                     << " " << e->getName()
                     << "  [" << (p ? p->getTitle() : "—") << "]"
                     << "  " << e->getStatusLabel()
                     << "  " << Utils::formatMoney(e->getSalary())
                     << "\n";
            }
            if (dEmps.empty()) cout << "  └── (нет сотрудников)\n";
        }
        cout << "\n";
    }
};

// ═══════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════
int main() {
    setlocale(LC_ALL, "ru_RU.UTF-8");
    ConsoleUI app("enterprise_combined.json");
    app.run();
    return 0;
}
