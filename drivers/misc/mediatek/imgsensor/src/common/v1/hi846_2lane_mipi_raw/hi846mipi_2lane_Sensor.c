/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 HI846mipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
//#include <asm/system.h>
//#include <linux/xlog.h>
#include "kd_camera_typedef.h"
//#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "hi846mipi_2lane_Sensor.h"


#define PFX "HI846_camera_sensor"
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#define Hi846_MaxGain 16
#define HI846_OTP_OB 64

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#ifdef VENDOR_EDIT
#define DEVICE_VERSION_HI846    "hi846"
static kal_uint32 streaming_control(kal_bool enable);
//extern void register_imgsensor_deviceinfo(char *name, char *version, u8 module_id);
//static uint8_t deviceInfo_register_value = 0x00;
//extern unsigned char hi846_get_module_id(void);
#endif

static struct imgsensor_info_struct imgsensor_info = { 
	.sensor_id = HI846_2LANE_SENSOR_ID,
	.checksum_value = 0xdf4593fd,

	.pre = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength = 3800,				//record different mode's linelength
		.framelength = 2526,			//record different mode's framelength
		.startx =0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 1632,		//record different mode's width of grabwindow
		.grabwindow_height = 1224,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate =  144000000,//7200Mbps*2/10=720 000 000 *2/10=144000000
	},

	.cap = {
		.pclk = 288000000,
		.linelength = 3800,
		.framelength = 2526,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate =  288000000,//1440Mbps*2/10=1440 000 000 *2/10=288000000
	},
    
	.cap1 = {                            //capture for PIP 24fps relative information, capture1 mode must use same framelength, linelength with Capture mode for shutter calculate
        .pclk = 288000000,
		.linelength = 3800,	
		.framelength = 5052,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
		.mipi_pixel_rate =  288000000,//1440Mbps*2/10=1440 000 000 *2/10=288000000
    },
    
	.normal_video = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength = 3800,				//record different mode's linelength
		.framelength = 2526,			//record different mode's framelength
		.startx =0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 3264,		//record different mode's width of grabwindow
		.grabwindow_height = 2448,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate =  288000000,//1440Mbps*2/10=1440 000 000 *2/10=288000000
	},

	.hs_video = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength = 3800, 			//record different mode's linelength
		.framelength = 2526,			//record different mode's framelength
		.startx =0, 				//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 1632,		//record different mode's width of grabwindow
		.grabwindow_height = 1224,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate =	144000000,//720Mbps*2/10=720 000 000 *2/10=144000000

	},

    .slim_video = {
       .pclk = 288000000,
		.linelength = 3800,
		.framelength = 842,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 900,
		.mipi_pixel_rate =	144000000,//720Mbps*2/10=720 000 000 *2/10=144000000
    },
	.margin = 6,
	.min_shutter = 6,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0, // per-frame : 0 , No perframe : 1
	.ae_ispGain_delay_frame = 2,
    .ihdr_support = 0,      //1, support; 0,not support
    .ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num
	
	.cap_delay_frame = 3, 
	.pre_delay_frame = 3, 
	.video_delay_frame = 3,
    .hs_video_delay_frame = 3,    //enter high speed video  delay frame num
    .slim_video_delay_frame = 3,//enter slim video delay frame num
	
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = 1,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x42,0x40,0xff},//0x40   bst01 -> 0x42
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x0100,					//current shutter
	.gain = 0xe0,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_TRUE for in test pattern mode, KAL_FALSE for normal output
//	.enable_secure = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
    .ihdr_en = 0, 
	.i2c_write_id = 0x40,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{
 { 3264, 2448,   0,   0,   3264, 2448,   1632, 1224,   0, 0, 1632, 1224,   0, 0, 1632, 1224}, // previeow
 { 3264, 2448,   0,   0,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, // capture
 { 3264, 2448,   0,   0,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, // video
 { 3264, 2448,   0,   0,   3264, 2448,   1632, 1224,   0, 0, 1632, 1224,   0, 0, 1632, 1224}, //hight speed video
 { 3280, 2448,   0, 504,   3264, 1440,   1632,  720, 176, 0, 1280,  720,   0, 0, 1280, 720}};// slim video

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8),(char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}


static void set_dummy(void)
{
	pr_info("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x0006, imgsensor.frame_length & 0xFFFF );
	write_cmos_sensor(0x0008, imgsensor.line_length & 0xFFFF );
  
}	/*	set_dummy  */


static kal_uint32 return_sensor_id(void)
{
    return ((read_cmos_sensor(0x0F17) << 8) | read_cmos_sensor(0x0F16))+1;
}
static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	//kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	pr_info("framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);
   
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length; 
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.min_frame_length;
	//if (dummy_line < 0)
		//imgsensor.dummy_line = 0;
	//else
		//imgsensor.dummy_line = dummy_line;
	//imgsensor.frame_length = frame_length + imgsensor.dummy_line;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	{
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;

	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	//kal_uint32 frame_length = 0;
	   
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
	
	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	printk("pangfei shutter %d line %d\n",shutter,__LINE__);
	spin_lock(&imgsensor_drv_lock);

	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)		
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	if (imgsensor.autoflicker_en) { 
		realtime_fps = imgsensor.pclk * 10 / (imgsensor.line_length * imgsensor.frame_length);
		if(realtime_fps >= 297 && realtime_fps <= 305)
      set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
      set_max_framerate(146,0);	
		else{
      write_cmos_sensor(0x0006, imgsensor.frame_length);
		}
	} 
	else{
		// Extend frame length
    write_cmos_sensor(0x0006, imgsensor.frame_length);
  }

	// Update Shutter
	// Update Shutter
	write_cmos_sensor_8(0x0073, ((shutter & 0xFF0000) >> 16));	  
	write_cmos_sensor(0x0074, shutter & 0x00FFFF);	  
	pr_info("shutter =%d, framelength =%d", shutter,imgsensor.frame_length);

}	/*	write_shutter  */

/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	pr_info("set_shutter");

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	
	write_shutter(shutter);
}	/*	set_shutter */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
  	kal_uint16 reg_gain = 0x0000;
	reg_gain = gain / 4 - 16;

	return (kal_uint16)reg_gain;
}
/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

    /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
    /* [0:3] = N meams N /16 X    */
    /* [4:9] = M meams M X         */
    /* Total gain = M + N /16 X   */

    if (gain < BASEGAIN || gain > Hi846_MaxGain * BASEGAIN) {
        pr_info("Error gain setting");

        if (gain < BASEGAIN)
            gain = BASEGAIN;
        else if (gain > Hi846_MaxGain * BASEGAIN)
            gain = Hi846_MaxGain * BASEGAIN;
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    pr_info("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_8(0x0077,reg_gain);

    return gain;
		
}	/*	set_gain  */

/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/ 
}	/*	night_mode	*/
static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0x0a00, 0x0100);
		mdelay(1);
	} else {
		write_cmos_sensor(0x0a00, 0x0000);
	}
	return ERROR_NONE;
}


