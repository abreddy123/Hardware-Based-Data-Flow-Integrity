// only inputing "ab 2" will be correctly authorized. input more than 4 bytes will overflow, and illegally authorize.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

struct st{
	char data[4];
	int auth;
};


int main(){
	
	int a = 8738;
	struct st vulnst;
	int len=5;
	int i;
	int temp;
   	char input_data[] = {'a','b','c','d','e'};
	
	for(i=0;i<4;i++)
	vulnst.data[i]=0;
	
	printf("%d\n",len);
	printf("%x,%x\n",(unsigned int)vulnst.data,(unsigned int)&vulnst.auth);
	
	vulnst.auth=0;
	for(i=0;i<len;i++){
		vulnst.data[i]=input_data[i];
	}
	if(strcmp(vulnst.data,"ab")==0)
	vulnst.auth=1;
	if(vulnst.auth){
		printf("autherized\n");
		temp=0;
	}else{
		printf("not autherized\n");
		temp=1;
	}
  //      while(1){
		vulnst.auth=0;
		for(i=0;i<len;i++){
			vulnst.data[i]=input_data[i];
		}
		if(strcmp(vulnst.data,"ab")==0)
		vulnst.auth=1;
		if(vulnst.auth){
			temp=0;
		}else{
			temp=1;
		}
//	}
	//return 0;
	int k = 8738; 
}
