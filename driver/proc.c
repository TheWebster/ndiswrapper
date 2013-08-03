/*
 *  Copyright (C) 2003-2005 Pontus Fuchs, Giridhar Pemmasani
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <asm/uaccess.h>

#include "ndis.h"
#include "iw_ndis.h"
#include "wrapndis.h"
#include "pnp.h"
#include "wrapper.h"

#define MAX_PROC_STR_LEN 32

static struct proc_dir_entry *procfs_hw,
                             *procfs_stats,
                             *procfs_encr,
                             *procfs_settings,
                             *procfs_debug,
                             *wrap_procfs_entry;

/* stats */

static int procfs_ndis_stats_show( struct seq_file *seqf, void *data)
{
	struct ndis_device *wnd = (struct ndis_device *)data;
	struct ndis_wireless_stats stats;
	NDIS_STATUS res;
	ndis_rssi rssi;

	/*if (off != 0) {
		*eof = 1;
		return 0;
	}*/

	res = mp_query(wnd, OID_802_11_RSSI, &rssi, sizeof(rssi));
	if (!res)
		seq_printf(seqf, "signal_level=%d dBm\n", (s32)rssi);

	res = mp_query(wnd, OID_802_11_STATISTICS, &stats, sizeof(stats));
	if (!res) {

		seq_printf(seqf, "tx_frames=%llu\n", stats.tx_frag);
		seq_printf(seqf, "tx_multicast_frames=%llu\n", stats.tx_multi_frag);
		seq_printf(seqf, "tx_failed=%llu\n", stats.failed);
		seq_printf(seqf, "tx_retry=%llu\n", stats.retry);
		seq_printf(seqf, "tx_multi_retry=%llu\n", stats.multi_retry);
		seq_printf(seqf, "tx_rtss_success=%llu\n", stats.rtss_succ);
		seq_printf(seqf, "tx_rtss_fail=%llu\n", stats.rtss_fail);
		seq_printf(seqf, "ack_fail=%llu\n", stats.ack_fail);
		seq_printf(seqf, "frame_duplicates=%llu\n", stats.frame_dup);
		seq_printf(seqf, "rx_frames=%llu\n", stats.rx_frag);
		seq_printf(seqf, "rx_multicast_frames=%llu\n", stats.rx_multi_frag);
		seq_printf(seqf, "fcs_errors=%llu\n", stats.fcs_err);
	}

	/* if (p - page > count) {
		ERROR("wrote %td bytes (limit is %u)\n",
		      p - page, count);
		*eof = 1;
	}*/

	return 0;
}

static int procfs_ndis_stats_open( struct inode *ino, struct file *f)
{
	return single_open( f, procfs_ndis_stats_show, PDE_DATA( ino));
};

static struct file_operations procfs_ndis_stats_ops = {
	.open    = procfs_ndis_stats_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};


/* encr */
static int procfs_ndis_encr_show( struct seq_file *seqf, void *data)
{
	struct ndis_device *wnd = (struct ndis_device *)data;
	int i, encr_status, auth_mode, infra_mode;
	NDIS_STATUS res;
	struct ndis_essid essid;
	mac_address ap_address;

	/*if (off != 0) {
		*eof = 1;
		return 0;
	}*/

	res = mp_query(wnd, OID_802_11_BSSID,
		       &ap_address, sizeof(ap_address));
	if (res)
		memset(ap_address, 0, ETH_ALEN);
	seq_printf(seqf, "ap_address=%2.2X", ap_address[0]);
	for (i = 1; i < ETH_ALEN; i++)
		seq_printf(seqf, ":%2.2X", ap_address[i]);
	seq_printf(seqf, "\n");

	res = mp_query(wnd, OID_802_11_SSID, &essid, sizeof(essid));
	if (!res)
		seq_printf(seqf, "essid=%.*s\n", essid.length, essid.essid);

	res = mp_query_int(wnd, OID_802_11_ENCRYPTION_STATUS, &encr_status);
	if (!res) {
		typeof(&wnd->encr_info.keys[0]) tx_key;
		seq_printf(seqf, "tx_key=%u\n", wnd->encr_info.tx_key_index);
		seq_printf(seqf, "key=");
		tx_key = &wnd->encr_info.keys[wnd->encr_info.tx_key_index];
		if (tx_key->length > 0)
			for (i = 0; i < tx_key->length; i++)
				seq_printf(seqf, "%2.2X", tx_key->key[i]);
		else
			seq_printf(seqf, "off");
		seq_printf(seqf, "\n");
		seq_printf(seqf, "encr_mode=%d\n", encr_status);
	}
	res = mp_query_int(wnd, OID_802_11_AUTHENTICATION_MODE, &auth_mode);
	if (!res)
		seq_printf(seqf, "auth_mode=%d\n", auth_mode);
	res = mp_query_int(wnd, OID_802_11_INFRASTRUCTURE_MODE, &infra_mode);
	seq_printf(seqf, "mode=%s\n", (infra_mode == Ndis802_11IBSS) ?
		     "adhoc" : (infra_mode == Ndis802_11Infrastructure) ?
		     "managed" : "auto");
	/*if (p - page > count) {
		WARNING("wrote %td bytes (limit is %u)",
			p - page, count);
		*eof = 1;
	}*/

	return 0;
}

