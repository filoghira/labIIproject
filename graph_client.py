#! /usr/bin/env python3
import argparse
import concurrent.futures
import logging
import socket
import os.path

# Indirizzo e porta del server
HOST = "127.0.0.1"
PORT = 54829

# Configuro il logger
logging.basicConfig(level=logging.INFO, filename="client.log", filemode="w")
log = logging.getLogger("graph_client")


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


# Funzione per la gestione della connessione (thread)
def connection_handler(file):
    # Creo la connessione
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as conn:
        # Mi connetto al server
        conn.connect((HOST, PORT))
        data = open(file, "rb").read()

        # Elimino i commenti che iniziano con %
        data = data.split(b"\n")
        data = [line for line in data if not line.startswith(b"%")]

        # Prelevo il numero di nodi e di archi dalla prima riga
        n1, n2, a = map(int, data[0].split())
        data = data[1:]
        n = n1

        # Invio il numero di nodi e di archi
        conn.sendall(n.to_bytes(4, byteorder="big"))
        conn.sendall(a.to_bytes(4, byteorder="big"))

        # Invio il file riga per riga
        for line in data:
            # Controllo se la riga è vuota
            if line == b'':
                continue
            i, j = map(int, line.split(b' '))
            conn.sendall(i.to_bytes(4, byteorder="big"))
            conn.sendall(j.to_bytes(4, byteorder="big"))

        # Ricevo il codice di exit dal server
        data = receive_bytes(conn, 4)
        code = int.from_bytes(data, byteorder="big")

        # Stampo il codice di exit
        print(f"{file} Exit code: {code}")
        # Se non è 0, termino
        if code != 0:
            return

        # Ricevo il risultato del calcolo (stdout)
        size = int.from_bytes(receive_bytes(conn, 4), byteorder="big")
        data = receive_bytes(conn, size)
        # Lo decodifico e lo stampo
        text = data.decode("utf-8")
        lines = text.split("\n")
        for line in lines:
            print(f"{file} {line}")

        print("Bye")


def main(files):
    # Lista dei futures per la gestione concorrente
    futures = []

    # Creo un pool di thread
    with concurrent.futures.ThreadPoolExecutor() as executor:
        # Per ogni file nella lista
        for file in files:
            # Avvio il thread per la connessione
            futures.append(executor.submit(connection_handler, file))

        # Attendo la terminazione di tutti i thread
        concurrent.futures.wait(futures)


# Funzione per controllare che il file abbia una delle estensioni specificate
def file_choices(choices, fname):
    ext = os.path.splitext(fname)[1][1:]
    if ext not in choices:
        log.error("file doesn't end with one of {}".format(choices))
    return fname


if __name__ == "__main__":
    # Creo il parser degli argomenti
    parser = argparse.ArgumentParser(description="Client per il server di grafi",
                                     formatter_class=argparse.RawTextHelpFormatter)

    # Aggiungo come argomento la lista dei file
    parser.add_argument("files", metavar="file", type=lambda s: file_choices("mtx", s), nargs="+",
                        help="Lista dei file da inviare al server")

    # Eseguo il parsing degli argomenti
    args = parser.parse_args()

    # Eseguo il client
    main(args.files)
