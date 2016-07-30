#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <sys/sendfile.h>
#include <fcntl.h>

void syserr(char* msg) { perror(msg); exit(-1); }

int main(int argc, char* argv[])
{
  int sockfd, portno, n, fileSize;
  struct hostent* server;
  struct sockaddr_in serv_addr;
  char buffer[256];
  char fileSizeBuffer[256];
  DIR *dir;
  struct dirent *directory;
  dir = opendir("./folder-local");

  if(argc != 3) {
    fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
    return 1;
  }
  server = gethostbyname(argv[1]);
  if(!server) {
    fprintf(stderr, "ERROR: no such host: %s\n", argv[1]);
    return 2;
  }
  portno = atoi(argv[2]);
  
  /*{
  struct in_addr **addr_list; int i;
  printf("Official name is: %s\n", server->h_name);
  printf("    IP addresses: ");
  addr_list = (struct in_addr **)server->h_addr_list;
  for(i = 0; addr_list[i] != NULL; i++) {
    printf("%s ", inet_ntoa(*addr_list[i]));
  }
  printf("\n");
  }*/

  //socket file descriptor
  sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(sockfd < 0) syserr("can't open socket");
  printf("create socket...\n");

  //once socket is created:
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET; //IPV4
  serv_addr.sin_addr = *((struct in_addr*)server->h_addr);
  serv_addr.sin_port = htons(portno); //port

  if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    syserr("can't connect to server");
  printf("connect...\n");

  do
  {
	  printf("\nPLEASE ENTER MESSAGE: ");
	  fgets(buffer, 255, stdin);
	  n = strlen(buffer);
  
	  if(n > 0 && buffer[n-1] == '\n') //line break
		  buffer[n-1] = '\0';
	  
	  //send
	  n = send(sockfd, buffer, strlen(buffer), 0);
	  printf("user sent %s\n", buffer);

	  if(n < 0) //couldn't send
		  syserr("can't send to server");
	
	  //user calls download
	  if(buffer[0] == 'g' &&
		 buffer[1] == 'e' &&
		 buffer[2] == 't' &&
		 buffer[3] == ' ')
	  {
		  printf("User requested a download.\n");

		  //we catch the file name
		  char fileName[256];
		  memset(&fileName, 0, sizeof(fileName));
		  
		  //parse
		  int j = 0;
		  for(int i = 4; i <= strlen(buffer); i++)
		  {
			  fileName[j] = buffer[i];
			  j++;
		  }

		  //catch file size:
		  recv(sockfd, buffer, sizeof(buffer), 0);
		  fileSize = atoi(buffer);

		  //send size back as ACK:
		  send(sockfd, buffer, sizeof(buffer), 0);

		  //print file name and size:
		  printf("File: '%s' (%d bytes)\n",fileName, fileSize);
		  

		  //receive data:
		  memset(&buffer, 0, sizeof(buffer));
		  int remainingData = 0;
		  ssize_t len;
		  char path[256] = "./folder-local/";
		  strcat(path, fileName);
		  printf("path: %s", path);
		  FILE* fp;
		  fp = fopen(path, "wb");//overwrite if existing
		  							//create if not
		  remainingData = fileSize;
		  //while(((len = recv(sockfd, buffer, 256, 0)) > 0) && (remainingData > 0))
		  printf("remainingData: %d", remainingData);
		  while(remainingData != 0)
		  {
			  if(remainingData < 256)
			  {
				  len = recv(sockfd, buffer, remainingData, 0);
				  fwrite(buffer, sizeof(char), len, fp);
				  remainingData -= len;
				  printf("Received %lu bytes, expecting %d bytes\n", len, remainingData);
				  break;
			  }
			  else
			  {
			  	len = recv(sockfd, buffer, 256, 0); //256
			  	fwrite(buffer, sizeof(char), len, fp);
		      	remainingData -= len;
			  	printf("Received %lu bytes, expecting: %d bytes\n", len, remainingData);
			  }
		  }
		  fclose(fp);
		  n = recv(sockfd, buffer, 256, 0); //receive bizarre lingering packet.

		  //clean buffer
		  memset(&buffer, 0, sizeof(buffer));
	  }
	  //user calls 'put file'
          //send file to server! (upload)
	  else if(buffer[0] == 'p' &&
	          buffer[1] == 'u' &&
		  buffer[2] == 't' &&
		  buffer[3] == ' ')
	  {
		  printf("User requested an upload\n");

                  //we wait for the server's ACK
                  n = recv(sockfd, buffer, sizeof(buffer), 0);
                  if(n < 0)
                      printf("Server didn't acknowledge name");

		  //parse the string
		  int j = 0;
		  for(int i = 4; i <= strlen(buffer); i++)
		  {
			  buffer[j] = buffer[i];
			  j++;
		  }
		  char address[256] = "./folder-local/";
		  strcat(address, buffer); //get file path

		  //open file path
                  FILE* fp;
		  fp = fopen(address, "rb"); //filename, read bytes
		  if(fp == NULL)
			  printf("error opening file in: %s\n", buffer);
		  printf("File opened successfully!\n");

		  //we will attempt to read the file
		  //in chunks of 256 bytes and send!

		  //figure out file size:
		  int file_size = 0;
		  if(fseek(fp, 0, SEEK_END) != 0)
			printf("Error determining file size\n");

		  file_size = ftell(fp);
		  rewind(fp);
		  printf("File size: %lu bytes\n", file_size);
		  
		  //pass this size to a buffer in order to send it:
                  //no need for host to network long, we're passing char array
		  memset(&fileSizeBuffer, 0, sizeof(fileSizeBuffer));
		  sprintf(fileSizeBuffer, "%d", file_size);
		  //memset(&buffer, 0, sizeof(buffer));
		  //send file size:
		  n = send(sockfd, fileSizeBuffer, sizeof(fileSizeBuffer), 0);
		  if(n < 0)
			  printf("Error sending file size information\n"); 
		  
		  //receive ACK for file size:
                  //give enough time for server to get
                  //file size we just sent
                  n = recv(sockfd, fileSizeBuffer, sizeof(fileSizeBuffer), 0);
                  if(n < 0)
                          printf("Error receiving handshake");
                  
		  //we create a byte array:
                  char byteArray[256];
                  memset(&byteArray, 0, sizeof(byteArray));
 
                  int buffRead = 0;
                  int bytesRemaining = file_size;

                  //while there are still bytes to be sent:
                  while(bytesRemaining != 0)
                  {
                       //we fill in the byte array
                       //with slabs smaller than 256 bytes:
                       if(bytesRemaining < 256)
                       {
                           buffRead = fread(byteArray, 1, bytesRemaining, fp);
                           bytesRemaining = bytesRemaining - buffRead;
                           n = send(sockfd, byteArray, 256, 0);
                           if(n < 0)
                                   printf("Error sending small slab\n");

                           printf("sent %d slab\n", buffRead);
                       }
                       //for slabs of 256 bytes:
                       else
                       {
                           buffRead = fread(byteArray, 1, 256, fp);
                           bytesRemaining = bytesRemaining - buffRead;
                           n = send(sockfd, byteArray, 256, 0);
                           if(n < 0)
                                   printf("Error sending slab\n");
                           printf("sent %d slab\n", buffRead);
                       }
                  }
                  printf("File sent!\n");
                  //clean buffers
                  memset(&buffer, 0, sizeof(buffer));
                  memset(&byteArray, 0, sizeof(byteArray));
	  }
	  //user calls ls-local
	  else if(strcmp(buffer, "ls-local") == 0)
	  {
		  memset(&buffer, 0, sizeof(buffer));
		  printf("running ls-local function:");

		  if(dir)//if directory successfully opens
		  {
		  	while((directory = readdir(dir)) != NULL)//while in dir.
			{
				if(strcmp(directory->d_name, ".") == 0 ||                                        
				   strcmp(directory->d_name, "..") == 0)                                         
				{
					//printf("\n%s", directory->d_name);
				}                                                         
				else	
					printf("\n%s", directory->d_name);
			}
			printf("\n");

		  	//rewind
		  	rewinddir(dir);
		  }
		  else
			  printf("could not open directory");

		  n = recv(sockfd, buffer, sizeof(buffer), 0);

		  if(n < 0) //couldn't receive
			  syserr("can't receive from server");

		  //clean buffer
		  memset(&buffer, 0, sizeof(buffer));
	  }
	  else if(strcmp(buffer, "ls-remote") == 0)
	  {
		 n = recv(sockfd, buffer, sizeof(buffer), 0);

		 if(n < 0) //couldn't receive
			 syserr("can't receive from server");

		 printf("running ls-remote function: %s\n", buffer);

		 //clean buffer
		 memset(&buffer, 0, sizeof(buffer));
	  }
	  //user exits
	  else if(strcmp(buffer, "exit") == 0)
	  {
		  break;
	  }
	  else//user sent a normal message
	  {	  
		  //echo (receive)
		  //n = recv(sockfd, buffer, sizeof(buffer), 0);
		  //memset(&buffer, 0, sizeof(buffer));
		  n = recv(sockfd, buffer, sizeof(buffer), 0);
  
		  if(n < 0) //couldn't receive 
			  syserr("can't receive from server"); 
		  else
			  buffer[n] = '\0';
	  	  
		  printf("Client received message: %s\n", buffer);
		  
		  //clean buffer
		  memset(&buffer, 0, sizeof(buffer));
	  }
	  //clean buffer here maybe
	  memset(&buffer, 0, sizeof(buffer));
  } while(strcmp(buffer, "exit") != 0);
  
  close(sockfd);
  return 0;
}
