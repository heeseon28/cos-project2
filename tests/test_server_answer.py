import socket
import threading
import argparse
import logging

OPCODE_REPLY = 2


def recv_exact(sock, size):
    """Receive exactly size bytes from a TCP stream.

    TCP is a stream protocol, so sock.recv(size) may return fewer than size bytes.
    The C++ client uses repeated read() loops for the same reason.
    """
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("connection closed while receiving data")
        data += chunk
    return data


def protocol_execution(sock):
    # 1. Alice -> Bob: length of the name (4 bytes) || name (length bytes)
    # 이름 본문을 받기 전에 4-byte Big Endian 길이 값을 먼저 받는다.
    buf = recv_exact(sock, 4)
    length = int.from_bytes(buf, "big")
    logging.info("[*] Length received: {}".format(length))

    # 길이를 알게 되었으므로 정확히 length bytes만큼 이름 문자열을 받는다.
    buf = recv_exact(sock, length)
    logging.info("[*] Name received: {}".format(buf.decode()))

    # 2. Bob -> Alice: length of the name (4 bytes) || name (length bytes)
    # 서버 이름도 같은 규칙으로 길이 먼저, 문자열 다음 순서로 보낸다.
    name = "Bob"
    length = len(name)
    logging.info("[*] Length to be sent: {}".format(length))
    sock.sendall(int.to_bytes(length, 4, "big"))

    logging.info("[*] Name to be sent: {}".format(name))
    sock.sendall(name.encode())

    # 3. Alice -> Bob: opcode (4 bytes) || arg1 (4 bytes) || arg2 (4 bytes)
    # C++ client uses VAR_TO_MEM_4BYTES_BIG_ENDIAN for all three fields.
    # Therefore Python must decode all fields with byteorder="big".
    buf = recv_exact(sock, 12)

    opcode = int.from_bytes(buf[0:4], "big")
    arg1 = int.from_bytes(buf[4:8], "big")
    arg2 = int.from_bytes(buf[8:12], "big")

    logging.info("[*] Opcode: {}".format(opcode))
    logging.info("[*] Arg1: {}".format(arg1))
    logging.info("[*] Arg2: {}".format(arg2))

    # 4. Bob -> Alice: opcode (4 bytes) || result (4 bytes)
    # opcode 2 means this packet is the server's reply.
    result = arg1 + arg2
    logging.info("[*] Result: {}".format(result))
    sock.sendall(int.to_bytes(OPCODE_REPLY, 4, "big"))
    sock.sendall(int.to_bytes(result, 4, "big"))

    sock.close()


def run(addr, port):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((addr, port))

    server.listen(2)
    logging.info("[*] Server is Listening on {}:{}".format(addr, port))

    while True:
        client, info = server.accept()

        logging.info("[*] Server accept the connection from {}:{}".format(info[0], info[1]))

        # Each accepted client gets its own thread so the listener can return
        # to accept() and wait for another connection.
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
