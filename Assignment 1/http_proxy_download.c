#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFER_SIZE 512
const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

//base64 encoder
/*-------------------------------------------------------------------------------------------------------------------------------*/
size_t b64_encoded_size(size_t inlen)
{
	size_t ret;

	ret = inlen;
	if (inlen % 3 != 0)
		ret += 3 - (inlen % 3);
	ret /= 3;
	ret *= 4;

	return ret;
}

char *b64_encode(const unsigned char *in, size_t len)
{
	char *out;
	size_t elen;
	size_t i;
	size_t j;
	size_t v;

	if (in == NULL || len == 0)
		return NULL;

	elen = b64_encoded_size(len);
	out  = malloc(elen+1);
	out[elen] = '\0';

	for (i=0, j=0; i<len; i+=3, j+=4) {
		v = in[i];
		v = i+1 < len ? v << 8 | in[i+1] : v << 8;
		v = i+2 < len ? v << 8 | in[i+2] : v << 8;

		out[j]   = b64chars[(v >> 18) & 0x3F];
		out[j+1] = b64chars[(v >> 12) & 0x3F];
		if (i+1 < len) {
			out[j+2] = b64chars[(v >> 6) & 0x3F];
		} else {
			out[j+2] = '=';
		}
		if (i+2 < len) {
			out[j+3] = b64chars[v & 0x3F];
		} else {
			out[j+3] = '=';
		}
	}

	return out;
}


/*-------------------------------------------------------------------------------------------------------------------------------*/
//making a socket using tcp and ipv4 to connect to server

int socket_connect(char *host, in_port_t port){
	struct hostent *hp;
	struct sockaddr_in addr;
	int on = 1, sock;     

	if((hp = gethostbyname(host)) == NULL)
	{
		herror("gethostbyname");
		exit(1);
	}
	bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	sock = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

	if(sock == -1)
	{
		perror("setsockopt");
		exit(1);
	}
	
	if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
	{
		perror("connect");
		exit(1); 

	}
	return sock;
}

/*-------------------------------------------------------------------------------------------------------------------------------*/
//function to check the redirect link present in the server response

char* check_redirect( char *reply_serv)
{
	char loc[] = "Location:";
	int compare = 0;
	char * pos = strstr(reply_serv,loc);
	if(pos!= NULL){
		pos = pos + 17;
		int itr = 0;
		while(pos[itr]!='\r')
		{
			itr++;
		}
		char *redirected_loc=malloc((itr)*sizeof(char));
		if(pos[itr -1]=='/')
			strncpy ( redirected_loc, pos, itr -1);
		else 
			strncpy ( redirected_loc, pos, itr );
		return redirected_loc;
	}
	return NULL;

}