static void sensor_init(void)
{
	//18-01-15
	//v16
	LOG_INF("Enter init v16\n");
	write_cmos_sensor(0x2000, 0x987A);
	write_cmos_sensor(0x2002, 0x00FF);
	write_cmos_sensor(0x2004, 0x0047);
	write_cmos_sensor(0x2006, 0x3FFF);
	write_cmos_sensor(0x2008, 0x3FFF);
	write_cmos_sensor(0x200A, 0xC216);
	write_cmos_sensor(0x200C, 0x1292);
	write_cmos_sensor(0x200E, 0xC01A);
	write_cmos_sensor(0x2010, 0x403D);
	write_cmos_sensor(0x2012, 0x000E);
	write_cmos_sensor(0x2014, 0x403E);
	write_cmos_sensor(0x2016, 0x0B80);
	write_cmos_sensor(0x2018, 0x403F);
	write_cmos_sensor(0x201A, 0x82AE);
	write_cmos_sensor(0x201C, 0x1292);
	write_cmos_sensor(0x201E, 0xC00C);
	write_cmos_sensor(0x2020, 0x4130);
	write_cmos_sensor(0x2022, 0x43E2);
	write_cmos_sensor(0x2024, 0x0180);
	write_cmos_sensor(0x2026, 0x4130);
	write_cmos_sensor(0x2028, 0x7400);
	write_cmos_sensor(0x202A, 0x5000);
	write_cmos_sensor(0x202C, 0x0253);
	write_cmos_sensor(0x202E, 0x0AD1);
	write_cmos_sensor(0x2030, 0x2360);
	write_cmos_sensor(0x2032, 0x0009);
	write_cmos_sensor(0x2034, 0x5020);
	write_cmos_sensor(0x2036, 0x000B);
	write_cmos_sensor(0x2038, 0x0002);
	write_cmos_sensor(0x203A, 0x0044);
	write_cmos_sensor(0x203C, 0x0016);
	write_cmos_sensor(0x203E, 0x1792);
	write_cmos_sensor(0x2040, 0x7002);
	write_cmos_sensor(0x2042, 0x154F);
	write_cmos_sensor(0x2044, 0x00D5);
	write_cmos_sensor(0x2046, 0x000B);
	write_cmos_sensor(0x2048, 0x0019);
	write_cmos_sensor(0x204A, 0x1698);
	write_cmos_sensor(0x204C, 0x000E);
	write_cmos_sensor(0x204E, 0x099A);
	write_cmos_sensor(0x2050, 0x0058);
	write_cmos_sensor(0x2052, 0x7000);
	write_cmos_sensor(0x2054, 0x1799);
	write_cmos_sensor(0x2056, 0x0310);
	write_cmos_sensor(0x2058, 0x03C3);
	write_cmos_sensor(0x205A, 0x004C);
	write_cmos_sensor(0x205C, 0x064A);
	write_cmos_sensor(0x205E, 0x0001);
	write_cmos_sensor(0x2060, 0x0007);
	write_cmos_sensor(0x2062, 0x0BC7);
	write_cmos_sensor(0x2064, 0x0055);
	write_cmos_sensor(0x2066, 0x7000);
	write_cmos_sensor(0x2068, 0x1550);
	write_cmos_sensor(0x206A, 0x158A);
	write_cmos_sensor(0x206C, 0x0004);
	write_cmos_sensor(0x206E, 0x1488);
	write_cmos_sensor(0x2070, 0x7010);
	write_cmos_sensor(0x2072, 0x1508);
	write_cmos_sensor(0x2074, 0x0004);
	write_cmos_sensor(0x2076, 0x0016);
	write_cmos_sensor(0x2078, 0x03D5);
	write_cmos_sensor(0x207A, 0x0055);
	write_cmos_sensor(0x207C, 0x08CA);
	write_cmos_sensor(0x207E, 0x2019);
	write_cmos_sensor(0x2080, 0x0007);
	write_cmos_sensor(0x2082, 0x7057);
	write_cmos_sensor(0x2084, 0x0FC7);
	write_cmos_sensor(0x2086, 0x5041);
	write_cmos_sensor(0x2088, 0x12C8);
	write_cmos_sensor(0x208A, 0x5060);
	write_cmos_sensor(0x208C, 0x5080);
	write_cmos_sensor(0x208E, 0x2084);
	write_cmos_sensor(0x2090, 0x12C8);
	write_cmos_sensor(0x2092, 0x7800);
	write_cmos_sensor(0x2094, 0x0802);
	write_cmos_sensor(0x2096, 0x040F);
	write_cmos_sensor(0x2098, 0x1007);
	write_cmos_sensor(0x209A, 0x0803);
	write_cmos_sensor(0x209C, 0x080B);
	write_cmos_sensor(0x209E, 0x3803);
	write_cmos_sensor(0x20A0, 0x0807);
	write_cmos_sensor(0x20A2, 0x0404);
	write_cmos_sensor(0x20A4, 0x0400);
	write_cmos_sensor(0x20A6, 0xFFFF);
	write_cmos_sensor(0x20A8, 0xF0B2);
	write_cmos_sensor(0x20AA, 0xFFEF);
	write_cmos_sensor(0x20AC, 0x0A84);
	write_cmos_sensor(0x20AE, 0x1292);
	write_cmos_sensor(0x20B0, 0xC02E);
	write_cmos_sensor(0x20B2, 0x4130);
	write_cmos_sensor(0x20B4, 0xF0B2);
	write_cmos_sensor(0x20B6, 0xFFBF);
	write_cmos_sensor(0x20B8, 0x2004);
	write_cmos_sensor(0x20BA, 0x403F);
	write_cmos_sensor(0x20BC, 0x00C3);
	write_cmos_sensor(0x20BE, 0x4FE2);
	write_cmos_sensor(0x20C0, 0x8318);
	write_cmos_sensor(0x20C2, 0x43CF);
	write_cmos_sensor(0x20C4, 0x0000);
	write_cmos_sensor(0x20C6, 0x9382);
	write_cmos_sensor(0x20C8, 0xC314);
	write_cmos_sensor(0x20CA, 0x2003);
	write_cmos_sensor(0x20CC, 0x12B0);
	write_cmos_sensor(0x20CE, 0xCAB0);
	write_cmos_sensor(0x20D0, 0x4130);
	write_cmos_sensor(0x20D2, 0x12B0);
	write_cmos_sensor(0x20D4, 0xC90A);
	write_cmos_sensor(0x20D6, 0x4130);
	write_cmos_sensor(0x20D8, 0x42D2);
	write_cmos_sensor(0x20DA, 0x8318);
	write_cmos_sensor(0x20DC, 0x00C3);
	write_cmos_sensor(0x20DE, 0x9382);
	write_cmos_sensor(0x20E0, 0xC314);
	write_cmos_sensor(0x20E2, 0x2009);
	write_cmos_sensor(0x20E4, 0x120B);
	write_cmos_sensor(0x20E6, 0x120A);
	write_cmos_sensor(0x20E8, 0x1209);
	write_cmos_sensor(0x20EA, 0x1208);
	write_cmos_sensor(0x20EC, 0x1207);
	write_cmos_sensor(0x20EE, 0x1206);
	write_cmos_sensor(0x20F0, 0x4030);
	write_cmos_sensor(0x20F2, 0xC15E);
	write_cmos_sensor(0x20F4, 0x4130);
	write_cmos_sensor(0x20F6, 0x1292);
	write_cmos_sensor(0x20F8, 0xC008);
	write_cmos_sensor(0x20FA, 0x4130);
	write_cmos_sensor(0x20FC, 0x42D2);
	write_cmos_sensor(0x20FE, 0x82A1);
	write_cmos_sensor(0x2100, 0x00C2);
	write_cmos_sensor(0x2102, 0x1292);
	write_cmos_sensor(0x2104, 0xC040);
	write_cmos_sensor(0x2106, 0x4130);
	write_cmos_sensor(0x2108, 0x1292);
	write_cmos_sensor(0x210A, 0xC006);
	write_cmos_sensor(0x210C, 0x42A2);
	write_cmos_sensor(0x210E, 0x7324);
	write_cmos_sensor(0x2110, 0x9382);
	write_cmos_sensor(0x2112, 0xC314);
	write_cmos_sensor(0x2114, 0x2011);
	write_cmos_sensor(0x2116, 0x425F);
	write_cmos_sensor(0x2118, 0x82A1);
	write_cmos_sensor(0x211A, 0xF25F);
	write_cmos_sensor(0x211C, 0x00C1);
	write_cmos_sensor(0x211E, 0xF35F);
	write_cmos_sensor(0x2120, 0x2406);
	write_cmos_sensor(0x2122, 0x425F);
	write_cmos_sensor(0x2124, 0x00C0);
	write_cmos_sensor(0x2126, 0xF37F);
	write_cmos_sensor(0x2128, 0x522F);
	write_cmos_sensor(0x212A, 0x4F82);
	write_cmos_sensor(0x212C, 0x7324);
	write_cmos_sensor(0x212E, 0x425F);
	write_cmos_sensor(0x2130, 0x82D4);
	write_cmos_sensor(0x2132, 0xF35F);
	write_cmos_sensor(0x2134, 0x4FC2);
	write_cmos_sensor(0x2136, 0x01B3);
	write_cmos_sensor(0x2138, 0x93C2);
	write_cmos_sensor(0x213A, 0x829F);
	write_cmos_sensor(0x213C, 0x2421);
	write_cmos_sensor(0x213E, 0x403E);
	write_cmos_sensor(0x2140, 0xFFFE);
	write_cmos_sensor(0x2142, 0x40B2);
	write_cmos_sensor(0x2144, 0xEC78);
	write_cmos_sensor(0x2146, 0x831C);
	write_cmos_sensor(0x2148, 0x40B2);
	write_cmos_sensor(0x214A, 0xEC78);
	write_cmos_sensor(0x214C, 0x831E);
	write_cmos_sensor(0x214E, 0x40B2);
	write_cmos_sensor(0x2150, 0xEC78);
	write_cmos_sensor(0x2152, 0x8320);
	write_cmos_sensor(0x2154, 0xB3D2);
	write_cmos_sensor(0x2156, 0x008C);
	write_cmos_sensor(0x2158, 0x2405);
	write_cmos_sensor(0x215A, 0x4E0F);
	write_cmos_sensor(0x215C, 0x503F);
	write_cmos_sensor(0x215E, 0xFFD8);
	write_cmos_sensor(0x2160, 0x4F82);
	write_cmos_sensor(0x2162, 0x831C);
	write_cmos_sensor(0x2164, 0x90F2);
	write_cmos_sensor(0x2166, 0x0003);
	write_cmos_sensor(0x2168, 0x008C);
	write_cmos_sensor(0x216A, 0x2401);
	write_cmos_sensor(0x216C, 0x4130);
	write_cmos_sensor(0x216E, 0x421F);
	write_cmos_sensor(0x2170, 0x831C);
	write_cmos_sensor(0x2172, 0x5E0F);
	write_cmos_sensor(0x2174, 0x4F82);
	write_cmos_sensor(0x2176, 0x831E);
	write_cmos_sensor(0x2178, 0x5E0F);
	write_cmos_sensor(0x217A, 0x4F82);
	write_cmos_sensor(0x217C, 0x8320);
	write_cmos_sensor(0x217E, 0x3FF6);
	write_cmos_sensor(0x2180, 0x432E);
	write_cmos_sensor(0x2182, 0x3FDF);
	write_cmos_sensor(0x2184, 0x421F);
	write_cmos_sensor(0x2186, 0x7100);
	write_cmos_sensor(0x2188, 0x4F0E);
	write_cmos_sensor(0x218A, 0x503E);
	write_cmos_sensor(0x218C, 0xFFD8);
	write_cmos_sensor(0x218E, 0x4E82);
	write_cmos_sensor(0x2190, 0x7A04);
	write_cmos_sensor(0x2192, 0x421E);
	write_cmos_sensor(0x2194, 0x831C);
	write_cmos_sensor(0x2196, 0x5F0E);
	write_cmos_sensor(0x2198, 0x4E82);
	write_cmos_sensor(0x219A, 0x7A06);
	write_cmos_sensor(0x219C, 0x0B00);
	write_cmos_sensor(0x219E, 0x7304);
	write_cmos_sensor(0x21A0, 0x0050);
	write_cmos_sensor(0x21A2, 0x40B2);
	write_cmos_sensor(0x21A4, 0xD081);
	write_cmos_sensor(0x21A6, 0x0B88);
	write_cmos_sensor(0x21A8, 0x421E);
	write_cmos_sensor(0x21AA, 0x831E);
	write_cmos_sensor(0x21AC, 0x5F0E);
	write_cmos_sensor(0x21AE, 0x4E82);
	write_cmos_sensor(0x21B0, 0x7A0E);
	write_cmos_sensor(0x21B2, 0x521F);
	write_cmos_sensor(0x21B4, 0x8320);
	write_cmos_sensor(0x21B6, 0x4F82);
	write_cmos_sensor(0x21B8, 0x7A10);
	write_cmos_sensor(0x21BA, 0x0B00);
	write_cmos_sensor(0x21BC, 0x7304);
	write_cmos_sensor(0x21BE, 0x007A);
	write_cmos_sensor(0x21C0, 0x40B2);
	write_cmos_sensor(0x21C2, 0x0081);
	write_cmos_sensor(0x21C4, 0x0B88);
	write_cmos_sensor(0x21C6, 0x4392);
	write_cmos_sensor(0x21C8, 0x7A0A);
	write_cmos_sensor(0x21CA, 0x0800);
	write_cmos_sensor(0x21CC, 0x7A0C);
	write_cmos_sensor(0x21CE, 0x0B00);
	write_cmos_sensor(0x21D0, 0x7304);
	write_cmos_sensor(0x21D2, 0x022B);
	write_cmos_sensor(0x21D4, 0x40B2);
	write_cmos_sensor(0x21D6, 0xD081);
	write_cmos_sensor(0x21D8, 0x0B88);
	write_cmos_sensor(0x21DA, 0x0B00);
	write_cmos_sensor(0x21DC, 0x7304);
	write_cmos_sensor(0x21DE, 0x0255);
	write_cmos_sensor(0x21E0, 0x40B2);
	write_cmos_sensor(0x21E2, 0x0081);
	write_cmos_sensor(0x21E4, 0x0B88);
	write_cmos_sensor(0x21E6, 0x4130);
	write_cmos_sensor(0x23FE, 0xC056);
	write_cmos_sensor(0x3232, 0xFC0C);
	write_cmos_sensor(0x3236, 0xFC22);
	write_cmos_sensor(0x3238, 0xFCFC);
	write_cmos_sensor(0x323A, 0xFD84);
	write_cmos_sensor(0x323C, 0xFD08);
	write_cmos_sensor(0x3246, 0xFCD8);
	write_cmos_sensor(0x3248, 0xFCA8);
	write_cmos_sensor(0x324E, 0xFCB4);
	write_cmos_sensor(0x326A, 0x8302);
	write_cmos_sensor(0x326C, 0x830A);
	write_cmos_sensor(0x326E, 0x0000);
	write_cmos_sensor(0x32CA, 0xFC28);
	write_cmos_sensor(0x32CC, 0xC3BC);
	write_cmos_sensor(0x32CE, 0xC34C);
	write_cmos_sensor(0x32D0, 0xC35A);
	write_cmos_sensor(0x32D2, 0xC368);
	write_cmos_sensor(0x32D4, 0xC376);
	write_cmos_sensor(0x32D6, 0xC3C2);
	write_cmos_sensor(0x32D8, 0xC3E6);
	write_cmos_sensor(0x32DA, 0x0003);
	write_cmos_sensor(0x32DC, 0x0003);
	write_cmos_sensor(0x32DE, 0x00C7);
	write_cmos_sensor(0x32E0, 0x0031);
	write_cmos_sensor(0x32E2, 0x0031);
	write_cmos_sensor(0x32E4, 0x0031);
	write_cmos_sensor(0x32E6, 0xFC28);
	write_cmos_sensor(0x32E8, 0xC3BC);
	write_cmos_sensor(0x32EA, 0xC384);
	write_cmos_sensor(0x32EC, 0xC392);
	write_cmos_sensor(0x32EE, 0xC3A0);
	write_cmos_sensor(0x32F0, 0xC3AE);
	write_cmos_sensor(0x32F2, 0xC3C4);
	write_cmos_sensor(0x32F4, 0xC3E6);
	write_cmos_sensor(0x32F6, 0x0003);
	write_cmos_sensor(0x32F8, 0x0003);
	write_cmos_sensor(0x32FA, 0x00C7);
	write_cmos_sensor(0x32FC, 0x0031);
	write_cmos_sensor(0x32FE, 0x0031);
	write_cmos_sensor(0x3300, 0x0031);
	write_cmos_sensor(0x3302, 0x82CA);
	write_cmos_sensor(0x3304, 0xC164);
	write_cmos_sensor(0x3306, 0x82E6);
	write_cmos_sensor(0x3308, 0xC19C);
	write_cmos_sensor(0x330A, 0x001F);
	write_cmos_sensor(0x330C, 0x001A);
	write_cmos_sensor(0x330E, 0x0034);
	write_cmos_sensor(0x3310, 0x0000);
	write_cmos_sensor(0x3312, 0x0000);
	write_cmos_sensor(0x3314, 0xFC94);
	write_cmos_sensor(0x3316, 0xC3D8);

	write_cmos_sensor(0x0A00, 0x0000);
	write_cmos_sensor(0x0E04, 0x0012);
	write_cmos_sensor(0x002E, 0x1111);
	write_cmos_sensor(0x0032, 0x1111);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0026, 0x0040);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x002C, 0x09CF);
	write_cmos_sensor(0x005C, 0x2101);
	write_cmos_sensor(0x0006, 0x09DE);
	write_cmos_sensor(0x0008, 0x0ED8);
	write_cmos_sensor(0x000E, 0x0100);
	write_cmos_sensor(0x000C, 0x0022);
	write_cmos_sensor(0x0A22, 0x0000);
	write_cmos_sensor(0x0A24, 0x0000);
	write_cmos_sensor(0x0804, 0x0000);
	write_cmos_sensor(0x0A12, 0x0CC0);
	write_cmos_sensor(0x0A14, 0x0990);
	write_cmos_sensor(0x0074, 0x09DE);
	write_cmos_sensor(0x0076, 0x0000);
	write_cmos_sensor(0x051E, 0x0000);
	write_cmos_sensor(0x0200, 0x0400);
	write_cmos_sensor(0x0A1A, 0x0C00);
	write_cmos_sensor(0x0A0C, 0x0010);
	write_cmos_sensor(0x0A1E, 0x0CCF);
	write_cmos_sensor(0x0402, 0x0110);
	write_cmos_sensor(0x0404, 0x00F4);
	write_cmos_sensor(0x0408, 0x0000);
	write_cmos_sensor(0x0410, 0x008D);
	write_cmos_sensor(0x0412, 0x011A);
	write_cmos_sensor(0x0414, 0x864C);
	/////////////ADD For OTP//////////////////
	write_cmos_sensor(0x021C, 0x0001);
