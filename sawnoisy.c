#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<poll.h>
#include<time.h>

typedef enum {frame_arrival} event_type;
typedef enum {data,ack} frame_kind;
char current_frame[8],ack_frame[8];
char str[256],info[4];
event_type current_event;
char *ip="111.11.111.111";
int port=2900;
int server_sock,client_sock;
struct sockaddr_in server_addr,client_addr;
socklen_t addr_size;
int b,r,w;

int waitForEvent(event_type *event)
{
	*event=current_event;
}

void getData(int seqnum)
{
	info[0]=str[(seqnum-1)*4];
	info[1]=str[(seqnum-1)*4+1];
	info[2]=str[(seqnum-1)*4+2];
	info[3]=str[(seqnum-1)*4+3];
}

void makeFrame(frame_kind kind,int seqnum,int acknum)
{
	bzero(current_frame,8);
	current_frame[0]=(kind==data)?'D':'0';
	current_frame[1]=(char)(seqnum+48);
	current_frame[2]=(char)(acknum+48);
	current_frame[3]=info[0];
	current_frame[4]=info[1];
	current_frame[5]=info[2];
	current_frame[6]=info[3];
	current_frame[7]='0';
}

int sendFrame()
{
	if((rand()%10)+1>7)
		return 0;
	w=write(client_sock,current_frame,strlen(current_frame));
	if(w!=-1)
		return 0;
	return 1;
}

int receiveFrame()
{
	bzero(current_frame,8);
	r=read(client_sock,current_frame,sizeof(current_frame));
	if(r!=-1)
		return 0;
	return 1;
}

char extractData()
{
	info[0]=current_frame[3];
	info[1]=current_frame[4];
	info[2]=current_frame[5];
	info[3]=current_frame[6];
}

void deliverData(int seqnum)
{
	str[(seqnum-1)*4]=info[0];
	str[(seqnum-1)*4+1]=info[1];
	str[(seqnum-1)*4+2]=info[2];
	str[(seqnum-1)*4+3]=info[3];
}

int isCorrupted()
{
	if((rand()%10)+1>7)
		return 1;
	return 0;
}

int sendAck(int seqnum,int acknum)
{
	if((rand()%10)+1>8)
		return 0;
	bzero(ack_frame,8);
	ack_frame[0]='A';
	ack_frame[1]=(char)(seqnum+48);
	ack_frame[2]=(char)(acknum+48);
	int j;
	for(j=3;j<8;j++)
		ack_frame[j]='0';
	w=write(client_sock,ack_frame,strlen(ack_frame));
	if(w!=-1)
		return 0;
	return 1;
}

int getAck()
{
	bzero(ack_frame,8);
	struct pollfd mypoll={client_sock,POLLIN|POLLPRI};
	if(poll(&mypoll,1,2000))
	{
		r=read(client_sock,ack_frame,sizeof(ack_frame));
		if(r!=-1)
			return ((int)ack_frame[2])-48;
		return -1;
	}
	else
		return -2;
}

