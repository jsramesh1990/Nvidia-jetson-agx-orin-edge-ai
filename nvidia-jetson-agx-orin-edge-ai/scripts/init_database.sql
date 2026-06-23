-- Employees table
CREATE TABLE employees (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    email TEXT,
    department TEXT,
    position TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Face embeddings table
CREATE TABLE face_embeddings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id TEXT NOT NULL,
    embedding BLOB NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES employees(user_id)
);

-- Voice prints table
CREATE TABLE voice_prints (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id TEXT NOT NULL,
    voice_print BLOB NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES employees(user_id)
);

-- Attendance records
CREATE TABLE attendance (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id TEXT NOT NULL,
    check_in DATETIME DEFAULT CURRENT_TIMESTAMP,
    check_out DATETIME,
    status TEXT DEFAULT 'present',
    method TEXT DEFAULT 'face_recognition',
    confidence REAL,
    FOREIGN KEY (user_id) REFERENCES employees(user_id)
);

-- Visitors table
CREATE TABLE visitors (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    phone TEXT,
    email TEXT,
    company TEXT,
    purpose TEXT,
    check_in DATETIME DEFAULT CURRENT_TIMESTAMP,
    check_out DATETIME,
    host_employee TEXT,
    status TEXT DEFAULT 'pending',
    FOREIGN KEY (host_employee) REFERENCES employees(user_id)
);

-- Notifications table
CREATE TABLE notifications (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,
    message TEXT NOT NULL,
    target_user TEXT,
    is_read BOOLEAN DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (target_user) REFERENCES employees(user_id)
);

-- Analytics table
CREATE TABLE analytics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    metric TEXT NOT NULL,
    value REAL NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Indexes
CREATE INDEX idx_attendance_user ON attendance(user_id);
CREATE INDEX idx_attendance_date ON attendance(check_in);
CREATE INDEX idx_visitors_checkin ON visitors(check_in);
CREATE INDEX idx_notifications_user ON notifications(target_user);
