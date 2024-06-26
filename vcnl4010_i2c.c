// SPDX-License-Identifier: GPL-2.0+
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>

// User defines
#define SLAVE_DEVICE_NAME "vcnl4010"
#define SLAVE_ADDRESS 0x13
#define COMPATIBLE_FIELD_DT "vishay,vcnl4010"
#define DRIVER_CLASS "vcnl4010_class"
#define I2C_BUS_AVAILABLE 2 // I2C Bus available on beaglebone black
#define PROXIMITY_RATE_REGISTER_MASK (0x07)

// vcnl4010 device driver struct
const struct vcnl4010 {
	struct i2c_client *client;
	dev_t dev;
	struct cdev cdev;
	struct class *class;
};

static int vcnl4010_open(const struct inode *inode, const struct file *file)
{
	struct vcnl4010 *dev = container_of(inode->i_cdev, const struct vcnl4010, cdev);

	if (dev == NULL) {
	    pr_err("container_of did not found any valid data!");
		return -ENODEV;
	}

    file->private_data = dev;

    if (inode->i_cdev != &dev->cdev) {
	pr_err("Device open: internal error!");
	return -ENODEV;
    }

	return 0;
}

static int vcnl4010_release(const struct inode *inode, const struct file *file)
{
	return 0;
}

static ssize_t vcnl4010_read(const struct file *file, char *buf, size_t len, loff_t *offset)
{
	struct vcnl4010 *dev = (struct vcnl4010 *)(file->private_data);
	struct i2c_adapter *adap = dev->client->adapter;
	struct i2c_msg msg;
	char *temp;
	int ret = 0;

	temp = kmalloc(len, GFP_KERNEL);

	msg.addr   = SLAVE_ADDRESS;
	msg.flags  = 0;
	msg.flags |= I2C_M_RD;
	msg.len    = len;
	msg.buf    = temp;

	ret = i2c_transfer(adap, &msg, 1);
	if (ret < 0) {
		pr_err("i2c_transfer() ERROR!");
	}

	if (ret >= 0) {
		ret = copy_to_user(buf, temp, len) ? -EFAULT : len;
	}

	kfree(temp);
	return ret;
}

static ssize_t vcnl4010_write(struct file *file, const char *buf,
				size_t len, loff_t *offset)
{
	struct vcnl4010 *dev = (struct vcnl4010 *)(file->private_data);
	struct i2c_adapter *adap = dev->client->adapter;
	struct i2c_msg msg;
	char *temp;
	int ret;

	temp = memdup_user(buf, len);
	if (IS_ERR(temp)) {
		pr_err("Failed to duplicate memory -> temp!");
	}

	msg.addr = SLAVE_ADDRESS;
	msg.flags = 0;
	msg.len = len;
	msg.buf = temp;

	ret = i2c_transfer(adap, &msg, 1);
	if (ret < 0) {
		pr_err("i2c_transfer() error!");
	}

	kfree(temp);
	return (ret == 1 ? len : ret);
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = vcnl4010_open,
	.release = vcnl4010_release,
	.read = vcnl4010_read,
	.write = vcnl4010_write,
};

static ssize_t command_reg_read_show(struct device *child,
			struct device_attribute *attr, char *buf)
{
	struct vcnl4010 *dev = dev_get_drvdata(child);
	u8 command_register_value   = 0x00;
	u8 command_register_address = 0x80;

	command_register_value = i2c_smbus_read_byte_data(dev->client,
						command_register_address);
	if (command_register_value < 0) {
		pr_err("command_register_value = %d", command_register_value);
		pr_err("%s function failed!", __func__);
		return -1;
	}

	pr_info("Command register (addr=0x%x), read value: 0x%.2x",
					command_register_address, command_register_value);

	return sprintf(buf, "%.2x\n", command_register_value);
}
static DEVICE_ATTR_RO(command_reg_read);

static ssize_t command_reg_write_store(struct device *child,
			struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf == NULL) {
		pr_err("Invalid writing to command register!");
		return -1;
	}

	struct vcnl4010 *dev = dev_get_drvdata(child);
	u8 command_register_value = buf[0];
	u8 command_register_address = 0x80;
	int write_success = -99;

	if (count > 2) {
		pr_err("Only byte writing to command register is allowed!");
		return -1;
	}

	write_success = i2c_smbus_write_byte_data(dev->client,
			command_register_address, command_register_value);
	if (write_success < 0) {
		pr_err("write_success = %d", write_success);
		pr_err("%s function failed!", __func__);
		return -1;
	}

	pr_info("Command register (addr=0x%x), written value: 0x%x",
				command_register_address, command_register_value);

	return count;
}
static DEVICE_ATTR_WO(command_reg_write);

static ssize_t proximity_rate_show(struct device *child,
			struct device_attribute *attr, char *buf)
{
	struct vcnl4010 *dev = dev_get_drvdata(child);
	u8 proximity_rate_register_value   = 0xFF;
	u8 proximity_rate_register_address = 0x82;

