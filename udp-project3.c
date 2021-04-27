// File: $Id: udp-project3.C,v 1.5 2007/05/14 01:58:06 jeg3600 Exp jeg3600 $
// Author: John Garnham
// Description: A UDP client for transferring data
// Revisions:
// $Log: udp-project3.C,v $
// Revision 1.5  2007/05/14 01:58:06  jeg3600
// Should be final
//
// Revision 1.4  2007/05/12 22:31:34  jeg3600
// Added time outs
//
// Revision 1.3  2007/05/12 20:59:51  jeg3600
// Sends HELLO and GOODBYE packets
//
// Revision 1.2  2007/05/12 01:54:19  jeg3600
// Sets up socket. Collects user input.
//
// Revision 1.1  2007/05/12 00:43:17  jeg3600
// Initial revision
//
//

#include "datagram.h"
#include "Timer.h"
#include <stdlib.h>
#include <unistd.h>
#include <iostream.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT_MIN 1024         // The lowest possible port number 
#define PORT_MAX 65535        // The highest possible port number
#define TIME_OUT 10          // How long the time-out is (s)

typedef struct {
  int command;     // O for add, 1 for retrieve
  int id;          // sequence number
  char name[32];   // the name of the person
  int age;         // the age of the person
} data_record;

  
// hostname for the socket
char* hostname;  

// port to connect to on server
int port;        

// socket handler
int sock;

// length of what is returned
int length;

// host
struct hostent *hostent;

// server data structure
struct sockaddr_in server;

// server structure for where packet came from
struct sockaddr_in from_addr;

// the length of the address of where the packet is from
int from_addr_len = sizeof(from_addr);

// status (success/failure)
int status;

// number of tries
int tries = 0;

// whether a time out has occured or not
bool timed_out = false;

// datagram to send to server
Datagram dgram_to;

// datagram to receive from server
Datagram dgram_from;

// Sends an ACK back to the server
void send_ack();

