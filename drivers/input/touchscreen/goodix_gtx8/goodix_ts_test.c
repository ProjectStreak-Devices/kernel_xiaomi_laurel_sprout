/*
 * gtx8_ts_test.c - TP test
 *
 * Copyright (C) 2015 - 2016 gtx8 Technology Incorporated
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright (C) 2015 - 2016 Yulong Cai <caiyulong@gtx8.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the gtx8's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/firmware.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include "goodix_ts_core.h"

#include <linux/fs.h>
/* test switch */
//#define RAWDATA_TEST_SPEC /* test 16 frames of rawdata */
//#define NOISE_DATA_TEST

//static u8 save_path[200] = "/sdcard/goodix_config.csv";
#define MAX_LINE_LEN                             1024*5

#define TOTAL_FRAME_NUM				 16
#define GTX8_RETRY_NUM_3			 3
#define GTX8_CONFIG_REFRESH_DATA		 0x01
#define PARAM_FILE_NAME				 "gtx8_limits.csv"
#define STRTOL_LEN				 10
#define STRTOL_HEX_LEN				 16

#define GTX8_TEST_FILE_NAME_LEN			 64
#define MAX_STR_LEN				 32

#define FLOAT_AMPLIFIER				 1000
#define MAX_U16_VALUE				 65535
#define RAWDATA_TEST_TIMES			 10

#define STATISTICS_DATA_LEN			 32
#define MAX_TEST_ITEMS				 10 /* 0P-1P-2P-3P-5P total test items */

#define GTP_CAP_TEST				 1
#define GTP_DELTA_TEST				 2
#define GTP_NOISE_TEST				 3
#define GTP_SHORT_TEST				 5
#define GTP_SELFCAP_TEST			 6
#define GTP_SELFNOISE_TEST			 7

#define GTP_TEST_PASS				 1
#define GTP_PANEL_REASON			 2
#define SYS_SOFTWARE_REASON			 3

/* error code */
#define NO_ERR					 0
#define RESULT_ERR				-1
#define RAWDATA_SIZE_LIMIT			-2

/*param key word in .csv */
#define CSV_TP_UNIFIED_LIMIT		"unified_raw_limit"
#define CSV_TP_NOISE_LIMIT		"noise_data_limit"
#define CSV_TP_SPECIAL_RAW_MIN		"specail_raw_min"
#define CSV_TP_SPECIAL_RAW_MAX		"specail_raw_max"
#define CSV_TP_SPECIAL_RAW_DELTA	"special_raw_delta"
#define CSV_TP_SHORT_THRESHOLD		"shortciurt_threshold"
#define CSV_TP_SELF_UNIFIED_LIMIT	"unified_selfraw_limit"
#define CSV_TP_SPECIAL_SELFRAW_MAX	"special_selfraw_max"
#define CSV_TP_SPECIAL_SELFRAW_MIN	"special_selfraw_min"
#define CSV_TP_SELFNOISE_LIMIT		"noise_selfdata_limit"
#define CSV_TP_TEST_CONFIG		"test_config"
#define CSV_TP_NOISE_CONFIG		"noise_config"

/*GTX8 CMD*/
#define GTX8_CMD_NORMAL				0x00
#define GTX8_CMD_RAWDATA			0x01
/* Regiter for rawdata test*/
#define GTP_RAWDATA_ADDR_9886			0x8FA0
#define GTP_NOISEDATA_ADDR_9886			0x9D20
#define GTP_BASEDATA_ADDR_9886			0xA980
#define GTP_SELF_RAWDATA_ADDR_9886		0x4C0C
#define GTP_SELF_NOISEDATA_ADDR_9886		0x4CA4

#define GTP_RAWDATA_ADDR_9883			0x7828
#define GTP_NOISEDATA_ADDR_9883			0x7C8C
#define GTP_BASEDATA_ADDR_9883			0x846C
#define GTP_SELF_RAWDATA_ADDR_9883		0x4A8C
#define GTP_SELF_NOISEDATA_ADDR_9883		0x4B00

#define GTP_RAWDATA_ADDR_6861			0x9078
#define GTP_NOISEDATA_ADDR_6861			0x9B92
#define GTP_BASEDATA_ADDR_6861			0xB0DE

#define GTP_RAWDATA_ADDR_6862			0x9078
#define GTP_NOISEDATA_ADDR_6862			0x9B92
#define GTP_BASEDATA_ADDR_6862			0xB0DE

#define GTP_REG_COOR                            0x4100
/*short test,	(52662.85/code-40) * 100*/
#define SHORT_TO_GND_RESISTER(sig)  (div_s64(5266285, (sig) & (~0x8000)) - 40 * 100)
#define SHORT_TO_VDD_RESISTER(sig, value) (div_s64		\
	(36864 * ((value) - 9) * 100, (((sig) & (~0x8000)) * 7)) - 40 * 100)

#define DRV_CHANNEL_FLAG			0x80
#define SHORT_STATUS_REG			0x5095
#define WATCH_DOG_TIMER_REG			0x20B0

#define TXRX_THRESHOLD_REG			0x7408
#define GNDVDD_THRESHOLD_REG			0x740A
#define ADC_DUMP_NUM_REG			0x740C

#define GNDAVDD_SHORT_VALUE			30
#define ADC_DUMP_NUM				200
#define SHORT_CAL_SIZE(a)			(4 + (a) * 2 + 2)

#define SHORT_TESTEND_REG			0x7400
#define TEST_RESTLT_REG				0x7401
#define TX_SHORT_NUM				0x7402

#define DIFF_CODE_REG				0x8A92
#define DRV_SELF_CODE_REG			0x8A1C
#define TX_SHORT_NUM_REG			0x8A58

#define MAX_DRV_NUM				16	//40
#define MAX_SEN_NUM				30	//36

#define MAX_DRV_NUM_9886			40
#define MAX_SEN_NUM_9886			36

#define MAX_DRV_NUM_9883			18
#define MAX_SEN_NUM_9883			28

#define MAX_DRV_NUM_6861			47
#define MAX_SEN_NUM_6861			36

#define MAX_DRV_NUM_6862			47
#define MAX_SEN_NUM_6862			29

/*  end  */



