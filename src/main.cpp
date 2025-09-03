
#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in
#include <arpa/inet.h> // For inet_addr()
#include <cstdlib> // For exit() and EXIT_FAILURE
#include <iostream> // For cout
#include <unistd.h> // For read
#include <poll.h> // For poll()
#include <fcntl.h> // For fcntl()
#include <chrono> // For C++11's <chrono> library
#include <random>

#include "tcp_connector.hpp"
#include "db_connector.hpp"

const char * DB = "../posgw.db";
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

                      long ts = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

              // simple random nonce
              std::mt19937_64 rng(std::random_device{}());
              uint64_t nonce = rng();

              std::string msg = "AUTH|" + std::to_string(args.amount) +
                          "|" + std::to_string(ts) +
                          "|" + std::to_string(nonce) + "\n";
              conn.send(msg + "\n");
              std::string resp = conn.read(5000);
              std::cout << "Response: " << resp << "\n";

              DBConnector db(DB);

              db.write("INSERT INTO transactions (amount, resp) VALUES (" +
                 std::to_string(args.amount) + ", '" + resp + "');");
              
            } catch (const std::exception& e) {
              std::cerr << "TCP error: " << e.what() << "\n";
              return 2;
            }
          return 0;

        case Cmd::Last:
          try {
               DBConnector db(DB);

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
                DBConnector db(DB);

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

  return 0;
}