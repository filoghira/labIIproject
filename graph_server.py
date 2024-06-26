#! /usr/bin/env python3
import concurrent.futures
import socket
import struct
import subprocess
import tempfile
import logging

# Indirizzo e porta del server
HOST = "127.0.0.1"
PORT = 54829

# Configurazione del logger
logging.basicConfig(level=logging.INFO, filename="server.log", filemode="w")
log = logging.getLogger("server.log")


def main(host=HOST, port=PORT):
    # Creo il socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, port))
        # Mi metto in ascolto
        s.listen()
        # Creo un pool di thread
        with concurrent.futures.ThreadPoolExecutor() as executor:
            while True:
                # Finché non ricevo un segnale di interruzione SIGUSR1
                try:
                    # Accetto la connessione
                    conn, addr = s.accept()
                    # Faccio partire un thread
                    executor.submit(connection_handler, conn)
                except KeyboardInterrupt:
                    executor.shutdown()
                    break
    print("Bye da server")


# Funzione per controllare la validità degli archi
def check_validity(n, i, j):
    if i < 1 or i > n or j < 1 or j > n:
        return False
    return True


def connection_handler(conn: socket):
    # Creo un file temporaneo per salvare il grafo
    with conn, tempfile.NamedTemporaryFile(suffix=".mtx") as f:
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

        # Eseguo il pagerank
        res = subprocess.run(["./pagerank", f.name], capture_output=True)

        log.info(f"Processo terminato con codice {res.returncode}")
        log.info(f"stdout: {res.stdout}")

        # Invio il codice di exit
        conn.sendall(res.returncode.to_bytes(4, byteorder="big"))

        # Controllo se il processo è terminato correttamente
        if res.returncode == 0:
            # Invio la lunghezza dello stdout
            conn.sendall(len(res.stdout).to_bytes(4, byteorder="big"))
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
    main()