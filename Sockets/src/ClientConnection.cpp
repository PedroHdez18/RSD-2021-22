//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//              This class processes an FTP transaction.
// 
//****************************************************************************



#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h> 
#include <iostream>
#include <dirent.h>

#include "common.h"

#include "ClientConnection.h"
#include "FTPServer.h"



ClientConnection::ClientConnection(int s) {
  int sock = (int)(s);
  char buffer[MAX_BUFF];
  control_socket = s;
  fd = fdopen(s, "a+");
  if (fd == NULL) {
    std::cout << "Connection closed" << std::endl;
    fclose(fd);
    close(control_socket);
    ok = false;
    return;
  }
  
  ok = true;
  data_socket = -1;
  parar = false;
};


ClientConnection::~ClientConnection() {
 	fclose(fd);
	close(control_socket); 
}


int connect_TCP( uint32_t address,  uint16_t  port) {

	struct sockaddr_in sin;
	struct hostent *hent;
	int s;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	sin.sin_addr.s_addr = address;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		errexit("No se puede crear el socket: %s\n", strerror(errno));
  }
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		errexit("No se puede conectar con %d: %s\n", address, strerror(errno));
  }

	return s;
}






void ClientConnection::stop() {
  close(data_socket);
  close(control_socket);
  parar = true;
}





    
#define COMMAND(cmd) strcmp(command, cmd)==0

// This method processes the requests.
// Here you should implement the actions related to the FTP commands.
// See the example for the USER command.
// If you think that you have to add other commands feel free to do so. You 
// are allowed to add auxiliary methods if necessary.

