#if !defined FTPServer_H
#define FTPServer_H

#include <list>


#include "ClientConnection.h"

class FTPServer {
public:
  FTPServer(int port = 21);
  void run();
  void stop();

private:
  int port;
  int msock;
  std::list<ClientConnection*> connection_list;
};
// Hace falta añadir esta funcion al .h para que la pueda leer el archivo ClientConnection
int define_socket_TCP(int port);

#endif