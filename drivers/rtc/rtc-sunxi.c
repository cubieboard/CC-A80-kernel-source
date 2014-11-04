/*
 * drivers\rtc\rtc-sunxi.c
 * (C) Copyright 2010-2016
 * ruinerwinner Technology Co., Ltd. <www.allwinnertech.com>
 * huangxin <huangxin@allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#if defined(CONFIG_ARCH_SUN8IW6P1) || defined(CONFIG_ARCH_SUN9IW1P1)
#include "rtc-sunxi-external.c"
#else
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/clk.h>
#include <linux/log2.h>
#include <linux/delay.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>

#undef RTC_DBG
#undef RTC_ERR
#if (0)
    #define RTC_DBG(format,args...)  printk("[rtc] "format,##args)
    #define RTC_ERR(format,args...)  printk("[rtc] "format,##args)
#else
    #define RTC_DBG(...)
    #define RTC_ERR(...)
#endif

#define SUNXI_VA_TIMERC_IO_BASE				(0xf1f00000)

#define SUNXI_LOSC_CTRL_REG               	(0x0)

#define SUNXI_RTC_DATE_REG                	(0x0010)
#define SUNXI_RTC_TIME_REG                	(0x0014)

#define SUNXI_RTC_ALARM_COUNTER_REG   		(0x0020)
#define SUNXI_RTC_ALARM_CURRENT_REG   		(0x0024)
#define SUNXI_ALARM_EN_REG                	(0x0028)
#define SUNXI_ALARM_INT_CTRL_REG          	(0x002c)
#define SUNXI_ALARM_INT_STATUS_REG        	(0x0030)
#define SUNXI_ALARM_CONFIG					(0x0050)

/*rtc count interrupt control*/
#define RTC_ALARM_COUNT_INT_EN 				0x00000100

#define RTC_ENABLE_CNT_IRQ        			0x00000001

/*Crystal Control*/
#define REG_LOSCCTRL_MAGIC		    		0x16aa0000
#define REG_CLK32K_AUTO_SWT_EN  			(0x00004000)
#define RTC_SOURCE_EXTERNAL         		0x00000001
#define RTC_HHMMSS_ACCESS           		0x00000100
#define RTC_YYMMDD_ACCESS           		0x00000080
#define EXT_LOSC_GSM                    	(0x00000008)

/*Date Value*/
#define DATE_GET_DAY_VALUE(x)       		((x) &0x0000001f)
#define DATE_GET_MON_VALUE(x)       		(((x)&0x00000f00) >> 8 )
#define DATE_GET_YEAR_VALUE(x)      		(((x)&0x003f0000) >> 16)

#define DATE_SET_DAY_VALUE(x)       		DATE_GET_DAY_VALUE(x)
#define DATE_SET_MON_VALUE(x)       		(((x)&0x0000000f) << 8 )
#define DATE_SET_YEAR_VALUE(x)      		(((x)&0x0000003f) << 16)
#define LEAP_SET_VALUE(x)           		(((x)&0x00000001) << 22)

/*Time Value*/
#define TIME_GET_SEC_VALUE(x)       		((x) &0x0000003f)
#define TIME_GET_MIN_VALUE(x)       		(((x)&0x00003f00) >> 8 )
#define TIME_GET_HOUR_VALUE(x)      		(((x)&0x001f0000) >> 16)

#define TIME_SET_SEC_VALUE(x)       		TIME_GET_SEC_VALUE(x)
#define TIME_SET_MIN_VALUE(x)       		(((x)&0x0000003f) << 8 )
#define TIME_SET_HOUR_VALUE(x)      		(((x)&0x0000001f) << 16)

#define SUNXI_ALARM

static void __iomem *sunxi_rtc_base;

static int sunxi_rtc_alarmno = NO_IRQ;
static int losc_err_flag   = 0;