	proximity_rate_register_value = i2c_smbus_read_byte_data(dev->client,
					proximity_rate_register_address);
	if (proximity_rate_register_value < 0) {
		pr_err("proximity_rate_register_value = %d",
					proximity_rate_register_value);
		pr_err("i2c_smbus_read_byte_data(...) function failed!");
		return -1;
	}

	pr_info("Proximity rate register (addr=0x%x), read value: 0x%.2x",
			proximity_rate_register_address, proximity_rate_register_value);

	return sprintf(buf, "%.2x\n", proximity_rate_register_value);
}

static ssize_t proximity_rate_store(struct device *child,
			struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf == NULL) {
		pr_err("Invalid writing to command register!");
		return -1;
	}

	struct vcnl4010 *dev = dev_get_drvdata(child);
	u8 proximity_rate_register_value = buf[0];
	u8 proximity_rate_register_address = 0x82;
	int write_success = -99;

	proximity_rate_register_value &= PROXIMITY_RATE_REGISTER_MASK;

	if (count > 2) {
		pr_err("Only byte writing to proximity rate register is allowed!");
		return -1;
	}

	write_success = i2c_smbus_write_byte_data(dev->client,
			proximity_rate_register_address, proximity_rate_register_value);
	if (write_success < 0) {
		pr_err("write_success = %d", write_success);
		pr_err("i2c_smbus_write_byte_data(...) function failed!");
		return -1;
	}

	pr_info("Proximity rate register (addr=0x%x), written value: 0x%.2x\n",
			proximity_rate_register_address, proximity_rate_register_value);

	return count;
}
static DEVICE_ATTR_RW(proximity_rate);

static ssize_t ambient_light_high_read_show(struct device *child,
			struct device_attribute *attr, char *buf)
{
	struct vcnl4010 *dev = dev_get_drvdata(child);
	u8 ambient_light_high_reg_value = 0x00;
	u8 ambient_light_high_reg_address = 0x85;

	ambient_light_high_reg_value = i2c_smbus_read_byte_data(dev->client,
						ambient_light_high_reg_address);
	if (ambient_light_high_reg_value < 0) {
		pr_err("ambient_light_high_reg_value = %d",
				ambient_light_high_reg_value);
		pr_err("i2c_smbus_read_byte_data(...) function failed!");
		return -1;
	}

	pr_info("Ambient light(high byte) register (addr=0x%x), read value: 0x%.2x",
				ambient_light_high_reg_address, ambient_light_high_reg_value);

	return sprintf(buf, "%.2x\n", ambient_light_high_reg_value);
}
static DEVICE_ATTR_RO(ambient_light_high_read);

static ssize_t ambient_light_low_read_show(struct device *child,
							struct device_attribute *attr, char *buf)
{
	struct vcnl4010 *dev = dev_get_drvdata(child);
	u8 ambient_light_low_reg_value = 0x00;
	u8 ambient_light_low_reg_address = 0x86;

	ambient_light_low_reg_value = i2c_smbus_read_byte_data(dev->client,
						ambient_light_low_reg_address);
	if (ambient_light_low_reg_value < 0) {
		pr_err("ambient_light_low_reg_value = %d", ambient_light_low_reg_value);
		pr_err("i2c_smbus_read_byte_data(...) function failed!");
		return -1;
	}

	pr_info("Ambient light(low byte) register (addr=0x%x), read value: 0x%.2x",
				ambient_light_low_reg_address, ambient_light_low_reg_value);

	return sprintf(buf, "%.2x\n", ambient_light_low_reg_value);
}
static DEVICE_ATTR_RO(ambient_light_low_read);

static ssize_t proximity_high_read_show(struct device *child,
							struct device_attribute *attr, char *buf)
{
	struct vcnl4010 *dev = dev_get_drvdata(child);
	u8 proximity_high_reg_value = 0x00;
	u8 proximity_high_reg_address = 0x87;

	proximity_high_reg_value = i2c_smbus_read_byte_data(dev->client,
						proximity_high_reg_address);
	if (proximity_high_reg_value < 0) {
		pr_err("proximity_high_reg_value = %d", proximity_high_reg_value);
		pr_err("i2c_smbus_read_byte_data(...) function failed!");
		return -1;
	}

	pr_info("Proximity(high byte) register (addr=0x%x), read value: 0x%.2x",
						proximity_high_reg_address, proximity_high_reg_value);

	return sprintf(buf, "%.2x\n", proximity_high_reg_value);
}
static DEVICE_ATTR_RO(proximity_high_read);

static ssize_t proximity_low_read_show(struct device *child,
					struct device_attribute *attr, char *buf)
{
	struct vcnl4010 *dev = dev_get_drvdata(child);
	u8 proximity_low_reg_value = 0x00;
	u8 proximity_low_reg_address = 0x88;

	proximity_low_reg_value = i2c_smbus_read_byte_data(dev->client,
									proximity_low_reg_address);
	if (proximity_low_reg_value < 0) {
		pr_err("proximity_low_reg_value = %d", proximity_low_reg_value);
		pr_err("i2c_smbus_read_byte_data(...) function failed!");
		return -1;
	}

	pr_info("Proximity(low byte) register (addr=0x%x), read value: 0x%.2x",
						proximity_low_reg_address, proximity_low_reg_value);