//	write_cmos_sensor(0x021E, 0x0235);
	/////////////ADD For OTP//////////////////
	write_cmos_sensor(0x0C00, 0x9950);
	write_cmos_sensor(0x0C06, 0x0021);
	write_cmos_sensor(0x0C10, 0x0040);
	write_cmos_sensor(0x0C12, 0x0040);
	write_cmos_sensor(0x0C14, 0x0040);
	write_cmos_sensor(0x0C16, 0x0040);
	write_cmos_sensor(0x0A02, 0x0100);
	write_cmos_sensor(0x0A04, 0x014A);
	write_cmos_sensor(0x0418, 0x0000);
//	write_cmos_sensor(0x0128, 0x0028);
	write_cmos_sensor(0x012A, 0xFFFF);
	write_cmos_sensor(0x0120, 0x0046);
	write_cmos_sensor(0x0122, 0x0376);
//	write_cmos_sensor(0x012C, 0x0020);
//	write_cmos_sensor(0x012E, 0xFFFF);
//	write_cmos_sensor(0x0124, 0x0040);
//	write_cmos_sensor(0x0126, 0x0378);
	write_cmos_sensor(0x0746, 0x0050);
	write_cmos_sensor(0x0748, 0x01D5);
	write_cmos_sensor(0x074A, 0x022B);
	write_cmos_sensor(0x074C, 0x03B0);
	write_cmos_sensor(0x0756, 0x043F);
	write_cmos_sensor(0x0758, 0x3F1D);
	write_cmos_sensor(0x0B02, 0xE04D);
	write_cmos_sensor(0x0B10, 0x6821);
	write_cmos_sensor(0x0B12, 0x0120);
	write_cmos_sensor(0x0B14, 0x0001);
	write_cmos_sensor(0x2008, 0x38FD);
	write_cmos_sensor(0x326E, 0x0000);
	write_cmos_sensor(0x0900, 0x0320);
	write_cmos_sensor(0x0902, 0xC31A);
	write_cmos_sensor(0x0914, 0xC109);
	write_cmos_sensor(0x0916, 0x061A);
	write_cmos_sensor(0x0918, 0x0306);
	write_cmos_sensor(0x091A, 0x0B09);
	write_cmos_sensor(0x091C, 0x0C07);
	write_cmos_sensor(0x091E, 0x0A00);
	write_cmos_sensor(0x090C, 0x042A);
	write_cmos_sensor(0x090E, 0x006B);
	write_cmos_sensor(0x0954, 0x0089);
	write_cmos_sensor(0x0956, 0x0000);
	write_cmos_sensor(0x0958, 0xCA00);
	write_cmos_sensor(0x095A, 0x9240);
	write_cmos_sensor(0x0F08, 0x2F04);
	write_cmos_sensor(0x0F30, 0x001F);
	write_cmos_sensor(0x0F36, 0x001F);
	write_cmos_sensor(0x0F04, 0x3A00);
	write_cmos_sensor(0x0F32, 0x025A);
	write_cmos_sensor(0x0F38, 0x025A);
	write_cmos_sensor(0x0F2A, 0x0024);
	write_cmos_sensor(0x006A, 0x0100);
	write_cmos_sensor(0x004C, 0x0100);
