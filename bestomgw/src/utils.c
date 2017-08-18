#include "utils.h"
//sqlite3
#include "sqlite3.h"


//static 
void gw_debug(char *declare, unsigned char *data, unsigned int dlen)
{
	int i;
	if(declare)
	{
		printf("%s", declare);
	}

	for(i = 0; i < dlen; i++)
	{
		printf("%02X ", data[i]);
	}
	printf("*\n");
}

static uint32 hex2int(uint8 *data, uint32 dlen)
{
	uint32 value = 0;
	int i;
	for(i = 0; i < dlen; i++)
	{
		value <<= 8;
		value += *data++;
	}

	return value;
}









static void send_data_to_client(unsigned char *data, int dlen)
{
	int i;

}



#if 1
void prt_debug(char *declare, unsigned char *data, unsigned int dlen)
{
	int i;
	if(declare)
	{
		printf("%s", declare);
	}

	for(i = 0; i < dlen; i++)
	{
		printf("%02X ", data[i]);
	}
	printf("*\n");
}

char * strtrimr(char * buf)
{
 int len,i;
 char * tmp = NULL;
 len = strlen(buf);
 tmp = (char*)malloc(len);
 
 memset(tmp,0x00,len);
 for(i = 0;i < len;i++)
 {
  if (buf[i] !=' ')
   break;
 }
 if (i < len) {
  strncpy(tmp,(buf+i),(len-i));
 }
 strncpy(buf,tmp,len);
 free(tmp);
 return buf;
}

char * strtriml(char * buf)
{
 int len,i; 
 char * tmp=NULL;
 len = strlen(buf);
 tmp = (char*)malloc(len);
 memset(tmp,0x00,len);
 for(i = 0;i < len;i++)
 {
  if (buf[len-i-1] !=' ')
   break;
 }
 if (i < len) {
  strncpy(tmp,buf,len-i);
 }
 strncpy(buf,tmp,len);
 free(tmp);
 return buf;
}

int  SplitKeyValue(char *buf, char **key, char **val)
{
 int  i, k1, k2, n;
 
 if((n = strlen((char *)buf)) < 1) return 0;
 for(i = 0; i < n; i++)
  if(buf[i] != ' ' && buf[i] != '\t') break;
  if(i >= n) return 0;
  if(buf[i] == '=') return -1;
  k1 = i;
  for(i++; i < n; i++)
   if(buf[i] == '=') break;
   if(i >= n) return -2;
   k2 = i;
   for(i++; i < n; i++)
    if(buf[i] != ' ' && buf[i] != '\t') break;
    buf[k2] = '\0';
    *key = buf + k1;
    *val = buf + i;
    return 1;
} 

int  FileGetLine(FILE *fp, char *buffer, int maxlen)
{
 int  i, j;
 char ch1;
 
 for(i = 0, j = 0; i < maxlen; j++)
 {
  if(fread(&ch1, sizeof(char), 1, fp) != 1)
  {
   if(feof(fp) != 0)
   {
    if(j == 0) return -1;               /* 文件结束 */
    else break;
   }
   if(ferror(fp) != 0) return -2;        /* 读文件出错 */
   return -2;
  }
  else
  {
   if(ch1 == '\n' || ch1 == 0x00) break; /* 换行 */
   if(ch1 == '\f' || ch1 == 0x1A)        /* '\f':换页符也算有效字符 */
   {
    buffer[i++] = ch1;
    break;
   }
   if(ch1 != '\r') buffer[i++] = ch1;    /* 忽略回车符 */
  }
 }
 buffer[i] = '\0';
 return i;
} 

