/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

/*
 * AK7374AF voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "lens_info.h"


#define AF_DRVNAME "AK7374AF_DRV"
#define AF_I2C_SLAVE_ADDR        0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif


static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

#if defined(CONFIG_MACH_MT6771)
static unsigned int g_ACKErrorCnt = 5;
#else
static unsigned int g_ACKErrorCnt = 100;
#endif

#ifdef  VENDOR_EDIT
/*weiriqin@camera.driver, 2019/09/01, add for update AK7374 motor PID parameter*/
#include <soc/oppo/oppo_project.h>
static void ak7374af_loadfirmware(void);
static void ak7374af_checkfirmware(void);
static int check_status = 0;
#endif

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static inline void AFI2CSendFormat(struct stAF_MotorI2CSendCmd *pstMotor)
{
	pstMotor->Resolution = 10;
	pstMotor->SlaveAddr  = 0x18;
	pstMotor->I2CSendNum = 2;

	pstMotor->I2CFmt[0].AddrNum = 1;
	pstMotor->I2CFmt[0].DataNum = 1;
	/* Addr Format */
	pstMotor->I2CFmt[0].Addr[0]  = 0x00;
	/* Data Format : CtrlData | ( ( Data >> BitRR ) & Mask1 ) << BitRL ) & Mask2 */
	pstMotor->I2CFmt[0].CtrlData[0] = 0x00; /* Control Data */
	pstMotor->I2CFmt[0].BitRR[0] = 2;
	pstMotor->I2CFmt[0].Mask1[0] = 0xFF;
	pstMotor->I2CFmt[0].BitRL[0] = 0;
	pstMotor->I2CFmt[0].Mask2[0] = 0xFF;


	pstMotor->I2CFmt[1].AddrNum = 1;
	pstMotor->I2CFmt[1].DataNum = 1;
	/* Addr Format */
	pstMotor->I2CFmt[1].Addr[0]  = 0x01;
	/* Data Format : CtrlData | ( ( Data >> BitRR ) & Mask1 ) << BitRL ) & Mask2 */
	pstMotor->I2CFmt[1].CtrlData[0] = 0x00; /* Control Data */
	pstMotor->I2CFmt[1].BitRR[0] = 0;
	pstMotor->I2CFmt[1].Mask1[0] = 0x03;
	pstMotor->I2CFmt[1].BitRL[0] = 6;
	pstMotor->I2CFmt[1].Mask2[0] = 0xC0;
}