	return sprintf(buf, "%.2x\n", proximity_low_reg_value);
}
static DEVICE_ATTR_RO(proximity_low_read);


static struct attribute *vcnl4010_device_attrs[] = {
	&dev_attr_command_reg_read.attr,
	&dev_attr_command_reg_write.attr,
	&dev_attr_proximity_rate.attr,
	&dev_attr_ambient_light_high_read.attr,
	&dev_attr_ambient_light_low_read.attr,
	&dev_attr_proximity_high_read.attr,
	&dev_attr_proximity_low_read.attr,
	NULL, // NULL is always last
};
ATTRIBUTE_GROUPS(vcnl4010_device);

static const struct of_device_id vcnl4010_dt_match[] = {
	{ .compatible = COMPATIBLE_FIELD_DT },
	{ }
};


static int vcnl4010_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct vcnl4010 *vcnl4010_device = NULL;
	struct device *device = NULL;
	int err = 0;

	vcnl4010_device = devm_kzalloc(&client->dev, sizeof(*vcnl4010_device),
						GFP_KERNEL);
	if (!vcnl4010_device)
		return -ENOMEM;

	i2c_set_clientdata(client, vcnl4010_device);

	vcnl4010_device->client = client;

	err = alloc_chrdev_region(&vcnl4010_device->dev, 0, 1, SLAVE_DEVICE_NAME);
	if (err < 0) {
		pr_info(KERN_ALERT "Unable to do device registration for %s",
						SLAVE_DEVICE_NAME);
		return err;
	}

	vcnl4010_device->class = class_create(THIS_MODULE, DRIVER_CLASS);
	if (IS_ERR(vcnl4010_device->class)) {
		err = PTR_ERR(vcnl4010_device->class);
		pr_err("Unable to create device class for %s", SLAVE_DEVICE_NAME);
		unregister_chrdev_region(vcnl4010_device->dev, 1);
	    return err;
	}

	device = device_create_with_groups(vcnl4010_device->class, NULL,
							vcnl4010_device->dev, vcnl4010_device,
							vcnl4010_device_groups, "%s%d",
							SLAVE_DEVICE_NAME, MINOR(vcnl4010_device->dev));
	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		pr_err("Unable to create device for %s", SLAVE_DEVICE_NAME);
		class_destroy(vcnl4010_device->class);
		unregister_chrdev_region(vcnl4010_device->dev, 1);
		return err;
	}

	cdev_init(&vcnl4010_device->cdev, &fops);

    err = cdev_add(&vcnl4010_device->cdev, vcnl4010_device->dev, 1);
	if (err < 0) {
		pr_err("Unable to add the device for %s", SLAVE_DEVICE_NAME);
		device_destroy(vcnl4010_device->class, vcnl4010_device->dev);
		class_destroy(vcnl4010_device->class);
		unregister_chrdev_region(vcnl4010_device->dev, 1);
		return err;
	}

	pr_info("Sensor initializing .......");

	u8 command_register_value   = 0xFF;
	u8 command_register_address = 0x80;

	command_register_value = i2c_smbus_write_byte_data(vcnl4010_device->client,
				command_register_address, command_register_value);

	u8 prox_rate_register_value   = 0x00;
	u8 prox_rate_register_address = 0x82;

	prox_rate_register_value = i2c_smbus_write_byte_data(vcnl4010_device->client,
				prox_rate_register_address, prox_rate_register_value);

	u8 als_register_value   = 0x9D;
	u8 als_register_address = 0x84;

	als_register_value = i2c_smbus_write_byte_data(vcnl4010_device->client,
				als_register_address, als_register_value);

	return 0;
}

static int vcnl4010_remove(struct i2c_client *client)
{
	struct vcnl4010 *vcnl4010_device;

    vcnl4010_device = i2c_get_clientdata(client);
    cdev_del(&vcnl4010_device->cdev);
    device_destroy(vcnl4010_device->class, vcnl4010_device->dev);
    pr_info("VCNL4010 - Device successfully destroyed!");
    class_destroy(vcnl4010_device->class);
    pr_info("VCNL4010 - Class successfully unregistered!");
    unregister_chrdev_region(vcnl4010_device->dev, 1);
    pr_info("VCNL4010 - Device number successfully freed!");

	return 0;
}

static const struct i2c_device_id vcnl4010_id[] = {
	{ SLAVE_DEVICE_NAME, 0 },
	{  }
};

MODULE_DEVICE_TABLE(of, vcnl4010_dt_match);
MODULE_DEVICE_TABLE(i2c, vcnl4010_id);

const static struct i2c_driver vcnl4010_driver = {
	.driver = {
		.name = SLAVE_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vcnl4010_dt_match),
	},
	.probe = vcnl4010_probe,
	.remove = vcnl4010_remove,
	.id_table = vcnl4010_id,
};

module_i2c_driver(vcnl4010_driver);

MODULE_AUTHOR("lukakrstic031@gmail.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Device driver for Mikroelektronika click board with
		proximity and temperature sensor, and VCNL4010 chip");
MODULE_SUPPORTED_DEVICE("NONE");
