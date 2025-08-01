/**
 * \file pcm/pcm.c
 * \ingroup PCM
 * \brief PCM Interface
 * \author Jaroslav Kysela <perex@perex.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2000-2001
 *
 * PCM Interface is designed to write or read digital audio frames. A
 * frame is the data unit converted into/from sound in one time unit
 * (1/rate seconds), by example if you set your playback PCM rate to
 * 44100 you'll hear 44100 frames per second. The size in bytes of a
 * frame may be obtained from bits needed to store a sample and
 * channels count.
 *
 * See the \ref pcm page for more details.
 */
/*
 *  PCM Interface - main file
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@perex.cz>
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*! \page pcm PCM (digital audio) interface

<P>Although abbreviation PCM stands for Pulse Code Modulation, we are
understanding it as general digital audio processing with volume samples
generated in continuous time periods.</P>

<P>The analog signal is recorded via analog to digital converters (ADC).
The digital value (de-facto a volume at a specific time) obtained
from ADC can be further processed. The following picture shows a perfect
sinus waveform:</P>

<BR>
\image html wave1.gif

<P>Next image shows digitized representation:</P>

<BR>
\image html wave2.gif

<P>As you may see, the quality of digital audio signal depends on the time
(recording rate) and voltage resolution (usually in an linear integer
representation with basic unit one bit).</P>

<P>The stored digital signal can be converted back to voltage (analog)
representation via digital to analog converters (DAC).</P>

<P>One digital value is called sample. More samples are collected to frames
(frame is terminology for ALSA) depending on count of converters used at one
specific time. One frame might contain one sample (when only one converter is
used - mono) or more samples (for example: stereo has signals from two converters
recorded at same time). Digital audio stream contains collection of frames
recorded at boundaries of continuous time periods.</P>

\section pcm_general_overview General overview

ALSA uses the ring buffer to store outgoing (playback) and incoming (capture,
record) samples. There are two pointers being maintained to allow
a precise communication between application and device pointing to current
processed sample by hardware and last processed sample by application.
The modern audio chips allow to program the transfer time periods.
It means that the stream of samples is divided to small chunks. Device
acknowledges to application when the transfer of a chunk is complete.

\section pcm_transfer Transfer methods in UNIX environments

In the UNIX environment, data chunk acknowledges are received via standard I/O
calls or event waiting routines (poll or select function). To accomplish
this list, the asynchronous notification of acknowledges should be listed
here. The ALSA implementation for these methods is described in
the \ref alsa_transfers section.

\subsection pcm_transfer_io Standard I/O transfers

The standard I/O transfers are using the read (see 'man 2 read') and write
(see 'man 2 write') C functions. There are two basic behaviours of these
functions - blocked and non-blocked (see the O_NONBLOCK flag for the
standard C open function - see 'man 2 open'). In non-blocked behaviour,
these I/O functions never stops, they return -EAGAIN error code, when no
data can be transferred (the ring buffer is full in our case). In blocked
behaviour, these I/O functions stop and wait until there is a room in the
ring buffer (playback) or until there are new samples (capture). The ALSA
implementation can be found in the \ref alsa_pcm_rw section.

\subsection pcm_transfer_event Event waiting routines

The poll or select functions (see 'man 2 poll' or 'man 2 select' for further
details) allows to receive requests/events from the device while
an application is waiting on events from other sources (like keyboard, screen,
network etc.), too. \ref snd_pcm_poll_descriptors can be used to get file
descriptors to poll or select on (note that wait direction might be different
than expected - do not use only returned file descriptors, but handle
events member as well - see \ref snd_pcm_poll_descriptors function
description for more details and \ref snd_pcm_poll_descriptors_revents for
events demangling). The implemented transfer routines can be found in
the \ref alsa_transfers section.

\subsection pcm_transfer_async Asynchronous notification

ALSA driver and library knows to handle the asynchronous notifications over
the SIGIO signal. This signal allows to interrupt application and transfer
data in the signal handler. For further details see the sigaction function
('man 2 sigaction'). The section \ref pcm_async describes the ALSA API for
this extension. The implemented transfer routines can be found in the
\ref alsa_transfers section.

\section pcm_open_behaviour Blocked and non-blocked open

The ALSA PCM API uses a different behaviour when the device is opened
with blocked or non-blocked mode. The mode can be specified with
\a mode argument in #snd_pcm_open() function.
The blocked mode is the default (without #SND_PCM_NONBLOCK mode).
In this mode, the behaviour is that if the resources have already used
with another application, then it blocks the caller, until resources are
free. The non-blocked behaviour (with #SND_PCM_NONBLOCK)
doesn't block the caller in any way and returns -EBUSY error when the
resources are not available. Note that the mode also determines the
behaviour of standard I/O calls, returning -EAGAIN when non-blocked mode is
used and the ring buffer is full (playback) or empty (capture).
The operation mode for I/O calls can be changed later with
the #snd_pcm_nonblock() function.

\section pcm_async Asynchronous mode

There is also possibility to receive asynchronous notification after
specified time periods. You may see the #SND_PCM_ASYNC
mode for #snd_pcm_open() function and
#snd_async_add_pcm_handler() function for further details.

\section pcm_handshake Handshake between application and library

The ALSA PCM API design uses the states to determine the communication
phase between application and library. The actual state can be determined
using #snd_pcm_state() call. There are these states:

\par SND_PCM_STATE_OPEN
The PCM device is in the open state. After the #snd_pcm_open() open call,
the device is in this state. Also, when #snd_pcm_hw_params() call fails,
then this state is entered to force application calling 
#snd_pcm_hw_params() function to set right communication
parameters.

\par SND_PCM_STATE_SETUP
The PCM device has accepted communication parameters and it is waiting
for #snd_pcm_prepare() call to prepare the hardware for
selected operation (playback or capture).

\par SND_PCM_STATE_PREPARED
The PCM device is prepared for operation. Application can use
#snd_pcm_start() call, write or read data to start
the operation.

\par SND_PCM_STATE_RUNNING
The PCM device has been started and is running. It processes the samples. The stream can
be stopped using the #snd_pcm_drop() or
#snd_pcm_drain() calls.

\par SND_PCM_STATE_XRUN
The PCM device reached overrun (capture) or underrun (playback).
You can use the -EPIPE return code from I/O functions
(#snd_pcm_writei(), #snd_pcm_writen(), #snd_pcm_readi(), #snd_pcm_readn())
to determine this state without checking
the actual state via #snd_pcm_state() call. It is recommended to use
the helper function #snd_pcm_recover() to recover from this state, but you can also use #snd_pcm_prepare(),
#snd_pcm_drop() or #snd_pcm_drain() calls.

\par SND_PCM_STATE_DRAINING
The device is in this state when application using the capture mode
called #snd_pcm_drain() function. Until all data are
read from the internal ring buffer using I/O routines
(#snd_pcm_readi(), #snd_pcm_readn()),
then the device stays in this state.

\par SND_PCM_STATE_PAUSED
The device is in this state when application called
the #snd_pcm_pause() function until the pause is released.
Not all hardware supports this feature. Application should check the
capability with the #snd_pcm_hw_params_can_pause().

\par SND_PCM_STATE_SUSPENDED
The device is in the suspend state provoked with the power management
system. The stream can be resumed using #snd_pcm_resume()
call, but not all hardware supports this feature. Application should check
the capability with the #snd_pcm_hw_params_can_resume().
In other case, the calls #snd_pcm_prepare(),
#snd_pcm_drop(), #snd_pcm_drain() can be used
to leave this state.

\par SND_PCM_STATE_DISCONNECTED
The device is physicaly disconnected. It does not accept any I/O calls in this state.

\section pcm_formats PCM formats

The full list of formats present the #snd_pcm_format_t type.
The 24-bit linear samples use 32-bit physical space, but the sample is
stored in the lower three bytes. Some hardware does not support processing of full
range, thus you may get the significant bits for linear samples via
#snd_pcm_hw_params_get_sbits() function. The example: ICE1712
chips support 32-bit sample processing, but low byte is ignored (playback)
or zero (capture). The function snd_pcm_hw_params_get_sbits()
returns 24 in this case. The significant bits are related to the usable
sample bits (width) not the physical sample space.

\section alsa_transfers ALSA transfers

There are two methods to transfer samples in application. The first method
is the standard read / write one. The second method, uses the direct audio
buffer to communicate with the device while ALSA library manages this space
itself. You can find examples of all communication schemes for playback
in \link example_test_pcm Sine-wave generator example \endlink. To complete the
list, we should note that #snd_pcm_wait() function contains
embedded poll waiting implementation.

\subsection alsa_pcm_rw Read / Write transfer

There are two versions of read / write routines. The first expects the
interleaved samples at input (#SND_PCM_ACCESS_RW_INTERLEAVED access method),
and the second one expects non-interleaved (samples in separated buffers -
#SND_PCM_ACCESS_RW_NONINTERLEAVED access method) at input. There are these
functions for interleaved transfers: #snd_pcm_writei()
#snd_pcm_readi(). For non-interleaved transfers, there are
these functions: #snd_pcm_writen() and #snd_pcm_readn().

\subsection alsa_mmap_rw Direct Read / Write transfer (via mmap'ed areas)

Three kinds of organization of ring buffer memory areas exist in ALSA API.
Access #SND_PCM_ACCESS_MMAP_INTERLEAVED has interleaved samples. Access
#SND_PCM_ACCESS_MMAP_NONINTERLEAVED expects continous sample areas for
one channel. Access #SND_PCM_ACCESS_MMAP_COMPLEX does not fit to interleaved
and non-interleaved ring buffer organization.

There are two functions for this kind of transfer. Application can get an
access to memory areas via #snd_pcm_mmap_begin() function.
This function returns the areas (single area is equal to a channel)
containing the direct pointers to memory and sample position description
in #snd_pcm_channel_area_t structure. After application
transfers the data in the memory areas, then it must be acknowledged
the end of transfer via #snd_pcm_mmap_commit() function
to allow the ALSA library update the pointers to ring buffer. This kind of
communication is also called "zero-copy", because the device does not require
to copy the samples from application to another place in system memory.

If you like to use the compatibility functions in mmap mode, there are
read / write routines equaling to standard read / write transfers. Using
these functions discards the benefits of direct access to memory region.
See the #snd_pcm_mmap_readi(),
#snd_pcm_mmap_writei(), #snd_pcm_mmap_readn()
and #snd_pcm_mmap_writen() functions. These functions use
#snd_pcm_areas_copy() internally.

\section pcm_errors Error codes

\par -EPIPE

This error means xrun (underrun for playback or overrun for capture).
The underrun can happen when an application does not feed new samples
in time to alsa-lib (due CPU usage). The overrun can happen when
an application does not take new captured samples in time from alsa-lib.

\par -ESTRPIPE

This error means that system has suspended drivers. The application
should wait in loop when snd_pcm_resume() != -EAGAIN and then
call snd_pcm_prepare() when snd_pcm_resume() return an error code.
If snd_pcm_resume() does not fail (a zero value is returned), driver
supports resume and the snd_pcm_prepare() call can be ommited.

\par -EBADFD

This error means that the device is in a bad state. It means that
the handshake between application and alsa-lib is corrupted.

\par -ENOTTY, -ENODEV

This error can happen when device is physically removed (for example
some hotplug devices like USB or PCMCIA, CardBus or ExpressCard
can be removed on the fly).

\par -ENODATA

This error can happen if the device data transfer is dependent on
an external condition and that condition is not met. For example,
PCM device for echo reference as described by SND_USE_CASE_MOD_ECHO_REF
UCM token, may return -ENODATA if the linked playback stream has not been
started.

There is no defined recovery or event mechanism to signal the data / link
availability at the moment. The PCM must be completely restarted until
the mechanism is designed. The #snd_pcm_recover() function cannot be
used for this.

\section pcm_params Managing parameters

The ALSA PCM device uses two groups of PCM related parameters. The hardware
parameters contains the stream description like format, rate, count of
channels, ring buffer size etc. The software parameters contains the
software (driver) related parameters. The communication behaviour can be
controlled via these parameters, like automatic start, automatic stop,
interrupting (chunk acknowledge) etc. The software parameters can be
modified at any time (when valid hardware parameters are set). It includes
the running state as well.

\subsection pcm_hw_params Hardware related parameters

The ALSA PCM devices use the parameter refining system for hardware
parameters - #snd_pcm_hw_params_t. It means, that
application choose the full-range of configurations at first and then
application sets single parameters until all parameters are elementary
(definite).

\par Access modes

ALSA knows about five access modes. The first three can be used for direct
communication. The access mode #SND_PCM_ACCESS_MMAP_INTERLEAVED
determines the direct memory area and interleaved sample organization.
Interleaved organization means, that samples from channels are mixed together.
The access mode #SND_PCM_ACCESS_MMAP_NONINTERLEAVED
determines the direct memory area and non-interleaved sample organization.
Each channel has a separate buffer in the case. The complex direct memory
organization represents the #SND_PCM_ACCESS_MMAP_COMPLEX
access mode. The sample organization does not fit the interleaved or
non-interleaved access modes in the case. The last two access modes
describes the read / write access methods.
The #SND_PCM_ACCESS_RW_INTERLEAVED access represents the read /
write interleaved access and the #SND_PCM_ACCESS_RW_NONINTERLEAVED
represents the non-interleaved access.

\par Formats

The full list of formats is available in #snd_pcm_format_t
enumeration.

\subsection pcm_sw_params Software related parameters

These parameters - #snd_pcm_sw_params_t can be modified at
any time including the running state.

\par Minimum available count of frames

This parameter controls the wakeup point. If the count of available frames
is equal or greater than this value, then application will be activated.

\par Timestamp mode

The timestamp mode specifies, if timestamps are activated. Currently, only
#SND_PCM_TSTAMP_NONE and #SND_PCM_TSTAMP_MMAP
modes are known. The mmap mode means that timestamp is taken
on every period time boundary. Corresponding position in the ring buffer
assigned to timestamp can be obtained using #snd_pcm_htimestamp() function.

\par Transfer align

The read / write transfers can be aligned to this sample count. The modulo
is ignored by device. Usually, this value is set to one (no align).

\par Start threshold

The start threshold parameter is used to determine the start point in
stream. For playback, if the frame count in the ring buffer is equal or greater
than the start threshold parameter and the stream is not running, the stream
will be started automatically from the device. For capture, if the application
wants to read count of frames equal or greater then the stream will be started.
If you want to use explicit start (#snd_pcm_start), you can set this value
greater than the ring buffer size (in frames). For that simply using a large
constant such as LONG_MAX or the boundary value is not a bad idea.

\par Stop threshold

Similarly, the stop threshold parameter is used to automatically stop
the running stream, when the available frames crosses this boundary.
It means, for playback, the empty samples in ring buffer and for capture,
the filled (used) samples in ring buffer.

\par Silence threshold

The silence threshold specifies the count of frames before an underrun when the
buffer gets filled with frames of silence according to the silence size parameter
ahead of the current application pointer for playback. It is usable for applications
when an underrun is possible (like tasks depending on network I/O etc.). If
application wants to manage the ahead samples itself, the #snd_pcm_rewind() function
allows to forget the last samples in the stream.

\section pcm_status Obtaining stream status

The stream status is stored in #snd_pcm_status_t structure.
These parameters can be obtained: the current stream state -
#snd_pcm_status_get_state(), timestamp of trigger -
#snd_pcm_status_get_trigger_tstamp(), timestamp of last
pointer update #snd_pcm_status_get_tstamp(), delay in frames -
#snd_pcm_status_get_delay(), available count in frames -
#snd_pcm_status_get_avail(), maximum available frames -
#snd_pcm_status_get_avail_max(), ADC over-range count in
frames - #snd_pcm_status_get_overrange(). The last two
parameters - avail_max and overrange are reset to zero after the status
call.

\subsection pcm_status_fast Obtaining stream state fast and update r/w pointer

<p>
The function #snd_pcm_avail_update() updates the current
available count of frames for writing (playback) or filled frames for
reading (capture). This call is mandatory for updating actual r/w pointer.
Using standalone, it is a light method to obtain current stream position,
because it does not require the user <-> kernel context switch, but the value
is less accurate, because ring buffer pointers are updated in kernel drivers
only when an interrupt occurs. If you want to get accurate stream state,
use functions #snd_pcm_avail(), #snd_pcm_delay() or #snd_pcm_avail_delay().
</p>
<p>
The function #snd_pcm_avail() reads the current hardware pointer
in the ring buffer from hardware and calls #snd_pcm_avail_update() then.
</p>
<p>
The function #snd_pcm_delay() returns the delay in frames.
For playback, it means count of frames in the ring buffer before
the next frames will be sent to DAC. For capture, it means count of frames
in the ring buffer before the next frames will be captured from ADC. It works
only when the stream is in the running or draining (playback only) state.
Note that this function does not update the current r/w pointer for applications,
so the function #snd_pcm_avail_update() must be called afterwards
before any read/write begin+commit operations.
</p>
<p>
The function #snd_pcm_avail_delay() combines #snd_pcm_avail() and
#snd_pcm_delay() and returns both values in sync.
</p>

\section pcm_action Managing the stream state

The following functions directly and indirectly affect the stream state:

\par snd_pcm_hw_params
The #snd_pcm_hw_params() function brings the stream state
to #SND_PCM_STATE_SETUP
if successfully finishes, otherwise the state #SND_PCM_STATE_OPEN
is entered.
When it is brought to SETUP state, this function automatically
calls #snd_pcm_prepare() function to bring to the PREPARED state
as below.

\par snd_pcm_prepare
The #snd_pcm_prepare() function enters from #SND_PCM_STATE_SETUP
to the #SND_PCM_STATE_PREPARED after a successful finish.

\par snd_pcm_start
The #snd_pcm_start() function enters
the #SND_PCM_STATE_RUNNING after a successful finish.

\par snd_pcm_drop
The #snd_pcm_drop() function enters the
#SND_PCM_STATE_SETUP state.

\par snd_pcm_drain
The #snd_pcm_drain() function enters the
#SND_PCM_STATE_DRAINING, if
the capture device has some samples in the ring buffer otherwise
#SND_PCM_STATE_SETUP state is entered.

\par snd_pcm_pause
The #snd_pcm_pause() function enters the
#SND_PCM_STATE_PAUSED or #SND_PCM_STATE_RUNNING.

\par snd_pcm_writei, snd_pcm_writen
The #snd_pcm_writei() and #snd_pcm_writen()
functions can conditionally start the stream -
#SND_PCM_STATE_RUNNING. They depend on the start threshold
software parameter.

\par snd_pcm_readi, snd_pcm_readn
The #snd_pcm_readi() and #snd_pcm_readn()
functions can conditionally start the stream -
#SND_PCM_STATE_RUNNING. They depend on the start threshold
software parameter.

\section pcm_sync Streams synchronization

There are two functions allowing link multiple streams together. In the
case, the linking means that all operations are synchronized. Because the
drivers cannot guarantee the synchronization (sample resolution) on hardware
lacking this feature, the #snd_pcm_hw_params_get_sync() function
returns 16-byte synchronization ID, which is equal
for hardware synchronized streams. When the #snd_pcm_link()
function is called, all operations managing the stream state for these two
streams are joined. The opposite function is #snd_pcm_unlink().

\section pcm_thread_safety Thread-safety

When the library is configured with the proper option, some PCM functions
(e.g. #snd_pcm_avail_update()) are thread-safe and can be called concurrently
from multiple threads.  Meanwhile, some functions (e.g. #snd_pcm_hw_params())
aren't thread-safe, and application needs to call them carefully when they
are called from multiple threads.  In general, all the functions that are
often called during streaming are covered as thread-safe.

This thread-safe behavior can be disabled also by passing 0 to the environment
variable LIBASOUND_THREAD_SAFE, e.g.
\code
LIBASOUND_THREAD_SAFE=0 aplay foo.wav
\endcode
for making the debugging easier.

\section pcm_dev_names PCM naming conventions

The ALSA library uses a generic string representation for names of devices.
The devices might be virtual, physical or a mix of both. The generic string
is passed to #snd_pcm_open() or #snd_pcm_open_lconf().
It contains two parts: device name and arguments. Devices and arguments are described
in configuration files. The usual place for default definitions is at /usr/share/alsa/alsa.conf.
For detailed descriptions about integrated PCM plugins look to \ref pcm_plugins.

\subsection pcm_dev_names_default Default device

The default device is equal to plug plugin with hw plugin as slave. The defaults are
used:

\code
defaults.pcm.card 0
defaults.pcm.device 0
defaults.pcm.subdevice -1
\endcode

These defaults can be freely overwritten in local configuration files.

Example:

\code
default
\endcode

\subsection pcm_dev_names_hw HW device

The hw device description uses the hw plugin. The three arguments (in order: CARD,DEV,SUBDEV)
specify card number or identifier, device number and subdevice number (-1 means any).

Example:

\code
hw
hw:0
hw:0,0
hw:supersonic,1
hw:soundwave,1,2
hw:DEV=1,CARD=soundwave,SUBDEV=2
\endcode

\subsection pcm_dev_names_plughw Plug->HW device

The plughw device description uses the plug plugin and hw plugin as slave. The arguments
are same as for hw device.

Example:

\code
plughw
plughw:0
plughw:0,0
plughw:supersonic,1
plughw:soundwave,1,2
plughw:DEV=1,CARD=soundwave,SUBDEV=2
\endcode

\subsection pcm_dev_names_plug Plug device

The plug device uses the plug plugin. The one SLAVE argument specifies the slave plugin.

Example:

\code
plug:mypcmdef
plug:hw
plug:'hw:0,0'
plug:SLAVE=hw
\endcode

\subsection pcm_dev_names_shm Shared memory device

The shm device uses the shm plugin. The two arguments (in order: SOCKET,PCM) specify
UNIX socket name (for example /tmp/alsa.socket) for server communication and server's PCM name.

Example:

\code
shm:'/tmp/alsa.sock',default
shm:SOCKET='/tmp/alsa.sock',PCM=default
\endcode

\subsection pcm_dev_names_tee Tee device

The tee device stores contents of a stream to given file plus transfers it to given slave plugin.
The three arguments (in order: SLAVE,FILE,FORMAT) specify slave plugin, filename and file format.

Example:

\code
tee:hw,'/tmp/out.raw',raw
\endcode

\subsection pcm_dev_names_file File device

The file device is file plugin with null plugin as slave. The arguments (in order: FILE,FORMAT)
specify filename and file format.

Example:

\code
file:'/tmp/out.raw',raw
\endcode

\subsection pcm_dev_names_null Null device

The null device is null plugin. This device has not any arguments.


\section pcm_examples Examples

The full featured examples with cross-links can be found in Examples section
(see top of page):

\par Sine-wave generator
\par
\link example_test_pcm alsa-lib/test/pcm.c \endlink
example shows various transfer methods for the playback direction.

\par Minimalistic PCM playback code
\par
\link example_test_minimal alsa-lib/test/pcm_min.c \endlink
example shows the minimal code to produce a sound.

\par Latency measuring tool
\par
\link example_test_latency alsa-lib/test/latency.c \endlink
example shows the measuring of minimal latency between capture and
playback devices.

*/

/**
\example ../../test/pcm.c
\anchor example_test_pcm
Shows various transfer methods for the playback direction.
*/
/**
\example ../../test/pcm_min.c
\anchor example_test_minimal
Shows the minimal code to produce a sound.
*/
/**
\example ../../test/latency.c
\anchor example_test_latency
Shows the measuring of minimal latency between capture and
playback devices.
*/

#include "pcm_local.h"
#include <stdio.h>
#include <string.h>
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdarg.h>
#include <signal.h>
#include <ctype.h>
#include <poll.h>
#include <sys/mman.h>
#include <limits.h>

#ifndef DOC_HIDDEN
/* return specific error codes for known bad PCM states */
static int pcm_state_to_error(snd_pcm_state_t state)
{
	switch (state) {
	case SND_PCM_STATE_XRUN:
		return -EPIPE;
	case SND_PCM_STATE_SUSPENDED:
		return -ESTRPIPE;
	case SND_PCM_STATE_DISCONNECTED:
		return -ENODEV;
	default:
		return 0;
	}
}

#define P_STATE(x)	(1U << SND_PCM_STATE_ ## x)
#define P_STATE_RUNNABLE (P_STATE(PREPARED) | \
			  P_STATE(RUNNING) | \
			  P_STATE(XRUN) | \
			  P_STATE(PAUSED) | \
			  P_STATE(DRAINING))

/* check whether the PCM is in the unexpected state */
static int bad_pcm_state(snd_pcm_t *pcm, unsigned int supported_states,
			 unsigned int noop_states)
{
	snd_pcm_state_t state;
	int err;

	if (pcm->own_state_check)
		return 0; /* don't care, the plugin checks by itself */
	state = snd_pcm_state(pcm);
	if (noop_states & (1U << state))
		return 1; /* OK, return immediately */
	if (supported_states & (1U << state))
		return 0; /* OK */
	err = pcm_state_to_error(state);
	if (err < 0)
		return err;
	return -EBADFD;
}
#endif

/**
 * \brief get identifier of PCM handle
 * \param pcm PCM handle
 * \return ascii identifier of PCM handle
 *
 * Returns the ASCII identifier of given PCM handle. It's the same
 * identifier specified in snd_pcm_open().
 */
const char *snd_pcm_name(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->name;
}

/**
 * \brief get type of PCM handle
 * \param pcm PCM handle
 * \return type of PCM handle
 *
 * Returns the type #snd_pcm_type_t of given PCM handle.
 */
snd_pcm_type_t snd_pcm_type(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->type;
}

/**
 * \brief get stream for a PCM handle
 * \param pcm PCM handle
 * \return stream of PCM handle
 *
 * Returns the type #snd_pcm_stream_t of given PCM handle.
 */
snd_pcm_stream_t snd_pcm_stream(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->stream;
}

/**
 * \brief close PCM handle
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 *
 * Closes the specified PCM handle and frees all associated
 * resources.
 */
int snd_pcm_close(snd_pcm_t *pcm)
{
	int res = 0, err;
	assert(pcm);
	while (!list_empty(&pcm->async_handlers)) {
		snd_async_handler_t *h = list_entry(pcm->async_handlers.next, snd_async_handler_t, hlist);
		snd_async_del_handler(h);
	}
	if (pcm->setup && !pcm->donot_close) {
		snd_pcm_drop(pcm);
		err = snd_pcm_hw_free(pcm);
		if (err < 0)
			res = err;
	}
	if (pcm->mmap_channels)
		snd_pcm_munmap(pcm);
	if (pcm->ops->close)
		err = pcm->ops->close(pcm->op_arg);
	else
		err = -ENOSYS;
	if (err < 0)
		res = err;
	err = snd_pcm_free(pcm);
	if (err < 0)
		res = err;
	return res;
}	

/**
 * \brief set nonblock mode
 * \param pcm PCM handle
 * \param nonblock 0 = block, 1 = nonblock mode, 2 = abort
 * \return 0 on success otherwise a negative error code
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_nonblock(snd_pcm_t *pcm, int nonblock)
{
	int err = 0;

	assert(pcm);
	/* FIXME: __snd_pcm_lock() call below is commented out because of the
	 * the possible deadlock in signal handler calling snd_pcm_abort()
	 */
	/* __snd_pcm_lock(pcm->op_arg); */ /* forced lock due to pcm field change */
	if (pcm->ops->nonblock)
		err = pcm->ops->nonblock(pcm->op_arg, nonblock);
	else
		err = -ENOSYS;
	if (err < 0)
		goto unlock;
	if (nonblock == 2) {
		pcm->mode |= SND_PCM_ABORT;
		goto unlock;
	}
	if (nonblock)
		pcm->mode |= SND_PCM_NONBLOCK;
	else {
		if (pcm->hw_flags & SND_PCM_HW_PARAMS_NO_PERIOD_WAKEUP)
			err = -EINVAL;
		else
			pcm->mode &= ~SND_PCM_NONBLOCK;
	}
 unlock:
	/* __snd_pcm_unlock(pcm->op_arg); */ /* FIXME: see above */
	return err;
}

#ifndef DOC_HIDDEN
/**
 * \brief set async mode
 * \param pcm PCM handle
 * \param sig Signal to raise: < 0 disable, 0 default (SIGIO)
 * \param pid Process ID to signal: 0 current
 * \return 0 on success otherwise a negative error code
 *
 * A signal is raised every period.
 */
int snd_pcm_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	int err = 0;

	assert(pcm);
	if (sig == 0)
		sig = SIGIO;
	if (pid == 0)
		pid = getpid();

#ifdef THREAD_SAFE_API
	/* async handler may lead to a deadlock; suppose no multi thread */
	pcm->lock_enabled = 0;
#endif
	if (pcm->ops->async)
		err = pcm->ops->async(pcm->op_arg, sig, pid);
	else
		err = -ENOSYS;
	return err;
}
#endif

/**
 * \brief Obtain general (static) information for PCM handle
 * \param pcm PCM handle
 * \param info Information container
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	int err = 0;

	assert(pcm && info);
	if (pcm->ops->info)
		err = pcm->ops->info(pcm->op_arg, info);
	else
		err = -ENOSYS;
	return err;
}

/** \brief Retreive current PCM hardware configuration chosen with #snd_pcm_hw_params
 * \param pcm PCM handle
 * \param params Configuration space definition container
 * \return 0 on success otherwise a negative error code
 * \retval -EBADFD no hardware configuration is set
 */
