import socket

# Connect to the kvstore server.
HOST, PORT = "localhost", 6380
s = socket.create_connection((HOST, PORT))
print(f"connected to kvstore server at {HOST}:{PORT}")
print("type commands like: SET city Ludhiana | GET city | DEL city | COMPACT | quit\n")

while True:
    line = input("> ").strip()
    if line.lower() in ("quit", "exit"):
        break
    if not line:
        continue

    s.sendall((line + "\n").encode())

    # The server sends one newline-terminated response per command.
    chunks = []
    while True:
        chunk = s.recv(4096)
        if not chunk:
            break
        chunks.append(chunk)
        if b"\n" in chunk:
            break

    reply = b"".join(chunks).decode(errors="replace").strip()
    print(reply)

s.close()
