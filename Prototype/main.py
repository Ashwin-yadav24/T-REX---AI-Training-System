# ==============================================================================
# Project: T-REX Laser Reaction Time Tester (Laptop Side)
# This script uses Firebase to synchronize with an ESP32-based laser unit.
# It tracks the athlete's hands and feet in real-time and measures the reaction time
# upon collision with the laser dot.
# ==============================================================================

# ==============================================================================
# Package Requirements:
# 1. mediapipe: pip install mediapipe
# 2. opencv-python: pip install opencv-python
# 3. firebase-admin: pip install firebase-admin
# ==============================================================================

import cv2
import time
import mediapipe as mp
import firebase_admin
from firebase_admin import credentials, db
import threading
import numpy as np

# ==============================================================================
# FIREBASE AND HARDWARE SETUP
# ==============================================================================
# Initialize Firebase Admin SDK
# Make sure you've placed your service account JSON file in the specified path.
try:
    cred = credentials.Certificate(r'D:\STUDY\sih demo\foot tracking\t-rex-team404-firebase-adminsdk-fbsvc-0166f309fa.json')
    firebase_admin.initialize_app(cred, {
        'databaseURL': 'https://t-rex-team404-default-rtdb.firebaseio.com/'
    })
    database_ref = db.reference('coordinate_sync')
    print("✅ Firebase connected successfully.")
except Exception as e:
    print(f"❌ Firebase connection error: {e}")
    exit()

# Initialize MediaPipe Hand and Pose models
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(min_detection_confidence=0.5, min_tracking_confidence=0.5)
mp_pose = mp.solutions.pose
pose = mp_pose.Pose(min_detection_confidence=0.5, min_tracking_confidence=0.5)

# Initialize webcam.
cap = cv2.VideoCapture(0)
if not cap.isOpened():
    print("Error: Could not open webcam.")
    exit()

frame_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
frame_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
print(f"Webcam resolution: {frame_width}x{frame_height}")

# ==============================================================================
# SYSTEM VARIABLES
# ==============================================================================
reaction_times = []
laser_pos_pixel = None
current_target = None
game_active = False
start_time = 0
hit_tolerance = 40  # Pixel tolerance for collision detection (e.g., 40 pixels)