static u8 gt9886_drv_map[] = {46, 48, 49, 47, 45, 50, 56, 52, 51, 53, 55, 54, 59, 64, 57, 60, 62, 58, 65, 63, 61, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
static u8 gt9886_sen_map[] = {32, 34, 35, 30, 31, 33, 27, 28, 29, 10, 25, 26, 23, 13, 24, 12, 9, 11, 8, 7, 5, 6, 4, 3, 2, 1, 0, 73, 75, 74, 39, 72, 40, 36, 37, 38};

static u8 gt9883_drv_map[] = {45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28};
static u8 gt9883_sen_map[] = {27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 15, 14, 13, 12, 11, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46};

static u8 gt6862_drv_map[] = {36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 0, 1, 2, 3, 4, 5, 6};
static u8 gt6862_sen_map[] = {34, 35, 33, 31, 32, 30, 28, 27, 29, 24, 25, 26, 21, 22, 23, 18, 19, 20, 15, 17, 16, 14, 13, 12, 11, 9, 10, 8, 7};
static u8 gt6861_drv_map[] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 56, 46, 47, 48, 49, 45, 50, 52, 51, 55, 53, 54, 64, 59, 57, 60, 62, 58, 65, 63, 61, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
static u8 gt6861_sen_map[] = {72, 37, 38, 40, 36, 74, 39, 75, 0, 73, 2, 1, 3, 4, 5, 6, 7, 8, 10, 11, 9, 12, 13, 24, 23, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35};


/**
 * struct ts_test_params - test parameters
 * drv_num: touch panel tx(driver) number
 * sen_num: touch panel tx(sensor) number
 * max_limits: max limits of rawdata
 * min_limits: min limits of rawdata
 * deviation_limits: channel deviation limits
 * short_threshold: short resistance threshold
 * r_drv_drv_threshold: resistance threshold between drv and drv
 * r_drv_sen_threshold: resistance threshold between drv and sen
 * r_sen_sen_threshold: resistance threshold between sen and sen
 * r_drv_gnd_threshold: resistance threshold between drv and gnd
 * r_sen_gnd_threshold: resistance threshold between sen and gnd
 * avdd_value: avdd voltage value
 */
struct ts_test_params {
	u16 rawdata_addr;
	u16 noisedata_addr;
	u16 self_rawdata_addr;
	u16 self_noisedata_addr;

	u16 basedata_addr;
	u32 max_drv_num;
	u32 max_sen_num;
	u32 drv_num;
	u32 sen_num;
	u8 *drv_map;
	u8 *sen_map;

	u32 max_limits[MAX_DRV_NUM * MAX_SEN_NUM];
	u32 min_limits[MAX_DRV_NUM * MAX_SEN_NUM];
	u32 deviation_limits[MAX_DRV_NUM * MAX_SEN_NUM];
	u32 self_max_limits[MAX_DRV_NUM + MAX_SEN_NUM];
	u32 self_min_limits[MAX_DRV_NUM + MAX_SEN_NUM];

	u32 noise_threshold;
	u32 self_noise_threshold;
	u32 short_threshold;
	u32 r_drv_drv_threshold;
	u32 r_drv_sen_threshold;
	u32 r_sen_sen_threshold;
	u32 r_drv_gnd_threshold;
	u32 r_sen_gnd_threshold;
	u32 avdd_value;
};

/**
 * struct ts_test_rawdata - rawdata structure
 * data: rawdata buffer
 * size: rawdata size
 */
struct ts_test_rawdata {
	u16 data[MAX_SEN_NUM * MAX_DRV_NUM];
	u32 size;
};

struct ts_test_self_rawdata {
	u16 data[MAX_DRV_NUM + MAX_SEN_NUM];
	u32 size;
};


/**
 * struct gtx8_ts_test - main data structrue
 * ts: gtx8 touch screen data
 * test_config: test mode config data
 * orig_config: original config data
 * test_param: test parameters
 * rawdata: raw data structure
 * test_result: test result string
 */
struct gtx8_ts_test {
	//struct gtx8_ts_data *ts;
	void *ts;
	struct goodix_ts_config test_config;
	struct goodix_ts_config orig_config;
#ifdef NOISE_DATA_TEST
	struct goodix_ts_config noise_config;
#endif
	struct goodix_ts_cmd rawdata_cmd;
	struct goodix_ts_cmd normal_cmd;
	struct ts_test_params test_params;
	struct ts_test_rawdata rawdata;
	struct ts_test_rawdata noisedata;
	struct ts_test_self_rawdata self_rawdata;
	struct ts_test_self_rawdata self_noisedata;

	/*[0][0][0][0][0]..  0 without test; 1 pass, 2 panel failed; 3 software failed */
	char test_result[MAX_TEST_ITEMS];
	char test_info[TS_RAWDATA_RESULT_MAX];
};

struct short_record {
	u32 master;
	u32 slave;
	u16 short_code;
	u8 group1;
	u8 group2;
};


static void gtx8_check_setting_group(struct gtx8_ts_test *ts_test, struct short_record *r_data);

/*******************************csv_parse**********************************************/
static void copy_this_line(char *dest, char *src)
{
	char *copy_from;
	char *copy_to;

	copy_from = src;
	copy_to = dest;
	do {
		*copy_to = *copy_from;
		copy_from++;
		copy_to++;
	} while((*copy_from != '\n') && (*copy_from != '\r') && (*copy_from != '\0'));
	*copy_to = '\0';
}

static void goto_next_line(char **ptr)
{
	do {
		*ptr = *ptr + 1;
	} while (**ptr != '\n' && **ptr != '\0');
	if (**ptr == '\0') {
		return;
	}
	*ptr = *ptr + 1;
}

s32 getrid_space(s8* data, s32 len)
{
	u8* buf = NULL;
	s32 i;
	u32 count = 0;

	buf = (char*)kzalloc(len+5, GFP_KERNEL);
	if (buf == NULL){
		ts_err("get space kzalloc error");
		return -ESRCH;
	}

	for (i = 0; i < len; i++)
	{
		if (data[i] == ' ' || data[i] == '\r' || data[i] == '\n')
		{
			continue;
		}
		buf[count++] = data[i];
	}

	buf[count++] = '\0';

	memcpy(data, buf, count);

	kfree(buf);
	buf = NULL;

	return count;
}

static int parse_valid_data(char *buf_start, loff_t buf_size,
				char *ptr, int32_t* data, int rows)
{
	int i = 0;
	int j = 0;
	char *token = NULL;
	char *tok_ptr = NULL;
	char *row_data = NULL;

	if(!ptr) {
		ts_err("%s, ptr is NULL\n", __func__);
		return -EPERM;
	}
	if (!data) {
		ts_err("%s, data is NULL\n", __func__);
		return -EPERM;
	}

	row_data = (char *)kzalloc(MAX_LINE_LEN, GFP_KERNEL);
	if(NULL == row_data) {
		ts_err("%s: kzalloc %d bytes failed.\n", __func__, MAX_LINE_LEN);
		return -ESRCH;
	}

	for (i = 0; i < rows; i++) {
		// copy this line to row_data buffer
		memset(row_data, 0, MAX_LINE_LEN);
		copy_this_line(row_data, ptr);
		getrid_space(row_data,strlen(row_data));
		tok_ptr = row_data;
		while ((token = strsep(&tok_ptr,","))) {
			if (strlen(token) == 0)
				continue;
			if ( token[0] == '0' && (token[1] == 'X' || token[1] == 'x')){
				data[j] = (int32_t)simple_strtol(token, NULL, STRTOL_HEX_LEN);
				j++;
			}else{
				data[j] = (int32_t)simple_strtol(token, NULL, STRTOL_LEN);
				j++;
			}
		}
		if (i == rows -1)
			break;
		goto_next_line(&ptr);				//next row
		if(!ptr || (0 == strlen(ptr)) || (ptr >= (buf_start + buf_size))) {
			ts_info("invalid ptr, return\n");
			kfree(row_data);
			row_data = NULL;
			return -EPERM;
		}
	}
	kfree(row_data);
	row_data = NULL;
	return j;
}

//static void print_data(char* target_name, int32_t* data, int rows, int columns)
//{
//	int i,j;
//	(void)target_name;
//
//	if(NULL == data) {
//		ts_err("rawdata is NULL\n");
//		return;
//	}
//
//	for (i = 0; i < rows; i++) {
//		for (j = 0; j < columns; j++) {
//			printk("\t%d", data[i*columns + j]);
//		}
//		printk("\n");
//	}
//
//	return;
//}

/*parse data from csv
 * @return: length of data
 */
int parse_csvfile(struct device *dev, char *target_name, int32_t  *data, int rows, int columns)
{
	int ret = 0;
	int i = 0;
	int32_t read_ret = 0;
	char *buf = NULL;
	char *ptr = NULL;
	const struct firmware *firmware = NULL;

	if(NULL == target_name) {
		ts_err("target path pointer is NULL\n");
		ret = -EPERM;
		goto exit_free;
	}

	ts_info("%s, file name is %s, target is %s.\n", __func__,PARAM_FILE_NAME,target_name);
	for (i = 0; i < 3; i++) {
		ret = request_firmware(&firmware, PARAM_FILE_NAME, dev);
		if (ret < 0) {
			ts_err("limits file [%s] not available,error:%d, try_times:%d", PARAM_FILE_NAME, ret, i + 1);
			msleep(1000);
		} else {
			ts_info("limits file  [%s] is ready, try_times:%d", PARAM_FILE_NAME, i + 1);
			break;
		}
	}
	if (i >= 3) {
		ts_err("get limits file FAILED");
		goto exit_free;
	}

	if (firmware->size <= 0) {
		ts_err("request_firmware, limits param length ERROR,len:%zu", firmware->size);
		ret = -EINVAL;
		goto exit_free;
	}

	buf = (char *)kzalloc(firmware->size + 1, GFP_KERNEL);
	if(NULL == buf) {
		ts_err("%s: kzalloc %zu bytes failed.\n", __func__, firmware->size);
		ret = -ESRCH;
		goto exit_free;
	}
	memcpy(buf,firmware->data,firmware->size);
	read_ret = firmware->size;
	if (read_ret > 0) {
		buf[firmware->size] = '\0';
		ptr = buf;
		ptr = strstr(ptr, target_name);
		if (ptr == NULL) {
			ts_err("%s: load %s failed 1!\n", __func__,target_name);
			ret = -EINTR;
			goto exit_free;
		}

	      // walk thru this line
		goto_next_line(&ptr);
		if ((NULL == ptr) || (0 == strlen(ptr))) {
			ts_err("%s: load %s failed 2!\n", __func__,target_name);
			ret = -EIO;
			goto exit_free;
		}

		//analyze the data
		if (data) {
			ret = parse_valid_data(buf, firmware->size, ptr, data, rows);
			//print_data(target_name, data,  rows, ret/rows);
		}else{
			ts_err("%s: load %s failed 3!\n", __func__,target_name);
			ret = -EINTR;
			goto exit_free;
		}
	}
	else {
		ts_err("%s: ret=%d,read_ret=%d, buf=%p, firmware->size=%zu\n", __func__, ret, read_ret, buf, firmware->size);
		ret = -ENXIO;
		goto exit_free;
	}
exit_free:
	ts_info("%s exit free\n", __func__);
	if(buf) {
		ts_info("kfree buf\n");
		kfree(buf);
		buf = NULL;
	}

	if (firmware) {
		release_firmware(firmware);
		firmware = NULL;
	}
	return ret;
}
/*******************************csv_parse end**********************************************/

/***********************************************************************
* Function Name  : gtx8_get_cfg_value
* Description    : read config data specified by sub-bag number and inside-bag offset.
* config	 : pointer to config data
* buf		 : output buffer
* len		 : data length want to read, if len = 0, get full bag data
* sub_bag_num    : sub-bag number
* offset         : offset inside sub-bag
* Return         : int(return offset with config[0], < 0 failed)
*******************************************************************************/
static int gtx8_get_cfg_value(u8 *config, u8 *buf, u8 len, u8 sub_bag_num, u8 offset)
{
	u8 *sub_bag_ptr = NULL;
	u8 i = 0;

	sub_bag_ptr = &config[4];
	for (i = 0; i < config[2]; i++) {
		if (sub_bag_ptr[0] == sub_bag_num)
			break;
		sub_bag_ptr += sub_bag_ptr[1] + 3;
	}

	if (i >= config[2]) {
		ts_err("Cann't find the specifiled bag num %d\n", sub_bag_num);
		return -EINVAL;
	}

	if (sub_bag_ptr[1]+3 < offset + len) {
		ts_err("Sub bag len less then you want to read: %d < %d\n", sub_bag_ptr[1]+3, offset + len);
		return -EINVAL;
	}

	if (len)
		memcpy(buf, sub_bag_ptr + offset, len);
	else
		memcpy(buf, sub_bag_ptr, sub_bag_ptr[1] + 3);
	return (sub_bag_ptr + offset - config);
}

/*
 * parse driver and sensor num only on screen
 */
int gtx8_get_channel_num(u32 *sen_num, u32 *drv_num, u8 *cfg_data)
{
	int ret = 0;
	u8 buf[3] = {0};
	u8 *temp_cfg_data = cfg_data;

	if (!sen_num || !drv_num || !temp_cfg_data) {
		ts_err("%s: invalid param\n", __func__);
		return -EINVAL;
	}

	ret = gtx8_get_cfg_value(temp_cfg_data, buf, 1, 1, 19);
	if (ret > 0) {
		*drv_num = buf[0];
	} else {
		ts_err("Failed read drv_num reg\n");
		ret = -EINVAL;
		goto err_out;
	}

	ret = gtx8_get_cfg_value(temp_cfg_data, buf, 1, 1, 20);
	if (ret > 0) {
		*sen_num = buf[0];
	} else {
		ts_err("Failed read sen_num reg\n");
		ret = -EINVAL;
		goto err_out;
	}

	ts_info("drv_num:%d,sen_num:%d\n", *drv_num, *sen_num);

	if (*drv_num > MAX_DRV_NUM || *drv_num <= 0
		|| *sen_num > MAX_SEN_NUM || *sen_num <= 0) {
		ts_err("invalid sensor or driver num\n");
		ret = -EINVAL;
	}

err_out:
	return ret < 0 ? ret : 0;
}


/*******************************************************************************
* Function Name  : cfg_update_chksum
* Description    : update check sum
* Input          : u32* config
* Input          : u16 cfg_len
* Output         : u8* config
* Return         : none
*******************************************************************************/
static void cfg_update_chksum(u8* config, u16 cfg_len)
{
	u16 pack_map_len_arr[100];
	u16 packNum = 0;
	u16 pack_len_tmp = 0;
	u16 pack_id_tmp = 0;
	u16 i = 0,j = 0;
	u16 cur_pos = 0;
	u8  check_sum = 0;

	if (cfg_len < 4) return;
	/* Head:4 bytes    |byte0:version|byte1:config refresh|byte2:package total num|byte3:check sum|*/
	pack_map_len_arr[pack_id_tmp] = 4;
	packNum = config[2];
	for (i = 4; i < cfg_len;){
		pack_id_tmp++;
		pack_len_tmp = config[i + 1] + 3;
		pack_map_len_arr[pack_id_tmp] = pack_len_tmp;
		i += pack_len_tmp;
	}

	cur_pos = 0;
	for (i = 0; i <= pack_id_tmp; i++)
	{
		check_sum = 0;
		for (j = cur_pos; j < cur_pos + pack_map_len_arr[i] - 1; j++)
		{
			check_sum += config[j];
		}
		config[cur_pos + pack_map_len_arr[i] - 1] = (u8)(0 - check_sum);
		cur_pos += pack_map_len_arr[i];
	}
}

static int disable_hopping(struct gtx8_ts_test *ts_test, struct goodix_ts_config *test_config)
{
	int ret = 0;
	u8 value = 0;
	u16 offset = 0;
	ret = gtx8_get_cfg_value(test_config->data, &value, 1, 10, 2);
	if (ret < 0 ){
		ts_err("Failed parse hopping reg\n");
		return -EINVAL;
	}
	offset = ret;
	/* disable hopping */
	value = test_config->data[offset];
	value &= 0xfe;
	test_config->data[offset] = value;
	ts_info("disable_hopping:0x%02x_%d",test_config->data[offset], offset);
	cfg_update_chksum(test_config->data, test_config->length);
	return ret;
}
static int gtx8_captest_prepare(struct gtx8_ts_test *ts_test);

static int init_test_config(struct gtx8_ts_test *ts_test){
	int ret = 0;
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;
	if (ts_test->test_config.length == 0){
		ts_info("switch to orignal config!\n");
		memmove(ts_test->test_config.data,ts_test->orig_config.data,
				ts_test->orig_config.length);
		ts_test->test_config.length = ts_test->orig_config.length;
	}else{
		ts_test->test_config.data[0] = ts_test->orig_config.data[0];
		ts_test->test_config.data[1] |= 0x01;
		ts_info("switch to test config!\n");
	}
	ts_test->test_config.reg_base = core_data->ts_dev->reg.cfg_addr;
	mutex_init(&(ts_test->test_config.lock));
	ts_test->test_config.initialized = true;
	strcpy(ts_test->test_config.name, "test_config");
	disable_hopping(ts_test, &ts_test->test_config);
	return ret;
}

static int gtx8_init_testlimits(struct gtx8_ts_test *ts_test)
{
	int ret = 0, i = 0;
	char file_path[100] = {0};
	char file_name[GTX8_TEST_FILE_NAME_LEN] = {0};
	u32 data_buf[MAX_DRV_NUM] = {0};
	int *tmp_config = NULL;
	struct ts_test_params *test_params = NULL;
	struct goodix_ts_config *test_config = NULL;
	struct goodix_ts_core *core_data;

	test_params = &ts_test->test_params;
	test_config = &ts_test->test_config;
	core_data = (struct goodix_ts_core*)ts_test->ts;
	snprintf(file_name, GTX8_TEST_FILE_NAME_LEN, "%s", PARAM_FILE_NAME);
	tmp_config = (int*)kzalloc(GOODIX_CFG_MAX_SIZE*sizeof(int), GFP_KERNEL);
	if(NULL == tmp_config) {
		ts_err("%s: config kzalloc bytes failed.\n", __func__);
		return -ENOMEM;
	}
	/* TODO change default limit file csv to vendor */
	snprintf(file_path, sizeof(file_path), "/etc/firmware/%s", file_name);
	ts_info("%s: csv_file_path =%s\n", __func__, file_path);

	/* <max_threshold, min_threshold, delta_threshold> */
	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_UNIFIED_LIMIT, data_buf, 1, 3);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_UNIFIED_LIMIT);
		goto INIT_LIMIT_END;
	}
	/* store data to test_parms */
	for (i = 0; i < MAX_DRV_NUM * MAX_SEN_NUM; i++) {
		test_params->max_limits[i] = data_buf[0];
		test_params->min_limits[i] = data_buf[1];
		test_params->deviation_limits[i] = data_buf[2];
	}
#ifdef NOISE_DATA_TEST
	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_NOISE_LIMIT, data_buf, 1, 1);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_NOISE_LIMIT);
		goto INIT_LIMIT_END;
	}
	test_params->noise_threshold = data_buf[0];

	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_SELFNOISE_LIMIT, data_buf, 1, 1);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_SELFNOISE_LIMIT);
		goto INIT_LIMIT_END;
	}
	test_params->self_noise_threshold = data_buf[0];