static inline int getAFI2CSendFormat(__user struct stAF_MotorI2CSendCmd *pstMotorI2CSendCmd)
{
	struct stAF_MotorI2CSendCmd stMotor;

	AFI2CSendFormat(&stMotor);

	if (copy_to_user(pstMotorI2CSendCmd, &stMotor, sizeof(struct stAF_MotorI2CSendCmd)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static int s4AF_ReadReg(u8 a_uAddr, u16 *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff;
	char puSendCmd[1];

	if (g_ACKErrorCnt == 0)
		return 0;

	puSendCmd[0] = a_uAddr;

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 1);

	if (i4RetValue < 0) {
		if (g_ACKErrorCnt > 0)
			g_ACKErrorCnt--;

		LOG_INF("I2C read - send failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, &pBuff, 1);

	if (i4RetValue < 0) {
		if (g_ACKErrorCnt > 0)
			g_ACKErrorCnt--;

		LOG_INF("I2C read - recv failed!!\n");
		return -1;
	}
	*a_pu2Result = pBuff;

	return 0;
}

static int s4AF_WriteReg(u16 a_u2Addr, u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = { (char)a_u2Addr, (char)a_u2Data };

	if (g_ACKErrorCnt == 0)
		return 0;

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	/* LOG_INF("I2C Addr[0] = 0x%x , Data[0] = 0x%x\n", puSendCmd[0], puSendCmd[1]); */

	if (i4RetValue < 0) {
		if (g_ACKErrorCnt > 0)
			g_ACKErrorCnt--;

		LOG_INF("I2C write failed!!\n");
		return -1;
	}

	return 0;
}

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static inline int setVCMPos(unsigned long a_u4Position)
{
	int i4RetValue = 0;

	#if 1

	i4RetValue = s4AF_WriteReg(0x0, (u16) ((a_u4Position >> 2) & 0xff));

	if (i4RetValue < 0)
		return -1;

	i4RetValue = s4AF_WriteReg(0x1, (u16) ((g_u4TargetPosition & 0x3) << 6));

	#else
	{
		u8 i, j;
		u8 Resolution, SlaveAddr;

		struct stAF_MotorI2CSendCmd stMotor;

		AFI2CSendFormat(&stMotor);

		Resolution = stMotor.Resolution;
		SlaveAddr = stMotor.SlaveAddr;
		for (i = 0; i < stMotor.I2CSendNum; i++) {
			struct stAF_CCUI2CFormat stCCUFmt;
			struct stAF_DrvI2CFormat *pstI2CFmt;

			stCCUFmt.BufSize = 0;
			pstI2CFmt = &stMotor.I2CFmt[i];

			/* Slave Addr */
			stCCUFmt.I2CBuf[stCCUFmt.BufSize] = SlaveAddr;
			stCCUFmt.BufSize++;

			/* Addr part */
			for (j = 0; j < pstI2CFmt->AddrNum; j++) {
				stCCUFmt.I2CBuf[stCCUFmt.BufSize] = pstI2CFmt->Addr[j];
				stCCUFmt.BufSize++;
			}

			/* Data part */
			for (j = 0; j < pstI2CFmt->DataNum; j++) {
				u8 DataByte = pstI2CFmt->CtrlData[j]; /* Control bits */

				/* Position bits */
				DataByte |= ((((a_u4Position >> pstI2CFmt->BitRR[j]) & pstI2CFmt->Mask1[j]) <<
						pstI2CFmt->BitRL[j]) & pstI2CFmt->Mask2[j]);
				stCCUFmt.I2CBuf[stCCUFmt.BufSize] = DataByte;
				stCCUFmt.BufSize++;
			}
			LOG_INF("I2CBuf[%d] Data[0] = 0x%x\tData[1] = 0x%x\tData[2] = 0x%x\n", i,
				stCCUFmt.I2CBuf[0], stCCUFmt.I2CBuf[1], stCCUFmt.I2CBuf[2]);

			g_pstAF_I2Cclient->addr = stCCUFmt.I2CBuf[0] >> 1;
			i2c_master_send(g_pstAF_I2Cclient, &stCCUFmt.I2CBuf[1], stCCUFmt.BufSize - 1);
		}
	}
	#endif

	return i4RetValue;
}

static inline int initdrv(void)
{
	int i4RetValue = 0;
	int ret = 0;
	unsigned short data = 0;
	#ifdef VENDOR_EDIT
	/*weiriqin@camera.driver, 2019/09/01, add for update AK7374 motor PID parameter*/
	//if(is_project(OPPO_19531)) {
	LOG_INF("AK7374AF  start  load firmware\n");
	ak7374af_checkfirmware();
	LOG_INF("initdrv check_status=%d\n", check_status);
	if(check_status == 1) {
		ak7374af_loadfirmware();
	}
	LOG_INF("AK7374AF  end  load firmware\n");
	//}
	#endif
	/* 00:active mode , 10:Standby mode , x1:Sleep mode */
	ret = s4AF_WriteReg(0x02, 0x00); /* from Standby mode to Active mode */
	msleep(20);

	if (ret == 0) {
		ret = s4AF_ReadReg(0x02, &data);

		if ((ret == 0) && (data == 0))
			i4RetValue = 1;
	}

	return i4RetValue;
}
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos, InitPosM, InitPosL;

		if (initdrv() == 1) {
			spin_lock(g_pAF_SpinLock);
			*g_pAF_Opened = 2;
			spin_unlock(g_pAF_SpinLock);
		} else {
			LOG_INF("InitDrv Fail!! I2C error occurred");
		}

		s4AF_ReadReg(0x0, &InitPosM);
		ret = s4AF_ReadReg(0x1, &InitPosL);
		InitPos = ((InitPosM & 0xFF) << 2) + ((InitPosL >> 6) & 0x3);

		if (ret == 0) {
			LOG_INF("Init Pos %6d\n", InitPos);

			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(g_pAF_SpinLock);

		} else {
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = 0;
			spin_unlock(g_pAF_SpinLock);
		}
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	/* LOG_INF("move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition); */

	/* s4AF_WriteReg(0x02, 0x00); */

	if (setVCMPos(g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
		ret = -1;
	}

	return ret;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long AK7374AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user struct stAF_MotorInfo *) (a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int AK7374AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");
		s4AF_WriteReg(0x02, 0x20);
		msleep(20);
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

int AK7374AF_PowerDown(void)
{
	LOG_INF("+\n");
	if (*g_pAF_Opened == 0) {
		unsigned short data = 0;
		int cnt = 0;

		while (1) {
			data = 0;

			s4AF_WriteReg(0x02, 0x20);

			s4AF_ReadReg(0x02, &data);

			LOG_INF("Addr : 0x02 , Data : %x\n", data);

			if (data == 0x20 || cnt == 1)
				break;

			cnt++;
		}
	} else if (*g_pAF_Opened == 2) {
		*g_pAF_Opened = 1;
		LOG_INF("reopen driver init\n");
	}
	LOG_INF("-\n");

	return 0;
}

int AK7374AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	#if defined(CONFIG_MACH_MT6771)
	g_ACKErrorCnt = 5;
	#else
	g_ACKErrorCnt = 100;
	#endif

	return 1;
}

int AK7374AF_GetFileName(unsigned char *pFileName)
{
        #if SUPPORT_GETTING_LENS_FOLDER_NAME
        char FilePath[256];
        char *FileString;

        sprintf(FilePath, "%s", __FILE__);
        FileString = strrchr(FilePath, '/');
        *FileString = '\0';
        FileString = (strrchr(FilePath, '/') + 1);
        strncpy(pFileName, FileString, AF_MOTOR_NAME);
        LOG_INF("FileName : %s\n", pFileName);
        #else
        pFileName[0] = '\0';
        #endif
        return 1;
}

#ifdef VENDOR_EDIT
/*weiriqin@camera.driver, 2019/09/01, add for update AK7374 motor PID parameter*/
static u8 readfirmware_addr[20] = {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1c,0x1d,0x1e,0x20,0x21,0x22,0x23,0x24,0x25};
static u16 readfirmware_value[20] = {0x2a,0x41,0xb9,0x4e,0x23,0x73,0x0c,0x5f,0xdb,0x00,0x12,0x00,0x00,0x00,0x00,0x41,0x01,0x0d,0x00,0x00};
static void ak7374af_checkfirmware(void)
{
	int i;
	int ret;
	unsigned short register_data;
	for(i = 0; i < 20; i++) {
		ret = s4AF_ReadReg(readfirmware_addr[i],&register_data);
		if(ret != 0) {
			LOG_INF("ak7374af_checkfirmware, read readfirmware_addr[%d]=0x%x  fail!!!!!\n", i , readfirmware_addr[i]);
		}
		LOG_INF("ak7374af_checkfirmware, readfirmware_addr[%d]= 0x%x and  reigster_data=0x%x\n", i,readfirmware_addr[i],register_data );
		if(register_data != readfirmware_value[i]) {
			check_status=1;
			LOG_INF("ak7374af_checkfirmware check_status=1\n");
			break;
		}
	}
}

static void ak7374af_writereg(void)
{
	msleep(10);
	s4AF_WriteReg(0xae,0x3b);
	s4AF_WriteReg(0x10,0x2a);
	s4AF_WriteReg(0x11,0x41);
	s4AF_WriteReg(0x12,0xb9);
	s4AF_WriteReg(0x13,0x4e);
	s4AF_WriteReg(0x14,0x23);
	s4AF_WriteReg(0x15,0x73);
	s4AF_WriteReg(0x16,0x0c);
	s4AF_WriteReg(0x17,0x5f);
	s4AF_WriteReg(0x18,0xdb);
	s4AF_WriteReg(0x19,0x00);
	s4AF_WriteReg(0x1a,0x12);
	s4AF_WriteReg(0x1c,0x00);
	s4AF_WriteReg(0x1d,0x00);
	s4AF_WriteReg(0x1e,0x00);
	s4AF_WriteReg(0x20,0x00);
	s4AF_WriteReg(0x21,0x41);
	s4AF_WriteReg(0x22,0x01);
	s4AF_WriteReg(0x23,0x0d);
	s4AF_WriteReg(0x24,0x00);
	s4AF_WriteReg(0x25,0x00);

	s4AF_WriteReg(0x03,0x01);
	msleep(90);
	s4AF_WriteReg(0x03,0x02);
	msleep(90);
	s4AF_WriteReg(0x03,0x04);
	msleep(54);
	s4AF_WriteReg(0x03,0x08);
	msleep(36);
}

static void ak7374af_loadfirmware(void)
{
	int i,ret;
	int retry = 3;
	unsigned short save_flag;
	for(i = 0; i < retry; i++) {

		ak7374af_writereg();

		ret =s4AF_ReadReg(0x4b,&save_flag);
		if(ret != 0) {
			LOG_INF("Read addr 0x04b fail!!\n");
		}
		LOG_INF("Addr : 0x04b ,  save_flag: 0x%x\n", save_flag);

		save_flag &= (1<<2);
		if(save_flag) {
			LOG_INF("ak7374af_writereg fail !!!\n");
		} else {
			LOG_INF("ak7374af_writereg success !!!\n");
			break;
		}
	}

	s4AF_WriteReg(0xae,0x00);

	if(i >= 3) {
		check_status = 1;
		LOG_INF("ak7374af_loadfirmware fail !!!\n");
	} else {
		check_status = 0;
		LOG_INF("ak7374af_loadfirmware success !!!\n");
	}

}
#endif
