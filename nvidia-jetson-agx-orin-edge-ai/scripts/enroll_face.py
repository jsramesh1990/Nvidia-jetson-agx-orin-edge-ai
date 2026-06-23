#!/usr/bin/env python3
import cv2
import numpy as np
import sys
import json
import requests
import argparse

def enroll_face(user_id, name):
    """Enroll a new user by capturing face images"""
    cap = cv2.VideoCapture(0)
    print(f"📸 Enrolling user: {name} ({user_id})")
    print("Press SPACE to capture, ESC to quit")
    
    faces = []
    while len(faces) < 10:
        ret, frame = cap.read()
        if not ret:
            continue
            
        # Detect face
        face_cascade = cv2.CascadeClassifier(
            cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
        )
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        detected_faces = face_cascade.detectMultiScale(gray, 1.3, 5)
        
        for (x, y, w, h) in detected_faces:
            face_roi = frame[y:y+h, x:x+w]
            cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)
            
            cv2.imshow('Face Enrollment', frame)
            key = cv2.waitKey(1) & 0xFF
            
            if key == ord(' '):
                faces.append(face_roi)
                print(f"Captured {len(faces)}/10 faces")
                break
            elif key == 27:  # ESC
                cap.release()
                cv2.destroyAllWindows()
                return False
    
    cap.release()
    cv2.destroyAllWindows()
    
    # Save faces to database via API
    data = {
        "user_id": user_id,
        "name": name,
        "faces": [face.tolist() for face in faces]
    }
    
    response = requests.post(
        "http://localhost:8000/api/enroll",
        json=data
    )
    
    if response.status_code == 200:
        print(f"✅ User {name} enrolled successfully!")
        return True
    else:
        print(f"❌ Failed to enroll user: {response.text}")
        return False

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Enroll face for attendance")
    parser.add_argument("--user", required=True, help="User ID")
    parser.add_argument("--name", required=True, help="User name")
    args = parser.parse_args()
    
    enroll_face(args.user, args.name)