#endif
	/* <self max_threshold, min_threshold> */
	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_SELF_UNIFIED_LIMIT, data_buf, 1, 2);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_SELF_UNIFIED_LIMIT);
		goto INIT_LIMIT_END;
	}
	/* store data to test_parms */
	for (i = 0; i < MAX_DRV_NUM + MAX_SEN_NUM; i++) {
		test_params->self_max_limits[i] = data_buf[0];
		test_params->self_min_limits[i] = data_buf[1];
	}

	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_SPECIAL_SELFRAW_MAX, test_params->self_max_limits,
			1, test_params->drv_num + test_params->sen_num);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_SPECIAL_SELFRAW_MAX);
		ret = 0;
	}

	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_SPECIAL_SELFRAW_MIN, test_params->self_min_limits,
			1, test_params->drv_num + test_params->sen_num);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_SPECIAL_SELFRAW_MIN);
		ret = 0;
	}

	/* shortciurt_threshold <short_threshold,drv_to_drv,
		drv_to_sen,sen_to_sen, drv_to_gnd, sen_to_gnd, avdd_r> */
	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_SHORT_THRESHOLD, data_buf, 1, 7);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_SHORT_THRESHOLD);
		goto INIT_LIMIT_END;
	}
	test_params->short_threshold = data_buf[0];
	test_params->r_drv_drv_threshold = data_buf[1];
	test_params->r_drv_sen_threshold = data_buf[2];
	test_params->r_sen_sen_threshold = data_buf[3];
	test_params->r_drv_gnd_threshold = data_buf[4];
	test_params->r_sen_gnd_threshold = data_buf[5];
	test_params->avdd_value = data_buf[6];

	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_SPECIAL_RAW_MAX, test_params->max_limits,
			test_params->drv_num, test_params->sen_num);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_SPECIAL_RAW_MAX);
		ret = 0;	/* if does not specialed the node value, we will use unified limits setting */
	}

	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_SPECIAL_RAW_MIN, test_params->min_limits,
			test_params->drv_num, test_params->sen_num);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_SPECIAL_RAW_MIN);
		ret = 0;
	}

	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_SPECIAL_RAW_DELTA, test_params->deviation_limits,
			test_params->drv_num, test_params->sen_num);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_SPECIAL_RAW_DELTA);
		ret = 0;
	}

	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_TEST_CONFIG, tmp_config,
			1, GOODIX_CFG_MAX_SIZE);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_TEST_CONFIG);
		test_config->length = 0;
		ret = 0;
	}else{
		//ts_info("test config1:");
		for (i=0;i<ret;i++){
			test_config->data[i] = tmp_config[i];
		//	ts_info("0x%02x_%d,",test_config->data[i],i);
		}
		test_config->length = ret;
		ret = 0;
	}
	init_test_config(ts_test);

#ifdef NOISE_DATA_TEST
	ret = parse_csvfile(core_data->ts_dev->dev, CSV_TP_NOISE_CONFIG, tmp_config,
			1, GOODIX_CFG_MAX_SIZE);
	if (ret < 0) {
		ts_info("%s: Failed get %s\n", __func__, CSV_TP_NOISE_CONFIG);
	}else{
		for (i=0;i<ret;i++)
			ts_test->noise_config.data[i] = tmp_config[i];
		ts_test->noise_config.length = ret;
		ts_test->noise_config.data[0] = ts_test->orig_config.data[0];
		ts_test->noise_config.data[1] |= GTX8_CONFIG_REFRESH_DATA;
		ts_test->noise_config.data[3] = (u8)(0 -
				checksum_u8(ts_test->noise_config.data, 3));

		ts_test->noise_config.reg_base = core_data->ts_dev->reg.cfg_addr;
		mutex_init(&(ts_test->noise_config.lock));
		ts_test->noise_config.initialized = true;
		strcpy(ts_test->noise_config.name, "noise_config");
		ret = 0;
	}
#endif
INIT_LIMIT_END:
	kfree(tmp_config);
	tmp_config = NULL;
	return ret;
}

static int gtx8_init_params(struct gtx8_ts_test *ts_test)
{
	int ret = 0;
	struct goodix_ts_core *core_data;
	struct ts_test_params *test_params = &ts_test->test_params;
	core_data = (struct goodix_ts_core*)ts_test->ts;
	ts_info("product id:%s\n", core_data->ts_dev->chip_version.pid);

	if (!strncmp(core_data->ts_dev->chip_version.pid, "9886", 4)){
		test_params->rawdata_addr = GTP_RAWDATA_ADDR_9886;
		test_params->noisedata_addr = GTP_NOISEDATA_ADDR_9886;
		test_params->self_rawdata_addr =  GTP_SELF_RAWDATA_ADDR_9886;
		test_params->self_noisedata_addr = GTP_SELF_NOISEDATA_ADDR_9886;
		test_params->basedata_addr = GTP_BASEDATA_ADDR_9886;
		test_params->max_drv_num = MAX_DRV_NUM_9886;
		test_params->max_sen_num = MAX_SEN_NUM_9886;
		test_params->drv_map = gt9886_drv_map;
		test_params->sen_map = gt9886_sen_map;
	}else if (!strncmp(core_data->ts_dev->chip_version.pid, "9883", 4)){
		test_params->rawdata_addr = GTP_RAWDATA_ADDR_9883;
		test_params->noisedata_addr = GTP_NOISEDATA_ADDR_9883;
		test_params->basedata_addr = GTP_BASEDATA_ADDR_9883;
		test_params->self_rawdata_addr =  GTP_SELF_RAWDATA_ADDR_9883;
		test_params->self_noisedata_addr = GTP_SELF_NOISEDATA_ADDR_9883;
		test_params->max_drv_num = MAX_DRV_NUM_9883;
		test_params->max_sen_num = MAX_SEN_NUM_9883;
		test_params->drv_map = gt9883_drv_map;
		test_params->sen_map = gt9883_sen_map;
	}else if (!strncmp(core_data->ts_dev->chip_version.pid, "6861", 4)){
		test_params->rawdata_addr = GTP_RAWDATA_ADDR_6861;
		test_params->noisedata_addr = GTP_NOISEDATA_ADDR_6861;
		test_params->basedata_addr = GTP_BASEDATA_ADDR_6861;
		test_params->max_drv_num = MAX_DRV_NUM_6861;
		test_params->max_sen_num = MAX_SEN_NUM_6861;
		test_params->drv_map = gt6861_drv_map;
		test_params->sen_map = gt6861_sen_map;
	}else if (!strncmp(core_data->ts_dev->chip_version.pid, "6862", 4)){
		test_params->rawdata_addr = GTP_RAWDATA_ADDR_6862;
		test_params->noisedata_addr = GTP_NOISEDATA_ADDR_6862;
		test_params->basedata_addr = GTP_BASEDATA_ADDR_6862;
		test_params->max_drv_num = MAX_DRV_NUM_6862;
		test_params->max_sen_num = MAX_SEN_NUM_6862;
		test_params->drv_map = gt6862_drv_map;
		test_params->sen_map = gt6862_sen_map;
	}else{
		ts_err("unsupport ic type:%s\n", core_data->ts_dev->chip_version.pid);
		test_params->rawdata_addr = GTP_RAWDATA_ADDR_9886;
		test_params->noisedata_addr = GTP_NOISEDATA_ADDR_9886;
		test_params->self_rawdata_addr =  GTP_SELF_RAWDATA_ADDR_9886;
		test_params->self_noisedata_addr = GTP_SELF_NOISEDATA_ADDR_9886;
		test_params->basedata_addr = GTP_BASEDATA_ADDR_9886;
		test_params->max_drv_num = MAX_DRV_NUM_9886;
		test_params->max_sen_num = MAX_SEN_NUM_9886;
		test_params->drv_map = gt9886_drv_map;
		test_params->sen_map = gt9886_sen_map;
		ret = NO_ERR;
		//ret = -1;
	}
	return ret;
}

/* init cmd data*/
static int gtx8_init_cmds(struct gtx8_ts_test *ts_test)
{
	int ret = 0;
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;
	/* init rawdata cmd */
	ts_test->rawdata_cmd.initialized = 1;
	ts_test->rawdata_cmd.cmd_reg = core_data->ts_dev->reg.command;
	ts_test->rawdata_cmd.length = 3;
	ts_test->rawdata_cmd.cmds[0] = GTX8_CMD_RAWDATA;
	ts_test->rawdata_cmd.cmds[1] = 0x00;
	ts_test->rawdata_cmd.cmds[2] = (u8)((0-ts_test->rawdata_cmd.cmds[0] -
				ts_test->rawdata_cmd.cmds[1])&0xff);
	/* init normal cmd */
	ts_test->normal_cmd.initialized = 1;
	ts_test->normal_cmd.cmd_reg = core_data->ts_dev->reg.command;
	ts_test->normal_cmd.length = 3;
	ts_test->normal_cmd.cmds[0] = GTX8_CMD_NORMAL;
	ts_test->normal_cmd.cmds[1] = 0x00;
	ts_test->normal_cmd.cmds[2] = (u8)((0-ts_test->normal_cmd.cmds[0] -
				ts_test->normal_cmd.cmds[1])&0xff);

	ts_info("cmd addr.0x%04x\n",ts_test->rawdata_cmd.cmd_reg);
	return ret;
}

/* gtx8_read_origconfig
 *
 * read original config data
 */
static int gtx8_cache_origconfig(struct gtx8_ts_test *ts_test)
{
	int ret = -ENODEV;
	u8 checksum = 0;
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;

	if (core_data->ts_dev->hw_ops->read_config) {
		ret = core_data->ts_dev->hw_ops->read_config(core_data->ts_dev,
				&ts_test->orig_config.data[0], 0);
		if (ret < 0) {
			ts_err("Failed to read original config data\n");
			return ret;
		}

		ts_test->orig_config.data[1] |= GTX8_CONFIG_REFRESH_DATA;
		checksum = checksum_u8(&ts_test->orig_config.data[0], 3);
		ts_test->orig_config.data[3] = (u8)(0 - checksum);

		mutex_init(&ts_test->orig_config.lock);
		ts_test->orig_config.length = ret;
		strcpy(ts_test->orig_config.name, "original_config");
		ts_test->orig_config.initialized = true;
	}

	return NO_ERR;
}

/* gtx8_tptest_prepare
 *
 * preparation before tp test
 */
static int gtx8_tptest_prepare(struct gtx8_ts_test *ts_test)
{
	int ret = 0;
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;

	ts_info("TP test preparation\n");
	ret = gtx8_cache_origconfig(ts_test);
	if (ret) {
		ts_err("Failed cache origin config\n");
		return ret;
	}

	/* init cmd*/
	ret = gtx8_init_cmds(ts_test);
	if (ret) {
		ts_err("Failed init cmd\n");
		return ret;
	}

	/* init reg addr and short cal map */
	ret = gtx8_init_params(ts_test);
	if (ret) {
		ts_err("Failed init register address\n");
		return ret;
	}

	/* get sensor and driver num currently in use */
	ret = gtx8_get_channel_num(&ts_test->test_params.sen_num,
				   &ts_test->test_params.drv_num,
				   ts_test->orig_config.data);
	if (ret) {
		ts_err("Failed get channel num:%d\n", ret);
		ts_test->test_params.sen_num = MAX_DRV_NUM;
		ts_test->test_params.drv_num = MAX_SEN_NUM;
		return ret;
	}

	/* parse test limits from csv */
	ret = gtx8_init_testlimits(ts_test);
	if (ret) {
		ts_err("Failed to init testlimits from csv:%d\n", ret);
		return ret;
	}
#ifdef NOISE_DATA_TEST
	if (core_data->ts_dev->hw_ops->send_config) {
		ret = core_data->ts_dev->hw_ops->send_config(core_data->ts_dev,
			&ts_test->noise_config);
		if (ret) {
			ts_err("Failed to send noise test config config:%d\n", ret);
			return ret;
		}
		ts_info("send noise test config success :%d\n", ret);
	}
#endif
	return ret;
}

/* gtx8_tptest_finish
 *
 * finish test
 */
static int gtx8_tptest_finish(struct gtx8_ts_test *ts_test)
{
	int ret = RESULT_ERR;
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;

	ts_info("TP test finish\n");
	ret = core_data->ts_dev->hw_ops->reset(core_data->ts_dev);
	if (ret)
		ts_err("%s: chip reset failed\n", __func__);

//	if (goodix_set_i2c_doze_mode(core_data->ts_dev,true))
//		ts_info("WRNING: doze may enabled after reset\n");

	if (core_data->ts_dev->hw_ops->send_config) {
		ret = core_data->ts_dev->hw_ops->send_config(core_data->ts_dev,&ts_test->orig_config);
		if (ret)
			ts_err("Failed to send normal config:%d\n", ret);
	}


	return ret;
}