int snd_pcm_hw_params_current(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	unsigned int frame_bits;

	assert(pcm && params);
	if (!pcm->setup)
		return -EBADFD;
	memset(params, 0, snd_pcm_hw_params_sizeof());
	params->flags = pcm->hw_flags;
	snd_mask_set(&params->masks[SND_PCM_HW_PARAM_ACCESS - SND_PCM_HW_PARAM_FIRST_MASK], pcm->access);
	snd_mask_set(&params->masks[SND_PCM_HW_PARAM_FORMAT - SND_PCM_HW_PARAM_FIRST_MASK], pcm->format);
	snd_mask_set(&params->masks[SND_PCM_HW_PARAM_SUBFORMAT - SND_PCM_HW_PARAM_FIRST_MASK], pcm->subformat);
	frame_bits = snd_pcm_format_physical_width(pcm->format) * pcm->channels;
	snd_interval_set_value(&params->intervals[SND_PCM_HW_PARAM_FRAME_BITS - SND_PCM_HW_PARAM_FIRST_INTERVAL], frame_bits);
	snd_interval_set_value(&params->intervals[SND_PCM_HW_PARAM_CHANNELS - SND_PCM_HW_PARAM_FIRST_INTERVAL], pcm->channels);
	snd_interval_set_value(&params->intervals[SND_PCM_HW_PARAM_RATE - SND_PCM_HW_PARAM_FIRST_INTERVAL], pcm->rate);
	snd_interval_set_value(&params->intervals[SND_PCM_HW_PARAM_PERIOD_TIME - SND_PCM_HW_PARAM_FIRST_INTERVAL], pcm->period_time);
	snd_interval_set_value(&params->intervals[SND_PCM_HW_PARAM_PERIOD_SIZE - SND_PCM_HW_PARAM_FIRST_INTERVAL], pcm->period_size);
	snd_interval_copy(&params->intervals[SND_PCM_HW_PARAM_PERIODS - SND_PCM_HW_PARAM_FIRST_INTERVAL], &pcm->periods);
	snd_interval_copy(&params->intervals[SND_PCM_HW_PARAM_BUFFER_TIME - SND_PCM_HW_PARAM_FIRST_INTERVAL], &pcm->buffer_time);
	snd_interval_set_value(&params->intervals[SND_PCM_HW_PARAM_BUFFER_SIZE - SND_PCM_HW_PARAM_FIRST_INTERVAL], pcm->buffer_size);
	snd_interval_set_value(&params->intervals[SND_PCM_HW_PARAM_BUFFER_BYTES - SND_PCM_HW_PARAM_FIRST_INTERVAL], (pcm->buffer_size * frame_bits) / 8);
	params->info = pcm->info;
	params->msbits = pcm->msbits;
	params->rate_num = pcm->rate_num;
	params->rate_den = pcm->rate_den;
	params->fifo_size = pcm->fifo_size;
	return 0;
} 

/** \brief Install one PCM hardware configuration chosen from a configuration space and #snd_pcm_prepare it
 * \param pcm PCM handle
 * \param params Configuration space definition container
 * \return 0 on success otherwise a negative error code
 *
 * The configuration is chosen fixing single parameters in this order:
 * first access, first format, first subformat, min channels, min rate, 
 * min period time, max buffer size, min tick time. If no mutually
 * compatible set of parameters can be chosen, a negative error code
 * will be returned.
 *
 * After this call, #snd_pcm_prepare() is called automatically and
 * the stream is brought to \c #SND_PCM_STATE_PREPARED state.
 *
 * The hardware parameters cannot be changed when the stream is
 * running (active). The software parameters can be changed
 * at any time.
 *
 * The configuration space will be updated to reflect the chosen
 * parameters.
 */
int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	int err;
	assert(pcm && params);
	err = _snd_pcm_hw_params_internal(pcm, params);
	if (err < 0)
		return err;
	err = snd_pcm_prepare(pcm);
	return err;
}

/** \brief Remove PCM hardware configuration and free associated resources
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 *
 * The function will also report success if no configuration is set.
 */
int snd_pcm_hw_free(snd_pcm_t *pcm)
{
	int err;
	if (! pcm->setup)
		return 0;
	if (pcm->mmap_channels) {
		err = snd_pcm_munmap(pcm);
		if (err < 0)
			return err;
	}
	// assert(snd_pcm_state(pcm) == SND_PCM_STATE_SETUP ||
	//        snd_pcm_state(pcm) == SND_PCM_STATE_PREPARED);
	if (pcm->ops->hw_free)
		err = pcm->ops->hw_free(pcm->op_arg);
	else
		err = -ENOSYS;
	pcm->setup = 0;
	if (err < 0)
		return err;
	return 0;
}

/** \brief Install PCM software configuration defined by params
 * \param pcm PCM handle
 * \param params Configuration container
 * \return 0 on success otherwise a negative error code
 *
 * The software parameters can be changed at any time.
 * The hardware parameters cannot be changed when the stream is
 * running (active).
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	int err;
	/* the hw_params must be set at first!!! */
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	if (! params->avail_min) {
		SNDMSG("params->avail_min is 0");
		return -EINVAL;
	}
#if 0
	/* disable the check below - it looks too restrictive
	 * (start_threshold is basically independent from avail_min)
	 */
	if (params->start_threshold <= pcm->buffer_size &&
	    params->start_threshold > (pcm->buffer_size / params->avail_min) * params->avail_min) {
		SNDMSG("params->avail_min problem for start_threshold");
		return -EINVAL;
	}
#endif
	__snd_pcm_lock(pcm->op_arg); /* forced lock due to pcm field change */
	if (pcm->ops->sw_params)
		err = pcm->ops->sw_params(pcm->op_arg, params);
	else
		err = -ENOSYS;
	if (err < 0) {
		__snd_pcm_unlock(pcm->op_arg);
		return err;
	}
	pcm->tstamp_mode = params->tstamp_mode;
	pcm->tstamp_type = params->tstamp_type;
	pcm->period_step = params->period_step;
	pcm->avail_min = params->avail_min;
	pcm->period_event = sw_get_period_event(params);
	pcm->start_threshold = params->start_threshold;
	pcm->stop_threshold = params->stop_threshold;
	pcm->silence_threshold = params->silence_threshold;
	pcm->silence_size = params->silence_size;
	pcm->boundary = params->boundary;
	__snd_pcm_unlock(pcm->op_arg);
	return 0;
}

/**
 * \brief Obtain status (runtime) information for PCM handle
 * \param pcm PCM handle
 * \param status Status container
 * \return 0 on success otherwise a negative error code
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_status(snd_pcm_t *pcm, snd_pcm_status_t *status)
{
	int err;

	assert(pcm && status);
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->status)
		err = pcm->fast_ops->status(pcm->fast_op_arg, status);
	else
		err = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);

	return err;
}

/**
 * \brief Return PCM state
 * \param pcm PCM handle
 * \return PCM state #snd_pcm_state_t of given PCM handle
 *
 * This is a faster way to obtain only the PCM state without calling
 * \link ::snd_pcm_status() \endlink.
 *
 * Note that this function always returns one of the
 * #snd_pcm_state_t enum variants.
 * It will never return a negative error code.
 *
 * The function is thread-safe when built with the proper option.
 */
snd_pcm_state_t snd_pcm_state(snd_pcm_t *pcm)
{
	snd_pcm_state_t state;

	assert(pcm);
	snd_pcm_lock(pcm->fast_op_arg);
	state = __snd_pcm_state(pcm);
	snd_pcm_unlock(pcm->fast_op_arg);
	return state;
}

/**
 * \brief (DEPRECATED) Synchronize stream position with hardware
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 *
 * Note this function does not update the actual r/w pointer
 * for applications. The function #snd_pcm_avail_update()
 * have to be called before any mmap begin+commit operation.
 *
 * The function is thread-safe when built with the proper option.
 *
 * This function is deprecated. Use #snd_pcm_avail_update() instead.
 */
int snd_pcm_hwsync(snd_pcm_t *pcm)
{
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	snd_pcm_lock(pcm->fast_op_arg);
	err = __snd_pcm_hwsync(pcm);
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

/**
 * \brief Obtain delay for a running PCM handle
 * \param pcm PCM handle
 * \param delayp Returned delay in frames
 * \return 0 on success otherwise a negative error code
 *
 * For playback the delay is defined as the time that a frame that is written
 * to the PCM stream shortly after this call will take to be actually
 * audible. It is as such the overall latency from the write call to the final
 * DAC.
 *
 * For capture the delay is defined as the time that a frame that was
 * digitized by the audio device takes until it can be read from the PCM
 * stream shortly after this call returns. It is as such the overall latency
 * from the initial ADC to the read call.
 *
 * Please note that hence in case of a playback underrun this value will not
 * necessarily got down to 0.
 *
 * If the application is interested in the fill level of the playback buffer
 * of the device, it should use #snd_pcm_avail*() functions. The
 * value returned by that call is not directly related to the delay, since the
 * latter might include some additional, fixed latencies the former does not.
 *
 * Note this function does not update the actual r/w pointer
 * for applications. The function #snd_pcm_avail_update()
 * have to be called before any begin+commit operation.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	snd_pcm_lock(pcm->fast_op_arg);
	err = __snd_pcm_delay(pcm, delayp);
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

/**
 * \brief Resume from suspend, no samples are lost
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 * \retval -EAGAIN resume can't be proceed immediately (audio hardware is probably still suspended)
 * \retval -ENOSYS hardware doesn't support this feature
 *
 * This function can be used when the stream is in the suspend state
 * to do the fine resume from this state. Not all hardware supports
 * this feature, when an -ENOSYS error is returned, use the \link ::snd_pcm_prepare() \endlink
 * function to recovery.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_resume(snd_pcm_t *pcm)
{
	int err = 0;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	/* lock handled in the callback */
	if (pcm->fast_ops->resume)
		err = pcm->fast_ops->resume(pcm->fast_op_arg);
	else
		err = -ENOSYS;
	return err;
}

/**
 * \brief Obtain last position update hi-res timestamp
 * \param pcm PCM handle
 * \param avail Number of available frames when timestamp was grabbed
 * \param tstamp Hi-res timestamp
 * \return 0 on success otherwise a negative error code
 *
 * Note this function does not update the actual r/w pointer
 * for applications.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_htimestamp(snd_pcm_t *pcm, snd_pcm_uframes_t *avail, snd_htimestamp_t *tstamp)
{
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->htimestamp)
		err = pcm->fast_ops->htimestamp(pcm->fast_op_arg, avail, tstamp);
	else
		err = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

/**
 * \brief Prepare PCM for use
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_prepare(snd_pcm_t *pcm)
{
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	err = bad_pcm_state(pcm, ~P_STATE(DISCONNECTED), 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->prepare)
		err = pcm->fast_ops->prepare(pcm->fast_op_arg);
	else
		err = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

/**
 * \brief Reset PCM position
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 *
 * Reduce PCM delay to 0.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_reset(snd_pcm_t *pcm)
{
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->reset)
		err = pcm->fast_ops->reset(pcm->fast_op_arg);
	else
		err = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

/**
 * \brief Start a PCM
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_start(snd_pcm_t *pcm)
{
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	err = bad_pcm_state(pcm, P_STATE(PREPARED), 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	err = __snd_pcm_start(pcm);
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

/**
 * \brief Stop a PCM dropping pending frames
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 *
 * This function stops the PCM <i>immediately</i>.
 * The pending samples on the buffer are ignored.
 *
 * For processing all pending samples, use \link ::snd_pcm_drain() \endlink
 * instead.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_drop(snd_pcm_t *pcm)
{
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE | P_STATE(SETUP) |
			    P_STATE(SUSPENDED), 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->drop)
		err = pcm->fast_ops->drop(pcm->fast_op_arg);
	else
		err = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

/**
 * \brief Stop a PCM preserving pending frames
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 * \retval -ESTRPIPE a suspend event occurred
 *
 * For playback wait for all pending frames to be played and then stop
 * the PCM.
 * For capture stop PCM permitting to retrieve residual frames.
 *
 * For stopping the PCM stream immediately, use \link ::snd_pcm_drop() \endlink
 * instead.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_drain(snd_pcm_t *pcm)
{
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE | P_STATE(SETUP), P_STATE(SETUP));
	if (err < 0)
		return err;
	if (err == 1)
		return 0;
	/* lock handled in the callback */
	if (pcm->fast_ops->drain)
		err = pcm->fast_ops->drain(pcm->fast_op_arg);
	else
		err = -ENOSYS;
	return err;
}

/**
 * \brief Pause/resume PCM
 * \param pcm PCM handle
 * \param enable 0 = resume, 1 = pause
 * \return 0 on success otherwise a negative error code
 *
 * Note that this function works only on the hardware which supports
 * pause feature.  You can check it via \link ::snd_pcm_hw_params_can_pause() \endlink
 * function.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_pause(snd_pcm_t *pcm, int enable)
{
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->pause)
		err = pcm->fast_ops->pause(pcm->fast_op_arg, enable);
	else
		err = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

/**
 * \brief Get safe count of frames which can be rewinded
 * \param pcm PCM handle
 * \return a positive number of frames or negative error code
 *
 * Note: The snd_pcm_rewind() can accept bigger value than returned
 * by this function. But it is not guaranteed that output stream
 * will be consistent with bigger value.
 *
 * The function is thread-safe when built with the proper option.
 */
snd_pcm_sframes_t snd_pcm_rewindable(snd_pcm_t *pcm)
{
	snd_pcm_sframes_t result;
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->rewindable)
		result = pcm->fast_ops->rewindable(pcm->fast_op_arg);
	else
		result = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);
	return result;
}

/**
 * \brief Move application frame position backward
 * \param pcm PCM handle
 * \param frames wanted displacement in frames
 * \return a positive number for actual displacement otherwise a
 * negative error code
 *
 * The function is thread-safe when built with the proper option.
 */
snd_pcm_sframes_t snd_pcm_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_sframes_t result;
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	if (frames == 0)
		return 0;
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->rewind)
		result = pcm->fast_ops->rewind(pcm->fast_op_arg, frames);
	else
		result = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);
	return result;
}

/**
 * \brief Get safe count of frames which can be forwarded
 * \param pcm PCM handle
 * \return a positive number of frames or negative error code
 *
 * Note: The snd_pcm_forward() can accept bigger value than returned
 * by this function. But it is not guaranteed that output stream
 * will be consistent with bigger value.
 *
 * The function is thread-safe when built with the proper option.
 */
snd_pcm_sframes_t snd_pcm_forwardable(snd_pcm_t *pcm)
{
	snd_pcm_sframes_t result;
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->forwardable)
		result = pcm->fast_ops->forwardable(pcm->fast_op_arg);
	else
		result = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);
	return result;
}

/**
 * \brief Move application frame position forward
 * \param pcm PCM handle
 * \param frames wanted skip in frames
 * \return a positive number for actual skip otherwise a negative error code
 * \retval 0 means no action
 *
 * The function is thread-safe when built with the proper option.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL snd_pcm_sframes_t INTERNAL(snd_pcm_forward)(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
#else
snd_pcm_sframes_t snd_pcm_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
#endif
{
	snd_pcm_sframes_t result;
	int err;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	if (frames == 0)
		return 0;
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	if (pcm->fast_ops->forward)
		result = pcm->fast_ops->forward(pcm->fast_op_arg, frames);
	else
		result = -ENOSYS;
	snd_pcm_unlock(pcm->fast_op_arg);
	return result;
}
use_default_symbol_version(__snd_pcm_forward, snd_pcm_forward, ALSA_0.9.0rc8);

/**
 * \brief Write interleaved frames to a PCM
 * \param pcm PCM handle
 * \param buffer frames containing buffer
 * \param size frames to be written
 * \return a positive number of frames actually written otherwise a
 * negative error code
 * \retval -EBADFD PCM is not in the right state (#SND_PCM_STATE_PREPARED or #SND_PCM_STATE_RUNNING)
 * \retval -EPIPE an underrun occurred
 * \retval -ESTRPIPE a suspend event occurred (stream is suspended and waiting for an application recovery)
 *
 * If the blocking behaviour is selected and it is running, then routine waits until
 * all requested frames are played or put to the playback ring buffer.
 * The returned number of frames can be less only if a signal or underrun occurred.
 *
 * If the non-blocking behaviour is selected, then routine doesn't wait at all.
 *
 * The function is thread-safe when built with the proper option.
 */ 
snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	int err;

	assert(pcm);
	assert(size == 0 || buffer);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	if (pcm->access != SND_PCM_ACCESS_RW_INTERLEAVED) {
		SNDMSG("invalid access type %s", snd_pcm_access_name(pcm->access));
		return -EINVAL;
	}
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	return _snd_pcm_writei(pcm, buffer, size);
}

/**
 * \brief Write non interleaved frames to a PCM
 * \param pcm PCM handle
 * \param bufs frames containing buffers (one for each channel)
 * \param size frames to be written
 * \return a positive number of frames actually written otherwise a
 * negative error code
 * \retval -EBADFD PCM is not in the right state (#SND_PCM_STATE_PREPARED or #SND_PCM_STATE_RUNNING)
 * \retval -EPIPE an underrun occurred
 * \retval -ESTRPIPE a suspend event occurred (stream is suspended and waiting for an application recovery)
 *
 * If the blocking behaviour is selected and it is running, then routine waits until
 * all requested frames are played or put to the playback ring buffer.
 * The returned number of frames can be less only if a signal or underrun occurred.
 *
 * If the non-blocking behaviour is selected, then routine doesn't wait at all.
 *
 * The function is thread-safe when built with the proper option.
 */ 
snd_pcm_sframes_t snd_pcm_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	int err;

	assert(pcm);
	assert(size == 0 || bufs);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	if (pcm->access != SND_PCM_ACCESS_RW_NONINTERLEAVED) {
		SNDMSG("invalid access type %s", snd_pcm_access_name(pcm->access));
		return -EINVAL;
	}
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	return _snd_pcm_writen(pcm, bufs, size);
}

/**
 * \brief Read interleaved frames from a PCM
 * \param pcm PCM handle
 * \param buffer frames containing buffer
 * \param size frames to be read
 * \return a positive number of frames actually read otherwise a
 * negative error code
 * \retval -EBADFD PCM is not in the right state (#SND_PCM_STATE_PREPARED or #SND_PCM_STATE_RUNNING)
 * \retval -EPIPE an overrun occurred
 * \retval -ESTRPIPE a suspend event occurred (stream is suspended and waiting for an application recovery)
 *
 * If the blocking behaviour was selected and it is running, then routine waits until
 * all requested frames are filled. The returned number of frames can be less only
 * if a signal or underrun occurred.
 *
 * If the non-blocking behaviour is selected, then routine doesn't wait at all.
 *
 * The function is thread-safe when built with the proper option.
 */ 
snd_pcm_sframes_t snd_pcm_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	int err;

	assert(pcm);
	assert(size == 0 || buffer);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	if (pcm->access != SND_PCM_ACCESS_RW_INTERLEAVED) {
		SNDMSG("invalid access type %s", snd_pcm_access_name(pcm->access));
		return -EINVAL;
	}
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	return _snd_pcm_readi(pcm, buffer, size);
}

/**
 * \brief Read non interleaved frames to a PCM
 * \param pcm PCM handle
 * \param bufs frames containing buffers (one for each channel)
 * \param size frames to be read
 * \return a positive number of frames actually read otherwise a
 * negative error code
 * \retval -EBADFD PCM is not in the right state (#SND_PCM_STATE_PREPARED or #SND_PCM_STATE_RUNNING)
 * \retval -EPIPE an overrun occurred
 * \retval -ESTRPIPE a suspend event occurred (stream is suspended and waiting for an application recovery)
 *
 * If the blocking behaviour was selected and it is running, then routine waits until
 * all requested frames are filled. The returned number of frames can be less only
 * if a signal or underrun occurred.
 *
 * If the non-blocking behaviour is selected, then routine doesn't wait at all.
 *
 * The function is thread-safe when built with the proper option.
 */ 
snd_pcm_sframes_t snd_pcm_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	int err;

	assert(pcm);
	assert(size == 0 || bufs);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	if (pcm->access != SND_PCM_ACCESS_RW_NONINTERLEAVED) {
		SNDMSG("invalid access type %s", snd_pcm_access_name(pcm->access));
		return -EINVAL;
	}
	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	return _snd_pcm_readn(pcm, bufs, size);
}

/**
 * \brief Link two PCMs
 * \param pcm1 first PCM handle
 * \param pcm2 first PCM handle
 * \return 0 on success otherwise a negative error code
 *
 * The two PCMs will start/stop/prepare in sync.
 */ 
int snd_pcm_link(snd_pcm_t *pcm1, snd_pcm_t *pcm2)
{
	int err = 0;

	assert(pcm1);
	assert(pcm2);
	if (pcm1->fast_ops->link)
		err = pcm1->fast_ops->link(pcm1->fast_op_arg, pcm2);
	else
		err = -ENOSYS;
	return err;
}

/**
 * \brief Remove a PCM from a linked group
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_unlink(snd_pcm_t *pcm)
{
	int err = 0;

	assert(pcm);
	if (pcm->fast_ops->unlink)
		err = pcm->fast_ops->unlink(pcm->fast_op_arg);
	else
		err = -ENOSYS;
	return err;
}

/* locked version */
static int __snd_pcm_poll_descriptors_count(snd_pcm_t *pcm)
{
	if (pcm->fast_ops->poll_descriptors_count)
		return pcm->fast_ops->poll_descriptors_count(pcm->fast_op_arg);
	return pcm->poll_fd_count;
}

/**
 * \brief get count of poll descriptors for PCM handle
 * \param pcm PCM handle
 * \return count of poll descriptors
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_poll_descriptors_count(snd_pcm_t *pcm)
{
	int count;

	assert(pcm);
	snd_pcm_lock(pcm->fast_op_arg);
	count = __snd_pcm_poll_descriptors_count(pcm);
	snd_pcm_unlock(pcm->fast_op_arg);
	return count;
}

/* locked version */
static int __snd_pcm_poll_descriptors(snd_pcm_t *pcm, struct pollfd *pfds,
				      unsigned int space)
{
	if (pcm->fast_ops->poll_descriptors)
		return pcm->fast_ops->poll_descriptors(pcm->fast_op_arg, pfds, space);
	if (pcm->poll_fd < 0) {
		SNDMSG("poll_fd < 0");
		return -EIO;
	}
	if (space >= 1 && pfds) {
		pfds->fd = pcm->poll_fd;
		pfds->events = pcm->poll_events | POLLERR | POLLNVAL;
	} else {
		return 0;
	}
	return 1;
}

/**
 * \brief get poll descriptors
 * \param pcm PCM handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 *
 * This function fills the given poll descriptor structs for the specified
 * PCM handle.  The poll desctiptor array should have the size returned by
 * \link ::snd_pcm_poll_descriptors_count() \endlink function.
 *
 * The result is intended for direct use with the poll() syscall.
 *
 * For reading the returned events of poll descriptor after poll() system
 * call, use \link ::snd_pcm_poll_descriptors_revents() \endlink function.
 * The field values in pollfd structs may be bogus regarding the stream
 * direction from the application perspective (POLLIN might not imply read
 * direction and POLLOUT might not imply write), but
 * the \link ::snd_pcm_poll_descriptors_revents() \endlink function
 * does the right "demangling".
 *
 * You can use output from this function as arguments for the select()
 * syscall, too. Do not forget to translate POLLIN and POLLOUT events to
 * corresponding FD_SET arrays and demangle events using
 * \link ::snd_pcm_poll_descriptors_revents() \endlink .
 *
 * It is guaranteed that for the given PCM handle, the output poll
 * descriptor structs (and their count) will not change after
 * hardware and software parameters setup. Thus it is valid to call
 * the function once when all parameters are set and reuse its output
 * for the lifetime of the stream parameters.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_poll_descriptors(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int space)
{
	int err;

	assert(pcm && pfds);
	snd_pcm_lock(pcm->fast_op_arg);
	err = __snd_pcm_poll_descriptors(pcm, pfds, space);
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

static int __snd_pcm_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds,
				  unsigned int nfds, unsigned short *revents);

/**
 * \brief get returned events from poll descriptors
 * \param pcm PCM handle
 * \param pfds array of poll descriptors
 * \param nfds count of poll descriptors
 * \param revents pointer to the returned (single) event
 * \return zero if success, otherwise a negative error code
 *
 * This function does "demangling" of the revents mask returned from
 * the poll() syscall to correct semantics (POLLIN = read, POLLOUT = write).
 *
 * Note: The null event also exists. Even if poll() or select()
 * syscall returned that some events are waiting, this function might
 * return empty set of events. In this case, application should
 * do next event waiting using poll() or select().
 *
 * Note: Even if multiple poll descriptors are used (i.e. pfds > 1),
 * this function returns only a single event.
 *
 * The passed in count of poll descriptors must be equal to
 * \link ::snd_pcm_poll_descriptors_count() \endlink and the passed in array
 * must match the array returned by \link ::snd_pcm_poll_descriptors() \endlink
 * (in its full length and original order) with the revent fields updated
 * according to the poll() result. This function will not modify the file
 * descriptor or event field of any element of the given poll descriptor array.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_poll_descriptors_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	int err;

	assert(pcm && pfds && revents);
	snd_pcm_lock(pcm->fast_op_arg);
	err = __snd_pcm_poll_revents(pcm, pfds, nfds, revents);
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

static int __snd_pcm_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds,
				  unsigned int nfds, unsigned short *revents)
{
	if (pcm->fast_ops->poll_revents)
		return pcm->fast_ops->poll_revents(pcm->fast_op_arg, pfds, nfds, revents);
	if (nfds == 1) {
		*revents = pfds->revents;
		return 0;
	}
	return -EINVAL;
}

#ifndef DOC_HIDDEN
#define PCMTYPE(v) [SND_PCM_TYPE_##v] = #v
#define STATE(v) [SND_PCM_STATE_##v] = #v
#define STREAM(v) [SND_PCM_STREAM_##v] = #v
#define READY(v) [SND_PCM_READY_##v] = #v
#define XRUN(v) [SND_PCM_XRUN_##v] = #v
#define SILENCE(v) [SND_PCM_SILENCE_##v] = #v
#define TSTAMP(v) [SND_PCM_TSTAMP_##v] = #v
#define TSTAMP_TYPE(v) [SND_PCM_TSTAMP_TYPE_##v] = #v
#define ACCESS(v) [SND_PCM_ACCESS_##v] = #v
#define START(v) [SND_PCM_START_##v] = #v
#define HW_PARAM(v) [SND_PCM_HW_PARAM_##v] = #v
#define SW_PARAM(v) [SND_PCM_SW_PARAM_##v] = #v
#define FORMAT(v) [SND_PCM_FORMAT_##v] = #v
#define SUBFORMAT(v) [SND_PCM_SUBFORMAT_##v] = #v 

#define FORMATD(v, d) [SND_PCM_FORMAT_##v] = d
#define SUBFORMATD(v, d) [SND_PCM_SUBFORMAT_##v] = d 


static const char *const snd_pcm_stream_names[] = {
	STREAM(PLAYBACK),
	STREAM(CAPTURE),
};

static const char *const snd_pcm_state_names[] = {
	STATE(OPEN),
	STATE(SETUP),
	STATE(PREPARED),
	STATE(RUNNING),
	STATE(XRUN),
	STATE(DRAINING),
	STATE(PAUSED),
	STATE(SUSPENDED),
	STATE(DISCONNECTED),
};

static const char *const snd_pcm_access_names[] = {
	ACCESS(MMAP_INTERLEAVED), 
	ACCESS(MMAP_NONINTERLEAVED),
	ACCESS(MMAP_COMPLEX),
	ACCESS(RW_INTERLEAVED),
	ACCESS(RW_NONINTERLEAVED),
};

static const char *const snd_pcm_format_names[] = {
	FORMAT(S8),
	FORMAT(U8),
	FORMAT(S16_LE),
	FORMAT(S16_BE),
	FORMAT(U16_LE),
	FORMAT(U16_BE),
	FORMAT(S24_LE),
	FORMAT(S24_BE),
	FORMAT(U24_LE),
	FORMAT(U24_BE),
	FORMAT(S32_LE),
	FORMAT(S32_BE),
	FORMAT(U32_LE),
	FORMAT(U32_BE),
	FORMAT(FLOAT_LE),
	FORMAT(FLOAT_BE),
	FORMAT(FLOAT64_LE),
	FORMAT(FLOAT64_BE),
	FORMAT(IEC958_SUBFRAME_LE),
	FORMAT(IEC958_SUBFRAME_BE),
	FORMAT(MU_LAW),
	FORMAT(A_LAW),
	FORMAT(IMA_ADPCM),
	FORMAT(MPEG),
	FORMAT(GSM),
	FORMAT(S20_LE),
	FORMAT(S20_BE),
	FORMAT(U20_LE),
	FORMAT(U20_BE),
	FORMAT(SPECIAL),
	FORMAT(S24_3LE),
	FORMAT(S24_3BE),
	FORMAT(U24_3LE),
	FORMAT(U24_3BE),
	FORMAT(S20_3LE),
	FORMAT(S20_3BE),
	FORMAT(U20_3LE),
	FORMAT(U20_3BE),
	FORMAT(S18_3LE),
	FORMAT(S18_3BE),
	FORMAT(U18_3LE),
	FORMAT(U18_3BE),
	FORMAT(G723_24),
	FORMAT(G723_24_1B),
	FORMAT(G723_40),
	FORMAT(G723_40_1B),
	FORMAT(DSD_U8),
	FORMAT(DSD_U16_LE),
	FORMAT(DSD_U32_LE),
	FORMAT(DSD_U16_BE),
	FORMAT(DSD_U32_BE),
};

static const char *const snd_pcm_format_aliases[SND_PCM_FORMAT_LAST+1] = {
	FORMAT(S16),
	FORMAT(U16),
	FORMAT(S24),
	FORMAT(U24),
	FORMAT(S32),
	FORMAT(U32),
	FORMAT(FLOAT),
	FORMAT(FLOAT64),
	FORMAT(IEC958_SUBFRAME),
	FORMAT(S20),
	FORMAT(U20),
};

static const char *const snd_pcm_format_descriptions[] = {
	FORMATD(S8, "Signed 8 bit"), 
	FORMATD(U8, "Unsigned 8 bit"),
	FORMATD(S16_LE, "Signed 16 bit Little Endian"),
	FORMATD(S16_BE, "Signed 16 bit Big Endian"),
	FORMATD(U16_LE, "Unsigned 16 bit Little Endian"),
	FORMATD(U16_BE, "Unsigned 16 bit Big Endian"),
	FORMATD(S24_LE, "Signed 24 bit Little Endian"),
	FORMATD(S24_BE, "Signed 24 bit Big Endian"),
	FORMATD(U24_LE, "Unsigned 24 bit Little Endian"),
	FORMATD(U24_BE, "Unsigned 24 bit Big Endian"),
	FORMATD(S32_LE, "Signed 32 bit Little Endian"),
	FORMATD(S32_BE, "Signed 32 bit Big Endian"),
	FORMATD(U32_LE, "Unsigned 32 bit Little Endian"),
	FORMATD(U32_BE, "Unsigned 32 bit Big Endian"),
	FORMATD(FLOAT_LE, "Float 32 bit Little Endian"),
	FORMATD(FLOAT_BE, "Float 32 bit Big Endian"),
	FORMATD(FLOAT64_LE, "Float 64 bit Little Endian"),
	FORMATD(FLOAT64_BE, "Float 64 bit Big Endian"),
	FORMATD(IEC958_SUBFRAME_LE, "IEC-958 Little Endian"),
	FORMATD(IEC958_SUBFRAME_BE, "IEC-958 Big Endian"),
	FORMATD(MU_LAW, "Mu-Law"),
	FORMATD(A_LAW, "A-Law"),
	FORMATD(IMA_ADPCM, "Ima-ADPCM"),
	FORMATD(MPEG, "MPEG"),
	FORMATD(GSM, "GSM"),
	FORMATD(S20_LE, "Signed 20 bit Little Endian in 4 bytes, LSB justified"),
	FORMATD(S20_BE, "Signed 20 bit Big Endian in 4 bytes, LSB justified"),
	FORMATD(U20_LE, "Unsigned 20 bit Little Endian in 4 bytes, LSB justified"),
	FORMATD(U20_BE, "Unsigned 20 bit Big Endian in 4 bytes, LSB justified"),
	FORMATD(SPECIAL, "Special"),
	FORMATD(S24_3LE, "Signed 24 bit Little Endian in 3bytes"),
	FORMATD(S24_3BE, "Signed 24 bit Big Endian in 3bytes"),
	FORMATD(U24_3LE, "Unsigned 24 bit Little Endian in 3bytes"),
	FORMATD(U24_3BE, "Unsigned 24 bit Big Endian in 3bytes"),
	FORMATD(S20_3LE, "Signed 20 bit Little Endian in 3bytes"),
	FORMATD(S20_3BE, "Signed 20 bit Big Endian in 3bytes"),
	FORMATD(U20_3LE, "Unsigned 20 bit Little Endian in 3bytes"),
	FORMATD(U20_3BE, "Unsigned 20 bit Big Endian in 3bytes"),
	FORMATD(S18_3LE, "Signed 18 bit Little Endian in 3bytes"),
	FORMATD(S18_3BE, "Signed 18 bit Big Endian in 3bytes"),
	FORMATD(U18_3LE, "Unsigned 18 bit Little Endian in 3bytes"),
	FORMATD(U18_3BE, "Unsigned 18 bit Big Endian in 3bytes"),
	FORMATD(G723_24, "G.723 (ADPCM) 24 kbit/s, 8 samples in 3 bytes"),
	FORMATD(G723_24_1B, "G.723 (ADPCM) 24 kbit/s, 1 sample in 1 byte"),
	FORMATD(G723_40, "G.723 (ADPCM) 40 kbit/s, 8 samples in 3 bytes"),
	FORMATD(G723_40_1B, "G.723 (ADPCM) 40 kbit/s, 1 sample in 1 byte"),
	FORMATD(DSD_U8,  "Direct Stream Digital, 1-byte (x8), oldest bit in MSB"),
	FORMATD(DSD_U16_LE, "Direct Stream Digital, 2-byte (x16), little endian, oldest bits in MSB"),
	FORMATD(DSD_U32_LE, "Direct Stream Digital, 4-byte (x32), little endian, oldest bits in MSB"),
	FORMATD(DSD_U16_BE, "Direct Stream Digital, 2-byte (x16), big endian, oldest bits in MSB"),
	FORMATD(DSD_U32_BE, "Direct Stream Digital, 4-byte (x32), big endian, oldest bits in MSB"),
};

static const char *const snd_pcm_type_names[] = {
	PCMTYPE(HW), 
	PCMTYPE(HOOKS), 
	PCMTYPE(MULTI), 
	PCMTYPE(FILE), 
	PCMTYPE(NULL), 
	PCMTYPE(SHM), 
	PCMTYPE(INET), 
	PCMTYPE(COPY), 
	PCMTYPE(LINEAR), 
	PCMTYPE(ALAW), 
	PCMTYPE(MULAW), 
	PCMTYPE(ADPCM), 
	PCMTYPE(RATE), 
	PCMTYPE(ROUTE), 
	PCMTYPE(PLUG), 
	PCMTYPE(SHARE), 
	PCMTYPE(METER), 
	PCMTYPE(MIX), 
	PCMTYPE(DROUTE), 
	PCMTYPE(LBSERVER), 
	PCMTYPE(LINEAR_FLOAT), 
	PCMTYPE(LADSPA), 
	PCMTYPE(DMIX), 
	PCMTYPE(JACK),
	PCMTYPE(DSNOOP),
	PCMTYPE(IEC958),
	PCMTYPE(SOFTVOL),
	PCMTYPE(IOPLUG),
	PCMTYPE(EXTPLUG),
	PCMTYPE(MMAP_EMUL),
};

static const char *const snd_pcm_subformat_names[] = {
	SUBFORMAT(STD), 
	SUBFORMAT(MSBITS_MAX),
	SUBFORMAT(MSBITS_20),
	SUBFORMAT(MSBITS_24),
};

static const char *const snd_pcm_subformat_descriptions[] = {
	SUBFORMATD(STD, "Standard"), 
	SUBFORMATD(MSBITS_MAX, "Maximum based on PCM format"),
	SUBFORMATD(MSBITS_20, "20 most significant bits"),
	SUBFORMATD(MSBITS_24, "24 most significant bits"),
};

static const char *const snd_pcm_start_mode_names[] = {
	START(EXPLICIT),
	START(DATA),
};

static const char *const snd_pcm_xrun_mode_names[] = {
	XRUN(NONE),
	XRUN(STOP),
};

static const char *const snd_pcm_tstamp_mode_names[] = {
	TSTAMP(NONE),
	TSTAMP(ENABLE),
};

static const char *const snd_pcm_tstamp_type_names[] = {
	TSTAMP_TYPE(GETTIMEOFDAY),
	TSTAMP_TYPE(MONOTONIC),
	TSTAMP_TYPE(MONOTONIC_RAW),
};
#endif

/**
 * \brief get name of PCM stream type
 * \param stream PCM stream type
 * \return ascii name of PCM stream type
 */