void ClientConnection::WaitForRequests() {

// BOOLEANO QUE COMPRUEBA QUE SE HAYA INICIADO SESION EN EL SISTEMA
  bool iniciado{false};
  if (!ok) {
    return;
  }

  fprintf(fd, "220 Service ready\n");

  while(!parar) {

    fscanf(fd, "%s", command);
    if (COMMAND("USER")) {
    fscanf(fd, "%s", arg);
      if (strcmp(arg, "jonas") == 0) {
        fprintf(fd, "331 User name ok, need password\n");
      } else {
        // No se ha iniciado sesion, error de nombre
        fprintf(fd, "530 Not logged in.\n");
      }
    } else if (COMMAND("PWD")) {
      char arg2[MAX_BUFF];

      if (iniciado == true ) {
        strcat(arg2, getcwd(arg, sizeof(arg)));
        fprintf(fd, "257 %s is current directory\n", arg2);
      }

      else if(iniciado == false) {
        fprintf(fd, "530 Access denied, not logged in.\n");
      } else {
        fprintf(fd, "450 Error, system busy.\n");
      }
    } else if (COMMAND("PASS")) {
      fscanf(fd, "%s", arg);
      if(strcmp(arg,"1234") == 0){
        fprintf(fd, "230 User logged in\n");
        iniciado = true;
      } else {
        // contraseña incorrecta, saliendo
        fprintf(fd, "530 Not logged in.\n");
        parar = true;
      }
    
    } else if (COMMAND("PORT")) {
      int h0, h1, h2, h3;
      int p0, p1;
      fscanf(fd, "%d,%d,%d,%d,%d,%d", &h0, &h1, &h2, &h3, &p0, &p1);
      uint32_t address = h3 << 24 | h2 << 16 | h1 << 8 | h0;
      uint32_t port = p0 << 8 | p1;
      data_socket = connect_TCP(address, port);
      fprintf(fd, "200 OK.\n");
    } else if (COMMAND("PASV")) {
      int s, p0, p1;
      uint16_t port;
      struct sockaddr_in sin;
      socklen_t slen;
      slen = sizeof(sin);

      s = define_socket_TCP(0);
      getsockname(s,(struct sockaddr *)&sin, &slen);

      port = sin.sin_port;
      p0 = (port &  0xff);
      p1 = (port >> 8);
      fprintf(fd,"227 Entering Passive Mode (127,0,0,1,%d,%d).\n",p0,p1);
      fflush(fd);

      data_socket = accept(s,(struct sockaddr *)&sin,&slen);
    } else if (COMMAND("STOR") ) {
      fscanf(fd, "%s", arg);
      printf("STOR: %s\n", arg);

      char Buffer[MAX_BUFF];
      int file;
      int aux;
      if (iniciado == true ) {
        file = open(arg, O_RDWR | O_CREAT, S_IRWXU);
        fprintf(fd, "150 File ok, creating connection\n");
        fflush(fd);

        if (file < 0) {
          fprintf(fd, "450 Requested file action not taken.\n");
          close(data_socket);
        } else {
          fflush(fd);
          struct sockaddr_in sa;
          socklen_t sa_len = sizeof(sa);
          char buffer[MAX_BUFF];
          int i;
          do {
            aux = read(data_socket, Buffer, sizeof(Buffer));
            write(file, Buffer, aux);
          } while (aux > 0);

          fprintf(fd, "226 Closing data connection.\n");
          close(file);
          close(data_socket);
        }
      } else if (iniciado == false) {
        fprintf(fd, "530 Access denied, not logged in.\n");   
      } else {
        fprintf(fd, "450 Error, system busy.\n");
      }
    } else if (COMMAND("RETR")) {
      fscanf(fd, "%s", arg);
      printf("RETR: Retrieving file %s\n", arg);

      FILE *file = fopen(arg, "rb");
      if (iniciado == true ) {
        if (!file) {
          fprintf(fd, "450 Requested file action cancelled. File isn't available.\n");
          close(data_socket);
        } else {
          fprintf(fd, "150 File status okay; oppening conection.\n");
          struct sockaddr_in sa;
          socklen_t sa_len = sizeof(sa);
          char buffer[MAX_BUFF];
          int n;
          do {
            n = fread(buffer, sizeof(char), MAX_BUFF, file);
            send(data_socket, buffer, n, 0);
          } while (n == MAX_BUFF);
          fprintf(fd, "226 Closing data connection.\n");
          fclose(file);
          close(data_socket);
        }
      } else if(iniciado == false) {
        fprintf(fd, "530 Access denied, not logged in.\n");
      } else {
        fprintf(fd, "450 Error, system busy.\n");
      }
    } else if (COMMAND("LIST")) {
      if(iniciado == true) {
        printf("(LIST): Listing current directory\n");
        fprintf(fd, "125 Data connection already open.\n");

        struct sockaddr_in sa;
        socklen_t sa_len = sizeof(sa);
        char buffer[MAX_BUFF];
        std::string list_str;
        FILE *file = popen("ls", "r");
        if (!file) {
          fprintf(fd, "450 Requested proccess stoped.\n");
          close(data_socket);
        } else {
          while (!feof(file)) {
            if (fgets(buffer, MAX_BUFF, file) != NULL) {
              list_str.append(buffer);
            }
          }
          send(data_socket, list_str.c_str(), list_str.size(), 0);
          fprintf(fd, "250 Closing data connection.\n");
          pclose(file);
          close(data_socket);
        }
      } else if(iniciado == false) {
        fprintf(fd, "530 Not logged in.\n");
      } else {
        fprintf(fd, "450 Error, system busy.\n");
      }
    } else if (COMMAND("SYST")) {
      fprintf(fd, "215 UNIX Type: L8.\n");   
    } else if (COMMAND("TYPE")) {
      fscanf(fd, "%s", arg);
      fprintf(fd, "200 OK\n");   
    } else if (COMMAND("QUIT")) {
      fprintf(fd, "221 Service closing control connection. Logged out if appropriate.\n");
      close(data_socket);	
      parar=true;
      break;
    } else {
    fprintf(fd, "502 Command not implemented.\n"); fflush(fd);
    printf("Comando : %s %s\n", command, arg);
    printf("Error interno del servidor\n");
    }
  }
  
  fclose(fd);
  return; 
}