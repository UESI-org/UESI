#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include "stdio_internal.h"

#define POS_ERR	(-(fpos_t)1)

extern int __sflush(FILE *);
extern fpos_t __sseek(void *, fpos_t, int);

int
fseek(FILE *fp, long offset, int whence)
{
	return fseeko(fp, (off_t)offset, whence);
}

int
fseeko(FILE *fp, off_t offset, int whence)
{
	fpos_t (*seekfn)(void *, fpos_t, int);
	fpos_t target, curoff;
	size_t n;
	struct stat st;
	int havepos;

	/* Check for valid whence argument */
	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Have to be able to seek.
	 */
	if ((seekfn = fp->_seek) == NULL) {
		errno = ESPIPE;			/* historic practice */
		return -1;
	}

	/*
	 * Change any SEEK_CUR to SEEK_SET, and check `whence' argument.
	 * After this, whence is either SEEK_SET or SEEK_END.
	 */
	switch (whence) {
	case SEEK_CUR:
		/*
		 * In order to seek relative to the current stream offset,
		 * we have to first find the current stream offset a la
		 * ftell (see ftell for details).
		 */
		__sflush(fp);	/* may adjust seek offset on append stream */
		if (fp->_flags & __SOFF)
			curoff = fp->_offset;
		else {
			curoff = (*seekfn)(fp->_cookie, (fpos_t)0, SEEK_CUR);
			if (curoff == -1L)
				return -1;
		}
		if (fp->_flags & __SRD) {
			curoff -= fp->_r;
			if (HASUB(fp))
				curoff -= fp->_ur;
		} else if (fp->_flags & __SWR && fp->_p != NULL)
			curoff += fp->_p - fp->_bf._base;

		offset += curoff;
		whence = SEEK_SET;
		havepos = 1;
		break;

	case SEEK_SET:
	case SEEK_END:
		curoff = 0;		/* XXX just to keep gcc quiet */
		havepos = 0;
		break;

	default:
		errno = EINVAL;
		return -1;
	}

	/*
	 * Can only optimise if:
	 *	reading (and not reading-and-writing);
	 *	not unbuffered; and
	 *	this is a `regular' Unix file (and hence seekfn==__sseek).
	 * We must check __NBF first, because it is possible to have __NBF
	 * and __SOPT both set.
	 */
	if (fp->_bf._base == NULL)
		__smakebuf(fp);
	if (fp->_flags & (__SWR | __SRW | __SNBF | __SNPT))
		goto dumb;
	if ((fp->_flags & __SOPT) == 0) {
		if (seekfn != __sseek ||
		    fp->_file < 0 || fstat(fp->_file, &st) ||
		    !S_ISREG(st.st_mode)) {
			fp->_flags |= __SNPT;
			goto dumb;
		}
		fp->_blksize = st.st_blksize;
		fp->_flags |= __SOPT;
	}

	/*
	 * We are reading; we can try to optimise.
	 * Figure out where we are going and where we are now.
	 */
	if (whence == SEEK_SET)
		target = offset;
	else {
		if (fstat(fp->_file, &st))
			goto dumb;
		target = st.st_size + offset;
	}

	if (!havepos) {
		if (fp->_flags & __SOFF)
			curoff = fp->_offset;
		else {
			curoff = (*seekfn)(fp->_cookie, (fpos_t)0, SEEK_CUR);
			if (curoff == POS_ERR)
				goto dumb;
		}
		curoff -= fp->_r;
		if (HASUB(fp))
			curoff -= fp->_ur;
	}

	/*
	 * Compute the number of bytes in the input buffer (pretending
	 * that any ungetc() input has been discarded).  Adjust current
	 * offset backwards by this count so that it represents the
	 * file offset for the first byte in the current input buffer.
	 */
	if (HASUB(fp)) {
		curoff += fp->_r;	/* kill off ungetc */
		n = fp->_up - fp->_bf._base;
		curoff -= n;
		n += fp->_ur;
	} else {
		n = fp->_p - fp->_bf._base;
		curoff -= n;
		n += fp->_r;
	}

	/*
	 * If the target offset is within the current buffer,
	 * simply adjust the pointers, clear EOF, undo ungetc(),
	 * and return.  (If the buffer was modified, we have to
	 * skip this; see fgetln.c.)
	 */
	if ((fp->_flags & __SMOD) == 0 &&
	    target >= curoff && target < curoff + n) {
		int o = target - curoff;

		fp->_p = fp->_bf._base + o;
		fp->_r = n - o;
		if (HASUB(fp))
			FREEUB(fp);
		fp->_flags &= ~__SEOF;
		return 0;
	}

	/*
	 * The place we want to get to is not within the current buffer,
	 * but we can still be smart about it.  Simply discard the current
	 * buffer contents and seek to the new position.  Avoid the extra
	 * function call if we can seek to the exact location we want.
	 */
	if (HASUB(fp))
		FREEUB(fp);
	fp->_p = fp->_bf._base;
	fp->_r = 0;
	fp->_flags &= ~__SEOF;

	/* Try to seek directly to where we want (offset is in bytes) */
	if ((*seekfn)(fp->_cookie, target, SEEK_SET) == target) {
		fp->_flags |= __SOFF;
		fp->_offset = target;
		return 0;
	}

dumb:
	/*
	 * We get here if we cannot optimise the seek ... just
	 * do it.  Allow the seek function to change fp->_bf._base.
	 */
	if (__sflush(fp) || (*seekfn)(fp->_cookie, (fpos_t)offset, whence) == POS_ERR)
		return -1;
	/* success: clear EOF indicator and discard ungetc() data */
	if (HASUB(fp))
		FREEUB(fp);
	fp->_p = fp->_bf._base;
	fp->_r = 0;
	/* fp->_w = 0; */	/* unnecessary (I think...) */
	fp->_flags &= ~__SEOF;
	return 0;
}

long
ftell(FILE *fp)
{
	off_t pos;

	pos = ftello(fp);
	if (pos > LONG_MAX) {
		errno = EOVERFLOW;
		return -1;
	}
	return (long)pos;
}

off_t
ftello(FILE *fp)
{
	fpos_t pos;

	if (fp->_seek == NULL) {
		errno = ESPIPE;			/* historic practice */
		return -1;
	}

	/*
	 * Find offset of underlying I/O object, then
	 * adjust for buffered bytes.
	 */
	__sflush(fp);				/* may adjust seek offset on append stream */
	if (fp->_flags & __SOFF)
		pos = fp->_offset;
	else {
		pos = (*fp->_seek)(fp->_cookie, (fpos_t)0, SEEK_CUR);
		if (pos == -1L)
			return -1;
	}
	if (fp->_flags & __SRD) {
		/*
		 * Reading.  Any unread characters (including
		 * those from ungetc) cause the position to be
		 * smaller than that in the underlying object.
		 */
		pos -= fp->_r;
		if (HASUB(fp))
			pos -= fp->_ur;
	} else if (fp->_flags & __SWR && fp->_p != NULL) {
		/*
		 * Writing.  Any buffered characters cause the
		 * position to be greater than that in the
		 * underlying object.
		 */
		pos += fp->_p - fp->_bf._base;
	}
	return pos;
}

void
rewind(FILE *fp)
{
	(void)fseek(fp, 0L, SEEK_SET);
	clearerr(fp);
}

int
fgetpos(FILE *fp, fpos_t *pos)
{
	*pos = ftello(fp);
	if (*pos == -1)
		return -1;
	return 0;
}

int
fsetpos(FILE *fp, const fpos_t *pos)
{
	return fseeko(fp, *pos, SEEK_SET);
}