const char *snd_pcm_stream_name(const snd_pcm_stream_t stream)
{
	if (stream > SND_PCM_STREAM_LAST)
		return NULL;
	return snd_pcm_stream_names[stream];
}

/**
 * \brief get name of PCM access type
 * \param acc PCM access type
 * \return ascii name of PCM access type
 */
const char *snd_pcm_access_name(const snd_pcm_access_t acc)
{
	if (acc > SND_PCM_ACCESS_LAST)
		return NULL;
	return snd_pcm_access_names[acc];
}

/**
 * \brief get name of PCM sample format
 * \param format PCM sample format
 * \return ascii name of PCM sample format
 */
const char *snd_pcm_format_name(const snd_pcm_format_t format)
{
	if (format > SND_PCM_FORMAT_LAST)
		return NULL;
	return snd_pcm_format_names[format];
}

/**
 * \brief get description of PCM sample format
 * \param format PCM sample format
 * \return ascii description of PCM sample format
 */
const char *snd_pcm_format_description(const snd_pcm_format_t format)
{
	if (format > SND_PCM_FORMAT_LAST)
		return NULL;
	return snd_pcm_format_descriptions[format];
}

/**
 * \brief get PCM sample format from name
 * \param name PCM sample format name (case insensitive)
 * \return PCM sample format
 */
snd_pcm_format_t snd_pcm_format_value(const char* name)
{
	snd_pcm_format_t format;
	for (format = 0; format <= SND_PCM_FORMAT_LAST; format++) {
		if (snd_pcm_format_names[format] &&
		    strcasecmp(name, snd_pcm_format_names[format]) == 0) {
			return format;
		}
		if (snd_pcm_format_aliases[format] &&
		    strcasecmp(name, snd_pcm_format_aliases[format]) == 0) {
			return format;
		}
	}
	for (format = 0; format <= SND_PCM_FORMAT_LAST; format++) {
		if (snd_pcm_format_descriptions[format] &&
		    strcasecmp(name, snd_pcm_format_descriptions[format]) == 0) {
			return format;
		}
	}
	return SND_PCM_FORMAT_UNKNOWN;
}

/**
 * \brief get name of PCM sample subformat
 * \param subformat PCM sample subformat
 * \return ascii name of PCM sample subformat
 */
const char *snd_pcm_subformat_name(const snd_pcm_subformat_t subformat)
{
	if (subformat > SND_PCM_SUBFORMAT_LAST)
		return NULL;
	return snd_pcm_subformat_names[subformat];
}

/**
 * \brief get description of PCM sample subformat
 * \param subformat PCM sample subformat
 * \return ascii description of PCM sample subformat
 */
const char *snd_pcm_subformat_description(const snd_pcm_subformat_t subformat)
{
	if (subformat > SND_PCM_SUBFORMAT_LAST)
		return NULL;
	return snd_pcm_subformat_descriptions[subformat];
}

/**
 * \brief get PCM sample subformat from name
 * \param name PCM sample subformat name (case insensitive)
 * \return PCM sample subformat
 */
snd_pcm_subformat_t snd_pcm_subformat_value(const char* name)
{
	snd_pcm_subformat_t subformat;

	for (subformat = 0; subformat <= SND_PCM_SUBFORMAT_LAST; subformat++) {
		if (snd_pcm_subformat_names[subformat] &&
		    !strcasecmp(name, snd_pcm_subformat_names[subformat]))
			return subformat;
	}

	for (subformat = 0; subformat <= SND_PCM_SUBFORMAT_LAST; subformat++) {
		if (snd_pcm_subformat_descriptions[subformat] &&
		    !strcasecmp(name, snd_pcm_subformat_descriptions[subformat]))
			return subformat;
	}

	return SND_PCM_SUBFORMAT_UNKNOWN;
}

/**
 * \brief (DEPRECATED) get name of PCM start mode setting
 * \param mode PCM start mode
 * \return ascii name of PCM start mode setting
 */
const char *snd_pcm_start_mode_name(snd_pcm_start_t mode)
{
	if (mode > SND_PCM_START_LAST)
		return NULL;
	return snd_pcm_start_mode_names[mode];
}

#ifndef DOC_HIDDEN
link_warning(snd_pcm_start_mode_name, "Warning: start_mode is deprecated, consider to use start_threshold");
#endif

/**
 * \brief (DEPRECATED) get name of PCM xrun mode setting
 * \param mode PCM xrun mode
 * \return ascii name of PCM xrun mode setting
 */
const char *snd_pcm_xrun_mode_name(snd_pcm_xrun_t mode)
{
	if (mode > SND_PCM_XRUN_LAST)
		return NULL;
	return snd_pcm_xrun_mode_names[mode];
}

#ifndef DOC_HIDDEN
link_warning(snd_pcm_xrun_mode_name, "Warning: xrun_mode is deprecated, consider to use stop_threshold");
#endif

/**
 * \brief get name of PCM tstamp mode setting
 * \param mode PCM tstamp mode
 * \return ascii name of PCM tstamp mode setting
 */
const char *snd_pcm_tstamp_mode_name(const snd_pcm_tstamp_t mode)
{
	if (mode > SND_PCM_TSTAMP_LAST)
		return NULL;
	return snd_pcm_tstamp_mode_names[mode];
}

/**
 * \brief get name of PCM tstamp type setting
 * \param type PCM tstamp type
 * \return ascii name of PCM tstamp type setting
 */
const char *snd_pcm_tstamp_type_name(snd_pcm_tstamp_type_t type)
{
	if (type > SND_PCM_TSTAMP_TYPE_LAST)
		return NULL;
	return snd_pcm_tstamp_type_names[type];
}

/**
 * \brief get name of PCM state
 * \param state PCM state
 * \return ascii name of PCM state
 */
const char *snd_pcm_state_name(const snd_pcm_state_t state)
{
	if (state > SND_PCM_STATE_LAST)
		return NULL;
	return snd_pcm_state_names[state];
}

/**
 * \brief get name of PCM type
 * \param type PCM type
 * \return ascii name of PCM type
 */
#ifndef DOXYGEN
EXPORT_SYMBOL const char *INTERNAL(snd_pcm_type_name)(snd_pcm_type_t type)
#else
const char *snd_pcm_type_name(snd_pcm_type_t type)
#endif
{
	if (type > SND_PCM_TYPE_LAST)
		return NULL;
	return snd_pcm_type_names[type];
}
use_default_symbol_version(__snd_pcm_type_name, snd_pcm_type_name, ALSA_0.9.0);

/**
 * \brief Dump current hardware setup for PCM
 * \param pcm PCM handle
 * \param out Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_dump_hw_setup(snd_pcm_t *pcm, snd_output_t *out)
{
	assert(pcm);
	assert(out);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
        snd_output_printf(out, "  stream       : %s\n", snd_pcm_stream_name(pcm->stream));
	snd_output_printf(out, "  access       : %s\n", snd_pcm_access_name(pcm->access));
	snd_output_printf(out, "  format       : %s\n", snd_pcm_format_name(pcm->format));
	snd_output_printf(out, "  subformat    : %s\n", snd_pcm_subformat_name(pcm->subformat));
	snd_output_printf(out, "  channels     : %u\n", pcm->channels);
	snd_output_printf(out, "  rate         : %u\n", pcm->rate);
	snd_output_printf(out, "  exact rate   : %g (%u/%u)\n",
			  (pcm->rate_den ? ((double) pcm->rate_num / pcm->rate_den) : 0.0),
			  pcm->rate_num, pcm->rate_den);
	snd_output_printf(out, "  msbits       : %u\n", pcm->msbits);
	snd_output_printf(out, "  buffer_size  : %lu\n", pcm->buffer_size);
	snd_output_printf(out, "  period_size  : %lu\n", pcm->period_size);
	snd_output_printf(out, "  period_time  : %u\n", pcm->period_time);
	return 0;
}

/**
 * \brief Dump current software setup for PCM
 * \param pcm PCM handle
 * \param out Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_dump_sw_setup(snd_pcm_t *pcm, snd_output_t *out)
{
	assert(pcm);
	assert(out);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	snd_output_printf(out, "  tstamp_mode  : %s\n", snd_pcm_tstamp_mode_name(pcm->tstamp_mode));
	snd_output_printf(out, "  tstamp_type  : %s\n", snd_pcm_tstamp_type_name(pcm->tstamp_type));
	snd_output_printf(out, "  period_step  : %d\n", pcm->period_step);
	snd_output_printf(out, "  avail_min    : %ld\n", pcm->avail_min);
	snd_output_printf(out, "  period_event : %i\n", pcm->period_event);
	snd_output_printf(out, "  start_threshold  : %ld\n", pcm->start_threshold);
	snd_output_printf(out, "  stop_threshold   : %ld\n", pcm->stop_threshold);
	snd_output_printf(out, "  silence_threshold: %ld\n", pcm->silence_threshold);
	snd_output_printf(out, "  silence_size : %ld\n", pcm->silence_size);
	snd_output_printf(out, "  boundary     : %ld\n", pcm->boundary);
	return 0;
}

/**
 * \brief Dump current setup (hardware and software) for PCM
 * \param pcm PCM handle
 * \param out Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_dump_setup(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_dump_hw_setup(pcm, out);
	snd_pcm_dump_sw_setup(pcm, out);
	return 0;
}

/**
 * \brief Dump status
 * \param status Status container
 * \param out Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_status_dump(snd_pcm_status_t *status, snd_output_t *out)
{
	assert(status);
	snd_output_printf(out, "  state       : %s\n", snd_pcm_state_name((snd_pcm_state_t) status->state));
	snd_output_printf(out, "  trigger_time: %ld.%06ld\n",
			  status->trigger_tstamp.tv_sec,
			  status->trigger_tstamp.tv_nsec / 1000);
	snd_output_printf(out, "  tstamp      : %ld.%06ld\n",
		status->tstamp.tv_sec, status->tstamp.tv_nsec / 1000);
	snd_output_printf(out, "  delay       : %ld\n", (long)status->delay);
	snd_output_printf(out, "  avail       : %ld\n", (long)status->avail);
	snd_output_printf(out, "  avail_max   : %ld\n", (long)status->avail_max);
	return 0;
}

/**
 * \brief Dump PCM info
 * \param pcm PCM handle
 * \param out Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	int err = 0;

	assert(pcm);
	assert(out);
	if (pcm->ops->dump)
		pcm->ops->dump(pcm->op_arg, out);
	else
		err = -ENOSYS;
	return err;
}

/**
 * \brief Convert bytes in frames for a PCM
 * \param pcm PCM handle
 * \param bytes quantity in bytes
 * \return quantity expressed in frames
 */
snd_pcm_sframes_t snd_pcm_bytes_to_frames(snd_pcm_t *pcm, ssize_t bytes)
{
	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	return bytes * 8 / pcm->frame_bits;
}

/**
 * \brief Convert frames in bytes for a PCM
 * \param pcm PCM handle
 * \param frames quantity in frames
 * \return quantity expressed in bytes
 */
ssize_t snd_pcm_frames_to_bytes(snd_pcm_t *pcm, snd_pcm_sframes_t frames)
{
	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	return frames * pcm->frame_bits / 8;
}

/**
 * \brief Convert bytes in samples for a PCM
 * \param pcm PCM handle
 * \param bytes quantity in bytes
 * \return quantity expressed in samples
 */
long snd_pcm_bytes_to_samples(snd_pcm_t *pcm, ssize_t bytes)
{
	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	return bytes * 8 / pcm->sample_bits;
}

/**
 * \brief Convert samples in bytes for a PCM
 * \param pcm PCM handle
 * \param samples quantity in samples
 * \return quantity expressed in bytes
 */
ssize_t snd_pcm_samples_to_bytes(snd_pcm_t *pcm, long samples)
{
	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	return samples * pcm->sample_bits / 8;
}

/**
 * \brief Add an async handler for a PCM
 * \param handler Returned handler handle
 * \param pcm PCM handle
 * \param callback Callback function
 * \param private_data Callback private data
 * \return 0 otherwise a negative error code on failure
 *
 * The asynchronous callback is called when period boundary elapses.
 */
int snd_async_add_pcm_handler(snd_async_handler_t **handler, snd_pcm_t *pcm, 
			      snd_async_callback_t callback, void *private_data)
{
	int err;
	int was_empty;
	snd_async_handler_t *h;
	err = snd_async_add_handler(&h, _snd_pcm_async_descriptor(pcm),
				    callback, private_data);
	if (err < 0)
		return err;
	h->type = SND_ASYNC_HANDLER_PCM;
	h->u.pcm = pcm;
	was_empty = list_empty(&pcm->async_handlers);
	list_add_tail(&h->hlist, &pcm->async_handlers);
	if (was_empty) {
		err = snd_pcm_async(pcm, snd_async_handler_get_signo(h), getpid());
		if (err < 0) {
			snd_async_del_handler(h);
			return err;
		}
	}
	*handler = h;
	return 0;
}

/**
 * \brief Return PCM handle related to an async handler
 * \param handler Async handler handle
 * \return PCM handle
 */
snd_pcm_t *snd_async_handler_get_pcm(snd_async_handler_t *handler)
{
	if (handler->type != SND_ASYNC_HANDLER_PCM) {
		SNDMSG("invalid handler type %d", handler->type);
		return NULL;
	}
	return handler->u.pcm;
}

static const char *const build_in_pcms[] = {
	"adpcm", "alaw", "copy", "dmix", "file", "hooks", "hw", "ladspa", "lfloat",
	"linear", "meter", "mulaw", "multi", "null", "empty", "plug", "rate", "route", "share",
	"shm", "dsnoop", "dshare", "asym", "iec958", "softvol", "mmap_emul",
	NULL
};

static int snd_pcm_open_conf(snd_pcm_t **pcmp, const char *name,
			     snd_config_t *pcm_root, snd_config_t *pcm_conf,
			     snd_pcm_stream_t stream, int mode)
{
	const char *str;
	char *buf = NULL, *buf1 = NULL;
	int err;
	snd_config_t *conf, *type_conf = NULL, *tmp;
	snd_config_iterator_t i, next;
	const char *id;
	const char *lib = NULL, *open_name = NULL;
	int (*open_func)(snd_pcm_t **, const char *, 
			 snd_config_t *, snd_config_t *, 
			 snd_pcm_stream_t, int) = NULL;
#ifndef PIC
	extern void *snd_pcm_open_symbols(void);
#endif
	if (snd_config_get_type(pcm_conf) != SND_CONFIG_TYPE_COMPOUND) {
		char *val;
		id = NULL;
		snd_config_get_id(pcm_conf, &id);
		val = NULL;
		snd_config_get_ascii(pcm_conf, &val);
		SNDERR("Invalid type for PCM %s%sdefinition (id: %s, value: %s)", name ? name : "", name ? " " : "", id, val);
		free(val);
		return -EINVAL;
	}
	err = snd_config_search(pcm_conf, "type", &conf);
	if (err < 0) {
		SNDERR("type is not defined");
		return err;
	}
	err = snd_config_get_id(conf, &id);
	if (err < 0) {
		SNDERR("unable to get id");
		return err;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", id);
		return err;
	}
	err = snd_config_search_definition(pcm_root, "pcm_type", str, &type_conf);
	if (err >= 0) {
		if (snd_config_get_type(type_conf) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for PCM type %s definition", str);
			err = -EINVAL;
			goto _err;
		}
		snd_config_for_each(i, next, type_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			if (snd_config_get_id(n, &id) < 0)
				continue;
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "lib") == 0) {
				err = snd_config_get_string(n, &lib);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			if (strcmp(id, "open") == 0) {
				err = snd_config_get_string(n, &open_name);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
			err = -EINVAL;
			goto _err;
		}
	}
	if (!open_name) {
		buf = malloc(strlen(str) + 32);
		if (buf == NULL) {
			err = -ENOMEM;
			goto _err;
		}
		open_name = buf;
		sprintf(buf, "_snd_pcm_%s_open", str);
	}
	if (!lib) {
		const char *const *build_in = build_in_pcms;
		while (*build_in) {
			if (!strcmp(*build_in, str))
				break;
			build_in++;
		}
		if (*build_in == NULL) {
			buf1 = malloc(strlen(str) + 32);
			if (buf1 == NULL) {
				err = -ENOMEM;
				goto _err;
			}
			lib = buf1;
			sprintf(buf1, "libasound_module_pcm_%s.so", str);
		}
	}
#ifndef PIC
	snd_pcm_open_symbols();	/* this call is for static linking only */
#endif
	open_func = snd_dlobj_cache_get(lib, open_name,
			SND_DLSYM_VERSION(SND_PCM_DLSYM_VERSION), 1);
	if (open_func) {
		err = open_func(pcmp, name, pcm_root, pcm_conf, stream, mode);
		if (err >= 0) {
			if ((*pcmp)->open_func) {
				/* only init plugin (like empty, asym) */
				snd_dlobj_cache_put(open_func);
			} else {
				(*pcmp)->open_func = open_func;
			}
			err = 0;
		} else {
			snd_dlobj_cache_put(open_func);
		}
	} else {
		err = -ENXIO;
	}
	if (err >= 0) {
		err = snd_config_search(pcm_root, "defaults.pcm.compat", &tmp);
		if (err >= 0) {
			long i;
			if (snd_config_get_integer(tmp, &i) >= 0) {
				if (i > 0)
					(*pcmp)->compat = 1;
			}
		} else {
			char *str = getenv("LIBASOUND_COMPAT");
			if (str && *str)
				(*pcmp)->compat = 1;
		}
		err = snd_config_search(pcm_root, "defaults.pcm.minperiodtime", &tmp);
		if (err >= 0)
			snd_config_get_integer(tmp, &(*pcmp)->minperiodtime);
		err = 0;
	}
       _err:
	if (type_conf)
		snd_config_delete(type_conf);
	free(buf);
	free(buf1);
	return err;
}

static int snd_pcm_open_noupdate(snd_pcm_t **pcmp, snd_config_t *root,
				 const char *name, snd_pcm_stream_t stream,
				 int mode, int hop)
{
	int err;
	snd_config_t *pcm_conf;
	const char *str;

	err = snd_config_search_definition(root, "pcm", name, &pcm_conf);
	if (err < 0) {
		SNDERR("Unknown PCM %s", name);
		return err;
	}
	if (snd_config_get_string(pcm_conf, &str) >= 0)
		err = snd_pcm_open_noupdate(pcmp, root, str, stream, mode,
					    hop + 1);
	else {
		snd_config_set_hop(pcm_conf, hop);
		err = snd_pcm_open_conf(pcmp, name, root, pcm_conf, stream, mode);
	}
	snd_config_delete(pcm_conf);
	return err;
}

/**
 * \brief Opens a PCM
 * \param pcmp Returned PCM handle
 * \param name ASCII identifier of the PCM handle
 * \param stream Wanted stream
 * \param mode Open mode (see #SND_PCM_NONBLOCK, #SND_PCM_ASYNC)
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_open(snd_pcm_t **pcmp, const char *name, 
		 snd_pcm_stream_t stream, int mode)
{
	snd_config_t *top;
	int err;

	assert(pcmp && name);
	if (_snd_is_ucm_device(name)) {
		name = uc_mgr_alibcfg_by_device(&top, name);
		if (name == NULL)
			return -ENODEV;
	} else {
		err = snd_config_update_ref(&top);
		if (err < 0)
			return err;
	}
	err = snd_pcm_open_noupdate(pcmp, top, name, stream, mode, 0);
	snd_config_unref(top);
	return err;
}

/**
 * \brief Opens a PCM using local configuration
 * \param pcmp Returned PCM handle
 * \param name ASCII identifier of the PCM handle
 * \param stream Wanted stream
 * \param mode Open mode (see #SND_PCM_NONBLOCK, #SND_PCM_ASYNC)
 * \param lconf Local configuration
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_open_lconf(snd_pcm_t **pcmp, const char *name, 
		       snd_pcm_stream_t stream, int mode,
		       snd_config_t *lconf)
{
	assert(pcmp && name && lconf);
	return snd_pcm_open_noupdate(pcmp, lconf, name, stream, mode, 0);
}

/**
 * \brief Opens a fallback PCM
 * \param pcmp Returned PCM handle
 * \param root Configuration root
 * \param name ASCII identifier of the PCM handle
 * \param orig_name The original ASCII name
 * \param stream Wanted stream
 * \param mode Open mode (see #SND_PCM_NONBLOCK, #SND_PCM_ASYNC)
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_open_fallback(snd_pcm_t **pcmp, snd_config_t *root,
			  const char *name, const char *orig_name,
			  snd_pcm_stream_t stream, int mode)
{
	int err;
	assert(pcmp && name && root);
	err = snd_pcm_open_noupdate(pcmp, root, name, stream, mode, 0);
	if (err >= 0) {
		free((*pcmp)->name);
		(*pcmp)->name = orig_name ? strdup(orig_name) : NULL;
	}
	return err;
}

#ifndef DOC_HIDDEN
int snd_pcm_new(snd_pcm_t **pcmp, snd_pcm_type_t type, const char *name,
		snd_pcm_stream_t stream, int mode)
{
	snd_pcm_t *pcm;
#ifdef THREAD_SAFE_API
	pthread_mutexattr_t attr;
#endif

	pcm = calloc(1, sizeof(*pcm));
	if (!pcm)
		return -ENOMEM;
	pcm->type = type;
	if (name)
		pcm->name = strdup(name);
	pcm->stream = stream;
	pcm->mode = mode;
	pcm->poll_fd_count = 1;
	pcm->poll_fd = -1;
	pcm->op_arg = pcm;
	pcm->fast_op_arg = pcm;
	INIT_LIST_HEAD(&pcm->async_handlers);
#ifdef THREAD_SAFE_API
	pthread_mutexattr_init(&attr);
#ifdef HAVE_PTHREAD_MUTEX_RECURSIVE
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#endif
	pthread_mutex_init(&pcm->lock, &attr);
	pthread_mutexattr_destroy(&attr);
	/* use locking as default;
	 * each plugin may suppress this in its open call
	 */
	pcm->need_lock = 1;
	if (mode & SND_PCM_ASYNC) {
		/* async handler may lead to a deadlock; suppose no MT */
		pcm->lock_enabled = 0;
	} else {
		/* set lock_enabled field depending on $LIBASOUND_THREAD_SAFE */
		static int do_lock_enable = -1; /* uninitialized */

		/* evaluate env var only once at the first open for consistency */
		if (do_lock_enable == -1) {
			char *p = getenv("LIBASOUND_THREAD_SAFE");
			do_lock_enable = !p || *p != '0';
		}
		pcm->lock_enabled = do_lock_enable;
	}
#endif
	*pcmp = pcm;
	return 0;
}

int snd_pcm_free(snd_pcm_t *pcm)
{
	assert(pcm);
	free(pcm->name);
	free(pcm->hw.link_dst);
	free(pcm->appl.link_dst);
	snd_dlobj_cache_put(pcm->open_func);
#ifdef THREAD_SAFE_API
	pthread_mutex_destroy(&pcm->lock);
#endif
	free(pcm);
	return 0;
}

int snd_pcm_open_named_slave(snd_pcm_t **pcmp, const char *name,
			     snd_config_t *root,
			     snd_config_t *conf, snd_pcm_stream_t stream,
			     int mode, snd_config_t *parent_conf)
{
	const char *str;
	int hop;

	if ((hop = snd_config_check_hop(parent_conf)) < 0)
		return hop;
	if (snd_config_get_string(conf, &str) >= 0)
		return snd_pcm_open_noupdate(pcmp, root, str, stream, mode,
					     hop + 1);
	return snd_pcm_open_conf(pcmp, name, root, conf, stream, mode);
}
#endif

/**
 * \brief Wait for a PCM to become ready
 * \param pcm PCM handle
 * \param timeout maximum time in milliseconds to wait,
 *        a -1 value means infinity (SND_PCM_WAIT_INFINITE),
 *	       see also SND_PCM_WAIT_IO and SND_PCM_WAIT_DRAIN
 * \return a positive value on success otherwise a negative error code
 *         (-EPIPE for the xrun and -ESTRPIPE for the suspended status,
 *          others for general errors) 
 * \retval 0 timeout occurred
 * \retval 1 PCM stream is ready for I/O
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_wait(snd_pcm_t *pcm, int timeout)
{
	int err;

	__snd_pcm_lock(pcm->fast_op_arg); /* forced lock */
	err = __snd_pcm_wait_in_lock(pcm, timeout);
	__snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

#ifndef DOC_HIDDEN
/* locked version */
int __snd_pcm_wait_in_lock(snd_pcm_t *pcm, int timeout)
{
	int err;

	/* NOTE: avail_min check can be skipped during draining */
	if (__snd_pcm_state(pcm) != SND_PCM_STATE_DRAINING &&
	    !snd_pcm_may_wait_for_avail_min(pcm, snd_pcm_mmap_avail(pcm))) {
		/* check more precisely */
		err = pcm_state_to_error(__snd_pcm_state(pcm));
		return err < 0 ? err : 1;
	}
	return snd_pcm_wait_nocheck(pcm, timeout);
}

