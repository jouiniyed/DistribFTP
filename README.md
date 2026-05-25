# FTP Server

A simple FTP-like file transfer system with a master/slave architecture, built on top of the CS:APP socket library.

## Architecture

Three processes work together:

```
ftpclient
    |
    |-- connect port 2121 --> ftpmaster
    |<-- slave_addr_t (ip + port) --
    |-- close --
    |
    |-- connect port slave_port --> ftpslave
    |-- request_t (GET / PUT / LS / RM / BYE) -->
    |<-- response_t + data ---
    |-- close --
```

**ftpmaster** sits on port 2121. When a client connects, it picks an available slave using round-robin, sends its address, then closes. The client then talks directly to the slave.

**ftpslave** runs a pool of 10 pre-forked worker processes. Each worker loops on `Accept()` to handle clients. Additional slaves can be passed as arguments; writes (PUT/RM) are propagated to all of them.

**ftpserveri** is a standalone single-node version with the same worker pool, no master required.

## Wire Protocol

All messages are fixed-size binary structs sent with `Rio_writen` / `Rio_readnb`.

**`request_t`** (client → slave)

| Field | Type | Description |
|---|---|---|
| `type` | `typereq_t` | GET, PUT, LS, RM, or BYE |
| `nom` | `char[256]` | filename |
| `offset` | `size_t` | blocks already received (0 = fresh download) |
| `propagate` | `int` | 1 = already propagated, skip; 0 = forward to peers |

**`response_t`** (slave → client): a single `codeRetour` field — `SUCCES` or `ERREUR`.

**`slave_addr_t`** (master → client): `char ip[64]` + `int port`. Port 0 means no slave available.

### GET sequence

```
client                          slave
  |-- request_t (GET, offset) -->|
  |<-- response_t (SUCCES) ------|
  |<-- size_t nb_blocks ---------|
  |<-- block[0..n] (512 B each) -|
```

### PUT sequence

```
client                          slave
  |-- request_t (PUT) ---------->|
  |<-- response_t (SUCCES) ------|
  |-- size_t nb_blocks ---------->|
  |-- block[0..n] (512 B each) -->|
```

### LS / RM

LS returns `response_t`, then `size_t n`, then `n` bytes of newline-separated filenames.
RM returns `response_t` only.

After a successful PUT or RM, the slave forwards the same request (with `propagate=1`) to all its peers.

## Commands

| Command | Description |
|---|---|
| `get <file>` | Download a file from the server into `dirClient/` |
| `put <file>` | Upload `dirClient/<file>` to the server |
| `rm <file>` | Delete a file on the server |
| `ls` | List files in the server's `dirServer/` |
| `bye` | Close the session |

## Build

```bash
make
```

Produces: `ftpclient`, `ftpmaster`, `ftpslave`, `ftpserveri`.

```bash
make clean
```

## Usage

### Standalone server

```bash
# Terminal 1
./ftpserveri

# Terminal 2
./ftpclient localhost
get lorem.txt
ls
bye
```

### Master/slave setup

```bash
# Terminal 1 — slave
./ftpslave 2122

# Terminal 2 — master (tell it where the slave is)
./ftpmaster localhost 2122

# Terminal 3 — client
./ftpclient localhost
get lorem.txt
bye
```

### Two slaves with propagation

```bash
# Terminal 1
./ftpslave 2122 localhost 2123

# Terminal 2
./ftpslave 2123 localhost 2122

# Terminal 3 — master must know NB_SLAVES=2 (set in ftpmaster.c)
./ftpmaster localhost 2122 localhost 2123

# Terminal 4
./ftpclient localhost
put myfile.txt   # uploaded to slave 1, auto-propagated to slave 2
rm myfile.txt    # deleted on slave 1, auto-propagated to slave 2
bye
```

### Crash recovery

If a GET is interrupted (Ctrl-C), a `.prog` file records how many 512-byte blocks were received. Re-running the same `get` command resumes from that block. The `.prog` file is deleted once the transfer completes.

If the slave dies mid-transfer, the client detects the broken write, reconnects to the master, gets a new slave address, and retries with the current offset.

### Ports

| Process | Port |
|---|---|
| ftpmaster | 2121 |
| ftpslave | 2122 (overridable via argv) |