int  ConfigGetKey(void *CFG_file, void *section, void *key, void *buf)
{
 FILE *fp;
 char buf1[MAX_CFG_BUF + 1], buf2[MAX_CFG_BUF + 1];
 char *key_ptr, *val_ptr;
 int  line_no, n, ret;
 
 line_no = 0;
 int CFG_section_line_no = 0;
 int CFG_key_line_no = 0;
 int CFG_key_lines = 0;
 
 char CFG_ssl = '[', CFG_ssr = ']';  /* 项标志符Section Symbol
--可根据特殊需要进行定义更改，如 { }等*/
char CFG_nis = ':';                 /*name 与 index 之间的分隔符 */
char CFG_nts = '#';                 /*注释符*/

 
 if((fp = fopen((char *)CFG_file, "rb")) == NULL) return CFG_ERR_OPEN_FILE;
 
 while(1)                                       /* 搜找项section */
 {
  ret = CFG_ERR_READ_FILE;
  n = FileGetLine(fp, buf1, MAX_CFG_BUF);
  if(n < -1) goto r_cfg_end;
  ret = CFG_SECTION_NOT_FOUND;
  if(n < 0) goto r_cfg_end;                    /* 文件尾，未发现 */
  line_no++;
  n = strlen(strtriml(strtrimr(buf1)));
  if(n == 0 || buf1[0] == CFG_nts) continue;       /* 空行 或 注释行 */
  ret = CFG_ERR_FILE_FORMAT;
  if(n > 2 && ((buf1[0] == CFG_ssl && buf1[n-1] != CFG_ssr)))
   goto r_cfg_end;
  if(buf1[0] == CFG_ssl)
  {
   buf1[n-1] = 0x00;
   if(strcmp(buf1+1, section) == 0)
    break;                                   /* 找到项section */
  }
 }
 CFG_section_line_no = line_no;
 while(1)                                       /* 搜找key */
 {
  ret = CFG_ERR_READ_FILE;
  n = FileGetLine(fp, buf1, MAX_CFG_BUF);
  if(n < -1) goto r_cfg_end;
  ret = CFG_KEY_NOT_FOUND;
  if(n < 0) goto r_cfg_end;                    /* 文件尾，未发现key */
  line_no++;
  CFG_key_line_no = line_no;
  CFG_key_lines = 1;
  n = strlen(strtriml(strtrimr(buf1)));
  if(n == 0 || buf1[0] == CFG_nts) continue;       /* 空行 或 注释行 */
  ret = CFG_KEY_NOT_FOUND;
  if(buf1[0] == CFG_ssl) goto r_cfg_end;
  if(buf1[n-1] == '+')                         /* 遇+号表示下一行继续  */
  {
   buf1[n-1] = 0x00;
   while(1)
   {
    ret = CFG_ERR_READ_FILE;
    n = FileGetLine(fp, buf2, MAX_CFG_BUF);
    if(n < -1) goto r_cfg_end;
    if(n < 0) break;                         /* 文件结束 */
    line_no++;
    CFG_key_lines++;
    n = strlen(strtrimr(buf2));
    ret = CFG_ERR_EXCEED_BUF_SIZE;
    if(n > 0 && buf2[n-1] == '+')            /* 遇+号表示下一行继续 */
    {
     buf2[n-1] = 0x00;
     if(strlen(buf1) + strlen(buf2) > MAX_CFG_BUF)
      goto r_cfg_end;
     strcat(buf1, buf2);
     continue;
    }
    if(strlen(buf1) + strlen(buf2) > MAX_CFG_BUF)
     goto r_cfg_end;
    strcat(buf1, buf2);
    break;
   }
  }
  ret = CFG_ERR_FILE_FORMAT;
  if(SplitKeyValue(buf1, &key_ptr, &val_ptr) != 1)
   goto r_cfg_end;
  strtriml(strtrimr(key_ptr));
  if(strcmp(key_ptr, key) != 0)
   continue;                                  /* 和key值不匹配 */
  strcpy(buf, val_ptr);
  break;
 }
 ret = CFG_OK;
r_cfg_end:
 if(fp != NULL) fclose(fp);
 return ret;
} 
#endif

size_t GetCurrentExcutableFilePathName( char* processdir,char* processname, size_t len)  
{  
	char* path_end;  
	if(readlink("/proc/self/exe", processdir,len) <=0)  
			return -1;  
	path_end = strrchr(processdir,  '/');  
	if(path_end == NULL)  
			return -1;  
	++path_end;  
	strcpy(processname, path_end);  
	*path_end = '\0';  
	return (size_t)(path_end - processdir);  
}  

// 十进制转 二进制 字符串
char *myitoa(int num,char *str,int radix)
//num是输入数字，str是存放对应的字符串，radix是进制
{
    //char *temp=new char[10];
	char temp[10];
    int a,i=0,j=0;
    if(num<0) 
    {
        str[i++] = '-';
        num *= -1;
    }
    while(num>0)
    {
        a = num%radix; 
        if(a > 9)
            temp[j++] = a -10 +'A';
        else
            temp[j++] = a +'0';
        num /= radix;
    }
    while(j>0)
    {
        str[i++] = temp[--j];
    }
    str[i] = '\0';
    return str;
}

//字符串到tm到time_t的转换
////这里是完成将诸如"2011-01-01 00:00:00"格式的日期字符串转换为时间戳