static int procfs_ndis_encr_open( struct inode *ino, struct file *f)
{
	return single_open( f, procfs_ndis_encr_show, PDE_DATA( ino));
};

static struct file_operations procfs_ndis_encr_ops = {
	.open    = procfs_ndis_encr_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};


/* hw */

static int procfs_ndis_hw_show( struct seq_file *seqf, void *data)
{
	struct ndis_device *wnd = (struct ndis_device *)data;
	struct ndis_configuration config;
	enum ndis_power power_mode;
	NDIS_STATUS res;
	ndis_tx_power_level tx_power;
	ULONG bit_rate;
	ndis_rts_threshold rts_threshold;
	ndis_fragmentation_threshold frag_threshold;
	ndis_antenna antenna;
	ULONG packet_filter;
	int n;
	mac_address mac;
	char *hw_status[] = {"ready", "initializing", "resetting", "closing",
			     "not ready"};

	/*if (off != 0) {
		*eof = 1;
		return 0;
	}*/

	res = mp_query_int(wnd, OID_GEN_HARDWARE_STATUS, &n);
	if (res == NDIS_STATUS_SUCCESS && n >= 0 && n < ARRAY_SIZE(hw_status))
		seq_printf(seqf, "status=%s\n", hw_status[n]);

	res = mp_query(wnd, OID_802_3_CURRENT_ADDRESS, mac, sizeof(mac));
	if (!res)
		seq_printf(seqf, "mac: " MACSTRSEP "\n", MAC2STR(mac));
	res = mp_query(wnd, OID_802_11_CONFIGURATION, &config, sizeof(config));
	if (!res) {
		seq_printf(seqf, "beacon_period=%u msec\n",
			     config.beacon_period);
		seq_printf(seqf, "atim_window=%u msec\n", config.atim_window);
		seq_printf(seqf, "frequency=%u kHz\n", config.ds_config);
		seq_printf(seqf, "hop_pattern=%u\n", config.fh_config.hop_pattern);
		seq_printf(seqf, "hop_set=%u\n", config.fh_config.hop_set);
		seq_printf(seqf, "dwell_time=%u msec\n", config.fh_config.dwell_time);
	}

	res = mp_query(wnd, OID_802_11_TX_POWER_LEVEL,
		       &tx_power, sizeof(tx_power));
	if (!res)
		seq_printf(seqf, "tx_power=%u mW\n", tx_power);

	res = mp_query(wnd, OID_GEN_LINK_SPEED, &bit_rate, sizeof(bit_rate));
	if (!res)
		seq_printf(seqf, "bit_rate=%u kBps\n", (u32)bit_rate / 10);

	res = mp_query(wnd, OID_802_11_RTS_THRESHOLD,
		       &rts_threshold, sizeof(rts_threshold));
	if (!res)
		seq_printf(seqf, "rts_threshold=%u bytes\n", rts_threshold);

	res = mp_query(wnd, OID_802_11_FRAGMENTATION_THRESHOLD,
		       &frag_threshold, sizeof(frag_threshold));
	if (!res)
		seq_printf(seqf, "frag_threshold=%u bytes\n", frag_threshold);

	res = mp_query_int(wnd, OID_802_11_POWER_MODE, &power_mode);
	if (!res)
		seq_printf(seqf, "power_mode=%s\n",
			     (power_mode == NDIS_POWER_OFF) ? "always_on" :
			     (power_mode == NDIS_POWER_MAX) ?
			     "max_savings" : "min_savings");

	res = mp_query(wnd, OID_802_11_NUMBER_OF_ANTENNAS,
		       &antenna, sizeof(antenna));
	if (!res)
		seq_printf(seqf, "num_antennas=%u\n", antenna);

	res = mp_query(wnd, OID_802_11_TX_ANTENNA_SELECTED,
		       &antenna, sizeof(antenna));
	if (!res)
		seq_printf(seqf, "tx_antenna=%u\n", antenna);

	res = mp_query(wnd, OID_802_11_RX_ANTENNA_SELECTED,
		       &antenna, sizeof(antenna));
	if (!res)
		seq_printf(seqf, "rx_antenna=%u\n", antenna);

	seq_printf(seqf, "encryption_modes=%s%s%s%s%s%s%s\n",
		     test_bit(Ndis802_11Encryption1Enabled, &wnd->capa.encr) ?
		     "WEP" : "none",

		     test_bit(Ndis802_11Encryption2Enabled, &wnd->capa.encr) ?
		     "; TKIP with WPA" : "",
		     test_bit(Ndis802_11AuthModeWPA2, &wnd->capa.auth) ?
		     ", WPA2" : "",
		     test_bit(Ndis802_11AuthModeWPA2PSK, &wnd->capa.auth) ?
		     ", WPA2PSK" : "",

		     test_bit(Ndis802_11Encryption3Enabled, &wnd->capa.encr) ?
		     "; AES/CCMP with WPA" : "",
		     test_bit(Ndis802_11AuthModeWPA2, &wnd->capa.auth) ?
		     ", WPA2" : "",
		     test_bit(Ndis802_11AuthModeWPA2PSK, &wnd->capa.auth) ?
		     ", WPA2PSK" : "");

	res = mp_query_int(wnd, OID_GEN_CURRENT_PACKET_FILTER, &packet_filter);
	if (!res) {
		if (packet_filter != wnd->packet_filter)
			WARNING("wrong packet_filter? 0x%08x, 0x%08x\n",
				packet_filter, wnd->packet_filter);
		seq_printf(seqf, "packet_filter: 0x%08x\n", packet_filter);
	}
	/*if (p - page > count) {
		WARNING("wrote %td bytes (limit is %u)",
			p - page, count);
		*eof = 1;
	}*/

	return 0;
}

