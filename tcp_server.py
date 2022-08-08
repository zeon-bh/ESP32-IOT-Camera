# Test TCP Server - ESP 32 Client
from ctypes.wintypes import RGB
import socket
from io import BytesIO
from PIL import Image, ImageFile

ImageFile.LOAD_TRUNCATED_IMAGES = True

rsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)


server_address = ('192.168.0.109',3333)
print("Starting Listen socket server....")
rsock.bind(server_address)
rsock.listen(1)

while True:
    print("Waiting for connection....")
    connection,client_address = rsock.accept()

    try:
        print("Connection from {}",client_address)

        img_size = connection.recv(4)
        img_size = int.from_bytes(img_size,'big')
        print("Size of the image buffer = ",img_size)
        img_data = connection.recv(img_size)

        with open('esp_image.jpg','wb') as esp_img:
            esp_img.write(img_data)

    finally:
        print('Saved File')
        exit()