unsigned int GetTimeStampByStr( const char* pDate )   
{ 

   const char* pStart = pDate; 
	 
   char szYear[5], szMonth[3], szDay[3], szHour[3], szMin[3], szSec[3]; 
	 
   szYear[0]   = *pDate++; 
   szYear[1]   = *pDate++; 
   szYear[2]   = *pDate++; 
   szYear[3]   = *pDate++; 
   szYear[4]   = 0x0; 

   ++pDate; 
	 
   szMonth[0]  = *pDate++; 
   szMonth[1]  = *pDate++; 
   szMonth[2]  = 0x0; 
 	 
   ++pDate; 
	 
   szDay[0]    = *pDate++;   
   szDay[1]    = *pDate++; 
   szDay[2]    = 0x0; 
		 
   ++pDate; 
	 
   szHour[0]   = *pDate++; 
   szHour[1]   = *pDate++; 
   szHour[2]   = 0x0; 
 
   ++pDate; 
	 
   szMin[0]    = *pDate++; 
   szMin[1]    = *pDate++; 
   szMin[2]    = 0x0; 
		 
   ++pDate; 
	 
   szSec[0]    = *pDate++; 
   szSec[1]    = *pDate++; 
   szSec[2]    = 0x0; 
 	 
   struct tm tmObj; 
	
   tmObj.tm_year = atoi(szYear)-1900; 
   tmObj.tm_mon  = atoi(szMonth)-1; 
   tmObj.tm_mday = atoi(szDay); 
   tmObj.tm_hour = atoi(szHour); 
   tmObj.tm_min  = atoi(szMin); 
   tmObj.tm_sec  = atoi(szSec); 
   tmObj.tm_isdst= -1; 
	
   return mktime(&tmObj); 
 
} 











//十六进制字符串转换为字节流  
void HexStrToByte(const char* source, unsigned char* dest, int sourceLen)
{  
    short i;  
    unsigned char highByte, lowByte;  
      
    for (i = 0; i < sourceLen; i += 2)  
    {  
        highByte = toupper(source[i]);  
        lowByte  = toupper(source[i + 1]);  
  
        if (highByte > 0x39)  
            highByte -= 0x37;  
        else  
            highByte -= 0x30;  
  
        if (lowByte > 0x39)  
            lowByte -= 0x37;  
        else  
            lowByte -= 0x30;  
  
        dest[i / 2] = (highByte << 4) | lowByte;  
    }  
    return ;  
}  

//16 进制字符转 数字
int htoi(char s[]) 
{ 
    int i; 
    int n = 0; 
    if (s[0] == '0' && (s[1]=='x' || s[1]=='X')) //判断是否有前导0x或者0X
    { 
        i = 2; 
    } 
    else 
    { 
        i = 0; 
    } 
    for (; (s[i] >= '0' && s[i] <= '9') 
|| (s[i] >= 'a' && s[i] <= 'z') || (s[i] >='A' && s[i] <= 'Z');++i) 
    {   
        if (tolower(s[i]) > '9') 
        { 
            n = 16 * n + (10 + tolower(s[i]) - 'a'); 
        } 
        else 
        { 
            n = 16 * n + (tolower(s[i]) - '0'); 
        } 
    } 
    return n; 
} 
void hex2dec(const char* hex_str, char* dec_str){
	   int a ;
	   sscanf(hex_str, "%x", &a);
	   sprintf(dec_str, "%d", a);
	   return;
}
/*将str1字符串中第一次出现的str2字符串替换成str3*/  
void replaceFirst(char *str1,char *str2,char *str3)  
{  
    char str4[strlen(str1)+1];  
    char *p;  
    strcpy(str4,str1);  
    if((p=strstr(str1,str2))!=NULL)/*p指向str2在str1中第一次出现的位置*/  
    {  
        while(str1!=p&&str1!=NULL)/*将str1指针移动到p的位置*/  
        {  
            str1++;  
        }  
        str1[0]='\0';/*将str1指针指向的值变成/0,以此来截断str1,舍弃str2及以后的内容，只保留str2以前的内容*/  
        strcat(str1,str3);/*在str1后拼接上str3,组成新str1*/  
        strcat(str1,strstr(str4,str2)+strlen(str2));/*strstr(str4,str2)是指向str2及以后的内容(包括str2),strstr(str4,str2)+strlen(str2)就是将指针向前移动strlen(str2)位，跳过str2*/  
    }  
}  

