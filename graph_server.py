#! /usr/bin/env python3
import concurrent.futures
import socket
import struct
import subprocess
import tempfile
import logging

HOST = "127.0.0.1"
PORT = 54829


def main(host=HOST, port=PORT):
    log = logging.getLogger("server.log")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, port))
        s.listen()
        with concurrent.futures.ThreadPoolExecutor() as executor:
            while True:
                try:
                    conn, addr = s.accept()
                    executor.submit(connection_handler, conn, log)
                except KeyboardInterrupt:
                    executor.shutdown()
                    break
    print("Bye da server")


# Funzione per controllare la validità degli archi
def check_validity(n, i, j):
    if i < 0 or i >= n or j < 0 or j >= n:
        return False
    return True


def connection_handler(conn: socket, log: logging.Logger):
    with conn, tempfile.NamedTemporaryFile(suffix=".mtx") as f:
        print(f.name)
        # Ricevo la lunghezza del file
        len_file = receive_bytes(conn, 4)

        # Ricevo la dimensione del grafo e il numero di archi
        data = receive_bytes(conn, 8)

        # Controllo che la lunghezza sia corretta
        assert len(data) == 8, "Errore ricezione interi"

        # Estraggo i valori
        n = struct.unpack("!i", data[:4])[0]
        a = struct.unpack("!i", data[4:])[0]

        # Scrivo i valori nel file temporaneo
        f.write(f"{n} {n} {a}\n".encode())

        log.info(f"Ricevuto grafo di {n} nodi")
        log.info(f"Nome del file temporaneo: {f.name}")

        # Contatore per gli archi validi
        count = 0

        for _ in range(a):
            # Leggo un arco
            data = receive_bytes(conn, 8)

            print(data)

            # Controllo che la lunghezza sia corretta
            assert len(data) == 8, "Errore ricezione riga"

            # Estraggo i valori
            i, j = struct.unpack("!ii", data)

            # Controllo la validità dell'arco
            if check_validity(n, i, j):
                # Scrivo l'arco nel file temporaneo
                f.write(f"{i} {j}\n".encode())
                count += 1

        log.info(f"Archi scartati: {a - count}")
        log.info(f"Archi validi: {count}")

        # Copio il contenuto del file temporaneo nel file temp.mtx
        f.flush()
        f.seek(0)
        with open("temp.mtx", "wb") as f2:
            f2.write(f.read())

        # Eseguo il pagerank
        res = subprocess.run(["./pagerank", f.name])

        log.info(f"Processo terminato con codice {res.returncode}")
        log.info(f"stdout: {res.stdout}")

        # Controllo se il processo è terminato correttamente
        if res.returncode != 0:
            # Invio il codice di errore
            conn.sendall(res.returncode.to_bytes(4, byteorder="big"))
            return
        else:
            # Invio stdout
            conn.sendall(res.stdout)

        # Chiudo il file temporaneo
        f.close()
        # Chiudo la connessione
        conn.close()


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


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, filename="server.log", filemode="w")
    main()