#ifdef SUNXI_ALARM
/* IRQ Handlers, irq no. is shared with timer2 */
static irqreturn_t sunxi_rtc_alarmirq(int irq, void *id)
{
	struct rtc_device *rdev = id;
	u32 val;
	
	RTC_DBG("hx-%s: line:%d\n", __func__, __LINE__);

    /*judge the irq is beyond to the alarm0*/
    val = readl(sunxi_rtc_base + SUNXI_ALARM_INT_STATUS_REG)&(RTC_ENABLE_CNT_IRQ);
    if (val) {
    	RTC_DBG("%s: line:%d\n", __func__, __LINE__);
		/*Clear pending count alarm*/
		val = readl(sunxi_rtc_base + SUNXI_ALARM_INT_STATUS_REG);//0x0030
		val |= (RTC_ENABLE_CNT_IRQ);	//0x00000001
		writel(val, sunxi_rtc_base + SUNXI_ALARM_INT_STATUS_REG);
		
		rtc_update_irq(rdev, 1, RTC_AF | RTC_IRQF);
		return IRQ_HANDLED;
    } else {
        return IRQ_NONE;
    }
}

/* Update control registers,asynchronous interrupt enable*/
static void sunxi_rtc_setaie(int to)
{
	u32 alarm_irq_val;  
	
	RTC_DBG("%s: aie=%d\n", __func__, to);

	//alarm_irq_val = readl(sunxi_rtc_base + SUNXI_ALARM_EN_REG);//0x0028
	switch(to){		
		case 1:
		alarm_irq_val = 0x00000001;		//0x00000100
	    writel(0x00000001, sunxi_rtc_base + SUNXI_ALARM_EN_REG);//0x0028
	    writel(0x00000001, sunxi_rtc_base + SUNXI_ALARM_CONFIG);
	    RTC_DBG("%s,line:%d,28 reg val:%x\n", __func__, __LINE__, *(volatile int *)(0xf1f00000+0x28));
		break;
		case 0:
		default:
		alarm_irq_val = 0x00000000;
	    writel(alarm_irq_val, sunxi_rtc_base + SUNXI_ALARM_EN_REG);//0x0028
	    /*clear the alarm irq*/
	    writel(0x00000000, sunxi_rtc_base + SUNXI_ALARM_INT_CTRL_REG);//0x002c
	    writel(0x00000000, sunxi_rtc_base + SUNXI_ALARM_CONFIG);
	    /*Clear pending count alarm*/
		writel(0x00000001, sunxi_rtc_base + SUNXI_ALARM_INT_STATUS_REG);//0x0030
		break;
	}	
}
#endif