/*将str1出现的所有的str2都替换为str3*/  
void replace(char *str1,char *str2,char *str3)  
{  
    while(strstr(str1,str2)!=NULL)  
    {  
        replaceFirst(str1,str2,str3);  
    }  
}  

#if 1
void substring(char *dest,char *src,int start,int end)  
{  
    int i=start;  
    if(start>strlen(src))return;  
    if(end>strlen(src))  
        end=strlen(src);  
    while(i<end)  
    {     
        dest[i-start]=src[i];  
        i++;  
    }  
    dest[i-start]='\0';  
    return;  
} 
#endif 

char* substr( const char *src, unsigned int start, unsigned int end) {
 
    unsigned int length = end - start;
    unsigned char * retval = (char*) malloc(length);
    memset(retval, 0x00, sizeof(retval));
	int i=0;
	for(i=0; i<start; i++ )
	    *src++;
    strncpy(retval, src, length);
    return retval;
 
}

// get the file size.  
long getfilesize(FILE *pFile)  
{  
        // check FILE*.  
        if( pFile == NULL)  
        {  
                return -1;  
        }  
  
        // get current file pointer offset.  
        long cur_pos = ftell(pFile);  
        if(cur_pos == -1)  
        {  
                return -1;  
        }  
  
        // fseek to the position of file end.  
        if(fseek(pFile, 0L, SEEK_END) == -1)  
        {  
                return -1;  
        }  
  
        // get file size.  
        long size = ftell(pFile);  
  
        // fseek back to previous pos.  
        if(fseek(pFile, cur_pos, SEEK_SET) == -1)  
        {  
                return -1;  
        }  
  
        // deal returns.  
        return size;  
}  

//写发送数据到数据库 
void WriteSendDB(sqlite3 *gdb, unsigned char *str,char uuid[37], int nlen, int cmdkind, int cmdsub)
{
	//WriteDB(write_buf, 12,write_buf[2],write_buf[3] ); //写到数据库中
	
//sqlite3 *gdb, char strsql[500]	
	char strsql[500];
	unsigned char sendcmd[512];
	unsigned char sshtaddr[5];
	unsigned char spoint[3];
	unsigned char sprofile[5];
	int i=0;

	//printf("-->%d, cmdkind=%d;cmdsub=%d  nlen=%d \n",__LINE__, cmdkind,cmdsub, nlen);
					
	sprintf(sendcmd, "%02x", str[0]);
	//printf("-->%d, sendcmd=%s  \n",__LINE__, sendcmd);	
	for(i=1;i<nlen; i++) 
	{
		sprintf(sendcmd, "%s-%02x", sendcmd,str[i]);
	//	printf("-->%d, i=%d; sendcmd=%s  \n",__LINE__,i, sendcmd);
	}

	//printf("-->%d, sendcmd=%s  \n",__LINE__, sendcmd);
	
	switch (cmdkind)
	{
		case 0x05:
		{
			
        }
		break;
	}
}

//写接收数据到数据库 
void WriteRecvDB(sqlite3 *gdb, unsigned char *str, int nlen, int cmdkind, int cmdsub)
{

	char strsql[500];
	unsigned char recvcmd[512];
	int i=0;
	
	sprintf(recvcmd, "%02x", str[0]);
	for(i=1;i<nlen; i++) 
	{
		sprintf(recvcmd, "%s-%02x", recvcmd,str[i]);
	}

	switch (cmdkind)
	{
		case 0x05:
		{

		}
		break;
	}

}

 
 //写到log
