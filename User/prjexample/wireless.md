uint8_t App_Ble_Init(void)
{
    uint8_t advData[32];  // 蓝牙广播数据最大31字节
    uint8_t advLen = 0;
    char Device_Name[] = "PRIMEDIC-CPRSensor-";
    
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QRST\r\n",400,wRecv_Char) == wRecv_Char) {
			if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
							return 0;
					}
		}
        else {
			return 2;
		}
    osDelay(300);

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QSTASTOP\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEINIT=2\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLENAME=PRIMEDIC-CPRSensor-%s\r\n", Factory_Info.Name);
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,(char *)su8_AT_Command,400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
                        
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEGATTSSRV=fe60\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
                   
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEGATTSCHAR=fe61\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEGATTSCHAR=fe62\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEADVPARAM=150,150\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
                    
    memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    advData[advLen++] = 0;  // 长度
    advData[advLen++] = 0x09;                 // 类型：Complete Local Name
    memcpy(&advData[advLen], Device_Name, sizeof(Device_Name));  // 序列号
    advLen += sizeof(Device_Name);
    memcpy(&advData[advLen-1], Factory_Info.Name, 5);     // 名称数据
    advLen += 4;
	advData[advLen++] = '-';
	memcpy(&advData[advLen], &Factory_Info.Device_SN[9], 5);
	advLen += 4;
    advData[0] = advLen-1;
    snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command),"AT+QBLEADVDATA=%02X", advData[0]);
    for (int i = 1; i < advLen; i++) {
        char temp[5] = {0};
        snprintf(temp, sizeof(temp), "%02X", advData[i]);
        strcat((char *)su8_AT_Command, temp);
    }                      
    strcat((char *)su8_AT_Command, "\r\n");
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,(char *)su8_AT_Command,400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEADDR?\r\n",400,wRecv_Puls) == wRecv_Puls) {
		Ring_WireLessComm.RevData[9] = 0;
		if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLEADDR") != 0) {
            return 0;
        }
        for(int i = 0; i < 6; i++) {
            MAC_ADRESS[i] = hexCharToValue(Ring_WireLessComm.RevData[10+3*i])<<4;
            MAC_ADRESS[i] += hexCharToValue(Ring_WireLessComm.RevData[11+3*i]);
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QVERSION\r\n",400,wRecv_Puls) == wRecv_Puls) {
        Ring_WireLessComm.RevData[9] = 0;
		if(strcmp((const char*)Ring_WireLessComm.RevData, "+QVERSION") != 0) {
            return 0;
        }
        memcpy(BLE_VER, &Ring_WireLessComm.RevData[10], 33);
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEADVSTART\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
    return 1;
}