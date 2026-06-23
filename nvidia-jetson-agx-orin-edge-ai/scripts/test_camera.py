#!/usr/bin/env python3
import cv2
import sys
import time

def test_camera(device="/dev/video0"):
    """Test camera functionality"""
    print(f"📷 Testing camera: {device}")
    
    cap = cv2.VideoCapture(device)
    if not cap.isOpened():
        print("❌ Failed to open camera")
        return False
    
    print("✅ Camera opened successfully")
    
    # Get camera properties
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    
    print(f"   Resolution: {width}x{height}")
    print(f"   FPS: {fps}")
    
    # Capture test frames
    print("📸 Capturing test frames...")
    frames = []
    for i in range(30):
        ret, frame = cap.read()
        if ret:
            frames.append(frame)
            print(f"   Frame {i+1}: {frame.shape}")
        else:
            print(f"   Frame {i+1}: FAILED")
    
    cap.release()
    
    if len(frames) > 0:
        print(f"✅ Captured {len(frames)} frames successfully")
        return True
    else:
        print("❌ Failed to capture frames")
        return False

if __name__ == "__main__":
    device = sys.argv[1] if len(sys.argv) > 1 else "/dev/video0"
    test_camera(device)
