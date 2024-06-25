#! /usr/bin/env python3
import argparse
import concurrent.futures
import logging
import socket

HOST = "127.0.0.1"
PORT = 54829


# Funzione per ricevere n byte
def receive_bytes(conn, n):
    chunks = b''
    bytes_recd = 0
    while bytes_recd < n:
        chunk = conn.recv(min(n - bytes_recd, 1024))
        if len(chunk) == 0:
            raise RuntimeError("Connessione chiusa dal client")
        chunks += chunk
        bytes_recd = bytes_recd + len(chunk)
    return chunks


def connection_handler(conn, file, log):
    data = open(file, "rb").read()

    # Invio la lunghezza del file
    conn.sendall(len(data).to_bytes(4, byteorder="big"))

    # Elimino i commenti che iniziano con %
    data = data.split(b"\n")
    data = [line for line in data if not line.startswith(b"%")]

    # Prelevo il numero di nodi e di archi dalla prima riga
    n1, n2, a = map(int, data[0].split())
    data = data[1:]

    n = n1 if n1 == n2 else -1
    if n == -1:
        print(f"{file} Errore: il grafo non è orientato")
        return

    conn.sendall(n.to_bytes(4, byteorder="big"))
    conn.sendall(a.to_bytes(4, byteorder="big"))

    # Invio il file
    for line in data:
        # Controllo se la riga è vuota
        if line == b'':
            continue
        i, j = map(int, line.split(b' '))
        conn.sendall(i.to_bytes(4, byteorder="big"))
        conn.sendall(j.to_bytes(4, byteorder="big"))

    # Ricevo il codice di errore
    data = receive_bytes(conn, 4)
    code = int.from_bytes(data, byteorder="big")

    print(f"{file} Exit code: {code}")

    if code != 0:
        print(f"{file} Exit code: {code}")
        return

    # Ricevo stdout
    data = receive_bytes(conn, 1024)
    text = data.decode("utf-8")
    lines = text.split("\n")
    for line in lines:
        print(f"{file} {line}")

    print("Bye")


def main(files):
    log = logging.getLogger("server.log")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as conn:
        futures = []
        for file in files:
            conn.connect((HOST, PORT))
            with concurrent.futures.ThreadPoolExecutor() as executor:
                futures.append(executor.submit(connection_handler, conn, file, log))

            concurrent.futures.wait(futures)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, filename="server.log", filemode="w")
    parser = argparse.ArgumentParser(description="Client per il server di grafi",
                                     formatter_class=argparse.RawTextHelpFormatter)

    # Aggiungo come argomento la lista dei file
    parser.add_argument("files", metavar="file", type=str, nargs="+", help="Lista dei file da inviare al server")

    args = parser.parse_args()
    main(args.files)