//	write_cmos_sensor(0x0044, 0x0001);
//	write_cmos_sensor(0x0A00, 0x0100);
}

static void preview_setting(void)
{
	pr_info("E preview\n"); 
	//Sensor Information////////////////////////////
	//Sensor	  : Hi-846
	//Date		  : 2017-01-04                        
	//Customer        : MTK_validation
	//Image size	  : 1632x1224(BIN2)
	//MCLK/PCLK	  : 24MHz /288Mhz
	//MIPI speed(Mbps): 7200Mbps x 2Lane
	//Frame Length	  : 2526
	//Line Length 	  : 3800
	//Max Fps 	  : 30.00fps
	//Pixel order 	  : Green 1st (=GB)
	//X/Y-flip        : X-flip
	//BLC offset	    : 64code
	//Firmware Ver.   : v16
	////////////////////////////////////////////////
	//write_cmos_sensor(0x0A00, 0x0000);
	write_cmos_sensor(0x002E, 0x3311);
	write_cmos_sensor(0x0032, 0x3311);
	write_cmos_sensor(0x0026, 0x0040);
	write_cmos_sensor(0x002C, 0x09CF);
	write_cmos_sensor(0x005C, 0x4202);
	write_cmos_sensor(0x0006, 0x09DE);
	write_cmos_sensor(0x0008, 0x0ED8);
	write_cmos_sensor(0x000C, 0x0122);
	write_cmos_sensor(0x0A22, 0x0100);
	write_cmos_sensor(0x0A24, 0x0000);
	write_cmos_sensor(0x0804, 0x0000);
	write_cmos_sensor(0x0A12, 0x0660);
	write_cmos_sensor(0x0A14, 0x04C8);
	write_cmos_sensor(0x0074, 0x09D8);
	write_cmos_sensor(0x0A04, 0x016A);
	/////////////ADD For OTP//////////////////
//	write_cmos_sensor(0x021C, 0x0003);
///	write_cmos_sensor(0x021E, 0x0235);
	/////////////ADD For OTP//////////////////
	write_cmos_sensor(0x0418, 0x0000);
	write_cmos_sensor(0x0B02, 0xE04D);
	write_cmos_sensor(0x0B10, 0x6C21);
	write_cmos_sensor(0x0B12, 0x0120);
	write_cmos_sensor(0x0B14, 0x0005);
	write_cmos_sensor(0x2008, 0x38FD);
	write_cmos_sensor(0x326E, 0x0000);
	//=============================================//
	//      MIPI 2lane 720Mbps
	//=============================================//
	write_cmos_sensor(0x0900, 0x0300);
	write_cmos_sensor(0x0902, 0x4319);
	write_cmos_sensor(0x0914, 0xC109);
	write_cmos_sensor(0x0916, 0x061A);
	write_cmos_sensor(0x0918, 0x0407);
	write_cmos_sensor(0x091A, 0x0A0B);
	write_cmos_sensor(0x091C, 0x0E08);
	write_cmos_sensor(0x091E, 0x0A00);
	write_cmos_sensor(0x090C, 0x0427);
	write_cmos_sensor(0x090E, 0x0069);
	write_cmos_sensor(0x0954, 0x0089);
	write_cmos_sensor(0x0956, 0x0000);
	write_cmos_sensor(0x0958, 0xCA80);
	write_cmos_sensor(0x095A, 0x9240);
	write_cmos_sensor(0x0F2A, 0x4124);
	write_cmos_sensor(0x004C, 0x0100);
//	write_cmos_sensor(0x0A00, 0x0100);
}