static int __snd_pcm_wait_io_timeout(snd_pcm_t *pcm)
{
	int timeout;

	/* period size is the time boundary */
	timeout = (pcm->period_size * 1000ULL) / pcm->rate;
	/* should not happen */
	if (timeout < 0)
		timeout = 0;
	/* add extra time of 200 milliseconds */
	timeout += 200;
	return timeout;
}

static int __snd_pcm_wait_drain_timeout(snd_pcm_t *pcm)
{
	int timeout;

	/* for capture, there's no reason to wait, just one iteration */
	if (snd_pcm_stream(pcm) == SND_PCM_STREAM_CAPTURE)
		return 0;
	/* result is in milliseconds */
	timeout = (snd_pcm_mmap_playback_delay(pcm) * 1000LL) / pcm->rate;
	/* should not happen */
	if (timeout < 0)
		timeout = 0;
	/* add extra time of 200 milliseconds */
	timeout += 200;
	return timeout;
}

/* 
 * like snd_pcm_wait() but doesn't check mmap_avail before calling poll()
 *
 * used in drain code in some plugins
 *
 * This function is called inside pcm lock.
 */
int snd_pcm_wait_nocheck(snd_pcm_t *pcm, int timeout)
{
	struct pollfd *pfd;
	unsigned short revents = 0;
	int npfds, err, err_poll;
	
	npfds = __snd_pcm_poll_descriptors_count(pcm);
	if (npfds <= 0 || npfds >= 16) {
		SNDERR("Invalid poll_fds %d", npfds);
		return -EIO;
	}
	pfd = alloca(sizeof(*pfd) * npfds);
	err = __snd_pcm_poll_descriptors(pcm, pfd, npfds);
	if (err < 0)
		return err;
	if (err != npfds) {
		SNDMSG("invalid poll descriptors %d", err);
		return -EIO;
	}
	if (timeout == SND_PCM_WAIT_IO)
		timeout = __snd_pcm_wait_io_timeout(pcm);
	else if (timeout == SND_PCM_WAIT_DRAIN)
		timeout = __snd_pcm_wait_drain_timeout(pcm);
	else if (timeout < -1)
		SNDMSG("invalid snd_pcm_wait timeout argument %d", timeout);
	do {
		__snd_pcm_unlock(pcm->fast_op_arg);
		err_poll = poll(pfd, npfds, timeout);
		__snd_pcm_lock(pcm->fast_op_arg);
		if (err_poll < 0) {
			if (errno == EINTR && !PCMINABORT(pcm) && !(pcm->mode & SND_PCM_EINTR))
		                continue;
			return -errno;
                }
		if (! err_poll)
			break;
		err = __snd_pcm_poll_revents(pcm, pfd, npfds, &revents);
		if (err < 0)
			return err;
		if (revents & (POLLERR | POLLNVAL)) {
			/* check more precisely */
			err = pcm_state_to_error(__snd_pcm_state(pcm));
			return err < 0 ? err : -EIO;
		}
	} while (!(revents & (POLLIN | POLLOUT)));
#if 0 /* very useful code to test poll related problems */
	{
		snd_pcm_sframes_t avail_update;
		__snd_pcm_hwsync(pcm);
		avail_update = __snd_pcm_avail_update(pcm);
		if (avail_update < (snd_pcm_sframes_t)pcm->avail_min) {
			printf("*** snd_pcm_wait() FATAL ERROR!!!\n");
			printf("avail_min = %li, avail_update = %li\n", pcm->avail_min, avail_update);
		}
	}
#endif
	return err_poll > 0 ? 1 : 0;
}
#endif

/**
 * \brief Return number of frames ready to be read (capture) / written (playback)
 * \param pcm PCM handle
 * \return a positive number of frames ready otherwise a negative
 * error code
 *
 * On capture does all the actions needed to transport to application
 * level all the ready frames across underlying layers.
 *
 * The position is not synced with hardware (driver) position in the sound
 * ring buffer in this function. This function is a light version of
 * #snd_pcm_avail() .
 *
 * Using this function is ideal after poll() or select() when audio
 * file descriptor made the event and when application expects just period
 * timing.
 *
 * Also this function might be called after #snd_pcm_delay() or
 * #snd_pcm_hwsync() functions to move private ring buffer pointers
 * in alsa-lib (the internal plugin chain).
 *
 * The function is thread-safe when built with the proper option.
 */
snd_pcm_sframes_t snd_pcm_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_sframes_t result;

	snd_pcm_lock(pcm->fast_op_arg);
	result = __snd_pcm_avail_update(pcm);
	snd_pcm_unlock(pcm->fast_op_arg);
	return result;
}

/**
 * \brief Return number of frames ready to be read (capture) / written (playback)
 * \param pcm PCM handle
 * \return a positive number of frames ready otherwise a negative
 * error code
 *
 * On capture does all the actions needed to transport to application
 * level all the ready frames across underlying layers.
 *
 * The position is synced with hardware (driver) position in the sound
 * ring buffer in this functions.
 *
 * The function is thread-safe when built with the proper option.
 */
snd_pcm_sframes_t snd_pcm_avail(snd_pcm_t *pcm)
{
	int err;
	snd_pcm_sframes_t result;

	assert(pcm);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	snd_pcm_lock(pcm->fast_op_arg);
	err = __snd_pcm_hwsync(pcm);
	if (err < 0)
		result = err;
	else
		result = __snd_pcm_avail_update(pcm);
	snd_pcm_unlock(pcm->fast_op_arg);
	return result;
}

/**
 * \brief Combine snd_pcm_avail and snd_pcm_delay functions
 * \param pcm PCM handle
 * \param availp Number of available frames in the ring buffer
 * \param delayp Total I/O latency in frames
 * \return zero on success otherwise a negative error code
 *
 * The avail and delay values returned are in sync.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_avail_delay(snd_pcm_t *pcm,
			snd_pcm_sframes_t *availp,
			snd_pcm_sframes_t *delayp)
{
	snd_pcm_sframes_t sf;
	int err, ok = 0;

	assert(pcm && availp && delayp);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	snd_pcm_lock(pcm->fast_op_arg);
	err = __snd_pcm_hwsync(pcm);
	if (err < 0)
		goto unlock;

	/*
	 * Delay value is relative to avail, so we have to
	 * loop to avoid reporting stale delay data.
	 */
	while (1) {
		sf = __snd_pcm_avail_update(pcm);
		if (sf < 0) {
			err = (int)sf;
			goto unlock;
		}
		if (ok && sf == *availp)
			break;
		*availp = sf;
		err = __snd_pcm_delay(pcm, delayp);
		if (err < 0)
			goto unlock;
		ok = 1;
	}
	err = 0;
 unlock:
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

/**
 * \brief Silence an area
 * \param dst_area area specification
 * \param dst_offset offset in frames inside area
 * \param samples samples to silence
 * \param format PCM sample format
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_area_silence(const snd_pcm_channel_area_t *dst_area, snd_pcm_uframes_t dst_offset,
			 unsigned int samples, snd_pcm_format_t format)
{
	/* FIXME: sub byte resolution and odd dst_offset */
	char *dst;
	unsigned int dst_step;
	int width;
	uint64_t silence;
	if (!dst_area->addr)
		return 0;
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	width = snd_pcm_format_physical_width(format);
	silence = snd_pcm_format_silence_64(format);
        /*
         * Iterate copying silent sample for sample data aligned to 64 bit.
         * This is a fast path.
         */
        if (dst_area->step == (unsigned int) width &&
            width != 24 &&
            ((intptr_t)dst & 7) == 0) {
		unsigned int dwords = samples * width / 64;
		uint64_t *dstp = (uint64_t *)dst;
		samples -= dwords * 64 / width;
		while (dwords-- > 0)
			*dstp++ = silence;
		if (samples == 0)
			return 0;
		dst = (char *)dstp;
	}
	dst_step = dst_area->step / 8;
	switch (width) {
	case 4: {
		uint8_t s0 = silence & 0xf0;
		uint8_t s1 = silence & 0x0f;
		int dstbit = dst_area->first % 8;
		int dstbit_step = dst_area->step % 8;
		while (samples-- > 0) {
			if (dstbit) {
				*dst &= 0xf0;
				*dst |= s1;
			} else {
				*dst &= 0x0f;
				*dst |= s0;
			}
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
		break;
	}
	case 8: {
		uint8_t sil = silence;
		while (samples-- > 0) {
			*dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 16: {
		uint16_t sil = silence;
		while (samples-- > 0) {
			*(uint16_t*)dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 24: {
		while (samples-- > 0) {
#ifdef SNDRV_LITTLE_ENDIAN
			*(dst + 0) = silence >> 0;
			*(dst + 1) = silence >> 8;
			*(dst + 2) = silence >> 16;
#else
			*(dst + 2) = silence >> 0;
			*(dst + 1) = silence >> 8;
			*(dst + 0) = silence >> 16;
#endif
			dst += dst_step;
		}
	}
		break;
	case 32: {
		uint32_t sil = silence;
		while (samples-- > 0) {
			*(uint32_t*)dst = sil;
			dst += dst_step;
		}
		break;
	}
	case 64: {
		while (samples-- > 0) {
			*(uint64_t*)dst = silence;
			dst += dst_step;
		}
		break;
	}
	default:
		SNDMSG("invalid format width %d", width);
		return -EINVAL;
	}
	return 0;
}

/**
 * \brief Silence one or more areas
 * \param dst_areas areas specification (one for each channel)
 * \param dst_offset offset in frames inside area
 * \param channels channels count
 * \param frames frames to silence
 * \param format PCM sample format
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_areas_silence(const snd_pcm_channel_area_t *dst_areas, snd_pcm_uframes_t dst_offset,
			  unsigned int channels, snd_pcm_uframes_t frames, snd_pcm_format_t format)
{
	int width = snd_pcm_format_physical_width(format);
	while (channels > 0) {
		void *addr = dst_areas->addr;
		unsigned int step = dst_areas->step;
		const snd_pcm_channel_area_t *begin = dst_areas;
		int channels1 = channels;
		unsigned int chns = 0;
		int err;
		while (1) {
			channels1--;
			chns++;
			dst_areas++;
			if (channels1 == 0 ||
			    dst_areas->addr != addr ||
			    dst_areas->step != step ||
			    dst_areas->first != dst_areas[-1].first + width)
				break;
		}
		if (chns > 1 && chns * width == step) {
			/* Collapse the areas */
			snd_pcm_channel_area_t d;
			d.addr = begin->addr;
			d.first = begin->first;
			d.step = width;
			err = snd_pcm_area_silence(&d, dst_offset * chns, frames * chns, format);
			channels -= chns;
		} else {
			err = snd_pcm_area_silence(begin, dst_offset, frames, format);
			dst_areas = begin + 1;
			channels--;
		}
		if (err < 0)
			return err;
	}
	return 0;
}


/**
 * \brief Copy an area
 * \param dst_area destination area specification
 * \param dst_offset offset in frames inside destination area
 * \param src_area source area specification
 * \param src_offset offset in frames inside source area
 * \param samples samples to copy
 * \param format PCM sample format
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_area_copy(const snd_pcm_channel_area_t *dst_area, snd_pcm_uframes_t dst_offset,
		      const snd_pcm_channel_area_t *src_area, snd_pcm_uframes_t src_offset,
		      unsigned int samples, snd_pcm_format_t format)
{
	/* FIXME: sub byte resolution and odd dst_offset */
	const char *src;
	char *dst;
	int width;
	int src_step, dst_step;
	if (dst_area == src_area && dst_offset == src_offset)
		return 0;
	if (!src_area->addr)
		return snd_pcm_area_silence(dst_area, dst_offset, samples, format);
	src = snd_pcm_channel_area_addr(src_area, src_offset);
	if (!dst_area->addr)
		return 0;
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	width = snd_pcm_format_physical_width(format);
	if (src_area->step == (unsigned int) width &&
	    dst_area->step == (unsigned int) width) {
		size_t bytes = samples * width / 8;
		samples -= bytes * 8 / width;
		assert(src < dst || src >= dst + bytes);
		assert(dst < src || dst >= src + bytes);
		memcpy(dst, src, bytes);
		if (samples == 0)
			return 0;
	}
	src_step = src_area->step / 8;
	dst_step = dst_area->step / 8;
	switch (width) {
	case 4: {
		int srcbit = src_area->first % 8;
		int srcbit_step = src_area->step % 8;
		int dstbit = dst_area->first % 8;
		int dstbit_step = dst_area->step % 8;
		while (samples-- > 0) {
			unsigned char srcval;
			if (srcbit)
				srcval = *src & 0x0f;
			else
				srcval = *src & 0xf0;
			if (dstbit)
				*dst &= 0xf0;
			else
				*dst &= 0x0f;
			*dst |= srcval;
			src += src_step;
			srcbit += srcbit_step;
			if (srcbit == 8) {
				src++;
				srcbit = 0;
			}
			dst += dst_step;
			dstbit += dstbit_step;
			if (dstbit == 8) {
				dst++;
				dstbit = 0;
			}
		}
		break;
	}
	case 8: {
		while (samples-- > 0) {
			*dst = *src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 16: {
		while (samples-- > 0) {
			*(uint16_t*)dst = *(const uint16_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 24:
		while (samples-- > 0) {
			*(dst + 0) = *(src + 0);
			*(dst + 1) = *(src + 1);
			*(dst + 2) = *(src + 2);
			src += src_step;
			dst += dst_step;
		}
		break;
	case 32: {
		while (samples-- > 0) {
			*(uint32_t*)dst = *(const uint32_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	case 64: {
		while (samples-- > 0) {
			*(uint64_t*)dst = *(const uint64_t*)src;
			src += src_step;
			dst += dst_step;
		}
		break;
	}
	default:
		SNDMSG("invalid format width %d", width);
		return -EINVAL;
	}
	return 0;
}

/**
 * \brief Copy one or more areas
 * \param dst_areas destination areas specification (one for each channel)
 * \param dst_offset offset in frames inside destination area
 * \param src_areas source areas specification (one for each channel)
 * \param src_offset offset in frames inside source area
 * \param channels channels count
 * \param frames frames to copy
 * \param format PCM sample format
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_areas_copy(const snd_pcm_channel_area_t *dst_areas, snd_pcm_uframes_t dst_offset,
		       const snd_pcm_channel_area_t *src_areas, snd_pcm_uframes_t src_offset,
		       unsigned int channels, snd_pcm_uframes_t frames, snd_pcm_format_t format)
{
	int width = snd_pcm_format_physical_width(format);
	assert(dst_areas);
	assert(src_areas);
	if (! channels) {
		SNDMSG("invalid channels %d", channels);
		return -EINVAL;
	}
	if (! frames) {
		SNDMSG("invalid frames %ld", frames);
		return -EINVAL;
	}
	while (channels > 0) {
		unsigned int step = src_areas->step;
		void *src_addr = src_areas->addr;
		const snd_pcm_channel_area_t *src_start = src_areas;
		void *dst_addr = dst_areas->addr;
		const snd_pcm_channel_area_t *dst_start = dst_areas;
		int channels1 = channels;
		unsigned int chns = 0;
		while (dst_areas->step == step) {
			channels1--;
			chns++;
			src_areas++;
			dst_areas++;
			if (channels1 == 0 ||
			    src_areas->step != step ||
			    src_areas->addr != src_addr ||
			    dst_areas->addr != dst_addr ||
			    src_areas->first != src_areas[-1].first + width ||
			    dst_areas->first != dst_areas[-1].first + width)
				break;
		}
		if (chns > 1 && chns * width == step) {
			if (src_offset != dst_offset ||
			    src_start->addr != dst_start->addr ||
			    src_start->first != dst_start->first) {
				/* Collapse the areas */
				snd_pcm_channel_area_t s, d;
				s.addr = src_start->addr;
				s.first = src_start->first;
				s.step = width;
				d.addr = dst_start->addr;
				d.first = dst_start->first;
				d.step = width;
				snd_pcm_area_copy(&d, dst_offset * chns,
						  &s, src_offset * chns, 
						  frames * chns, format);
			}
			channels -= chns;
		} else {
			snd_pcm_area_copy(dst_start, dst_offset,
					  src_start, src_offset,
					  frames, format);
			src_areas = src_start + 1;
			dst_areas = dst_start + 1;
			channels--;
		}
	}
	return 0;
}

/**
 * \brief Copy one or more areas
 * \param dst_channels destination areas specification (one for each channel)
 * \param dst_offset offset in frames inside destination area
 * \param dst_size size in frames of the destination buffer
 * \param src_channels source areas specification (one for each channel)
 * \param src_offset offset in frames inside source area
 * \param src_size size in frames of the source buffer
 * \param channels channels count
 * \param frames frames to copy
 * \param format PCM sample format
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_areas_copy_wrap(const snd_pcm_channel_area_t *dst_channels,
			    snd_pcm_uframes_t dst_offset,
			    const snd_pcm_uframes_t dst_size,
			    const snd_pcm_channel_area_t *src_channels,
			    snd_pcm_uframes_t src_offset,
			    const snd_pcm_uframes_t src_size,
			    const unsigned int channels,
			    snd_pcm_uframes_t frames,
			    const snd_pcm_format_t format)
{
	while (frames > 0) {
		int err;
		snd_pcm_uframes_t xfer = frames;
		/* do not write above the destination buffer */
		if ((dst_offset + xfer) > dst_size)
			xfer = dst_size - dst_offset;
		/* do not read from above the source buffer */
		if ((src_offset + xfer) > src_size)
			xfer = src_size - src_offset;
		err = snd_pcm_areas_copy(dst_channels, dst_offset, src_channels,
					 src_offset, channels, xfer, format);
		if (err < 0)
			return err;

		dst_offset += xfer;
		if (dst_offset >= dst_size)
			dst_offset = 0;
		src_offset += xfer;
		if (src_offset >= src_size)
			src_offset = 0;
		frames -= xfer;
	}

	return 0;
}

static void dump_one_param(snd_pcm_hw_params_t *params, unsigned int k, snd_output_t *out)
{
	snd_output_printf(out, "%s: ", snd_pcm_hw_param_name(k));
	snd_pcm_hw_param_dump(params, k, out);
	snd_output_putc(out, '\n');
}

/**
 * \brief Dump a PCM hardware configuration space
 * \param params Configuration space
 * \param out Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_hw_params_dump(snd_pcm_hw_params_t *params, snd_output_t *out)
{
	unsigned int k;
	for (k = SND_PCM_HW_PARAM_FIRST_MASK; k <= SND_PCM_HW_PARAM_LAST_MASK; k++)
		dump_one_param(params, k, out);
	for (k = SND_PCM_HW_PARAM_FIRST_INTERVAL; k <= SND_PCM_HW_PARAM_LAST_INTERVAL; k++)
		dump_one_param(params, k, out);
	return 0;
}

/**
 * \brief Check if hardware supports sample-resolution mmap for given configuration
 * \param params Configuration space
 * \retval 0 Hardware doesn't support sample-resolution mmap
 * \retval 1 Hardware supports sample-resolution mmap
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_can_mmap_sample_resolution(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_MMAP_VALID);
}

/**
 * \brief Check if hardware does double buffering for start/stop for given configuration
 * \param params Configuration space
 * \retval 0 Hardware doesn't do double buffering for start/stop
 * \retval 1 Hardware does double buffering for start/stop
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_is_double(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_DOUBLE);
}

/**
 * \brief Check if hardware does double buffering for data transfers for given configuration
 * \param params Configuration space
 * \retval 0 Hardware doesn't do double buffering for data transfers
 * \retval 1 Hardware does double buffering for data transfers
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_is_batch(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_BATCH);
}

/**
 * \brief Check if hardware does block transfers for samples for given configuration
 * \param params Configuration space
 * \retval 0 Hardware doesn't block transfers
 * \retval 1 Hardware does block transfers
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_is_block_transfer(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_BLOCK_TRANSFER);
}

/**
 * \brief Check if timestamps are monotonic for given configuration
 * \param params Configuration space
 * \retval 0 Device doesn't do monotomic timestamps
 * \retval 1 Device does monotonic timestamps
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_is_monotonic(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SND_PCM_INFO_MONOTONIC);
}

/**
 * \brief Check if hardware supports overrange detection
 * \param params Configuration space
 * \retval 0 Hardware doesn't support overrange detection
 * \retval 1 Hardware supports overrange detection
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_can_overrange(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_OVERRANGE);
}

/**
 * \brief Check if hardware supports pause
 * \param params Configuration space
 * \retval 0 Hardware doesn't support pause
 * \retval 1 Hardware supports pause
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_can_pause(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_PAUSE);
}

/**
 * \brief Check if hardware supports resume
 * \param params Configuration space
 * \retval 0 Hardware doesn't support resume
 * \retval 1 Hardware supports resume
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_can_resume(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_RESUME);
}

/**
 * \brief Check if hardware does half-duplex only
 * \param params Configuration space
 * \retval 0 Hardware doesn't do half-duplex
 * \retval 1 Hardware does half-duplex
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_is_half_duplex(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_HALF_DUPLEX);
}

/**
 * \brief Check if hardware does joint-duplex (playback and capture are somewhat correlated)
 * \param params Configuration space
 * \retval 0 Hardware doesn't do joint-duplex
 * \retval 1 Hardware does joint-duplex
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_is_joint_duplex(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_JOINT_DUPLEX);
}

/**
 * \brief Check if hardware supports synchronized start with sample resolution
 * \param params Configuration space
 * \retval 0 Hardware doesn't support synchronized start
 * \retval 1 Hardware supports synchronized start
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_can_sync_start(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_SYNC_START);
}

/**
 * \brief Check if hardware can disable period wakeups
 * \param params Configuration space
 * \retval 0 Hardware cannot disable period wakeups
 * \retval 1 Hardware can disable period wakeups
 */
int snd_pcm_hw_params_can_disable_period_wakeup(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_NO_PERIOD_WAKEUP);
}

/**
 * \brief Check if hardware is capable of perfect drain
 * \param params Configuration space
 * \retval 0 Hardware doesn't do perfect drain
 * \retval 1 Hardware does perfect drain
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 *
 * Perfect drain means that the hardware does not use samples
 * beyond the stream application pointer.
 */
int snd_pcm_hw_params_is_perfect_drain(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	return !!(params->info & SNDRV_PCM_INFO_PERFECT_DRAIN);
}

/**
 * \brief Check if hardware supports audio wallclock timestamps
 * \param params Configuration space
 * \retval 0 Hardware doesn't support audio wallclock timestamps
 * \retval 1 Hardware supports audio wallclock timestamps
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_supports_audio_wallclock_ts(const snd_pcm_hw_params_t *params)
{
	/* deprecated */
	return snd_pcm_hw_params_supports_audio_ts_type(params,
							SNDRV_PCM_AUDIO_TSTAMP_TYPE_COMPAT);
}

/**
 * \brief Check if hardware supports type of audio timestamps
 * \param params Configuration space
 * \param type   Audio timestamp type
 * \retval 0 Hardware doesn't support type of audio timestamps
 * \retval 1 Hardware supports type of audio timestamps
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_supports_audio_ts_type(const snd_pcm_hw_params_t *params, int type)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return 0; /* FIXME: should be a negative error? */
	}
	switch (type) {
	case SNDRV_PCM_AUDIO_TSTAMP_TYPE_COMPAT:
		return !!(params->info & SNDRV_PCM_INFO_HAS_WALL_CLOCK); /* deprecated */
	case SNDRV_PCM_AUDIO_TSTAMP_TYPE_DEFAULT:
		return 1; /* always supported, based on hw_ptr */
	case SNDRV_PCM_AUDIO_TSTAMP_TYPE_LINK:
		return !!(params->info & SNDRV_PCM_INFO_HAS_LINK_ATIME);
	case SNDRV_PCM_AUDIO_TSTAMP_TYPE_LINK_ABSOLUTE:
		return !!(params->info & SNDRV_PCM_INFO_HAS_LINK_ABSOLUTE_ATIME);
	case SNDRV_PCM_AUDIO_TSTAMP_TYPE_LINK_ESTIMATED:
		return !!(params->info & SNDRV_PCM_INFO_HAS_LINK_ESTIMATED_ATIME);
	case SNDRV_PCM_AUDIO_TSTAMP_TYPE_LINK_SYNCHRONIZED:
		return !!(params->info & SNDRV_PCM_INFO_HAS_LINK_SYNCHRONIZED_ATIME);
	default:
		return 0;
	}
}

/**
 * \brief Get rate exact info from a configuration space
 * \param params Configuration space
 * \param rate_num Pointer to returned rate numerator
 * \param rate_den Pointer to returned rate denominator
 * \return 0 otherwise a negative error code if the info is not available
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_get_rate_numden(const snd_pcm_hw_params_t *params,
				      unsigned int *rate_num, unsigned int *rate_den)
{
	assert(params);
	if (CHECK_SANITY(params->rate_den == 0)) {
		SNDMSG("invalid rate_den value");
		return -EINVAL;
	}
	*rate_num = params->rate_num;
	*rate_den = params->rate_den;
	return 0;
}

/**
 * \brief Get sample resolution info from a configuration space
 * \param params Configuration space
 * \return sample resolution (in bits) otherwise a negative error code if the info is not available
 *
 * For linear formats, this function returns sample resolution -
 * used bits starting from the first usable significant bit defined by
 * the format (e.g. bit 31 for S32_LE format or bit 23 for S24_LE format -
 * starting from bit zero). Application may use full sample bit range defined
 * by the format, but additional bits (outside this sample resolution) are
 * stripped (not processed).
 *
 * For non-linear formats, this value may have a special meaning which may be defined in future.
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_get_sbits(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->msbits == 0)) {
		SNDMSG("invalid msbits value");
		return -EINVAL;
	}
	return params->msbits;
}

/**
 * \brief Get hardware FIFO size info from a configuration space
 * \param params Configuration space
 * \return FIFO size in frames otherwise a negative error code if the info is not available
 *
 * This function should only be called when the configuration space
 * contains a single configuration. Call #snd_pcm_hw_params to choose
 * a single configuration from the configuration space.
 */
int snd_pcm_hw_params_get_fifo_size(const snd_pcm_hw_params_t *params)
{
	assert(params);
	if (CHECK_SANITY(params->info == ~0U)) {
		SNDMSG("invalid PCM info field");
		return -EINVAL;
	}
	return params->fifo_size;
}

/**
 * \brief Get hardware synchronization ID from a PCM info container
 * \param params Configuration space
 * \return 16-byte synchronization ID (use #SND_PCM_HW_PARAMS_SYNC_SIZE)
 *
 * This synchronization ID determines the similar clocks for the
 * PCM stream between multiple devices (including different cards).
 * "All zeros" means "not set". The contents of the ID can be used
 * only for a comparison with the contents of another ID returned
 * from this function. Applications should not do a comparison with
 * hard-coded values, because the implementation generating such
 * synchronization IDs may be changed in future.
 */
const unsigned char *snd_pcm_hw_params_get_sync(const snd_pcm_hw_params_t *params)
{
	assert(params);
	return params->sync;
}

/**
 * \brief Fill params with a full configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 *
 * The configuration space will be filled with all possible ranges
 * for the PCM device.
 *
 * Note that the configuration space may be constrained by the
 * currently installed configuration on the PCM device. To remove
 * any constrains, free the configuration with #snd_pcm_hw_free
 * first.
 */
int snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	_snd_pcm_hw_params_any(params);
	return snd_pcm_hw_refine(pcm, params);
}

/**
 * \brief get size of #snd_pcm_access_mask_t
 * \return size in bytes
 */
size_t snd_pcm_access_mask_sizeof()
{
	return sizeof(snd_pcm_access_mask_t);
}

/**
 * \brief allocate an empty #snd_pcm_access_mask_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_pcm_access_mask_malloc(snd_pcm_access_mask_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_access_mask_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_pcm_access_mask_t
 * \param obj pointer to object to free
 */
