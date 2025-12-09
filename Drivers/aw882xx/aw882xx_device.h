#ifndef __AWINIC_DEVICE_FILE_H__
#define __AWINIC_DEVICE_FILE_H__


#include "aw882xx_base.h"
#include "aw882xx_init.h"
#include "aw_profile_process.h"

#ifdef AW_MONITOR
#include "aw882xx_monitor.h"
#endif


#define AW_VOLUME_STEP_DB	(6 * 2)
#define AW_REG_NONE		(0xFF)

enum {
	AW_1_MS = 1,
	AW_2_MS = 2,
	AW_3_MS = 3,
	AW_4_MS = 4,
	AW_5_MS = 5,
	AW_10_MS = 10,
};

struct aw_device;

enum {
	AW_DEV_TYPE_OK = 0,
	AW_DEV_TYPE_NONE = 1,
};

enum {
	AW_EF_AND_CHECK = 0,
	AW_EF_OR_CHECK,
};

enum {
	AW_DEV_CH_PRI_L = 0,
	AW_DEV_CH_PRI_R = 1,
	AW_DEV_CH_SEC_L = 2,
	AW_DEV_CH_SEC_R = 3,
	AW_DEV_CH_MAX,
};

enum AW_DEV_INIT {
	AW_DEV_INIT_ST = 0,
	AW_DEV_INIT_OK = 1,
	AW_DEV_INIT_NG = 2,
};

enum AW_DEV_STATUS {
	AW_DEV_PW_OFF = 0,
	AW_DEV_PW_ON,
};

enum AW_MODE_STATUS {
	AW_NOT_SPK_MODE = 0,
	AW_SPK_MODE = 1,
};


struct aw_device_ops {
	int (*aw_i2c_write)(struct aw_device *aw_dev, unsigned char reg_addr, unsigned int reg_data);
	int (*aw_i2c_read)(struct aw_device *aw_dev, unsigned char reg_addr, unsigned int *reg_data);
	int (*aw_i2c_write_bits)(struct aw_device *aw_dev, unsigned char reg_addr, unsigned int mask, unsigned int reg_data);
	int (*aw_set_volume)(struct aw_device *aw_dev, unsigned int value);
	int (*aw_get_volume)(struct aw_device *aw_dev, unsigned int *value);
	unsigned int (*aw_reg_val_to_db)(unsigned int value);
	void (*aw_i2s_enable)(struct aw_device *aw_dev, bool flag);
	bool (*aw_check_wr_access)(int reg);
	bool (*aw_check_rd_access)(int reg);
	int (*aw_get_reg_num)(void);
	int (*aw_get_version)(char *buf, int size);
	int (*aw_get_dev_num)(void);
	int (*aw_get_icalk_splice)(struct aw_device *aw_dev, int16_t *icalk);
	unsigned int (*aw_get_irq_type)(struct aw_device *aw_dev, unsigned int value);
	void (*aw_reg_force_set)(struct aw_device *aw_dev);
	int (*aw_monitor_init)(void *dev);
	void (*aw_monitor_start)(void *dev);
	void (*aw_monitor_stop)(void *dev);
	void (*aw_monitor_deinit)(void *dev);
	int (*aw_monitor_work_func)(void *dev);
	void (*aw_monitor_set_handle) (void *dev);
};

struct aw_int_desc {
	unsigned int mask_reg;			/*interrupt mask reg*/
	unsigned int st_reg;			/*interrupt status reg*/
	unsigned int mask_default;		/*default mask close all*/
	unsigned int int_mask;			/*set mask*/
	unsigned int uvl_val;
	unsigned int clock_val;
	unsigned int clks_val;
};

struct aw_soft_rst {
	int reg;
	int reg_value;
};

struct aw_pwd_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_amppd_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_bop_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disbale;
};

struct aw_vcalb_desc {
	unsigned int icalk_reg;
	unsigned int icalk_reg_mask;
	unsigned int icalk_shift;
	unsigned int icalkl_reg;
	unsigned int icalkl_reg_mask;
	unsigned int icalkl_shift;
	unsigned int icalk_sign_mask;
	unsigned int icalk_neg_mask;
	int icalk_value_factor;

	unsigned int vcalk_reg;
	unsigned int vcalk_reg_mask;
	unsigned int vcalk_shift;
	unsigned int vcalkl_reg;
	unsigned int vcalkl_reg_mask;
	unsigned int vcalkl_shift;
	unsigned int vcalk_sign_mask;
	unsigned int vcalk_neg_mask;
	int vcalk_value_factor;

	unsigned int vcalb_reg;
	int cabl_base_value;
	int vcal_factor;
};

struct aw_mute_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_uls_hmute_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_sysst_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int st_check;
	unsigned int pll_check;
};

struct aw_profctrl_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int spk_mode;
	unsigned int cfg_prof_mode;
};

struct aw_bstctrl_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int frc_bst;
	unsigned int tsp_type;
	unsigned int cfg_bst_type;
};

struct aw_cco_mux_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int divided_val;
	unsigned int bypass_val;
};

