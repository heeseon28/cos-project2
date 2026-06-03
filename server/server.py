# Edge와 AI 모듈 사이의 통신을 담당 -> 통역사 느낌. 이를 실행하기 위해 vpn을 사용해야함.
import socket
import requests
import threading
import argparse
import logging
import json
import sys

# 이 부분은 opcode.h에 정의된 opcode와 일치해야 함.
OPCODE_DATA = 1  # edge device가 데이터를 보고할 때 사용되는 opcode
OPCODE_WAIT = 2  # edge device가 데이터를 모두 보고한 후, AI module이 모델을 학습하는 동안 edge device가 대기해야 할 때 사용되는 opcode
OPCODE_DONE = 3  # 다음 데이터를 보고할 준비가 되었음을 알리는 opcode
OPCODE_QUIT = 4  # 전체 종료

# Modified 2026-06-03 15:51 KST (feature/version3): version3 2-feature protocol payload length.
# month(4) + avg_power(4) = 8 bytes
PAYLOAD_LEN = 8


class Server:
    def __init__(
        self, name, algorithm, dimension, index, port, caddr, cport, ntrain, ntest
    ):
        logging.info(
            "[*] Initializing the server module to receive data from the edge device"
        )
        self.name = name  # 모델의 이름. AI module이 여러 모델을 지원하는 경우, 모델을 구분하기 위해 사용됨
        self.algorithm = algorithm  # 우린 lstm을 사용하니까 lstm으로 고정
        self.dimension = dimension  # feature의 차원. feature/version3 protocol에서는 2
        self.index = index  # power value의 index. avg_power가 1번째이므로 feature/version3 protocol에서는 1
        self.caddr = caddr  # AI module의 IP 주소
        self.cport = cport  # AI module의 포트 번호
        self.ntrain = ntrain  # AI가 학습하는 training instance의 개수
        self.ntest = ntest  # AI가 예측하는 testing instance의 개수
        success = (
            self.connecter()
        )  # AI module과의 연결을 시도. 연결에 실패하면, 서버는 종료됨

        if (
            success
        ):  # 성공시 edge device와의 통신을 위해 소켓을 생성하고, edge device의 요청을 처리하기 시작함
            self.port = port
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.bind(("0.0.0.0", port))
            self.socket.listen(10)
            self.listener()

    def connecter(self):  # AI module에 모델 생성을 요청하는 함수
        success = True
        self.ai = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.ai.connect((self.caddr, self.cport))
        url = "http://{}:{}/{}".format(self.caddr, self.cport, self.name)
        # AI module에 모델 생성을 요청하기 위해, 모델의 이름, 알고리즘, feature의 차원, power value의 index를 AI module에 전달해야 함.
        """
        예를 들어 옵션이 --caddr 127.0.0.1 --cport 5556 --name local_test 였다면
        url은 http://127.0.0.1:5556/local_test 가 될 것임. 그리고 AI module에 전달되는 json 객체는 다음과 같음:
        """
        request = {}
        request["algorithm"] = self.algorithm  # LSTM
        request["dimension"] = self.dimension  # feature의 차원
        request["index"] = self.index  # power value의 index
        js = json.dumps(request)
        logging.debug("[*] To be sent to the AI module: {}".format(js))
        result = requests.post(url, json=js)  # AI module에 요청을 보내서 모델을 생성.
        response = json.loads(result.content)
        logging.debug("[*] Received: {}".format(response))

        # AI module이 모델 생성을 성공적으로 수행했는지 확인. 실패한 경우, 서버는 종료됨.
        if "opcode" not in response:
            logging.debug("[*] Invalid response")
            success = False
        else:
            if response["opcode"] == "failure":
                logging.error("Error happened")
                if "reason" in response:
                    logging.error("Reason: {}".format(response["reason"]))
                    logging.error("Please try again.")
                else:
                    logging.error("Reason: unknown. not specified")
                success = False
            else:
                assert response["opcode"] == "success"
                logging.info("[*] Successfully connected to the AI module")
        return success

    def listener(self):  # edge의 요청을 처리. 접속할 때까지 기다리는 함수임.
        logging.info("[*] Server is listening on 0.0.0.0:{}".format(self.port))

        while True:
            client, info = self.socket.accept()
            logging.info(
                "[*] Server accept the connection from {}:{}".format(info[0], info[1])
            )

            client_handle = threading.Thread(target=self.handler, args=(client,))
            client_handle.start()

    # Modified 2026-06-03 15:51 KST (feature/version3): TCP recv can return partial data, so read exactly size bytes.
    def recv_exact(self, client, size):
        data = b""
        while len(data) < size:
            chunk = client.recv(size - len(data))
            if not chunk:
                logging.error("[*] connection closed while receiving data")
                sys.exit(1)
            data += chunk
        return data

    # edge에서 받은 데이터를 AI module에 전달.
    def send_instance(self, vlst, is_training):
        if (
            is_training
        ):  # training instance인지 testing instance인지에 따라, 데이터를 전달할 url이 달라짐.
            url = "http://{}:{}/{}/training".format(self.caddr, self.cport, self.name)
        else:
            url = "http://{}:{}/{}/testing".format(self.caddr, self.cport, self.name)
        data = {}
        data["value"] = vlst  # 보낼 feature들의 리스트
        req = json.dumps(data)
        response = requests.put(url, json=req)
        resp = response.json()

        if "opcode" in resp:
            if resp["opcode"] == "failure":
                logging.error("fail to send the instance to the ai module")

                if "reason" in resp:
                    logging.error(resp["reason"])
                else:
                    logging.error("unknown error")
                sys.exit(1)
        else:
            logging.error("unknown response")
            sys.exit(1)

    # edge에서 받은 데이터를 python 정수형 데이터로 변환해서 전달하는 함수.
    # Modified 2026-06-03 15:51 KST (feature/version3): decode 8-byte payload for [month, avg_power].
    def parse_data(self, buf, is_training):
        month = int.from_bytes(buf[0:4], byteorder="big", signed=True)
        avg_power = int.from_bytes(buf[4:8], byteorder="big", signed=True)

        lst = [month, avg_power]
        logging.info("[month, avg_power] = {}".format(lst))

        self.send_instance(lst, is_training)

    # TODO: You should implement your own protocol in this function
    # The following implementation is just a simple example
    # 가장 중요함. edge에서 받은 데이터를 바탕으로 traing과 testing의 흐름을 관리.
    def handler(self, client):
        logging.info("[*] Server starts to process the client's request")

        ntrain = self.ntrain
        url = "http://{}:{}/{}/training".format(self.caddr, self.cport, self.name)

        while True:
            # opcode (1 byte):
            rbuf = self.recv_exact(client, 1)
            opcode = int.from_bytes(rbuf, "big")  # edge가 보낸 opcode를 정수형으로 변환
            logging.debug("[*] opcode: {}".format(opcode))

            if opcode == OPCODE_DATA:  # 잘 수행되었으면 OPCODE_DATA = 1을 받아야함.
                logging.info("[*] data report from the edge")
                rbuf = self.recv_exact(
                    client, PAYLOAD_LEN
                )  # Modified 2026-06-03 15:51 KST (feature/version3): receive 8-byte payload for version3 protocol.
                logging.debug("[*] received buf: {}".format(rbuf))
                self.parse_data(rbuf, True)
            else:
                logging.error("[*] invalid opcode")
                logging.error("[*] please try again")
                sys.exit(1)

            ntrain -= 1  # 받은 training instance의 개수를 하나 줄임.

            # training instance를 모두 받은 경우, AI module이 모델을 학습하는 동안 edge device가 대기해야 하므로, OPCODE_WAIT = 2를 보냄.
            if ntrain > 0:
                opcode = OPCODE_DONE
                logging.debug("[*] send the opcode OPCODE_DONE")
                client.send(int.to_bytes(opcode, 1, "big"))
            else:
                opcode = OPCODE_WAIT
                logging.debug("[*] send the opcode OPCODE_WAIT")
                client.send(int.to_bytes(opcode, 1, "big"))
                break

        result = requests.post(url)
        response = json.loads(result.content)
        logging.debug("[*] return: {}".format(response["opcode"]))

        ntest = self.ntest
        url = "http://{}:{}/{}/testing".format(self.caddr, self.cport, self.name)
        opcode = OPCODE_DONE
        logging.debug("[*] send the opcode OPCODE_DONE")
        # training instance를 모두 받은 후, testing instance를 보낼 준비가 되었음을 알리는 opcode OPCODE_DONE = 3를 보냄.
        client.send(int.to_bytes(opcode, 1, "big"))

        # training과 거의 유사하나, training이 끝난 후 AI module이 모델을 학습하는 동안 edge device가 대기해야 하는 반면, testing은 AI module이 예측을 수행하는 동안 edge device가 대기할 필요가 없음. 따라서 testing에서는 OPCODE_DONE = 3를 보냄.
        while ntest > 0:
            # opcode (1 byte):
            rbuf = self.recv_exact(client, 1)
            opcode = int.from_bytes(rbuf, "big")
            logging.debug("[*] opcode: {}".format(opcode))

            if opcode == OPCODE_DATA:
                logging.info("[*] data report from the edge")
                rbuf = self.recv_exact(client, PAYLOAD_LEN)  # Modified 2026-06-03 15:51 KST (feature/version3): receive 8-byte testing payload.
                logging.debug("[*] received buf: {}".format(rbuf))
                self.parse_data(rbuf, False)
            else:
                logging.error("[*] invalid opcode")
                logging.error("[*] please try again")
                sys.exit(1)

            ntest -= 1

            if ntest > 0:
                opcode = OPCODE_DONE
                client.send(int.to_bytes(opcode, 1, "big"))
            else:
                opcode = OPCODE_QUIT
                client.send(int.to_bytes(opcode, 1, "big"))
                break

        # result를 AI module에서 받아서 출력하는 함수. AI module이 예측을 수행한 후, edge device가 결과를 요청할 때 호출됨.
        url = "http://{}:{}/{}/result".format(self.caddr, self.cport, self.name)
        result = requests.get(url)
        response = json.loads(result.content)
        logging.debug("response: {}".format(response))
        if "opcode" not in response:
            logging.error("invalid response from the AI module: no opcode is specified")
            logging.error("please try again")
            sys.exit(1)
        else:
            if response["opcode"] == "failure":
                logging.error("getting the result from the AI module failed")
                if "reason" in response:
                    logging.error(response["reason"])
                logging.error("please try again")
                sys.exit(1)
            elif response["opcode"] == "success":
                self.print_result(response)
            else:
                logging.error("unknown error")
                logging.error("please try again")
                sys.exit(1)

    # AI module에서 받아온 예측 결과를 출력하는 함수. AI module이 예측을 수행한 후, edge device가 결과를 요청할 때 호출됨.
    def print_result(self, result):
        logging.info("=== Result of Prediction ({}) ===".format(self.name))
        logging.info("   # of instances: {}".format(result["num"]))
        logging.debug("   sequence: {}".format(result["sequence"]))
        logging.debug("   prediction: {}".format(result["prediction"]))
        logging.info("   correct predictions: {}".format(result["correct"]))
        logging.info("   incorrect predictions: {}".format(result["incorrect"]))
        logging.info("   accuracy: {}\%".format(result["accuracy"]))


