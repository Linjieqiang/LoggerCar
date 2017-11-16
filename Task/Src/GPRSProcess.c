#include "GPRSProcess.h"
#include "gprs.h"
#include "gps.h"
#include "rt.h"
#include "MainProcess.h"

/******************************************************************************/
extern GPS_LocateTypedef  GPS_Locate;				/* 定位信息 */
extern osMessageQId gprsTaskMessageQid;
extern osThreadId mainprocessTaskHandle;
extern GPRS_SendbufferTyepdef GPRS_NewSendbuffer;
extern const char Message[];
extern uint16_t GPRS_SendPackSize;							/* GPRS发送包大小 */
extern char     ICCID[20];									/* ICCID */
extern char	 	IMSI[15];									/* IMSI */
extern char     IMEI[15];									/* IMEI */

/******************************************************************************/
static osEvent signal;
static GPRS_ModuleStatusEnum moduleStatus = SET_BAUD_RATE;			/* GPRS模块状态 */
static char* expectString;											/* 预期收到的字符串 */
static GPRS_TaskStatusEnum taskStatus = START_TASK_INVALID;
static uint8_t moduleTimeoutCnt;									/* 模块超时计数 */
static uint8_t moduleErrorCnt;										/* 模块接收错误指令计数 */
static BOOL gprsInited = FALSE;									/* gprs功能初始化标志位 */

/******************************************************************************/
void SendProcess(void);
void TimeoutProcess(void);
void RecvProcess(void);
void RecvErrorProcess(void);


/*******************************************************************************
 * @note 模块重复开机无法启动解决方案：
 * 		 开机先发送设置波特率指令，若超时则代表模块未启动，执行启动程序，如果收到指令，则复位模块，继续往下执行
 */
void GPRSPROCESS_Task(void)
{
	while(1)
	{
		/* 发送部分 */
		SendProcess();

		/* 等待接收 */
		signal = osSignalWait(GPRS_PROCESS_TASK_RECV_ENABLE, 3000);
		/* 接收超时 */
		if (signal.status == osEventTimeout)
		{
			TimeoutProcess();
		}
		/* 接收到数据 */
		else if ((signal.value.signals & GPRS_PROCESS_TASK_RECV_ENABLE)
				== GPRS_PROCESS_TASK_RECV_ENABLE)
		{
			/* 接收到任意数据，则将超时计数清空 */
			moduleTimeoutCnt = 0;

			/* 数据或者短信发送完成后收到平台的回文 */
			if ((moduleStatus == DATA_SEND_FINISH) || (moduleStatus == MESSAGE_SEND_FINISH))
			{
//				RT_TimeAdjustWithCloud(GPRS_RecvBuffer.buffer.recvBuffer);
				/* 如果是发送数据的，则标志发送完成 */
				if (moduleStatus == DATA_SEND_FINISH)
				{
					/* GPRS发送完成 */
					osSignalSet(mainprocessTaskHandle, MAINPROCESS_GPRS_SEND_FINISHED);
				}

				moduleStatus = EXTI_SERIANET_MODE;
			}
			/* 寻找预期接收的字符串是否在接收的数据中 */
			else if (NULL != strstr((char*)GPRS_RecvBuffer.buffer.recvBuffer, expectString))
			{
				RecvProcess();
			}
			/* 模块返回的指令不正确 */
			else
			{
				RecvErrorProcess();
			}
			memset(GPRS_RecvBuffer.buffer.recvBuffer, 0, GPRS_RecvBuffer.bufferSize);
		}
	}
}

/*******************************************************************************
 *
 */
