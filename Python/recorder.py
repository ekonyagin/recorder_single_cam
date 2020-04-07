
import cv2
from ximea import xiapi
import os
import socket
import datetime
import sys
import time
import json

HOST = "0.0.0.0"           
PORT = 50009

cam = xiapi.Camera(dev_id=0)

def PrintOut(conn, message):
    msg = message + "\n"
    conn.send(msg.encode())
    print(message)

def MsgDecode(data):
    task = data.decode().split()
    name = task[0]
    rec_length = int(task[1])
    if len(task) == 3:
        fps = int(task[2])
    else:
        fps = 30
    
    return name, rec_length, fps

def WriteFiles(name, counter, img, roi):
    img = img.get_image_data_numpy()
    img = img[roi[2]: roi[3], roi[0]: roi[1]]
    cv2.imwrite(name+'/IMG'+str(100000 + counter)+'.png', 
                       img)
    
    return 0

def MakeRecording(name, rec_length, fps=30):
    with open("camera_params.json") as f:
        a = json.load(f)
        roi= [a['offsetX1'],
              a['offsetX2'],
              a['offsetY1'],
              a['offsetY2']]
        exp = a['exposure']
        gain = a['gain']
    cam.open_device()
    
    PrintOut(connection, 'RECORDING BEING LAUNCHED')
    #set interface data rate
    
    cam.set_imgdataformat('XI_RAW8')
    cam.set_exposure(exp)
    print(gain)
    cam.set_param('gain', gain)
    img = xiapi.Image()
    cam.start_acquisition()                 
    
    print(name, " ", rec_length)
                        
    d = datetime.datetime.now()
                     
    time_prev = time.time()
    
    try:
        os.mkdir(name)
    except:
        print("Unable to create folders. Recording aborted")
        return -1
    N = 0
    while (datetime.datetime.now() - d).total_seconds() < rec_length:
        if (time.time() - time_prev > 1./fps):
            time_prev = time.time()
            
            cam.get_image(img)
            
            if WriteFiles(name, N, img, roi) == 0:
                N += 1
            else:
                PrintOut("Failed")
                return -1
    PrintOut(connection, "Recording finished")
    cam.stop_acquisition()
    
    
    cam.close_device()
    return 0
    

if __name__ == "__main__":
    
    if len(sys.argv) > 1:
        PORT = int(sys.argv[1])

    print("Welcome to remote NUC recorder at port", PORT)
    
    server_info = (HOST, PORT)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(server_info)
    sock.listen(5)

    
    try:
        while True:
            connection, client_address = sock.accept()
            connection.send(b"Ready\n")
            
            try:
                data = connection.recv(1024)
                print('message received:' , data.decode())
                    
                if (len(data.decode()) != 0):
                    
                    name, rec_length, fps = MsgDecode(data)

                    if MakeRecording(name, rec_length, fps) == 0:
                        PrintOut(connection, "Ready")
                    else:
                        PrintOut(connection, "Failed")
                        
                    
            except Exception as e:
                PrintOut(connection, "INVALID TASK FORMAT. TRY AGAIN")
                print(e)
                
            finally:
                connection.close()
                
    finally:
        print('closing socket')
        sock.close()
