#include "encryption.h"

unsigned char aes_key[16] = {'0','0','1','1','2','2','3','3','4','4','5','5','6','6','7','7'};
unsigned char aes_IV[16] = {'q','w','e','r','t','y','u','i','o','p','g','b',51,53,55,59};

//设置加密方式、密匙
AESInfo_t aesInfo = {
	.type = AES128,
	.mode = AES_MODE_ECB,
	.key = aes_key,
  	.pIV = aes_IV,
};

/*
SubstituteBytes()
加密时：使用S盒，将待加密数据为S盒索引将加密数据替换为S盒的内容
解密时：使用逆S盒，将已加密数据为逆S盒索引将已加密数据替换为逆S盒子的内容
其实就是将数据按表替换，
例如待加密数据unsigned char data = 9;
加密数据:encryptData = SBox[data] = SBox[9] = 0x01;//注意索引从0开始
解密数据:decryptData = InvSBox[encryptData] = InvSBox[0x01] = 9;
SBox和InvSBox的关系是 data = InvSBox[SBox[data]];还跟GF(2^8) 多项式有关
*/

/*****************************************************************************
*	函数名：	RShiftWord
*	功能描述：	对一个pWord 4字节数据进行循环右移。
*	输入参数：	pWord -- 要右移的4字节数据。
*	输出参数：	pWord -- 右移后的4字节数据。
*	返回值：      无。
*****************************************************************************/
static void RShiftWord(unsigned char *pWord)
{
    unsigned char temp = pWord[0];
    pWord[0]  = pWord[1];
    pWord[1]  = pWord[2];
    pWord[2]  = pWord[3];
    pWord[3]  = temp;
}

/*****************************************************************************
*	函数名：	XorBytes
*	功能描述：	异或两组数据。
*	输入参数：	pData1 -- 要异或的第一组数据。
*			   pData2 -- 要异或的第二组数据。
*			   nCount -- 参与异或的数据长度。
*	输出参数：	pData1 -- 异或后的结果。
*	返回值：    无。
*****************************************************************************/
static void XorBytes(unsigned char *pData1, const unsigned char *pData2, unsigned char nCount)
{
    unsigned char i;
    for (i = 0; i < nCount; i++) {
        pData1[i] ^= pData2[i];
    }
}

/*****************************************************************************
*	函数名：	AddKey
*	功能描述：	把 pData数据 加上（异或）pKey密钥，数据长度为16字节。
*	输入参数：	pData	  -- 数据。
*			   pKey      -- 密钥。
*	输出参数：	pStpDataate	  -- 加上子密钥后的数据。
*	返回值：	无。
*****************************************************************************/
static void AddKey(unsigned char *pData, const unsigned char *pKey)
{
    XorBytes(pData, pKey, 4 * Nb);
}

/*****************************************************************************
*	函数名：	SubstituteBytes
*	功能描述：	通过S盒子置换数据。
*	输入参数：	pData  	-- 数据。
*			   dataCnt -- 数据长度。
*			   pBox	   -- 置换盒子，加密时使用SBox, 解密时使用InvSBox
*	输出参数：	pData	-- 置换后的状态数据。
*	返回值：	无。
*****************************************************************************/
static void SubstituteBytes(unsigned char *pData, unsigned char dataCnt, const unsigned char *pBox)
{
    unsigned char i;
    for (i = 0; i < dataCnt; i++) {
        pData[i] = pBox[pData[i]];
    }
}

/*****************************************************************************
*	函数名：	ShiftRows
*	功能描述：	把状态数据移行。
*	输入参数：	pState	-- 状态数据。
*			   bInvert	-- 是否反向移行（解密时使用）。
*	输出参数：	pState	-- 移行后的状态数据。
*	返回值：	无。
*****************************************************************************/
static void ShiftRows(unsigned char *pState, unsigned char bInvert)
{
    // 注意：状态数据以列形式存放！
    unsigned char r;	// row，   行
    unsigned char c;	// column，列
    unsigned char temp;
    unsigned char rowData[4];

    for (r = 1; r < 4; r++) {
        // 备份一行数据
        for (c = 0; c < 4; c++) {
            rowData[c] = pState[r + 4*c];
        }

        temp = bInvert ? (4 - r) : r;
        for (c = 0; c < 4; c++) {
            pState[r + 4*c] = rowData[(c + temp) % 4];
        }
    }
}
/*****************************************************************************
*	函数名：	GfMultBy02
*	功能描述：	在GF(28)域的 乘2 运算。
*	输入参数：	num	-- 乘数。
*	输出参数：	无。
*	返回值：	num乘以2的结果。
*****************************************************************************/
static unsigned char GfMultBy02(unsigned char num)
{
    if (0 == (num & 0x80)) {
        num = num << 1;
    } else {
        num = (num << 1) ^ BPOLY;
    }

    return num;
}

