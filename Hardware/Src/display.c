#include "display.h"

#include "file.h"
#include "tftlcd.h"

/******************************************************************************/
DISPLAY_StatusTypedef DISPLAY_Status;

/*******************************************************************************
 * 历史数据界面
 ******************************************************************************/
/*******************************************************************************
 * 历史数据显示
 */
void DISPLAY_HistoryData(uint16_t offsetStruct)
{
	uint8_t i;
	FILE_SaveInfoTypedef saveInfo;

	for (i = 0; i < DISPLAY_HIS_DATA_ONE_SCREEN_CNT; i++)
	{
		FILE_ReadFile(FILE_NAME_SAVE_DATA, offsetStruct * sizeof(FILE_SaveInfoTypedef),
				(uint8_t*)&saveInfo, sizeof(FILE_SaveInfoTypedef));

		TFTLCD_HistoryDataFormat(&saveInfo, (TFTLCD_HisDataCtlIdEnum)(CTL_ID_DIS_DATA_1 + i));

		offsetStruct++;
	}
}

/*******************************************************************************
 * function：历史数据界面，接收用户操作
 */
void DISPLAY_HistoryTouch(uint16_t typeID)
{
	switch (typeID)
	{
	case CTL_ID_PAGE_UP:
		DISPLAY_Status.hisDataDispStructOffset += DISPLAY_HIS_DATA_ONE_SCREEN_CNT;
		/* 避免翻过头，没有数据显示 */
		if (DISPLAY_Status.hisDataDispStructOffset < dataFileStructCnt)
			DISPLAY_HistoryData(DISPLAY_Status.hisDataDispStructOffset);
		else
			DISPLAY_Status.hisDataDispStructOffset -= DISPLAY_HIS_DATA_ONE_SCREEN_CNT;
		break;

	case CTL_ID_PAGE_DOWN:
		DISPLAY_Status.hisDataDispStructOffset -= DISPLAY_HIS_DATA_ONE_SCREEN_CNT;
		DISPLAY_HistoryData(DISPLAY_Status.hisDataDispStructOffset);
		break;

	default:
		break;
	}
}

/*******************************************************************************
 * 数据打印界面
 ******************************************************************************/
