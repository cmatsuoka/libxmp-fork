/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "mtm.h"

static int mtm_test (FILE *, char *, const int);
static int mtm_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info mtm_loader = {
    "MTM",
    "Multitracker",
    mtm_test,
    mtm_load
};

static int mtm_test(FILE *f, char *t, const int start)
{
    uint8 buf[4];

    if (fread(buf, 1, 4, f) < 4)
	return -1;
    if (memcmp(buf, "MTM", 3))
	return -1;
    if (buf[3] != 0x10)
	return -1;

    read_title(f, t, 20);

    return 0;
}


static int mtm_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &p->m;
    int i, j;
    struct mtm_file_header mfh;
    struct mtm_instrument_header mih;
    uint8 mt[192];
    uint16 mp[32];

    LOAD_INIT();

    fread(&mfh.magic, 3, 1, f);		/* "MTM" */
    mfh.version = read8(f);		/* MSN=major, LSN=minor */
    fread(&mfh.name, 20, 1, f);		/* ASCIIZ Module name */
    mfh.tracks = read16l(f);		/* Number of tracks saved */
    mfh.patterns = read8(f);		/* Number of patterns saved */
    mfh.modlen = read8(f);		/* Module length */
    mfh.extralen = read16l(f);		/* Length of the comment field */
    mfh.samples = read8(f);		/* Number of samples */
    mfh.attr = read8(f);		/* Always zero */
    mfh.rows = read8(f);		/* Number rows per track */
    mfh.channels = read8(f);		/* Number of tracks per pattern */
    fread(&mfh.pan, 32, 1, f);		/* Pan positions for each channel */

#if 0
    if (strncmp ((char *)mfh.magic, "MTM", 3))
	return -1;
#endif

    m->xxh->trk = mfh.tracks + 1;
    m->xxh->pat = mfh.patterns + 1;
    m->xxh->len = mfh.modlen + 1;
    m->xxh->ins = mfh.samples;
    m->xxh->smp = m->xxh->ins;
    m->xxh->chn = mfh.channels;
    m->xxh->tpo = 6;
    m->xxh->bpm = 125;

    strncpy(m->name, (char *)mfh.name, 20);
    set_type(m, "MTM (MultiTracker %d.%02d)",
				MSN(mfh.version), LSN(mfh.version));

    MODULE_INFO();

    INSTRUMENT_INIT();

    /* Read and convert instruments */
    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i].sub = calloc(sizeof (struct xxm_subinstrument), 1);

	fread(&mih.name, 22, 1, f);		/* Instrument name */
	mih.length = read32l(f);		/* Instrument length in bytes */
	mih.loop_start = read32l(f);		/* Sample loop start */
	mih.loopend = read32l(f);		/* Sample loop end */
	mih.finetune = read8(f);		/* Finetune */
	mih.volume = read8(f);			/* Playback volume */
	mih.attr = read8(f);			/* &0x01: 16bit sample */

	m->xxs[i].len = mih.length;
	m->xxi[i].nsm = mih.length > 0 ? 1 : 0;
	m->xxs[i].lps = mih.loop_start;
	m->xxs[i].lpe = mih.loopend;
	m->xxs[i].flg = m->xxs[i].lpe ? XMP_SAMPLE_LOOP : 0;	/* 1 == Forward loop */
	if (mfh.attr & 1) {
	    m->xxs[i].flg |= XMP_SAMPLE_16BIT;
	    m->xxs[i].len >>= 1;
	    m->xxs[i].lps >>= 1;
	    m->xxs[i].lpe >>= 1;
	}

	m->xxi[i].sub[0].vol = mih.volume;
	m->xxi[i].sub[0].fin = 0x80 + (int8)(mih.finetune << 4);
	m->xxi[i].sub[0].pan = 0x80;
	m->xxi[i].sub[0].sid = i;

	copy_adjust(m->xxi[i].name, mih.name, 22);

	_D(_D_INFO "[%2X] %-22.22s %04x%c%04x %04x %c V%02x F%+03d\n", i,
		m->xxi[i].name, m->xxs[i].len,
		m->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		m->xxs[i].lps, m->xxs[i].lpe,
		m->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		m->xxi[i].sub[0].vol, m->xxi[i].sub[0].fin - 0x80);
    }

    fread(m->xxo, 1, 128, f);

    PATTERN_INIT();

    _D(_D_INFO "Stored tracks: %d", m->xxh->trk - 1);

    for (i = 0; i < m->xxh->trk; i++) {
	m->xxt[i] = calloc (sizeof (struct xxm_track) +
	    sizeof (struct xxm_event) * mfh.rows, 1);
	m->xxt[i]->rows = mfh.rows;
	if (!i)
	    continue;
	fread (&mt, 3, 64, f);
	for (j = 0; j < 64; j++) {
	    if ((m->xxt[i]->event[j].note = mt[j * 3] >> 2))
		m->xxt[i]->event[j].note += 25;
	    m->xxt[i]->event[j].ins = ((mt[j * 3] & 0x3) << 4) + MSN (mt[j * 3 + 1]);
	    m->xxt[i]->event[j].fxt = LSN (mt[j * 3 + 1]);
	    m->xxt[i]->event[j].fxp = mt[j * 3 + 2];
	    if (m->xxt[i]->event[j].fxt > FX_TEMPO)
		m->xxt[i]->event[j].fxt = m->xxt[i]->event[j].fxp = 0;
	    /* Set pan effect translation */
	    if ((m->xxt[i]->event[j].fxt == FX_EXTENDED) &&
		(MSN (m->xxt[i]->event[j].fxp) == 0x8)) {
		m->xxt[i]->event[j].fxt = FX_SETPAN;
		m->xxt[i]->event[j].fxp <<= 4;
	    }
	}
    }

    /* Read patterns */
    _D(_D_INFO "Stored patterns: %d", m->xxh->pat - 1);

    for (i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
	for (j = 0; j < 32; j++)
	    mp[j] = read16l(f);
	for (j = 0; j < m->xxh->chn; j++)
	    m->xxp[i]->index[j] = mp[j];
    }

    /* Comments */
    fseek(f, mfh.extralen, SEEK_CUR);

    /* Read samples */
    _D(_D_INFO "Stored samples: %d", m->xxh->smp);

    for (i = 0; i < m->xxh->ins; i++) {
	xmp_drv_loadpatch(ctx, f, m->xxi[i].sub[0].sid,
	    XMP_SMP_UNS, &m->xxs[m->xxi[i].sub[0].sid], NULL);
    }

    for (i = 0; i < m->xxh->chn; i++)
	m->xxc[i].pan = mfh.pan[i] << 4;

    return 0;
}