/*****************************************************************************
*	函数名：	MixColumns
*	功能描述：	混合各列数据。
*	输入参数：	pData	 -- 数据。
*			   bInvert	-- 是否反向混合（解密时使用）。
*	输出参数：	pData	 -- 混合列后的状态数据。
*	返回值：	无。
*****************************************************************************/
static void MixColumns(unsigned char *pData, unsigned char bInvert)
{
    unsigned char i;
    unsigned char temp;
    unsigned char a0Pa2_M4;	// 4(a0 + a2)
    unsigned char a1Pa3_M4;	// 4(a1 + a3)
    unsigned char result[4];

    for (i = 0; i < 4; i++, pData += 4) {
        temp = pData[0] ^ pData[1] ^ pData[2] ^ pData[3];
        result[0] = temp ^ pData[0] ^ GfMultBy02((unsigned char)(pData[0] ^ pData[1]));
        result[1] = temp ^ pData[1] ^ GfMultBy02((unsigned char)(pData[1] ^ pData[2]));
        result[2] = temp ^ pData[2] ^ GfMultBy02((unsigned char)(pData[2] ^ pData[3]));
        result[3] = temp ^ pData[3] ^ GfMultBy02((unsigned char)(pData[3] ^ pData[0]));

        if (bInvert) {
            a0Pa2_M4 = GfMultBy02(GfMultBy02((unsigned char)(pData[0] ^ pData[2])));
            a1Pa3_M4 = GfMultBy02(GfMultBy02((unsigned char)(pData[1] ^ pData[3])));
            temp	 = GfMultBy02((unsigned char)(a0Pa2_M4 ^ a1Pa3_M4));
            result[0] ^= temp ^ a0Pa2_M4;
            result[1] ^= temp ^ a1Pa3_M4;
            result[2] ^= temp ^ a0Pa2_M4;
            result[3] ^= temp ^ a1Pa3_M4;
        }

        memcpy(pData, result, 4);
    }
}

/*****************************************************************************
*	函数名：	BlockEncrypt
*	功能描述：	对单块数据加密。
*	输入参数：	pData -- 要加密的块数据。
*	输出参数：	pData -- 加密后的块数据。
*	返回值：	无。
*****************************************************************************/
static void BlockEncrypt(AESInfo_t *aesInfoP, unsigned char *pData)
{
    unsigned char i;

    AddKey(pData, aesInfoP->expandKey);
    for (i = 1; i <= aesInfoP->Nr; i++) {
        SubstituteBytes(pData, 4 * Nb, SBox);
        ShiftRows(pData, 0);

        if (i != aesInfoP->Nr) {
            MixColumns(pData, 0);
        }

        AddKey(pData, &aesInfoP->expandKey[4*Nb*i]);
    }
}

/*****************************************************************************
*	函数名：	BlockDecrypt
*	功能描述：	对单块数据解密。
*	输入参数：	pData -- 要解密的数据。
*	输出参数：	pData -- 解密后的数据。
*	返回值：	无。
*****************************************************************************/
static void BlockDecrypt(AESInfo_t *aesInfoP, unsigned char *pData)
{
    unsigned char i;

    AddKey(pData, &aesInfoP->expandKey[4*Nb*aesInfoP->Nr]);

    for (i = aesInfoP->Nr; i > 0; i--) {
        ShiftRows(pData, 1);
        SubstituteBytes(pData, 4 * Nb, InvSBox);
        AddKey(pData, &aesInfoP->expandKey[4*Nb*(i-1)]);

        if (1 != i) {
            MixColumns(pData, 1);
        }
    }
}