void WriteZigbeeLog(char *filname, unsigned char *str, int nlen)
{
	unsigned char buf[512];

	time_t timep; 
	FILE *fp = NULL;
	struct tm *p; 
 
	time(&timep); 
	p = localtime(&timep); 
	memset(buf,0,sizeof(buf));
	#if 1  //PRINT_DEBUG
		sprintf(buf,"%d-%d-%d %d:%d:%d : ",(1900+p->tm_year),(1+p->tm_mon),\
		p->tm_mday,p->tm_hour, p->tm_min, p->tm_sec); //星期p->tm_wday
	#endif 
	int i =0; 
	sprintf(buf, "%s %02x", buf,str[0]);
	for(i=1;i<nlen; i++) 
	{
		sprintf(buf, "%s-%02x", buf,str[i]);
	}

	//strcat(buf,str);
	//strcat(buf,"\r\n");
	strcat(buf,"\n");
 

	//fp = fopen("./syslog.log","r");
	//fp = fopen(strcat(filname, "/mnt/"),"r");
	fp = fopen(filname,"r");
	if(fp==NULL)
	{
		//fp = fopen("./syslog.log","w+");
		//fp = fopen(filname,"w+");
		fp = fopen(filname,"a");
	}
	else
	{
		fseek(fp,0,2);
		//MAXLEN = 10*1024;//10KB 
		//现在 log 是 1M
		if(ftell(fp) >= 1024*1024)
		{
			//大于10KB则 备份原日志文件
			system("cp zigbee.log zigbee1.log");
			fclose(fp);
			//fp = fopen("./syslog.log","w+");
			fp = fopen(filname,"w+");
			//大于10MB则清空原日志文件			
		}
		else
		{
			fclose(fp);
			//fp = fopen("./syslog.log","a");
			fp = fopen(filname,"a");
		}
	}

	fwrite(buf,1,strlen(buf),fp);
	fflush(fp);
	fsync(fileno(fp));
	fclose(fp);

}


void WriteLog(char *filname, char *str)
{
	char buf[512];

	time_t timep; 
	FILE *fp = NULL;
	struct tm *p; 
 
	time(&timep); 
	p = localtime(&timep); 
	memset(buf,0,sizeof(buf));
	#if 0  //PRINT_DEBUG
		sprintf(buf,"%d-%d-%d %d:%d:%d : ",(1900+p->tm_year),(1+p->tm_mon),\
		p->tm_mday,p->tm_hour, p->tm_min, p->tm_sec); //星期p->tm_wday
	#endif 

	strcat(buf,str);
	//strcat(buf,"\r\n");
	strcat(buf,"\n");
 

	//fp = fopen("./syslog.log","r");
	//fp = fopen(strcat(filname, "/mnt/"),"r");
	fp = fopen(filname,"r");
	if(fp==NULL)
	{
		//fp = fopen("./syslog.log","w+");
		//fp = fopen(filname,"w+");
		fp = fopen(filname,"a");
	}
	else
	{
		fseek(fp,0,2);
		//MAXLEN =10*1024 =10K
		if(ftell(fp) >= 10240)
		{
			fclose(fp);
			//fp = fopen("./syslog.log","w+");
			fp = fopen(filname,"w+");
			//大于10MB则清空原日志文件
		}
		else
		{
			fclose(fp);
			//fp = fopen("./syslog.log","a");
			fp = fopen(filname,"a");
		}
	}

	fwrite(buf,1,strlen(buf),fp);
	fflush(fp);
	fsync(fileno(fp));
	fclose(fp);

}
 
 int ExecSql(sqlite3 *gdb, char strsql[500])
 {
	//char strsql[500];

	int rc;	
	char *zErrMsg = 0;	

	sqlite3_exec(gdb,"begin transaction",0,0,&zErrMsg);
	rc= sqlite3_exec(gdb ,strsql,0, 0,&zErrMsg);
	
	//rc= sqlite3_exec(gdb ,CW2A(strsql, CP_UTF8),0, 0,&zErrMsg);
	sqlite3_exec(gdb,"commit transaction",0,0,&zErrMsg);

	 return 0;
 }
 
//char **azResult; //二维数组存放结果
//			sqlite3_get_table( db , strsql , &azResult , &nrow , &ncolumn , &zErrMsg );
int getSqlResult(sqlite3 *db, char strsql[500], char **azResult, int nrow, int ncolumn)
{
	char *zErrMsg = 0;

				
	sqlite3_get_table( db , strsql , &azResult , &nrow , &ncolumn , &zErrMsg );

		
	return 0;
}


 
 //从INI文件读取字符串类型数据