/* Time read/write */
static int sunxi_rtc_gettime(struct device *dev, struct rtc_time *rtc_tm)
{
	unsigned int have_retried = 0;
	void __iomem *base = sunxi_rtc_base;
	unsigned int date_tmp = 0;
	unsigned int time_tmp = 0;

    /*only for alarm losc err occur.*/
    if(losc_err_flag) {
		rtc_tm->tm_sec  = 0;
		rtc_tm->tm_min  = 0;
		rtc_tm->tm_hour = 0;
		
		rtc_tm->tm_mday = 0;
		rtc_tm->tm_mon  = 0;
		rtc_tm->tm_year = 0;
		return -1;
    }

retry_get_time:
	_dev_info(dev,"sunxi_rtc_gettime\n");
    /*first to get the date, then time, because the sec turn to 0 will effect the date;*/
	date_tmp = readl(base + SUNXI_RTC_DATE_REG);
	time_tmp = readl(base + SUNXI_RTC_TIME_REG);

	rtc_tm->tm_sec  = TIME_GET_SEC_VALUE(time_tmp);
	rtc_tm->tm_min  = TIME_GET_MIN_VALUE(time_tmp);
	rtc_tm->tm_hour = TIME_GET_HOUR_VALUE(time_tmp);	
	
	rtc_tm->tm_mday = DATE_GET_DAY_VALUE(date_tmp);
	rtc_tm->tm_mon  = DATE_GET_MON_VALUE(date_tmp);
	rtc_tm->tm_year = DATE_GET_YEAR_VALUE(date_tmp);

	/* the only way to work out wether the system was mid-update
	 * when we read it is to check the second counter, and if it
	 * is zero, then we re-try the entire read
	 */
	if (rtc_tm->tm_sec == 0 && !have_retried) {
		have_retried = 1;
		goto retry_get_time;
	}

	rtc_tm->tm_year += 70;
	rtc_tm->tm_mon  -= 1;
	_dev_info(dev,"read time %d-%d-%d %d:%d:%d\n",
	       rtc_tm->tm_year + 1900, rtc_tm->tm_mon + 1, rtc_tm->tm_mday,
	       rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

	return 0;
}

static int sunxi_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	void __iomem *base = sunxi_rtc_base;
	unsigned int date_tmp = 0;
	unsigned int time_tmp = 0;
	unsigned int crystal_data = 0;
	unsigned int timeout = 0;
	int leap_year = 0;

    /*int tm_year; years from 1900
    *int tm_mon; months since january 0-11
    *the input para tm->tm_year is the offset related 1900;
    */
	leap_year = tm->tm_year + 1900;
	if(leap_year > 2033 || leap_year < 1970) {
		dev_err(dev, "rtc only supports 63��1970��2033�� years\n");
		return -EINVAL;
	}

	crystal_data = readl(base + SUNXI_LOSC_CTRL_REG);

	/*Any bit of [9:7] is set, The time and date
	* register can`t be written, we re-try the entried read
	*/
	{
	    /*check at most 3 times.*/
	    int times = 3;
	    while((crystal_data & 0x380) && (times--)){
	    	RTC_DBG(KERN_INFO"[RTC]canot change rtc now!\n");
	    	msleep(500);
	    	crystal_data = readl(base + SUNXI_LOSC_CTRL_REG);
	    }
	}

	/*sunxi ONLY SYPPORTS 63 YEARS,hardware base time:1900. 
	*1970 as 0
	*/
	tm->tm_year -= 70;
	tm->tm_mon  += 1;
	
	/*prevent the application seting the error time*/
	if (tm->tm_mon > 12) {
		RTC_DBG("set time month error:line:%d,%d-%d-%d %d:%d:%d\n", __LINE__,
	       tm->tm_year + 2010, tm->tm_mon, tm->tm_mday,
	       tm->tm_hour, tm->tm_min, tm->tm_sec);
		switch (tm->tm_mon) {
			case 1:
			case 3:
			case 5:
			case 7:
			case 8:
			case 10:
			case 12:
				if (tm->tm_mday > 31) {
					_dev_info(dev, "set time day error:line:%d,%d-%d-%d %d:%d:%d\n",__LINE__,
				       tm->tm_year + 2010, tm->tm_mon, tm->tm_mday,
				       tm->tm_hour, tm->tm_min, tm->tm_sec);					
				}
				if ((tm->tm_hour > 24)||(tm->tm_min > 59)||(tm->tm_sec > 59)) {
						_dev_info(dev, "set time error:line:%d,%d-%d-%d %d:%d:%d\n",__LINE__,
				       tm->tm_year + 2010, tm->tm_mon, tm->tm_mday,
				       tm->tm_hour, tm->tm_min, tm->tm_sec);									
				}
				break;
			case 4:
			case 6:
			case 9:
			case 11:
				if (tm->tm_mday > 30) {
					_dev_info(dev, "set time day error:line:%d,%d-%d-%d %d:%d:%d\n",__LINE__,
				       tm->tm_year + 2010, tm->tm_mon, tm->tm_mday,
				       tm->tm_hour, tm->tm_min, tm->tm_sec);					
				}
				if ((tm->tm_hour > 24)||(tm->tm_min > 59)||(tm->tm_sec > 59)) {
					_dev_info(dev, "set time error:line:%d,%d-%d-%d %d:%d:%d\n",__LINE__,
				       tm->tm_year + 2010, tm->tm_mon, tm->tm_mday,
				       tm->tm_hour, tm->tm_min, tm->tm_sec);									
				}
				break;				
			case 2:
				if ((leap_year%400==0) || ((leap_year%100!=0) && (leap_year%4==0))) {
					if (tm->tm_mday > 28) {
						_dev_info(dev, "set time day error:line:%d,%d-%d-%d %d:%d:%d\n",__LINE__,
				       		tm->tm_year + 2010, tm->tm_mon, tm->tm_mday,
				       		tm->tm_hour, tm->tm_min, tm->tm_sec);					
					}
					if((tm->tm_hour > 24)||(tm->tm_min > 59)||(tm->tm_sec > 59)){
						_dev_info(dev, "set time error:line:%d,%d-%d-%d %d:%d:%d\n",__LINE__,
					       tm->tm_year + 2010, tm->tm_mon, tm->tm_mday,
					       tm->tm_hour, tm->tm_min, tm->tm_sec);									
					}
				} else {
					if (tm->tm_mday > 29) {
						_dev_info(dev, "set time day error:line:%d,%d-%d-%d %d:%d:%d\n",__LINE__,
					       tm->tm_year + 2010, tm->tm_mon, tm->tm_mday,
					       tm->tm_hour, tm->tm_min, tm->tm_sec);					
					}
					if ((tm->tm_hour > 24)||(tm->tm_min > 59)||(tm->tm_sec > 59)) {
						_dev_info(dev, "set time error:line:%d,%d-%d-%d %d:%d:%d\n",__LINE__,
					       tm->tm_year + 2010, tm->tm_mon, tm->tm_mday,
					       tm->tm_hour, tm->tm_min, tm->tm_sec);									
					}

				}
				break;
			default:				
				break;
		}
		tm->tm_sec  = 0;
		tm->tm_min  = 0;
		tm->tm_hour = 0;		
		tm->tm_mday = 0;
		tm->tm_mon  = 0;
		tm->tm_year = 0;
	}

	_dev_info(dev, "set time %d-%d-%d %d:%d:%d\n",
	       tm->tm_year + 1970, tm->tm_mon, tm->tm_mday,
	       tm->tm_hour, tm->tm_min, tm->tm_sec);
	
	date_tmp = (DATE_SET_DAY_VALUE(tm->tm_mday)|DATE_SET_MON_VALUE(tm->tm_mon)
                    |DATE_SET_YEAR_VALUE(tm->tm_year));

	time_tmp = (TIME_SET_SEC_VALUE(tm->tm_sec)|TIME_SET_MIN_VALUE(tm->tm_min)
                    |TIME_SET_HOUR_VALUE(tm->tm_hour));
                    
	writel(time_tmp,  base + SUNXI_RTC_TIME_REG);
	timeout = 0xffff;
	
	while((readl(base + SUNXI_LOSC_CTRL_REG)&(RTC_HHMMSS_ACCESS))&&(--timeout))
	if (timeout == 0) {
        dev_err(dev, "fail to set rtc time.\n");        
        return -1;
    }
    
	if((leap_year%400==0) || ((leap_year%100!=0) && (leap_year%4==0))) {
		/*Set Leap Year bit*/
		date_tmp |= LEAP_SET_VALUE(1);
	}

	writel(date_tmp, base + SUNXI_RTC_DATE_REG);
	timeout = 0xffff;
	while((readl(base + SUNXI_LOSC_CTRL_REG)&(RTC_YYMMDD_ACCESS))&&(--timeout))
	if (timeout == 0) {
        dev_err(dev, "fail to set rtc date.\n");
        return -1;
    }

	return 0;
}