static int procfs_ndis_hw_open( struct inode *ino, struct file *f)
{
	return single_open( f, procfs_ndis_hw_show, PDE_DATA( ino));
};

static struct file_operations procfs_ndis_hw_ops = {
	.open    = procfs_ndis_hw_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};


/* settings */

static int procfs_ndis_settings_show( struct seq_file *seqf, void *data)
{
	struct ndis_device *wnd = (struct ndis_device *)data;
	struct wrap_device_setting *setting;

	/*if (off != 0) {
		*eof = 1;
		return 0;
	}*/

	seq_printf(seqf, "hangcheck_interval=%d\n",
		     hangcheck_interval == 0 ?
		     (wnd->hangcheck_interval / HZ) : -1);

	list_for_each_entry(setting, &wnd->wd->settings, list) {
		seq_printf(seqf, "%s=%s\n", setting->name, setting->value);
	}

	list_for_each_entry(setting, &wnd->wd->driver->settings, list) {
		seq_printf(seqf, "%s=%s\n", setting->name, setting->value);
	}

	return 0;
}

static int procfs_write_ndis_settings(struct file *file, const char __user *buf,
				      size_t size, loff_t *ppos)
{
	struct ndis_device *wnd = (struct ndis_device *)PDE_DATA( file->f_inode);
	char setting[MAX_PROC_STR_LEN], *p;
	unsigned int i;
	NDIS_STATUS res;

	if (size > MAX_PROC_STR_LEN)
		return -EINVAL;

	memset(setting, 0, sizeof(setting));
	if (copy_from_user(setting, buf, size))
		return -EFAULT;

	if ((p = strchr(setting, '\n')))
		*p = 0;

	if ((p = strchr(setting, '=')))
		*p = 0;

	if (!strcmp(setting, "hangcheck_interval")) {
		if (!p)
			return -EINVAL;
		p++;
		i = simple_strtol(p, NULL, 10);
		hangcheck_del(wnd);
		if (i > 0) {
			wnd->hangcheck_interval = i * HZ;
			hangcheck_add(wnd);
		}
	} else if (!strcmp(setting, "suspend")) {
		if (!p)
			return -EINVAL;
		p++;
		i = simple_strtol(p, NULL, 10);
		if (i <= 0 || i > 3)
			return -EINVAL;
		i = -1;
		if (wrap_is_pci_bus(wnd->wd->dev_bus))
			i = wrap_pnp_suspend_pci_device(wnd->wd->pci.pdev,
							PMSG_SUSPEND);
		else if (wrap_is_usb_bus(wnd->wd->dev_bus))
			i = wrap_pnp_suspend_usb_device(wnd->wd->usb.intf,
							PMSG_SUSPEND);
		if (i)
			return -EINVAL;
	} else if (!strcmp(setting, "resume")) {
		i = -1;
		if (wrap_is_pci_bus(wnd->wd->dev_bus))
			i = wrap_pnp_resume_pci_device(wnd->wd->pci.pdev);
		else if (wrap_is_usb_bus(wnd->wd->dev_bus))
			i = wrap_pnp_resume_usb_device(wnd->wd->usb.intf);
		if (i)
			return -EINVAL;
	} else if (!strcmp(setting, "stats_enabled")) {
		if (!p)
			return -EINVAL;
		p++;
		i = simple_strtol(p, NULL, 10);
		if (i > 0)
			wnd->iw_stats_enabled = TRUE;
		else
			wnd->iw_stats_enabled = FALSE;
	} else if (!strcmp(setting, "packet_filter")) {
		if (!p)
			return -EINVAL;
		p++;
		i = simple_strtol(p, NULL, 10);
		res = mp_set_int(wnd, OID_GEN_CURRENT_PACKET_FILTER, i);
		if (res)
			WARNING("setting packet_filter failed: %08X", res);
	} else if (!strcmp(setting, "reinit")) {
		if (ndis_reinit(wnd) != NDIS_STATUS_SUCCESS)
			return -EFAULT;
	} else {
		struct ndis_configuration_parameter param;
		struct unicode_string key;
		struct ansi_string ansi;

		if (!p)
			return -EINVAL;
		p++;
		RtlInitAnsiString(&ansi, p);
		if (RtlAnsiStringToUnicodeString(&param.data.string, &ansi,
						 TRUE) != STATUS_SUCCESS)
			EXIT1(return -EFAULT);
		param.type = NdisParameterString;
		RtlInitAnsiString(&ansi, setting);
		if (RtlAnsiStringToUnicodeString(&key, &ansi,
						 TRUE) != STATUS_SUCCESS) {
			RtlFreeUnicodeString(&param.data.string);
			EXIT1(return -EINVAL);
		}
		NdisWriteConfiguration(&res, wnd->nmb, &key, &param);
		RtlFreeUnicodeString(&key);
		RtlFreeUnicodeString(&param.data.string);
		if (res != NDIS_STATUS_SUCCESS)
			return -EFAULT;
	}
	return size;
}