void SendProcess(void)
{
	/* 发送部分 */
	switch (moduleStatus)
	{
	/* 如果模块无效，则先执行开机 */
	case MODULE_INVALID:
		/* 开机 */
		GPRS_PWR_CTRL_ENABLE();
		expectString = AT_CMD_POWER_ON_READY_RESPOND;
		moduleStatus = MODULE_VALID;
		break;

	/* 设置波特率 */
	case SET_BAUD_RATE:
		GPRS_RST_CTRL_ENABLE();
		osDelay(1);
		GPRS_RST_CTRL_DISABLE();
		osDelay(1);
		GPRS_SendCmd(AT_CMD_SET_BAUD_RATE);
		expectString = AT_CMD_SET_BAUD_RATE_RESPOND;
		moduleStatus = SET_BAUD_RATE_FINISH;
		break;

	/* 关闭回显模式 */
	case ECHO_DISABLE:
		GPRS_SendCmd(AT_CMD_ECHO_DISABLE);
		expectString = AT_CMD_ECHO_DISABLE_RESPOND;
		moduleStatus = ECHO_DISABLE_FINISH;
		break;

	/* 获取ICCID */
	case GET_ICCID:
		GPRS_SendCmd(AT_CMD_GET_ICCID);
		expectString = AT_CMD_GET_ICCID_RESPOND;
		moduleStatus = GET_ICCID_FINISH;
		break;

	/* 获取IMSI */
	case GET_IMSI:
		GPRS_SendCmd(AT_CMD_GET_IMSI);
		expectString = AT_CMD_GET_IMSI_RESPOND;
		moduleStatus = GET_IMSI_FINISH;
		break;

	/* 获取IMEI */
	case GET_IMEI:
		GPRS_SendCmd(AT_CMD_GET_IMEI);
		expectString = AT_CMD_GET_IMEI_RESPOND;
		moduleStatus = GET_IMEI_FINISH;
		break;

	/* 使能GPS功能 */
	case ENABLE_GPS:
		GPRS_SendCmd(AT_CMD_GPS_ENABLE);
		expectString = AT_CMD_GPS_ENABLE_RESPOND;
		moduleStatus = ENABLE_GPS_FINISH;
		break;

	/* 初始状态 */
	case INIT:
		signal = osMessageGet(gprsTaskMessageQid, osWaitForever);
		taskStatus = (GPRS_TaskStatusEnum)signal.value.v;
		switch (taskStatus)
		{
		/* 开启定位 */
		case START_TASK_GPS:
			moduleStatus = GET_GPS_GNRMC;
			break;

		/* 开启GPRS发送数据 */
		case START_TASK_DATA:
			/* 如果模块已经初始化完成 */
			if (gprsInited == TRUE)
				moduleStatus = GET_SIGNAL_QUALITY;
			else
				moduleStatus = CHECK_SIM_STATUS;
			break;

		/* 开启短信发送 */
		case START_TASK_MESSAGE:
			/* 如果模块已经初始化完成，则启动发送短信，否则短信发送失败，等待下次触发发送 */
			if (gprsInited == TRUE)
				moduleStatus = SET_MESSAGE_SERVER_IP_ADDR;
			break;

		default:
			break;
		}
		/* 获取状态后，先发一次AT命令 */
		GPRS_SendCmd(AT_CMD_CHECK_STATUS);
		expectString = AT_CMD_CHECK_STATUS_RESPOND;
		break;

	/* 获取GNRMC定位值 */
	case GET_GPS_GNRMC:
		/* GPS功能使能比较慢，需要先延时一段时间 */
		GPRS_SendCmd(AT_CMD_GPS_GET_GNRMC);
		expectString = AT_CMD_GPS_GET_GNRMC_RESPOND;
		moduleStatus = GET_GPS_GNRMC_FINISH;
		break;

	/* 查询SIM卡状态 */
	case CHECK_SIM_STATUS:
		GPRS_SendCmd(AT_CMD_CHECK_SIM_STATUS);
		expectString = AT_CMD_CHECK_SIM_STATUS_RESPOND;
		moduleStatus = CHECK_SIM_STATUS_FINISH;
		break;

	/* 查找网络状态 */
	case SEARCH_NET_STATUS:
		GPRS_SendCmd(AT_CMD_SEARCH_NET_STATUS);
		expectString = AT_CMD_SEARCH_NET_STATUS_RESPOND;
		moduleStatus = SEARCH_NET_STATUS_FINISH;
		break;

	/* 查找GPRS状态 */
	case CHECK_GPRS_STATUS:
		GPRS_SendCmd(AT_CMD_CHECK_GPRS_STATUS);
		expectString = AT_CMD_CHECK_GPRS_STATUS_RESPOND;
		moduleStatus = CHECK_GPRS_STATUS_FINISH;
		break;

	/* 设置单连接方式 */
	case SET_SINGLE_LINK:
		GPRS_SendCmd(AT_CMD_SET_SINGLE_LINK);
		expectString = AT_CMD_SET_SINGLE_LINK_RESPOND;
		moduleStatus = SET_SINGLE_LINK_FINISH;
		break;

	/* 设置为透传模式 */
	case SET_SERIANET_MODE:
		GPRS_SendCmd(AT_CMD_SET_SERIANET_MODE);
		expectString = AT_CMD_SET_SERIANET_MODE_RESPOND;
		moduleStatus = SET_SERIANET_MODE_FINISH;
		break;

	/* 设置APN名称 */
	case SET_APN_NAME:
		GPRS_SendCmd(AT_CMD_SET_APN_NAME);
		expectString = AT_CMD_SET_APN_NAME_RESPOND;
		moduleStatus = SET_APN_NAME_FINISH;
		break;

	/* 激活PDP场景 */
	case ACTIVE_PDP:
		GPRS_SendCmd(AT_CMD_ACTIVE_PDP);
		expectString = AT_CMD_ACTIVE_PDP_RESPOND;
		moduleStatus = ACTIVE_PDP_FINISH;
		break;

	/* 获取本机IP地址 */
	case GET_SELF_IP_ADDR:
		GPRS_SendCmd(AT_CMD_GET_SELF_IP_ADDR);
		expectString = AT_CMD_GET_SELF_IP_ADDR_RESPOND;
		moduleStatus = GET_SELF_IP_ADDR_FINISH;
		break;

	/* 获取信号质量 */
	case GET_SIGNAL_QUALITY:
		GPRS_SendCmd(AT_CMD_GET_SIGNAL_QUALITY);
		expectString = AT_CMD_GET_SIGNAL_QUALITY_RESPOND;
		moduleStatus = GET_SIGNAL_QUALITY_FINISH;
		break;

	/* 设置服务器地址 */
	case SET_SERVER_IP_ADDR:
		GPRS_SendCmd(AT_CMD_SET_SERVER_IP_ADDR);
		expectString = AT_CMD_SET_SERVER_IP_ADDR_RESPOND;
		moduleStatus = SET_SERVER_IP_ADDR_FINISH;
		break;

	/* 模块准备好了 */
	case READY:
		GPRS_SendData(GPRS_SendPackSize);
		moduleStatus = DATA_SEND_FINISH;
		break;

	/* 设置短信服务器地址 */
	case SET_MESSAGE_SERVER_IP_ADDR:
		GPRS_SendCmd(AT_CMD_SET_MESSAGE_SERVER_IP_ADDR);
		expectString = AT_CMD_SET_MESSAGE_SERVER_IP_ADDR_RESPOND;
		moduleStatus = SET_MESSAGE_SERVER_IP_ADDR_FINISH;
		break;

	/* 短信准备好 */
	case MESSAGE_READY:
		GPRS_SendMessagePack(&GPRS_NewSendbuffer, RT_RealTime, (char*)Message, 12);
		expectString = AT_CMD_MESSAGE_SEND_SUCCESS_RESPOND;
		moduleStatus = MESSAGE_SEND_FINISH;
		break;

	/* 退出透传模式 */
	case EXTI_SERIANET_MODE:
		GPRS_SendCmd(AT_CMD_EXIT_SERIANET_MODE);
		expectString = AT_CMD_EXIT_SERIANET_MODE_RESPOND;
		moduleStatus = EXTI_SERIANET_MODE_FINISH;
		break;

	/* 退出连接模式 */
	case EXTI_LINK_MODE:
		GPRS_SendCmd(AT_CMD_EXIT_LINK_MODE);
		expectString = AT_CMD_EXIT_LINK_MODE_RESPOND;
		moduleStatus = EXTI_LINK_MODE_FINISH;
		break;

	/* 关闭移动场景 */
	case SHUT_MODULE:
		GPRS_SendCmd(AT_CMD_SHUT_MODELU);
		expectString = AT_CMD_SHUT_MODELU_RESPOND;
		moduleStatus = SHUT_MODULE_FINISH;
		break;

	default:
		break;
	}
}