void snd_pcm_access_mask_free(snd_pcm_access_mask_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_pcm_access_mask_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_pcm_access_mask_copy(snd_pcm_access_mask_t *dst, const snd_pcm_access_mask_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief reset all bits in a #snd_pcm_access_mask_t
 * \param mask pointer to mask
 */
void snd_pcm_access_mask_none(snd_pcm_access_mask_t *mask)
{
	snd_mask_none((snd_mask_t *) mask);
}

/**
 * \brief set all bits in a #snd_pcm_access_mask_t
 * \param mask pointer to mask
 */
void snd_pcm_access_mask_any(snd_pcm_access_mask_t *mask)
{
	snd_mask_any((snd_mask_t *) mask);
}

/**
 * \brief test the presence of an access type in a #snd_pcm_access_mask_t
 * \param mask pointer to mask
 * \param val access type
 */
int snd_pcm_access_mask_test(const snd_pcm_access_mask_t *mask, snd_pcm_access_t val)
{
	return snd_mask_test((const snd_mask_t *) mask, (unsigned long) val);
}

/**
 * \brief test, if given a #snd_pcm_access_mask_t is empty
 * \param mask pointer to mask
 * \retval 0 not empty
 * \retval 1 empty
 */
int snd_pcm_access_mask_empty(const snd_pcm_access_mask_t *mask)
{
	return snd_mask_empty((const snd_mask_t *) mask);
}

/**
 * \brief make an access type present in a #snd_pcm_access_mask_t
 * \param mask pointer to mask
 * \param val access type
 */
void snd_pcm_access_mask_set(snd_pcm_access_mask_t *mask, snd_pcm_access_t val)
{
	snd_mask_set((snd_mask_t *) mask, (unsigned long) val);
}

/**
 * \brief make an access type missing from a #snd_pcm_access_mask_t
 * \param mask pointer to mask
 * \param val access type
 */
void snd_pcm_access_mask_reset(snd_pcm_access_mask_t *mask, snd_pcm_access_t val)
{
	snd_mask_reset((snd_mask_t *) mask, (unsigned long) val);
}

/**
 * \brief get size of #snd_pcm_format_mask_t
 * \return size in bytes
 */
size_t snd_pcm_format_mask_sizeof()
{
	return sizeof(snd_pcm_format_mask_t);
}

/**
 * \brief allocate an empty #snd_pcm_format_mask_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_pcm_format_mask_malloc(snd_pcm_format_mask_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_format_mask_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_pcm_format_mask_t
 * \param obj pointer to object to free
 */
void snd_pcm_format_mask_free(snd_pcm_format_mask_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_pcm_format_mask_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_pcm_format_mask_copy(snd_pcm_format_mask_t *dst, const snd_pcm_format_mask_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief reset all bits in a #snd_pcm_format_mask_t
 * \param mask pointer to mask
 */
void snd_pcm_format_mask_none(snd_pcm_format_mask_t *mask)
{
	snd_mask_none((snd_mask_t *) mask);
}

/**
 * \brief set all bits in a #snd_pcm_format_mask_t
 * \param mask pointer to mask
 */
void snd_pcm_format_mask_any(snd_pcm_format_mask_t *mask)
{
	snd_mask_any((snd_mask_t *) mask);
}

/**
 * \brief test the presence of a format in a #snd_pcm_format_mask_t
 * \param mask pointer to mask
 * \param val format
 */
int snd_pcm_format_mask_test(const snd_pcm_format_mask_t *mask, snd_pcm_format_t val)
{
	return snd_mask_test((const snd_mask_t *) mask, (unsigned long) val);
}

/**
 * \brief test, if given a #snd_pcm_format_mask_t is empty
 * \param mask pointer to mask
 * \retval 0 not empty
 * \retval 1 empty
 */
int snd_pcm_format_mask_empty(const snd_pcm_format_mask_t *mask)
{
	return snd_mask_empty((const snd_mask_t *) mask);
}

/**
 * \brief make a format present in a #snd_pcm_format_mask_t
 * \param mask pointer to mask
 * \param val format
 */
void snd_pcm_format_mask_set(snd_pcm_format_mask_t *mask, snd_pcm_format_t val)
{
	snd_mask_set((snd_mask_t *) mask, (unsigned long) val);
}

/**
 * \brief make a format missing from a #snd_pcm_format_mask_t
 * \param mask pointer to mask
 * \param val format
 */
void snd_pcm_format_mask_reset(snd_pcm_format_mask_t *mask, snd_pcm_format_t val)
{
	snd_mask_reset((snd_mask_t *) mask, (unsigned long) val);
}


/**
 * \brief get size of #snd_pcm_subformat_mask_t
 * \return size in bytes
 */
size_t snd_pcm_subformat_mask_sizeof()
{
	return sizeof(snd_pcm_subformat_mask_t);
}

/**
 * \brief allocate an empty #snd_pcm_subformat_mask_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_pcm_subformat_mask_malloc(snd_pcm_subformat_mask_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_subformat_mask_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_pcm_subformat_mask_t
 * \param obj pointer to object to free
 */
void snd_pcm_subformat_mask_free(snd_pcm_subformat_mask_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_pcm_subformat_mask_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_pcm_subformat_mask_copy(snd_pcm_subformat_mask_t *dst, const snd_pcm_subformat_mask_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief reset all bits in a #snd_pcm_subformat_mask_t
 * \param mask pointer to mask
 */
void snd_pcm_subformat_mask_none(snd_pcm_subformat_mask_t *mask)
{
	snd_mask_none((snd_mask_t *) mask);
}

/**
 * \brief set all bits in a #snd_pcm_subformat_mask_t
 * \param mask pointer to mask
 */
void snd_pcm_subformat_mask_any(snd_pcm_subformat_mask_t *mask)
{
	snd_mask_any((snd_mask_t *) mask);
}

/**
 * \brief test the presence of a subformat in a #snd_pcm_subformat_mask_t
 * \param mask pointer to mask
 * \param val subformat
 */
int snd_pcm_subformat_mask_test(const snd_pcm_subformat_mask_t *mask, snd_pcm_subformat_t val)
{
	return snd_mask_test((const snd_mask_t *) mask, (unsigned long) val);
}

/**
 * \brief test, if given a #snd_pcm_subformat_mask_t is empty
 * \param mask pointer to mask
 * \retval 0 not empty
 * \retval 1 empty
 */
int snd_pcm_subformat_mask_empty(const snd_pcm_subformat_mask_t *mask)
{
	return snd_mask_empty((const snd_mask_t *) mask);
}

/**
 * \brief make a subformat present in a #snd_pcm_subformat_mask_t
 * \param mask pointer to mask
 * \param val subformat
 */
void snd_pcm_subformat_mask_set(snd_pcm_subformat_mask_t *mask, snd_pcm_subformat_t val)
{
	snd_mask_set((snd_mask_t *) mask, (unsigned long) val);
}

/**
 * \brief make a subformat missing from a #snd_pcm_subformat_mask_t
 * \param mask pointer to mask
 * \param val subformat
 */
void snd_pcm_subformat_mask_reset(snd_pcm_subformat_mask_t *mask, snd_pcm_subformat_t val)
{
	snd_mask_reset((snd_mask_t *) mask, (unsigned long) val);
}


/**
 * \brief get size of #snd_pcm_hw_params_t
 * \return size in bytes
 */
size_t snd_pcm_hw_params_sizeof()
{
	return sizeof(snd_pcm_hw_params_t);
}

/**
 * \brief allocate an invalid #snd_pcm_hw_params_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_pcm_hw_params_malloc(snd_pcm_hw_params_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_hw_params_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_pcm_hw_params_t
 * \param obj pointer to object to free
 */
void snd_pcm_hw_params_free(snd_pcm_hw_params_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_pcm_hw_params_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_pcm_hw_params_copy(snd_pcm_hw_params_t *dst, const snd_pcm_hw_params_t *src)
{
	assert(dst && src);
	*dst = *src;
}


/**
 * \brief Extract access type from a configuration space
 * \param params Configuration space
 * \param access Returned value
 * \return access type otherwise a negative error code if the configuration space does not contain a single value
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_access)(const snd_pcm_hw_params_t *params, snd_pcm_access_t *access)
#else
int snd_pcm_hw_params_get_access(const snd_pcm_hw_params_t *params, snd_pcm_access_t *access)
#endif
{
	unsigned int _val;
	int err = snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_ACCESS, &_val, NULL);
	if (err >= 0)
		*access = _val;
	return err;
}

/**
 * \brief Verify if an access type is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param access access type
 * \return 0 if available a negative error code otherwise
 */
int snd_pcm_hw_params_test_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t access)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_ACCESS, access, 0);
}

/**
 * \brief Restrict a configuration space to contain only one access type
 * \param pcm PCM handle
 * \param params Configuration space
 * \param access access type
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t access)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_ACCESS, access, 0);
}

/**
 * \brief Restrict a configuration space to contain only its first access type
 * \param pcm PCM handle
 * \param params Configuration space
 * \param access Returned first access type
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_access_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t *access)
#else
int snd_pcm_hw_params_set_access_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t *access)
#endif
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_ACCESS, access, NULL);
}

/**
 * \brief Restrict a configuration space to contain only its last access type
 * \param pcm PCM handle
 * \param params Configuration space
 * \param access Returned last access type
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_access_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t *access)
#else
int snd_pcm_hw_params_set_access_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t *access)
#endif
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_ACCESS, access, NULL);
}

/**
 * \brief Restrict a configuration space to contain only a set of access types
 * \param pcm PCM handle
 * \param params Configuration space
 * \param mask Access mask
 * \return 0 otherwise a negative error code
 */
int snd_pcm_hw_params_set_access_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_mask_t *mask)
{
	return snd_pcm_hw_param_set_mask(pcm, params, SND_TRY, SND_PCM_HW_PARAM_ACCESS, (snd_mask_t *) mask);
}

/**
 * \brief Get access mask from a configuration space
 * \param params Configuration space
 * \param mask Returned Access mask
 */
int snd_pcm_hw_params_get_access_mask(snd_pcm_hw_params_t *params, snd_pcm_access_mask_t *mask)
{
	if (params == NULL || mask == NULL)
		return -EINVAL;
	snd_pcm_access_mask_copy(mask, snd_pcm_hw_param_get_mask(params, SND_PCM_HW_PARAM_ACCESS));
	return 0;
}


/**
 * \brief Extract format from a configuration space
 * \param params Configuration space
 * \param format returned format
 * \return format otherwise a negative error code if the configuration space does not contain a single value
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_format)(const snd_pcm_hw_params_t *params, snd_pcm_format_t *format)
#else
int snd_pcm_hw_params_get_format(const snd_pcm_hw_params_t *params, snd_pcm_format_t *format)
#endif
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_FORMAT, (unsigned int *)format, NULL);
}

/**
 * \brief Verify if a format is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param format format
 * \return 0 if available a negative error code otherwise
 */
int snd_pcm_hw_params_test_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t format)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_FORMAT, format, 0);
}

/**
 * \brief Restrict a configuration space to contain only one format
 * \param pcm PCM handle
 * \param params Configuration space
 * \param format format
 * \return 0 otherwise a negative error code
 */
int snd_pcm_hw_params_set_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t format)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_FORMAT, format, 0);
}

/**
 * \brief Restrict a configuration space to contain only its first format
 * \param pcm PCM handle
 * \param params Configuration space
 * \param format Returned first format
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_format_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t *format)
#else
int snd_pcm_hw_params_set_format_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t *format)
#endif
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_FORMAT, (unsigned int *)format, NULL);
}

/**
 * \brief Restrict a configuration space to contain only its last format
 * \param pcm PCM handle
 * \param params Configuration space
 * \param format Returned last format
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_format_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t *format)
#else
int snd_pcm_hw_params_set_format_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t *format)
#endif
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_FORMAT, (unsigned int *)format, NULL);
}

/**
 * \brief Restrict a configuration space to contain only a set of formats
 * \param pcm PCM handle
 * \param params Configuration space
 * \param mask Format mask
 * \return 0 otherwise a negative error code
 */
int snd_pcm_hw_params_set_format_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_mask_t *mask)
{
	return snd_pcm_hw_param_set_mask(pcm, params, SND_TRY, SND_PCM_HW_PARAM_FORMAT, (snd_mask_t *) mask);
}

/**
 * \brief Get format mask from a configuration space
 * \param params Configuration space
 * \param mask Returned Format mask
 */
void snd_pcm_hw_params_get_format_mask(snd_pcm_hw_params_t *params, snd_pcm_format_mask_t *mask)
{
	snd_pcm_format_mask_copy(mask, snd_pcm_hw_param_get_mask(params, SND_PCM_HW_PARAM_FORMAT));
}


/**
 * \brief Extract subformat from a configuration space
 * \param params Configuration space
 * \param subformat Returned subformat value
 * \return subformat otherwise a negative error code if the configuration space does not contain a single value
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_subformat)(const snd_pcm_hw_params_t *params, snd_pcm_subformat_t *subformat)
#else
int snd_pcm_hw_params_get_subformat(const snd_pcm_hw_params_t *params, snd_pcm_subformat_t *subformat)
#endif
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_SUBFORMAT, (unsigned int *)subformat, NULL);
}

/**
 * \brief Verify if a subformat is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param subformat subformat value
 * \return 0 if available a negative error code otherwise
 */
int snd_pcm_hw_params_test_subformat(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t subformat)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_SUBFORMAT, subformat, 0);
}

/**
 * \brief Restrict a configuration space to contain only one subformat
 * \param pcm PCM handle
 * \param params Configuration space
 * \param subformat subformat value
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_subformat(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t subformat)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_SUBFORMAT, subformat, 0);
}

/**
 * \brief Restrict a configuration space to contain only its first subformat
 * \param pcm PCM handle
 * \param params Configuration space
 * \param subformat Returned subformat
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_subformat_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t *subformat)
#else
int snd_pcm_hw_params_set_subformat_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t *subformat)
#endif
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_SUBFORMAT, (unsigned int *)subformat, NULL);
}

/**
 * \brief Restrict a configuration space to contain only its last subformat
 * \param pcm PCM handle
 * \param params Configuration space
 * \param subformat Returned subformat
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_subformat_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t *subformat)
#else
int snd_pcm_hw_params_set_subformat_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t *subformat)
#endif
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_SUBFORMAT, (unsigned int *)subformat, NULL);
}

/**
 * \brief Restrict a configuration space to contain only a set of subformats
 * \param pcm PCM handle
 * \param params Configuration space
 * \param mask Subformat mask
 * \return 0 otherwise a negative error code
 */
int snd_pcm_hw_params_set_subformat_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_mask_t *mask)
{
	return snd_pcm_hw_param_set_mask(pcm, params, SND_TRY, SND_PCM_HW_PARAM_SUBFORMAT, (snd_mask_t *) mask);
}

/**
 * \brief Get subformat mask from a configuration space
 * \param params Configuration space
 * \param mask Returned Subformat mask
 */
void snd_pcm_hw_params_get_subformat_mask(snd_pcm_hw_params_t *params, snd_pcm_subformat_mask_t *mask)
{
	snd_pcm_subformat_mask_copy(mask, snd_pcm_hw_param_get_mask(params, SND_PCM_HW_PARAM_SUBFORMAT));
}


/**
 * \brief Extract channels from a configuration space
 * \param params Configuration space
 * \param val Returned channels count
 * \return 0 otherwise a negative error code if the configuration space does not contain a single value
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_channels)(const snd_pcm_hw_params_t *params, unsigned int *val)
#else
int snd_pcm_hw_params_get_channels(const snd_pcm_hw_params_t *params, unsigned int *val)
#endif
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

/**
 * \brief Extract minimum channels count from a configuration space
 * \param params Configuration space
 * \param val minimum channels count
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_channels_min)(const snd_pcm_hw_params_t *params, unsigned int *val)
#else
int snd_pcm_hw_params_get_channels_min(const snd_pcm_hw_params_t *params, unsigned int *val)
#endif
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

/**
 * \brief Extract maximum channels count from a configuration space
 * \param params Configuration space
 * \param val maximum channels count
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_channels_max)(const snd_pcm_hw_params_t *params, unsigned int *val)
#else
int snd_pcm_hw_params_get_channels_max(const snd_pcm_hw_params_t *params, unsigned int *val)
#endif
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

/**
 * \brief Verify if a channels count is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val channels count
 * \return 0 if available a negative error code otherwise
 */
int snd_pcm_hw_params_test_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_CHANNELS, val, 0);
}

/**
 * \brief Restrict a configuration space to contain only one channels count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val channels count
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_CHANNELS, val, 0);
}

/**
 * \brief Restrict a configuration space with a minimum channels count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val minimum channels count (on return filled with actual minimum)
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_channels_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

/**
 * \brief Restrict a configuration space with a maximum channels count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val maximum channels count (on return filled with actual maximum)
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_channels_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

/**
 * \brief Restrict a configuration space to have channels counts in a given range
 * \param pcm PCM handle
 * \param params Configuration space
 * \param min minimum channels count (on return filled with actual minimum)
 * \param max maximum channels count (on return filled with actual maximum)
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_channels_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, unsigned int *max)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_CHANNELS, min, NULL, max, NULL);
}

/**
 * \brief Restrict a configuration space to have channels count nearest to a target
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val target channels count, returned chosen channels count
 * \return 0 otherwise a negative error code if configuration space is empty
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_channels_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
#else
int snd_pcm_hw_params_set_channels_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
#endif
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

/**
 * \brief Restrict a configuration space to contain only its minimum channels count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val minimum channels count
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_channels_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
#else
int snd_pcm_hw_params_set_channels_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
#endif
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

/**
 * \brief Restrict a configuration space to contain only its maximum channels count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val maximum channels count
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_channels_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
#else
int snd_pcm_hw_params_set_channels_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
#endif
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}


/**
 * \brief Extract rate from a configuration space
 * \param params Configuration space
 * \param val Returned approximate rate
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if the configuration space does not contain a single value
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_rate)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_rate(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Extract minimum rate from a configuration space
 * \param params Configuration space
 * \param val Returned approximate minimum rate
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_rate_min)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_rate_min(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Extract maximum rate from a configuration space
 * \param params Configuration space
 * \param val Returned approximate maximum rate
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_rate_max)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_rate_max(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Verify if a rate is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate rate
 * \param dir Sub unit direction
 * \return 0 if available a negative error code otherwise
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_test_rate(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only one rate
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate rate
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_rate(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Restrict a configuration space with a minimum rate
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate minimum rate (on return filled with actual minimum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact minimum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_rate_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Restrict a configuration space with a maximum rate
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate maximum rate (on return filled with actual maximum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact maximum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_rate_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Restrict a configuration space to have rates in a given range
 * \param pcm PCM handle
 * \param params Configuration space
 * \param min approximate minimum rate (on return filled with actual minimum)
 * \param mindir Sub unit direction for minimum (on return filled with actual direction)
 * \param max approximate maximum rate (on return filled with actual maximum)
 * \param maxdir Sub unit direction for maximum (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact min/max is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_rate_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_RATE, min, mindir, max, maxdir);
}

/**
 * \brief Restrict a configuration space to have rate nearest to a target
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate target rate / returned approximate set rate
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space is empty
 *
 * target/chosen exact value is <,=,> val following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_rate_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_rate_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only its minimum rate
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned minimum approximate rate
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_rate_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_rate_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only its maximum rate
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned maximum approximate rate
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_rate_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_rate_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_RATE, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only real hardware rates
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val 0 = disable, 1 = enable (default) rate resampling
 * \return 0 otherwise a negative error code
 */