/**
 * gtx8_cache_rawdata - cache rawdata
 */
static int gtx8_cache_rawdata(struct gtx8_ts_test *ts_test)
{
	int i = 0, j = 0;
	int ret = -EINVAL;
	u32 rawdata_size = 0;
	u16 rawdata_addr = 0;
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;
	ts_debug("Cache rawdata\n");
	ts_test->rawdata.size = 0;
	rawdata_size = ts_test->test_params.sen_num *ts_test->test_params.drv_num;

	if (rawdata_size > MAX_DRV_NUM * MAX_SEN_NUM || rawdata_size <= 0) {
		ts_err("Invalid rawdata size(%u)\n", rawdata_size);
		return ret;
	}

	rawdata_addr = ts_test->test_params.rawdata_addr;
	ts_info("Rawdata address=0x%x\n", rawdata_addr);

	for (j = 0; j < GTX8_RETRY_NUM_3; j++) {
		/* read rawdata */
		ret = sync_read_rawdata(
				rawdata_addr,
				(u8 *)&ts_test->rawdata.data[0],
				rawdata_size * sizeof(u16));
		if (ret < 0) {
			if (j == GTX8_RETRY_NUM_3 - 1){
				ts_err("Failed to read rawdata:%d\n", ret);
				goto cache_exit;
			}else{
				continue;
			}
		}
		for (i = 0; i < rawdata_size; i++)
			ts_test->rawdata.data[i] = be16_to_cpu(ts_test->rawdata.data[i]);
		ts_test->rawdata.size = rawdata_size;
		ts_info("Rawdata ready\n");
		break;
	}

//	for(i=0;i<15;i++)
//	for(j=0;j<33;j++){
//	ts_info("rawdata:%d",ts_test->rawdata.data[i*33+j]);
//	}

cache_exit:
	return ret;
}
#ifdef SELF_RAWDATA_TEST
/**
 * gtx8_cache_selfrawdata - cache selfrawdata
 */
static int gtx8_cache_self_rawdata(struct gtx8_ts_test *ts_test)
{
	int i = 0, j = 0;
	int ret = -EINVAL;
	u16 self_rawdata_size =0;
	u16 self_rawdata_addr = 0;
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;
	ts_debug("Cache selfrawdata\n");
	ts_test->self_rawdata.size = 0;
	self_rawdata_size = ts_test->test_params.sen_num +ts_test->test_params.drv_num;

	if (self_rawdata_size > MAX_DRV_NUM + MAX_SEN_NUM || self_rawdata_size <= 0) {
		ts_err("Invalid selfrawdata size(%u)\n", self_rawdata_size);
		return ret;
	}

	self_rawdata_addr = ts_test->test_params.self_rawdata_addr;
	ts_info("Selfraw address=0x%x\n", self_rawdata_addr);

	for (j = 0; j < GTX8_RETRY_NUM_3; j++) {
		/* read selfrawdata */
		ret = sync_read_rawdata(self_rawdata_addr,
				(u8 *)&ts_test->self_rawdata.data[0],
				self_rawdata_size * sizeof(u16));
		if (ret < 0) {
			if (j == GTX8_RETRY_NUM_3 - 1){
				ts_err("Failed to read self_rawdata:%d\n", ret);
				goto cache_exit;
			}else{
				continue;
			}
		}
		for (i = 0; i < self_rawdata_size; i++)
			ts_test->self_rawdata.data[i] = be16_to_cpu(ts_test->self_rawdata.data[i]);
		ts_test->self_rawdata.size = self_rawdata_size;
		ts_info("self_Rawdata ready\n");
	 	break;
	}

cache_exit:
	return ret;
}
#else
static int gtx8_cache_self_rawdata(struct gtx8_ts_test *ts_test)
{
	return 0;
}
#endif
#ifdef  NOISE_DATA_TEST
/**
 * gtx8_noisetest_prepare- noisetest prepare
 */
static int gtx8_noisetest_prepare(struct gtx8_ts_test *ts_test)
{
	int ret = 0;
	struct goodix_ts_core *core_data;
	u32 noise_data_size = 0;
	u32 self_noise_data_size = 0;

	core_data = (struct goodix_ts_core*)ts_test->ts;
	noise_data_size = ts_test->test_params.sen_num * ts_test->test_params.drv_num;
	self_noise_data_size = ts_test->test_params.sen_num + ts_test->test_params.drv_num;

	if (noise_data_size <= 0 || noise_data_size > MAX_DRV_NUM * MAX_SEN_NUM) {
		ts_err("%s: Bad noise_data_size[%d]\n", __func__, noise_data_size);
		ts_test->test_result[GTP_NOISE_TEST] = SYS_SOFTWARE_REASON;
		return -EINVAL;
	}

	if (self_noise_data_size <= 0 || self_noise_data_size > MAX_DRV_NUM + MAX_SEN_NUM) {
		ts_err("%s: Bad self_noise_data_size[%d]\n", __func__, self_noise_data_size);
		ts_test->test_result[GTP_SELFNOISE_TEST] = SYS_SOFTWARE_REASON;
		return -EINVAL;
	}

	ts_test->noisedata.size = noise_data_size;
	ts_test->self_noisedata.size = self_noise_data_size;

	ts_info("%s: noise test prepare rawdata addr:0x%04x", __func__, ts_test->rawdata_cmd.cmd_reg);
	/* change to rawdata mode */
	ret = core_data->ts_dev->hw_ops->send_cmd(core_data->ts_dev, &ts_test->rawdata_cmd);
	if (ret) {
		ts_err("%s: Failed send rawdata command:ret%d\n", __func__, ret);
		ts_test->test_result[GTP_NOISE_TEST] = SYS_SOFTWARE_REASON;
		ts_test->test_result[GTP_SELFNOISE_TEST] = SYS_SOFTWARE_REASON;
		return ret;
	}
	msleep(50);

	ts_info("%s: Enter rawdata mode\n", __func__);

	return ret;
}

/**
 * gtx8_cache_noisedata- cache noisedata
 */
static int gtx8_cache_noisedata(struct gtx8_ts_test *ts_test)
{
	int ret = 0;
	int ret1 = 0;
	struct goodix_ts_core *core_data;
	u16 noisedata_addr = 0;
	u16 self_noisedata_addr = 0;
	u32 noise_data_size = 0;
	u32 self_noise_data_size = 0;

	core_data = (struct goodix_ts_core*)ts_test->ts;
	noisedata_addr = ts_test->test_params.noisedata_addr;
	self_noisedata_addr = ts_test->test_params.self_noisedata_addr;
	noise_data_size = ts_test->noisedata.size;
	self_noise_data_size = ts_test->self_noisedata.size;

	/* read noise data */
	ret1 = sync_read_rawdata(noisedata_addr,
				(u8 *)&ts_test->noisedata.data[0],
				noise_data_size * sizeof(u16));
	if (ret1) {
		ts_err("%s: Failed read noise data\n", __func__);
		ts_test->noisedata.size = 0;
		ts_test->test_result[GTP_NOISE_TEST] = SYS_SOFTWARE_REASON;
		ret = ret1;
	}

	/* read self noise data */
	ret1 = sync_read_rawdata(self_noisedata_addr,
				(u8 *)&ts_test->self_noisedata.data[0],
				self_noise_data_size * sizeof(u16));
	if (ret1) {
		ts_err("%s: Failed read self noise data\n", __func__);
		ts_test->self_noisedata.size = 0;
		ts_test->test_result[GTP_SELFNOISE_TEST] = SYS_SOFTWARE_REASON;
		ret = ret1;
	}

	return ret;
}

/**
 * gtx8_analyse_noisedata- analyse noisedata
 */
static void gtx8_analyse_noisedata(struct gtx8_ts_test *ts_test)
{
	int i = 0;
	u32 find_bad_node = 0;
	s16 noise_value = 0;

	for (i = 0; i < ts_test->noisedata.size; i++) {
		noise_value = (s16)be16_to_cpu(ts_test->noisedata.data[i]);
		ts_test->noisedata.data[i] = abs(noise_value);

		if (ts_test->noisedata.data[i] > ts_test->test_params.noise_threshold) {
			find_bad_node++;
			ts_err("noise check failed: niose[%d][%d]:%u, > %u\n",
				(u32)div_s64(i, ts_test->test_params.sen_num),
				i % ts_test->test_params.sen_num,
				ts_test->noisedata.data[i],
				ts_test->test_params.noise_threshold);
		}
	}
	if (find_bad_node) {
		ts_info("%s:noise test find bad node\n", __func__);
		ts_test->test_result[GTP_NOISE_TEST] = GTP_PANEL_REASON;
	}else{
		ts_test->test_result[GTP_NOISE_TEST] = GTP_TEST_PASS;
	}

	return;
}

/**
 * gtx8_analyse_self_noisedata- analyse self noisedata
 */
static void gtx8_analyse_self_noisedata(struct gtx8_ts_test *ts_test)
{
	int i = 0;
	u32 self_find_bad_node = 0;
	s16 self_noise_value = 0;

	for (i = 0; i < ts_test->self_noisedata.size; i++) {
		self_noise_value = (s16)be16_to_cpu(ts_test->self_noisedata.data[i]);
		ts_test->self_noisedata.data[i] = abs(self_noise_value);

		if (ts_test->self_noisedata.data[i] > ts_test->test_params.self_noise_threshold) {
			self_find_bad_node++;
			ts_err("self noise check failed: self_noise[%d][%d]:%u, > %u\n",
				(u32)div_s64(i, ts_test->test_params.drv_num),
				i % ts_test->test_params.drv_num,
				ts_test->self_noisedata.data[i],
				ts_test->test_params.self_noise_threshold);
		}
	}

	if (self_find_bad_node) {
		ts_info("%s:self_noise test find bad node", __func__);
		ts_test->test_result[GTP_SELFNOISE_TEST] = GTP_PANEL_REASON;
	}else{
		ts_test->test_result[GTP_SELFNOISE_TEST] = GTP_TEST_PASS;
	}

	return;
}


/* test noise data */
static void gtx8_test_noisedata(struct gtx8_ts_test *ts_test)
{
	int ret = 0;
	int test_cnt = 0;
	u8 buf[1];
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;

	ret = gtx8_noisetest_prepare(ts_test);
	if (ret) {
		ts_err("%s :Noisetest prepare failed\n", __func__);
		goto soft_err_out;
	}

	/* read noisedata and self_noisedata,calculate result */
	for (test_cnt = 0; test_cnt < RAWDATA_TEST_TIMES; test_cnt++) {
		ret = gtx8_cache_noisedata(ts_test);
		if (ret) {
			if (test_cnt == RAWDATA_TEST_TIMES - 1){
				ts_err("%s: Cache noisedata failed\n", __func__);
				goto soft_err_out;
			}else{
				continue;
			}
		}
		gtx8_analyse_noisedata(ts_test);

		gtx8_analyse_self_noisedata(ts_test);
		break;
	}

	ts_info("%s: Noisedata and Self_noisedata test end\n", __func__);
	goto noise_test_exit;

soft_err_out:
	ts_test->noisedata.size = 0;
	ts_test->test_result[GTP_NOISE_TEST] = SYS_SOFTWARE_REASON;
	ts_test->self_noisedata.size = 0;
	ts_test->test_result[GTP_SELFNOISE_TEST] = SYS_SOFTWARE_REASON;
noise_test_exit:
	//core_data->ts_dev->hw_ops->send_cmd(core_data->ts_dev, &ts_test->normal_cmd);
	buf[0] = 0x00;
	core_data->ts_dev->hw_ops->write_trans(core_data->ts_dev, GTP_REG_COOR, buf,1);
	core_data->ts_dev->hw_ops->write_trans(core_data->ts_dev,ts_test->normal_cmd.cmd_reg,
			ts_test->normal_cmd.cmds, ts_test->normal_cmd.length);
	return;
}
#endif


#ifdef SELF_RAWDATA_TEST
static int gtx8_self_rawcapacitance_test(struct ts_test_self_rawdata *rawdata,
		struct ts_test_params *test_params)
{
	int i = 0;
	int ret = NO_ERR;

	for (i = 0; i < rawdata->size; i++) {
		if (rawdata->data[i] > test_params->self_max_limits[i]) {
			ts_err("self_rawdata[%d][%d]:%u >self_max_limit:%u, NG\n",
				(u32)div_s64(i, test_params->drv_num), i % test_params->drv_num,
				rawdata->data[i], test_params->self_max_limits[i]);
			ret = RESULT_ERR;
		}

		if (rawdata->data[i] < test_params->self_min_limits[i]) {
			ts_err("self_rawdata[%d][%d]:%u < min_limit:%u, NG\n",
				(u32)div_s64(i, test_params->drv_num), i % test_params->drv_num,
				rawdata->data[i], test_params->self_min_limits[i]);
			ret = RESULT_ERR;
		}
	}

