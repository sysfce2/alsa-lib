/*
 *  PCM - Hardware
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "pcm_local.h"

#define SND_FILE_PCM_PLAYBACK		"/dev/snd/pcmC%iD%ip"
#define SND_FILE_PCM_CAPTURE		"/dev/snd/pcmC%iD%ic"
#define SND_PCM_VERSION_MAX	SND_PROTOCOL_VERSION(2, 0, 0)

static int snd_pcm_hw_channel_close(snd_pcm_t *pcm, int channel)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	if (hw->chan[channel].fd >= 0)
		if (close(hw->chan[channel].fd))
			return -errno;
	return 0;
}

int snd_pcm_hw_channel_fd(snd_pcm_t *pcm, int channel)
{
	snd_pcm_hw_t *hw;
	if (!pcm)
		return -EINVAL;
	if (channel < 0 || channel > 1)
		return -EINVAL;
	hw = (snd_pcm_hw_t*) &pcm->private;
	return hw->chan[channel].fd;
}

static int snd_pcm_hw_channel_nonblock(snd_pcm_t *pcm, int channel, int nonblock)
{
	long flags;
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[channel].fd;

	if ((flags = fcntl(fd, F_GETFL)) < 0)
		return -errno;
	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	int fd, channel;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	for (channel = 0; channel < 2; ++channel) {
		fd = hw->chan[channel].fd;
		if (fd >= 0)
			break;
	}
	if (fd < 0)
		return -EBADFD;
	if (ioctl(fd, SND_PCM_IOCTL_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[info->channel].fd;
	if (fd < 0)
		return -EINVAL;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_INFO, info) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t * params)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[params->channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_PARAMS, params) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t * setup)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[setup->channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_SETUP, setup) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_voice_setup(snd_pcm_t *pcm, int channel, snd_pcm_voice_setup_t * setup)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_VOICE_SETUP, setup) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_status(snd_pcm_t *pcm, snd_pcm_channel_status_t * status)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[status->channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_STATUS, status) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_update(snd_pcm_t *pcm, int channel)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_UPDATE) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_prepare(snd_pcm_t *pcm, int channel)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_PREPARE) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_go(snd_pcm_t *pcm, int channel)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_GO) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_sync_go(snd_pcm_t *pcm, snd_pcm_sync_t *sync)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	if (pcm->chan[SND_PCM_CHANNEL_PLAYBACK].open)
		fd = hw->chan[SND_PCM_CHANNEL_PLAYBACK].fd;
	else
		fd = hw->chan[SND_PCM_CHANNEL_CAPTURE].fd;
	if (ioctl(fd, SND_PCM_IOCTL_SYNC_GO, sync) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_drain(snd_pcm_t *pcm, int channel)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_DRAIN) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_flush(snd_pcm_t *pcm, int channel)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_FLUSH) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_channel_pause(snd_pcm_t *pcm, int channel, int enable)
{
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[channel].fd;
	if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_PAUSE, &enable) < 0)
		return -errno;
	return 0;
}

static ssize_t snd_pcm_hw_write(snd_pcm_t *pcm, const void *buffer, size_t size)
{
	ssize_t result;
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[SND_PCM_CHANNEL_PLAYBACK].fd;
	result = write(fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_writev(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	ssize_t result;
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[SND_PCM_CHANNEL_PLAYBACK].fd;
#if 0
	result = writev(fd, vector, count);
#else
	{
		snd_v_args_t args;
		args.vector = vector;
		args.count = count;
		result = ioctl(fd, SND_IOCTL_WRITEV, &args);
	}
#endif
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_pcm_hw_read(snd_pcm_t *pcm, void *buffer, size_t size)
{
	ssize_t result;
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[SND_PCM_CHANNEL_CAPTURE].fd;
	result = read(fd, buffer, size);
	if (result < 0)
		return -errno;
	return result;
}

ssize_t snd_pcm_hw_readv(snd_pcm_t *pcm, const struct iovec *vector, unsigned long count)
{
	ssize_t result;
	int fd;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	fd = hw->chan[SND_PCM_CHANNEL_CAPTURE].fd;
#if 0
	result = readv(fd, vector, count);
#else
	{
		snd_v_args_t args;
		args.vector = vector;
		args.count = count;
		result = ioctl(fd, SND_IOCTL_READV, &args);
	}
#endif
	if (result < 0)
		return -errno;
	return result;
}

static int snd_pcm_hw_mmap_control(snd_pcm_t *pcm, int channel, snd_pcm_mmap_control_t **control, size_t csize)
{
	void *caddr;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	caddr = mmap(NULL, csize, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, 
		     hw->chan[channel].fd, SND_PCM_MMAP_OFFSET_CONTROL);
	if (caddr == MAP_FAILED || caddr == NULL)
		return -errno;
	*control = caddr;
	return 0;
}

static int snd_pcm_hw_mmap_data(snd_pcm_t *pcm, int channel, void **buffer, size_t bsize)
{
	int prot;
	void *daddr;
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	prot = channel == SND_PCM_CHANNEL_PLAYBACK ? PROT_WRITE : PROT_READ;
	daddr = mmap(NULL, bsize, prot, MAP_FILE|MAP_SHARED, 
		     hw->chan[channel].fd, SND_PCM_MMAP_OFFSET_DATA);
	if (daddr == MAP_FAILED || daddr == NULL)
		return -errno;
	*buffer = daddr;
	return 0;
}

static int snd_pcm_hw_munmap_control(snd_pcm_t *pcm UNUSED, int channel UNUSED, snd_pcm_mmap_control_t *control, size_t csize)
{
	if (munmap(control, csize) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_munmap_data(snd_pcm_t *pcm UNUSED, int channel UNUSED, void *buffer, size_t bsize)
{
	if (munmap(buffer, bsize) < 0)
		return -errno;
	return 0;
}

static int snd_pcm_hw_file_descriptor(snd_pcm_t *pcm, int channel)
{
	snd_pcm_hw_t *hw = (snd_pcm_hw_t*) &pcm->private;
	return hw->chan[channel].fd;
}

static int snd_pcm_hw_voices_mask(snd_pcm_t *pcm UNUSED, int channel UNUSED,
				  bitset_t *client_vmask UNUSED)
{
	return 0;
}

struct snd_pcm_ops snd_pcm_hw_ops = {
	channel_close: snd_pcm_hw_channel_close,
	channel_nonblock: snd_pcm_hw_channel_nonblock,
	info: snd_pcm_hw_info,
	channel_info: snd_pcm_hw_channel_info,
	channel_params: snd_pcm_hw_channel_params,
	channel_setup: snd_pcm_hw_channel_setup,
	voice_setup: snd_pcm_hw_voice_setup,
	channel_status: snd_pcm_hw_channel_status,
	channel_update: snd_pcm_hw_channel_update,
	channel_prepare: snd_pcm_hw_channel_prepare,
	channel_go: snd_pcm_hw_channel_go,
	sync_go: snd_pcm_hw_sync_go,
	channel_drain: snd_pcm_hw_channel_drain,
	channel_flush: snd_pcm_hw_channel_flush,
	channel_pause: snd_pcm_hw_channel_pause,
	write: snd_pcm_hw_write,
	writev: snd_pcm_hw_writev,
	read: snd_pcm_hw_read,
	readv: snd_pcm_hw_readv,
	mmap_control: snd_pcm_hw_mmap_control,
	mmap_data: snd_pcm_hw_mmap_data,
	munmap_control: snd_pcm_hw_munmap_control,
	munmap_data: snd_pcm_hw_munmap_data,
	file_descriptor: snd_pcm_hw_file_descriptor,
	voices_mask: snd_pcm_hw_voices_mask,
};

static int snd_pcm_hw_open_channel(int card, int device, int channel, int subdevice, int fmode, snd_ctl_t *ctl, int *ver)
{
	char filename[32];
	char *filefmt;
	int err, fd;
	int attempt = 0;
	snd_pcm_channel_info_t info;
	switch (channel) {
	case SND_PCM_CHANNEL_PLAYBACK:
		filefmt = SND_FILE_PCM_PLAYBACK;
		break;
	case SND_PCM_CHANNEL_CAPTURE:
		filefmt = SND_FILE_PCM_CAPTURE;
		break;
	default:
		return -EINVAL;
	}
	if ((err = snd_ctl_pcm_channel_prefer_subdevice(ctl, device, channel, subdevice)) < 0)
		return err;
	sprintf(filename, filefmt, card, device);

      __again:
      	if (attempt++ > 3) {
      		snd_ctl_close(ctl);
      		return -EBUSY;
      	}
	if ((fd = open(filename, fmode)) < 0) {
		err = -errno;
		return err;
	}
	if (ioctl(fd, SND_PCM_IOCTL_PVERSION, ver) < 0) {
		err = -errno;
		close(fd);
		return err;
	}
	if (SND_PROTOCOL_INCOMPATIBLE(*ver, SND_PCM_VERSION_MAX)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	if (subdevice >= 0) {
		memset(&info, 0, sizeof(info));
		if (ioctl(fd, SND_PCM_IOCTL_CHANNEL_INFO, &info) < 0) {
			err = -errno;
			close(fd);
			return err;
		}
		if (info.subdevice != subdevice) {
			close(fd);
			goto __again;
		}
	}
	return fd;
}

int snd_pcm_open_subdevice(snd_pcm_t **handle, int card, int device, int subdevice, int mode)
{
	int fmode, ver, err;
	snd_pcm_t *pcm;
	snd_pcm_hw_t *hw;
	snd_ctl_t *ctl;
	int pfd = -1, cfd = -1;

	if (!handle)
		return -EFAULT;
	*handle = NULL;
	
	if (card < 0 || card >= SND_CARDS)
		return -EINVAL;
	if ((err = snd_ctl_open(&ctl, card)) < 0)
		return err;
	if (mode & SND_PCM_OPEN_PLAYBACK) {
		fmode = O_RDWR;
		if (mode & SND_PCM_NONBLOCK_PLAYBACK)
			fmode |= O_NONBLOCK;
		pfd = snd_pcm_hw_open_channel(card, device, SND_PCM_CHANNEL_PLAYBACK,
					  subdevice, fmode, ctl, &ver);
		if (pfd < 0) {
			snd_ctl_close(ctl);
			return pfd;
		}
	}
	if (mode & SND_PCM_OPEN_CAPTURE) {
		fmode = O_RDWR;
		if (mode & SND_PCM_NONBLOCK_CAPTURE)
			fmode |= O_NONBLOCK;
		cfd = snd_pcm_hw_open_channel(card, device, SND_PCM_CHANNEL_CAPTURE,
					  subdevice, fmode, ctl, &ver);
		if (cfd < 0) {
			if (pfd >= 0)
				close(pfd);
			snd_ctl_close(ctl);
			return cfd;
		}
	}
	snd_ctl_close(ctl);
	if (pfd < 0 && cfd < 0)
		return -EINVAL;

	err = snd_pcm_abstract_open(handle, mode, SND_PCM_TYPE_HW, sizeof(snd_pcm_hw_t));
	if (err < 0) {
		if (pfd >= 0)
			close(pfd);
		if (cfd >= 0)
			close(cfd);
		return err;
	}
	pcm = *handle;
	pcm->ops = &snd_pcm_hw_ops;
	hw = (snd_pcm_hw_t*) &pcm->private;
	hw->card = card;
	hw->device = device;
	hw->ver = ver;
	hw->chan[SND_PCM_CHANNEL_PLAYBACK].fd = pfd;
	hw->chan[SND_PCM_CHANNEL_CAPTURE].fd = cfd;
	return 0;
}

int snd_pcm_open(snd_pcm_t **handle, int card, int device, int mode)
{
	return snd_pcm_open_subdevice(handle, card, device, -1, mode);
}