char *GetIniKeyString(char *title,char *key,char *filename) 
{ 
    FILE *fp; 
    char szLine[1024];
    static char tmpstr[1024];
    int rtnval;
    int i = 0; 
    int flag = 0; 
    char *tmp;
 
    if((fp = fopen(filename, "r")) == NULL) 
    { 
        printf("have   no   such   file \n");
        return ""; 
    }
    while(!feof(fp)) 
    { 
        rtnval = fgetc(fp); 
        if(rtnval == EOF) 
        { 
            break; 
        } 
        else
        { 
            szLine[i++] = rtnval; 
        } 
        if(rtnval == '\n') 
        { 
#ifndef WIN32
            i--;
#endif  
            szLine[--i] = '\0';
            i = 0; 
            tmp = strchr(szLine, '='); 
 
            if(( tmp != NULL )&&(flag == 1)) 
            { 
                if(strstr(szLine,key)!=NULL) 
                { 
                    //注释行
                    if ('#' == szLine[0])
                    {
                    }
                    else if ( '\/' == szLine[0] && '\/' == szLine[1] )
                    {
                         
                    }
                    else
                    {
                        //找打key对应变量
                        strcpy(tmpstr,tmp+1); 
                        fclose(fp);
                        return tmpstr; 
                    }
                } 
            }
            else
            { 
                strcpy(tmpstr,"["); 
                strcat(tmpstr,title); 
                strcat(tmpstr,"]");
                if( strncmp(tmpstr,szLine,strlen(tmpstr)) == 0 ) 
                {
                    //找到title
                    flag = 1; 
                }
            }
        }
    }
    fclose(fp); 
    return ""; 
}
 
//从INI文件读取整类型数据
int GetIniKeyInt(char *title,char *key,char *filename)
{
    return atoi(GetIniKeyString(title,key,filename));
}
 
//字符串转换成16进制  
char *CharArrayToHexString(char* pOut, const int nMaxLen, const char* pInput, const int nInLen)  
{  
    const char* chHexList = "0123456789ABCDEF";  
    int nIndex = 0;  
    int i=0, j=0;  
    for (i=0, j=0;i<nInLen;i++, j+=2)  
    {  
        nIndex = (pInput[i] & 0xf);  
        pOut[i*2+1] = chHexList[nIndex];  
        nIndex = ((pInput[i]>>4) & 0xf);  
        pOut[i*2] = chHexList[nIndex];  
    }  
    return pOut;  
}  

uint8 crc_generate(uint8 *buff, uint8 len)
{
	uint8 crc=0,crcbuff;
	uint8 i,j;
	
	crc=0;
	for(j =0;j<len;j++) 
	{ 
		crcbuff=*(buff+j); 
		for(i=0;i<8;i++)  
		{  
			if(((crc^crcbuff)&0x01)==0)	
				crc >>=1;  
			else 
			{  
				crc ^= 0x07;	//CRC=X8+X5+X4+1 
				crc >>= 1;  
				crc |= 0x80;  
			}		  
			crcbuff >>= 1; 	   
		} 
	}
	return crc;	
}
unsigned char Crc8Gen_smbus(unsigned char *charP,unsigned char len)
{
    int i,j;
    unsigned char crc8;
    
    crc8 = 0;
    
    for(i=0;i<len;i++)
    {
        crc8 = crc8^*charP++;
        
        for(j=0;j<8;j++)
        {
            if(crc8&0x80)
                crc8 = (crc8<<1)^0x07;
                //本来应该是crc8 ^ 0x101
                //但是最高位都是1 异或必然得0 所以都吧开头的1去掉了
            else
                crc8 <<= 1;
        }
    }
    
    return crc8 & 0xff;
}


uint8 xCal_crc(uint8 *ptr,uint32 len,uint8 genPoly)
{
	//uint8 genPoly =0x8C;
	//unsigned int genPoly =0x8C;
 uint8 crc;
 uint8 i;
    crc = 0;
    while(len--)
    {
       crc ^= *ptr++;
       for(i = 0;i < 8;i++)
       {
           if(crc & 0x01)
           {
               //crc = (crc >> 1) ^ 0x8C;
			   crc = (crc >> 1) ^ genPoly;
			   
           }else crc >>= 1;
       }
    }
    return crc;
}


uint8 Calcxor( uint8 *msg_ptr, uint8 len ){
  uint8 x;
  uint8 xorResult;

  xorResult = 0;

  for ( x = 0; x < len; x++, msg_ptr++ )
    xorResult = xorResult ^ *msg_ptr;

  return ( xorResult );
}


char *random_uuid( char buf[37] )
{
    const char *c = "89ab";
    char *p = buf;
    int n;
    for( n = 0; n < 16; ++n )
    {
        int b = rand()%255;
        switch( n )
        {
            case 6:
                sprintf(p, "4%x", b%15 );
            break;
            case 8:
                sprintf(p, "%c%x", c[rand()%strlen(c)], b%15 );
            break;
            default:
                sprintf(p, "%02x", b);
            break;
        }
 
        p += 2;
        switch( n )
        {
            case 3:
            case 5:
            case 7:
            case 9:
                *p++ = '-';
                break;
        }
    }
    *p = 0;
    return buf;
}






