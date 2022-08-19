# Test TCP Server - ESP 32 Client
import socket
from datetime import datetime

def recv_payload(sock,data_len):
    img_data = []
    recv_size = 0

    while (recv_size < data_len):  
        try:
            img_buff = sock.recv(data_len)
            print('Recieved data = ',len(img_buff))
            img_data.append(img_buff)
            recv_size += len(img_buff)
        except:
            print('recv error!!!!')
            pass

    return img_data


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

        for count in range(0,15):
            img_size = connection.recv(4)
            img_size = int.from_bytes(img_size,'little')
            print("Size of the image buffer = ",img_size)
            esp_data = recv_payload(connection,img_size)

            cTime = datetime.now()
            img_name = 'images/{} {}.jpg'.format(cTime,count)
            with open(img_name,'wb') as esp_img:
                for buff in esp_data:
                    esp_img.write(buff)

    finally:
        print('Saved Data')
        connection.close()
        break

print('Exiting...')
exit()