static int procfs_ndis_settings_open( struct inode *ino, struct file *f)
{
	return single_open( f, procfs_ndis_settings_show, PDE_DATA( ino));
};

static struct file_operations procfs_ndis_settings_ops = {
	.open    = procfs_ndis_settings_open,
	.read    = seq_read,
	.write   = procfs_write_ndis_settings,
	.llseek  = seq_lseek,
	.release = seq_release
};


int wrap_procfs_add_ndis_device(struct ndis_device *wnd)
{
	if (wrap_procfs_entry == NULL)
		return -ENOMEM;

	if (wnd->procfs_iface) {
		ERROR("%s already registered?", wnd->net_dev->name);
/* NEEDS CLARIFICATION:
 * 'wnd->procfs_iface->name' changed to 'wnd->net_dev_name', as the 'proc_dir_entry'-struct
 * is now opaque.
 * Are they always the same?
 */
		return -EINVAL;
	}
	wnd->procfs_iface = proc_mkdir(wnd->net_dev->name, wrap_procfs_entry);
	if (wnd->procfs_iface == NULL) {
		ERROR("couldn't create proc directory");
		return -ENOMEM;
	}
	proc_set_user( wnd->procfs_iface, proc_uid, proc_gid);

	procfs_hw = proc_create_data("hw", S_IFREG | S_IRUSR | S_IRGRP, wnd->procfs_iface, &procfs_ndis_hw_ops, wnd);
	if (procfs_hw == NULL) {
		ERROR("couldn't create proc entry for 'hw'");
		goto err_hw;
	} else {
		proc_set_user( procfs_hw, proc_uid, proc_gid);
	}

	procfs_stats = proc_create_data("stats", S_IFREG | S_IRUSR | S_IRGRP, wnd->procfs_iface, &procfs_ndis_stats_ops, wnd);
	if (procfs_stats == NULL) {
		ERROR("couldn't create proc entry for 'stats'");
		goto err_stats;
	} else {
		proc_set_user( procfs_stats, proc_uid, proc_gid);
	}

	procfs_encr = proc_create_data("encr", S_IFREG | S_IRUSR | S_IRGRP, wnd->procfs_iface, &procfs_ndis_encr_ops, wnd);
	if (procfs_encr == NULL) {
		ERROR("couldn't create proc entry for 'encr'");
		goto err_encr;
	} else {
		proc_set_user( procfs_encr, proc_uid, proc_gid);
	}

	procfs_settings = proc_create_data("settings", S_IFREG |
					 S_IRUSR | S_IRGRP |
					 S_IWUSR | S_IWGRP, wnd->procfs_iface, &procfs_ndis_settings_ops, wnd);
	if (procfs_settings == NULL) {
		ERROR("couldn't create proc entry for 'settings'");
		goto err_settings;
	} else {
		proc_set_user( procfs_settings, proc_uid, proc_gid);
	}
	return 0;

err_settings:
	proc_remove( procfs_encr);
err_encr:
	proc_remove( procfs_stats);
err_stats:
	proc_remove( procfs_hw);
err_hw:
	proc_remove( wnd->procfs_iface);
	wnd->procfs_iface = NULL;
	return -ENOMEM;
}

