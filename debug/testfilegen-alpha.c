// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       testfilegen-alpha.c
/// \brief      Generates uncompressed test file for the DEC Alpha filter
//
//  Author:     Matt Turner
//
///////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stdio.h>

#define REG_RA 26
#define REG_PV 27
#define REG_GP 29

// bis $31,$31,$31
#define UNOP 0x47FF041F


static void
put32le(uint32_t v)
{
	putchar((v >> 0) & 0xFF);
	putchar((v >> 8) & 0xFF);
	putchar((v >> 16) & 0xFF);
	putchar((v >> 24) & 0xFF);
}


static void
putbsr(uint32_t disp)
{
	put32le((0x34U << 26) | ((uint32_t)REG_RA << 21)
			| (disp & 0x001FFFFF));
}


static void
putbr(uint32_t disp)
{
	put32le((0x30U << 26) | (31U << 21) | (disp & 0x001FFFFF));
}


static void
putbranch(uint32_t op, uint32_t ra, uint32_t disp)
{
	put32le((op << 26) | (ra << 21) | (disp & 0x001FFFFF));
}


static void
putldah(uint32_t base, uint32_t high)
{
	put32le((0x09U << 26) | ((uint32_t)REG_GP << 21)
			| (base << 16) | (high & 0xFFFF));
}


static void
putlda(uint32_t low)
{
	put32le((0x08U << 26) | ((uint32_t)REG_GP << 21)
			| ((uint32_t)REG_GP << 16) | (low & 0xFFFF));
}


/// ldah/lda pair with the lda gap instructions after the ldah
static void
putgpdisp(unsigned gap, uint32_t base, uint32_t high, uint32_t low)
{
	putldah(base, high);

	for (unsigned i = 1; i < gap; ++i)
		put32le(UNOP);

	putlda(low);
}


extern int
main(void)
{
	static const uint32_t bsr_disps[] = {
		0, 1, 2, 3,
		0x1FFFFF, 0x1FFFFE, 0x1FFFFD, 0x1FFFFC,
		0x100000, 0x0FFFFF, 0x000FFF, 0x001000,
	};

	// Also BR relative to the start of the file because no "ldah"
	// with $pv as the base register has been seen yet.
	for (unsigned rep = 0; rep < 8; ++rep)
		for (unsigned i = 0; i < sizeof(bsr_disps)
				/ sizeof(bsr_disps[0]); ++i) {
			putbsr(bsr_disps[i] + rep);
			putbr(bsr_disps[i] + rep);
		}

	static const uint32_t lows[] = {
		0x0000, 0x0001, 0x7FFE, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF,
	};

	static const uint32_t highs[] = {
		0x0000, 0x0001, 0x7FFF, 0x8000, 0xFFFF,
	};

	static const uint32_t bases[] = { REG_RA, REG_PV, REG_GP };

	for (unsigned gap = 1; gap <= 4; ++gap)
		for (unsigned b = 0; b < 3; ++b)
			for (unsigned h = 0; h < sizeof(highs)
					/ sizeof(highs[0]); ++h)
				for (unsigned l = 0; l < sizeof(lows)
						/ sizeof(lows[0]); ++l)
					putgpdisp(gap, bases[b],
							highs[h], lows[l]);

	// BR relative to the last pair with $pv as the base register.
	// A pair with $ra as the base register doesn't affect BR.
	putgpdisp(1, REG_PV, 0x1234, 0x5678);
	for (unsigned i = 0; i < sizeof(bsr_disps)
			/ sizeof(bsr_disps[0]); ++i) {
		putbr(bsr_disps[i]);
		putgpdisp(2, REG_RA, 0x1234, 0x5678);
		putbr(bsr_disps[i]);
	}

	// Clang's "br $gp,.+4" is not converted, but the pair after it is,
	// and BR is then relative to that pair.
	putbranch(0x30, REG_GP, 0);
	putgpdisp(1, REG_GP, 0x1234, 0x5678);
	for (unsigned i = 0; i < sizeof(bsr_disps)
			/ sizeof(bsr_disps[0]); ++i)
		putbr(bsr_disps[i]);

	// Not converted: BSR with a register other than $ra and
	// BR with a register other than $zero.
	putbranch(0x34, 0, 0x1234);
	putbranch(0x34, 31, 0x1234);
	putbranch(0x30, 0, 0x1234);
	putbranch(0x30, REG_RA, 0x1234);

	// Not converted: a gap of five and a base other than $ra, $pv,
	// or $gp.
	putgpdisp(5, REG_RA, 0x1234, 0x5678);
	putgpdisp(1, 1, 0x1234, 0x5678);

	// The first "ldah" gets the "lda" and the second is not converted.
	putldah(REG_RA, 0x1234);
	putgpdisp(1, REG_PV, 0x1234, 0x5678);

	// A BSR between the halves of a pair is not converted.
	putldah(REG_PV, 0x1234);
	putbsr(0x1234);
	putlda(0x5678);

	// A BR between the halves of a pair is not converted.
	putldah(REG_RA, 0x1234);
	putbr(0x1234);
	putlda(0x5678);

	// Padding so that the last pair is within the filter's reach.
	for (unsigned i = 0; i < 5; ++i)
		put32le(UNOP);

	return 0;
}
