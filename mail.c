//
// Christian Rivera
// Email Delivery Client
//
// Useage: ./mail file1.txt file2.txt ...
//
// Files must be formatted according to SMTP.  See example.txt.
// This program relies on Unix system calls.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>

#define BUF_SIZE 128
#define LINE_WIDTH 70
#define DEBUG 0

//-----------------------------------------------------------------------------
// Wrapper function for send.  Sends the entire string stored in buf.
void sendLine(int fd, char *buf) {
	int len = strlen(buf);
	int sent = 0;
	while (sent < len) {
		sent += send(fd, buf + sent, len - sent, 0);
	}
	printf("%s", buf);
	//memset(buf, '\0', BUF_SIZE);
}

//-----------------------------------------------------------------------------
// Wrapper function for recv.  Recvs an entire line and stores it in buf.
int recvLine(int fd, char *buf, int lim) {
	memset(buf, '\0', BUF_SIZE);

	int bytesRead = 0;
	while (bytesRead < lim) {
		bytesRead += recv(fd, buf + bytesRead, lim - bytesRead, 0);
		char *linebreak = strrchr(buf, '\n');		
		if (linebreak != NULL) {
			break;
		}
	}
	printf("%s", buf);

	if (strstr(buf, "220") != NULL) return 220;
	else if (strstr(buf, "250") != NULL) return 250;
	else if (strstr(buf, "354") != NULL) return 354;
	else return 0;
}

//-----------------------------------------------------------------------------
// Gets the header info from a file and stores it in the appropriate buffers.
void readFromFile(FILE *file, char* fromAddr, char* toAddr,
		  char* fromName, char* toName, char* subject) {

	char buf[BUF_SIZE];
	char *token;

	// Get from header
	fgets(buf, BUF_SIZE, file);
	token = strtok(buf, ":");
	token = strtok(NULL, "<");
	strcpy(fromName, token);
	
	token = strtok(NULL, ">");
	strcpy(fromAddr, token);

	// Get to header
	fgets(buf, BUF_SIZE, file);
	token = strtok(buf, ":");
	token = strtok(NULL, "<");
	strcpy(toName, token);
	
	token = strtok(NULL, ">");
	strcpy(toAddr, token);

	// Get subject header
	fgets(buf, BUF_SIZE, file);
	token = strtok(buf, ":");
	token = strtok(NULL, "\n");
	strcpy(subject, token);

	return;
}

//-----------------------------------------------------------------------------
// Trim the login name from the from name.
void getLoginName(char* fromName, char* login) {
	char temp[BUF_SIZE];
	memset(temp, '\0', BUF_SIZE);
	strcpy(temp, fromName);
	char *p;
	for (p = temp; *p != '\0'; p++) {
		if (isspace(*p) == 0) break;
	}
	p = strtok(p, " ");
	if (p != NULL) {
		strcpy(login, p);
	}
	else {
		strcpy(login, "world");
	}
	
	return;
}

//-----------------------------------------------------------------------------
// Trim the domain from the to address.
int getDomain(char* toAddr, char* domain) {
	if (strstr(toAddr, "@") == NULL) {
		return 0;
	}
	char temp[BUF_SIZE];
	memset(temp, '\0', BUF_SIZE);
	strcpy(temp, toAddr);
	char *p;
	for (p = temp; *p != '@'; p++);
	p++;
	strcpy(domain, p);

	return 1;
}

//-----------------------------------------------------------------------------
// Trims the mail server's name from the string returned by "host" command.
char* getServerName(char *buf) {
	if (strstr(buf, "not found") != NULL) {
		return NULL;
	}

	char *tok, *serverName;
	tok = strtok(buf, " ");

	while (tok != NULL) {
		serverName = tok;
		tok = strtok(NULL, " ");
	}
	
	char *end = strrchr(serverName, '.');
	*end = '\0';
	return serverName;
}

//-----------------------------------------------------------------------------
// Sends header data.
int sendHeader(int fd, char* buffer, char* login, char* fromName,
	       char* toName, char* fromAddr, char* toAddr, char* subject) {
	int status;
	
	sprintf(buffer, "HELO %s\r\n", login);
	sendLine(fd, buffer);
	status = recvLine(fd, buffer, BUF_SIZE);
	if (status != 250) return 0;

	sprintf(buffer, "MAIL FROM: <%s>\r\n", fromAddr);
	sendLine(fd, buffer);
	status = recvLine(fd, buffer, BUF_SIZE);
	if (status != 250) return 0;
		
	sprintf(buffer, "RCPT TO: <%s>\r\n", toAddr);
	sendLine(fd, buffer);
	status = recvLine(fd, buffer, BUF_SIZE);	
	if (status != 250) return 0;	

	sendLine(fd, "DATA\r\n");
	status = recvLine(fd, buffer, BUF_SIZE);	
	if (status != 354) return 0;
	
	sprintf(buffer, "From:%s<%s>\r\n", fromName, fromAddr);
	sendLine(fd, buffer);
	sprintf(buffer, "To:%s<%s>\r\n", toName, toAddr);
	sendLine(fd, buffer);
	sprintf(buffer, "Subject:%s\r\n", subject);
	sendLine(fd, buffer);	

	return 1;
}

//-----------------------------------------------------------------------------
// Send the main text body of the email.
void sendMessage(int fd, FILE *file, char* buffer) {
	memset(buffer, '\0', BUF_SIZE);
	while (fgets(buffer, BUF_SIZE, file) != NULL) {
		sendLine(fd, buffer);
		memset(buffer, '\0', BUF_SIZE);
	}
	return;
}

