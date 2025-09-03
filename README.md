# TCPClient
chatGPT session -https://chatgpt.com/share/68b864f5-728c-8011-a4bb-44997a2bd91b

The project is a CLI-based POS gateway emulator built in C++17 on Linux. It connects over TCP, exchanges messages using a simplified OPI-Lite protocol, and persists results in SQLite.

TCPConnector
RAII class handling socket lifecycle, non-blocking I/O with poll, connect retries, OPI-Lite handshake (HELLO|GW|1.0 ↔ HELLO|TERM|1.0), and keepalive (PING/PONG).

DBConnector
RAII wrapper around SQLite that ensures schema creation on startup and provides simple write and read methods.


CLI Dispatcher
Simple switch on cmd with three commands:

sale → connect, send sale, store result.

last → print last N transactions from DB.

recon → show daily totals of approved/declined.

Known limitations or functionality that you decided to drop due to time constraints

The implementation has not been thoroughly tested against error conditions, so edge cases may cause unexpected behavior.
Responses from the OPI-Lite protocol are only read and stored; their format is not fully validated for correctness.	

What you’d build next if you had more time?

I would extend the design by applying the Decorator patternto separate low-level connectors from business logic. In addition, I would consider replacing the current switch dispatcher with a Chain of Responsibility approach to make command handling cleaner and more extensible.