/*******************************************************************************
 *
 */
void TimeoutProcess(void)
{
	DebugPrintf("GMS模块指令接收等待超时\r\n");
	/* 模块超时计数,如果超过2次，放弃本次发送，挂起任务 */
	moduleTimeoutCnt++;
	switch (moduleStatus)
	{
	/* 可能因为看门狗等因素，导致单片机重启，也需要重启模块 */
	case MODULE_VALID:
		if (moduleTimeoutCnt > 1)
		{
			moduleTimeoutCnt = 0;
			/* 断开电源控制脚，重新开启模块 */
			GPRS_PWR_CTRL_DISABLE();
			osDelay(50);
			moduleStatus = MODULE_INVALID;
		}
		break;

	case SET_BAUD_RATE_FINISH:
		moduleStatus = MODULE_INVALID;
		break;

	/* 发送到平台的数据没有收到答复,放弃本次数据发送，将模式切换到退出透传模式 */
	case DATA_SEND_FINISH:
		if (moduleTimeoutCnt > 2)
		{
			moduleTimeoutCnt = 0;
			moduleStatus = EXTI_SERIANET_MODE;
		}
		break;

	case MESSAGE_SEND_FINISH:
		if (moduleTimeoutCnt > 2)
		{
			moduleTimeoutCnt = 0;
			moduleStatus = EXTI_SERIANET_MODE;
		}
		break;

	/* GPS启动过程比较慢，暂时忽略超时等待 */
	case ENABLE_GPS_FINISH:
		if (moduleTimeoutCnt > 3)
		{
			moduleTimeoutCnt = 0;
			moduleStatus = INIT;
		}
		break;

	case GET_GPS_GNRMC_FINISH:
		if (moduleTimeoutCnt > 3)
		{
			moduleTimeoutCnt = 0;
			moduleStatus = INIT;
		}
		break;

	default:
		/* 其他情况则将状态向前移动一步 */
		moduleStatus--;
		if (moduleTimeoutCnt > 2)
		{
			moduleTimeoutCnt = 0;
			/* 回到复位状态 */
			moduleStatus = SET_BAUD_RATE;
			DebugPrintf("模块指令接收超时3次,放弃本次发送\r\n");
		}
		break;
	}
}

