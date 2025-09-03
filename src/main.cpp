
#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in
#include <arpa/inet.h> // For inet_addr()
#include <cstdlib> // For exit() and EXIT_FAILURE
#include <iostream> // For cout
#include <unistd.h> // For read
#include <poll.h> // For poll()
#include <fcntl.h> // For fcntl()
#include <chrono> // For C++11's <chrono> library

#include "tcp_connector.hpp"
#include "db_connector.hpp"

const char * create = "CREATE TABLE IF NOT EXISTS transactions (" "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                 "amount REAL, resp TEXT, "
                 "ts DATETIME DEFAULT CURRENT_TIMESTAMP);";
struct Args {
    std::string cmd;
    double      amount = 0.0;
    std::string host   = "127.0.0.1";
    int         port   = 9000;
    int         n      = 5;
};

static bool parse_args(int argc, char** argv, Args& a) {
    if (argc < 2) {
       std::cerr << "missing arguments \n";
    return false;
    }
      
    a.cmd = argv[1];

    for (int i = 2; i < argc; ++i) {
        std::string k = argv[i];

        auto need = [&](const char* flag) -> const char* {
            if (++i >= argc) { std::cerr << "missing value for " << flag << "\n"; return nullptr; }
            return argv[i];
        };

        if (k == "--amount") { const char* v = need("--amount"); if (!v) return false; a.amount = std::stod(v); }
        else if (k == "--host") { const char* v = need("--host"); if (!v) return false; a.host = v; }
        else if (k == "--port") { const char* v = need("--port"); if (!v) return false; a.port = std::stoi(v); }
        else if (k == "--n")    { const char* v = need("--n");    if (!v) return false; a.n    = std::stoi(v); }
        else { std::cerr << "unknown flag: " << k << "\n"; return false; }
    }

    if (a.cmd == "sale") {
        if (a.amount <= 0.0) { std::cerr << "sale requires --amount > 0\n"; return false; }
    } else if (a.cmd == "last") {
        if (a.n <= 0) a.n = 5;
    } else if (a.cmd == "recon") {
        // no options
    } else {
        std::cerr << "unknown command: " << a.cmd << "\n";
        return false;
    }
    return true;
}


enum class Cmd { Sale, Last, Recon, Unknown };

 static Cmd cmd_from(const std::string& s) {
    if (s == "sale")  return Cmd::Sale;
    if (s == "last")  return Cmd::Last;
    if (s == "recon") return Cmd::Recon;
    return Cmd::Unknown;
}

int main(int argc, char* argv[]) {

  Args args;
  if(!parse_args(argc, argv, args) )
  {
    return 2;
  }

  
   switch (cmd_from(args.cmd)) {
        case Cmd::Sale:
            try {
              TCPConnector conn(args.host, args.port);
              conn.send(std::to_string(args.amount) + "\n");
              std::string resp = conn.read(5000);
              std::cout << "Response: " << resp << "\n";

              DBConnector db("../posgw.db");
              db.write(create);

              db.write("INSERT INTO transactions (amount, resp) VALUES (" +
                 std::to_string(args.amount) + ", '" + resp + "');");
              
            } catch (const std::exception& e) {
              std::cerr << "TCP error: " << e.what() << "\n";
              return 2;
            }
          return 0;

        case Cmd::Last:
          try {
               DBConnector db("../posgw.db");
               db.write(create);

               std::string sql = "SELECT amount, resp FROM transactions "
                                 "ORDER BY id DESC LIMIT " + std::to_string(args.n) + ";";

               auto rows = db.read(sql);
               for (auto& row : rows) {
                   std::cout << "amount=" << row[0] << " resp=" << row[1] << "\n";
               }
               if (rows.empty()) {
                   std::cout << "no transactions found\n";
               }
           } catch (const std::exception& e) {
               std::cerr << "DB error: " << e.what() << "\n";
               return 1;
           }
           return 0;
        case Cmd::Recon:
            try {
                DBConnector db("../posgw.db");
                db.write(create);

                std::string sql =
                    "SELECT date(ts), "
                    "SUM(CASE WHEN resp LIKE 'APPROVED%' THEN amount ELSE 0 END) as approved_sum, "
                    "COUNT(CASE WHEN resp LIKE 'APPROVED%' THEN 1 END) as approved_count, "
                    "SUM(CASE WHEN resp LIKE 'DECLINED%' THEN amount ELSE 0 END) as declined_sum, "
                    "COUNT(CASE WHEN resp LIKE 'DECLINED%' THEN 1 END) as declined_count "
                    "FROM transactions GROUP BY date(ts) ORDER BY date(ts) DESC;";

                auto rows = db.read(sql);
                for (auto& row : rows) {
                    // row[0]=date, row[1]=approved_sum, row[2]=approved_count,
                    // row[3]=declined_sum, row[4]=declined_count
                    std::cout << "date=" << row[0]
                              << " approved(count=" << row[2] << ", sum=" << row[1] << ")"
                              << " declined(count=" << row[4] << ", sum=" << row[3] << ")"
                              << "\n";
                }
                if (rows.empty()) {
                    std::cout << "no data for recon\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "DB error: " << e.what() << "\n";
                return 1;
          }
        return 0;

        default:
            std::cerr << "unknown command: " << args.cmd << "\n";
            return 2;
    }

  // Create a socket (IPv4, TCP)
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    std::cout << "Failed to create socket. errno: " << errno << std::endl;
    exit(EXIT_FAILURE);
  }

  // Connect to the server at 127.0.0.1:9999
  sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  sockaddr.sin_port = htons(9999);
  if (connect(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
    std::cout << "Failed to connect to server. errno: " << errno << std::endl;
    exit(EXIT_FAILURE);
  }

  // Set the socket to non-blocking mode
  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

  // Send a message to the server
  std::string message = "Hello, server!\n";
  if (write(sockfd, message.c_str(), message.size()) < 0) {
    std::cout << "Failed to send message to server. errno: " << errno << std::endl;
    exit(EXIT_FAILURE);
  }

  // Set up poll structure
  pollfd fds;
  fds.fd = sockfd;
  fds.events = POLLIN;

  // Specify a timeout of 5000 milliseconds using <chrono>
  auto timeout = std::chrono::milliseconds(5000);

  while (true) {
    // Poll for a response from the server with the specified timeout
    int poll_count = poll(&fds, 1, timeout.count());
    if (poll_count < 0) {
      std::cout << "Failed to poll. errno: " << errno << std::endl;
      exit(EXIT_FAILURE);
    }

    // Check if there's data to read
    if (fds.revents & POLLIN) {
      char buffer[100];
      auto bytesRead = read(sockfd, buffer, 100);
      if (bytesRead <= 0) {
        // If read() fails with EWOULDBLOCK, it means that there is no data to
        // read and we can continue.
        if (errno != EWOULDBLOCK && bytesRead < 0) {
          std::cout << "Failed to read from server. errno: " << errno << std::endl;
          exit(EXIT_FAILURE);
        }
        if (bytesRead == 0) {
          // The server has closed the connection
          std::cout << "The server has closed the connection" << std::endl;
          break;
        }
      } else {
        std::cout << "The server said: " << buffer;
      }
    }
  }

  // Close the connection
  close(sockfd);
}