int main()
{
	printf(">>> Enter local ip : ");
	ip=malloc(20*sizeof(char));
	scanf("%s",ip);
	int mode=0,i=0;
	char ch;
	printf("[+] start.\n>>> Enter (1) to run as sender (2) to run as receiver (0) to exit : ");
	scanf("%d",&mode);
	srand(time(0));
	switch(mode)
	{
		case 1:
			printf("[+] sender side activated.\n");
			printf(">>> Enter an array of characters : ");
			fflush(stdin);
			scanf(" %[^\n]s",str);
			printf("[+] data to be sent : %s\n",str);
			
			int p;
			if(strlen(str)%4!=0)
			{
				int x=strlen(str);
				for(p=0;p<4-x%4;p++)
					str[x+p]='\0';
			}

			client_sock=socket(AF_INET,SOCK_STREAM,0);
			if(client_sock<0)
			{
				printf("[-] client socket failed.\n");
				exit(1);
			}
			printf("[+] client socket created.\n");

			memset(&client_addr,'\0',sizeof(client_addr));
			client_addr.sin_family=AF_INET;
			client_addr.sin_port=port;
			client_addr.sin_addr.s_addr=inet_addr(ip);
			
			connect(client_sock,(struct sockaddr*)&client_addr,sizeof(client_addr));
			printf("[+] connected to server.\n");

			int len=strlen(str);
			for(p=0;p<4;p++)
				str[len+p]='\0';
			int sf,g,j=-1;
			for(i=0;i<=len/4+1;i++)
			{
				if(i>j)
					printf("\n");
				getData(i+1);
				makeFrame(data,(i+1),0);
				sf=sendFrame();
				if(sf==1)
				{
					printf("[-] frame %d not sent : [ data = '",(i+1));
					int q;
					for(q=0;q<4;q++)
						if(info[q]!='\0')
							printf("%c",info[q]);
					printf("' ], resending...\n");
					j=i--;
				}
				else
				{
					printf("[+] frame %d sent : [ data = '",(i+1));
					int q;
					for(q=0;q<4;q++)
						if(info[q]!='\0')
							printf("%c",info[q]);
					printf("' ].\n");
					g=getAck();
					if(g==-1)
					{
						printf("[-] failure : ack not received, resending...\n");
						j=i--;
					}
					else if(g==-2)
					{
						printf("[-] TIMEOUT : ack not received, resending...\n");
						j=i--;
					}
					else if(g==1)
					{
						printf("[-] NACK  %d received, resending...\n",((int)ack_frame[1]-48));
						j=i--;
					}
					else
					{
						if((int)ack_frame[1]==0)
							printf("[+] ACK   (end) received.\n");
						else
							printf("[+] ACK   %d received.\n",((int)ack_frame[1]-48));
						j=i;
					}
				}
			}

			close(client_sock);
			printf("\n[+] disconnected from server.\n");
		break;
		case 2:
			printf("[+] receiver side activated.\n");
			
			server_sock=socket(AF_INET,SOCK_STREAM,0);
			if(server_sock<0)
			{
				printf("[-] server socket failed.\n");
				exit(1);
			}
			printf("[+] server socket created.\n");
			
			memset(&server_addr,'\0',sizeof(server_addr));
			server_addr.sin_family=AF_INET;
			server_addr.sin_port=port;
			server_addr.sin_addr.s_addr=inet_addr(ip);
			
			b=bind(server_sock,(struct sockaddr*)&server_addr,sizeof(server_addr));
			if(b<0)
			{
				printf("[-] bind to port failed.\n");
				exit(1);
			}
			printf("[+] bind to port successful.\n");

			listen(server_sock,5);
			printf("[+] listening for connection...\n");

			addr_size=sizeof(client_addr);
			client_sock=accept(server_sock,(struct sockaddr*)&client_addr,&addr_size);
			printf("[+] client connected.\n");

			int rf,s,k=-1;
			do
			{
				if(i>k)
					printf("\n");
				event_type event;
				waitForEvent(&event);
				rf=receiveFrame();
				k=i;
				if(rf==1)
				{
					printf("[-] frame %d not received, waiting...\n",(i+1));
					continue;
				}
				ch='*';
				if(isCorrupted()==0)
				{
					extractData();
					if(((int)current_frame[1])-48==i+1)
					{
						printf("[+] frame %d received : [ data = '",(i+1));
						int q;
						for(q=0;q<4;q++)
							if(info[q]!='\0')
								printf("%c",info[q]);
						printf("' ].\n");
						ch=info[0];
						deliverData(i+1);
						i++;
					}
					else
						printf("[+] frame %d received again, discarded.\n",(i+1));

					s=sendAck(i+1,0);
					if(s==0)
						printf("[+] frame %d not corrupted, ACK %d sent, waiting...\n",i,(i+1));
					else
						printf("[-] frame %d not corrupted, ACK %d not sent, waiting...\n",i,(i+1));
				}
				else
				{
					s=sendAck(i+1,1);
					if(s==0)
						printf("[-] frame %d corrupted, NACK %d sent, waiting...\n",i+1,i+1);
					else
						printf("[-] frame %d corrupted, NACK %d not sent, waiting...\n",i+1,i+1);
				}
			}
			while(ch!='\0');

			close(client_sock);
			printf("\n[+] client disconnected.\n");
			close(server_sock);
			printf("[+] server disconnected.\n");
			int y;
			for(y=0;y<strlen(str);y++)
				if(str[y]=='\0')
					break;
			str[y]='\0';
			printf("[+] data received : %s\n",str);
		break;
		case 0:
		break;
		default:
			printf("[-] invalid choice.\n");
	}
	printf("[+] exit.\n");
	return 0;
}
