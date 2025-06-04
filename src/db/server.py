from flask import Flask, request, jsonify, render_template
import sqlite3
from datetime import datetime, date, timezone
import json
import ecdsa
import ecdh_aes

app = Flask(__name__)
DB_PATH = 'attendance.db'

# Load the ca private key for signing cert
with open("ca.pem", "rb") as f:
    ca_private_key, ca_public_key = ecdsa.load_private_key(f.read())
ca_public_bytes = ecdsa.get_public_bytes(ca_public_key)

def get_db_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


@app.route('/checkin', methods=['POST'])
def checkin():
    try:
        data = request.get_json()
        cert_id = data.get("cert_id")

        session_id = data.get('session_id', 1)  # Default to session 1
        
        conn = get_db_connection()
        cursor = conn.cursor()
        # Look up employee by token
        cursor.execute("SELECT Employees.EmployeeID, Name FROM cert INNER JOIN Employees ON cert.EmployeeID = Employees.EmployeeID where cert.id = ?", (cert_id,))
        employee = cursor.fetchone()

        if not employee:
            conn.close()
            return jsonify({
                'success': False,
                'message': 'Invalid token'
            }), 401

        employee_id = employee['EmployeeID']
        employee_name = employee['Name']
        
        print(f"Found employee: {employee_name} (ID: {employee_id})")

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
            'role': emp['Role'],
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

@app.route('/history/today', methods=['GET'])
def get_today_history():
    today = date.today().strftime('%Y-%m-%d')
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("""
        SELECT
            e.Name,
            strftime('%Y-%m-%d', ch.FirstCheckinTime) AS Date,
            s.TimeSlot,
            ch.FirstCheckinTime,
            ch.LastCheckinTime,
            ch.CheckStatus
        FROM CheckinHistory ch
        JOIN Employees e ON ch.EmployeeID = e.EmployeeID
        JOIN Sessions s ON ch.SessionID = s.SessionID
        WHERE strftime('%Y-%m-%d', ch.FirstCheckinTime) = ? OR strftime('%Y-%m-%d', ch.LastCheckinTime) = ?
        ORDER BY ch.FirstCheckinTime DESC
    """, (today, today))
    results = [dict(row) for row in cursor.fetchall()]
    conn.close()
    return jsonify(results)


@app.route('/history/monthly/<int:employee_id>', methods=['GET'])
def get_month_history(employee_id):
    month_str = date.today().strftime('%Y-%m')
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("""
        SELECT
            e.Name,
            strftime('%Y-%m-%d', ch.FirstCheckinTime) AS Date,
            s.TimeSlot,
            ch.FirstCheckinTime,
            ch.LastCheckinTime,
            ch.CheckStatus
        FROM CheckinHistory ch
        JOIN Employees e ON ch.EmployeeID = e.EmployeeID
        JOIN Sessions s ON ch.SessionID = s.SessionID
        WHERE ch.EmployeeID = ?
          AND (substr(ch.FirstCheckinTime, 1, 7) = ? OR substr(ch.LastCheckinTime, 1, 7) = ?)
        ORDER BY ch.FirstCheckinTime DESC
    """, (employee_id, month_str, month_str))
    results = [dict(row) for row in cursor.fetchall()]
    conn.close()
    return jsonify(results)

@app.route('/sign_cert', methods=['POST'])
def sign_cert():
    data = request.get_json()
    token = data.get("token")
    id = data.get("id")
    pub_key = data.get("pub_key")

    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("""
        SELECT id, pub_key, valid_until, token, issued 
        FROM cert
        where id = ?
    """, (id,))
    user = cursor.fetchone()

    if user is None:
        print(f"not found user {id}")
        return jsonify({
            "success": False,
            }), 303

    target_token = user["token"]
    issued = user["issued"]

    if target_token != token or issued == 1:
        print(f"cannot sign cert invalid info")
        return jsonify({
            "success": False,
            }), 303

    
    valid_until = datetime.now(timezone.utc)
    cursor.execute("update cert set issued = 1, pub_key = ?, valid_until = ?", (pub_key, valid_until))
    conn.commit()
    conn.close()

    valid_until_string = valid_until.strftime("%Y-%m-%d %H:%M:%S")
    cert_bytes = ecdsa.cert_bytes(id, bytes.fromhex(pub_key), valid_until_string)
    print(cert_bytes)
    
    signature = ecdsa.sign(ca_private_key, cert_bytes)
    

    return jsonify({
        'success': True,
        'valid_until': valid_until_string,
        'signature': signature.hex().upper(),
        'ca_pub': ca_public_bytes.hex().upper()
    }), 200

@app.route('/api/attendance_pie')
def attendance_pie():
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("""
        SELECT CheckStatus, COUNT(*) as count 
        FROM CheckinHistory 
        GROUP BY CheckStatus
    """)
    data = cursor.fetchall()
    conn.close()
    statuses = [row['CheckStatus'].title() for row in data]
    counts = [row['count'] for row in data]
    return jsonify({'labels': statuses, 'counts': counts})

@app.route('/api/monthly_trend')
def monthly_trend():
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT MIN(Date) as min_date FROM Sessions")
    min_date_row = cursor.fetchone()
    if not min_date_row['min_date']:
        return jsonify({'months': [], 'attendance_rates': [], 'late_rates': [], 'absent_rates': []})
    base_month = datetime.strptime(min_date_row['min_date'], "%Y-%m-%d")
    months = []
    attendance_rates = []
    late_rates = []
    absent_rates = []
    for i in range(12):
        month_date = (base_month.replace(day=1) + 
                      (datetime(base_month.year + (base_month.month + i - 1) // 12, (base_month.month + i - 1) % 12 + 1, 1) - base_month.replace(day=1)))
        month_str = month_date.strftime("%Y-%m")
        months.append(month_str)
        cursor.execute("""
            SELECT CheckStatus, COUNT(*) as count
            FROM CheckinHistory
            WHERE substr(FirstCheckinTime, 1, 7) = ?
            GROUP BY CheckStatus
        """, (month_str,))
        stats = {row['CheckStatus']: row['count'] for row in cursor.fetchall()}
        total = sum(stats.values()) or 1
        attendance_rates.append(round(100 * stats.get('on time', 0) / total, 1))
        late_rates.append(round(100 * stats.get('late', 0) / total, 1))
        absent_rates.append(round(100 * stats.get('absent', 0) / total, 1))
    conn.close()
    return jsonify({
        'months': months,
        'attendance_rates': attendance_rates,
        'late_rates': late_rates,
        'absent_rates': absent_rates
    })


@app.route('/')
def home():
    return render_template('web.html')


# add test check-in data
def add_test_data():
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM CheckinHistory")
    if cursor.fetchone()[0] == 0:
        # Get session IDs
        cursor.execute("SELECT SessionID FROM Sessions WHERE TimeSlot='Morning'")
        session_id = cursor.fetchone()[0]
        now = datetime.now().replace(hour=8, minute=30)
        cursor.execute("INSERT INTO CheckinHistory (EmployeeID, SessionID, FirstCheckinTime, LastCheckinTime, CheckStatus) VALUES (1, ?, ?, ?, 'on time')", (session_id, now, now))
        cursor.execute("INSERT INTO CheckinHistory (EmployeeID, SessionID, FirstCheckinTime, LastCheckinTime, CheckStatus) VALUES (2, ?, ?, ?, 'late')", (session_id, now, now))
        conn.commit()
    conn.close()


if __name__ == '__main__':
    add_test_data()
    app.run(host='0.0.0.0', port=5000, debug=True)
