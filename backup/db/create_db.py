import sqlite3
from datetime import datetime

# Connect to SQLite
conn = sqlite3.connect("attendance.db")
cursor = conn.cursor()

# Drop existing tables if they exist (for clean setup)
cursor.execute("DROP TABLE IF EXISTS CheckinHistory")
cursor.execute("DROP TABLE IF EXISTS Employees")
cursor.execute("DROP TABLE IF EXISTS Sessions")

# Employees table with Token
cursor.execute("""
CREATE TABLE IF NOT EXISTS Employees (
    EmployeeID INTEGER PRIMARY KEY AUTOINCREMENT,
    Name TEXT NOT NULL,
    DateOfBirth DATE NOT NULL,
    Email TEXT UNIQUE NOT NULL,
    Token TEXT UNIQUE NOT NULL  -- Used for authentication (the random code)
);
""")

# Sessions table
cursor.execute("""
CREATE TABLE IF NOT EXISTS Sessions (
    SessionID INTEGER PRIMARY KEY AUTOINCREMENT,
    DateTime DATETIME NOT NULL,
    SessionName TEXT NOT NULL
);
""")

# CheckinHistory table
cursor.execute("""
CREATE TABLE IF NOT EXISTS CheckinHistory (
    CheckinID INTEGER PRIMARY KEY AUTOINCREMENT,
    EmployeeID INTEGER NOT NULL,
    SessionID INTEGER NOT NULL,
    CheckinTime DATETIME DEFAULT CURRENT_TIMESTAMP,
    CheckStatus TEXT CHECK(CheckStatus IN ('on time', 'late', 'absent')) NOT NULL,
    FOREIGN KEY (EmployeeID) REFERENCES Employees(EmployeeID),
    FOREIGN KEY (SessionID) REFERENCES Sessions(SessionID),
    UNIQUE(EmployeeID, SessionID)
);
""")

# Insert sample employees with tokens
cursor.execute("""
INSERT INTO Employees (Name, DateOfBirth, Email, Token) VALUES
('John Doe', '1990-01-01', 'john.doe@example.com', 'ABC123'),
('Jane Smith', '1992-02-02', 'jane.smith@example.com', 'DEF456'),
('Bob Johnson', '1985-05-15', 'bob.johnson@example.com', 'GHI789'),
('Alice Brown', '1993-08-20', 'alice.brown@example.com', 'JKL012')
""")

# Insert a sample session
cursor.execute("""
INSERT INTO Sessions (DateTime, SessionName) VALUES
('2025-06-02 09:00:00', 'Morning Meeting'),
('2025-06-02 14:00:00', 'Afternoon Session')
""")

# Commit and close
conn.commit()
conn.close()

print("Database created successfully!")
print("Sample tokens: ABC123, DEF456, GHI789, JKL012")