/*-------------------------------------------------------------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	int file_desc;

	if(argc < 6)
	{
		printf("Wrong Input");
		exit(1); 
	}
	//Encoding of credentials
	char *plain;
	asprintf(&plain, "%s:%s", argv[4],argv[5]);
	int len_plain = (sizeof(argv[4])+sizeof(argv[5])+1) / sizeof(argv[4][0]);
	char *encoded = b64_encode(plain, len_plain);
	//printf("Encoded credentials: %s\n", encoded);
	//Create Socket
	file_desc = socket_connect(argv[2], atoi(argv[3]));
	char *Header_req;
	asprintf(&Header_req, "GET http://%s/ HTTP/1.1\r\nHost: www.%s\r\nProxy-Authorization: Basic %s\r\nUser-Agent: myapp\r\nAccept: */\r\nProxy-Connection: close\r\n\r\n", argv[1],argv[1],encoded);
	//Send the request to server
	write(file_desc, Header_req, strlen(Header_req));
	//free unnecessary memory
	free(plain);
	free(Header_req);
	//declaring temp array to store server response
	char header[900000];
	int headcount=0;
	int return_count;
	char *buffer=malloc(BUFFER_SIZE);
	while((return_count=read(file_desc, buffer, BUFFER_SIZE - 1)) != 0)
	{
		int counter=0;
		while(counter<return_count)	
			header[headcount++] = buffer[counter++];
		bzero(buffer, BUFFER_SIZE);
		
	}	
	shutdown(file_desc, SHUT_RDWR); 
	close(file_desc); 
	//check for any redirects
	file_desc = socket_connect(argv[2], atoi(argv[3]));
	while(1)
	{
		char* redirect = check_redirect(header);
		if(redirect!=NULL)
		{
			bzero(header, 900000);
			Header_req=NULL;
			if(strstr(redirect, "www.")!=NULL)
				asprintf(&Header_req, "GET http://%s/ HTTP/1.1\r\nHost: %s\r\nProxy-Authorization: Basic %s\r\nUser-Agent: myapp\r\nAccept: */\r\nProxy-Connection: close\r\n\r\n", redirect,redirect,encoded);
				else
				asprintf(&Header_req, "GET http://%s/ HTTP/1.1\r\nHost: www.%s\r\nProxy-Authorization: Basic %s\r\nUser-Agent: myapp\r\nAccept: */\r\nProxy-Connection: close\r\n\r\n", redirect,redirect,encoded);
					write(file_desc, Header_req, strlen(Header_req));
				headcount=0;
				bzero(buffer, BUFFER_SIZE);
				return_count=read(file_desc, buffer, BUFFER_SIZE - 1);
				while((return_count) != 0)
				{
					int count22=0;
					while(count22<return_count)
					{	
						header[headcount] = buffer[count22];
						headcount++;
						count22++;

					}
					bzero(buffer, BUFFER_SIZE);	
					return_count=read(file_desc, buffer, BUFFER_SIZE - 1);	
				}

			}
			else
				break;
		}

		// write to the html file

		FILE *ptrIndex = fopen(argv[6], "w");
		int char_index=0;
		char *search_html;

		//search for the end of the response header

		search_html = strstr(header, "\r\n\r\n");
		char_index = search_html - header + 4;

		while(1)
		{
			if(char_index>=headcount)
				break;
			else
			{
				fputc(header[char_index++], ptrIndex);
			}
		}
		fclose(ptrIndex);
		

		// Downloading image


		char *tag;
		tag = strstr(header, "IMG");

		//extracting image name

		if(tag!=NULL)
		{
			int tag2 = tag-header+9;
			char img_loc[10] = "";
			int index=0;
			int flag=1;
			while(flag==1)
			{
				if(header[tag2]!='"')
				{
					while(1)
					{
						if(header[tag2]=='"')
							break;
						img_loc[index++] = header[tag2++];
						flag=0;

					}

				}
			}

			//requesting server for image

			bzero(buffer, BUFFER_SIZE);
			char img_resp[40000];
			memset(img_resp, '0',sizeof(img_resp));
			Header_req = NULL;
			file_desc = socket_connect(argv[2], atoi(argv[3]));
			int size2 = asprintf(&Header_req, "GET http://%s/%s HTTP/1.1\r\nHost: www.%s\r\nProxy-Authorization: Basic %s\r\nUser-Agent: myapp\r\nAccept: */\r\nProxy-Connection: close\r\n\r\n", argv[1],img_loc,argv[1],encoded);
			write(file_desc, Header_req, strlen(Header_req));
	 		//reading server response
			bzero(buffer, BUFFER_SIZE);
			FILE *ptrImg = fopen( argv[7], "wb");
			int count=0;
			int sum=0;
			int n;
			while ( (n = read(file_desc, buffer, sizeof(buffer)-1)) != 0)
			{
				buffer[n] = 0;
				int count2=0;
				while(count2 < n)
				{
					img_resp[count++] = buffer[count2++];
				}
				bzero(buffer, BUFFER_SIZE);
			}

			// extracting image data

			char *search1;
			char *search2;
			search1 = strstr(img_resp, "Content-Length:");
			search2 = strstr(img_resp, "Content-Type:");
			char *head_end = strstr(img_resp, "\r\n\r\n");
			int indi=16;
			char intstr[10];
			int point=0;
			while((search1 - img_resp + indi)<(search2 - img_resp-2)){

				intstr[point++]=img_resp[search1 - img_resp + indi++];
			}
			int x; 
			int written = head_end-img_resp+4;
			sscanf(intstr, "%d", &x); 
			int counter = 0;
			//witing to logo.gif
			while(written<(x+418)){

				fputc(img_resp[written++], ptrImg);

			}
			fclose(ptrImg);

			free(Header_req);
		}
		shutdown(file_desc, SHUT_RDWR); 
		close(file_desc); 
		return 0;
	}

/*-------------------------------------------------------------------------------------------------------------------------------*//*-------------------------------------------------------------------------------------------------------------------------------*//*-------------------------------------------------------------------------------------------------------------------------------*/
	/*-------------------------------------------------------------------------------------------------------------------------------*//*-------------------------------------------------------------------------------------------------------------------------------*//*-------------------------------------------------------------------------------------------------------------------------------*/
	/*-------------------------------------------------------------------------------------------------------------------------------*//*-------------------------------------------------------------------------------------------------------------------------------*//*-------------------------------------------------------------------------------------------------------------------------------*/
	