/*****************************************************************************
*	函数名：	AESAddPKCS7Padding
*	描述：		PKCS7 方式填充数据
*	输入参数：	data -- 后面最多预留16个字节空间用于存放填充值
*			   len --  数据的长度
*	输出参数：	data  -- 添加填充码后的数据
*	返回值：	填充后的长度
*****************************************************************************/
//static unsigned int AESAddPKCS7Padding(unsigned char *data, unsigned int len)
//{
//    unsigned int newLen;
//    newLen = len + 16 - (len % 16);
//    memset(&data[len], newLen-len, newLen-len);
//    return newLen;
//}

/*****************************************************************************
*	函数名：	AESDelPKCS7Padding
*	描述：		PKCS7Padding 填充密文解密后剔除填充值
*	输入参数：	pData -- 解密后的数据
*			   len --  数据的长度
*	输出参数：	pData  -- 删除填充码后的数据
*	返回值：	删除后的实际有效数据长度，为0表示传入的数据异常
*****************************************************************************/
//static unsigned int AESDelPKCS7Padding(unsigned char *pData, unsigned int len)
//{
//    if (0 != (len & 0x0F)) {//1组16字节，(0 != (len & 0x0F)说明不是16的倍数
//        return 0;
//    }
//    if (pData[len - 1] > len) {
//        return 0;
//    }

//    return len - pData[len - 1];
//}

/*****************************************************************************
*	函数名：	AESInit
*	功能描述：	初始化
*	输入参数：	aesInfoP -- 用户需要填充
*	输出参数：	无。
*	返回值：	无。
*****************************************************************************/
void AESInit(AESInfo_t *aesInfoP)
{

    unsigned char i;
    unsigned char *pExpandKey;//扩展密钥
    unsigned char Rcon[4] = {0x01, 0x00, 0x00, 0x00};

    switch (aesInfoP->type) {
        case AES128:
            aesInfoP->Nr = 10;
            aesInfoP->Nk = 4;
            break;
        case AES192:
            aesInfoP->Nr = 12;
            aesInfoP->Nk = 6;
            break;
        case AES256:
            aesInfoP->Nr = 14;
            aesInfoP->Nk = 8;
            break;
        default:
            aesInfoP->Nr = 10;
            aesInfoP->Nk = 4;
            break;
    }
    //拓展密匙
    memcpy(aesInfoP->expandKey, aesInfoP->key, 4 * aesInfoP->Nk);//第一个是原始密匙，
    pExpandKey = &aesInfoP->expandKey[4*aesInfoP->Nk]; //拓展密匙AES128:10个、AES192:12个、AES256:14个
    for (i = aesInfoP->Nk; i < Nb*(aesInfoP->Nr + 1); pExpandKey += 4, i++) {
        memcpy(pExpandKey, pExpandKey - 4, 4);

        if (0 == i % aesInfoP->Nk) {
            RShiftWord(pExpandKey);
            SubstituteBytes(pExpandKey, 4, SBox);
            XorBytes(pExpandKey, Rcon, 4);

            Rcon[0] = GfMultBy02(Rcon[0]);
        } else if (6 < aesInfoP->Nk && i % aesInfoP->Nk == Nb) {
            SubstituteBytes(pExpandKey, 4, SBox);
        }

        XorBytes(pExpandKey, pExpandKey - 4 * aesInfoP->Nk, 4);
    }
}

/*****************************************************************************
*	函数名：	AESEncrypt
*	功能描述：	加密数据
*	输入参数：  aesInfoP    -- 包含key、加密方式等初始化信息
*              pPlainText   -- 要加密的明文数据，其长度为dataLen字节。
*			    dataLen	   -- 明文数据长度，以字节为单位
*	输出参数：	pCipherText	-- 加密后的数据
*	返回值：	解密后的数据长度。
*****************************************************************************/
unsigned int AESEncrypt(AESInfo_t *aesInfoP, const unsigned char *pPlainText, unsigned char *pCipherText,
                 unsigned int dataLen)
{
    unsigned int i;
    const void *pIV;

    if (pPlainText != pCipherText) {
        memcpy(pCipherText, pPlainText, dataLen);
    }

    //必须是16的整倍数，不够的填充，pkcs7算法是缺n补n个n，比如13字节数据缺了3个，后面就补3个3;如果刚好是16的倍数，就填充16个16
    //dataLen = AESAddPKCS7Padding(pCipherText, dataLen);//注意如果是使用NOpadding方式，则此句注释掉即可，同时解密函数对应的AESDelPKCS7Padding()函数也需一同注释掉。

    pIV = aesInfoP->pIV;
    for (i = dataLen / (4 * Nb); i > 0 ; i--, pCipherText += 4 * Nb) {
        if (AES_MODE_CBC == aesInfoP->mode) {
            XorBytes(pCipherText, pIV, 4 * Nb);
        }

        BlockEncrypt(aesInfoP, pCipherText);
        pIV = pCipherText;
    }
    return dataLen;
}

