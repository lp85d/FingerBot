import cv2
import mediapipe as mp
import numpy as np
import tkinter as tk
from tkinter import filedialog, scrolledtext
from tkinter import ttk
from PIL import Image, ImageTk
import time

mp_drawing = mp.solutions.drawing_utils
mp_hands = mp.solutions.hands

def get_finger_position(image, hand_landmarks, finger_name):
    height, width, _ = image.shape
    finger = hand_landmarks.landmark[finger_name]
    finger_x = int(finger.x * width)
    finger_y = int(finger.y * height)
    return finger_x, finger_y

def is_finger_open(hand_landmarks, finger_tip, finger_dip):
    return hand_landmarks.landmark[finger_tip].y < hand_landmarks.landmark[finger_dip].y

initial_width, initial_height = 1280, 720
canvas = np.ones((initial_height, initial_width, 3), dtype=np.uint8) * 255
hands = mp_hands.Hands(max_num_hands=1, min_detection_confidence=0.5, min_tracking_confidence=0.5)
prev_x, prev_y = None, None
cap = cv2.VideoCapture(0)

blue_circles = []

root = tk.Tk()
root.title("Drawing Canvas")

log_enabled = 0  # Флаг для включения/выключения вывода лога

def save_image():
    file_path = filedialog.asksaveasfilename(defaultextension=".png")
    if file_path:
        cv2.imwrite(file_path, canvas)
        if log_enabled:
            log.insert(tk.END, "Изображение сохранено: " + file_path + "\n")
        print("Изображение сохранено:", file_path)

def clear_canvas():
    global canvas
    canvas = np.ones((initial_height, initial_width, 3), dtype=np.uint8) * 255
    if log_enabled:
        log.insert(tk.END, "Холст очищен\n")
    print("Холст очищен")

def copy_log():
    if log_enabled:
        log_content = log.get("1.0", tk.END)
        if log_content.strip():
            root.clipboard_clear()
            root.clipboard_append(log_content)
            log.insert(tk.END, "Лог скопирован в буфер обмена\n")
            print("Лог скопирован в буфер обмена")

def clear_log():
    if log_enabled:
        log.delete("1.0", tk.END)
        log.insert(tk.END, "Лог очищен\n")
    print("Лог очищен")

def update_canvas():
    global prev_x, prev_y, canvas, blue_circles

    ret, frame = cap.read()
    if not ret:
        root.after(10, update_canvas)
        return

    image = cv2.cvtColor(cv2.flip(frame, 1), cv2.COLOR_BGR2RGB)
    image.flags.writeable = False
    results = hands.process(image)

    if results.multi_hand_landmarks:
        for hand_landmarks in results.multi_hand_landmarks:
            index_finger_x, index_finger_y = get_finger_position(image, hand_landmarks, mp_hands.HandLandmark.INDEX_FINGER_TIP)
            pinky_finger_x, pinky_finger_y = get_finger_position(image, hand_landmarks, mp_hands.HandLandmark.PINKY_TIP)

            window_width = frame.shape[1]
            window_height = frame.shape[0]
            canvas_width = canvas.shape[1]
            canvas_height = canvas.shape[0]

            index_finger_x = int(index_finger_x * canvas_width / window_width)
            index_finger_y = int(index_finger_y * canvas_height / window_height)
            pinky_finger_x = int(pinky_finger_x * canvas_width / window_width)
            pinky_finger_y = int(pinky_finger_y * canvas_height / window_height)

            index_open = is_finger_open(hand_landmarks, mp_hands.HandLandmark.INDEX_FINGER_TIP, mp_hands.HandLandmark.INDEX_FINGER_DIP)
            pinky_open = is_finger_open(hand_landmarks, mp_hands.HandLandmark.PINKY_TIP, mp_hands.HandLandmark.PINKY_DIP)
            
            if index_open:
                if prev_x is not None and prev_y is not None:
                    cv2.line(canvas, (prev_x, prev_y), (index_finger_x, index_finger_y), (0, 0, 0), 5)
                    if log_enabled:
                        log.insert(tk.END, f"Добавлен след ластиком: ({prev_x}, {prev_y}) -> ({index_finger_x}, {index_finger_y})\n")
                prev_x, prev_y = index_finger_x, index_finger_y
            else:
                prev_x, prev_y = None, None

            if pinky_open:
                blue_circles.append((pinky_finger_x, pinky_finger_y, time.time()))
                cv2.circle(canvas, (pinky_finger_x, pinky_finger_y), 30, (255, 255, 255), -1)  # Белый круг
                cv2.circle(canvas, (pinky_finger_x, pinky_finger_y), 40, (255, 255, 255), -1)  # Удаление с большим радиусом
                cv2.circle(canvas, (pinky_finger_x, pinky_finger_y), 30, (0, 0, 255), 1, lineType=cv2.LINE_AA)  # Синяя обводка
                if log_enabled:
                    log.insert(tk.END, f"Добавлен синий круг с центром ({pinky_finger_x}, {pinky_finger_y})\n")

            hand_landmarks_image = np.zeros((200, 200, 3), dtype=np.uint8)
            mp_drawing.draw_landmarks(hand_landmarks_image, hand_landmarks, mp_hands.HAND_CONNECTIONS)
            canvas[0:200, canvas.shape[1] - 200:canvas.shape[1]] = hand_landmarks_image
    else:
        prev_x, prev_y = None, None

    current_time = time.time()
    for (x, y, t) in blue_circles:
        if current_time - t > 0.5:  # Удаление через 1 секунду
            cv2.circle(canvas, (x, y), 40, (255, 255, 255), -1)  # Удаление с большим радиусом
            blue_circles.remove((x, y, t))
            if log_enabled:
                log.insert(tk.END, f"Удален синий круг с центром ({x}, {y})\n")

    img = Image.fromarray(canvas)
    imgtk = ImageTk.PhotoImage(image=img)
    canvas_tk.create_image(0, 0, anchor=tk.NW, image=imgtk)
    canvas_tk.imgtk = imgtk

    root.after(10, update_canvas)

