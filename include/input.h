/**
 * \file <alsa/input.h>
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@suse.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 1998-2001
 *
 * Application interface library for the ALSA driver
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

#ifndef __ALSA_INPUT_H
#define __ALSA_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Input Input Interface
 *  Input Interface
 *  \{
 */

/** Input handle */
typedef struct _snd_input snd_input_t;

/** Input type */
typedef enum _snd_input_type {
	/** Input from a stdio stream */
	SND_INPUT_STDIO,
	/** Input from a memory buffer */
	SND_INPUT_BUFFER,
} snd_input_type_t;

int snd_input_stdio_open(snd_input_t **inputp, const char *file, const char *mode);
int snd_input_stdio_attach(snd_input_t **inputp, FILE *fp, int _close);
int snd_input_buffer_open(snd_input_t **inputp, const char *buffer, ssize_t size);
int snd_input_close(snd_input_t *input);
int snd_input_scanf(snd_input_t *input, const char *format, ...) __attribute__ ((format (scanf, 2, 3)));
char *snd_input_gets(snd_input_t *input, char *str, size_t size);
int snd_input_getc(snd_input_t *input);
int snd_input_ungetc(snd_input_t *input, int c);

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_INPUT_H */