/*******************************************************************************
 *
 */
void RecvProcess(void)
{
	/* 正确接收，则接收错误清空 */
	moduleErrorCnt = 0;

	switch (moduleStatus)
	{
	/* 模块可用 */
	case MODULE_VALID:
		/* 开机完成，断开power控制引脚 */
		GPRS_PWR_CTRL_DISABLE();
		/* 模块开机适当延时 */
		osDelay(5000);
		moduleStatus = SET_BAUD_RATE;
		break;

	/* 设置波特率完成 */
	case SET_BAUD_RATE_FINISH:
		/* 复位模块 */
		GPRS_RST_CTRL_ENABLE();
		osDelay(50);
		GPRS_RST_CTRL_DISABLE();
		osDelay(50);
		moduleStatus = ECHO_DISABLE;
		break;

	/* 关闭回显模式完成 */
	case ECHO_DISABLE_FINISH:
		moduleStatus = GET_ICCID;
		break;

	/* 获取ICCID完成 */
	case GET_ICCID_FINISH:
		memcpy(ICCID, &GPRS_RecvBuffer.buffer.recvBuffer[10], 20);
		moduleStatus = GET_IMSI;
		break;

	/* 获取IMSI完成 */
	case GET_IMSI_FINISH:
		memcpy(IMSI, &GPRS_RecvBuffer.buffer.recvBuffer[2], 15);
		moduleStatus = GET_IMEI;
		break;

	/* 获取IMEI完成 */
	case GET_IMEI_FINISH:
		memcpy(IMEI, &GPRS_RecvBuffer.buffer.recvBuffer[10], 15);
		moduleStatus = ENABLE_GPS;
		break;

	/* 使能GPS功能完成 */
	case ENABLE_GPS_FINISH:
		/* 使能GPS功能后，开机已经完成，回到Init模式 */
		moduleStatus = INIT;
//		/* 开启GPS后，5s再获取定位数据 */
//		osDelay(5000);
		break;

	/* 获取GNRMC定位值完成 */
	case GET_GPS_GNRMC_FINISH:
		/* 转换定位数据 */
		GPS_GetLocation(GPRS_RecvBuffer.buffer.recvBuffer, &GPS_Locate);
		printf("定位数据是%50s\r\n",GPRS_RecvBuffer.buffer.recvBuffer);

		osSignalSet(mainprocessTaskHandle, MAINPROCESS_GPS_CONVERT_FINISH);
		/* GPS定位成功，回到init状态 */
		moduleStatus = INIT;
		break;

	/* 检测sim卡状态完成 */
	case CHECK_SIM_STATUS_FINISH:
		moduleStatus = SEARCH_NET_STATUS;
		break;

	/* 查找网络状态完成 */
	case SEARCH_NET_STATUS_FINISH:
		/* 注册到本地网络，或者注册到漫游网络 */
		if ((GPRS_RecvBuffer.buffer.recvBuffer[11] == '1') ||
				(GPRS_RecvBuffer.buffer.recvBuffer[11] == '5'))
			moduleStatus = CHECK_GPRS_STATUS;
		else
			moduleStatus = INIT;
		break;

	/* 查找GPRS状态完成 */
	case CHECK_GPRS_STATUS_FINISH:
		moduleStatus = SET_SINGLE_LINK;
		break;

	/* 设置单连方式完成 */
	case SET_SINGLE_LINK_FINISH:
		moduleStatus = SET_SERIANET_MODE;
		break;

	/* 设置透传模式完成 */
	case SET_SERIANET_MODE_FINISH:
		moduleStatus = SET_APN_NAME;
		break;

	/* 设置APN名称完成 */
	case SET_APN_NAME_FINISH:
		moduleStatus = ACTIVE_PDP;
		break;

	/* 激活PDP场景完成 */
	case ACTIVE_PDP_FINISH:
		moduleStatus = GET_SELF_IP_ADDR;
		break;

	/* 获取本机IP地址完成 */
	case GET_SELF_IP_ADDR_FINISH:
		moduleStatus = GET_SIGNAL_QUALITY;
		/* 标记GPRS功能初始化完成 */
		gprsInited = TRUE;
		break;

	/* 获取信号质量完成 */
	case GET_SIGNAL_QUALITY_FINISH:
		GPRS_signalQuality = GPRS_GetSignalQuality(GPRS_RecvBuffer.buffer.recvBuffer);
		printf("信号强度=%d\r\n", GPRS_signalQuality);
		moduleStatus = SET_SERVER_IP_ADDR;
		break;

	/* 设置服务器地址完成 */
	case SET_SERVER_IP_ADDR_FINISH:
		moduleStatus = READY;
		break;

	/* 设置短信服务器地址完成 */
	case SET_MESSAGE_SERVER_IP_ADDR_FINISH:
		moduleStatus = MESSAGE_READY;
		break;

	/* 退出透传模式完成 */
	case EXTI_SERIANET_MODE_FINISH:
		moduleStatus = EXTI_LINK_MODE;
		break;

	/* 退出单连模式完成 */
	case EXTI_LINK_MODE_FINISH:
		moduleStatus = SHUT_MODULE;
		break;

	/* 关闭移动场景完成 */
	case SHUT_MODULE_FINISH:
		/* 模块发送完成，把状态设置成使能GPS定位，下次启动直接连接服务器地址即可发送 */
		moduleStatus = INIT;
		break;

	default:
		break;
	}
}

/*******************************************************************************
 *
 */
void RecvErrorProcess(void)
{
	/* 错误计数 */
	moduleErrorCnt++;
	if (moduleErrorCnt >= 5)
	{
		DebugPrintf("模块接收到错误指令超过5次\r\n");
		moduleErrorCnt = 0;
		switch (moduleStatus)
		{
		case MODULE_VALID:
			break;

		/* 链接服务器地址出现“FAIL”或者“ERROR”，不能链接上服务器 */
//		case SET_SERVER_IP_ADDR_FINISH:
//			if (NULL != strstr((char*)GPRS_RecvBuffer.recvBuffer, "FAIL"))
//			{
//				/* 放弃本次发送 */
//				moduleStatus = INIT;
//
//				DebugPrintf("不能链接上服务器，放弃本次发送\r\n");
//			}
//			break;

		case DATA_SEND_FINISH:
			break;

		case ENABLE_GPS:
			break;

		case GET_GPS_GNRMC:
			moduleStatus = INIT;
			break;

		default:
			moduleStatus = SET_BAUD_RATE;
			gprsInited = FALSE;
			DebugPrintf("模块配置错误，等待下次重新配置\r\n");
			break;
		}
	}
}