# ==============================================================================
# IMAGE PROCESSING FUNCTIONS
# ==============================================================================
def find_red_dot(frame):
    """
    Finds the red laser dot in the frame using color filtering.
    Returns (x, y) coordinates of the dot's center or None if not found.
    """
    # Convert the frame from BGR to HSV
    hsv_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    
    # Define a range for red color in HSV. The red color wraps around the color space.
    lower_red1 = np.array([0, 100, 100])
    upper_red1 = np.array([10, 255, 255])
    lower_red2 = np.array([160, 100, 100])
    upper_red2 = np.array([180, 255, 255])
    
    # Create two masks for red color
    mask1 = cv2.inRange(hsv_frame, lower_red1, upper_red1)
    mask2 = cv2.inRange(hsv_frame, lower_red2, upper_red2)
    
    # Combine the masks
    red_mask = mask1 + mask2
    
    # Find contours in the red mask
    contours, _ = cv2.findContours(red_mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    
    if contours:
        # Find the largest contour, which is likely the laser dot
        largest_contour = max(contours, key=cv2.contourArea)
        area = cv2.contourArea(largest_contour)
        
        # Only proceed if the area is above a certain threshold to filter out noise
        if area > 10: # Adjust this threshold based on your laser dot's size
            M = cv2.moments(largest_contour)
            if M['m00'] > 0:
                center_x = int(M['m10'] / M['m00'])
                center_y = int(M['m01'] / M['m00'])
                return (center_x, center_y)
    return None

# ==============================================================================
# FIREBASE LISTENER THREAD
# ==============================================================================
def firebase_listener():
    """Listens for new targets from the ESP32 via Firebase."""
    def on_target_update(event):
        global current_target, game_active, laser_pos_pixel, start_time
        
        if event.data and event.data.get('active'):
            new_target_id = event.data.get('target_id')
            
            if not current_target or new_target_id != current_target.get('target_id'):
                current_target = event.data
                
                # The Python script will now rely on its own vision to find the laser dot
                laser_pos_pixel = None # Reset the laser position until it's found by vision

                start_time = time.time()
                game_active = True
                print(f"🎯 New target received from ESP32: {new_target_id}")
        else:
            current_target = None
            game_active = False
            laser_pos_pixel = None
            print("Target deactivated by hardware.")
            
    database_ref.child('current_target').listen(on_target_update)

# Start the listener thread
firebase_thread = threading.Thread(target=firebase_listener, daemon=True)
firebase_thread.start()

# ==============================================================================
# MAIN GAME LOOP
# ==============================================================================
def main():
    """Main function to run the reaction timer."""
    global game_active, current_target, laser_pos_pixel, start_time

    print("System is ready. Waiting for ESP32 to start a trial.")
    
    while True:
        success, image = cap.read()
        if not success:
            print("Ignoring empty camera frame.")
            continue

        image = cv2.flip(image, 1)
        image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        
        hands_results = hands.process(image_rgb)
        pose_results = pose.process(image_rgb)
        
        # Use image processing to find the laser dot's actual pixel position
        laser_pos_pixel = find_red_dot(image)
        
        # Draw game visuals
        cv2.rectangle(image, (0, 0), (frame_width, frame_height), (255, 0, 0), 5)

        if game_active and laser_pos_pixel:
            cv2.circle(image, laser_pos_pixel, 20, (0, 0, 255), cv2.FILLED)
            cv2.circle(image, laser_pos_pixel, hit_tolerance, (0, 0, 255), 2)

            detected_body_parts = []
            if hands_results.multi_hand_landmarks:
                for hand_landmarks in hands_results.multi_hand_landmarks:
                    index_finger_tip = hand_landmarks.landmark[mp_hands.HandLandmark.INDEX_FINGER_TIP]
                    detected_body_parts.append(
                        (int(index_finger_tip.x * frame_width), int(index_finger_tip.y * frame_height))
                    )
            
            if pose_results.pose_landmarks:
                left_foot = pose_results.pose_landmarks.landmark[mp_pose.PoseLandmark.LEFT_FOOT_INDEX]
                right_foot = pose_results.pose_landmarks.landmark[mp_pose.PoseLandmark.RIGHT_FOOT_INDEX]
                detected_body_parts.append(
                    (int(left_foot.x * frame_width), int(left_foot.y * frame_height))
                )
                detected_body_parts.append(
                    (int(right_foot.x * frame_width), int(right_foot.y * frame_height))
                )

            for part_pos in detected_body_parts:
                cv2.circle(image, part_pos, 15, (0, 255, 0), cv2.FILLED)
                distance_sq = (part_pos[0] - laser_pos_pixel[0])**2 + (part_pos[1] - laser_pos_pixel[1])**2
                
                if distance_sq < hit_tolerance**2: 
                    reaction_time = time.time() - start_time
                    reaction_times.append(reaction_time)
                    
                    hit_data = {
                        "target_id": current_target.get('target_id'),
                        "hit_detected": True,
                        "timestamp": int(time.time() * 1000)
                    }
                    database_ref.child('hit_detection').set(hit_data)
                    
                    print(f"✅ HIT! Reaction time: {reaction_time:.3f} seconds.")
                    
                    game_active = False
                    current_target = None
                    
                    cv2.putText(image, "Success!", (200, 250), cv2.FONT_HERSHEY_SIMPLEX, 2, (0, 255, 0), 3)
                    cv2.imshow("Image", image)
                    cv2.waitKey(1000)
                    break

        if len(reaction_times) > 0:
            avg_time = sum(reaction_times) / len(reaction_times)
            cv2.putText(image, f"Avg Time: {avg_time:.3f} s", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
            cv2.putText(image, f"Trials: {len(reaction_times)}", (10, 70), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)

        cv2.imshow("Image", image)

        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break

    if firebase_admin._apps:
        database_ref.child('current_target').update({'active': False})
    
    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
