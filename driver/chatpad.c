// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2021 Severin von Wnuck <severinvonw@outlook.de>
 */

#include <linux/module.h>
#include <linux/hid.h>

#include "common.h"

struct gip_chatpad {
	struct gip_common common;

	struct hid_device *hid_dev;
};

static int gip_chatpad_hid_start(struct hid_device *dev)
{
	return 0;
}

static void gip_chatpad_hid_stop(struct hid_device *dev)
{
}

static int gip_chatpad_hid_open(struct hid_device *dev)
{
	return 0;
}

static void gip_chatpad_hid_close(struct hid_device *dev)
{
}

static int gip_chatpad_hid_parse(struct hid_device *dev)
{
	struct gip_chatpad *chatpad = dev->driver_data;
	struct gip_client *client = chatpad->common.client;
	struct hid_descriptor *desc;

	desc = (struct hid_descriptor *)client->hid_descriptor->data;
	if (desc->bLength < sizeof(*desc) || desc->bNumDescriptors != 1) {
		dev_err(&client->dev, "%s: invalid descriptor\n", __func__);
		return -EINVAL;
	}

	dev->version = le16_to_cpu(desc->bcdHID);
	dev->country = desc->bCountryCode;

	return hid_parse_report(dev,
				client->hid_descriptor->data + sizeof(*desc),
				client->hid_descriptor->count - sizeof(*desc));
}

static int gip_chatpad_hid_raw_request(struct hid_device *dev,
				       unsigned char report_num, __u8 *buf,
				       size_t len, unsigned char report_type,
				       int request_type)
{
	return 0;
}

static struct hid_ll_driver gip_chatpad_hid_driver = {
	.start = gip_chatpad_hid_start,
	.stop = gip_chatpad_hid_stop,
	.open = gip_chatpad_hid_open,
	.close = gip_chatpad_hid_close,
	.parse = gip_chatpad_hid_parse,
	.raw_request = gip_chatpad_hid_raw_request,
};

static int gip_chatpad_init_input(struct gip_chatpad *chatpad)
{
	struct gip_client *client = chatpad->common.client;
	struct input_dev *dev = chatpad->common.input_dev;
	int err;

	input_set_capability(dev, EV_KEY, BTN_MODE);

	err = input_register_device(dev);
	if (err)
		dev_err(&client->dev, "%s: register failed: %d\n",
			__func__, err);

	return err;
}

static int gip_chatpad_init_hid(struct gip_chatpad *chatpad)
{
	struct gip_client *client = chatpad->common.client;
	struct hid_device *dev;
	int err;

	dev = hid_allocate_device();
	if (IS_ERR(dev)) {
		dev_err(&client->dev, "%s: allocate failed: %ld\n",
			__func__, PTR_ERR(dev));
		return PTR_ERR(dev);
	}

	dev->bus = BUS_USB;
	dev->vendor = client->hardware.vendor;
	dev->product = client->hardware.product;
	dev->version = client->hardware.version;
	dev->dev.parent = &client->dev;
	dev->ll_driver = &gip_chatpad_hid_driver;

	strscpy(dev->name, chatpad->common.name, sizeof(dev->name));
	snprintf(dev->phys, sizeof(dev->phys), "%s/input1",
		 dev_name(&client->dev));

	dev->driver_data = chatpad;

	err = hid_add_device(dev);
	if (err) {
		dev_err(&client->dev, "%s: add failed: %d\n", __func__, err);
		hid_destroy_device(dev);
		return err;
	}

	chatpad->hid_dev = dev;

	return 0;
}

static int gip_chatpad_op_guide_button(struct gip_client *client, bool pressed)
{
	struct gip_chatpad *chatpad = dev_get_drvdata(&client->dev);
	struct input_dev *dev = chatpad->common.input_dev;

	input_report_key(dev, BTN_MODE, pressed);
	input_sync(dev);

	return 0;
}

static int gip_chatpad_op_hid_report(struct gip_client *client,
				     void *data, int len)
{
	struct gip_chatpad *chatpad = dev_get_drvdata(&client->dev);

	return hid_input_report(chatpad->hid_dev, HID_INPUT_REPORT,
				data, len, true);
}

static int gip_chatpad_probe(struct gip_client *client)
{
	struct gip_chatpad *chatpad;
	struct gip_info_element *hid_desc = client->hid_descriptor;
	int err;

	if (!hid_desc || hid_desc->count < sizeof(struct hid_descriptor))
		return -ENODEV;

	chatpad = devm_kzalloc(&client->dev, sizeof(*chatpad), GFP_KERNEL);
	if (!chatpad)
		return -ENOMEM;

	chatpad->common.client = client;
	chatpad->common.name = "Microsoft X-Box One chatpad";

	err = gip_init_input(&chatpad->common);
	if (err)
		return err;

	err = gip_chatpad_init_input(chatpad);
	if (err)
		return err;

	err = gip_chatpad_init_hid(chatpad);
	if (err)
		return err;

	err = gip_set_power_mode(client, GIP_PWR_ON);
	if (err)
		return err;

	dev_set_drvdata(&client->dev, chatpad);

	return 0;
}

static void gip_chatpad_remove(struct gip_client *client)
{
	struct gip_chatpad *chatpad = dev_get_drvdata(&client->dev);

	hid_destroy_device(chatpad->hid_dev);
	dev_set_drvdata(&client->dev, NULL);
}

static struct gip_driver gip_chatpad_driver = {
	.name = "xone-gip-chatpad",
	.class = "Windows.Xbox.Input.Chatpad",
	.ops = {
		.guide_button = gip_chatpad_op_guide_button,
		.hid_report = gip_chatpad_op_hid_report,
	},
	.probe = gip_chatpad_probe,
	.remove = gip_chatpad_remove,
};
module_gip_driver(gip_chatpad_driver);

MODULE_ALIAS("gip:Windows.Xbox.Input.Chatpad");
MODULE_AUTHOR("Severin von Wnuck <severinvonw@outlook.de>");
MODULE_DESCRIPTION("xone GIP chatpad driver");
MODULE_VERSION("#VERSION#");
MODULE_LICENSE("GPL");