void wrap_procfs_remove_ndis_device(struct ndis_device *wnd)
{
	struct proc_dir_entry *procfs_iface = xchg(&wnd->procfs_iface, NULL);

	if (procfs_iface == NULL)
		return;
	proc_remove( procfs_hw);
	proc_remove( procfs_stats);
	proc_remove( procfs_encr);
	proc_remove( procfs_settings);
	if (wrap_procfs_entry)
		proc_remove( procfs_iface);
}

/* debug */
static int procfs_debug_show( struct seq_file *seqf, void *data)
{
#if ALLOC_DEBUG
	enum alloc_type type;
#endif

	/*if (off != 0) {
		*eof = 1;
		return 0;
	}*/
	seq_printf(seqf, "%d\n", debug);
#if ALLOC_DEBUG
	for (type = 0; type < ALLOC_TYPE_MAX; type++)
		seq_printf(seqf, "total size of allocations in %s: %d\n",
			     alloc_type_name[type], alloc_size(type));
#endif
	return 0;
}

static int procfs_write_debug(struct file *file, const char __user *buf,
				      size_t size, loff_t *ppos)
{
	int i;
	char setting[MAX_PROC_STR_LEN], *p;

	if (size > MAX_PROC_STR_LEN)
		return -EINVAL;

	memset(setting, 0, sizeof(setting));
	if (copy_from_user(setting, buf, size))
		return -EFAULT;

	if ((p = strchr(setting, '\n')))
		*p = 0;

	if ((p = strchr(setting, '=')))
		*p = 0;

	i = simple_strtol(setting, NULL, 10);
	if (i >= 0 && i < 10)
		debug = i;
	else
		return -EINVAL;
	return size;
}

static int procfs_debug_open( struct inode *ino, struct file *f)
{
	return single_open( f, procfs_debug_show, NULL);
};

static struct file_operations procfs_debug_ops = {
	.open    = procfs_debug_open,
	.read    = seq_read,
	.write   = procfs_write_debug,
	.llseek  = seq_lseek,
	.release = seq_release
};

int wrap_procfs_init(void)
{
	wrap_procfs_entry = proc_mkdir(DRIVER_NAME, proc_net_root);
	if (wrap_procfs_entry == NULL) {
		ERROR("couldn't create procfs directory");
		return -ENOMEM;
	}
	proc_set_user( wrap_procfs_entry, proc_uid, proc_gid);

	procfs_debug = proc_create("debug", S_IFREG | S_IRUSR | S_IRGRP,
					 wrap_procfs_entry, &procfs_debug_ops);
	if (procfs_debug == NULL) {
		ERROR("couldn't create proc entry for 'debug'");
		return -ENOMEM;
	} else {
		proc_set_user( procfs_debug, proc_uid, proc_gid);
	}
	return 0;
}

void wrap_procfs_remove(void)
{
	if (wrap_procfs_entry == NULL)
		return;
	proc_remove( procfs_debug);
	proc_remove( wrap_procfs_entry);
}
