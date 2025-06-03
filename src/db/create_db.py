import sqlite3
from auto_scheduler import auto_schedule

conn = sqlite3.connect("attendance.db")
cursor = conn.cursor()

# Drop existing tables to recreate with correct schema
cursor.execute("DROP TABLE IF EXISTS CheckinHistory")
cursor.execute("DROP TABLE IF EXISTS Employees")
cursor.execute("DROP TABLE IF EXISTS Sessions")

cursor.execute("""
CREATE TABLE IF NOT EXISTS Employees (
    EmployeeID INTEGER PRIMARY KEY AUTOINCREMENT,
    Name TEXT NOT NULL,
    Role TEXT NOT NULL,
    FirstTime BOOLEAN DEFAULT 1,
    Token TEXT UNIQUE NOT NULL
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
INSERT INTO Employees (Name, Role, Token) VALUES
('Emily Johnson', 'Cashier', 'A7X9K2L1M4PQZ8RW'),
('Michael Smith', 'Supervisor', 'L0Y3Z9KP2X7MWT5Q'),
('Sarah Miller', 'Cashier', 'Q1M7Z5K9Y3L2TXRW'),
('David Brown', 'Supervisor', 'Z8R2K1M4Y7PWTQX9'),
('Ashley Davis', 'Cashier', 'M6Q9Y4X1PTKZ73WL'),
('James Wilson', 'Supervisor', 'T0KPZ4X7Y3MWL9QR'),
('Jessica Moore', 'Cashier', 'W1X8LY3K2P9ZTQRM'),
('John Taylor', 'Supervisor', 'K9MQ2WT7Y4LXPZ13'),
('Amanda Anderson', 'Cashier', 'R3ZQKWT9M1Y84XPL'),
('Robert Thomas', 'Supervisor', 'Y7M2PLQKX9W3ZT01'),
('Jennifer Jackson', 'Cashier', 'X1Y9K4ZQM7TPLW28'),
('William White', 'Supervisor', 'PQXW7K3MLYZT0192'),
('Elizabeth Harris', 'Cashier', 'M4T8PQZLYK193XW7'),
('Christopher Martin', 'Supervisor', 'ZT0KLYW9P3XQ728M'),
('Patricia Thompson', 'Cashier', 'Y2MZXLPTQW379K1R'),
('Joshua Garcia', 'Supervisor', 'LWXTKY37M0QPZ149'),
('Linda Martinez', 'Cashier', 'QZ9WPT3XK7YML240'),
('Daniel Robinson', 'Supervisor', 'KM1LYZXPW473TQ98'),
('Barbara Clark', 'Cashier', '3M9KWLXYTQZP7182'),
('Matthew Rodriguez', 'Supervisor', 'ZKTW8PQ1XLYM9340');
""")

conn.commit()
conn.close()

auto_schedule(2025, 6)
print("Database created successfully!")