# 터미널에서 입력한 옵션들을 처리하는 함수들.
# Modified 2026-06-03 15:51 KST (feature/version3) 예시 : python3 server.py --algorithm lstm --dimension 2 --index 1 --ntrain 10 --ntest 7 --name version3_test --caddr 127.0.0.1 --cport 5556 --lport 6000 --log DEBUG
def command_line_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-a",
        "--algorithm",
        metavar="<AI algorithm to be used>",
        help="AI algorithm to be used",
        type=str,
        required=True,
    )
    parser.add_argument(
        "-d",
        "--dimension",
        metavar="<Dimension of each instance>",
        help="Dimension of each instance",
        type=int,
        default=1,
    )
    parser.add_argument(
        "-b",
        "--caddr",
        metavar="<AI module's IP address>",
        help="AI module's IP address",
        type=str,
        required=True,
    )
    parser.add_argument(
        "-c",
        "--cport",
        metavar="<AI module's listening port>",
        help="AI module's listening port",
        type=int,
        required=True,
    )
    parser.add_argument(
        "-p",
        "--lport",
        metavar="<server's listening port>",
        help="Server's listening port",
        type=int,
        required=True,
    )
    parser.add_argument(
        "-n",
        "--name",
        metavar="<model name>",
        help="Name of the model",
        type=str,
        default="model",
    )
    parser.add_argument(
        "-x",
        "--ntrain",
        metavar="<number of instances for training>",
        help="Number of instances for training",
        type=int,
        default=10,
    )
    parser.add_argument(
        "-y",
        "--ntest",
        metavar="<number of instances for testing>",
        help="Number of instances for testing",
        type=int,
        default=10,
    )
    parser.add_argument(
        "-z",
        "--index",
        metavar="<the index number for the power value>",
        help="Index number for the power value",
        type=int,
        default=0,
    )
    parser.add_argument(
        "-l",
        "--log",
        metavar="<log level (DEBUG/INFO/WARNING/ERROR/CRITICAL)>",
        help="Log level (DEBUG/INFO/WARNING/ERROR/CRITICAL)",
        type=str,
        default="INFO",
    )
    args = parser.parse_args()
    return args


def main():
    args = command_line_args()
    logging.basicConfig(level=args.log)

    if args.ntrain <= 0 or args.ntest <= 0:
        logging.error(
            "Number of instances for training or testing should be larger than 0"
        )
        sys.exit(1)

    Server(
        args.name,
        args.algorithm,
        args.dimension,
        args.index,
        args.lport,
        args.caddr,
        args.cport,
        args.ntrain,
        args.ntest,
    )


if __name__ == "__main__":
    main()