#ifdef SUNXI_ALARM
static int sunxi_rtc_getalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alm_tm = &alrm->time;
	void __iomem *base = sunxi_rtc_base;
	unsigned int alarm_en;
	unsigned long alarm_count = 0;
	unsigned long alarm_current = 0;
	unsigned long alarm_gap = 0;
	unsigned long alarm_gap_day = 0;
	unsigned long alarm_gap_hour = 0;
	unsigned long alarm_gap_minute = 0;
	unsigned long alarm_gap_second = 0;
	unsigned int date_tmp = 0;

    alarm_count = readl(base + SUNXI_RTC_ALARM_COUNTER_REG);
    alarm_current = readl(base + SUNXI_RTC_ALARM_CURRENT_REG);
    
	date_tmp = readl(base + SUNXI_RTC_DATE_REG);
	
	alarm_gap = alarm_count - alarm_count;
	
    alarm_gap_day = alarm_gap/(3600*24);//day
    alarm_gap_hour = (alarm_gap - alarm_gap_day*24*60*60)/3600;//hour
    alarm_gap_minute = (alarm_gap - alarm_gap_day*24*60*60 - alarm_gap_hour*60*60)/60;//minute
    alarm_gap_second = alarm_gap - alarm_gap_day*24*60*60 - alarm_gap_hour*60*60-alarm_gap_minute*60;//second
    if(alarm_gap_day > 31) {
    	dev_err(dev, "The alarm time is too long,we assume the alarm ranges from 0 to 31\n");
    	return -EINVAL;
    }
    
	alm_tm->tm_mday = DATE_GET_DAY_VALUE(date_tmp);
	alm_tm->tm_mon  = DATE_GET_MON_VALUE(date_tmp);
	alm_tm->tm_year = DATE_GET_YEAR_VALUE(date_tmp);

    alm_tm->tm_year += 70;
    alm_tm->tm_mon  -= 1;

    alarm_en = readl(base + SUNXI_ALARM_INT_CTRL_REG);
    if(alarm_en&&RTC_ALARM_COUNT_INT_EN)
    	alrm->enabled = 1;
	
	return 0;
}