int main(int argc, char* argv[]) {
  
  // data record to send to the server
  data_record to;

  // data record to receive from the server 
 data_record *from;

  // Client MUST accept two values: hostname and port
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " hostname port" << std::endl;
    exit(0);
  }
  
  // Grab the host name
  hostname = argv[1];

  // Get the port number, and make sure that it is legitimate
  port = atoi( argv[2] );
  if( port < PORT_MIN || port > PORT_MAX ){
    std::cerr << port << ": invalid port number" << std::endl;
    exit(0);
  }
  
  // Create UDP (SOCK_DGRAM) socket
  sock = socket(AF_INET,SOCK_DGRAM,0);
  if (sock < 0) { 
    std::cerr << "socket: " << strerror(errno) << std::endl;
    exit(0);
  }

  // DNS look-up
  hostent = gethostbyname( hostname );
  if ( hostent == NULL) {
    std::cerr << "gethostbyname: " << strerror(errno) << std::endl;
    exit(0);
  }

  // Clear up garbage in server data space
  bzero( &server, sizeof(server));

  // Set up server data structure
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = *(u_long *)hostent->h_addr;
  
  // Send a HELLO (SYN) packet
  dgram_to.seq = 0;
  dgram_to.type = SYN;

  // Send to server
  sendto(sock, (const void*) &dgram_to, sizeof(dgram_to), 0, 
	(struct sockaddr*)&server, sizeof(server));  
  
  // Receive response (ACK)
  start_timer(TIME_OUT, timed_out);

  length = recvfrom(sock, (void*)&dgram_from, sizeof(dgram_from), 0, 
		    (struct sockaddr*)&from_addr, &from_addr_len);
  stop_timer();

  while((server.sin_addr.s_addr != from_addr.sin_addr.s_addr 
	 || server.sin_port != from_addr.sin_port
	 || dgram_from.type != ACK || dgram_from.seq != 0) 
	&& timed_out && tries < 5) {

    std::cout << "Timeout on attempt # " << (tries + 1) << std::endl;
    
    timed_out = false;
    sendto(sock, (const void*) &dgram_to, sizeof(dgram_to) + 1, 0, 
	   (struct sockaddr*)&server, sizeof(server));

    do {

      start_timer(TIME_OUT, timed_out);
      length = recvfrom(sock, (char*)&dgram_from, sizeof(dgram_from), 0, 
			(struct sockaddr*)&from_addr, &from_addr_len);
      stop_timer();
      
    } while ((server.sin_addr.s_addr != from_addr.sin_addr.s_addr 
	      || server.sin_port != from_addr.sin_port
	      || dgram_from.type != ACK || dgram_from.seq != 0)
	     && timed_out);
      
    if (++tries >= 5) exit(0);
    
  }

  tries = 0;
  timed_out = false;

  do {

    // temp string for gathering input
    std::string in;
    
    std::cout << "Enter command (0 for Add, 1 for Retrieve, 2 to Quit):";    
    std::cin >> in;
    to.command = atoi(in.c_str());

    if (to.command == 0) {

      // Add a data entry
      
      // Grab the ID from the user
      std::cout << "Enter id (integer):";
      std::cin >> in;
      to.id = atoi(in.c_str());

      // Grab the name
      std::cout << "Enter name (up to 32 char):";
      std::cin >> to.name;

      // Grab the age
      std::cout << "Enter age (integer):";
      std::cin >> in;
      to.age = atoi(in.c_str());

      // Set up the UDP datagram
      dgram_to.type = DATA;
      memcpy(dgram_to.data, (void*) &to, sizeof(to));

      // Send to server
      sendto(sock, (const void*) &dgram_to, sizeof(dgram_to) + 1, 0, 
	     (struct sockaddr*)&server, sizeof(server));

      // Receive response
      start_timer(TIME_OUT, timed_out);
      length = recvfrom(sock, (char*)&dgram_from, sizeof(dgram_from), 0, 
			(struct sockaddr*)&from_addr, &from_addr_len);
      stop_timer();    
      
      while(timed_out && tries < 5) {
   
	std::cout << "Timeout on attempt # " << (tries + 1) << std::endl;
	
	timed_out = false;
	
	sendto(sock, (const void*) &dgram_to, sizeof(dgram_to) + 1, 0, 
	       (struct sockaddr*)&server, sizeof(server));

	do {
	  
	  start_timer(TIME_OUT, timed_out);
	  length = recvfrom(sock, (char*)&dgram_from, sizeof(dgram_from), 0, 
			    (struct sockaddr*)&from_addr, &from_addr_len);
	  stop_timer();
	  
	} while (! timed_out);
	
	if (++tries >= 5) exit(0);
	
      }    

      // Clear variables
      timed_out = false;
      tries = 0;

      // Send an ACKnowledgement
      //      send_ack();

      // Check if the sequence number was correct
      if (dgram_from.seq != dgram_to.seq) {
	std::cout << "Out-of-sequence datagram discarded\n" 
		  << "Expected seq# " << dgram_to.seq 
		  << ", received " << dgram_from.seq << std::endl;
	dgram_to.seq++;
	send_ack();
	dgram_to.seq--;
      } else if (server.sin_addr.s_addr != from_addr.sin_addr.s_addr 
		 || server.sin_port != from_addr.sin_port) {
	std::cout << "Datagram from wrong source discarded\n" 
		  << "Expected from " << inet_ntoa(server.sin_addr)
		  << " port " << ntohs(server.sin_port) << std::endl
		  << "Received from " << inet_ntoa(from_addr.sin_addr)
		  << " port " << ntohs(server.sin_port) << std::endl;
      } else {

	// Decode and display the result
	from = (data_record*) &dgram_from.data;
	
	if( from->command == 0) {
	  std::cout << "ID " << to.id << " added successfully" << std::endl;
	} else if (from->command == 1) {
	  std::cout << "ID " << to.id << " already exists" << std::endl;
	} else {
	  std::cout << "Error: corrupted data" << std::endl;
	}

	send_ack();
	// Increment the sequence number
	dgram_to.seq++;

      }

    } else if (to.command == 1) {

      // Retrieve a data entry

      // Grab the ID from the user
      std::cout << "Enter id (integer):";
      std::cin >> in;
      to.id = atoi(in.c_str());

      // Set up the datagram
      dgram_to.type = DATA;
      memcpy(dgram_to.data, (void*) &to, sizeof(to));

      // Send to server
      sendto(sock, (const void*) &dgram_to, sizeof(dgram_to) + 1, 0, 
	     (struct sockaddr*)&server, sizeof(server));
 
      // Receive response
      start_timer(TIME_OUT, timed_out);
      length = recvfrom(sock, (char*)&dgram_from, sizeof(dgram_from), 0, 
			  (struct sockaddr*)&from_addr, &from_addr_len);
      stop_timer();    
      
      while(timed_out && tries < 5) {
   
	std::cout << "Timeout on attempt # " << (tries + 1) << std::endl;
	
	timed_out = false;
	sendto(sock, (const void*) &dgram_to, sizeof(dgram_to) + 1, 0, 
	       (struct sockaddr*)&server, sizeof(server));

	do {
	  
	  start_timer(TIME_OUT, timed_out);
	  length = recvfrom(sock, (char*)&dgram_from, sizeof(dgram_from), 0, 
			    (struct sockaddr*)&from_addr, &from_addr_len);
	  stop_timer();
	  
	} while (! timed_out);
	
	if (++tries >= 5) exit(0);
	
      }    
      
      timed_out = false;
      tries = 0;

      // Send an ACKnowledgement
      //      send_ack();

      // Check if the sequence number was correct
      if ( ! (dgram_from.seq == dgram_to.seq)) {
	std::cout << "Out-of-sequence datagram discarded\n" 
		  << "Expected seq# " << dgram_to.seq 
		  << ", received " << dgram_from.seq << std::endl;
	dgram_to.seq++;
	send_ack();
	dgram_to.seq--;
      } else if (server.sin_addr.s_addr != from_addr.sin_addr.s_addr 
		 || server.sin_port != from_addr.sin_port) {
	std::cout << "Datagram from wrong source discarded\n" 
		  << "Expected from " << inet_ntoa(server.sin_addr)
		  << " port " << ntohs(server.sin_port) << std::endl
		  << "Received from " << inet_ntoa(from_addr.sin_addr)
		  << " port " << ntohs(server.sin_port) << std::endl;
     } else {
	
       std::cout << dgram_from.type;

	// Decode and display the data
	from = (data_record*) &dgram_from.data;
	if (from->command == 0) {
	  std::cout << "ID: " << to.id << std::endl
		    << "Name: " << from->name << std::endl
		    << "Age: " << from->age << std::endl;
	} else if (from->command == 1) {
	  std::cout << "ID " << to.id << " does not exist" << std::endl;
	} else {
	  std::cout << "Error: corrupted data" << std::endl;
	}


	send_ack();

	// Increment the sequence number
	dgram_to.seq++;
	
      }

    }

    bzero(&dgram_from, sizeof(dgram_from));
    bzero(&from, sizeof(from));

  } while(to.command != 2);

  // Send a GOODBYE (SYN) packet
  dgram_to.type = FIN;

  // Send to server
  sendto(sock, (const void*) &dgram_to, sizeof(dgram_to) + 1, 0, 
	 (struct sockaddr*)&server, sizeof(server));

  // Close the socket
  close(sock);
  
  return 0;

}

