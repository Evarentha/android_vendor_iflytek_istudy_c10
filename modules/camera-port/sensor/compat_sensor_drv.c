/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <video/sprd_sensor_k.h>
#include "compat_sensor_drv.h"

struct compat_sensor_reg_tab_tag {
	compat_uptr_t sensor_reg_tab_ptr;
	u32 reg_count, reg_bits, burst_mode;
};
struct compat_sensor_i2c_tag {
	compat_uptr_t i2c_data;
	u16 i2c_count, slave_addr, read_len;
};
struct compat_sensor_muti_aec_i2c_tag {
	compat_uptr_t sensor_id;
	u16 id_size;
	compat_uptr_t i2c_slave_addr;
	u16 i2c_slave_len;
	compat_uptr_t addr_bits_type;
	u16 addr_bits_type_len;
	compat_uptr_t data_bits_type;
	u16 data_bits_type_len;
	compat_uptr_t master_i2c_tab;
	u16 msize;
	compat_uptr_t slave_i2c_tab;
	u16 ssize;
	compat_uptr_t slave_i2c_tab_2;
	u16 ssize_2;
};
#define COMPAT_SENSOR_IO_I2C_WRITE_REGS \
	_IOW(SENSOR_IOC_MAGIC, 14, struct compat_sensor_reg_tab_tag)
#define COMPAT_SENSOR_IO_GRC_I2C_WRITE \
	_IOW(SENSOR_IOC_MAGIC, 19, struct compat_sensor_i2c_tag)
#define COMPAT_SENSOR_IO_GRC_I2C_READ \
	_IOWR(SENSOR_IOC_MAGIC, 20, struct compat_sensor_i2c_tag)
#define COMPAT_SENSOR_IO_MUTI_I2C_WRITE \
	_IOW(SENSOR_IOC_MAGIC, 23, struct compat_sensor_muti_aec_i2c_tag)

long sprd_sensor_ioctl_kernel(struct file *file, unsigned int cmd, void *data);

long compat_sensor_k_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *user = compat_ptr(arg);

	if (!file->f_op->unlocked_ioctl)
		return -ENOTTY;
	switch (cmd) {
	case COMPAT_SENSOR_IO_I2C_WRITE_REGS: {
		struct compat_sensor_reg_tab_tag c;
		struct sensor_reg_tab_tag n = {0};

		if (copy_from_user(&c, user, sizeof(c)))
			return -EFAULT;
		n.sensor_reg_tab_ptr = compat_ptr(c.sensor_reg_tab_ptr);
		n.reg_count = c.reg_count;
		n.reg_bits = c.reg_bits;
		n.burst_mode = c.burst_mode;
		return sprd_sensor_ioctl_kernel(file, SENSOR_IO_I2C_WRITE_REGS, &n);
	}
	case COMPAT_SENSOR_IO_GRC_I2C_WRITE:
	case COMPAT_SENSOR_IO_GRC_I2C_READ: {
		struct compat_sensor_i2c_tag c;
		struct sensor_i2c_tag n = {0};

		if (copy_from_user(&c, user, sizeof(c)))
			return -EFAULT;
		n.i2c_data = compat_ptr(c.i2c_data);
		n.i2c_count = c.i2c_count;
		n.slave_addr = c.slave_addr;
		n.read_len = c.read_len;
		return sprd_sensor_ioctl_kernel(file,
			cmd == COMPAT_SENSOR_IO_GRC_I2C_READ ?
			SENSOR_IO_GRC_I2C_READ : SENSOR_IO_GRC_I2C_WRITE, &n);
	}
	case COMPAT_SENSOR_IO_MUTI_I2C_WRITE: {
		struct compat_sensor_muti_aec_i2c_tag c;
		struct sensor_muti_aec_i2c_tag n = {0};

		if (copy_from_user(&c, user, sizeof(c)))
			return -EFAULT;
#define PTR(field) n.field = compat_ptr(c.field)
#define VAL(field) n.field = c.field
		PTR(sensor_id); VAL(id_size);
		PTR(i2c_slave_addr); VAL(i2c_slave_len);
		PTR(addr_bits_type); VAL(addr_bits_type_len);
		PTR(data_bits_type); VAL(data_bits_type_len);
		PTR(master_i2c_tab); VAL(msize);
		PTR(slave_i2c_tab); VAL(ssize);
		PTR(slave_i2c_tab_2); VAL(ssize_2);
#undef PTR
#undef VAL
		return sprd_sensor_ioctl_kernel(file, SENSOR_IO_MUTI_I2C_WRITE, &n);
	}
	default:
		/* No native OTP handler exists in this source revision. */
		if (_IOC_TYPE(cmd) == SENSOR_IOC_MAGIC && _IOC_NR(cmd) == 254)
			return -ENOTTY;
		return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)user);
	}
}
