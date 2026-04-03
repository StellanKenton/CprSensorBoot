#ifndef MD5_H
#define MD5_H
 
#include <stdio.h>
#include <stdint.h>
 
extern void MD5_String(char *input, uint8_t *result);//给字串加密
extern void MD5_Data(uint8_t *input, uint32_t length, uint8_t *result);//给数据加密
//extern void MD5_File(FIL *file, uint8_t *result);//给文件加密，注意只能在文件操作线程中使用
  
  
void MD5_To_32Upper(char *input, char *user_result32);  //字串MD5加密转为32位大写      
void MD5_To_32Lower(char *input, char *user_result32);  //字串MD5加密转为32位小写    
void MD5_To_16Upper(char *input, char *user_result16);  //字串MD5加密转为16位小写   
void MD5_To_16Lower(char *input, char *user_result16);  //字串MD5加密转为16位小写   
int MD5_Decode(const char *input32, uint8_t *result16); //把32位的MD5字符串转换为16字节的数组

//void MD5_File_To_32Upper(FIL *file, char *user_result32);  //文件MD5加密转为32位大写      
//void MD5_File_To_32Lower(FIL *file, char *user_result32);  //文件MD5加密转为32位小写    



#endif
