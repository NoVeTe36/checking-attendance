import sqlite3
from collections import defaultdict
from datetime import datetime
import calendar

def generate_monthly_sessions(year, month):
    days_in_month = calendar.monthrange(year, month)[1]
    session_defs = []
    for day in range(1, days_in_month + 1):
        date = datetime(year, month, day)
        weekday = date.weekday()
        if weekday == 6:  # Sunday: skip
            continue
        for slot, slot_name in enumerate(['Morning', 'Afternoon', 'Evening']):
            if weekday == 5 and slot == 2:
                continue  # Saturday evening: skip
            for role in ['Cashier', 'Supervisor']:
                session_defs.append((date.strftime("%Y-%m-%d"), slot_name, role, slot))
    return session_defs

def auto_schedule(year, month):
    conn = sqlite3.connect("attendance.db")
    cursor = conn.cursor()

    # Drop and create Sessions table
    cursor.execute("DROP TABLE IF EXISTS Sessions")
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS Sessions (
        SessionID INTEGER PRIMARY KEY AUTOINCREMENT,
        Date TEXT NOT NULL,
        TimeSlot TEXT NOT NULL,
        Role TEXT NOT NULL,
        EmployeeID INTEGER,
        SlotIndex INTEGER,
        FOREIGN KEY (EmployeeID) REFERENCES Employees(EmployeeID)
    );
    """)

    # Load employees
    cursor.execute("SELECT EmployeeID, Name, Role FROM Employees")
    employees = []
    for row in cursor.fetchall():
        employees.append({
            "EmployeeID": row[0],
            "Name": row[1],
            "Role": row[2]
        })

    # Group employees by role
    role_employees = defaultdict(list)
    for emp in employees:
        role_employees[emp["Role"]].append(emp)

    # Generate sessions for the month
    sessions = generate_monthly_sessions(year, month)

    # Assignment logic: evening counts as 1.5 for balancing, but NOT more slots
    emp_weighted_work = defaultdict(float)
    session_assignments = []
    for date, timeslot, role, slot in sessions:
        candidates = role_employees[role]
        # Assign to employee with minimum weighted work so far
        best_emp = min(candidates, key=lambda e: emp_weighted_work[e["EmployeeID"]])
        weight = 1.5 if timeslot == "Evening" else 1.0
        emp_weighted_work[best_emp["EmployeeID"]] += weight
        session_assignments.append((date, timeslot, role, best_emp["EmployeeID"], slot))

    # Insert sessions with assignments into the database
    cursor.executemany("""
    INSERT INTO Sessions (Date, TimeSlot, Role, EmployeeID, SlotIndex)
    VALUES (?, ?, ?, ?, ?)
    """, session_assignments)

    conn.commit()
    conn.close()
    print(f"Scheduled and inserted {len(session_assignments)} sessions for {year}-{month:02d}.")


if __name__ == "__main__":
    auto_schedule(2025, 6)