static int sunxi_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
    struct rtc_time *tm = &alrm->time;
    void __iomem *base = sunxi_rtc_base;
    unsigned int alarm_en;
    int ret = 0;
    struct rtc_time tm_now;
    unsigned long time_now = 0;
    unsigned long time_set = 0;
    unsigned long time_gap = 0;
    unsigned long time_gap_day = 0;
    unsigned long time_gap_hour = 0;
    unsigned long time_gap_minute = 0;
    unsigned long time_gap_second = 0;

    RTC_DBG("*****************************\n\n");
    RTC_DBG("line:%d,%s the alarm time: year:%d, month:%d, day:%d. hour:%d.minute:%d.second:%d\n",\
    __LINE__, __func__, tm->tm_year, tm->tm_mon,\
    	 tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);	  
   	RTC_DBG("*****************************\n\n");
   	
    ret = sunxi_rtc_gettime(dev, &tm_now);

    RTC_DBG("line:%d,%s the current time: year:%d, month:%d, day:%d. hour:%d.minute:%d.second:%d\n",\
    __LINE__, __func__, tm_now.tm_year, tm_now.tm_mon,\
    	 tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
   	RTC_DBG("*****************************\n\n");

    ret = rtc_tm_to_time(tm, &time_set);
    ret = rtc_tm_to_time(&tm_now, &time_now);
    if(time_set <= time_now){
    	dev_err(dev, "The time or date can`t set, The day has pass!!!\n");
    	return -EINVAL;
    }
    time_gap = time_set - time_now;
    time_gap_day = time_gap/(3600*24);//day
    time_gap_hour = (time_gap - time_gap_day*24*60*60)/3600;//hour
    time_gap_minute = (time_gap - time_gap_day*24*60*60 - time_gap_hour*60*60)/60;//minute
    time_gap_second = time_gap - time_gap_day*24*60*60 - time_gap_hour*60*60-time_gap_minute*60;//second
    if(time_gap_day > 255) {
    	dev_err(dev, "The time or date can`t set, The day range of 0 to 255\n");
    	return -EINVAL;
    }

   	RTC_DBG("%s,line:%d,time_gap:%ld,alrm->enabled:%d\n", __func__, __LINE__, time_gap, alrm->enabled);
    RTC_DBG("*****************************\n\n");	
	
	/*clear the alarm counter enable bit*/
    sunxi_rtc_setaie(0);
	
    /*clear the alarm count value!!!*/
    writel(0x00000000, base + SUNXI_RTC_ALARM_COUNTER_REG);   
    
    /*rewrite the alarm count value!!!*/
    writel(time_gap, base + SUNXI_RTC_ALARM_COUNTER_REG);//0x0020

    /*clear the count enable alarm irq bit*/
    writel(0x00000000, base + SUNXI_ALARM_INT_CTRL_REG);
	alarm_en = readl(base + SUNXI_ALARM_INT_CTRL_REG);//0x002c
	
	/*enable the counter alarm irq*/
	alarm_en = readl(base + SUNXI_ALARM_INT_CTRL_REG);//0x002c
	alarm_en |= RTC_ENABLE_CNT_IRQ;
    writel(alarm_en, base + SUNXI_ALARM_INT_CTRL_REG);//enable the counter irq!!!

	if(alrm->enabled != 1){
		RTC_DBG("warning:the rtc counter interrupt isnot enable!!!,%s,%d\n", __func__, __LINE__);		
	}

	/*decided whether we should start the counter to down count*/	
	sunxi_rtc_setaie(alrm->enabled);
	
	RTC_DBG("------------------------------------------\n\n");
	RTC_DBG("%d,2c reg val:%x\n", __LINE__, *(volatile int *)(0xf1f00000+0x2c));
	RTC_DBG("%d,30 reg val:%x\n", __LINE__, *(volatile int *)(0xf1f00000+0x30));
	RTC_DBG("%d,28 reg val:%x\n", __LINE__, *(volatile int *)(0xf1f00000+0x28));	
	RTC_DBG("------------------------------------------\n\n");	

	return 0;
}