	return ret;
}
#else
static int gtx8_self_rawcapacitance_test(struct ts_test_self_rawdata *rawdata,
		struct ts_test_params *test_params)
{
	return 0;
}
#endif

/* gtx8_captest_prepare
 *
 * parse test peremeters from dt
 */
static int gtx8_captest_prepare(struct gtx8_ts_test *ts_test)
{
	int ret = -EINVAL;
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;

	if (core_data->ts_dev->hw_ops->send_config) {
		ret = core_data->ts_dev->hw_ops->send_config(core_data->ts_dev, &ts_test->test_config);
		if (ret)
			ts_err("Failed to send test config:%d\n", ret);
		else
			ts_info("Success send test config");
	} else {
		ts_err("Ops.send_cfg is NULL\n");
	}
	return ret;
}

#ifdef RAWDATA_TEST_SPEC
static void gtx8_rawcapacitance_test_spec(struct gtx8_ts_test *ts_test,
		u8 *p_beyond_max_cnt, u8 *p_beyond_min_cnt)
{
	int i = 0;
	struct ts_test_params *test_params;
	struct ts_test_rawdata *rawdata;
	test_params = &ts_test->test_params;
	rawdata = &ts_test->rawdata;
	for (i=0;i<ts_test->rawdata.size;i++){
		if (rawdata->data[i] > test_params->max_limits[i]) {
			p_beyond_max_cnt[i]++;
		}
		if (rawdata->data[i] < test_params->min_limits[i]) {
			p_beyond_min_cnt[i]++;
		}
	}
}

static int gtx8_accord_test_spec(struct gtx8_ts_test *ts_test, u8 *p_beyond_cnt)
{
	int i = 0;
	int ret = NO_ERR;
	int cols = 0;
	u32 max_val = 0;
	u32 rawdata_val = 0;
	u32 sc_data_num = 0;
	u32 up = 0, down = 0, left = 0, right = 0;
	struct ts_test_params *test_params;
	struct ts_test_rawdata *rawdata;
	test_params = &ts_test->test_params;
	rawdata = &ts_test->rawdata;

	cols = test_params->sen_num;
	sc_data_num = test_params->drv_num * test_params->sen_num;
	if (cols <= 0) {
		ts_err("%s: parmas invalid\n", __func__);
		return RESULT_ERR;
	}

	for (i = 0; i < sc_data_num; i++) {
		rawdata_val = rawdata->data[i];
		max_val = 0;
		/* calculate deltacpacitance with above node */
		if (i - cols >= 0) {
			up = rawdata->data[i - cols];
			up = abs(rawdata_val - up);
			if (up > max_val)
				max_val = up;
		}

		/* calculate deltacpacitance with bellow node */
		if (i + cols < sc_data_num) {
			down = rawdata->data[i + cols];
			down = abs(rawdata_val - down);
			if (down > max_val)
				max_val = down;
		}

		/* calculate deltacpacitance with left node */
		if (i % cols) {
			left = rawdata->data[i - 1];
			left = abs(rawdata_val - left);
			if (left > max_val)
				max_val = left;
		}

		/* calculate deltacpacitance with right node */
		if ((i + 1) % cols) {
			right = rawdata->data[i + 1];
			right = abs(rawdata_val - right);
			if (right > max_val)
				max_val = right;
		}

		/* float to integer */
		if (rawdata_val) {
			max_val *= FLOAT_AMPLIFIER;
			max_val = (u32)div_s64(max_val, rawdata_val);
			if (max_val > test_params->deviation_limits[i]) {
				p_beyond_cnt[i]++;
			}
		} else {
			ts_err("Find rawdata=0 when calculate deltacapacitance:[%d][%d]\n",
				(u32)div_s64(i, cols), i % cols);
			ret = RESULT_ERR;
		}
	}
	return ret;
}


static int gtx8_rawdata_result_analyze(struct gtx8_ts_test *ts_test, u8 *p_beyond_max_cnt,
	u8 *p_beyond_min_cnt, u8 *p_beyond_accord_cnt, u8 frame_cnt)
{
	int ret = 0;
	int i = 0;
	int low_ratio = 1;
	int high_ratio = 9;
	struct ts_test_params *test_params;
	struct ts_test_rawdata *rawdata;
	test_params = &ts_test->test_params;
	rawdata = &ts_test->rawdata;
	ts_test->test_result[GTP_CAP_TEST] = GTP_TEST_PASS;
	ts_test->test_result[GTP_DELTA_TEST] = GTP_TEST_PASS;
	for (i=0;i<rawdata->size;i++){
		/* analyze max test*/
		if (p_beyond_max_cnt[i]*10/frame_cnt >= high_ratio){
			ts_err("max_cnt:%d, rawdata[%d][%d]:%u > max_limit:%u, NG\n",p_beyond_max_cnt[i],
				(u32)div_s64(i, test_params->sen_num), i % test_params->sen_num,
				rawdata->data[i], test_params->max_limits[i]);
			ts_test->test_result[GTP_CAP_TEST] = GTP_PANEL_REASON;
		}else if (p_beyond_min_cnt[i]*10/frame_cnt > low_ratio){
			ts_test->test_result[GTP_CAP_TEST] = GTP_PANEL_REASON;
			ret = -1;
		}
		/*analyze min test*/
		if (p_beyond_min_cnt[i]*10/frame_cnt >= high_ratio){
			ts_err("rawdata[%d][%d]:%u < min_limit:%u, NG\n",
				(u32)div_s64(i, test_params->sen_num), i % test_params->sen_num,
				rawdata->data[i], test_params->min_limits[i]);
			ts_test->test_result[GTP_CAP_TEST] = GTP_PANEL_REASON;
		}else if (p_beyond_min_cnt[i]*10/frame_cnt > low_ratio){
			ts_test->test_result[GTP_CAP_TEST] = GTP_PANEL_REASON;
			ret = -1;
		}
	}
	for (i=0;i<test_params->drv_num * test_params->sen_num;i++){

		/* analyze accord test*/
		if (p_beyond_accord_cnt[i]*10 >= high_ratio){
			ts_err("rawdata[%d][%d]:%u, deviation_limit:%u, NG\n",
				(u32)div_s64(i, test_params->sen_num), i % test_params->sen_num,
				rawdata->data[i], test_params->deviation_limits[i]);
			ts_test->test_result[GTP_DELTA_TEST] = GTP_PANEL_REASON;
		}else if (p_beyond_accord_cnt[i]*10 > low_ratio){
			ts_test->test_result[GTP_DELTA_TEST] = GTP_PANEL_REASON;
			ret = -1;
		}
	}
	return ret;
}

/* gtx8_rawdata_test_spec
 * test rawdata with 16 frames
 */
static void gtx8_rawdata_test_spec(struct gtx8_ts_test *ts_test)
{
	int ret = NO_ERR;
	u16 i = 0;
	u8 frame_num = 16;
	u16 data_len = 0;
	u8* p_beyond_max_limit_cnt;
	u8* p_beyond_min_limit_cnt;
	u8* p_beyond_accord_limit_cnt;

	data_len = MAX_DRV_NUM*MAX_SEN_NUM;
	p_beyond_max_limit_cnt = kzalloc(data_len, GFP_KERNEL);
	if (!p_beyond_max_limit_cnt){
		ts_err("%s: Failed to alloc mem\n", __func__);
		goto RAW_TEST_ERR;
	}
	p_beyond_min_limit_cnt = kzalloc(data_len, GFP_KERNEL);
	if (!p_beyond_min_limit_cnt){
		ts_err("%s: Failed to alloc mem\n", __func__);
		goto RAW_TEST_ERR;
	}
	p_beyond_accord_limit_cnt = kzalloc(data_len, GFP_KERNEL);
	if (!p_beyond_accord_limit_cnt){
		ts_err("%s: Failed to alloc mem\n", __func__);
		goto RAW_TEST_ERR;
	}

	//init beyond buffer
	memset(p_beyond_max_limit_cnt,0,data_len);
	memset(p_beyond_min_limit_cnt,0,data_len);
	memset(p_beyond_accord_limit_cnt,0,data_len);
	ts_info("data len:%d\n", data_len);

	/* rawdata test */
	for (i=0;i<TOTAL_FRAME_NUM;i++){
		ret = gtx8_cache_rawdata(ts_test);
		if (ret < 0) {
			/* Failed read rawdata */
			ts_err("Read rawdata failed\n");
			goto RAW_TEST_ERR;
		}

		gtx8_rawcapacitance_test_spec(ts_test,p_beyond_max_limit_cnt,
				p_beyond_min_limit_cnt);
		ret = gtx8_accord_test_spec(ts_test,p_beyond_accord_limit_cnt);
		if (ret){
			ts_test->test_result[GTP_DELTA_TEST] = GTP_PANEL_REASON;
			ts_err("DeltaCap test failed\n");
		}
		if ((i+1)%frame_num == 0){

			ret = gtx8_rawdata_result_analyze(ts_test, p_beyond_max_limit_cnt,
				p_beyond_min_limit_cnt, p_beyond_accord_limit_cnt,i+1);
			if (!ret){
				break;
			}
		}
	}
	goto RAW_TEST_END;

RAW_TEST_ERR:
	ts_test->test_result[GTP_CAP_TEST] = SYS_SOFTWARE_REASON;
	ts_test->test_result[GTP_DELTA_TEST] = SYS_SOFTWARE_REASON;
RAW_TEST_END:
	if(p_beyond_max_limit_cnt){
		kfree(p_beyond_max_limit_cnt);
		p_beyond_max_limit_cnt = NULL;
	}
	if(p_beyond_min_limit_cnt){
		kfree(p_beyond_min_limit_cnt);
		p_beyond_min_limit_cnt = NULL;
	}
	if(p_beyond_accord_limit_cnt){
		kfree(p_beyond_accord_limit_cnt);
		p_beyond_accord_limit_cnt = NULL;
	}
	return;
}

#else

static int gtx8_rawcapacitance_test(struct ts_test_rawdata *rawdata,
		struct ts_test_params *test_params)
{
	int i = 0;
	int ret = NO_ERR;

	for (i = 0; i < rawdata->size; i++) {
		if (rawdata->data[i] > test_params->max_limits[i]) {
			ts_err("rawdata[%d][%d]:%u > max_limit:%u, NG\n",
				(u32)div_s64(i, test_params->sen_num), i % test_params->sen_num,
				rawdata->data[i], test_params->max_limits[i]);
			ret = RESULT_ERR;
		}

		if (rawdata->data[i] < test_params->min_limits[i]) {
			ts_err("rawdata[%d][%d]:%u < min_limit:%u, NG\n",
				(u32)div_s64(i, test_params->sen_num), i % test_params->sen_num,
				rawdata->data[i], test_params->min_limits[i]);
			ret = RESULT_ERR;
		}
	}

	return ret;
}

static int gtx8_deltacapacitance_test(struct ts_test_rawdata *rawdata,
		struct ts_test_params *test_params)
{
	int i = 0;
	int ret = NO_ERR;
	int cols = 0;
	u32 max_val = 0;
	u32 rawdata_val = 0;
	u32 sc_data_num = 0;
	u32 up = 0, down = 0, left = 0, right = 0;

	cols = test_params->sen_num;
	sc_data_num = test_params->drv_num * test_params->sen_num;
	if (cols <= 0) {
		ts_err("%s: parmas invalid\n", __func__);
		return RESULT_ERR;
	}

	for (i = 0; i < sc_data_num; i++) {
		rawdata_val = rawdata->data[i];
		max_val = 0;
		/* calculate deltacpacitance with above node */
		if (i - cols >= 0) {
			up = rawdata->data[i - cols];
			up = abs(rawdata_val - up);
			if (up > max_val)
				max_val = up;
		}

		/* calculate deltacpacitance with bellow node */
		if (i + cols < sc_data_num) {
			down = rawdata->data[i + cols];
			down = abs(rawdata_val - down);
			if (down > max_val)
				max_val = down;
		}

		/* calculate deltacpacitance with left node */
		if (i % cols) {
			left = rawdata->data[i - 1];
			left = abs(rawdata_val - left);
			if (left > max_val)
				max_val = left;
		}

		/* calculate deltacpacitance with right node */
		if ((i + 1) % cols) {
			right = rawdata->data[i + 1];
			right = abs(rawdata_val - right);
			if (right > max_val)
				max_val = right;
		}

		/* float to integer */
		if (rawdata_val) {
			max_val *= FLOAT_AMPLIFIER;
			max_val = (u32)div_s64(max_val, rawdata_val);
			if (max_val > test_params->deviation_limits[i]) {
				ts_err("deviation[%d][%d]:%u > delta_limit:%u, NG\n",
					(u32)div_s64(i, cols), i % cols, max_val,
					test_params->deviation_limits[i]);
				ret = RESULT_ERR;
			}
		} else {
			ts_err("Find rawdata=0 when calculate deltacapacitance:[%d][%d]\n",
				(u32)div_s64(i, cols), i % cols);
			ret = RESULT_ERR;
		}
	}