static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E capture, currefps = %d\n",currefps);
	if( currefps == 300 )
	{
		//Sensor Information////////////////////////////
		//Sensor	  : Hi-846
		//Date		  : 2018-01-15
		//Customer        : MTK_validation
		//Image size	  : 3264x2448
		//MCLK/PCLK	  : 24MHz /288Mhz
		//MIPI speed(Mbps): 1440Mbps x 2Lane
		//Frame Length	  : 2526
		//Line Length 	  : 3800
		//Max Fps 	  : 30.00fps
		//Pixel order 	  : Green 1st (=GB)
		//X/Y-flip        : X-flip
		//BLC offset	    : 64code
		//Firmware Ver.   : v16
		////////////////////////////////////////////////
		//write_cmos_sensor(0x0A00, 0x0000);
		write_cmos_sensor(0x002E, 0x1111);
		write_cmos_sensor(0x0032, 0x1111);
		write_cmos_sensor(0x0026, 0x0040);
		write_cmos_sensor(0x002C, 0x09CF);
		write_cmos_sensor(0x005C, 0x2101);
		write_cmos_sensor(0x0006, 0x09DE);
		write_cmos_sensor(0x0008, 0x0ED8);
		write_cmos_sensor(0x000C, 0x0022);
		write_cmos_sensor(0x0A22, 0x0000);
		write_cmos_sensor(0x0A24, 0x0000);
		write_cmos_sensor(0x0804, 0x0000);
		write_cmos_sensor(0x0A12, 0x0CC0);
		write_cmos_sensor(0x0A14, 0x0990);
		write_cmos_sensor(0x0074, 0x09D8);
		/////////////ADD For OTP//////////////////
//		write_cmos_sensor(0x021C, 0x0003);
//		write_cmos_sensor(0x021E, 0x0235);
		/////////////ADD For OTP//////////////////
		write_cmos_sensor(0x0A04, 0x014A);
		write_cmos_sensor(0x0418, 0x0000);
		write_cmos_sensor(0x0B02, 0xE04D);
		write_cmos_sensor(0x0B10, 0x6821);
		write_cmos_sensor(0x0B12, 0x0120);
		write_cmos_sensor(0x0B14, 0x0001);
		write_cmos_sensor(0x2008, 0x38FD);
		write_cmos_sensor(0x326E, 0x0000);
		//=============================================//
		//      MIPI 2lane 1440Mbps
		//=============================================//
		write_cmos_sensor(0x0900, 0x0320);
		write_cmos_sensor(0x0902, 0xC31A);
		write_cmos_sensor(0x0914, 0xC109);
		write_cmos_sensor(0x0916, 0x061A);
		write_cmos_sensor(0x0918, 0x0306);
		write_cmos_sensor(0x091A, 0x0B09);
		write_cmos_sensor(0x091C, 0x0C07);
		write_cmos_sensor(0x091E, 0x0A00);
		write_cmos_sensor(0x090C, 0x042A);
		write_cmos_sensor(0x090E, 0x006B);
		write_cmos_sensor(0x0954, 0x0089);
		write_cmos_sensor(0x0956, 0x0000);
		write_cmos_sensor(0x0958, 0xCA00);
		write_cmos_sensor(0x095A, 0x9240);
		write_cmos_sensor(0x0F2A, 0x0024);
		write_cmos_sensor(0x004C, 0x0100);
//		write_cmos_sensor(0x0A00, 0x0100);
	} else {
		//Sensor Information////////////////////////////
		//Sensor	  : Hi-846
		//Date		  : 2018-01-15
		//Customer        : MTK_validation
		//Image size	  : 3264x2448
		//MCLK/PCLK	  : 24MHz /288Mhz
		//MIPI speed(Mbps): 1440Mbps x 2Lane
		//Frame Length	  : 5052
		//Line Length 	  : 3800
		//Max Fps 	  : 15.0fps
		//Pixel order 	  : Green 1st (=GB)
		//X/Y-flip        : X-flip
		//BLC offset	    : 64code
		//Firmware Ver.   : v16
		////////////////////////////////////////////////
		//write_cmos_sensor(0x0A00, 0x0000);
		write_cmos_sensor(0x002E, 0x1111);
		write_cmos_sensor(0x0032, 0x1111);
		write_cmos_sensor(0x0026, 0x0040);
		write_cmos_sensor(0x002C, 0x09CF);
		write_cmos_sensor(0x005C, 0x2101);
		write_cmos_sensor(0x0006, 0x13BC);
		write_cmos_sensor(0x0008, 0x0ED8);
		write_cmos_sensor(0x000C, 0x0022);
		write_cmos_sensor(0x0A22, 0x0000);
		write_cmos_sensor(0x0A24, 0x0000);
		write_cmos_sensor(0x0804, 0x0000);
		write_cmos_sensor(0x0A12, 0x0CC0);
		write_cmos_sensor(0x0A14, 0x0990);
		write_cmos_sensor(0x0074, 0x13B6);
		/////////////ADD For OTP//////////////////
//		write_cmos_sensor(0x021C, 0x0003);
//		write_cmos_sensor(0x021E, 0x0235);
		/////////////ADD For OTP//////////////////
		write_cmos_sensor(0x0A04, 0x014A);
		write_cmos_sensor(0x0418, 0x0000);
		write_cmos_sensor(0x0B02, 0xE04D);
		write_cmos_sensor(0x0B10, 0x6821);
		write_cmos_sensor(0x0B12, 0x0120);
		write_cmos_sensor(0x0B14, 0x0001);
		write_cmos_sensor(0x2008, 0x38FD);
		write_cmos_sensor(0x326E, 0x0000);
		//=============================================//
		//      MIPI 2lane 1440Mbps
		//=============================================//
		write_cmos_sensor(0x0900, 0x0320);
		write_cmos_sensor(0x0902, 0xC31A);
		write_cmos_sensor(0x0914, 0xC109);
		write_cmos_sensor(0x0916, 0x061A);
		write_cmos_sensor(0x0918, 0x0306);
		write_cmos_sensor(0x091A, 0x0B09);
		write_cmos_sensor(0x091C, 0x0C07);
		write_cmos_sensor(0x091E, 0x0A00);
		write_cmos_sensor(0x090C, 0x042A);
		write_cmos_sensor(0x090E, 0x006B);
		write_cmos_sensor(0x0954, 0x0089);
		write_cmos_sensor(0x0956, 0x0000);
		write_cmos_sensor(0x0958, 0xCA00);
		write_cmos_sensor(0x095A, 0x9240);
		write_cmos_sensor(0x0F2A, 0x0024);
		write_cmos_sensor(0x004C, 0x0100);
//		write_cmos_sensor(0x0A00, 0x0100);
	}
}
static void hs_video_setting(void)
{
	LOG_INF("E hs_video_setting\n");
	//Sensor Information////////////////////////////
	//Sensor	  : Hi-846
	//Date		  : 2018-01-15
	//Customer        : MTK_validation
	//Image size	  : 1632x1224(BIN2)
	//MCLK/PCLK	  : 24MHz /288Mhz
	//MIPI speed(Mbps): 720Mbps x 2Lane
	//Frame Length	  : 2526
	//Line Length 	  : 3800
	//Max Fps 	  : 30.00fps
	//Pixel order 	  : Green 1st (=GB)
	//X/Y-flip        : X-flip
	//BLC offset	    : 64code
	//Firmware Ver.   : v16
	////////////////////////////////////////////////
	//write_cmos_sensor(0x0A00, 0x0000);
	write_cmos_sensor(0x002E, 0x3311);
	write_cmos_sensor(0x0032, 0x3311);
	write_cmos_sensor(0x0026, 0x0040);
	write_cmos_sensor(0x002C, 0x09CF);
	write_cmos_sensor(0x005C, 0x4202);
	write_cmos_sensor(0x0006, 0x09DE);
	write_cmos_sensor(0x0008, 0x0ED8);
	write_cmos_sensor(0x000C, 0x0122);
	write_cmos_sensor(0x0A22, 0x0100);
	write_cmos_sensor(0x0A24, 0x0000);
	write_cmos_sensor(0x0804, 0x0000);
	write_cmos_sensor(0x0A12, 0x0660);
	write_cmos_sensor(0x0A14, 0x04C8);
	write_cmos_sensor(0x0074, 0x09D8);
	write_cmos_sensor(0x0A04, 0x016A);
	/////////////ADD For OTP//////////////////
//	write_cmos_sensor(0x021C, 0x0003);
//	write_cmos_sensor(0x021E, 0x0235);
	/////////////ADD For OTP//////////////////
	write_cmos_sensor(0x0418, 0x0000);
	write_cmos_sensor(0x0B02, 0xE04D);
	write_cmos_sensor(0x0B10, 0x6C21);
	write_cmos_sensor(0x0B12, 0x0120);
	write_cmos_sensor(0x0B14, 0x0005);
	write_cmos_sensor(0x2008, 0x38FD);
	write_cmos_sensor(0x326E, 0x0000);
	//=============================================//
	//      MIPI 2lane 720Mbps
	//=============================================//
	write_cmos_sensor(0x0900, 0x0300);
	write_cmos_sensor(0x0902, 0x4319);
	write_cmos_sensor(0x0914, 0xC109);
	write_cmos_sensor(0x0916, 0x061A);
	write_cmos_sensor(0x0918, 0x0407);
	write_cmos_sensor(0x091A, 0x0A0B);
	write_cmos_sensor(0x091C, 0x0E08);
	write_cmos_sensor(0x091E, 0x0A00);
	write_cmos_sensor(0x090C, 0x0427);
	write_cmos_sensor(0x090E, 0x0069);
	write_cmos_sensor(0x0954, 0x0089);
	write_cmos_sensor(0x0956, 0x0000);
	write_cmos_sensor(0x0958, 0xCA80);
	write_cmos_sensor(0x095A, 0x9240);
	write_cmos_sensor(0x0F2A, 0x4124);
	write_cmos_sensor(0x004C, 0x0100);
//	write_cmos_sensor(0x0A00, 0x0100);

};
static void slim_video_setting(void)
{
	//Sensor Information////////////////////////////
	//Sensor	  : Hi-846
	//Date		  : 2018-01-15
	//Customer        : MTK_validation
	//Image size	  : 1280x720
	//MCLK/PCLK	  : 24MHz /288Mhz
	//MIPI speed(Mbps): 720Mbps x 2Lane
	//Frame Length	  :  842
	//Line Length 	  : 3800
	//Max Fps 	  : 90.00fps
	//Pixel order 	  : Green 1st (=GB)
	//X/Y-flip        : X-flip
	//BLC offset	    : 64code
	//Firmware Ver.   : v16
	////////////////////////////////////////////////
	//write_cmos_sensor(0x0A00, 0x0000);
	write_cmos_sensor(0x002E, 0x3311);
	write_cmos_sensor(0x0032, 0x3311);
	write_cmos_sensor(0x0026, 0x0238);
	write_cmos_sensor(0x002C, 0x07D7);
	write_cmos_sensor(0x005C, 0x4202);
	write_cmos_sensor(0x0006, 0x034A);
	write_cmos_sensor(0x0008, 0x0ED8);
	write_cmos_sensor(0x000C, 0x0122);
	write_cmos_sensor(0x0A22, 0x0100);
	write_cmos_sensor(0x0A24, 0x0000);
	write_cmos_sensor(0x0804, 0x00B0);
	write_cmos_sensor(0x0A12, 0x0500);
	write_cmos_sensor(0x0A14, 0x02D0);
	write_cmos_sensor(0x0074, 0x0344);
	/////////////ADD For OTP//////////////////
//	write_cmos_sensor(0x021C, 0x0003);
//	write_cmos_sensor(0x021E, 0x0235);
	/////////////ADD For OTP//////////////////
	write_cmos_sensor(0x0A04, 0x016A);
	write_cmos_sensor(0x0418, 0x0410);
	write_cmos_sensor(0x0B02, 0xE04D);
	write_cmos_sensor(0x0B10, 0x6C21);
	write_cmos_sensor(0x0B12, 0x0120);
	write_cmos_sensor(0x0B14, 0x0005);
	write_cmos_sensor(0x2008, 0x38FD);
	write_cmos_sensor(0x326E, 0x0000);
	//=============================================//
	//      MIPI 2lane 720Mbps
	//=============================================//
	write_cmos_sensor(0x0900, 0x0300);
	write_cmos_sensor(0x0902, 0x4319);
	write_cmos_sensor(0x0914, 0xC109);
	write_cmos_sensor(0x0916, 0x061A);
	write_cmos_sensor(0x0918, 0x0407);
	write_cmos_sensor(0x091A, 0x0A0B);
	write_cmos_sensor(0x091C, 0x0E08);
	write_cmos_sensor(0x091E, 0x0A00);
	write_cmos_sensor(0x090C, 0x0427);
	write_cmos_sensor(0x090E, 0x0145);
	write_cmos_sensor(0x0954, 0x0089);
	write_cmos_sensor(0x0956, 0x0000);
	write_cmos_sensor(0x0958, 0xCA80);
	write_cmos_sensor(0x095A, 0x9240);
	write_cmos_sensor(0x0F2A, 0x4124);
	write_cmos_sensor(0x004C, 0x0100);
//	write_cmos_sensor(0x0A00, 0x0100);
};
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id) 
{    
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	pr_info("[get_imgsensor_id] ");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
			spin_lock(&imgsensor_drv_lock);
			imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
			spin_unlock(&imgsensor_drv_lock);
			do {
				*sensor_id =return_sensor_id();
				if (*sensor_id == imgsensor_info.sensor_id) {				
					pr_info("i2c write id  : 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);	  
					return ERROR_NONE;
				}	
				pr_info("get_imgsensor_id Read sensor id fail, i2c write id: 0x%x,sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
				retry--;
			} while(retry > 0);
			i++;
			retry = 2;
}
	if (*sensor_id != imgsensor_info.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*
static kal_uint32 OTP_EEPROM_Read(kal_uint32 uiStartAddr, kal_uint8* pwData, kal_uint32 uiDataAmount)
{
	kal_uint8 i = 0;
	kal_uint8 iicRtn[3] = {0};

	if(pwData == NULL ) {
		return ERROR_INVALID_PARA;
	}
	if(uiDataAmount == 0) {
		return ERROR_INVALID_PARA;
	}
	write_cmos_sensor_8(0x0A02, 0x01); //Fast sleep on
	write_cmos_sensor_8(0x0A00, 0x00); // stand by on
	mdelay(20);

	write_cmos_sensor_8(0x0F02, 0x00); // pll disable
	write_cmos_sensor_8(0x071A, 0x01); // CP TRIM_H
	write_cmos_sensor_8(0x071B, 0x09); // IPGM TRIM_H
	write_cmos_sensor_8(0x0D04, 0x00); // Fsync(OTP busy) Output Enable
	write_cmos_sensor_8(0x0D00, 0x07); // Fsync(OTP busy) Output Drivability
	write_cmos_sensor_8(0x003E, 0x10); // OTP R/W mode
	//m_lpUsbIicWrite(0x070F, 0x05, bRtn); // OTP data rewrite
	write_cmos_sensor_8(0x0A00, 0x01); // stand by off
	mdelay(1);
	//LOG_INF(" OTP_EEPROM_Read uiStartAddr = 0x%x uiDataAmount = %d\n", uiStartAddr,uiDataAmount);

	write_cmos_sensor_8(0x070A, (uiStartAddr>>8)&0xff); // start address H
	write_cmos_sensor_8(0x070B, uiStartAddr&0xff); // start address L
	write_cmos_sensor_8(0x0702, 0x01); // read mode

	for ( i = 0; i <= uiDataAmount; i++)
	{
		iicRtn[0] = read_cmos_sensor(0x0708);
		//LOG_INF(" OTP_EEPROM_Read Read Addr 0x%x iicRtn[0] = 0x%x\n", uiStartAddr+i,iicRtn[0]);
		pwData[i] = iicRtn[0];
	}

	write_cmos_sensor_8(0x0A00, 0x00); // stand by on
	mdelay(20);
	write_cmos_sensor_8(0x003E, 0x00); // display mode
	write_cmos_sensor_8(0x0A00, 0x01); // stand by off
	mdelay(1);
	return ERROR_NONE;

}

static void awb_lsc_update(void)
{
	kal_uint8 flag = 0;
	kal_uint8 flaginfo = 0;
	kal_uint8 flaglsc = 0;
	kal_uint32 uiStartAddr = 0;
	kal_uint8 cnt = 30;
	kal_uint8 awb_all_data[30] = {0};
	kal_uint8 i = 0;
	kal_uint32 sum = 0;

	kal_uint16 R_unit = 0;
	kal_uint16 Gr_unit = 0;
	kal_uint16 Gb_unit = 0;
	kal_uint16 B_unit = 0;

	kal_uint16 R_golden = 0;
	kal_uint16 Gr_golden = 0;
	kal_uint16 Gb_golden = 0;
	kal_uint16 B_golden = 0;

	kal_uint32 R_gain = 0x200;
	kal_uint32 G_gain = 0x200;
	kal_uint32 B_gain = 0x200;

	OTP_EEPROM_Read(0x0201, &flaginfo, 1);
	LOG_INF(" flaginfo = 0x%x", flaginfo);

	OTP_EEPROM_Read(0x0235, &flaglsc, 1);
	LOG_INF(" flaglsc = 0x%x", flaglsc);

	uiStartAddr = 0x0C5F;//0x0201;
	OTP_EEPROM_Read(uiStartAddr, &flag, 1);
	LOG_INF(" flag = 0x%x", flag);
	if (flag == 0x1) {
		uiStartAddr = 0x0C60;
	} else if (flag == 0x13) {
		uiStartAddr = 0x0C75;
	} else if (flag == 0x37) {
		uiStartAddr = 0x0C8A;
	}
	OTP_EEPROM_Read(uiStartAddr, awb_all_data, cnt);

	for (i = 0; i < 20; i++) {
		sum += awb_all_data[i];
		//LOG_INF(" awb_all_data[%d] = 0x%x sum = 0x%x \n",i,awb_all_data[i],sum);
	}
	if ( ((sum %255) + 1) == awb_all_data[20]) {
		LOG_INF(" awb read OK \n");
	} else {
		LOG_INF(" awb read fail");
	}

	R_unit  = (awb_all_data[1] << 8) | awb_all_data[0];
	Gr_unit = (awb_all_data[3] << 8) | awb_all_data[2];
	Gb_unit = (awb_all_data[5] << 8) | awb_all_data[4];
	B_unit  = (awb_all_data[7] << 8) | awb_all_data[6];
	LOG_INF(" R_unit = 0x%x Gr_unit = 0x%x Gb_unit = 0x%x B_unit = 0x%x   \n", R_unit, Gr_unit, Gb_unit, B_unit);

	//R_golden  = (awb_all_data[9] << 8) | awb_all_data[8];
	//Gr_golden = (awb_all_data[11] << 8) | awb_all_data[10];
	//Gb_golden = (awb_all_data[13] << 8) | awb_all_data[12];
	//B_golden  = (awb_all_data[15] << 8) | awb_all_data[14];

	R_golden  = 0x49;
	Gr_golden = 0x9a;
	Gb_golden = 0x9a;
	B_golden  = 0x5d;//#3478 module golden value (7BF810P1DA313036330100001923)
	LOG_INF(" R_golden = 0x%x Gr_golden = 0x%x Gb_golden = 0x%x B_golden = 0x%x \n", R_golden, Gr_golden, Gb_golden, B_golden);

	R_gain = 512 * R_golden * Gr_unit / Gr_golden / R_unit;
	B_gain = 512 * B_golden * Gb_unit / Gb_golden / B_unit; //Gr_unit=Gb_unit ,Gr_golden=Gb_golden
	LOG_INF(" R_gain = %d G_gain = %d B_gain = %d  \n", R_gain, G_gain, B_gain);

	if (R_gain < B_gain) {
		if (R_gain < 0x200) {
			B_gain = 0x200 * B_gain / R_gain;
			G_gain = 0x200 * G_gain / R_gain;
			R_gain = 0x200;
		}
	} else {
	 	if (B_gain < 0x200) {
			R_gain= 0x200 * R_gain / B_gain;
			G_gain= 0x200 * G_gain / B_gain;
			B_gain= 0x200;
		}
	}
	LOG_INF(" R_gain_after = %d G_gain_after = %d B_gain_after = %d  \n", R_gain, G_gain, B_gain);

	write_cmos_sensor_8(0x0078, (G_gain>>8));
	write_cmos_sensor_8(0x0079, (G_gain&0xff));
	write_cmos_sensor_8(0x007A, (G_gain>>8));
	write_cmos_sensor_8(0x007B, (G_gain&0xff));
	write_cmos_sensor_8(0x007C, (R_gain>>8));
	write_cmos_sensor_8(0x007D, (R_gain&0xff));
	write_cmos_sensor_8(0x007E, (B_gain>>8));
	write_cmos_sensor_8(0x007F, (B_gain&0xff));

	flag = read_cmos_sensor(0x0A05);
	LOG_INF(" flag 0A05 = 0x%x \n", flag);
	flag |= 0x10;
	write_cmos_sensor_8(0x0A05, flag);
	flag = read_cmos_sensor(0x0A05);
	LOG_INF(" flag22 0A05 = 0x%x  \n", flag);
}
*/
/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0; 
	pr_info("[open]: PLATFORM:MT6735,MIPI 4LANE\n");
	pr_info("preview 1632*1224@30fps,360Mbps/lane; capture 3264*2448@30fps,720Mbps/lane\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {				
				pr_info("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);	  
				break;
			}	
			pr_info("open:Read sensor id fail open i2c write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}		 
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;
	/* initail sequence write in  */

    sensor_init();


//	awb_lsc_update();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
    imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}	/*	open  */
static kal_uint32 close(void)
{
	pr_info("close E");
	/*No Need to implement this function*/ 
	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength; 
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();

	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

    if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) // 30fps
    {
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	else //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
    {  
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} 

	spin_unlock(&imgsensor_drv_lock);
	pr_info("Caputre fps:%d\n",imgsensor.current_fps);
	capture_setting(imgsensor.current_fps); 


	return ERROR_NONE;
    
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("E");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;  
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    //imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	//set_mirror_flip(sensor_config_data->SensorImageMirror);
    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();
	//set_mirror_flip(sensor_config_data->SensorImageMirror);

    return ERROR_NONE;
}    /*    slim_video     */






static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    pr_info("E\n");
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


    sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight     = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth     = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;
    return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("scenario_id = %d\n", scenario_id);


    //sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
    //sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
    //imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */

    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
    sensor_info->SensorHightSampling = 0;    // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

            sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

            break;
        default:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
    }

    return ERROR_NONE;
}    /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		
			pr_info("preview\n");
			preview(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_CAMERA_ZSD:
			capture(image_window, sensor_config_data);
			break;	
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;	  
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            hs_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            slim_video(image_window, sensor_config_data);
            break;	  
		default:
			pr_info("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */


static kal_uint32 set_video_mode(UINT16 framerate)
{
    pr_info("framerate = %d\n ", framerate);
    // SetVideoMode Function should fix framerate
    if (framerate == 0)
        // Dynamic frame rate
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 146;
    else
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps,1);
    set_dummy();
    return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	pr_info("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) 	  
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    pr_info("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            if(framerate == 0)
                return ERROR_NONE;
            frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        	  if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            } else {
        		    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                    pr_info("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            }
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
            imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
            break;
        default:  //coding with  preview scenario by default
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
            pr_info("error scenario_id = %d, we use preview scenario \n", scenario_id);
            break;
    }
    return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    pr_info("scenario_id = %d\n", scenario_id);

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            *framerate = imgsensor_info.pre.max_framerate;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *framerate = imgsensor_info.normal_video.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *framerate = imgsensor_info.cap.max_framerate;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *framerate = imgsensor_info.hs_video.max_framerate;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *framerate = imgsensor_info.slim_video.max_framerate;
            break;
        default:
            break;
    }

    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_info("enable: %d", enable);

	if (enable) { 
		  pr_info("enter color bar");            
		// 0x5E00[8]: 1 enable,  0 disable               
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK

		write_cmos_sensor(0x0A00, 0x0000);
		write_cmos_sensor(0x0a04, 0x0141);
		write_cmos_sensor(0x020a, 0x0200);

		write_cmos_sensor(0x0A00, 0x0100);
		mdelay(1);
				//write_cmos_sensor(0x021c, 0x0000);
			} else {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK

		write_cmos_sensor(0x0A00, 0x0000);
		write_cmos_sensor(0x0a04, 0x0140);
		write_cmos_sensor(0x020a, 0x0000);
		write_cmos_sensor(0x0A00, 0x0100);
		mdelay(1);
	}	 
	spin_lock(&imgsensor_drv_lock);
  imgsensor.test_pattern = enable;
  spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;
//    unsigned long long *feature_return_para=(unsigned long long *) feature_para;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    pr_info("feature_id = %d\n", feature_id);
    switch (feature_id) {
        case SENSOR_FEATURE_GET_PERIOD:
            *feature_return_para_16++ = imgsensor.line_length;
            *feature_return_para_16 = imgsensor.frame_length;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            *feature_return_para_32 = imgsensor.pclk;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            set_shutter(*feature_data);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            night_mode((BOOL) *feature_data);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            set_gain((UINT16) *feature_data);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode(*feature_data);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            get_imgsensor_id(feature_return_para_32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
            break;
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((BOOL)*feature_data);
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
            *feature_return_para_32 = imgsensor_info.checksum_value;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_FRAMERATE:
			LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = *feature_data_32;
			spin_unlock(&imgsensor_drv_lock);
			break;
			//case SENSOR_FEATURE_SET_HDR:
			//LOG_INF("ihdr enable :%d\n", *feature_data_16);
			//spin_lock(&imgsensor_drv_lock);
			//imgsensor.ihdr_en = *feature_data_16;
			//spin_unlock(&imgsensor_drv_lock);
			//break;
        case SENSOR_FEATURE_GET_CROP_INFO:
            pr_info("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);

            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

            switch (*feature_data_32) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
            }
        case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            pr_info("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            //ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            break;
		case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = imgsensor_info.cap.mipi_pixel_rate;
				break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate = imgsensor_info.normal_video.mipi_pixel_rate;
				break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate = imgsensor_info.slim_video.mipi_pixel_rate;
				break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;
		case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
			streaming_control(KAL_FALSE);
			break;
		case SENSOR_FEATURE_SET_STREAMING_RESUME:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
			if (*feature_data != 0) {
				set_shutter(*feature_data);
			}
			streaming_control(KAL_TRUE);
			break;
		default:
			break;
	}

    return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 HI846_2LANE_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	HI846_MIPI_RAW_SensorInit	*/