// Send an acknowledgement back to the server
void send_ack() {

  // Send ACK
  dgram_to.type = ACK;

  // Send to server
  sendto(sock, (const void*) &dgram_to, sizeof(dgram_to) + 1, 0, 
	(struct sockaddr*)&server, sizeof(server));  
  
  // Receive response 
  start_timer(TIME_OUT, timed_out);
  length = recvfrom(sock, (void*)&dgram_from, sizeof(dgram_from), 0, 
		    (struct sockaddr*)&from_addr, &from_addr_len);
  stop_timer();

  while(timed_out && tries < 5) {
   
    std::cout << "Timeout on attempt # " << (tries + 1) << std::endl;
    
    timed_out = false;
    
    sendto(sock, (const void*) &dgram_to, sizeof(dgram_to) + 1, 0, 
	   (struct sockaddr*)&server, sizeof(server));

    start_timer(TIME_OUT, timed_out);

    do {

      length = recvfrom(sock, (char*)&dgram_from, sizeof(dgram_from), 0, 
			(struct sockaddr*)&from_addr, &from_addr_len);
      
    } while (server.sin_addr.s_addr != from_addr.sin_addr.s_addr 
	     || server.sin_port != from_addr.sin_port);
      
    stop_timer();
    
    if (++tries >= 5) exit(0);
    
  }

  tries = 0;
  timed_out = false;

}