int snd_pcm_hw_params_set_rate_resample(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
{
	assert(pcm && params);
	if (!val)
		params->flags |= SND_PCM_HW_PARAMS_NORESAMPLE;
	else
		params->flags &= ~SND_PCM_HW_PARAMS_NORESAMPLE;
	params->rmask = ~0;
	return snd_pcm_hw_refine(pcm, params);
}

/**
 * \brief Extract resample state from a configuration space
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val 0 = disable, 1 = enable rate resampling
 * \return 0 otherwise a negative error code
 */
int snd_pcm_hw_params_get_rate_resample(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
{
	assert(pcm && params && val);
	*val = params->flags & SND_PCM_HW_PARAMS_NORESAMPLE ? 0 : 1;
	return 0;
}

/**
 * \brief Restrict a configuration space to allow the buffer to be accessible from outside
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val 0 = disable, 1 = enable (default) exporting buffer
 * \return 0 otherwise a negative error code
 */
int snd_pcm_hw_params_set_export_buffer(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
{
	assert(pcm && params);
	if (val)
		params->flags |= SND_PCM_HW_PARAMS_EXPORT_BUFFER;
	else
		params->flags &= ~SND_PCM_HW_PARAMS_EXPORT_BUFFER;
	params->rmask = ~0;
	return snd_pcm_hw_refine(pcm, params);
}

/**
 * \brief Extract buffer accessibility from a configuration space
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val 0 = disable, 1 = enable exporting buffer
 * \return 0 otherwise a negative error code
 */
int snd_pcm_hw_params_get_export_buffer(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
{
	assert(pcm && params && val);
	*val = params->flags & SND_PCM_HW_PARAMS_EXPORT_BUFFER ? 1 : 0;
	return 0;
}

/**
 * \brief Restrict a configuration space to settings without period wakeups
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val 0 = disable, 1 = enable (default) period wakeup
 * \return Zero on success, otherwise a negative error code.
 *
 * This function must be called only on devices where non-blocking mode is
 * enabled.
 *
 * To check whether the hardware does support disabling period wakeups, call
 * #snd_pcm_hw_params_can_disable_period_wakeup(). If the hardware does not
 * support this mode, standard period wakeups will be generated.
 *
 * Even with disabled period wakeups, the period size/time/count parameters
 * are valid; it is suggested to use #snd_pcm_hw_params_set_period_size_last().
 *
 * When period wakeups are disabled, the application must not use any functions
 * that could block on this device. The use of poll should be limited to error
 * cases. The application needs to use an external event or a timer to
 * check the state of the ring buffer and refill it apropriately.
 */
int snd_pcm_hw_params_set_period_wakeup(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
{
	assert(pcm && params);

	if (!val) {
		if (!(pcm->mode & SND_PCM_NONBLOCK))
			return -EINVAL;
		params->flags |= SND_PCM_HW_PARAMS_NO_PERIOD_WAKEUP;
	} else
		params->flags &= ~SND_PCM_HW_PARAMS_NO_PERIOD_WAKEUP;
	params->rmask = ~0;

	return snd_pcm_hw_refine(pcm, params);
}

/**
 * \brief Extract period wakeup flag from a configuration space
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val 0 = disabled, 1 = enabled period wakeups
 * \return Zero on success, otherwise a negative error code.
 */
int snd_pcm_hw_params_get_period_wakeup(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
{
	assert(pcm && params && val);
	*val = params->flags & SND_PCM_HW_PARAMS_NO_PERIOD_WAKEUP ? 0 : 1;
	return 0;
}

/**
 * \brief Restrict a configuration space to fill the end of playback stream with silence when drain() is invoked
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val 0 = disabled, 1 = enabled (default) fill the end of the playback stream with silence when drain() is invoked
 * \return Zero on success, otherwise a negative error code.
 *
 * When disabled, the application should handle the end of stream gracefully
 * (fill the silent samples to align to the period size plus some extra
 * samples for hardware / driver without perfect drain). Note that the rewind
 * may be used for this purpose or the sw_params silencing mechanism.
 */
int snd_pcm_hw_params_set_drain_silence(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
{
	assert(pcm && params);
	if (val)
		params->flags &= ~SND_PCM_HW_PARAMS_NO_DRAIN_SILENCE;
	else
		params->flags |= SND_PCM_HW_PARAMS_NO_DRAIN_SILENCE;
	params->rmask = ~0;
	return snd_pcm_hw_refine(pcm, params);
}

/**
 * \brief Extract drain with the filling of silence samples from a configuration space
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val 0 = disabled, 1 = enabled
 * \return 0 otherwise a negative error code
 */
int snd_pcm_hw_params_get_drain_silence(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
{
	assert(pcm && params && val);
	*val = params->flags & SND_PCM_HW_PARAMS_NO_DRAIN_SILENCE ? 0 : 1;
	return 0;
}

/**
 * \brief Extract period time from a configuration space
 * \param params Configuration space
 * \param val Returned approximate period duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if the configuration space does not contain a single value
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_period_time)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_period_time(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

/**
 * \brief Extract minimum period time from a configuration space
 * \param params Configuration space
 * \param val approximate minimum period duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_period_time_min)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_period_time_min(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

/**
 * \brief Extract maximum period time from a configuration space
 * \param params Configuration space
 * \param val approximate maximum period duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_period_time_max)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_period_time_max(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

/**
 * \brief Verify if a period time is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate period duration in us
 * \param dir Sub unit direction
 * \return 0 if available a negative error code otherwise
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_test_period_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only one period time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate period duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_period_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}


/**
 * \brief Restrict a configuration space with a minimum period time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate minimum period duration in us (on return filled with actual minimum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact minimum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_period_time_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space with a maximum period time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate maximum period duration in us (on return filled with actual maximum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact maximum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_period_time_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space to have period times in a given range
 * \param pcm PCM handle
 * \param params Configuration space
 * \param min approximate minimum period duration in us (on return filled with actual minimum)
 * \param mindir Sub unit direction for minimum (on return filled with actual direction)
 * \param max approximate maximum period duration in us (on return filled with actual maximum)
 * \param maxdir Sub unit direction for maximum (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact min/max is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_period_time_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_TIME, min, mindir, max, maxdir);
}

/**
 * \brief Restrict a configuration space to have period time nearest to a target
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate target period duration in us / returned chosen approximate target period duration
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space is empty
 *
 * target/chosen exact value is <,=,> val following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_period_time_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_period_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only its minimum period time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned approximate period duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_period_time_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_period_time_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only its maximum period time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned maximum approximate period time
 * \param dir Sub unit direction
 * \return approximate period duration in us
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_period_time_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_period_time_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}


/**
 * \brief Extract period size from a configuration space
 * \param params Configuration space
 * \param val Returned approximate period size in frames
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if the configuration space does not contain a single value
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_period_size)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#else
int snd_pcm_hw_params_get_period_size(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#endif
{
	unsigned int _val;
	int err = snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Extract minimum period size from a configuration space
 * \param params Configuration space
 * \param val approximate minimum period size in frames
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_period_size_min)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#else
int snd_pcm_hw_params_get_period_size_min(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#endif
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Extract maximum period size from a configuration space
 * \param params Configuration space
 * \param val approximate minimum period size in frames
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_period_size_max)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#else
int snd_pcm_hw_params_get_period_size_max(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#endif
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Verify if a period size is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate period size in frames
 * \param dir Sub unit direction
 * \return 0 if available a negative error code otherwise
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_test_period_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_PERIOD_SIZE, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only one period size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate period size in frames
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_period_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE, val, dir);
}

/**
 * \brief Restrict a configuration space with a minimum period size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate minimum period size in frames (on return filled with actual minimum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact minimum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_period_size_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Restrict a configuration space with a maximum period size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate maximum period size in frames (on return filled with actual maximum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact minimum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_period_size_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Restrict a configuration space to have period sizes in a given range
 * \param pcm PCM handle
 * \param params Configuration space
 * \param min approximate minimum period size in frames (on return filled with actual minimum)
 * \param mindir Sub unit direction for minimum (on return filled with actual direction)
 * \param max approximate maximum period size in frames (on return filled with actual maximum)
 * \param maxdir Sub unit direction for maximum (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact min/max is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_period_size_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *min, int *mindir, snd_pcm_uframes_t *max, int *maxdir)
{
	unsigned int _min = *min;
	unsigned int _max = *max;
	int err = snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE, &_min, mindir, &_max, maxdir);
	*min = _min;
	*max = _max;
	return err;
}

/**
 * \brief Restrict a configuration space to have period size nearest to a target
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate target period size in frames / returned chosen approximate target period size
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space is empty
 *
 * target/chosen exact value is <,=,> val following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_period_size_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#else
int snd_pcm_hw_params_set_period_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#endif
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Restrict a configuration space to contain only its minimum period size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned maximum approximate period size in frames
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_period_size_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#else
int snd_pcm_hw_params_set_period_size_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#endif
{
	unsigned int _val;
	int err = snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Restrict a configuration space to contain only its maximum period size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned maximum approximate period size in frames
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_period_size_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#else
int snd_pcm_hw_params_set_period_size_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
#endif
{
	unsigned int _val;
	int err = snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Restrict a configuration space to contain only integer period sizes
 * \param pcm PCM handle
 * \param params Configuration space
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_period_size_integer(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_set_integer(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE);
}


/**
 * \brief Extract periods from a configuration space
 * \param params Configuration space
 * \param val approximate periods per buffer
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if the configuration space does not contain a single value
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_periods)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_periods(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Extract minimum periods count from a configuration space
 * \param params Configuration space
 * \param val approximate minimum periods per buffer
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_periods_min)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_periods_min(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Extract maximum periods count from a configuration space
 * \param params Configuration space
 * \param val approximate maximum periods per buffer
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_periods_max)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_periods_max(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Verify if a periods count is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate periods per buffer
 * \param dir Sub unit direction
 * \return 0 if available a negative error code otherwise
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_test_periods(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only one periods count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate periods per buffer
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_periods(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Restrict a configuration space with a minimum periods count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate minimum periods per buffer (on return filled with actual minimum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact minimum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_periods_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Restrict a configuration space with a maximum periods count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate maximum periods per buffer (on return filled with actual maximum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact maximum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_periods_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Restrict a configuration space to have periods counts in a given range
 * \param pcm PCM handle
 * \param params Configuration space
 * \param min approximate minimum periods per buffer (on return filled with actual minimum)
 * \param mindir Sub unit direction for minimum (on return filled with actual direction)
 * \param max approximate maximum periods per buffer (on return filled with actual maximum)
 * \param maxdir Sub unit direction for maximum (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact min/max is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_periods_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS, min, mindir, max, maxdir);
}

/**
 * \brief Restrict a configuration space to have periods count nearest to a target
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate target periods per buffer / returned chosen approximate target periods per buffer
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space is empty
 *
 * target/chosen exact value is <,=,> val following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_periods_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_periods_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only its minimum periods count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned approximate minimum periods per buffer
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_periods_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_periods_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only its maximum periods count
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned approximate maximum periods per buffer
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_periods_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_periods_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only integer periods counts
 * \param pcm PCM handle
 * \param params Configuration space
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_periods_integer(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_set_integer(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS);
}


/**
 * \brief Extract buffer time from a configuration space
 * \param params Configuration space
 * \param val Returned buffer time in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if the configuration space does not contain a single value
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_buffer_time)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_buffer_time(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

/**
 * \brief Extract minimum buffer time from a configuration space
 * \param params Configuration space
 * \param val approximate minimum buffer duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_buffer_time_min)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_buffer_time_min(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

/**
 * \brief Extract maximum buffer time from a configuration space
 * \param params Configuration space
 * \param val approximate maximum buffer duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_buffer_time_max)(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_get_buffer_time_max(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

/**
 * \brief Verify if a buffer time is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate buffer duration in us
 * \param dir Sub unit direction
 * \return 0 if available a negative error code otherwise
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_test_buffer_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only one buffer time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate buffer duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_buffer_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space with a minimum buffer time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate minimum buffer duration in us (on return filled with actual minimum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact minimum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_buffer_time_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space with a maximum buffer time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate maximum buffer duration in us (on return filled with actual maximum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact maximum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_buffer_time_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space to have buffer times in a given range
 * \param pcm PCM handle
 * \param params Configuration space
 * \param min approximate minimum buffer duration in us (on return filled with actual minimum)
 * \param mindir Sub unit direction for minimum (on return filled with actual direction)
 * \param max approximate maximum buffer duration in us (on return filled with actual maximum)
 * \param maxdir Sub unit direction for maximum (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact min/max is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_buffer_time_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_TIME, min, mindir, max, maxdir);
}

/**
 * \brief Restrict a configuration space to have buffer time nearest to a target
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate target buffer duration in us / returned chosen approximate target buffer duration
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space is empty
 *
 * target/chosen exact value is <,=,> val following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_buffer_time_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_buffer_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only its minimum buffer time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned approximate minimum buffer duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_buffer_time_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_buffer_time_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

/**
 * \brief Restrict a configuration space to contain only its maximum buffered time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned approximate maximum buffer duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_buffer_time_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#else
int snd_pcm_hw_params_set_buffer_time_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}


/**
 * \brief Extract buffer size from a configuration space
 * \param params Configuration space
 * \param val Returned buffer size in frames
 * \return 0 otherwise a negative error code if the configuration space does not contain a single value
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_buffer_size)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_hw_params_get_buffer_size(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	unsigned int _val;
	int err = snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Extract minimum buffer size from a configuration space
 * \param params Configuration space
 * \param val Returned approximate minimum buffer size in frames
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_buffer_size_min)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_hw_params_get_buffer_size_min(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	unsigned int _val;
	int err = snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Extract maximum buffer size from a configuration space
 * \param params Configuration space
 * \param val Returned approximate maximum buffer size in frames
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_buffer_size_max)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_hw_params_get_buffer_size_max(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	unsigned int _val;
	int err = snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Verify if a buffer size is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val buffer size in frames
 * \return 0 if available a negative error code otherwise
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_test_buffer_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_BUFFER_SIZE, val, 0);
}

/**
 * \brief Restrict a configuration space to contain only one buffer size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val buffer size in frames
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_buffer_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_SIZE, val, 0);
}

/**
 * \brief Restrict a configuration space with a minimum buffer size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate minimum buffer size in frames (on return filled with actual minimum)
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_buffer_size_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Restrict a configuration space with a maximum buffer size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate maximum buffer size in frames (on return filled with actual maximum)
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_buffer_size_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Restrict a configuration space to have buffer sizes in a given range
 * \param pcm PCM handle
 * \param params Configuration space
 * \param min approximate minimum buffer size in frames (on return filled with actual minimum)
 * \param max approximate maximum buffer size in frames (on return filled with actual maximum)
 * \return 0 otherwise a negative error code if configuration space would become empty
 */
int snd_pcm_hw_params_set_buffer_size_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *min, snd_pcm_uframes_t *max)
{
	unsigned int _min = *min;
	unsigned int _max = *max;
	int err = snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_SIZE, &_min, NULL, &_max, NULL);
	*min = _min;
	*max = _max;
	return err;
}

/**
 * \brief Restrict a configuration space to have buffer size nearest to a target
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate target buffer size in frames / returned chosen approximate target buffer size in frames
 * \return 0 otherwise a negative error code if configuration space is empty
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_buffer_size_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_hw_params_set_buffer_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Restrict a configuration space to contain only its minimum buffer size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned minimum buffer size in frames
 * \return buffer size in frames
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_buffer_size_first)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_hw_params_set_buffer_size_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	unsigned int _val;
	int err = snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	if (err >= 0)
		*val = _val;
	return err;
}

/**
 * \brief Restrict a configuration space to contain only its maximum buffer size
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned maximum buffer size in frames
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_buffer_size_last)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_hw_params_set_buffer_size_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	unsigned int _val;
	int err = snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	if (err >= 0)
		*val = _val;
	return err;
}


/**
 * \brief (DEPRECATED) Extract tick time from a configuration space
 * \param params Configuration space
 * \param val Returned approximate tick duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if the configuration space does not contain a single value
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_tick_time)(const snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int *val, int *dir ATTRIBUTE_UNUSED)
#else
int snd_pcm_hw_params_get_tick_time(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	*val = 0;
	return 0;
}

/**
 * \brief (DEPRECATED) Extract minimum tick time from a configuration space
 * \param params Configuration space
 * \param val Returned approximate minimum tick duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_tick_time_min)(const snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int *val, int *dir ATTRIBUTE_UNUSED)
#else
int snd_pcm_hw_params_get_tick_time_min(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	*val = 0;
	return 0;
}

/**
 * \brief (DEPRECATED) Extract maximum tick time from a configuration space
 * \param params Configuration space
 * \param val Returned approximate maximum tick duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Exact value is <,=,> the returned one following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_get_tick_time_max)(const snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int *val, int *dir ATTRIBUTE_UNUSED)
#else
int snd_pcm_hw_params_get_tick_time_max(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	*val = 0;
	return 0;
}

/**
 * \brief (DEPRECATED) Verify if a tick time is available inside a configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate tick duration in us
 * \param dir Sub unit direction
 * \return 0 if available a negative error code otherwise
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_test_tick_time(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int val, int dir ATTRIBUTE_UNUSED)
{
	return val ? -EINVAL : 0;
}

/**
 * \brief (DEPRECATED) Restrict a configuration space to contain only one tick time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate tick duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted exact value is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_tick_time(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int val ATTRIBUTE_UNUSED, int dir ATTRIBUTE_UNUSED)
{
	return 0;
}

/**
 * \brief (DEPRECATED) Restrict a configuration space with a minimum tick time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate minimum tick duration in us (on return filled with actual minimum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact minimum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_tick_time_min(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int *val ATTRIBUTE_UNUSED, int *dir ATTRIBUTE_UNUSED)
{
	return 0;
}

/**
 * \brief (DEPRECATED) Restrict a configuration space with a maximum tick time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate maximum tick duration in us (on return filled with actual maximum)
 * \param dir Sub unit direction (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact maximum is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_tick_time_max(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int *val ATTRIBUTE_UNUSED, int *dir ATTRIBUTE_UNUSED)
{
	return 0;
}

/**
 * \brief (DEPRECATED) Restrict a configuration space to have tick times in a given range
 * \param pcm PCM handle
 * \param params Configuration space
 * \param min approximate minimum tick duration in us (on return filled with actual minimum)
 * \param mindir Sub unit direction for minimum (on return filled with actual direction)
 * \param max approximate maximum tick duration in us (on return filled with actual maximum)
 * \param maxdir Sub unit direction for maximum (on return filled with actual direction)
 * \return 0 otherwise a negative error code if configuration space would become empty
 *
 * Wanted/actual exact min/max is <,=,> val following dir (-1,0,1)
 */
int snd_pcm_hw_params_set_tick_time_minmax(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int *min ATTRIBUTE_UNUSED, int *mindir ATTRIBUTE_UNUSED, unsigned int *max ATTRIBUTE_UNUSED, int *maxdir ATTRIBUTE_UNUSED)
{
	return 0;
}

/**
 * \brief (DEPRECATED) Restrict a configuration space to have tick time nearest to a target
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val approximate target tick duration in us / returned chosen approximate target tick duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code if configuration space is empty
 *
 * target/chosen exact value is <,=,> val following dir (-1,0,1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_tick_time_near)(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int *val ATTRIBUTE_UNUSED, int *dir ATTRIBUTE_UNUSED)
#else
int snd_pcm_hw_params_set_tick_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return 0;
}

/**
 * \brief (DEPRECATED) Restrict a configuration space to contain only its minimum tick time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned approximate minimum tick duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_tick_time_first)(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int *val ATTRIBUTE_UNUSED, int *dir ATTRIBUTE_UNUSED)
#else
int snd_pcm_hw_params_set_tick_time_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return 0;
}

/**
 * \brief (DEPRECATED) Restrict a configuration space to contain only its maximum tick time
 * \param pcm PCM handle
 * \param params Configuration space
 * \param val Returned approximate maximum tick duration in us
 * \param dir Sub unit direction
 * \return 0 otherwise a negative error code
 *
 * Actual exact value is <,=,> the approximate one following dir (-1, 0, 1)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_hw_params_set_tick_time_last)(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params ATTRIBUTE_UNUSED, unsigned int *val ATTRIBUTE_UNUSED, int *dir ATTRIBUTE_UNUSED)
#else
int snd_pcm_hw_params_set_tick_time_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
#endif
{
	return 0;
}

/**
 * \brief Get the minimum transfer align value in samples
 * \param params Configuration space
 * \param val Returned minimum align value
 * \return 0 otherwise a negative error code if the configuration space does not contain a single value
 */
int snd_pcm_hw_params_get_min_align(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
	unsigned int format, channels, fb, min_align;
	int err;

	err = snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_FORMAT, &format, NULL);
	if (err < 0)
		return err;
	err = snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_CHANNELS, &channels, NULL);
	if (err < 0)
		return err;
	// compute frame bits
	fb = snd_pcm_format_physical_width((snd_pcm_format_t)format) * channels;
        min_align = 1;
	while (fb % 8) {
		fb *= 2;
                min_align *= 2;
	}
	if (val)
		*val = min_align;
	return 0;
}

#ifndef DOXYGEN
void snd_pcm_sw_params_current_no_lock(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	params->proto = SNDRV_PCM_VERSION;
	params->tstamp_mode = pcm->tstamp_mode;
	params->tstamp_type = pcm->tstamp_type;
	params->period_step = pcm->period_step;
	params->sleep_min = 0;
	params->avail_min = pcm->avail_min;
	sw_set_period_event(params, pcm->period_event);
	params->xfer_align = 1;
	params->start_threshold = pcm->start_threshold;
	params->stop_threshold = pcm->stop_threshold;
	params->silence_threshold = pcm->silence_threshold;
	params->silence_size = pcm->silence_size;
	params->boundary = pcm->boundary;
}
#endif

/**
 * \brief Return current software configuration for a PCM
 * \param pcm PCM handle
 * \param params Software configuration container
 * \return 0 on success otherwise a negative error code
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_sw_params_current(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	assert(pcm && params);
	if (CHECK_SANITY(! pcm->setup)) {
		SNDMSG("PCM not set up");
		return -EIO;
	}
	__snd_pcm_lock(pcm); /* forced lock due to pcm field changes */
	snd_pcm_sw_params_current_no_lock(pcm, params);
	__snd_pcm_unlock(pcm);
	return 0;
}

/**
 * \brief Dump a software configuration
 * \param params Software configuration container
 * \param out Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_sw_params_dump(snd_pcm_sw_params_t *params, snd_output_t *out)
{
	snd_output_printf(out, "tstamp_mode: %s\n", snd_pcm_tstamp_mode_name(params->tstamp_mode));
	snd_output_printf(out, "tstamp_type: %s\n", snd_pcm_tstamp_type_name(params->tstamp_type));
	snd_output_printf(out, "period_step: %u\n", params->period_step);
	snd_output_printf(out, "avail_min: %lu\n", params->avail_min);
	snd_output_printf(out, "start_threshold: %ld\n", params->start_threshold);
	snd_output_printf(out, "stop_threshold: %ld\n", params->stop_threshold);
	snd_output_printf(out, "silence_threshold: %lu\n", params->silence_threshold);
	snd_output_printf(out, "silence_size: %lu\n", params->silence_size);
	snd_output_printf(out, "boundary: %lu\n", params->boundary);
	return 0;
}

/**
 * \brief get size of #snd_pcm_sw_params_t
 * \return size in bytes
 */
size_t snd_pcm_sw_params_sizeof()
{
	return sizeof(snd_pcm_sw_params_t);
}

/**
 * \brief allocate an invalid #snd_pcm_sw_params_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_pcm_sw_params_malloc(snd_pcm_sw_params_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_sw_params_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_pcm_sw_params_t
 * \param obj pointer to object to free
 */
void snd_pcm_sw_params_free(snd_pcm_sw_params_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_pcm_sw_params_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_pcm_sw_params_copy(snd_pcm_sw_params_t *dst, const snd_pcm_sw_params_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief Get boundary for ring pointers from a software configuration container
 * \param params Software configuration container
 * \param val Returned boundary in frames
 * \return 0 otherwise a negative error code
 */
int snd_pcm_sw_params_get_boundary(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
{
	assert(params);
	*val = params->boundary;
	return 0;
}

/**
 * \brief (DEPRECATED) Set start mode inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Start mode
 * \return 0 otherwise a negative error code
 */
int snd_pcm_sw_params_set_start_mode(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_start_t val)
{
	assert(pcm && params);
	switch (val) {
	case SND_PCM_START_DATA:
		params->start_threshold = 1;
		break;
	case SND_PCM_START_EXPLICIT:
		params->start_threshold = pcm->boundary;
		break;
	default:
		SNDMSG("invalid start mode value %d", val);
		return -EINVAL;
	}
	return 0;
}

#ifndef DOC_HIDDEN
link_warning(snd_pcm_sw_params_set_start_mode, "Warning: start_mode is deprecated, consider to use start_threshold");
#endif

/**
 * \brief (DEPRECATED) Get start mode from a software configuration container
 * \param params Software configuration container
 * \return start mode
 */
snd_pcm_start_t snd_pcm_sw_params_get_start_mode(const snd_pcm_sw_params_t *params)
{
	assert(params);
	/* FIXME: Ugly */
	return params->start_threshold > 1024 * 1024 ? SND_PCM_START_EXPLICIT : SND_PCM_START_DATA;
}

#ifndef DOC_HIDDEN
link_warning(snd_pcm_sw_params_get_start_mode, "Warning: start_mode is deprecated, consider to use start_threshold");
#endif

/**
 * \brief (DEPRECATED) Set xrun mode inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Xrun mode
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
int snd_pcm_sw_params_set_xrun_mode(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_xrun_t val)
#else
int snd_pcm_sw_params_set_xrun_mode(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_xrun_t val)
#endif
{
	assert(pcm && params);
	switch (val) {
	case SND_PCM_XRUN_STOP:
		params->stop_threshold = pcm->buffer_size;
		break;
	case SND_PCM_XRUN_NONE:
		params->stop_threshold = pcm->boundary;
		break;
	default:
		SNDMSG("invalid xrun mode value %d", val);
		return -EINVAL;
	}
	return 0;
}

#ifndef DOC_HIDDEN
link_warning(snd_pcm_sw_params_set_xrun_mode, "Warning: xrun_mode is deprecated, consider to use stop_threshold");
#endif

/**
 * \brief (DEPRECATED) Get xrun mode from a software configuration container
 * \param params Software configuration container
 * \return xrun mode
 */
snd_pcm_xrun_t snd_pcm_sw_params_get_xrun_mode(const snd_pcm_sw_params_t *params)
{
	assert(params);
	/* FIXME: Ugly */
	return params->stop_threshold > 1024 * 1024 ? SND_PCM_XRUN_NONE : SND_PCM_XRUN_STOP;
}

#ifndef DOC_HIDDEN
link_warning(snd_pcm_sw_params_get_xrun_mode, "Warning: xrun_mode is deprecated, consider to use stop_threshold");
#endif

/**
 * \brief Set timestamp mode inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Timestamp mode
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
int snd_pcm_sw_params_set_tstamp_mode(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_tstamp_t val)
#else
int snd_pcm_sw_params_set_tstamp_mode(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_tstamp_t val)
#endif
{
	assert(pcm && params);
	if (CHECK_SANITY(val > SND_PCM_TSTAMP_LAST)) {
		SNDMSG("invalid tstamp_mode value %d", val);
		return -EINVAL;
	}
	params->tstamp_mode = val;
	return 0;
}

/**
 * \brief Get timestamp mode from a software configuration container
 * \param params Software configuration container
 * \param val Returned timestamp
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_sw_params_get_tstamp_mode)(const snd_pcm_sw_params_t *params, snd_pcm_tstamp_t *val)
#else
int snd_pcm_sw_params_get_tstamp_mode(const snd_pcm_sw_params_t *params, snd_pcm_tstamp_t *val)
#endif
{
	assert(params && val);
	*val = params->tstamp_mode;
	return 0;
}

/**
 * \brief Set timestamp type inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Timestamp type
 * \return 0 otherwise a negative error code
 */
int snd_pcm_sw_params_set_tstamp_type(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_tstamp_type_t val)
{
	assert(pcm && params);
	if (CHECK_SANITY(val > SND_PCM_TSTAMP_TYPE_LAST)) {
		SNDMSG("invalid tstamp_type value %d", val);
		return -EINVAL;
	}
	params->tstamp_type = val;
	return 0;
}

/**
 * \brief Get timestamp type from a software configuration container
 * \param params Software configuration container
 * \param val Returned timestamp type
 * \return 0 otherwise a negative error code
 */
int snd_pcm_sw_params_get_tstamp_type(const snd_pcm_sw_params_t *params, snd_pcm_tstamp_type_t *val)
{
	assert(params && val);
	*val = params->tstamp_type;
	return 0;
}

/**
 * \brief (DEPRECATED) Set minimum number of ticks to sleep inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Minimum ticks to sleep or 0 to disable the use of tick timer
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
int snd_pcm_sw_params_set_sleep_min(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params ATTRIBUTE_UNUSED, unsigned int val ATTRIBUTE_UNUSED)
#else
int snd_pcm_sw_params_set_sleep_min(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, unsigned int val)
#endif
{
	return 0;
}

/**
 * \brief (DEPRECATED) Get minimum numbers of ticks to sleep from a software configuration container
 * \param params Software configuration container
 * \param val returned minimum number of ticks to sleep or 0 if tick timer is disabled
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_sw_params_get_sleep_min)(const snd_pcm_sw_params_t *params ATTRIBUTE_UNUSED, unsigned int *val)
#else
int snd_pcm_sw_params_get_sleep_min(const snd_pcm_sw_params_t *params, unsigned int *val)
#endif
{
	*val = 0;
	return 0;
}

/**
 * \brief Set avail min inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Minimum avail frames to consider PCM ready
 * \return 0 otherwise a negative error code
 *
 * Note: This is similar to setting an OSS wakeup point.  The valid
 * values for 'val' are determined by the specific hardware.  Most PC
 * sound cards can only accept power of 2 frame counts (i.e. 512,
 * 1024, 2048).  You cannot use this as a high resolution timer - it
 * is limited to how often the sound card hardware raises an
 * interrupt.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int snd_pcm_sw_params_set_avail_min(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#else
int snd_pcm_sw_params_set_avail_min(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#endif
{
	assert(pcm && params);
	/* Fix avail_min if it's below period size.  The period_size
	 * defines the minimal wake-up timing accuracy, so it doesn't
	 * make sense to set below that.
	 */
	if (val < pcm->period_size)
		val = pcm->period_size;
	params->avail_min = val;
	return 0;
}

/**
 * \brief Get avail min from a software configuration container
 * \param params Software configuration container
 * \param val returned minimum available frames to consider PCM ready
 * \return 0 otherwise a negative error code
 *
 * This is a threshold value when the PCM stream is considered as ready for
 * another read/write operation or poll event.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_sw_params_get_avail_min)(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_sw_params_get_avail_min(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	assert(params && val);
	*val = params->avail_min;
	return 0;
}

/**
 * \brief Set period event inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val 0 = disable period event, 1 = enable period event
 * \return 0 otherwise a negative error code
 *
 * An poll (select) wakeup event is raised if enabled.
 */
int snd_pcm_sw_params_set_period_event(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, int val)
{
	assert(pcm && params);
	sw_set_period_event(params, val);
	return 0;
}

/**
 * \brief Get period event from a software configuration container
 * \param params Software configuration container
 * \param val returned period event state
 * \return 0 otherwise a negative error code
 */
int snd_pcm_sw_params_get_period_event(const snd_pcm_sw_params_t *params, int *val)
{
	assert(params && val);
	*val = sw_get_period_event(params);
	return 0;
}

/**
 * \brief (DEPRECATED) Set xfer align inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Chunk size (frames are attempted to be transferred in chunks)
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int snd_pcm_sw_params_set_xfer_align(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params ATTRIBUTE_UNUSED, snd_pcm_uframes_t val ATTRIBUTE_UNUSED)
#else
int snd_pcm_sw_params_set_xfer_align(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#endif
{
	return 0;
}

/**
 * \brief (DEPRECATED) Get xfer align from a software configuration container
 * \param params Software configuration container
 * \param val returned chunk size (frames are attempted to be transferred in chunks)
 * \return 0 otherwise a negative error code
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_sw_params_get_xfer_align)(const snd_pcm_sw_params_t *params ATTRIBUTE_UNUSED, snd_pcm_uframes_t *val)
#else
int snd_pcm_sw_params_get_xfer_align(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	*val = 1;
	return 0;
}

/**
 * \brief Set start threshold inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Start threshold in frames
 * \return 0 otherwise a negative error code
 *
 * PCM is automatically started when playback frames available to PCM 
 * are >= threshold or when requested capture frames are >= threshold
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int snd_pcm_sw_params_set_start_threshold(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#else
int snd_pcm_sw_params_set_start_threshold(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#endif
{
	assert(pcm && params);
	params->start_threshold = val;
	return 0;
}

/**
 * \brief Get start threshold from a software configuration container
 * \param params Software configuration container
 * \param val Returned start threshold in frames
 * \return 0 otherwise a negative error code
 *
 * PCM is automatically started when playback frames available to PCM 
 * are >= threshold or when requested capture frames are >= threshold
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_sw_params_get_start_threshold)(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_sw_params_get_start_threshold(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	assert(params);
	*val = params->start_threshold;
	return 0;
}


/**
 * \brief Set stop threshold inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Stop threshold in frames
 * \return 0 otherwise a negative error code
 *
 * PCM is automatically stopped in #SND_PCM_STATE_XRUN state when available
 * frames is >= threshold. If the stop threshold is equal to boundary (also
 * software parameter - sw_param) then automatic stop will be disabled
 * (thus device will do the endless loop in the ring buffer).
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int snd_pcm_sw_params_set_stop_threshold(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#else
int snd_pcm_sw_params_set_stop_threshold(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#endif
{
	assert(pcm && params);
	params->stop_threshold = val;
	return 0;
}

/**
 * \brief Get stop threshold from a software configuration container
 * \param params Software configuration container
 * \param val Returned stop threshold in frames
 * \return 0 otherwise a negative error code
 *
 * PCM is automatically stopped in #SND_PCM_STATE_XRUN state when available
 * frames is >= threshold. If the stop threshold is equal to boundary (also
 * software parameter - sw_param) then automatic stop will be disabled
 * (thus device will do the endless loop in the ring buffer).
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_sw_params_get_stop_threshold)(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_sw_params_get_stop_threshold(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	assert(params);
	*val = params->stop_threshold;
	return 0;
}


/**
 * \brief Set silence threshold inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Silence threshold in frames 
 * \return 0 otherwise a negative error code
 *
 * A portion of playback buffer is overwritten with silence (see 
 * #snd_pcm_sw_params_set_silence_size) when playback underrun is nearer
 * than silence threshold.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int snd_pcm_sw_params_set_silence_threshold(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#else
int snd_pcm_sw_params_set_silence_threshold(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#endif
{
	assert(pcm && params);
	if (CHECK_SANITY(val >= pcm->buffer_size)) {
		SNDMSG("invalid silent_threshold value %ld (buffer_size = %ld)",
		       val, pcm->buffer_size);
		return -EINVAL;
	}
	params->silence_threshold = val;
	return 0;
}

/**
 * \brief Get silence threshold from a software configuration container
 * \param params Software configuration container
 * \param val Returned silence threshold in frames
 * \return 0 otherwise a negative error value
 *
 * A portion of playback buffer is overwritten with silence (see 
 * #snd_pcm_sw_params_set_silence_size) when playback underrun is nearer
 * than silence threshold.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_sw_params_get_silence_threshold)(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_sw_params_get_silence_threshold(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	assert(params && val);
	*val = params->silence_threshold;
	return 0;
}


/**
 * \brief Set silence size inside a software configuration container
 * \param pcm PCM handle
 * \param params Software configuration container
 * \param val Silence size in frames (0 for disabled)
 * \return 0 otherwise a negative error code
 *
 * A portion of playback buffer is overwritten with silence when playback
 * underrun is nearer than silence threshold (see 
 * #snd_pcm_sw_params_set_silence_threshold)
 *
 * When drain silence (see #snd_pcm_hw_params_get_drain_silence) is disabled,
 * this will also apply for draining, i.e. silence is written also when the
 * drain end is nearer than the silence threshold.
 *
 * The special case is when silence size value is equal or greater than
 * boundary. The unused portion of the ring buffer (initial written samples
 * are untouched) is filled with silence at start. Later, only just processed
 * sample area is filled with silence. Note: silence_threshold must be set to zero.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int snd_pcm_sw_params_set_silence_size(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#else
int snd_pcm_sw_params_set_silence_size(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
#endif
{
	assert(pcm && params);
	if (CHECK_SANITY(val < pcm->boundary && val > pcm->buffer_size)) {
		SNDMSG("invalid silence_size %ld (boundary %ld, buffer_size %ld)",
		       val, pcm->boundary, pcm->buffer_size);
		return -EINVAL;
	}
	params->silence_size = val;
	return 0;
}

/**
 * \brief Get silence size from a software configuration container
 * \param params Software configuration container
 * \param val Returned silence size in frames (0 for disabled)
 * \return 0 otherwise a negative error code
 *
 * A portion of playback buffer is overwritten with silence when playback
 * underrun is nearer than silence threshold (see 
 * #snd_pcm_sw_params_set_silence_threshold)
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_pcm_sw_params_get_silence_size)(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#else
int snd_pcm_sw_params_get_silence_size(const snd_pcm_sw_params_t *params, snd_pcm_uframes_t *val)
#endif
{
	assert(params);
	*val = params->silence_size;
	return 0;
}


/**
 * \brief get size of #snd_pcm_status_t
 * \return size in bytes
 */
size_t snd_pcm_status_sizeof()
{
	return sizeof(snd_pcm_status_t);
}

/**
 * \brief allocate an invalid #snd_pcm_status_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_pcm_status_malloc(snd_pcm_status_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_status_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_pcm_status_t
 * \param obj pointer to object to free
 */
void snd_pcm_status_free(snd_pcm_status_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_pcm_status_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_pcm_status_copy(snd_pcm_status_t *dst, const snd_pcm_status_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/** 
 * \brief Get state from a PCM status container (see #snd_pcm_state)
 * \param obj #snd_pcm_status_t pointer
 * \return PCM state
 */
snd_pcm_state_t snd_pcm_status_get_state(const snd_pcm_status_t *obj)
{
	assert(obj);
	return obj->state;
}

/** 
 * \brief Get trigger timestamp from a PCM status container
 * \param obj #snd_pcm_status_t pointer
 * \param ptr Pointer to returned timestamp
 *
 * Trigger means a PCM state transition (from stopped to running or
 * versa vice). It applies also to pause and suspend. In other words,
 * timestamp contains time when stream started or when it was stopped.
 */
void snd_pcm_status_get_trigger_tstamp(const snd_pcm_status_t *obj, snd_timestamp_t *ptr)
{
	assert(obj && ptr);
	ptr->tv_sec = obj->trigger_tstamp.tv_sec;
	ptr->tv_usec = obj->trigger_tstamp.tv_nsec / 1000L;
}

/** 
 * \brief Get trigger hi-res timestamp from a PCM status container
 * \param obj #snd_pcm_status_t pointer
 * \param ptr Pointer to returned timestamp
 *
 * Trigger means a PCM state transition (from stopped to running or
 * versa vice). It applies also to pause and suspend. In other words,
 * timestamp contains time when stream started or when it was stopped.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL void INTERNAL(snd_pcm_status_get_trigger_htstamp)(const snd_pcm_status_t *obj, snd_htimestamp_t *ptr)
#else
void snd_pcm_status_get_trigger_htstamp(const snd_pcm_status_t *obj, snd_htimestamp_t *ptr)
#endif
{
	assert(obj && ptr);
	*ptr = obj->trigger_tstamp;
}
use_default_symbol_version(__snd_pcm_status_get_trigger_htstamp, snd_pcm_status_get_trigger_htstamp, ALSA_0.9.0rc8);

/** 
 * \brief Get "now" timestamp from a PCM status container
 * \param obj #snd_pcm_status_t pointer
 * \param ptr Pointer to returned timestamp
 */
void snd_pcm_status_get_tstamp(const snd_pcm_status_t *obj, snd_timestamp_t *ptr)
{
	assert(obj && ptr);
	ptr->tv_sec = obj->tstamp.tv_sec;
	ptr->tv_usec = obj->tstamp.tv_nsec / 1000L;
}

/** 
 * \brief Get "now" hi-res timestamp from a PCM status container
 * \param obj pointer to #snd_pcm_status_t
 * \param ptr Pointer to returned timestamp
 */
#ifndef DOXYGEN
EXPORT_SYMBOL void INTERNAL(snd_pcm_status_get_htstamp)(const snd_pcm_status_t *obj, snd_htimestamp_t *ptr)
#else
void snd_pcm_status_get_htstamp(const snd_pcm_status_t *obj, snd_htimestamp_t *ptr)
#endif
{
	assert(obj && ptr);
	*ptr = obj->tstamp;
}
use_default_symbol_version(__snd_pcm_status_get_htstamp, snd_pcm_status_get_htstamp, ALSA_0.9.0rc8);

/** 
 * \brief Get "now" hi-res audio timestamp from a PCM status container
 * \param obj pointer to #snd_pcm_status_t
 * \param ptr Pointer to returned timestamp
 */
void snd_pcm_status_get_audio_htstamp(const snd_pcm_status_t *obj, snd_htimestamp_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->audio_tstamp;
}

/**
 * \brief Get "now" hi-res driver timestamp from a PCM status container. Defines when the status
 * was generated by driver, may differ from normal timestamp.
 * \param obj pointer to #snd_pcm_status_t
 * \param ptr Pointer to returned timestamp
 */
void snd_pcm_status_get_driver_htstamp(const snd_pcm_status_t *obj, snd_htimestamp_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->driver_tstamp;
}

/**
 * \brief Get audio_tstamp_report from a PCM status container
 * \param obj pointer to #snd_pcm_status_t
 * \param audio_tstamp_report Pointer to returned report
 */
void snd_pcm_status_get_audio_htstamp_report(const snd_pcm_status_t *obj,
					     snd_pcm_audio_tstamp_report_t *audio_tstamp_report)
{
	assert(obj && audio_tstamp_report);
	snd_pcm_unpack_audio_tstamp_report(obj->audio_tstamp_data,
					obj->audio_tstamp_accuracy,
					audio_tstamp_report);
}

/**
 * \brief set audio_tstamp_config from a PCM status container
 * \param obj pointer to #snd_pcm_status_t
 * \param audio_tstamp_config Pointer to config (valid fields are type_requested and report_delay)
 */
void snd_pcm_status_set_audio_htstamp_config(snd_pcm_status_t *obj,
					     snd_pcm_audio_tstamp_config_t *audio_tstamp_config)
{
	assert(obj && audio_tstamp_config);
	snd_pcm_pack_audio_tstamp_config(&obj->audio_tstamp_data, audio_tstamp_config);
}

/**
 * \brief Get delay from a PCM status container (see #snd_pcm_delay)
 * \return Delay in frames
 *
 * Delay is distance between current application frame position and
 * sound frame position.
 * It's positive and less than buffer size in normal situation,
 * negative on playback underrun and greater than buffer size on
 * capture overrun.
 */
snd_pcm_sframes_t snd_pcm_status_get_delay(const snd_pcm_status_t *obj)
{
	assert(obj);
	return obj->delay;
}

/** 
 * \brief Get number of frames available from a PCM status container (see #snd_pcm_avail_update)
 * \return Number of frames ready to be read/written
 */
snd_pcm_uframes_t snd_pcm_status_get_avail(const snd_pcm_status_t *obj)
{
	assert(obj);
	return obj->avail;
}

/** 
 * \brief Get maximum number of frames available from a PCM status container after last #snd_pcm_status call
 * \return Maximum number of frames ready to be read/written
 *
 * This value returns the peak for the available frames between #snd_pcm_status calls.
 */
snd_pcm_uframes_t snd_pcm_status_get_avail_max(const snd_pcm_status_t *obj)
{
	assert(obj);
	return obj->avail_max;
}

/** 
 * \brief Get count of ADC overrange detections since last call
 * \return Count of ADC overrange detections
 */
snd_pcm_uframes_t snd_pcm_status_get_overrange(const snd_pcm_status_t *obj)
{
	assert(obj);
	return obj->overrange;
}

/**
 * \brief get size of #snd_pcm_info_t
 * \return size in bytes
 */
size_t snd_pcm_info_sizeof()
{
	return sizeof(snd_pcm_info_t);
}

/**
 * \brief allocate an invalid #snd_pcm_info_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_pcm_info_malloc(snd_pcm_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_pcm_info_t
 * \param obj pointer to object to free
 */
void snd_pcm_info_free(snd_pcm_info_t *obj)
{
	free(obj);
}

/**
 * \brief copy one #snd_pcm_info_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_pcm_info_copy(snd_pcm_info_t *dst, const snd_pcm_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief Get device from a PCM info container
 * \param obj PCM info container
 * \return device number
 */
unsigned int snd_pcm_info_get_device(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->device;
}

/**
 * \brief Get subdevice from a PCM info container
 * \param obj PCM info container
 * \return subdevice number
 */
unsigned int snd_pcm_info_get_subdevice(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->subdevice;
}

/**
 * \brief Get stream (direction) from a PCM info container
 * \param obj PCM info container
 * \return stream
 */
snd_pcm_stream_t snd_pcm_info_get_stream(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->stream;
}

/**
 * \brief Get card from a PCM info container
 * \param obj PCM info container
 * \return card number otherwise a negative error code if not associable to a card
 */
int snd_pcm_info_get_card(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->card;
}

/**
 * \brief Get id from a PCM info container
 * \param obj PCM info container
 * \return short id of PCM
 */
const char *snd_pcm_info_get_id(const snd_pcm_info_t *obj)
{
	assert(obj);
	return (const char *)obj->id;
}

/**
 * \brief Get name from a PCM info container
 * \param obj PCM info container
 * \return name of PCM
 */
const char *snd_pcm_info_get_name(const snd_pcm_info_t *obj)
{
	assert(obj);
	return (const char *)obj->name;
}

/**
 * \brief Get subdevice name from a PCM info container
 * \param obj PCM info container
 * \return name of used PCM subdevice
 */
const char *snd_pcm_info_get_subdevice_name(const snd_pcm_info_t *obj)
{
	assert(obj);
	return (const char *)obj->subname;
}

/**
 * \brief Get class from a PCM info container
 * \param obj PCM info container
 * \return class of PCM
 */
snd_pcm_class_t snd_pcm_info_get_class(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->dev_class;
}

/**
 * \brief Get subclass from a PCM info container
 * \param obj PCM info container
 * \return subclass of PCM
 */
snd_pcm_subclass_t snd_pcm_info_get_subclass(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->dev_subclass;
}

/**
 * \brief Get subdevices count from a PCM info container
 * \param obj PCM info container
 * \return subdevices total count of PCM
 */
unsigned int snd_pcm_info_get_subdevices_count(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->subdevices_count;
}

/**
 * \brief Get available subdevices count from a PCM info container
 * \param obj PCM info container
 * \return available subdevices count of PCM
 */
unsigned int snd_pcm_info_get_subdevices_avail(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->subdevices_avail;
}

/**
 * \brief (DEPRECATED) Get hardware synchronization ID from a PCM info container
 * \param obj PCM info container
 * \return hardware synchronization ID
 */
snd_pcm_sync_id_t snd_pcm_info_get_sync(const snd_pcm_info_t *obj)
{
	snd_pcm_sync_id_t res;
	assert(obj);
	bzero(&res, sizeof(res));
	return res;
}
#ifndef DOC_HIDDEN
link_warning(snd_pcm_info_get_sync, "Warning: snd_pcm_info_get_sync is deprecated, consider to use snd_pcm_hw_params_get_sync");
#endif

/**
 * \brief Set wanted device inside a PCM info container (see #snd_ctl_pcm_info)
 * \param obj PCM info container
 * \param val Device number
 */
void snd_pcm_info_set_device(snd_pcm_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->device = val;
}

/**
 * \brief Set wanted subdevice inside a PCM info container (see #snd_ctl_pcm_info)
 * \param obj PCM info container
 * \param val Subdevice number
 */
void snd_pcm_info_set_subdevice(snd_pcm_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->subdevice = val;
}

/**
 * \brief Set wanted stream inside a PCM info container (see #snd_ctl_pcm_info)
 * \param obj PCM info container
 * \param val Stream
 */
void snd_pcm_info_set_stream(snd_pcm_info_t *obj, snd_pcm_stream_t val)
{
	assert(obj);
	obj->stream = val;
}

/**
 * \brief Application request to access a portion of direct (mmap) area
 * \param pcm PCM handle 
 * \param areas Returned mmap channel areas
 * \param offset Returned mmap area offset in area steps (== frames)
 * \param frames mmap area portion size in frames (wanted on entry, contiguous available on exit)
 * \return 0 on success otherwise a negative error code
 *
 * It is necessary to call the snd_pcm_avail_update() function directly before
 * this call. Otherwise, this function can return a wrong count of available frames.
 *
 * The function should be called before a sample-direct area can be accessed.
 * The resulting size parameter is always less or equal to the input count of frames
 * and can be zero, if no frames can be processed (the ring buffer is full).
 *
 * See the snd_pcm_mmap_commit() function to finish the frame processing in
 * the direct areas.
 *
 * The function is thread-safe when built with the proper option.
 */
int snd_pcm_mmap_begin(snd_pcm_t *pcm,
		       const snd_pcm_channel_area_t **areas,
		       snd_pcm_uframes_t *offset,
		       snd_pcm_uframes_t *frames)
{
	int err;

	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	err = __snd_pcm_mmap_begin(pcm, areas, offset, frames);
	snd_pcm_unlock(pcm->fast_op_arg);
	return err;
}

#ifndef DOC_HIDDEN
int __snd_pcm_mmap_begin_generic(snd_pcm_t *pcm, const snd_pcm_channel_area_t **areas,
				 snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames)
{
	snd_pcm_uframes_t cont;
	snd_pcm_uframes_t f;
	snd_pcm_uframes_t avail;
	const snd_pcm_channel_area_t *xareas;

	assert(pcm && areas && offset && frames);

	/* fallback for plugins that do not specify new callback */
	xareas = snd_pcm_mmap_areas(pcm);
	if (xareas == NULL)
		return -EBADFD;
	*areas = xareas;
	*offset = *pcm->appl.ptr % pcm->buffer_size;
	avail = snd_pcm_mmap_avail(pcm);
	if (avail > pcm->buffer_size)
		avail = pcm->buffer_size;
	cont = pcm->buffer_size - *offset;
	f = *frames;
	if (f > avail)
		f = avail;
	if (f > cont)
		f = cont;
	*frames = f;
	return 0;
}

/* locked version */
int __snd_pcm_mmap_begin(snd_pcm_t *pcm, const snd_pcm_channel_area_t **areas,
			 snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames)
{
	assert(pcm && areas && offset && frames);

	if (pcm->fast_ops->mmap_begin)
		return pcm->fast_ops->mmap_begin(pcm->fast_op_arg, areas, offset, frames);

	return __snd_pcm_mmap_begin_generic(pcm, areas, offset, frames);
}
#endif

/**
 * \brief Application has completed the access to area requested with #snd_pcm_mmap_begin
 * \param pcm PCM handle
 * \param offset area offset in area steps (== frames)
 * \param frames area portion size in frames
 * \return count of transferred frames otherwise a negative error code
 *
 * You should pass this function the offset value that
 * snd_pcm_mmap_begin() returned. The frames parameter should hold the
 * number of frames you have written or read to/from the audio
 * buffer. The frames parameter must never exceed the contiguous frames
 * count that snd_pcm_mmap_begin() returned. Each call to snd_pcm_mmap_begin()
 * must be followed by a call to snd_pcm_mmap_commit().
 *
 * Example:
\code
  double phase = 0;
  const snd_pcm_area_t *areas;
  snd_pcm_sframes_t avail, size, commitres;
  snd_pcm_uframes_t offset, frames;
  int err;

  avail = snd_pcm_avail_update(pcm);
  if (avail < 0)
    error(avail);
  // at this point, we can transfer at least 'avail' frames
  
  // we want to process frames in chunks (period_size)
  if (avail < period_size)
    goto _skip;
  size = period_size;
  // it is possible that contiguous areas are smaller, thus we use a loop
  while (size > 0) {
    frames = size;

    err = snd_pcm_mmap_begin(pcm_handle, &areas, &offset, &frames);
    if (err < 0)
      error(err);
    // this function fills the areas from offset with count of frames
    generate_sine(areas, offset, frames, &phase);
    commitres = snd_pcm_mmap_commit(pcm_handle, offset, frames);
    if (commitres < 0 || commitres != frames)
      error(commitres >= 0 ? -EPIPE : commitres);
      
    size -= frames;
  }
 _skip:
\endcode
 *
 * Look to the \link example_test_pcm Sine-wave generator \endlink example
 * for more details about the generate_sine function.
 *
 * The function is thread-safe when built with the proper option.
 */
snd_pcm_sframes_t snd_pcm_mmap_commit(snd_pcm_t *pcm,
				      snd_pcm_uframes_t offset,
				      snd_pcm_uframes_t frames)
{
	snd_pcm_sframes_t result;
	int err;

	err = bad_pcm_state(pcm, P_STATE_RUNNABLE, 0);
	if (err < 0)
		return err;
	snd_pcm_lock(pcm->fast_op_arg);
	result = __snd_pcm_mmap_commit(pcm, offset, frames);
	snd_pcm_unlock(pcm->fast_op_arg);
	return result;
}

#ifndef DOC_HIDDEN
/* locked version*/
snd_pcm_sframes_t __snd_pcm_mmap_commit(snd_pcm_t *pcm,
					snd_pcm_uframes_t offset,
					snd_pcm_uframes_t frames)
{
	assert(pcm);
	if (CHECK_SANITY(offset != *pcm->appl.ptr % pcm->buffer_size)) {
		SNDMSG("commit offset (%ld) doesn't match with appl_ptr (%ld) %% buf_size (%ld)",
		       offset, *pcm->appl.ptr, pcm->buffer_size);
		return -EPIPE;
	}
	if (CHECK_SANITY(frames > snd_pcm_mmap_avail(pcm))) {
		SNDMSG("commit frames (%ld) overflow (avail = %ld)", frames,
		       snd_pcm_mmap_avail(pcm));
		return -EPIPE;
	}
	if (pcm->fast_ops->mmap_commit)
		return pcm->fast_ops->mmap_commit(pcm->fast_op_arg, offset, frames);
	else
		return -ENOSYS;
}

int _snd_pcm_poll_descriptor(snd_pcm_t *pcm)
{
	assert(pcm);
	return pcm->poll_fd;
}

void snd_pcm_areas_from_buf(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, 
			    void *buf)
{
	unsigned int channel;
	unsigned int channels;

	snd_pcm_lock(pcm);
	channels = pcm->channels;
	for (channel = 0; channel < channels; ++channel, ++areas) {
		areas->addr = buf;
		areas->first = channel * pcm->sample_bits;
		areas->step = pcm->frame_bits;
	}
	snd_pcm_unlock(pcm);
}

void snd_pcm_areas_from_bufs(snd_pcm_t *pcm, snd_pcm_channel_area_t *areas, 
			     void **bufs)
{
	unsigned int channel;
	unsigned int channels;

	snd_pcm_lock(pcm);
	channels = pcm->channels;
	for (channel = 0; channel < channels; ++channel, ++areas, ++bufs) {
		areas->addr = *bufs;
		areas->first = 0;
		areas->step = pcm->sample_bits;
	}
	snd_pcm_unlock(pcm);
}

snd_pcm_sframes_t snd_pcm_read_areas(snd_pcm_t *pcm, const snd_pcm_channel_area_t *areas,
				     snd_pcm_uframes_t offset, snd_pcm_uframes_t size,
				     snd_pcm_xfer_areas_func_t func)
{
	snd_pcm_uframes_t xfer = 0;
	snd_pcm_sframes_t err = 0;
	snd_pcm_state_t state;

	if (size == 0)
		return 0;

	__snd_pcm_lock(pcm->fast_op_arg); /* forced lock */
	while (size > 0) {
		snd_pcm_uframes_t frames;
		snd_pcm_sframes_t avail;
	_again:
		state = __snd_pcm_state(pcm);
		switch (state) {
		case SND_PCM_STATE_PREPARED:
			err = __snd_pcm_start(pcm);
			if (err < 0)
				goto _end;
			break;
		case SND_PCM_STATE_RUNNING:
			err = __snd_pcm_hwsync(pcm);
			if (err < 0)
				goto _end;
			break;
		case SND_PCM_STATE_DRAINING:
		case SND_PCM_STATE_PAUSED:
			break;
		default:
			err = pcm_state_to_error(state);
			if (!err)
				err = -EBADFD;
			goto _end;
		}
		avail = __snd_pcm_avail_update(pcm);
		if (avail < 0) {
			err = avail;
			goto _end;
		}
		if (avail == 0) {
			if (state == SND_PCM_STATE_DRAINING)
				goto _end;
			if (pcm->mode & SND_PCM_NONBLOCK) {
				err = -EAGAIN;
				goto _end;
			}

			err = __snd_pcm_wait_in_lock(pcm, SND_PCM_WAIT_IO);
			if (err < 0)
				break;
			goto _again;
			
		}
		frames = size;
		if (frames > (snd_pcm_uframes_t) avail)
			frames = avail;
		if (! frames)
			break;
		err = func(pcm, areas, offset, frames);
		if (err < 0)
			break;
		frames = err;
		offset += frames;
		size -= frames;
		xfer += frames;
	}
 _end:
	__snd_pcm_unlock(pcm->fast_op_arg);
	return xfer > 0 ? (snd_pcm_sframes_t) xfer : snd_pcm_check_error(pcm, err);
}

snd_pcm_sframes_t snd_pcm_write_areas(snd_pcm_t *pcm, const snd_pcm_channel_area_t *areas,
				      snd_pcm_uframes_t offset, snd_pcm_uframes_t size,
				      snd_pcm_xfer_areas_func_t func)
{
	snd_pcm_uframes_t xfer = 0;
	snd_pcm_sframes_t err = 0;
	snd_pcm_state_t state;

	if (size == 0)
		return 0;

	__snd_pcm_lock(pcm->fast_op_arg); /* forced lock */
	while (size > 0) {
		snd_pcm_uframes_t frames;
		snd_pcm_sframes_t avail;
	_again:
		state = __snd_pcm_state(pcm);
		switch (state) {
		case SND_PCM_STATE_PREPARED:
		case SND_PCM_STATE_PAUSED:
			break;
		case SND_PCM_STATE_RUNNING:
			err = __snd_pcm_hwsync(pcm);
			if (err < 0)
				goto _end;
			break;
		default:
			err = pcm_state_to_error(state);
			if (!err)
				err = -EBADFD;
			goto _end;
		}
		avail = __snd_pcm_avail_update(pcm);
		if (avail < 0) {
			err = avail;
			goto _end;
		}
		if (state == SND_PCM_STATE_RUNNING &&
		    size > (snd_pcm_uframes_t)avail) {
			if (snd_pcm_may_wait_for_avail_min(pcm, avail)) {
				if (pcm->mode & SND_PCM_NONBLOCK) {
					err = -EAGAIN;
					goto _end;
				}

				err = snd_pcm_wait_nocheck(pcm, SND_PCM_WAIT_IO);
				if (err < 0)
					break;
				goto _again;
			}
			/* the snd_pcm_may_wait_for_avail_min may check against the
			 * updated hw.ptr (slaves), get the avail again here
			 */
			avail = __snd_pcm_avail_update(pcm);
			if (avail < 0) {
				err = avail;
				goto _end;
			}
		}
		frames = size;
		if (frames > (snd_pcm_uframes_t) avail)
			frames = avail;
		if (! frames)
			break;
		err = func(pcm, areas, offset, frames);
		if (err < 0)
			break;
		frames = err;
		if (state == SND_PCM_STATE_PREPARED) {
			snd_pcm_sframes_t hw_avail = pcm->buffer_size - avail;
			hw_avail += frames;
			/* some plugins might automatically start the stream */
			state = __snd_pcm_state(pcm);
			if (state == SND_PCM_STATE_PREPARED &&
			    hw_avail >= 0 &&
			    (snd_pcm_uframes_t) hw_avail >= pcm->start_threshold) {
				err = __snd_pcm_start(pcm);
				if (err < 0)
					goto _end;
			}
		}
		offset += frames;
		size -= frames;
		xfer += frames;
	}
 _end:
	__snd_pcm_unlock(pcm->fast_op_arg);
	return xfer > 0 ? (snd_pcm_sframes_t) xfer : snd_pcm_check_error(pcm, err);
}

snd_pcm_uframes_t _snd_pcm_mmap_hw_ptr(snd_pcm_t *pcm)
{
	return *pcm->hw.ptr;
}

snd_pcm_uframes_t _snd_pcm_boundary(snd_pcm_t *pcm)
{
	return pcm->boundary;
}

#ifndef DOC_HIDDEN
link_warning(_snd_pcm_mmap_hw_ptr, "Warning: _snd_pcm_mmap_hw_ptr() is deprecated, consider to not use this function");
link_warning(_snd_pcm_boundary, "Warning: _snd_pcm_boundary() is deprecated, consider to use snd_pcm_sw_params_current()");
#endif

static const char *const names[SND_PCM_HW_PARAM_LAST_INTERVAL + 1] = {
	[SND_PCM_HW_PARAM_FORMAT] = "format",
	[SND_PCM_HW_PARAM_CHANNELS] = "channels",
	[SND_PCM_HW_PARAM_RATE] = "rate",
	[SND_PCM_HW_PARAM_PERIOD_TIME] = "period_time",
	[SND_PCM_HW_PARAM_PERIOD_SIZE] = "period_size",
	[SND_PCM_HW_PARAM_BUFFER_TIME] = "buffer_time",
	[SND_PCM_HW_PARAM_BUFFER_SIZE] = "buffer_size",
	[SND_PCM_HW_PARAM_PERIODS] = "periods"
};

int snd_pcm_slave_conf(snd_config_t *root, snd_config_t *conf,
		       snd_config_t **_pcm_conf, unsigned int count, ...)
{
	snd_config_iterator_t i, next;
	const char *str;
	struct {
		unsigned int index;
		int flags;
		void *ptr;
		int present;
	} fields[count];
	unsigned int k;
	snd_config_t *pcm_conf = NULL;
	int err;
	int to_free = 0;
	va_list args;
	assert(root);
	assert(conf);
	assert(_pcm_conf);
	if (snd_config_get_string(conf, &str) >= 0) {
		err = snd_config_search_definition(root, "pcm_slave", str, &conf);
		if (err < 0) {
			SNDERR("Invalid slave definition");
			return -EINVAL;
		}
		to_free = 1;
	}
	if (snd_config_get_type(conf) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("Invalid slave definition");
		err = -EINVAL;
		goto _err;
	}
	va_start(args, count);
	for (k = 0; k < count; ++k) {
		fields[k].index = va_arg(args, int);
		fields[k].flags = va_arg(args, int);
		fields[k].ptr = va_arg(args, void *);
		fields[k].present = 0;
	}
	va_end(args);
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (strcmp(id, "comment") == 0)
			continue;
		if (strcmp(id, "pcm") == 0) {
			if (pcm_conf != NULL)
				snd_config_delete(pcm_conf);
			if ((err = snd_config_copy(&pcm_conf, n)) < 0)
				goto _err;
			continue;
		}
		for (k = 0; k < count; ++k) {
			unsigned int idx = fields[k].index;
			long v;
			assert(idx < SND_PCM_HW_PARAM_LAST_INTERVAL);
			assert(names[idx]);
			if (strcmp(id, names[idx]) != 0)
				continue;
			switch (idx) {
			case SND_PCM_HW_PARAM_FORMAT:
			{
				snd_pcm_format_t f;
				err = snd_config_get_string(n, &str);
				if (err < 0) {
				_invalid:
					SNDERR("invalid type for %s", id);
					goto _err;
				}
				if ((fields[k].flags & SCONF_UNCHANGED) &&
				    strcasecmp(str, "unchanged") == 0) {
					*(snd_pcm_format_t*)fields[k].ptr = (snd_pcm_format_t) -2;
					break;
				}
				f = snd_pcm_format_value(str);
				if (f == SND_PCM_FORMAT_UNKNOWN) {
					SNDERR("unknown format %s", str);
					err = -EINVAL;
					goto _err;
				}
				*(snd_pcm_format_t*)fields[k].ptr = f;
				break;
			}
			default:
				if ((fields[k].flags & SCONF_UNCHANGED)) {
					err = snd_config_get_string(n, &str);
					if (err >= 0 &&
					    strcasecmp(str, "unchanged") == 0) {
						*(int*)fields[k].ptr = -2;
						break;
					}
				}
				err = snd_config_get_integer(n, &v);
				if (err < 0)
					goto _invalid;
				*(int*)fields[k].ptr = v;
				break;
			}
			fields[k].present = 1;
			break;
		}
		if (k < count)
			continue;
		SNDERR("Unknown field %s", id);
		err = -EINVAL;
		goto _err;
	}
	if (!pcm_conf) {
		SNDERR("missing field pcm");
		err = -EINVAL;
		goto _err;
	}
	for (k = 0; k < count; ++k) {
		if ((fields[k].flags & SCONF_MANDATORY) && !fields[k].present) {
			SNDERR("missing field %s", names[fields[k].index]);
			err = -EINVAL;
			goto _err;
		}
	}
	*_pcm_conf = pcm_conf;
	pcm_conf = NULL;
	err = 0;
 _err:
 	if (pcm_conf)
 		snd_config_delete(pcm_conf);
	if (to_free)
		snd_config_delete(conf);
	return err;
}
		
static void snd_pcm_set_ptr(snd_pcm_t *pcm, snd_pcm_rbptr_t *rbptr,
			    volatile snd_pcm_uframes_t *hw_ptr, int fd, off_t offset)
{
	rbptr->master = NULL;	/* I'm master */
	rbptr->ptr = hw_ptr;
	rbptr->fd = fd;
	rbptr->offset = offset;
	if (rbptr->changed)
		rbptr->changed(pcm, NULL);
}

void snd_pcm_set_hw_ptr(snd_pcm_t *pcm, volatile snd_pcm_uframes_t *hw_ptr, int fd, off_t offset)
{
	assert(pcm);
	assert(hw_ptr);
	snd_pcm_set_ptr(pcm, &pcm->hw, hw_ptr, fd, offset);
}

void snd_pcm_set_appl_ptr(snd_pcm_t *pcm, volatile snd_pcm_uframes_t *appl_ptr, int fd, off_t offset)
{
	assert(pcm);
	assert(appl_ptr);
	snd_pcm_set_ptr(pcm, &pcm->appl, appl_ptr, fd, offset);
}

static void snd_pcm_link_ptr(snd_pcm_t *pcm, snd_pcm_rbptr_t *pcm_rbptr,
			     snd_pcm_t *slave, snd_pcm_rbptr_t *slave_rbptr)
{
	snd_pcm_t **a;
	int idx;
	
	a = slave_rbptr->link_dst;
	for (idx = 0; idx < slave_rbptr->link_dst_count; idx++)
		if (a[idx] == NULL) {
			a[idx] = pcm;
			goto __found_free_place;
		}
	a = realloc(a, sizeof(snd_pcm_t *) * (slave_rbptr->link_dst_count + 1));
	if (a == NULL) {
		pcm_rbptr->ptr = NULL;
		pcm_rbptr->fd = -1;
		pcm_rbptr->offset = 0UL;
		return;
	}
	a[slave_rbptr->link_dst_count++] = pcm;
      __found_free_place:
	pcm_rbptr->master = slave_rbptr->master ? slave_rbptr->master : slave;
	pcm_rbptr->ptr = slave_rbptr->ptr;
	pcm_rbptr->fd = slave_rbptr->fd;
	pcm_rbptr->offset = slave_rbptr->offset;
	slave_rbptr->link_dst = a;
	if (pcm_rbptr->changed)
		pcm_rbptr->changed(pcm, slave);
}

static void snd_pcm_unlink_ptr(snd_pcm_t *pcm, snd_pcm_rbptr_t *pcm_rbptr,
			       snd_pcm_t *slave, snd_pcm_rbptr_t *slave_rbptr)
{
	snd_pcm_t **a;
	int idx;

	a = slave_rbptr->link_dst;
	for (idx = 0; idx < slave_rbptr->link_dst_count; idx++) {
		if (a[idx] == pcm) {
			a[idx] = NULL;
			goto __found;
		}
	}
	/* assert(0); */
	return;

      __found:
      	pcm_rbptr->master = NULL;
	pcm_rbptr->ptr = NULL;
	pcm_rbptr->fd = -1;
	pcm_rbptr->offset = 0UL;
	if (pcm_rbptr->changed)
		pcm_rbptr->changed(pcm, slave);
}

void snd_pcm_link_hw_ptr(snd_pcm_t *pcm, snd_pcm_t *slave)
{
	assert(pcm);
	assert(slave);
	snd_pcm_link_ptr(pcm, &pcm->hw, slave, &slave->hw);
}

void snd_pcm_link_appl_ptr(snd_pcm_t *pcm, snd_pcm_t *slave)
{
	assert(pcm);
	assert(slave);
	snd_pcm_link_ptr(pcm, &pcm->appl, slave, &slave->appl);
}

void snd_pcm_unlink_hw_ptr(snd_pcm_t *pcm, snd_pcm_t *slave)
{
	assert(pcm);
	assert(slave);
	snd_pcm_unlink_ptr(pcm, &pcm->hw, slave, &slave->hw);
}

void snd_pcm_unlink_appl_ptr(snd_pcm_t *pcm, snd_pcm_t *slave)
{
	assert(pcm);
	assert(slave);
	snd_pcm_unlink_ptr(pcm, &pcm->appl, slave, &slave->appl);
}

#endif /* DOC_HIDDEN */

/*
 *
 */

#ifndef DOC_HIDDEN

#ifdef USE_VERSIONED_SYMBOLS

#define OBSOLETE1(name, what, new) \
  default_symbol_version(__##name, name, new); \
  symbol_version(__old_##name, name, what);

#else

#define OBSOLETE1(name, what, new) \
  use_default_symbol_version(__##name, name, new);

#endif /* USE_VERSIONED_SYMBOLS */

#define __P_OLD_GET(pfx, name, val_type, ret_type) \
EXPORT_SYMBOL ret_type pfx##name(const snd_pcm_hw_params_t *params) \
{ \
	val_type val; \
	if (INTERNAL(name)(params, &val) < 0) \
		return 0; \
	return (ret_type)val; \
}

#define __P_OLD_GET1(pfx, name, val_type, ret_type) \
EXPORT_SYMBOL ret_type pfx##name(const snd_pcm_hw_params_t *params, int *dir) \
{ \
	val_type val; \
	if (INTERNAL(name)(params, &val, dir) < 0) \
		return 0; \
	return (ret_type)val; \
}

#define __OLD_GET(name, val_type, ret_type) __P_OLD_GET(__old_, name, val_type, ret_type)
#define __OLD_GET1(name, val_type, ret_type) __P_OLD_GET1(__old_, name, val_type, ret_type)

__OLD_GET(snd_pcm_hw_params_get_access, snd_pcm_access_t, int);
__OLD_GET(snd_pcm_hw_params_get_format, snd_pcm_format_t, int);
__OLD_GET(snd_pcm_hw_params_get_subformat, snd_pcm_subformat_t, int);
__OLD_GET(snd_pcm_hw_params_get_channels, unsigned int, int);
__OLD_GET1(snd_pcm_hw_params_get_rate, unsigned int, int);
__OLD_GET1(snd_pcm_hw_params_get_period_time, unsigned int, int);
__OLD_GET1(snd_pcm_hw_params_get_period_size, snd_pcm_uframes_t, snd_pcm_sframes_t);
__OLD_GET1(snd_pcm_hw_params_get_periods, unsigned int, int);
__OLD_GET1(snd_pcm_hw_params_get_buffer_time, unsigned int, int);
__OLD_GET(snd_pcm_hw_params_get_buffer_size, snd_pcm_uframes_t, snd_pcm_sframes_t);
__OLD_GET1(snd_pcm_hw_params_get_tick_time, unsigned int, int);

__OLD_GET(snd_pcm_hw_params_get_channels_min, unsigned int, unsigned int);
__OLD_GET1(snd_pcm_hw_params_get_rate_min, unsigned int, unsigned int);
__OLD_GET1(snd_pcm_hw_params_get_period_time_min, unsigned int, unsigned int);
__OLD_GET1(snd_pcm_hw_params_get_period_size_min, snd_pcm_uframes_t, snd_pcm_uframes_t);
__OLD_GET1(snd_pcm_hw_params_get_periods_min, unsigned int, unsigned int);
__OLD_GET1(snd_pcm_hw_params_get_buffer_time_min, unsigned int, unsigned int);
__OLD_GET(snd_pcm_hw_params_get_buffer_size_min, snd_pcm_uframes_t, snd_pcm_uframes_t);
__OLD_GET1(snd_pcm_hw_params_get_tick_time_min, unsigned int, unsigned int);

__OLD_GET(snd_pcm_hw_params_get_channels_max, unsigned int, unsigned int);
__OLD_GET1(snd_pcm_hw_params_get_rate_max, unsigned int, unsigned int);
__OLD_GET1(snd_pcm_hw_params_get_period_time_max, unsigned int, unsigned int);
__OLD_GET1(snd_pcm_hw_params_get_period_size_max, snd_pcm_uframes_t, snd_pcm_uframes_t);
__OLD_GET1(snd_pcm_hw_params_get_periods_max, unsigned int, unsigned int);
__OLD_GET1(snd_pcm_hw_params_get_buffer_time_max, unsigned int, unsigned int);
__OLD_GET(snd_pcm_hw_params_get_buffer_size_max, snd_pcm_uframes_t, snd_pcm_uframes_t);
__OLD_GET1(snd_pcm_hw_params_get_tick_time_max, unsigned int, unsigned int);

#define __P_OLD_NEAR(pfx, name, ret_type) \
EXPORT_SYMBOL ret_type pfx##name(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, ret_type val) \
{ \
	if (INTERNAL(name)(pcm, params, &val) < 0) \
		return 0; \
	return (ret_type)val; \
}

#define __P_OLD_NEAR1(pfx, name, ret_type) \
EXPORT_SYMBOL ret_type pfx##name(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, ret_type val, int *dir) \
{ \
	if (INTERNAL(name)(pcm, params, &val, dir) < 0) \
		return 0; \
	return (ret_type)val; \
}

#define __OLD_NEAR(name, ret_type) __P_OLD_NEAR(__old_, name, ret_type)
#define __OLD_NEAR1(name, ret_type) __P_OLD_NEAR1(__old_, name, ret_type)

__OLD_NEAR(snd_pcm_hw_params_set_channels_near, unsigned int);
__OLD_NEAR1(snd_pcm_hw_params_set_rate_near, unsigned int);
__OLD_NEAR1(snd_pcm_hw_params_set_period_time_near, unsigned int);
__OLD_NEAR1(snd_pcm_hw_params_set_period_size_near, snd_pcm_uframes_t);
__OLD_NEAR1(snd_pcm_hw_params_set_periods_near, unsigned int);
__OLD_NEAR1(snd_pcm_hw_params_set_buffer_time_near, unsigned int);
__OLD_NEAR(snd_pcm_hw_params_set_buffer_size_near, snd_pcm_uframes_t);
__OLD_NEAR1(snd_pcm_hw_params_set_tick_time_near, unsigned int);

#define __P_OLD_SET_FL(pfx, name, ret_type) \
EXPORT_SYMBOL ret_type pfx##name(snd_pcm_t *pcm, snd_pcm_hw_params_t *params) \
{ \
	ret_type val; \
	if (INTERNAL(name)(pcm, params, &val) < 0) \
		return 0; \
	return (ret_type)val; \
}

#define __P_OLD_SET_FL1(pfx, name, ret_type) \
EXPORT_SYMBOL ret_type pfx##name(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir) \
{ \
	ret_type val; \
	if (INTERNAL(name)(pcm, params, &val, dir) < 0) \
		return 0; \
	return (ret_type)val; \
}

#define __OLD_SET_FL(name, ret_type) __P_OLD_SET_FL(__old_, name, ret_type)
#define __OLD_SET_FL1(name, ret_type) __P_OLD_SET_FL1(__old_, name, ret_type)

__OLD_SET_FL(snd_pcm_hw_params_set_access_first, snd_pcm_access_t);
__OLD_SET_FL(snd_pcm_hw_params_set_format_first, snd_pcm_format_t);
__OLD_SET_FL(snd_pcm_hw_params_set_subformat_first, snd_pcm_subformat_t);
__OLD_SET_FL(snd_pcm_hw_params_set_channels_first, unsigned int);
__OLD_SET_FL1(snd_pcm_hw_params_set_rate_first, unsigned int);
__OLD_SET_FL1(snd_pcm_hw_params_set_period_time_first, unsigned int);
__OLD_SET_FL1(snd_pcm_hw_params_set_period_size_first, snd_pcm_uframes_t);
__OLD_SET_FL1(snd_pcm_hw_params_set_periods_first, unsigned int);
__OLD_SET_FL1(snd_pcm_hw_params_set_buffer_time_first, unsigned int);
__OLD_SET_FL(snd_pcm_hw_params_set_buffer_size_first, snd_pcm_uframes_t);
__OLD_SET_FL1(snd_pcm_hw_params_set_tick_time_first, unsigned int);

__OLD_SET_FL(snd_pcm_hw_params_set_access_last, snd_pcm_access_t);
__OLD_SET_FL(snd_pcm_hw_params_set_format_last, snd_pcm_format_t);
__OLD_SET_FL(snd_pcm_hw_params_set_subformat_last, snd_pcm_subformat_t);
__OLD_SET_FL(snd_pcm_hw_params_set_channels_last, unsigned int);
__OLD_SET_FL1(snd_pcm_hw_params_set_rate_last, unsigned int);
__OLD_SET_FL1(snd_pcm_hw_params_set_period_time_last, unsigned int);
__OLD_SET_FL1(snd_pcm_hw_params_set_period_size_last, snd_pcm_uframes_t);
__OLD_SET_FL1(snd_pcm_hw_params_set_periods_last, unsigned int);
__OLD_SET_FL1(snd_pcm_hw_params_set_buffer_time_last, unsigned int);
__OLD_SET_FL(snd_pcm_hw_params_set_buffer_size_last, snd_pcm_uframes_t);
__OLD_SET_FL1(snd_pcm_hw_params_set_tick_time_last, unsigned int);

#define __P_OLD_GET_SW(pfx, name, ret_type) \
EXPORT_SYMBOL ret_type pfx##name(snd_pcm_sw_params_t *params) \
{ \
	ret_type val; \
	if (INTERNAL(name)(params, &val) < 0) \
		return 0; \
	return (ret_type)val; \
}

#define __OLD_GET_SW(name, ret_type) __P_OLD_GET_SW(__old_, name, ret_type)

__OLD_GET_SW(snd_pcm_sw_params_get_tstamp_mode, snd_pcm_tstamp_t);
__OLD_GET_SW(snd_pcm_sw_params_get_sleep_min, unsigned int);
__OLD_GET_SW(snd_pcm_sw_params_get_avail_min, snd_pcm_uframes_t);
__OLD_GET_SW(snd_pcm_sw_params_get_xfer_align, snd_pcm_uframes_t);
__OLD_GET_SW(snd_pcm_sw_params_get_start_threshold, snd_pcm_uframes_t);
__OLD_GET_SW(snd_pcm_sw_params_get_stop_threshold, snd_pcm_uframes_t);
__OLD_GET_SW(snd_pcm_sw_params_get_silence_threshold, snd_pcm_uframes_t);
__OLD_GET_SW(snd_pcm_sw_params_get_silence_size, snd_pcm_uframes_t);

OBSOLETE1(snd_pcm_hw_params_get_access, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_access_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_access_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_format, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_format_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_format_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_subformat, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_subformat_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_subformat_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_channels, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_channels_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_channels_max, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_channels_near, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_channels_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_channels_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_rate, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_rate_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_rate_max, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_rate_near, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_rate_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_rate_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_period_time, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_period_time_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_period_time_max, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_period_time_near, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_period_time_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_period_time_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_period_size, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_period_size_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_period_size_max, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_period_size_near, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_period_size_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_period_size_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_periods, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_periods_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_periods_max, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_periods_near, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_periods_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_periods_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_buffer_time, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_buffer_time_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_buffer_time_max, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_buffer_time_near, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_buffer_time_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_buffer_time_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_buffer_size, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_buffer_size_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_buffer_size_max, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_buffer_size_near, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_buffer_size_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_buffer_size_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_hw_params_get_tick_time, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_tick_time_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_get_tick_time_max, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_tick_time_near, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_tick_time_first, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_hw_params_set_tick_time_last, ALSA_0.9, ALSA_0.9.0rc4);

OBSOLETE1(snd_pcm_sw_params_get_tstamp_mode, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_sw_params_get_sleep_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_sw_params_get_avail_min, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_sw_params_get_xfer_align, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_sw_params_get_start_threshold, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_sw_params_get_stop_threshold, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_sw_params_get_silence_threshold, ALSA_0.9, ALSA_0.9.0rc4);
OBSOLETE1(snd_pcm_sw_params_get_silence_size, ALSA_0.9, ALSA_0.9.0rc4);

#endif /* DOC_HIDDEN */

static int chmap_equal(const snd_pcm_chmap_t *a, const snd_pcm_chmap_t *b)
{
	if (a->channels != b->channels)
		return 0;
	return !memcmp(a->pos, b->pos, a->channels * sizeof(a->pos[0]));
}

/**
 * \!brief Query the available channel maps
 * \param pcm PCM handle to query
 * \return the NULL-terminated array of integer pointers, each of
 * which contains the channel map. A channel map is represented by an
 * integer array, beginning with the channel map type, followed by the
 * number of channels, and the position of each channel. Return NULL
 * in case of an error.
 *
 * Note: the caller is requested to release the returned value via
 * snd_pcm_free_chmaps().
 */
snd_pcm_chmap_query_t **snd_pcm_query_chmaps(snd_pcm_t *pcm)
{
	if (!pcm->ops->query_chmaps)
		return NULL;
	return pcm->ops->query_chmaps(pcm);
}

/**
 * \!brief Release the channel map array allocated via #snd_pcm_query_chmaps
 * \param maps the array pointer to release
 */
void snd_pcm_free_chmaps(snd_pcm_chmap_query_t **maps)
{
	snd_pcm_chmap_query_t **p;
	if (!maps)
		return;
	for (p = maps; *p; p++)
		free(*p);
	free(maps);
}

/**
 * \!brief Get the current channel map
 * \param pcm PCM instance
 * \return the current channel map, or NULL if error
 *
 * Note: the caller is requested to release the returned value via free()
 */
snd_pcm_chmap_t *snd_pcm_get_chmap(snd_pcm_t *pcm)
{
	if (!pcm->ops->get_chmap)
		return NULL;
	return pcm->ops->get_chmap(pcm);
}

/**
 * \!brief Configure the current channel map
 * \param pcm PCM instance
 * \param map the channel map to write
 * \return zero if succeeded, or a negative error code
 */
int snd_pcm_set_chmap(snd_pcm_t *pcm, const snd_pcm_chmap_t *map)
{
	const snd_pcm_chmap_t *oldmap;
	int nochange;

	oldmap = snd_pcm_get_chmap(pcm);
	nochange = (oldmap && chmap_equal(oldmap, map));
	free((void *)oldmap);
	if (nochange)
		return 0;

	if (!pcm->ops->set_chmap)
		return -ENXIO;
	return pcm->ops->set_chmap(pcm, map);
}

/*
 */
#ifndef DOC_HIDDEN
#define _NAME(n) [SND_CHMAP_TYPE_##n] = #n
static const char *chmap_type_names[SND_CHMAP_TYPE_LAST + 1] = {
	_NAME(NONE), _NAME(FIXED), _NAME(VAR), _NAME(PAIRED),
};
#undef _NAME
#endif

/**
 * \!brief Get a name string for a channel map type as query results
 * \param val Channel position
 * \return The string corresponding to the given type, or NULL
 */
const char *snd_pcm_chmap_type_name(enum snd_pcm_chmap_type val)
{
	if (val <= SND_CHMAP_TYPE_LAST)
		return chmap_type_names[val];
	else
		return NULL;
}

#ifndef DOC_HIDDEN
#define _NAME(n) [SND_CHMAP_##n] = #n
static const char *chmap_names[SND_CHMAP_LAST + 1] = {
	_NAME(UNKNOWN), _NAME(NA), _NAME(MONO),
	_NAME(FL), _NAME(FR),
	_NAME(RL), _NAME(RR),
	_NAME(FC), _NAME(LFE),
	_NAME(SL), _NAME(SR),
	_NAME(RC), _NAME(FLC), _NAME(FRC), _NAME(RLC), _NAME(RRC),
	_NAME(FLW), _NAME(FRW),
	_NAME(FLH), _NAME(FCH), _NAME(FRH), _NAME(TC),
	_NAME(TFL), _NAME(TFR), _NAME(TFC),
	_NAME(TRL), _NAME(TRR), _NAME(TRC),
	_NAME(TFLC), _NAME(TFRC), _NAME(TSL), _NAME(TSR),
	_NAME(LLFE), _NAME(RLFE),
	_NAME(BC), _NAME(BLC), _NAME(BRC),
};
#undef _NAME
#endif

/**
 * \!brief Get a name string for a standard channel map position
 * \param val Channel position
 * \return The string corresponding to the given position, or NULL
 */
const char *snd_pcm_chmap_name(enum snd_pcm_chmap_position val)
{
	if (val <= SND_CHMAP_LAST)
		return chmap_names[val];
	else
		return NULL;
}

static const char *chmap_long_names[SND_CHMAP_LAST + 1] = {
	[SND_CHMAP_UNKNOWN] = "Unknown",
	[SND_CHMAP_NA] = "Unused",
	[SND_CHMAP_MONO] = "Mono",
	[SND_CHMAP_FL] = "Front Left",
	[SND_CHMAP_FR] = "Front Right",
	[SND_CHMAP_RL] = "Rear Left",
	[SND_CHMAP_RR] = "Rear Right",
	[SND_CHMAP_FC] = "Front Center",
	[SND_CHMAP_LFE] = "LFE",
	[SND_CHMAP_SL] = "Side Left",
	[SND_CHMAP_SR] = "Side Right",
	[SND_CHMAP_RC] = "Rear Center",
	[SND_CHMAP_FLC] = "Front Left Center",
	[SND_CHMAP_FRC] = "Front Right Center",
	[SND_CHMAP_RLC] = "Rear Left Center",
	[SND_CHMAP_RRC] = "Rear Right Center",
	[SND_CHMAP_FLW] = "Front Left Wide",
	[SND_CHMAP_FRW] = "Front Right Wide",
	[SND_CHMAP_FLH] = "Front Left High",
	[SND_CHMAP_FCH] = "Front Center High",
	[SND_CHMAP_FRH] = "Front Right High",
	[SND_CHMAP_TC] = "Top Center",
	[SND_CHMAP_TFL] = "Top Front Left",
	[SND_CHMAP_TFR] = "Top Front Right",
	[SND_CHMAP_TFC] = "Top Front Center",
	[SND_CHMAP_TRL] = "Top Rear Left",
	[SND_CHMAP_TRR] = "Top Rear Right",
	[SND_CHMAP_TRC] = "Top Rear Center",
	[SND_CHMAP_TFLC] = "Top Front Left Center",
	[SND_CHMAP_TFRC] = "Top Front Right Center",
	[SND_CHMAP_TSL] = "Top Side Left",
	[SND_CHMAP_TSR] = "Top Side Right",
	[SND_CHMAP_LLFE] = "Left LFE",
	[SND_CHMAP_RLFE] = "Right LFE",
	[SND_CHMAP_BC] = "Bottom Center",
	[SND_CHMAP_BLC] = "Bottom Left Center",
	[SND_CHMAP_BRC] = "Bottom Right Center",
};

/**
 * \!brief Get a longer name string for a standard channel map position
 * \param val Channel position
 * \return The string corresponding to the given position, or NULL
 */
const char *snd_pcm_chmap_long_name(enum snd_pcm_chmap_position val)
{
	if (val <= SND_CHMAP_LAST)
		return chmap_long_names[val];
	else
		return NULL;
}

/**
 * \!brief Print the channels in chmap on the buffer
 * \param map The channel map to print
 * \param maxlen The maximal length to write (including NUL letter)
 * \param buf The buffer to write
 * \return The actual string length or a negative error code
 */
int snd_pcm_chmap_print(const snd_pcm_chmap_t *map, size_t maxlen, char *buf)
{
	unsigned int i, len = 0;

	for (i = 0; i < map->channels; i++) {
		unsigned int p = map->pos[i] & SND_CHMAP_POSITION_MASK;
		if (i > 0) {
			len += snprintf(buf + len, maxlen - len, " ");
			if (len >= maxlen)
				return -ENOMEM;
		}
		if (map->pos[i] & SND_CHMAP_DRIVER_SPEC)
			len += snprintf(buf + len, maxlen - len, "%d", p);
		else {
			const char *name = chmap_names[p];
			if (name)
				len += snprintf(buf + len, maxlen - len,
						"%s", name);
			else
				len += snprintf(buf + len, maxlen - len,
						"Ch%d", p);
		}
		if (len >= maxlen)
			return -ENOMEM;
		if (map->pos[i] & SND_CHMAP_PHASE_INVERSE) {
			len += snprintf(buf + len, maxlen - len, "[INV]");
			if (len >= maxlen)
				return -ENOMEM;
		}
	}
	return len;
}

static int str_to_chmap(const char *str, int len)
{
	int val;
	unsigned long v;
	char *p;

	if (isdigit(*str)) {
		v = strtoul(str, &p, 0);
		if (v == ULONG_MAX)
			return -1;
		val = v;
		val |= SND_CHMAP_DRIVER_SPEC;
		str = p;
	} else if (!strncasecmp(str, "ch", 2)) {
		v = strtoul(str + 2, &p, 0);
		if (v == ULONG_MAX)
			return -1;
		val = v;
		str = p;
	} else {
		for (val = 0; val <= SND_CHMAP_LAST; val++) {
			int slen;
			assert(chmap_names[val]);
			slen = strlen(chmap_names[val]);
			if (slen > len)
				continue;
			if (!strncasecmp(str, chmap_names[val], slen) &&
			    !isalpha(str[slen])) {
				str += slen;
				break;
			}
		}
		if (val > SND_CHMAP_LAST)
			return -1;
	}
	if (str && !strncasecmp(str, "[INV]", 5))
		val |= SND_CHMAP_PHASE_INVERSE;
	return val;
}

/**
 * \!brief Convert from string to channel position
 * \param str The string to parse
 * \return The channel position value or -1 as an error
 */
unsigned int snd_pcm_chmap_from_string(const char *str)
{
	return str_to_chmap(str, strlen(str));
}

/**
 * \!brief Convert from string to channel map
 * \param str The string to parse
 * \return The channel map
 *
 * Note: the caller is requested to release the returned value via free()
 */
snd_pcm_chmap_t *snd_pcm_chmap_parse_string(const char *str)
{
	int i, ch = 0;
	int tmp_map[64];
	snd_pcm_chmap_t *map;

	for (;;) {
		const char *p;
		int len, val;

		if (ch >= (int)(sizeof(tmp_map) / sizeof(tmp_map[0])))
			return NULL;
		for (p = str; *p && isalnum(*p); p++)
			;
		len = p - str;
		if (!len)
			return NULL;
		val = str_to_chmap(str, len);
		if (val < 0)
			return NULL;
		str += len;
		if (*str == '[') {
			if (!strncmp(str, "[INV]", 5)) {
				val |= SND_CHMAP_PHASE_INVERSE;
				str += 5;
			}
		}
		tmp_map[ch] = val;
		ch++;
		for (; *str && !isalnum(*str); str++)
			;
		if (!*str)
			break;
	}
	map = malloc(sizeof(*map) + ch * sizeof(int));
	if (!map)
		return NULL;
	map->channels = ch;
	for (i = 0; i < ch; i++)
		map->pos[i] = tmp_map[i];
	return map;
}

/* copy a single channel map with the fixed type to chmap_query pointer */
static int _copy_to_fixed_query_map(snd_pcm_chmap_query_t **dst,
				    const snd_pcm_chmap_t *src)
{
	*dst = malloc((src->channels + 2) * sizeof(int));
	if (!*dst)
		return -ENOMEM;
	(*dst)->type = SND_CHMAP_TYPE_FIXED;
	memcpy(&(*dst)->map, src, (src->channels + 1) * sizeof(int));
	return 0;
}

#ifndef DOC_HIDDEN
/* make a chmap_query array from a single channel map */
snd_pcm_chmap_query_t **
_snd_pcm_make_single_query_chmaps(const snd_pcm_chmap_t *src)
{
	snd_pcm_chmap_query_t **maps;

	maps = calloc(2, sizeof(*maps));
	if (!maps)
		return NULL;
	if (_copy_to_fixed_query_map(maps, src)) {
		free(maps);
		return NULL;
	}
	return maps;
}

/* make a copy of chmap */
snd_pcm_chmap_t *_snd_pcm_copy_chmap(const snd_pcm_chmap_t *src)
{
	snd_pcm_chmap_t *map;

	map = malloc((src->channels + 1) * sizeof(int));
	if (!map)
		return NULL;
	memcpy(map, src, (src->channels + 1) * sizeof(int));
	return map;
}

/* make a copy of channel maps */
snd_pcm_chmap_query_t **
_snd_pcm_copy_chmap_query(snd_pcm_chmap_query_t * const *src)
{
	snd_pcm_chmap_query_t * const *p;
	snd_pcm_chmap_query_t **maps;
	int i, nums;

	for (nums = 0, p = src; *p; p++)
		nums++;

	maps = calloc(nums + 1, sizeof(*maps));
	if (!maps)
		return NULL;
	for (i = 0; i < nums; i++) {
		maps[i] = malloc((src[i]->map.channels + 2) * sizeof(int));
		if (!maps[i]) {
			snd_pcm_free_chmaps(maps);
			return NULL;
		}
		memcpy(maps[i], src[i], (src[i]->map.channels + 2) * sizeof(int));
	}
	return maps;
}

/* select the channel map with the current PCM channels and make a copy */
snd_pcm_chmap_t *
_snd_pcm_choose_fixed_chmap(snd_pcm_t *pcm, snd_pcm_chmap_query_t * const *maps)
{
	snd_pcm_chmap_query_t * const *p;

	for (p = maps; *p; p++) {
		if ((*p)->map.channels == pcm->channels)
			return _snd_pcm_copy_chmap(&(*p)->map);
	}
	return NULL;
}

/* make chmap_query array from the config tree;
 * conf must be a compound (array)
 */
snd_pcm_chmap_query_t **
_snd_pcm_parse_config_chmaps(snd_config_t *conf)
{
	snd_pcm_chmap_t *chmap;
	snd_pcm_chmap_query_t **maps;
	snd_config_iterator_t i, next;
	const char *str;
	int nums, err;

	if (snd_config_get_type(conf) != SND_CONFIG_TYPE_COMPOUND)
		return NULL;

	nums = 0;
	snd_config_for_each(i, next, conf) {
		nums++;
	}

	maps = calloc(nums + 1, sizeof(*maps));
	if (!maps)
		return NULL;

	nums = 0;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		err = snd_config_get_string(n, &str);
		if (err < 0)
			goto error;
		chmap = snd_pcm_chmap_parse_string(str);
		if (!chmap)
			goto error;
		if (_copy_to_fixed_query_map(maps + nums, chmap)) {
			free(chmap);
			goto error;
		}
		free(chmap);
		nums++;
	}
	return maps;

 error:
	snd_pcm_free_chmaps(maps);
	return NULL;
}
#endif /* DOC_HIDDEN */

/*
 * basic helpers
 */
 
 
/**
 * \brief Recover the stream state from an error or suspend
 * \param pcm PCM handle
 * \param err error number
 * \param silent do not print error reason
 * \return 0 when error code was handled successfuly, otherwise a negative error code
 *
 * This a high-level helper function building on other functions.
 *
 * This functions handles -EINTR (interrupted system call),
 * -EPIPE (overrun or underrun) and -ESTRPIPE (stream is suspended)
 * error codes trying to prepare given stream for next I/O.
 *
 * Note that this function returns the original error code when it is not
 * handled inside this function (for example -EAGAIN is returned back).
 */
int snd_pcm_recover(snd_pcm_t *pcm, int err, int silent)
{
        if (err > 0)
                err = -err;
        if (err == -EINTR)	/* nothing to do, continue */
                return 0;
        if (err == -EPIPE) {
                const char *s;
                if (snd_pcm_stream(pcm) == SND_PCM_STREAM_PLAYBACK)
                        s = "underrun";
                else
                        s = "overrun";
                if (!silent)
                        SNDERR("%s occurred", s);
                err = snd_pcm_prepare(pcm);
                if (err < 0) {
                        SNDERR("cannot recovery from %s, prepare failed: %s", s, snd_strerror(err));
                        return err;
                }
                return 0;
        }
        if (err == -ESTRPIPE) {
                while ((err = snd_pcm_resume(pcm)) == -EAGAIN)
                        /* wait until suspend flag is released */
                        poll(NULL, 0, 1000);
                if (err < 0) {
                        err = snd_pcm_prepare(pcm);
                        if (err < 0) {
                                SNDERR("cannot recovery from suspend, prepare failed: %s", snd_strerror(err));
                                return err;
                        }
                }
                return 0;
        }
        return err;
}

/**
 * \brief Set the hardware and software parameters in a simple way
 * \param pcm PCM handle
 * \param format required PCM format
 * \param access required PCM access
 * \param channels required PCM channels
 * \param rate required sample rate in Hz
 * \param soft_resample 0 = disallow alsa-lib resample stream, 1 = allow resampling
 * \param latency required overall latency in us
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_set_params(snd_pcm_t *pcm,
                       snd_pcm_format_t format,
                       snd_pcm_access_t access,
                       unsigned int channels,
                       unsigned int rate,
                       int soft_resample,
                       unsigned int latency)
{
	snd_pcm_hw_params_t params_saved, params = {0};
	snd_pcm_sw_params_t swparams = {0};
	const char *s = snd_pcm_stream_name(snd_pcm_stream(pcm));
	snd_pcm_uframes_t buffer_size, period_size;
	unsigned int rrate, period_time;
	int err;

	assert(pcm);
	/* choose all parameters */
	err = snd_pcm_hw_params_any(pcm, &params);
	if (err < 0) {
		SNDERR("Broken configuration for %s: no configurations available",
		       s);
		return err;
        }
	/* set software resampling */
	err = snd_pcm_hw_params_set_rate_resample(pcm, &params, soft_resample);
	if (err < 0) {
		SNDERR("Resampling setup failed for %s: %s",
		       s, snd_strerror(err));
		return err;
	}
	/* set the selected read/write format */
	err = snd_pcm_hw_params_set_access(pcm, &params, access);
	if (err < 0) {
		SNDERR("Access type not available for %s: %s",
		       s, snd_strerror(err));
		return err;
	}
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(pcm, &params, format);
	if (err < 0) {
		SNDERR("Sample format not available for %s: %s",
		       s, snd_strerror(err));
		return err;
	}
	/* set the count of channels */
	err = snd_pcm_hw_params_set_channels(pcm, &params, channels);
	if (err < 0) {
		SNDERR("Channels count (%i) not available for %s: %s",
		       channels, s, snd_strerror(err));
		return err;
	}
	/* set the stream rate */
	rrate = rate;
	err = INTERNAL(snd_pcm_hw_params_set_rate_near)(pcm, &params, &rrate,
							0);
	if (err < 0) {
		SNDERR("Rate %iHz not available for playback: %s",
		       rate, snd_strerror(err));
		return err;
	}
	if (rrate != rate) {
		SNDERR("Rate doesn't match (requested %iHz, get %iHz)",
		       rate, rrate);
		return -EINVAL;
	}
	/* set the buffer time */
	params_saved = params;
	err = INTERNAL(snd_pcm_hw_params_set_buffer_time_near)(pcm, &params,
							&latency, NULL);
	if (err < 0) {
		/* error path -> set period size as first */
		params = params_saved;
		/* set the period time */
		period_time = latency / 4;
		err = INTERNAL(snd_pcm_hw_params_set_period_time_near)(pcm,
						&params, &period_time, NULL);
		if (err < 0) {
			SNDERR("Unable to set period time %i for %s: %s",
			       period_time, s, snd_strerror(err));
			return err;
		}
		err = INTERNAL(snd_pcm_hw_params_get_period_size)(&params,
							&period_size, NULL);
		if (err < 0) {
			SNDERR("Unable to get period size for %s: %s",
							s, snd_strerror(err));
			return err;
		}
		buffer_size = period_size * 4;
		err = INTERNAL(snd_pcm_hw_params_set_buffer_size_near)(pcm,
							&params, &buffer_size);
		if (err < 0) {
			SNDERR("Unable to set buffer size %lu %s: %s",
					buffer_size, s, snd_strerror(err));
			return err;
		}
		err = INTERNAL(snd_pcm_hw_params_get_buffer_size)(&params,
								&buffer_size);
		if (err < 0) {
			SNDERR("Unable to get buffer size for %s: %s",
			       s, snd_strerror(err));
			return err;
		}
	} else {
		/* standard configuration buffer_time -> periods */
		err = INTERNAL(snd_pcm_hw_params_get_buffer_size)(&params,
								&buffer_size);
		if (err < 0) {
			SNDERR("Unable to get buffer size for %s: %s",
							s, snd_strerror(err));
			return err;
		}
		err = INTERNAL(snd_pcm_hw_params_get_buffer_time)(&params,
							&latency, NULL);
		if (err < 0) {
			SNDERR("Unable to get buffer time (latency) for %s: %s",
			       s, snd_strerror(err));
			return err;
		}
		/* set the period time */
		period_time = latency / 4;
		err = INTERNAL(snd_pcm_hw_params_set_period_time_near)(pcm,
						&params, &period_time, NULL);
		if (err < 0) {
			SNDERR("Unable to set period time %i for %s: %s",
			       period_time, s, snd_strerror(err));
			return err;
		}
		err = INTERNAL(snd_pcm_hw_params_get_period_size)(&params,
							&period_size, NULL);
		if (err < 0) {
			SNDERR("Unable to get period size for %s: %s",
			       s, snd_strerror(err));
			return err;
		}
	}
	/* write the parameters to device */
	err = snd_pcm_hw_params(pcm, &params);
	if (err < 0) {
		SNDERR("Unable to set hw params for %s: %s",
		       s, snd_strerror(err));
		return err;
	}

	/* get the current swparams */
	err = snd_pcm_sw_params_current(pcm, &swparams);
	if (err < 0) {
		SNDERR("Unable to determine current swparams for %s: %s",
		       s, snd_strerror(err));
		return err;
	}
	/*
	 * start the transfer when the buffer is almost full:
	 * (buffer_size / avail_min) * avail_min
	 */
	err = snd_pcm_sw_params_set_start_threshold(pcm, &swparams,
				(buffer_size / period_size) * period_size);
	if (err < 0) {
		SNDERR("Unable to set start threshold mode for %s: %s",
		       s, snd_strerror(err));
		return err;
	}
	/*
	 * allow the transfer when at least period_size samples can be
	 * processed
	 */
	err = snd_pcm_sw_params_set_avail_min(pcm, &swparams, period_size);
	if (err < 0) {
		SNDERR("Unable to set avail min for %s: %s",
		       s, snd_strerror(err));
		return err;
	}
	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(pcm, &swparams);
	if (err < 0) {
		SNDERR("Unable to set sw params for %s: %s",
		       s, snd_strerror(err));
		return err;
	}
	return 0;
}

/**
 * \brief Get the transfer size parameters in a simple way
 * \param pcm PCM handle
 * \param buffer_size PCM ring buffer size in frames
 * \param period_size PCM period size in frames
 * \return 0 on success otherwise a negative error code
 */
int snd_pcm_get_params(snd_pcm_t *pcm,
                       snd_pcm_uframes_t *buffer_size,
                       snd_pcm_uframes_t *period_size)
{
	snd_pcm_hw_params_t params = {0};
	int err;

	assert(pcm);
	err = snd_pcm_hw_params_current(pcm, &params);
	if (err < 0)
	        return err;
	err = INTERNAL(snd_pcm_hw_params_get_buffer_size)(&params, buffer_size);
	if (err < 0)
		return err;
	return INTERNAL(snd_pcm_hw_params_get_period_size)(&params, period_size,
							   NULL);
}
