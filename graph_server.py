#! /usr/bin/env python3
import concurrent.futures
import socket
import struct
import sys
import threading

HOST = "127.0.0.1"
PORT = 54829


def main(host=HOST, port=PORT):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((host, port))
            s.listen()
            with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
                while True:
                    conn, addr = s.accept()
                    executor.submit(gestisci_connessione, conn, addr)
        except KeyboardInterrupt:
            pass
        s.shutdown(socket.SHUT_RDWR)


def gestisci_connessione(conn, addr):
    with conn, open("grafo.mtx", "w") as f:
        # Ricevo i 2
        data = recv_all(conn, 8)
        assert len(data) == 8, "Errore ricezione interi"
        n = struct.unpack("!i", data[:4])[0]
        a = struct.unpack("!i", data[4:])[0]

        # Ricevo a coppie di interi
        while True:
            data = recv_all(conn, 8)
            if len(data) == 0:
                break
            assert len(data) == 8, "Errore ricezione coppia di interi"
            a = struct.unpack("!i", data[:4])[0]
            b = struct.unpack("!i", data[4:])[0]
            f.write(f"{a} {b}\n")

# riceve esattamente n byte e li restituisce in un array di byte
# il tipo restituto è "bytes": una sequenza immutabile di valori 0-255
# analoga alla readn che abbiamo visto nel C
def recv_all(conn, n):
    chunks = b''
    bytes_recd = 0
    while bytes_recd < n:
        chunk = conn.recv(min(n - bytes_recd, 1024))
        if len(chunk) == 0:
            raise RuntimeError("socket connection broken")
        chunks += chunk
        bytes_recd = bytes_recd + len(chunk)
    return chunks


# restituisce lista dei primi in [a,b]
def elenco_primi(a, b):
    ris = []
    for i in range(a, b + 1):
        if primo(i):
            ris.append(i);
    return ris


# dato un intero n>0 restituisce True se n e' primo
# False altrimenti
def primo(n):
    assert n > 0, "L'input deve essere positivo"
    if n == 1:
        return False
    if n == 2:
        return True
    if n % 2 == 0:
        return False
    assert n >= 3 and n % 2 == 1, "C'e' qualcosa che non funziona"
    for i in range(3, n // 2, 2):
        # fa attendere solamente questo thread
        # threading.Event().wait(.5)
        if n % i == 0:
            return False
        if i * i > n:
            break
    return True