	return ret;
}

/* gtx8_rawdata_test
 * test rawdata with one frame
 */
static void gtx8_rawdata_test(struct gtx8_ts_test *ts_test)
{
	int ret = 0;
	/* read rawdata and calculate result,  statistics fail times */
	ret = gtx8_cache_rawdata(ts_test);
	if (ret < 0) {
		/* Failed read rawdata */
		ts_err("Read rawdata failed\n");
		ts_test->test_result[GTP_CAP_TEST] = SYS_SOFTWARE_REASON;
		ts_test->test_result[GTP_DELTA_TEST] = SYS_SOFTWARE_REASON;
	} else {
		ret = gtx8_rawcapacitance_test(&ts_test->rawdata, &ts_test->test_params);
		if (!ret) {
			ts_test->test_result[GTP_CAP_TEST] = GTP_TEST_PASS;
			ts_info("Rawdata test pass\n");
		} else {
			ts_test->test_result[GTP_CAP_TEST] = GTP_PANEL_REASON;
			ts_err("RawCap test failed\n");
		}

		ret = gtx8_deltacapacitance_test(&ts_test->rawdata, &ts_test->test_params);
		if (!ret)  {
			ts_test->test_result[GTP_DELTA_TEST] = GTP_TEST_PASS;
			ts_info("DeltaCap test pass\n");
		} else {
			ts_test->test_result[GTP_DELTA_TEST] = GTP_PANEL_REASON;
			ts_err("DeltaCap test failed\n");
		}
	}
}
#endif

static void gtx8_capacitance_test(struct gtx8_ts_test *ts_test)
{
	int ret = 0;
	u8 buf[1];
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;

	ret = gtx8_captest_prepare(ts_test);
	if (ret) {
		ts_err("Captest prepare failed\n");
		ts_test->test_result[GTP_CAP_TEST] = SYS_SOFTWARE_REASON;
		ts_test->test_result[GTP_DELTA_TEST] = SYS_SOFTWARE_REASON;
		ts_test->test_result[GTP_SELFCAP_TEST] = SYS_SOFTWARE_REASON;
		return;
	}
	/* read rawdata and calculate result,  statistics fail times */

	ret = core_data->ts_dev->hw_ops->send_cmd(core_data->ts_dev, &ts_test->rawdata_cmd);
	if (ret) {
		ts_err("%s:Failed send rawdata cmd:ret%d\n", __func__, ret);
		goto capac_test_exit;
	} else {
		ts_info("%s: Success change to rawdata mode\n", __func__);
	}
	msleep(50);
#ifdef RAWDATA_TEST_SPEC
	gtx8_rawdata_test_spec(ts_test);
#else
	gtx8_rawdata_test(ts_test);
#endif
	/* read selfrawdata and calculate result,  statistics fail times */
	ret = gtx8_cache_self_rawdata(ts_test);
	if (ret < 0) {
		/* Failed read selfrawdata */
		ts_err("Read selfrawdata failed\n");
		ts_test->test_result[GTP_SELFCAP_TEST] = SYS_SOFTWARE_REASON;
		goto capac_test_exit;
	} else {
		ret = gtx8_self_rawcapacitance_test(&ts_test->self_rawdata, &ts_test->test_params);
		if (!ret) {
			ts_test->test_result[GTP_SELFCAP_TEST] = GTP_TEST_PASS;
			ts_info("selfrawdata test pass\n");
		} else {
			ts_test->test_result[GTP_SELFCAP_TEST] = GTP_PANEL_REASON;
			ts_err("selfrawCap test failed\n");
		}
	}
capac_test_exit:
//	core_data->ts_dev->hw_ops->send_cmd(core_data->ts_dev, &ts_test->normal_cmd);
	buf[0] = 0x00;
	core_data->ts_dev->hw_ops->write_trans(core_data->ts_dev, GTP_REG_COOR, buf,1);
	core_data->ts_dev->hw_ops->write_trans(core_data->ts_dev,ts_test->normal_cmd.cmd_reg,
			ts_test->normal_cmd.cmds, ts_test->normal_cmd.length);
	return;
}

static void gtx8_shortcircut_test(struct gtx8_ts_test *ts_test);
static void gtx8_put_test_result(struct ts_rawdata_info *info, struct gtx8_ts_test *ts_test);

int gtx8_get_rawdata(void *tsdev,struct ts_rawdata_info *info)
{
	int ret = 0;
	struct gtx8_ts_test *gts_test = NULL;
	struct device *dev;
	struct goodix_ts_core *core_data;

//	if (!tsdev || !info) {
	if (!tsdev) {
		ts_err("%s: gtx8_ts is NULL\n", __func__);
		return -ENODEV;
	}
	dev = (struct device*)tsdev;
	core_data = dev_get_drvdata(dev);

	gts_test = kzalloc(sizeof(struct gtx8_ts_test), GFP_KERNEL);
	if (!gts_test) {
		ts_err("%s: Failed to alloc mem\n", __func__);
		return -ENOMEM;
	}
	gts_test->ts = core_data;

	ret = gtx8_tptest_prepare(gts_test);
	if (ret) {
		ts_err("%s: Failed parse test peremeters, exit test\n", __func__);
		strncpy(info->result, "0F-software reason", TS_RAWDATA_RESULT_MAX - 1);
		goto exit_finish;
	}
	ts_info("%s: TP test prepare OK\n", __func__);
#ifdef  NOISE_DATA_TEST
	gtx8_test_noisedata(gts_test); /*3F 7F test*/
#endif
	gtx8_capacitance_test(gts_test); /* 1F 2F 6F test*/
	gtx8_shortcircut_test(gts_test); /* 5F test */
	gtx8_put_test_result(info, gts_test);
	gtx8_tptest_finish(gts_test);
	if (info->result[1] == 'P' && info->result[4] == 'P' &&  \
			info->result[7] == 'P' && info->result[13] == 'P') {
		ret = 1;
	} else {
		ret = 0;
	}

exit_finish:
	if (gts_test) {
		kfree(gts_test);
		gts_test = NULL;
	}
//	if (goodix_set_i2c_doze_mode(core_data->ts_dev, true))
//		ts_info("WARNING, may failed enable doze after rawdata test\n");
	return ret;
}

char *gtx8_strncat(char *dest, char *src, size_t dest_size)
{
	size_t dest_len = 0;

	dest_len = strnlen(dest, dest_size);
	return strncat(&dest[dest_len], src, dest_size - dest_len - 1);
}
char *gtx8_strncatint(char *dest, int src, char *format, size_t dest_size)
{
	char src_str[MAX_STR_LEN] = {0};

	snprintf(src_str, MAX_STR_LEN, format, src);
	return gtx8_strncat(dest, src_str, dest_size);
}

/** gtx8_data_statistics
 *
 * catlculate Avg Min Max value of data
 */
static void gtx8_data_statistics(u16 *data, size_t data_size, char *result, size_t res_size)
{
	u16 i = 0;
	u16 avg = 0;
	u16 min = 0;
	u16 max = 0;
	long long sum = 0;

	if (!data || !result) {
		ts_err("parameters error please check *data and *result value\n");
		return;
	}

	if (data_size <= 0 || res_size <= 0) {
		ts_err("input parameter is illegva:data_size=%ld, res_size=%ld\n",
			data_size, res_size);
		return;
	}

	min = data[0];
	max = data[0];
	for (i = 0; i < data_size; i++) {
		sum += data[i];
		if (max < data[i])
			max = data[i];
		if (min > data[i])
			min = data[i];
	}
	avg = div_s64(sum, data_size);
	memset(result, 0, res_size);
	snprintf(result, res_size, "[%d,%d,%d]", avg, max, min);
	return;
}

static void gtx8_put_test_result(
		struct ts_rawdata_info *info,
		struct gtx8_ts_test *ts_test)
{
	int i = 0;
	int have_bus_error = 0;
	int have_panel_error = 0;
	char statistics_data[STATISTICS_DATA_LEN] = {0};
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;

	/* save rawdata to info->buff */
	info->used_size = 0;

	if (ts_test->rawdata.size) {
		info->buff[0] = ts_test->test_params.sen_num;
		info->buff[1] = ts_test->test_params.drv_num;
		for (i = 0; i < ts_test->rawdata.size; i++)
			info->buff[i + 2] = ts_test->rawdata.data[i];

		info->used_size += ts_test->rawdata.size + 2;
	}
#ifdef NOISE_DATA_TEST
	/* save noise data to info->buff */
	if (ts_test->noisedata.size) {
		for (i = 0; i < ts_test->noisedata.size; i++)
			info->buff[info->used_size + i] = ts_test->noisedata.data[i];

		info->used_size += ts_test->noisedata.size;
	}
	/* save self_noisedata to info->buff */
	if (ts_test->self_noisedata.size) {
		for (i = 0; i < ts_test->self_noisedata.size; i++)
			info->buff[info->used_size + i] = ts_test->self_noisedata.data[i];

		info->used_size += ts_test->self_noisedata.size;
	}
#endif
	/* save self_rawdata to info->buff */
	if (ts_test->self_rawdata.size) {
		for (i = 0; i < ts_test->self_rawdata.size; i++)
			info->buff[info->used_size + i] = ts_test->self_rawdata.data[i];

		info->used_size += ts_test->self_rawdata.size;
	}


	/* check if there have bus error */
	for (i = 0; i < MAX_TEST_ITEMS; i++) {
		if (ts_test->test_result[i] == SYS_SOFTWARE_REASON)
			have_bus_error = 1;
		else if (ts_test->test_result[i] == GTP_PANEL_REASON)
			have_panel_error = 1;
	}
	ts_info("Have bus error:%d", have_bus_error);

	if (have_bus_error)
		gtx8_strncat(ts_test->test_info, "0F-", TS_RAWDATA_RESULT_MAX);
	else
		gtx8_strncat(ts_test->test_info, "0P-", TS_RAWDATA_RESULT_MAX);

	for (i = 1; i < MAX_TEST_ITEMS; i++) {
		/* if have tested, show result */
		if (ts_test->test_result[i]) {
			if (GTP_TEST_PASS == ts_test->test_result[i])
				gtx8_strncatint(ts_test->test_info, i, "%dP-", TS_RAWDATA_RESULT_MAX);
			else
				gtx8_strncatint(ts_test->test_info, i, "%dF-", TS_RAWDATA_RESULT_MAX);
		}
		ts_info("test_result_info [%d]%d\n", i, ts_test->test_result[i]);
	}

	if (ts_test->rawdata.size) {
		/* calculate rawdata min avg max vale*/
		gtx8_data_statistics(
				&ts_test->rawdata.data[0],
				ts_test->rawdata.size,
				statistics_data,
				STATISTICS_DATA_LEN);
		gtx8_strncat(ts_test->test_info, statistics_data, TS_RAWDATA_RESULT_MAX);
	} else {
		ts_info("NO valiable rawdata\n");
		gtx8_strncat(ts_test->test_info, "[0,0,0]", TS_RAWDATA_RESULT_MAX);
	}
#ifdef NOISE_DATA_TEST
	if (ts_test->noisedata.size) {
		/* calculate noise data min avg max vale*/
		gtx8_data_statistics(
				&ts_test->noisedata.data[0],
				ts_test->noisedata.size,
				statistics_data,
				STATISTICS_DATA_LEN);
		gtx8_strncat(ts_test->test_info, statistics_data, TS_RAWDATA_RESULT_MAX);
	} else {
		ts_info("NO valiable noisedata\n");
		gtx8_strncat(ts_test->test_info, "[0,0,0]", TS_RAWDATA_RESULT_MAX);
	}
#endif

	if (ts_test->self_rawdata.size) {
		/* calculate self_rawdata min avg max vale*/
		gtx8_data_statistics(
				&ts_test->self_rawdata.data[0],
				ts_test->self_rawdata.size,
				statistics_data,
				STATISTICS_DATA_LEN);
		gtx8_strncat(ts_test->test_info, statistics_data, TS_RAWDATA_RESULT_MAX);
	} else {
		ts_info("NO valiable self_rawdata\n");
		gtx8_strncat(ts_test->test_info, "[0,0,0]", TS_RAWDATA_RESULT_MAX);
	}

#ifdef NOISE_DATA_TEST
	if (ts_test->self_noisedata.size) {
		/* calculate self_noisedata min avg max vale*/
		gtx8_data_statistics(
				&ts_test->self_noisedata.data[0],
				ts_test->self_noisedata.size,
				statistics_data,
				STATISTICS_DATA_LEN);
		gtx8_strncat(ts_test->test_info, statistics_data, TS_RAWDATA_RESULT_MAX);
	} else {
		ts_info("NO valiable self_noisedata\n");
		gtx8_strncat(ts_test->test_info, "[0,0,0]", TS_RAWDATA_RESULT_MAX);
	}
#endif

