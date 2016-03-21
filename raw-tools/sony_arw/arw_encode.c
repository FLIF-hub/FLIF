#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void writebits(uint8_t *dst, int from, int len, uint16_t data)
{
	int bit;

	for(bit = 0; bit < len; bit++) {
		int val = (data & (1 << bit));
		int obyte = (from + bit) >> 3;
		int bitpos = (from+bit) & 0x07;

		if(val)
			dst[obyte] = dst[obyte] | (1 << bitpos);
		else
			dst[obyte] = dst[obyte] & ~(1 << bitpos);
	}
}

struct exception {
	uint32_t chunk;
	uint8_t minmax;
};

struct exception load_exception(FILE *f)
{
	struct exception rv;
	unsigned char buf[5];
	rv.chunk=0xffffffff;

	if(!f)
		return rv;

	if(fread(buf, 1, 5, f) != 5)
		return rv;

	rv.chunk = 0;
	rv.chunk |= buf[3];
	rv.chunk |= buf[2] << 8;
	rv.chunk |= buf[1] << 16;
	rv.chunk |= buf[0] << 24;
	rv.minmax = buf[4];

	return rv;
}

static inline int exception_valid(struct exception *e)
{
	return e->chunk != 0xffffffff;
}

uint32_t chunk_cnt=0;
struct exception exception;
FILE *excf=NULL;

void arw_encode(uint16_t *buf)
{
	uint8_t outbuf[16];
	uint16_t max=0, min=0xffff, imax, imin, sh;
	int i, pos;

	// find min/max
	if(exception_valid(&exception) && exception.chunk == chunk_cnt) {
		imin = exception.minmax >> 4;
		imax = exception.minmax & 0x0f;
		min = buf[imin];
		max = buf[imax];
		exception = load_exception(excf);
	} else {
		for(i = 0; i < 16; i++) {
			if(buf[i] >= max) {
				max = buf[i];
				imax = i;
			}
			if(buf[i] < min) {
				min = buf[i];
				imin = i;
			}
		}
	}

	// find shift
	for(sh=0; sh < 4 && 0x80 << sh <= max-min; sh++);

	writebits(outbuf, 0, 11, max);
	writebits(outbuf, 11, 11, min);
	writebits(outbuf, 22, 4, imax);
	writebits(outbuf, 26, 4, imin);

	pos = 30;
	for(i = 0; i < 16; i++) {
		if(i != imin && i != imax) {
			writebits(outbuf, pos, 7, (buf[i]-min) >> sh);
			pos += 7;
		}
	}

	fwrite(outbuf, 1, 16, stdout);
	chunk_cnt++;
}

void flush_arw(uint16_t *outbuf, int *outcnt)
{
    if(*outcnt >= 32) {
		uint16_t arwbuf[32];
		int arwbuf_cnt = 0;

        int j;
        for(j = 0; j < 32; j+=2) {
			arwbuf[arwbuf_cnt++] = outbuf[j];
        }
        for(j = 1; j < 32; j+=2) {
			arwbuf[arwbuf_cnt++] = outbuf[j];
        }

		arw_encode(arwbuf);
		arw_encode(arwbuf+16);

        *outcnt = 0;
    }
}

void write_zeroes(long headerlen)
{
	long our_len = headerlen + chunk_cnt * 16;
	long shouldbe = (our_len / 0x8000) * 0x8000 + 0x8000;
	long missing = shouldbe - our_len;

	char *buf;
	buf = calloc(missing, 1);
	if(!buf) {
		fprintf(stderr, "woops, can't alloc zeroes\n");
	} else {
		fwrite(buf, 1, missing, stdout);
	}
}

int main(int argc, char **argv)
{
	long width, headerlen;
	uint16_t *buf;
	uint16_t arw_buf[32];
	int arw_buf_cnt = 0;

	if(argc < 3) {
		fprintf(stderr, "need at least a width, a headerlen, and possibly an exceptions file\n");
		return 1;
	}

	width = atol(argv[1]);
	headerlen = atol(argv[2]);

	buf = malloc(sizeof(uint16_t) * width * 4);
	if(!buf) {
		fprintf(stderr, "alloc error\n");
		return 1;
	}

	if(argc > 3)
		excf = fopen(argv[3], "r");

	exception = load_exception(excf);

	while(!feof(stdin)) {
        size_t rv;
        int i;
        if((rv=fread(buf, sizeof(uint16_t), width*4, stdin)) != width*4) {
            if(rv != 0)
				fprintf(stderr, "read only %u\n", rv);
            continue;
        }

        for(i = 0; i < width * 4; i += 4) {
			arw_buf[arw_buf_cnt++] = buf[i] >> 5;
			arw_buf[arw_buf_cnt++] = buf[i+1] >> 5;
            flush_arw(arw_buf, &arw_buf_cnt);
        }
        for(i = 0; i < width * 4; i += 4) {
			arw_buf[arw_buf_cnt++] = buf[i+3] >> 5;
			arw_buf[arw_buf_cnt++] = buf[i+2] >> 5;
            flush_arw(arw_buf, &arw_buf_cnt);
        }
	}

	free(buf);

	write_zeroes(headerlen);

	return 0;
}