/*****************************************************************************
*	函数名：	AESDecrypt
*	功能描述：	解密数据
*	输入参数：	aesInfoP    -- 包含key、加密方式等初始化信息
*	            pCipherText	-- 要解密的密文
*			    dataLen	   -- 密文数据长度，以字节为单位，必须是整倍数，AES128:16倍数、AES192:24倍数、AES256:32倍数。
*  输出参数：  pPlainText  -- 解密出来的明文
*	返回值：	返回解密后的数据长度。
*****************************************************************************/
unsigned int AESDecrypt(AESInfo_t *aesInfoP, unsigned char *pPlainText, const unsigned char *pCipherText,
                 unsigned int dataLen)
{
    unsigned int i;
    //unsigned char *pPlainTextBack = pPlainText;

    if (pPlainText != pCipherText) {
        memcpy(pPlainText, pCipherText, dataLen);
    }

    //当mode=AES_MODE_CBC时需要从最后一块数据开始解密
    pPlainText += dataLen - 4 * Nb;
    for (i = dataLen / (4 * Nb); i > 0 ; i--, pPlainText -= 4 * Nb) {
        BlockDecrypt(aesInfoP, pPlainText);
        if (AES_MODE_CBC == aesInfoP->mode) {
            if (1 == i) {//原来的第一块数据是初始变量加密的
                XorBytes(pPlainText, aesInfoP->pIV, 4 * Nb);
            } else {
                XorBytes(pPlainText, pPlainText - 4 * Nb, 4 * Nb);
            }
        }
    }

    //因为数据需要16字节对齐，可能有填充数据，需要去除后面的填充数据
    //return AESDelPKCS7Padding(pPlainTextBack, dataLen);//注意如果是使用NOpadding方式，则此句注释掉直接return datalen即可，同时加密函数对应的AESAddPKCS7Padding()函数也需一同注释掉。
    return dataLen;
}

//打印
void PrintData(const char *head, unsigned char *data, unsigned int len)
{
    unsigned int i;

    printf("%s, len:%u:\r\n", head, len);

    //按16进制打印出来
    printf("HEX:[");
    for (i=0; i<len; i++) {
        printf("%02X ", data[i]);
    }
    printf("]\r\n");

    //按ASCII码打印出来
    printf("ASCII:[");
    for (i=0; i<len; i++) {
        if (isprint(data[i])) {//可打印字符
            printf("'%c' ", data[i]);
        } else {
            printf("\\%02X ", data[i]);
        }

    }
    printf("]\r\n");
}

//初始化
void my_aes_init(void)
{
	//初始化
    AESInit(&aesInfo);
}

//加密
void my_aes_encrypt(unsigned char* sou_data, unsigned char* enc_data,unsigned char len)
{
//    unsigned char  enc_len;             //加密后的密文长度
//    enc_len = 
	AESEncrypt(&aesInfo, sou_data, enc_data, len);
    //PrintData("encryptMsg", enc_data, enc_len);
}


//解密
void my_aes_decrypt(unsigned char* enc_data, unsigned char* dec_data, unsigned char len)
{
//    unsigned char  dec_len;                //解密后的明文长度
//    dec_len = 
	AESDecrypt(&aesInfo, dec_data, enc_data, len);
    //PrintData("decryptMsg", dec_data, dec_len);
}

//测试例1
void my_aes_test(void)
{
    //要加密的内容
    unsigned char sourceMsg[8] = "hello";

    unsigned char encrypt_data[33]={0};
    unsigned char decrypt_data[33]={0};
    my_aes_init();
    //PrintData("sourceMsg", sourceMsg, 5);
    my_aes_encrypt(sourceMsg,encrypt_data, 5);
    my_aes_decrypt(encrypt_data,decrypt_data, 16);
}