//-----------------------------------------------------------------------------
int main(int argc, char** argv) {
	
	if (argc < 2) {
        printf("Usage: ./mail file1.txt file2.txt ...\n");
		exit(0);
	}

	int status; // For return values
	char buffer[BUF_SIZE]; // For data transfer

	// For header info
	char login[BUF_SIZE];
	char fromName[BUF_SIZE];
	char toName[BUF_SIZE];
	char fromAddr[BUF_SIZE];
	char toAddr[BUF_SIZE];
	char toDomain[BUF_SIZE];
	char subject[BUF_SIZE];
	char serverName[BUF_SIZE];
	
	int i, j;

	if (DEBUG) for (i = 0; i < argc; i++) printf("-> %s\n", argv[i]);

	for (i = 1; i < argc; i++) {	
		
		// End previous output segment
		if (i > 1) {
			for (j = 0; j < LINE_WIDTH; j++) {
				printf("=");
			}		
			printf("\n");
		}

		// Clear buffers
		memset(buffer,     '\0', BUF_SIZE);
		memset(login,      '\0', BUF_SIZE);		
		memset(fromName,   '\0', BUF_SIZE);
		memset(toName,     '\0', BUF_SIZE);
		memset(fromAddr,   '\0', BUF_SIZE);
		memset(toAddr,     '\0', BUF_SIZE);	
		memset(toDomain,   '\0', BUF_SIZE);
		memset(subject,    '\0', BUF_SIZE);
		memset(serverName, '\0', BUF_SIZE);

		// Open the file
		FILE *file = fopen(argv[i], "r");
		if (file == NULL) {
			printf("Unable to open file '%s'.\n", argv[i]);
			continue;
		}
		if (DEBUG) printf("File '%s' opened.\n", argv[i]);

		// Get initial data from the file
		readFromFile(file, fromAddr, toAddr,
			     fromName, toName, subject);
		
		getLoginName(fromName, login);
		status = getDomain(toAddr, toDomain);

		if (status == 0) {
			printf("Recipient address is ");
			printf("formatted incorrectly.\n");
			printf("** Unable to send message **\n");
			continue;
		}
		
		// Begin display of this output segment
		printf("Domain: %s:\n", toDomain);
		for (j = 0; j < LINE_WIDTH; j++) {
			printf("-");
		}
		printf("\n");

		if (DEBUG) {
			printf("fADDR:   %s\n", fromAddr);
			printf("tADDR:   %s\n", toAddr);
			printf("fNAME:   %s\n", fromName);
			printf("tNAME:   %s\n", toName);
			printf("DOMAIN:  %s\n", toDomain);
			printf("SUBJECT: %s\n", subject);
		}

		// Get mail server name
		sprintf(buffer, "host -t mx %s", toDomain);
		FILE* serverList = popen(buffer, "r");
		
		if (DEBUG) printf("Getting mail server...\n");
		fgets(buffer, BUF_SIZE, serverList);
		if (DEBUG) printf("\t>%s\n", buffer);

		if (strstr(buffer, "connection timed out") != NULL) {
			printf("Unable to retrieve mail server for ");
			printf("'%s': ", toDomain);
			printf("connection timed out.\n");
			pclose(serverList);
			continue;
		}
		else if (strstr(buffer, "not found") != NULL) {
			printf("Unable to retrieve mail server for ");
			printf("'%s': ", toDomain);
			printf("host not found.\n");
			pclose(serverList);
			continue;
		}
		char *p = getServerName(buffer);
		strcpy(serverName, p);
		pclose(serverList);

		if (DEBUG) printf("SERVER NAME: %s\n", serverName);
	
		// Get the address
		struct addrinfo *info;
		info = NULL;
		status = getaddrinfo(serverName, "smtp", NULL, &info);
		if (status != 0 || info == NULL) {
			printf("Failed to get address info.\n");
			printf("%s\n", gai_strerror(status));
			continue;
		}

		// Open a socket
		int sock = socket(info->ai_family, info->ai_socktype,
				  info->ai_protocol);	
		if (sock > -1) {
			if (DEBUG) printf("Created socket %d.\n", sock);
		}
		else {
			int err = errno;
			printf("Errno: %d", err);
			printf("Unable to create socket.\n");
			continue;
		}
	
		// Make the socket reusable
		int reuse = 1;
		status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
					&reuse, sizeof(reuse));
		if (status > -1) {
			if (DEBUG) printf("Set socket to reusable.\n");
		}
		else {
			int err = errno;
			printf("Errno: %d", err);
			printf("Unable to set socket options.\n");
			continue;
		}	

		// Connect
		status = connect(sock, info->ai_addr, info->ai_addrlen);
	
		if (status == 0) {
			if (DEBUG) printf("Connection successful.\n");
		}
		else {
			int err = errno;
			printf("errno: %d\n", err);
			printf("Could not connect.\n");
			continue;
		}
		
		// Send mail
		int fail = 0;
		status = recvLine(sock, buffer, BUF_SIZE);
		if (status != 220) fail = 1;

		if (fail == 0)
			status = sendHeader(sock, buffer, login, fromName,
					    toName, fromAddr, toAddr, subject);
		if (status == 0) fail = 1;
		
		if (fail == 0) {
			sendMessage(sock, file, buffer);
			sendLine(sock, "\r\n.\r\n");
			status = recvLine(sock, buffer, BUF_SIZE);
			if (status != 250) fail = 1;
		}

		if (fail == 1)
			printf("** Unable to send message **\n");
		
		fclose(file);

		// Close the socket
		if (close(sock) == 0) {
			if (DEBUG) printf("Socket closed.\n");	
		}
		else {
			int err = errno;
			printf("Errno: %d\n", err);
			printf("Unable to close the socket.\n");
		}
	}

	for (j = 0; j < LINE_WIDTH; j++) {
		printf("=");
	}
	printf("\n");
	
	return 0;
}


