import sqlite3
from collections import defaultdict
from datetime import datetime
import calendar

def generate_monthly_sessions(year, month):
    days_in_month = calendar.monthrange(year, month)[1]
    session_defs = []
    slot_requirements = {
        'Morning': {
            'Cashier': 2,
            'Supervisor': 1
        },
        'Afternoon': {
            'Cashier': 3,
            'Supervisor': 1
        },
        'Evening': {
            'Cashier': 1,
            'Supervisor': 1
        }
    }
    for day in range(1, days_in_month + 1):
        date = datetime(year, month, day)
        weekday = date.weekday()
        if weekday == 6:
            continue
        for timeslot in ['Morning', 'Afternoon', 'Evening']:
            if weekday == 5 and timeslot == 'Evening':
                continue
            for role, count in slot_requirements[timeslot].items():
                for slot_index in range(count):
                    session_defs.append((
                        date.strftime("%Y-%m-%d"),
                        timeslot,
                        role,
                        slot_index
                    ))
    return session_defs

def auto_schedule(year, month):
    conn = sqlite3.connect("attendance.db")
    cursor = conn.cursor()
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
    cursor.execute("SELECT EmployeeID, Name, Role FROM Employees")
    employees = []
    for row in cursor.fetchall():
        employees.append({
            "EmployeeID": row[0],
            "Name": row[1],
            "Role": row[2]
        })
    role_employees = defaultdict(list)
    for emp in employees:
        role_employees[emp["Role"]].append(emp)
    sessions = generate_monthly_sessions(year, month)
    session_assignments = []
    role_pools = {
        role: {
            'employees': [e['EmployeeID'] for e in emps],
            'next_index': 0
        }
        for role, emps in role_employees.items()
    }
    for date, timeslot, role, slot_index in sessions:
        pool = role_pools[role]
        emp_id = pool['employees'][pool['next_index']]
        session_assignments.append((date, timeslot, role, emp_id, slot_index))
        pool['next_index'] = (pool['next_index'] + 1) % len(pool['employees'])
    cursor.executemany("""
    INSERT INTO Sessions (Date, TimeSlot, Role, EmployeeID, SlotIndex)
    VALUES (?, ?, ?, ?, ?)
    """, session_assignments)
    conn.commit()
    conn.close()
    print(f"Scheduled and inserted {len(session_assignments)} sessions for {year}-{month:02d}.")

if __name__ == "__main__":
    auto_schedule(2025, 6)