static int sunxi_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	if (!enabled) {
		sunxi_rtc_setaie(enabled);
	}

	return 0;
}
#endif

static const struct rtc_class_ops sunxi_rtcops = {	
	.read_time			= sunxi_rtc_gettime,
	.set_time			= sunxi_rtc_settime,
#ifdef SUNXI_ALARM
	.read_alarm			= sunxi_rtc_getalarm,
	.set_alarm			= sunxi_rtc_setalarm,
	.alarm_irq_enable 	= sunxi_rtc_alarm_irq_enable,
#endif
};

static int __exit sunxi_rtc_remove(struct platform_device *pdev)
{
	struct rtc_device *rtc = platform_get_drvdata(pdev);

#ifdef SUNXI_ALARM
    free_irq(sunxi_rtc_alarmno, rtc);
#endif

    rtc_device_unregister(rtc);
	platform_set_drvdata(pdev, NULL);

#ifdef SUNXI_ALARM
	sunxi_rtc_setaie(0);
#endif

	return 0;
}

static int __init sunxi_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;	
	int ret;
	unsigned int tmp_data;
	
	sunxi_rtc_base = (void __iomem *)(SUNXI_VA_TIMERC_IO_BASE);
	sunxi_rtc_alarmno = SUNXI_IRQ_RALARM0;

	/*
	 *	step1: select RTC clock source
	*/
	tmp_data = readl(sunxi_rtc_base + SUNXI_LOSC_CTRL_REG); 
	tmp_data &= (~REG_CLK32K_AUTO_SWT_EN);            		//disable auto switch,bit-14
	tmp_data |= (RTC_SOURCE_EXTERNAL | REG_LOSCCTRL_MAGIC); //external     32768hz osc
	tmp_data |= (EXT_LOSC_GSM);                             //external 32768hz osc gsm
	writel(tmp_data, sunxi_rtc_base + SUNXI_LOSC_CTRL_REG);
	
	_dev_info(&(pdev->dev),"sunxi_rtc_probe tmp_data = %d\n", tmp_data);
	
	/*step2: check set result,��ѯ�Ƿ����óɹ�*/
	tmp_data = readl(sunxi_rtc_base + SUNXI_LOSC_CTRL_REG);
	if(!(tmp_data & RTC_SOURCE_EXTERNAL)){		
		RTC_ERR("[RTC] WARNING: Rtc time will be wrong!!\n");
		losc_err_flag = 1;
	}

	device_init_wakeup(&pdev->dev, 1);
	/* register RTC and exit */
	rtc = rtc_device_register("rtc", &pdev->dev, &sunxi_rtcops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		dev_err(&pdev->dev, "cannot attach rtc\n");
		ret = PTR_ERR(rtc);
		goto err_out;
	}

