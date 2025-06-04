import sqlite3
from auto_scheduler import auto_schedule

conn = sqlite3.connect("attendance.db")
cursor = conn.cursor()

# Drop existing tables to recreate with correct schema
cursor.execute("DROP TABLE IF EXISTS CheckinHistory")
cursor.execute("DROP TABLE IF EXISTS Employees")
cursor.execute("DROP TABLE IF EXISTS Sessions")
cursor.execute("DROP TABLE IF EXISTS cert")

cursor.execute("""
CREATE TABLE IF NOT EXISTS Employees (
    EmployeeID INTEGER PRIMARY KEY AUTOINCREMENT,
    Name TEXT NOT NULL,
    Role TEXT NOT NULL
);
""")

cursor.execute("""
create table if not exists cert (
    id varchar(6),
    pub_key varchar(130),
    valid_until datetime,
    token varchar(6),
    issued integer,
    EmployeeID integer
);
 """)

cursor.execute("""
CREATE TABLE IF NOT EXISTS CheckinHistory (
    CheckinID INTEGER PRIMARY KEY AUTOINCREMENT,
    EmployeeID INTEGER NOT NULL,
    SessionID INTEGER NOT NULL,
    FirstCheckinTime DATETIME DEFAULT CURRENT_TIMESTAMP,
    LastCheckinTime DATETIME DEFAULT CURRENT_TIMESTAMP,
    CheckStatus TEXT CHECK(CheckStatus IN ('on time', 'late', 'absent')) NOT NULL,
    FOREIGN KEY (EmployeeID) REFERENCES Employees(EmployeeID),
    FOREIGN KEY (SessionID) REFERENCES Sessions(SessionID)
);
""")

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

cursor.execute("""
INSERT INTO Employees (Name, Role) VALUES
('Emily Johnson', 'Cashier'),
('Michael Smith', 'Supervisor'),
('Sarah Miller', 'Cashier'),
('David Brown', 'Supervisor'),
('Ashley Davis', 'Cashier'),
('James Wilson', 'Supervisor'),
('Jessica Moore', 'Cashier'),
('John Taylor', 'Supervisor'),
('Amanda Anderson', 'Cashier'),
('Robert Thomas', 'Supervisor'),
('Jennifer Jackson', 'Cashier'),
('William White', 'Cashier'),
('Elizabeth Harris', 'Cashier'),
('Christopher Martin', 'Cashier'),
('Patricia Thompson', 'Cashier'),
('Joshua Garcia', 'Cashier'),
('Linda Martinez', 'Cashier'),
('Daniel Robinson', 'Cashier'),
('Barbara Clark', 'Cashier'),
('Matthew Rodriguez', 'Cashier');
""")

conn.commit()
conn.close()

auto_schedule(2025, 6)
print("Database created successfully!")
