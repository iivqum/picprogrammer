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

void rspv(void){
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
	return 0;
}

FILE *readfile(char *f){
	FILE *fp=fopen(f,"rb+");
	if (fp==NULL){
		int ok=0;
		char dir[MAX_PATH];
		char *ext=malloc(strlen(f)+1);
		if (ext~=NULL){
			ext[0]='\\';
			strcpy(ext+1,f);
			memset(dir,MAX_PATH,0);
			getcwd(dir,MAX_PATH);
			if (strlen(dir)+strlen(ext)<=MAX_PATH){
				strcat(dir,ext);
				fp=fopen(dir,"rb+");
				if (fp~=NULL)
					ok=1;
			}
			free(ext);
		}
		if (!ok)
			printf("invalid file\n");
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
	COMMTIMEOUTS timeouts;
	
	memset(&mode,0,sizeof(DCB));
	mode.DCBlength=sizeof(DCB);
	
	timeouts.ReadIntervalTimeout=1;
	timeouts.ReadTotalTimeoutMultiplier=1000;
	timeouts.ReadTotalTimeoutConstant=1;
	timeouts.WriteTotalTimeoutMultiplier=1000;
	timeouts.WriteTotalTimeoutConstant=1;	
	
	GetCommState(PORT,&mode);
	
	if (!BuildCommDCBA("baud=2400 parity=n data=8 stop=1",&mode)||
			!SetCommState(PORT,&mode)||
			!SetCommTimeouts(PORT,&timeouts)){
		printf("port failure\n");
		CloseHandle(PORT);
		exit(1);
	}
	
	printf("port success\n");
	printf("baud %d parity %d stop %d data %d\n",mode.BaudRate,mode.fParity,mode.StopBits+1,mode.ByteSize);
	
	//rspv();
	
	FILE *fp=NULL;
	
	if (!strcmp(argv[2],"w")){
		fp=readfile(argv[3]);
		if (fp!=NULL){
//clear internal buffer		
			writebyte(0x05);			
			
			int eof=0;
			unsigned char c;
			unsigned char data[32];
			while (!feof(fp) && !eof){
				c=fgetc(fp);
				if (c==':'){
					unsigned char ln,adr1,adr2,rd,cs;
					ln=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
//upper/lower address bytes
					adr1=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
					adr2=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
					rd=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
/*
	not considering different record types
	however for PICs with larger flash memory
	it is necessary to use an extended address space
*/
					if (rd==0x01){
						eof=1;
					}else{
						unsigned char checksum=ln+adr1+adr2+rd;
						for (int i=0;i<ln;i++){
							c=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
							data[i]=c;
							checksum+=c;
						}
						checksum=~checksum+1;
						cs=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
						if (checksum!=cs){
							printf("checksum failed! terminating\n");
							break;
						}
/*
	the arduino expects a 16 bit address plus the record length
	and the data
	
	0x03 byte tells the arduino that a data record should be placed
	inside the arduino's internal buffer, this buffer is copied into the pic
	0x04 byte tells arduino to copy its internal buffer to the pic
	0x05 byte clears the internal buffer
*/				
						writebyte(0x03);
						writebyte(adr1);
						writebyte(adr2);	
						writebyte(ln>>2);
						for (int i=0;i<ln;i++)
							writebyte(data[i]);
					}
				}
			}
			writebyte(0x04);
		}
	}else if(!strcmp(argv[2],"r")){
		fp=readfile(argv[3]);
		if (fp!=NULL){
			//reading from PIC
			//todo
		}
	}else{
		printf("invalid command\n");
	}
	fclose(fp);	
	CloseHandle(PORT);
	return 0;
}