void DISPLAY_PrintTouch(uint16_t typeID)
{
	switch(typeID)
	{
	case CTL_ID_PRINT_TIME_START_TOUCH:
		/* 时间选择界面是由开始打印时间选择进入 */
		DISPLAY_Status.timeSelectStatus = TIME_SELECT_START_PRINT_TIME;

		/* 传递时间指针 */
		DISPLAY_Status.selectTime = &DISPLAY_Status.printTimeStart;

		/* 复位时间选择控件到当前时间 */
		/* todo */
		break;

	case CTL_ID_PRINT_TIME_END_TOUCH:
		/* 时间选择界面是由结束打印时间选择进入 */
		DISPLAY_Status.timeSelectStatus = TIME_SELECT_END_PRINT_TIME;

		/* 传递时间指针 */
		DISPLAY_Status.selectTime = &DISPLAY_Status.printTimeEnd;
		/* 复位时间选择控件到当前时间 */
		/* todo */
		break;

	case CTL_ID_CHANNAL_SELECT_CH1_TOUCH:
		DISPLAY_Status.printChannelStatus.status.bit.ch1 =
				!DISPLAY_Status.printChannelStatus.status.bit.ch1;
		TFTLCD_ChannelSelectICON(SCREEN_ID_PRINT, CTL_ID_CHANNAL_SELECT_CH1_ICON,
				DISPLAY_Status.printChannelStatus.status.bit.ch1);
		break;

	case CTL_ID_CHANNAL_SELECT_CH2_TOUCH:
		DISPLAY_Status.printChannelStatus.status.bit.ch2 =
				!DISPLAY_Status.printChannelStatus.status.bit.ch2;
		TFTLCD_ChannelSelectICON(SCREEN_ID_PRINT, CTL_ID_CHANNAL_SELECT_CH2_ICON,
				DISPLAY_Status.printChannelStatus.status.bit.ch2);
		break;

	case CTL_ID_CHANNAL_SELECT_CH3_TOUCH:
		DISPLAY_Status.printChannelStatus.status.bit.ch3 =
				!DISPLAY_Status.printChannelStatus.status.bit.ch3;
		TFTLCD_ChannelSelectICON(SCREEN_ID_PRINT, CTL_ID_CHANNAL_SELECT_CH3_ICON,
				DISPLAY_Status.printChannelStatus.status.bit.ch3);
		break;

	case CTL_ID_CHANNAL_SELECT_CH4_TOUCH:
		DISPLAY_Status.printChannelStatus.status.bit.ch4 =
				!DISPLAY_Status.printChannelStatus.status.bit.ch4;
		TFTLCD_ChannelSelectICON(SCREEN_ID_PRINT, CTL_ID_CHANNAL_SELECT_CH4_ICON,
				DISPLAY_Status.printChannelStatus.status.bit.ch4);
		break;

	case CTL_ID_CHANNAL_SELECT_CH5_TOUCH:
		DISPLAY_Status.printChannelStatus.status.bit.ch5 =
				!DISPLAY_Status.printChannelStatus.status.bit.ch5;
		TFTLCD_ChannelSelectICON(SCREEN_ID_PRINT, CTL_ID_CHANNAL_SELECT_CH5_ICON,
				DISPLAY_Status.printChannelStatus.status.bit.ch5);
		break;

	case CTL_ID_CHANNAL_SELECT_CH6_TOUCH:
		DISPLAY_Status.printChannelStatus.status.bit.ch6 =
				!DISPLAY_Status.printChannelStatus.status.bit.ch6;
		TFTLCD_ChannelSelectICON(SCREEN_ID_PRINT, CTL_ID_CHANNAL_SELECT_CH6_ICON,
				DISPLAY_Status.printChannelStatus.status.bit.ch6);
		break;

	case CTL_ID_CHANNAL_SELECT_CH7_TOUCH:
		DISPLAY_Status.printChannelStatus.status.bit.ch7 =
				!DISPLAY_Status.printChannelStatus.status.bit.ch7;
		TFTLCD_ChannelSelectICON(SCREEN_ID_PRINT, CTL_ID_CHANNAL_SELECT_CH7_ICON,
				DISPLAY_Status.printChannelStatus.status.bit.ch7);
		break;

	case CTL_ID_CHANNAL_SELECT_CH8_TOUCH:
		DISPLAY_Status.printChannelStatus.status.bit.ch8 =
				!DISPLAY_Status.printChannelStatus.status.bit.ch8;
		TFTLCD_ChannelSelectICON(SCREEN_ID_PRINT, CTL_ID_CHANNAL_SELECT_CH8_ICON,
				DISPLAY_Status.printChannelStatus.status.bit.ch8);
		break;

	default:
		break;
	}
}

/*******************************************************************************
 * 时间选择界面
 ******************************************************************************/
void DISPLAY_TimeSelectTouch(uint16_t typeID, uint8_t value)
{
	switch (typeID)
	{
	case CTL_ID_TIME_SELECT_YEAR:
		DISPLAY_Status.selectTime->year = 17 + value;
		break;

	case CTL_ID_TIME_SELECT_MONTH:
		DISPLAY_Status.selectTime->month = 1 + value;
		break;

	case CTL_ID_TIME_SELECT_DAY:
		DISPLAY_Status.selectTime->day = 1 + value;
		break;

	case CTL_ID_TIME_SELECT_HOUR:
		DISPLAY_Status.selectTime->hour = value;
		break;

	case CTL_ID_TIME_SELECT_MIN:
		DISPLAY_Status.selectTime->min = value;
		break;

	case CTL_ID_TIME_SELECT_CANCEL:
		memset(DISPLAY_Status.selectTime, 0, sizeof(FILE_RealTimeTypedef));
		break;

	case CTL_ID_TIME_SELECT_OK:
		break;

	default:
		break;
	}
}

/*******************************************************************************
 *
 */
static void TimeSelectReturn(void)
{
	switch(DISPLAY_Status.timeSelectStatus)
	{
	case TIME_SELECT_START_PRINT_TIME:
		break;

	case TIME_SELECT_END_PRINT_TIME:
		break;

	default:
		break;
	}
}