paned_window = ttk.PanedWindow(root, orient=tk.HORIZONTAL)
paned_window.pack(fill=tk.BOTH, expand=1)

left_frame = ttk.Frame(paned_window, width=200, height=initial_height)
paned_window.add(left_frame, weight=1)

right_frame = ttk.Frame(paned_window, width=initial_width, height=initial_height)
paned_window.add(right_frame, weight=4)

log = scrolledtext.ScrolledText(left_frame, width=30, height=10)
log.grid(row=0, column=0, columnspan=2, padx=5, pady=5, sticky="nsew")

button_style = {"padx": 5, "pady": 3, "bg": "#4CAF50", "fg": "white", "font": ("Helvetica", 10)}

save_button = tk.Button(left_frame, text="Сохранить", command=save_image, **button_style)
save_button.grid(row=1, column=0, padx=5, pady=3, sticky="ew")

clear_button = tk.Button(left_frame, text="Очистить холст", command=clear_canvas, **button_style)
clear_button.grid(row=1, column=1, padx=5, pady=3, sticky="ew")

copy_button = tk.Button(left_frame, text="Копировать лог", command=copy_log, **button_style)
copy_button.grid(row=2, column=0, padx=5, pady=3, sticky="ew")

clear_log_button = tk.Button(left_frame, text="Очистить лог", command=clear_log, **button_style)
clear_log_button.grid(row=2, column=1, padx=5, pady=3, sticky="ew")

frame_canvas = ttk.Frame(right_frame)
frame_canvas.grid(row=0, column=0, padx=5, pady=5, sticky="nsew")

canvas_tk = tk.Canvas(frame_canvas, width=initial_width, height=initial_height)
canvas_tk.pack(side="left", fill="both", expand=True)

scrollbar = ttk.Scrollbar(frame_canvas, orient="vertical", command=canvas_tk.yview)
scrollbar.pack(side="right", fill="y")

canvas_tk.configure(yscrollcommand=scrollbar.set)

left_frame.grid_rowconfigure(0, weight=1)
left_frame.grid_rowconfigure(1, weight=0)
left_frame.grid_rowconfigure(2, weight=0)
left_frame.grid_columnconfigure(0, weight=1)
left_frame.grid_columnconfigure(1, weight=1)

root.after(10, update_canvas)
root.mainloop()

cap.release()
cv2.destroyAllWindows()
