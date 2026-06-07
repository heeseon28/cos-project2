import socket
import threading
import argparse
import logging

OPCODE_REPLY = 2

def protocol_execution(sock):
    # 1. Alice -> Bob: length of the name (4 bytes) || name (length bytes)
    # Get the length information (4 bytes)

    # 먼저 이름 길이를 받아 이후에 수신할 문자열의 크기를 결정 
    buf = sock.recv(4)
    # 수신한 4바이트를 정수 형태의 길이 정보로 변환
    length = int.from_bytes(buf, "big")
    logging.info("[*] Length received: {}".format(length))

    # Get the name (Alice)
    # 앞에서 받은 길이만큼 실제 이름 문자열을 수신
    buf = sock.recv(length)
    logging.info("[*] Name received: {}".format(buf.decode()))

    # 2. Bob -> Alice: length of the name (4 bytes) || name (length bytes)
    # Send the length information (4 bytes)
    # 서버가 클라이언트에게 전달할 이름을 설정
    name = "Bob"
    # 이름 문자열의 길이를 계산
    length = len(name)
    logging.info("[*] Length to be sent: {}".format(length))
    # 수신 측이 문자열 길이를 알 수 있도록 4바이트 길이 정보를 먼저 전송
    sock.send(int.to_bytes(length, 4, "big"))

    # Send the name (Bob)
    # 길이 정보 전송 후 실제 이름 문자열을 전송
    logging.info("[*] Name to be sent: {}".format(name))
    sock.send(name.encode())

    # Implement following the instructions below
    # 3. Alice -> Bob: opcode (4 bytes) || arg1 (4 bytes) || arg2 (4 bytes)
    # The opcode should be 1

    # opcode와 두 개의 인자를 포함하는 총 12바이트 패킷을 수신
    buf = sock.recv(12)

    # The values are encoded in the big-endian style and should be translated into the little-endian style (because my machine follows the little-endian style)
    # 수신한 패킷에서 opcode를 추출
    opcode = int.from_bytes(buf[0:4], "little")
    # 수신한 패킷에서 첫 번째 피연산자를 추출
    arg1 = int.from_bytes(buf[4:8], "little")
    # 수신한 패킷에서 두 번째 피연산자를 추출
    arg2 = int.from_bytes(buf[8:12], "little")

    logging.info("[*] Opcode: {}".format(opcode))
    logging.info("[*] Arg1: {}".format(arg1))
    logging.info("[*] Arg2: {}".format(arg2))

    # 4. Bob -> Alice: opcode (4 bytes) || result (4 bytes)
    # The opcode should be 2
    # 클라이언트가 요청한 덧셈 연산을 수행
    result = arg1 + arg2
    logging.info("[*] Result: {}".format(result))
    # 응답 패킷의 opcode를 설정
    opcode = 2
    # 연산 결과 응답임을 나타내는 opcode를 전송
    sock.send(int.to_bytes(OPCODE_REPLY, 4, "big"))
    # 계산된 결과 값을 4바이트 형태로 전송
    sock.send(int.to_bytes(result, 4, "big"))

    # 프로토콜 수행이 완료되었으므로 소켓 연결을 종료
    sock.close()

def run(addr, port):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((addr, port))

    server.listen(2)
    logging.info("[*] Server is Listening on {}:{}".format(addr, port))

    while True:
        client, info = server.accept()

        logging.info("[*] Server accept the connection from {}:{}".format(info[0], info[1]))

        client_handle = threading.Thread(target=protocol_execution, args=(client,))
        client_handle.start()

def command_line_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-a", "--addr", metavar="<server's IP address>", help="Server's IP address", type=str, default="0.0.0.0")
    parser.add_argument("-p", "--port", metavar="<server's open port>", help="Server's port", type=int, required=True)
    parser.add_argument("-l", "--log", metavar="<log level (DEBUG/INFO/WARNING/ERROR/CRITICAL)>", help="Log level (DEBUG/INFO/WARNING/ERROR/CRITICAL)", type=str, default="INFO")
    args = parser.parse_args()
    return args

def main():
    args = command_line_args()
    log_level = args.log
    logging.basicConfig(level=log_level)

    run(args.addr, args.port)

if __name__ == "__main__":
    main()