	gtx8_strncat(ts_test->test_info, "-GT", TS_RAWDATA_RESULT_MAX);
	gtx8_strncat(ts_test->test_info, core_data->ts_dev->chip_version.pid, TS_RAWDATA_RESULT_MAX);
	gtx8_strncat(ts_test->test_info, "\n", TS_RAWDATA_RESULT_MAX);
	ts_info("ts_test->test_info:%s\n", ts_test->test_info);
	ts_err("info used size:%d\n",info->used_size);
	strncpy(info->result, ts_test->test_info, TS_RAWDATA_RESULT_MAX - 1);

	return;
}

/* short test */
static int gtx8_short_test_prepare(struct gtx8_ts_test *ts_test)
{
	int ret = 0, i = 0, retry = GTX8_RETRY_NUM_3;
	u8 data[MAX_DRV_NUM + MAX_SEN_NUM] = {0};
	struct goodix_ts_core *core_data;
	u8 short_cmd[3];
	u16 short_cmd_reg = 0;
	core_data = (struct goodix_ts_core*)ts_test->ts;
	short_cmd[0] = 0x0b;
	short_cmd[1] = 0x00;
	short_cmd[2] = (u8)((0 - short_cmd[0]-short_cmd[1])&0xff);
	short_cmd_reg = core_data->ts_dev->reg.command;

	ts_info("Short test prepare+\n");
	while (--retry) {
		/* switch to shrot test system */
		ret = core_data->ts_dev->hw_ops->write(core_data->ts_dev, short_cmd_reg,
				short_cmd,3);  /*bagan test command*/
		if (ret) {
			ts_err("Can not switch to short test system\n");
			return ret;
		}
		msleep(50);

		/* check firmware running */
		for (i = 0; i < 20; i++) {
			ts_info("Check firmware running..");
			ret = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev,
				SHORT_STATUS_REG, &data[0], 1);   /* SHORT_STATUS_REG is 0x5095 */
			if (ret) {
				ts_err("Check firmware running failed\n");
				return ret;
			} else if (data[0] == 0xaa) {
				ts_info("Short firmware is running\n");
				break;
			}
			msleep(10);
		}
		if (i < 20) {
			break;
		} else {
			core_data->ts_dev->hw_ops->reset(core_data->ts_dev);
		//	if (goodix_set_i2c_doze_mode(core_data->ts_dev,false))
		//		ts_info("WRNING: doze may enabled after reset\n");
		}
	}
	if (retry <= 0) {
		ret = -EINVAL;
		ts_err("Switch to short test mode timeout\n");
		return ret;
	}

	data[0] = 0;
	/* turn off watch dog timer */
	ret = core_data->ts_dev->hw_ops->write_trans(core_data->ts_dev, WATCH_DOG_TIMER_REG, data, 1);
	if (ret < 0) {
		ts_err("Failed turn off watch dog timer\n");
		return ret;
	}

	ts_info("Firmware in short test mode\n");

	data[0] = (ts_test->test_params.short_threshold >> 8) & 0xff;
	data[1] = ts_test->test_params.short_threshold & 0xff;

	/* write tx/tx, tx/rx, rx/rx short threshold value to 0x8408 */
	ret = core_data->ts_dev->hw_ops->write_trans(core_data->ts_dev, TXRX_THRESHOLD_REG,
			data, 2);  /* SHORT THRESHOLD_REG 0X8808 */
	if (ret < 0) {
		ts_err("Failed write tx/tx, tx/rx, rx/rx short threshold value\n");
		return ret;
	}
	data[0] = (GNDAVDD_SHORT_VALUE >> 8) & 0xff;
	data[1] = GNDAVDD_SHORT_VALUE & 0xff;
	/* write default txrx/gndavdd short threshold value 16 to 0x804A*/
	ret = core_data->ts_dev->hw_ops->write_trans(core_data->ts_dev, GNDVDD_THRESHOLD_REG,
			data, 2);  /* SHORT THRESHOLD_REG 0X8808 */
	if (ret < 0) {
		ts_err("Failed write txrx/gndavdd short threshold value\n");
		return ret;
	}

	/* Write ADC dump data num to 0x840c */
	data[0] = (ADC_DUMP_NUM >> 8) & 0xff;
	data[1] = ADC_DUMP_NUM & 0xff;
	ret = core_data->ts_dev->hw_ops->write_trans(core_data->ts_dev, ADC_DUMP_NUM_REG, data, 2);
	if (ret < 0) {
		ts_err("Failed write ADC dump data number\n");
		return ret;
	}

	/* write 0x01 to 0x5095 start short test */
	data[0] = 0x01;
	ret = core_data->ts_dev->hw_ops->write_trans(core_data->ts_dev, SHORT_STATUS_REG,
			data, 1);     /* SHORT_STATUS_REG 0X5095 */
	if (ret) {
		ts_err("Failed write running dsp reg\n");
		return ret;
	}

	ts_info("Short test prepare-\n");
	return 0;
}

static u32 map_die2pin(struct ts_test_params *test_params, u32 chn_num)
{
	int i = 0;
	u32 res = 255;

	if (chn_num & DRV_CHANNEL_FLAG)
		chn_num = (chn_num & ~DRV_CHANNEL_FLAG) + test_params->max_sen_num;

	for (i = 0; i < test_params->max_sen_num; i++) {
		if (test_params->sen_map[i] == chn_num) {
			res = i;
			break;
		}
	}

	/* res != 255 mean found the corresponding channel num */
	if (res != 255)
		return res;

	/* if cannot find in SenMap try find in DrvMap */
	for (i = 0; i < test_params->max_drv_num; i++) {
		if (test_params->drv_map[i] == chn_num) {
			res = i;
			break;
		}
	}
	if (i >= test_params->max_drv_num)
		ts_err("Faild found corrresponding channel num:%d\n", chn_num);
	else
		res |= DRV_CHANNEL_FLAG;

	return res;
}

