#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <windows.h>

HANDLE PORT;

void writebyte(unsigned char byte){
	int n;
	WriteFile(PORT,&byte,1,(void*)&n,NULL);
	if (n!=1)printf("write failed\n");
}

unsigned char readbyte(void){
	int n;
	char byte=0;
	ReadFile(PORT,&byte,1,(void*)&n,NULL);
	//if (n!=1)printf("read failure or timeout\n");
	return byte;
}

void querydevice(void){
//wait for bootloader
	char buf;
	printf("waiting for arduino\n");
	while (1){
		writebyte(0x01);
		buf=readbyte();
		if (buf==0x02)
			break;
	}
	printf("ready\n");
}

unsigned char hex2byte(unsigned char h){
	unsigned char tb1[]="0123456789abcdef";
	unsigned char tb2[]="          ABCDEF";
	for (int i=0;i<16;i++){
		if (tb1[i]==h||tb2[i]==h)
			return i;
	}
}

FILE *readfile(char *f){
	FILE *fp=fopen(f,"rb+");
	if (fp==NULL){
		char dir[MAX_PATH];
		char *ext=malloc(strlen(f)+1);
		if (ext==NULL)
			return NULL;
		ext[0]='\\';
		strcpy(ext+1,f);
		memset(dir,MAX_PATH,0);
		getcwd(dir,MAX_PATH);
		if (strlen(dir)+strlen(ext)>MAX_PATH)
			return NULL;
		strcat(dir,ext);
		free(ext);
		fp=fopen(dir,"rb+");
		if (fp==NULL)
			return NULL;
	}
	return fp;
}



int main(int argc, char **argv){
	if (argc<4){
		printf("missing arguments\n");
		exit(0);
	}
	
	PORT=CreateFileA(argv[1],GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	
	if (PORT==INVALID_HANDLE_VALUE){
		printf("failure to open port\n");
		exit(1);
	}
	
	DCB mode;
	
	memset(&mode,0,sizeof(DCB));
	mode.DCBlength=sizeof(DCB);
	
	COMMTIMEOUTS timeouts;
	
	timeouts.ReadIntervalTimeout=1;
	timeouts.ReadTotalTimeoutMultiplier=1000;
	timeouts.ReadTotalTimeoutConstant=1;
	timeouts.WriteTotalTimeoutMultiplier=1000;
	timeouts.WriteTotalTimeoutConstant=1;	
	
	GetCommState(PORT,&mode);
	
//	mode.fDsrSensitivity=FALSE;
//	mode.fRtsControl=RTS_CONTROL_TOGGLE;
//	mode.fDtrControl=DTR_CONTROL_HANDSHAKE;
//	mode.fOutX=TRUE;
	
	if (!BuildCommDCBA("baud=2400 parity=n data=8 stop=1",&mode)||
			!SetCommState(PORT,&mode)||
			!SetCommTimeouts(PORT,&timeouts)){
		printf("port failure\n");
		CloseHandle(PORT);
		exit(1);
	}
	
	printf("port success\n");
	printf("baud %d parity %d stop %d data %d\n",mode.BaudRate,mode.fParity,mode.StopBits+1,mode.ByteSize);
	
	//querydevice();
	
	FILE *fp=NULL;
	
	if (!strcmp(argv[2],"w")){
		fp=readfile(argv[3]);
		if (fp!=NULL){
			//writing to PIC
			int eof=0;
			unsigned char c;
			unsigned char data[32];
			while (!feof(fp) && !eof){
				c=fgetc(fp);
				if (c==':'){
					unsigned char ln,adr1,adr2,rd,cs;
					ln=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
					adr1=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
					adr2=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
					rd=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
					unsigned char checksum=ln+adr1+adr2+rd;
					if (rd==0x01){
						eof=1;
					}else{
						for (int i=0;i<ln;i++){
							c=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
							data[i]=c;
							checksum+=c;
						}
						checksum=~checksum+1;
						cs=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
						if (checksum~=cs){
							printf("checksum failed! terminating\n")
							break;
						}
						//now proceed with sending the data
					}
					printf("\n");
				}
			}
		}else{
			printf("invalid file\n");
		}
	}else if(!strcmp(argv[2],"r")){
		fp=readfile(argv[3]);
		if (fp!=NULL){
			//reading from PIC
			
		}else{
			printf("invalid file\n");
		}
	}else{
		printf("invalid command\n");
	}
		
	fclose(fp);	
/*
	querydevice();
	
	unsigned char buf=0x00;
	int n=0;
	int done=0;
	
	
	while (!done){
		readbyte(&buf);
		switch (buf){
			case 0x07: printf("entered programming mode\n"); break;
			case 0x05:
				unsigned char buf2[2];
				readbyte(buf2);
				readbyte(buf2+1);
				unsigned short r=0;
				r|=buf2[0];
				r|=buf2[1]<<8;
				printf("%03x ",r);
				n++;
				if (n==20){
					printf("\n");
					n=0;
				}
				break;
			case 0x03: printf("clearing memory...\n"); break;
			case 0x04: printf("\ndone\n"); done=1; break;
		}
		buf=0x00;
	}
*/		
	CloseHandle(PORT);
	return 0;
}