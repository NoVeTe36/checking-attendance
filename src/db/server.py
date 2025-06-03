from flask import Flask, request, jsonify
import sqlite3
from datetime import datetime, date
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
        name = data.get('Name')
        token = data.get('token')
        session_id = data.get('session_id', 1)  # Default to session 1
        
        print(f"Received check-in request - Name: {name}, Token: {token}")
        
        # Must have either name or token, but not both
        if not name and not token:
            return jsonify({'error': 'Missing name or token'}), 400
        
        if name and token:
            return jsonify({'error': 'Provide either name or token, not both'}), 400

        conn = get_db_connection()
        cursor = conn.cursor()

        # CASE 1: First-time login with name
        if name:
            print(f"Processing first-time login for name: {name}")
            
            # Look up employee by name
            cursor.execute("SELECT EmployeeID, Name, Token, FirstTime FROM Employees WHERE Name = ?", (name,))
            employee = cursor.fetchone()

            if not employee:
                conn.close()
                print(f"Employee not found: {name}")
                return jsonify({
                    'success': False,
                    'message': 'Employee not found'
                }), 404

            employee_id = employee['EmployeeID']
            employee_name = employee['Name']
            employee_token = employee['Token']
            first_time = employee['FirstTime']
            
            print(f"Found employee: {employee_name} (ID: {employee_id}, FirstTime: {first_time})")

            # Check if this is actually their first time
            if not first_time:
                conn.close()
                print(f"Employee {employee_name} has already completed first-time setup")
                return jsonify({
                    'success': False,
                    'message': 'Employee already registered. Use token for check-in.'
                }), 401

            # Update FirstTime to 0 (false)
            cursor.execute("UPDATE Employees SET FirstTime = 0 WHERE EmployeeID = ?", (employee_id,))
            
            # Process check-in for first time
            success = process_checkin(cursor, employee_id, session_id)
            
            if success:
                conn.commit()
                conn.close()
                print(f"First-time setup successful for {employee_name}")
                return jsonify({
                    'success': True,
                    'message': f'Welcome {employee_name}! First-time setup complete.',
                    'token': employee_token,
                    'employee_name': employee_name
                }), 200
            else:
                conn.rollback()
                conn.close()
                return jsonify({
                    'success': False,
                    'message': 'Check-in processing failed'
                }), 500

        # CASE 2: Regular check-in with token
        elif token:
            print(f"Processing token-based check-in for token: {token}")
            
            # Look up employee by token
            cursor.execute("SELECT EmployeeID, Name, FirstTime FROM Employees WHERE Token = ?", (token,))
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
            first_time = employee['FirstTime']
            
            print(f"Found employee: {employee_name} (ID: {employee_id})")

            # Check if they need to do first-time setup
            if first_time:
                conn.close()
                print(f"Employee {employee_name} needs to complete first-time setup with name")
                return jsonify({
                    'success': False,
                    'message': 'Please complete first-time setup using your name'
                }), 401

            # Process regular check-in
            success = process_checkin(cursor, employee_id, session_id)
            
            if success:
                conn.commit()
                conn.close()
                print(f"Check-in successful for {employee_name}")
                return jsonify({
                    'success': True,
                    'message': f'Welcome back {employee_name}!',
                    'employee_name': employee_name
                }), 200
            else:
                conn.rollback()
                conn.close()
                return jsonify({
                    'success': False,
                    'message': 'Check-in processing failed'
                }), 500

    except Exception as e:
        print(f"Error processing check-in: {str(e)}")
        return jsonify({
            'success': False,
            'message': 'Server error'
        }), 500

def process_checkin(cursor, employee_id, session_id):
    """
    Process check-in logic:
    - If no check-in today: create new record with FirstCheckinTime
    - If already checked in today: update LastCheckinTime
    """
    try:
        current_datetime = datetime.now()
        current_date = current_datetime.date()
        
        print(f"Processing check-in for employee {employee_id} on {current_date}")
        
        # Check if employee has checked in today for this session
        cursor.execute("""
            SELECT CheckinID, FirstCheckinTime, LastCheckinTime 
            FROM CheckinHistory 
            WHERE EmployeeID = ? AND SessionID = ? 
            AND DATE(FirstCheckinTime) = DATE(?)
        """, (employee_id, session_id, current_datetime))
        
        existing_checkin = cursor.fetchone()
        
        if existing_checkin:
            # Update LastCheckinTime for existing check-in today
            checkin_id = existing_checkin['CheckinID']
            print(f"Updating existing check-in {checkin_id} with LastCheckinTime")
            
            cursor.execute("""
                UPDATE CheckinHistory 
                SET LastCheckinTime = ?
                WHERE CheckinID = ?
            """, (current_datetime, checkin_id))
            
            print(f"Updated LastCheckinTime for check-in {checkin_id}")
            
        else:
            # Create new check-in record for today
            print(f"Creating new check-in record for employee {employee_id}")
            
            # Determine check status (simplified - you can add time logic here)
            check_status = 'on time'  # You can add logic to determine late/on time based on session time
            
            cursor.execute("""
                INSERT INTO CheckinHistory (EmployeeID, SessionID, FirstCheckinTime, LastCheckinTime, CheckStatus)
                VALUES (?, ?, ?, ?, ?)
            """, (employee_id, session_id, current_datetime, current_datetime, check_status))
            
            print(f"Created new check-in record for employee {employee_id}")
        
        return True
        
    except Exception as e:
        print(f"Error in process_checkin: {str(e)}")
        return False

@app.route('/employees', methods=['GET'])
def get_employees():
    """Debug endpoint to see all employees and tokens"""
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT EmployeeID, Name, Token, FirstTime FROM Employees")
    employees = cursor.fetchall()
    conn.close()
    
    result = []
    for emp in employees:
        result.append({
            'id': emp['EmployeeID'],
            'name': emp['Name'],
            'token': emp['Token'],
            'first_time': bool(emp['FirstTime'])
        })
    
    return jsonify(result)

@app.route('/history', methods=['GET'])
def get_history():
    """Debug endpoint to see check-in history"""
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("""
        SELECT h.CheckinID, e.Name, h.FirstCheckinTime, h.LastCheckinTime, h.CheckStatus, h.SessionID
        FROM CheckinHistory h
        JOIN Employees e ON h.EmployeeID = e.EmployeeID
        ORDER BY h.FirstCheckinTime DESC
    """)
    history = cursor.fetchall()
    conn.close()
    
    result = []
    for record in history:
        result.append({
            'checkin_id': record['CheckinID'],
            'employee_name': record['Name'],
            'first_checkin_time': record['FirstCheckinTime'],
            'last_checkin_time': record['LastCheckinTime'],
            'status': record['CheckStatus'],
            'session_id': record['SessionID']
        })
    
    return jsonify(result)

@app.route('/reset_employee/<name>', methods=['POST'])
def reset_employee_first_time(name):
    """Debug endpoint to reset an employee's FirstTime flag"""
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("UPDATE Employees SET FirstTime = 1 WHERE Name = ?", (name,))
    
    if cursor.rowcount > 0:
        conn.commit()
        conn.close()
        return jsonify({'message': f'Reset FirstTime flag for {name}'}), 200
    else:
        conn.close()
        return jsonify({'message': f'Employee {name} not found'}), 404

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)