static int gtx8_check_resistance_to_gnd(struct ts_test_params *test_params,
				u16 adc_signal, u32 pos)
{
	long r = 0;
	u16 r_th = 0, avdd_value = 0;
	u32 chn_id_tmp = 0;
	u32 pin_num = 0;

	avdd_value = test_params->avdd_value;
	if (adc_signal == 0 || adc_signal == 0x8000)
		adc_signal |= 1;

	if ((adc_signal & 0x8000) == 0)	/* short to GND */
		r = SHORT_TO_GND_RESISTER(adc_signal);
	else	/* short to VDD */
		r = SHORT_TO_VDD_RESISTER(adc_signal, avdd_value);

	r = (long)div_s64(r, 100);
	r = r > MAX_U16_VALUE ? MAX_U16_VALUE : r;
	r = r < 0 ? 0 : r;

	if (pos < MAX_DRV_NUM)
		r_th = test_params->r_drv_gnd_threshold;
	else
		r_th = test_params->r_sen_gnd_threshold;

	chn_id_tmp = pos;
	if (chn_id_tmp < test_params->max_drv_num)
		chn_id_tmp |= DRV_CHANNEL_FLAG;
	else
		chn_id_tmp -= test_params->max_drv_num;

	if (r < r_th) {
		pin_num = map_die2pin(test_params, chn_id_tmp);
		ts_err("%s%d shortcircut to %s,R=%ldK,R_Threshold=%dK\n",
					(pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(pin_num & ~DRV_CHANNEL_FLAG),
					(adc_signal & 0x8000) ? "VDD" : "GND",
					r, r_th);

		return RESULT_ERR;
	}
	return NO_ERR;
}

static u32 gtx8_short_resistance_calc(struct gtx8_ts_test *ts_test,
			struct short_record *r_data, u16 self_capdata, u8 flag)
{
	u16 lineDrvNum = 0, lineSenNum = 0;
	u8 DieNumber1 = 0, DieNumber2 = 0;
	long r = 0;

	lineDrvNum = ts_test->test_params.max_drv_num;
	lineSenNum = ts_test->test_params.max_sen_num;

	if (flag == 0) {
		if (r_data->group1 != r_data->group2) {	/* different Group */
			r = div_s64(self_capdata * 81 * FLOAT_AMPLIFIER, r_data->short_code);
			r -= (81 * FLOAT_AMPLIFIER);
		} else {
			DieNumber1 = ((r_data->master & 0x80) == 0x80) ? (r_data->master + lineSenNum) : r_data->master;
			DieNumber2 = ((r_data->slave & 0x80) == 0x80) ? (r_data->slave + lineSenNum) : r_data->slave;
			DieNumber1 = (DieNumber1 >= DieNumber2) ? (DieNumber1 - DieNumber2) : (DieNumber2 - DieNumber1);
			if ((DieNumber1 > 3) && (r_data->group1 == 0)) {
				r = div_s64(self_capdata * 81 * FLOAT_AMPLIFIER, r_data->short_code);
				r -= (81 * FLOAT_AMPLIFIER);
			} else {
				r = div_s64(self_capdata * 64 * FLOAT_AMPLIFIER, r_data->short_code);
				r -= (64 * FLOAT_AMPLIFIER);
			}
		}
	} else {
		r = div_s64(self_capdata * 81 * FLOAT_AMPLIFIER, r_data->short_code);
		r -= (81 * FLOAT_AMPLIFIER);
	}

	/*if (r < 6553)
		r *= 10;
	else
		r = 65535;*/
	r = (long)div_s64(r, FLOAT_AMPLIFIER);
	r = r > MAX_U16_VALUE ? MAX_U16_VALUE : r;

	return r >= 0 ? r : 0;
}

static int gtx8_shortcircut_analysis(struct gtx8_ts_test *ts_test)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;
	int size = 0, i = 0, j = 0;
	u32 master_pin_num, slave_pin_num;
	u16 adc_signal = 0, data_addr = 0;
	u8 short_flag = 0, *data_buf = NULL, short_status[3] = {0};
	u16 self_capdata[MAX_DRV_NUM + MAX_SEN_NUM] = {0}, short_die_num = 0;
	struct short_record temp_short_info;
	struct ts_test_params *test_params = &ts_test->test_params;
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;

	ret = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev, TEST_RESTLT_REG,
			&short_flag, 1);  /* TEST_RESTLT_REG  0x8401 */
	if (ret < 0) {
		ts_err("Read TEST_TESULT_REG falied\n");
		goto shortcircut_analysis_error;
	} else if ((short_flag & 0x0F) == 0x00) {
		ts_info("No shortcircut\n");
		return NO_ERR;
	}

	data_buf = kzalloc((MAX_DRV_NUM + MAX_SEN_NUM) * 2, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory\n");
		 goto shortcircut_analysis_error;
	}

	/* shortcircut to gnd&vdd */
	if (short_flag & 0x08) {
		/* read diff code, diff code will be used to calculate
		  * resistance between channel and GND */
		size = (MAX_DRV_NUM + MAX_SEN_NUM) * 2;
		ret = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev, DIFF_CODE_REG,
				data_buf, size); /* DIFF_CODE_REG   0xA97A */
		if (ret < 0) {
			ts_err("Failed read to-gnd rawdata\n");
			goto shortcircut_analysis_error;
		}
		for (i = 0; i < size; i += 2) {
			adc_signal = be16_to_cpup((__be16 *)&data_buf[i]);
			ret = gtx8_check_resistance_to_gnd(test_params,
						adc_signal, i >> 1); /* i >> 1 = i / 2 */
			if (ret) {
				ts_err("Resistance to-gnd test failed\n");
				err |= ret;
			}
		}
	}

	/* read self-capdata+ */
	size = (MAX_DRV_NUM + MAX_SEN_NUM) * 2;
	ret = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev, DRV_SELF_CODE_REG,
			data_buf, size);  /* DRV_SELF_CODE_REG   0xa8e0 */
	if (ret) {
		ts_err("Failed read selfcap rawdata\n");
		goto shortcircut_analysis_error;
	}
	for (i = 0; i < MAX_DRV_NUM + MAX_SEN_NUM; i++)
		self_capdata[i] = be16_to_cpup((__be16 *)&data_buf[i * 2]) & 0x7fff;
	/* read self-capdata- */

	/* read tx tx short number
	**   short_status[0]: tr tx
	**   short_status[1]: tr rx
	**   short_status[2]: rx rx
	*/
	ret = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev, TX_SHORT_NUM,
			&short_status[0], 3);  /* TX_SHORT_NUM   0x8402 */
	if (ret) {
		ts_err("Failed read tx-to-tx short rawdata\n");
		goto shortcircut_analysis_error;
	}
	ts_info("Tx&Tx:%d,Rx&Rx:%d,Tx&Rx:%d\n", short_status[0], short_status[1], short_status[2]);

	/* drv&drv shortcircut check */
	data_addr = 0x8460;
	for (i = 0; i < short_status[0]; i++) {
		size = SHORT_CAL_SIZE(MAX_DRV_NUM);	/* 4 + MAX_DRV_NUM * 2 + 2; */
		ret = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev, data_addr, data_buf, size);
		if (ret) {
			ts_err("Failed read drv-to-drv short rawdata\n");
			goto shortcircut_analysis_error;
		}

		r_threshold = test_params->r_drv_drv_threshold;
		short_die_num = be16_to_cpup((__be16 *)&data_buf[0]);
		if (short_die_num > MAX_DRV_NUM + MAX_SEN_NUM ||
			short_die_num < MAX_SEN_NUM) {
			ts_info("invalid short pad num:%d\n", short_die_num);
			continue;
		}

		/* TODO: j start position need recheck */
		short_die_num -= test_params->max_sen_num;
		for (j = short_die_num + 1; j < MAX_DRV_NUM; j++) {
			adc_signal = be16_to_cpup((__be16 *)&data_buf[4 + j * 2]);

			if (adc_signal > test_params->short_threshold) {
				temp_short_info.master = short_die_num | DRV_CHANNEL_FLAG;
				temp_short_info.slave = j | DRV_CHANNEL_FLAG;
				temp_short_info.short_code = adc_signal;
				gtx8_check_setting_group(ts_test, &temp_short_info);

				if (self_capdata[short_die_num] == 0xffff ||
				    self_capdata[short_die_num] == 0) {
					ts_info("invalid self_capdata:0x%x\n", self_capdata[short_die_num]);
					continue;
				    }

				short_r = gtx8_short_resistance_calc(ts_test, &temp_short_info,
							self_capdata[short_die_num], 0);
				if (short_r < r_threshold) {
					master_pin_num = map_die2pin(test_params, temp_short_info.master);
					slave_pin_num = map_die2pin(test_params, temp_short_info.slave);
					ts_err("Tx/Tx short circut:R=%dK,R_Threshold=%dK\n",
								short_r, r_threshold);
					ts_err("%s%d--%s%d shortcircut\n",
							   (master_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
							   (master_pin_num & ~DRV_CHANNEL_FLAG),
							   (slave_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
							   (slave_pin_num & ~DRV_CHANNEL_FLAG));
					err |= -EINVAL;
				}
			}
		}
		data_addr += size;
	}

	/* sen&sen shortcircut check */
	data_addr = 0x91d0;
	for (i = 0; i < short_status[1]; i++) {
		size =   SHORT_CAL_SIZE(MAX_SEN_NUM);     /* 4 + MAX_SEN_NUM * 2 + 2; */
		ret = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev, data_addr, data_buf, size);
		if (ret) {
			ts_err("Failed read sen-to-sen short rawdata\n");
			goto shortcircut_analysis_error;
		}

		r_threshold = ts_test->test_params.r_sen_sen_threshold;
		short_die_num = be16_to_cpup((__be16 *)&data_buf[0]);
		if (short_die_num > MAX_SEN_NUM)
			continue;

		for (j = short_die_num + 1; j < MAX_SEN_NUM; j++) {
			adc_signal = be16_to_cpup((__be16 *)&data_buf[4 + j * 2]);
			if (adc_signal > ts_test->test_params.short_threshold) {
				temp_short_info.master = short_die_num;
				temp_short_info.slave = j;
				temp_short_info.short_code = adc_signal;
				gtx8_check_setting_group(ts_test, &temp_short_info);

				if (self_capdata[short_die_num + test_params->max_drv_num] == 0xffff ||
				    self_capdata[short_die_num + test_params->max_drv_num] == 0) {
					ts_info("invalid self_capdata:0x%x\n",
						self_capdata[short_die_num + test_params->max_drv_num]);
					continue;
				}

				short_r = gtx8_short_resistance_calc(ts_test, &temp_short_info,
							self_capdata[short_die_num + test_params->max_drv_num], 0);
				if (short_r < r_threshold) {
					master_pin_num = map_die2pin(test_params, temp_short_info.master);
					slave_pin_num = map_die2pin(test_params, temp_short_info.slave);
					ts_err("Rx/Rx short circut:R=%dK,R_Threshold=%dK\n",
								short_r, r_threshold);
					ts_err("%s%d--%s%d shortcircut\n",
							   (master_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
							   (master_pin_num & ~DRV_CHANNEL_FLAG),
							   (slave_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
							   (slave_pin_num & ~DRV_CHANNEL_FLAG));
					err |= -EINVAL;
				}
			}
		}
		data_addr += size;
	}

	/* sen&drv shortcircut check */
	data_addr = 0x9cc8;
	for (i = 0; i < short_status[2]; i++) {
		size =  SHORT_CAL_SIZE(MAX_DRV_NUM);                        /* size = 4 + MAX_SEN_NUM * 2 + 2; */
		ret = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev, data_addr, data_buf, size);
		if (ret) {
			ts_err("Failed read sen-to-drv short rawdata\n");
			 goto shortcircut_analysis_error;
		}

		r_threshold = ts_test->test_params.r_drv_sen_threshold;
		short_die_num = be16_to_cpup((__be16 *)&data_buf[0]);
		if (short_die_num > MAX_SEN_NUM)
			continue;

		for (j = 0; j < MAX_DRV_NUM; j++) {
			adc_signal = be16_to_cpup((__be16 *)&data_buf[4 + j * 2]);
			if (adc_signal > ts_test->test_params.short_threshold) {
				temp_short_info.master = short_die_num;
				temp_short_info.slave = j | DRV_CHANNEL_FLAG;
				temp_short_info.short_code = adc_signal;
				gtx8_check_setting_group(ts_test, &temp_short_info);

				if (self_capdata[short_die_num + test_params->max_drv_num] == 0xffff ||
				    self_capdata[short_die_num + test_params->max_drv_num] == 0) {
					ts_info("invalid self_capdata:0x%x\n",
						self_capdata[short_die_num + test_params->max_drv_num]);
					continue;
				}

				short_r = gtx8_short_resistance_calc(ts_test, &temp_short_info,
							self_capdata[short_die_num + test_params->max_drv_num], 0);
				if (short_r < r_threshold) {
					master_pin_num = map_die2pin(test_params, temp_short_info.master);
					slave_pin_num = map_die2pin(test_params, temp_short_info.slave);
					ts_err("Rx/Tx short circut:R=%dK,R_Threshold=%dK\n",
								short_r, r_threshold);
					ts_err("%s%d--%s%d shortcircut\n",
							   (master_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
							   (master_pin_num & ~DRV_CHANNEL_FLAG),
							   (slave_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
							   (slave_pin_num & ~DRV_CHANNEL_FLAG));
					err |= -EINVAL;
				}
			}
		}
		data_addr += size;
	}

	if (data_buf) {
		kfree(data_buf);
		data_buf = NULL;
	}

	return err | ret ? -EFAULT :  NO_ERR;
shortcircut_analysis_error:
	if (data_buf != NULL) {
		kfree(data_buf);
		data_buf = NULL;
	}
	return -EINVAL;
}

static void gtx8_shortcircut_test(struct gtx8_ts_test *ts_test)
{
	int i = 0;
	int ret = 0;
	u8 data[2] = {0};
	struct goodix_ts_core *core_data;
	core_data = (struct goodix_ts_core*)ts_test->ts;

	ts_test->test_result[GTP_SHORT_TEST] = GTP_TEST_PASS;
	ret = gtx8_short_test_prepare(ts_test);
	if (ret < 0) {
		ts_err("Failed enter short test mode\n");
		ts_test->test_result[GTP_SHORT_TEST] = SYS_SOFTWARE_REASON;
		return;
	}

	for (i = 0; i < 150; i++) {
		msleep(50);
		ts_info("waitting for short test end...:retry=%d\n", i);
		ret = core_data->ts_dev->hw_ops->read_trans(core_data->ts_dev, SHORT_TESTEND_REG, data, 1);   /* SHORT_TESTEND_REG   0x8400 */
		if (ret)
			ts_err("Failed get short test result: retry%d\n", i);
		else if (data[0] == 0x88)  /* test ok*/
			break;
	}

	if (i < 150) {
		ret = gtx8_shortcircut_analysis(ts_test);
		if (ret) {
			ts_test->test_result[GTP_SHORT_TEST] = GTP_PANEL_REASON;
			ts_err("Short test failed\n");
		} else {
			ts_err("Short test success\n");
		}
	} else {
		ts_err("Wait short test finish timeout:reg_val=0x%x\n", data[0]);
		ts_test->test_result[GTP_SHORT_TEST] = SYS_SOFTWARE_REASON;
	}
	return;
}


static void gtx8_check_setting_group(struct gtx8_ts_test *ts_test, struct short_record *r_data)
{
	u32 dMaster = 0;
	u32 dSlave = 0;

	if (r_data->master & 0x80)
		dMaster = ts_test->test_params.max_sen_num;

	if (r_data->slave & 0x80)
		dSlave = ts_test->test_params.max_sen_num;

	dMaster += (r_data->master & 0x7f);
	dSlave += (r_data->slave & 0x7f);

	if ((dMaster >= 0) && (dMaster < 9))   /* pad s0~s8 */
	    r_data->group1 = 5;

	else if ((dMaster >= 9) && (dMaster < 14))   /* pad s9~s13 */
	    r_data->group1 = 4;

	else if ((dMaster >= 14) && (dMaster < 18))  /* pad s14~s17 */
	    r_data->group1 = 3;

	else if ((dMaster >= 18) && (dMaster < 27))  /* pad s18~s26 */
	    r_data->group1 = 2;

	else if ((dMaster >= 27) && (dMaster < 32))  /* pad s27~s31 */
	    r_data->group1 = 1;

	else if ((dMaster >= 32) && (dMaster < 36))  /* pad s32~s35 */
	    r_data->group1 = 0;

	else if ((dMaster >= 36) && (dMaster < 45))  /* pad d0~d8 */
	    r_data->group1 = 5;

	else if ((dMaster >= 45) && (dMaster < 54))  /* pad d9~d17 */
	    r_data->group1 = 2;

	else if ((dMaster >= 54) && (dMaster < 59))  /* pad d18~d22 */
	    r_data->group1 = 1;

	else if ((dMaster >= 59) && (dMaster < 63))  /*  pad d23~d26 */
	    r_data->group1 = 0;

	else if ((dMaster >= 63) && (dMaster < 67))  /* pad d27~d30 */
	    r_data->group1 = 3;

	else if ((dMaster >= 67) && (dMaster < 72))  /* pad d31~d35 */
	    r_data->group1 = 4;

	else if ((dMaster >= 72) && (dMaster < 76))  /* pad d36~d39 */
	    r_data->group1 = 0;


	if ((dSlave > 0) && (dSlave < 9))   /* pad s0~s8 */
	    r_data->group2 = 5;

	else if ((dSlave >= 9) && (dSlave < 14))   /* pad s9~s13 */
	    r_data->group2 = 4;

	else if ((dSlave >= 14) && (dSlave < 18))  /* pad s14~s17 */
	    r_data->group2 = 3;

	else if ((dSlave >= 18) && (dSlave < 27))  /* pad s18~s26 */
	    r_data->group2 = 2;

	else if ((dSlave >= 27) && (dSlave < 32))  /* pad s27~s31 */
	    r_data->group2 = 1;

	else if ((dSlave >= 32) && (dSlave < 36))  /* pad s32~s35 */
	    r_data->group2 = 0;

	else if ((dSlave >= 36) && (dSlave < 45))  /* pad d0~d8 */
	    r_data->group2 = 5;

	else if ((dSlave >= 45) && (dSlave < 54))  /* pad d9~d17 */
	    r_data->group2 = 2;

	else if ((dSlave >= 54) && (dSlave < 59))  /* pad d18~d22 */
	    r_data->group2 = 1;

	else if ((dSlave >= 59) && (dSlave < 63))  /* pad d23~d26 */
	    r_data->group2 = 0;

	else if ((dSlave >= 63) && (dSlave < 67))  /* pad d27~d30 */
	    r_data->group2 = 3;

	else if ((dSlave >= 67) && (dSlave < 72))  /* pad d31~d35 */
	    r_data->group2 = 4;

	else if ((dSlave >= 72) && (dSlave < 76))  /* pad d36~d39 */
	    r_data->group2 = 0;
}