struct aw_volume_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int shift;

	int init_volume;
	int mute_volume;
	int ctl_volume;

};

struct aw_voltage_desc {
	unsigned int reg;
	unsigned int vbat_range;
	unsigned int int_bit;
};

struct aw_temperature_desc {
	unsigned int reg;
	unsigned int sign_mask;
	unsigned int neg_mask;
};

struct aw_ipeak_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int shift;
	unsigned int min_ma;
	unsigned int max_ma;
	unsigned int step_ma;
};

struct aw_efcheck_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int and_val;
	unsigned int or_val;
};

struct aw_dither_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_vout_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int shift;
	unsigned int min_uv;
	unsigned int max_uv;
	unsigned int step_uv;
	unsigned int base_code;
};

struct aw_device {
	aw_dev_index_t dev_index;
	unsigned int chip_id;
	int status;

	int bstcfg_enable;
	unsigned int mute_st;
	unsigned int amppd_st;
	unsigned int dither_st;
	int frcset_en;
	int bop_en;
	int efuse_check;
	aw_fade_en_t fade_en;

	char cur_prof_name[AW_PROF_NAME_MAX];		/*current profile name*/
	char set_prof_name[AW_PROF_NAME_MAX];		/*set profile name*/
	char first_prof_name[AW_PROF_NAME_MAX];		/*first profile name in aw_prof_desc*/

	uint8_t spk_mode;
	unsigned int vol_step;
	void *private_data;
	struct aw_prof_info *prof_info;

	struct aw_int_desc int_desc;
	struct aw_pwd_desc pwd_desc;
	struct aw_amppd_desc amppd_desc;
	struct aw_mute_desc mute_desc;
	struct aw_uls_hmute_desc uls_hmute_desc;
	struct aw_vcalb_desc vcalb_desc;
	struct aw_sysst_desc sysst_desc;
	struct aw_profctrl_desc profctrl_desc;
	struct aw_bstctrl_desc bstctrl_desc;
	struct aw_cco_mux_desc cco_mux_desc;
	struct aw_voltage_desc voltage_desc;
	struct aw_temperature_desc temp_desc;
	struct aw_ipeak_desc ipeak_desc;
	struct aw_vout_desc vout_desc;
	struct aw_volume_desc volume_desc;
#ifdef AW_MONITOR
	struct aw_monitor_desc monitor_desc;
#endif
	struct aw_soft_rst soft_rst;
	struct aw_bop_desc bop_desc;
	struct aw_efcheck_desc efcheck_desc;
	struct aw_dither_desc dither_desc;
	struct aw_device_ops ops;
};

int aw882xx_device_start(struct aw_device *aw_dev);
int aw882xx_device_stop(struct aw_device *aw_dev);
int aw882xx_dev_reg_update(struct aw_device *aw_dev, bool force);
int aw882xx_device_irq_reinit(struct aw_device *aw_dev);
int aw882xx_dev_reg_dump(struct aw_device *aw_dev);
void aw882xx_dev_soft_reset(struct aw_device *aw_dev);

/*profile*/
int aw882xx_dev_prof_update(struct aw_device *aw_dev, bool force);

/*interrupt*/
int aw882xx_dev_status(struct aw_device *aw_dev);
int aw882xx_dev_get_int_status(struct aw_device *aw_dev, uint16_t *int_status);
void aw882xx_dev_clear_int_status(struct aw_device *aw_dev);
int aw882xx_dev_set_intmask(struct aw_device *aw_dev, bool flag);

/*fade int / out*/
void aw882xx_dev_set_fade_vol_step(struct aw_device *aw_dev, unsigned int step);
int aw882xx_dev_get_fade_vol_step(struct aw_device *aw_dev);
void aw882xx_dev_get_fade_time(unsigned int *time, bool fade_in);
void aw882xx_dev_set_fade_time(unsigned int time, bool fade_in);

int aw882xx_dev_set_volume(struct aw_device *aw_dev, uint32_t volume);
int aw882xx_dev_get_volume(struct aw_device *aw_dev, uint32_t *volume);
int aw882xx_device_probe(struct aw_device *aw_dev, struct aw_init_info *init_info);


int aw882xx_dev_set_profile_name(struct aw_device *aw_dev, const char *prof_name);
char *aw882xx_dev_get_profile_name(struct aw_device *aw_dev);

void aw882xx_dev_interrupt_clear(struct aw_device *aw_dev);

int aw882xx_dev_check_prof(aw_dev_index_t dev_index, struct aw_prof_info *prof_info);

int aw882xx_dev_set_boost_ipeak_ma(struct aw_device *aw_dev, unsigned int ipeak_ma);
int aw882xx_dev_get_boost_ipeak_ma(struct aw_device *aw_dev, unsigned int *ipeak_ma);
int aw882xx_dev_set_boost_voltage_uv(struct aw_device *aw_dev, unsigned int vout_uv);
int aw882xx_dev_get_boost_voltage_uv(struct aw_device *aw_dev, unsigned int *vout_uv);

#endif