#ifdef SUNXI_ALARM
	ret = request_irq(sunxi_rtc_alarmno, sunxi_rtc_alarmirq,
			  IRQF_DISABLED,  "sunxi-rtc alarm", rtc);
	if (ret) {
		dev_err(&pdev->dev, "IRQ%d error %d\n", sunxi_rtc_alarmno, ret);
		rtc_device_unregister(rtc);
		return ret;
	}
#endif

	platform_set_drvdata(pdev, rtc);

	/*clean the alarm count value*/
	writel(0x00000000, sunxi_rtc_base + SUNXI_RTC_ALARM_COUNTER_REG);//0x0020
	/*clean the alarm current value*/
	writel(0x00000000, sunxi_rtc_base + SUNXI_RTC_ALARM_CURRENT_REG);//0x0024
	/*disable the alarm0 when init*/
    writel(0x00000000, sunxi_rtc_base + SUNXI_ALARM_EN_REG);//0x0028
    /*disable the alarm0 irq*/
    writel(0x00000000, sunxi_rtc_base + SUNXI_ALARM_INT_CTRL_REG);//0x002c
    /*Clear pending count alarm*/
	writel(0x00000001, sunxi_rtc_base + SUNXI_ALARM_INT_STATUS_REG);//0x0030
	
	writel(0x00000000, sunxi_rtc_base + SUNXI_ALARM_CONFIG);

	return 0;
	
	err_out:
		return ret;
}

/*share the irq no. with timer2*/
static struct resource sunxi_rtc_resource[] = {
	[0] = {
		.start = SUNXI_IRQ_RALARM0,
		.end   = SUNXI_IRQ_RALARM0,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device sunxi_device_rtc = {
	.name		    = "sunxi-rtc",
	.id		        = -1,
	.num_resources	= ARRAY_SIZE(sunxi_rtc_resource),
	.resource	    = sunxi_rtc_resource,
};

static struct platform_driver sunxi_rtc_driver = {
	.probe		= sunxi_rtc_probe,
	.remove		= __exit_p(sunxi_rtc_remove),
	.driver		= {
		.name	= "sunxi-rtc",
		.owner	= THIS_MODULE,
	},
};

static int __init sunxi_rtc_init(void)
{
	platform_device_register(&sunxi_device_rtc);
	RTC_DBG("sunxi RTC version 0.1 \n");
	return platform_driver_register(&sunxi_rtc_driver);
}

static void __exit sunxi_rtc_exit(void)
{
	platform_driver_unregister(&sunxi_rtc_driver);
}

module_init(sunxi_rtc_init);
module_exit(sunxi_rtc_exit);

MODULE_DESCRIPTION("allwinnertech RTC Driver");
MODULE_AUTHOR("huangxin");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sunxi-rtc");
#endif

