from flask import Flask, request, jsonify
import sqlite3
from datetime import datetime
import json

app = Flask(__name__)
DB_PATH = 'attendance.db'

def get_db_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

@app.route('/checkin', methods=['POST'])
def checkin():
    try:
        data = request.get_json()
        token = data.get('token')
        session_id = data.get('session_id', 1)  # Default to session 1
        
        print(f"Received check-in request for token: {token}")
        
        if not token:
            return jsonify({'error': 'Missing token'}), 400

        conn = get_db_connection()
        cursor = conn.cursor()

        # Look up employee by token
        cursor.execute("SELECT EmployeeID, Name FROM Employees WHERE Token = ?", (token,))
        employee = cursor.fetchone()

        if not employee:
            conn.close()
            print(f"Invalid token: {token}")
            return jsonify({
                'success': False,
                'message': 'Invalid token'
            }), 401

        employee_id = employee['EmployeeID']
        employee_name = employee['Name']
        
        print(f"Found employee: {employee_name} (ID: {employee_id})")

        # Check if already checked in for this session
        cursor.execute("""
            SELECT CheckinID FROM CheckinHistory 
            WHERE EmployeeID = ? AND SessionID = ?
        """, (employee_id, session_id))
        
        existing_checkin = cursor.fetchone()
        
        if existing_checkin:
            conn.close()
            print(f"Employee {employee_name} already checked in for session {session_id}")
            return jsonify({
                'success': False,
                'message': f'{employee_name} already checked in'
            }), 409

        # Determine check status (simplified - you can add time logic here)
        current_time = datetime.now()
        check_status = 'on time'  # You can add logic to determine late/on time

        # Insert check-in record
        cursor.execute("""
            INSERT INTO CheckinHistory (EmployeeID, SessionID, CheckStatus)
            VALUES (?, ?, ?)
        """, (employee_id, session_id, check_status))

        conn.commit()
        conn.close()
        
        print(f"Check-in successful for {employee_name}")
        return jsonify({
            'success': True,
            'message': f'Welcome {employee_name}!',
            'employee_name': employee_name,
            'check_status': check_status
        }), 200

    except Exception as e:
        print(f"Error processing check-in: {str(e)}")
        return jsonify({
            'success': False,
            'message': 'Server error'
        }), 500

@app.route('/employees', methods=['GET'])
def get_employees():
    """Debug endpoint to see all employees and tokens"""
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT EmployeeID, Name, Token FROM Employees")
    employees = cursor.fetchall()
    conn.close()
    
    result = []
    for emp in employees:
        result.append({
            'id': emp['EmployeeID'],
            'name': emp['Name'],
            'token': emp['Token']
        })
    
    return jsonify(result)

@app.route('/history', methods=['GET'])
def get_history():
    """Debug endpoint to see check-in history"""
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("""
        SELECT h.CheckinID, e.Name, h.CheckinTime, h.CheckStatus, h.SessionID
        FROM CheckinHistory h
        JOIN Employees e ON h.EmployeeID = e.EmployeeID
        ORDER BY h.CheckinTime DESC
    """)
    history = cursor.fetchall()
    conn.close()
    
    result = []
    for record in history:
        result.append({
            'checkin_id': record['CheckinID'],
            'employee_name': record['Name'],
            'checkin_time': record['CheckinTime'],
            'status': record['CheckStatus'],
            'session_id': record['SessionID']
        })
    
    return jsonify(result)

if __name__ == '__main__':
    # Change from app.run() to:
    app.run(host='0.0.0.0', port=5000)
