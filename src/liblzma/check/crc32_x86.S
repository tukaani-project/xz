/*
 * Speed-optimized CRC32 using slicing-by-eight algorithm
 * Instruction set: i386
 * Optimized for:   i686
 *
 * This code has been put into the public domain by its authors:
 * Original code by Igor Pavlov <http://7-zip.org/>
 * Position-independent version by Lasse Collin <lasse.collin@tukaani.org>
 *
 * This code needs lzma_crc32_table, which can be created using the
 * following C code:

uint32_t lzma_crc32_table[8][256];

void
init_table(void)
{
	// IEEE-802.3 (CRC32)
	static const uint32_t poly32 = UINT32_C(0xEDB88320);

	// Castagnoli (CRC32C)
	// static const uint32_t poly32 = UINT32_C(0x82F63B78);

	// Koopman
	// static const uint32_t poly32 = UINT32_C(0xEB31D82E);

	for (size_t s = 0; s < 8; ++s) {
		for (size_t b = 0; b < 256; ++b) {
			uint32_t r = s == 0 ? b : lzma_crc32_table[s - 1][b];

			for (size_t i = 0; i < 8; ++i) {
				if (r & 1)
					r = (r >> 1) ^ poly32;
				else
					r >>= 1;
			}

			lzma_crc32_table[s][b] = r;
		}
	}
}

 * The prototype of the CRC32 function:
 * extern uint32_t lzma_crc32(const uint8_t *buf, size_t size, uint32_t crc);
 */

	.text
	.globl	lzma_crc32
	.type	lzma_crc32, @function

	.align	16
lzma_crc32:
	/*
	 * Register usage:
	 * %eax crc
	 * %esi buf
	 * %edi size or buf + size
	 * %ebx lzma_crc32_table
	 * %ebp Table index
	 * %ecx Temporary
	 * %edx Temporary
	 */
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	movl	0x14(%esp), %esi /* buf */
	movl	0x18(%esp), %edi /* size */
	movl	0x1C(%esp), %eax /* crc */

	/*
	 * Store the address of lzma_crc32_table to %ebx. This is needed to
	 * get position-independent code (PIC).
	 */
	call	.L_PIC
.L_PIC:
	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-.L_PIC], %ebx
	movl	lzma_crc32_table@GOT(%ebx), %ebx

	/* Complement the initial value. */
	notl	%eax

	.align	16
.L_align:
	/*
	 * Check if there is enough input to use slicing-by-eight.
	 * We need 16 bytes, because the loop pre-reads eight bytes.
	 */
	cmpl	$16, %edi
	jl	.L_rest

	/* Check if we have reached alignment of eight bytes. */
	testl	$7, %esi
	jz	.L_slice

	/* Calculate CRC of the next input byte. */
	movzbl	(%esi), %ebp
	incl	%esi
	movzbl	%al, %ecx
	xorl	%ecx, %ebp
	shrl	$8, %eax
	xorl	(%ebx, %ebp, 4), %eax
	decl	%edi
	jmp	.L_align

	.align	4
.L_slice:
	/*
	 * If we get here, there's at least 16 bytes of aligned input
	 * available. Make %edi multiple of eight bytes. Store the possible
	 * remainder over the "size" variable in the argument stack.
	 */
	movl	%edi, 0x18(%esp)
	andl	$-8, %edi
	subl	%edi, 0x18(%esp)

	/*
	 * Let %edi be buf + size - 8 while running the main loop. This way
	 * we can compare for equality to determine when exit the loop.
	 */
	addl	%esi, %edi
	subl	$8, %edi

	/* Read in the first eight aligned bytes. */
	xorl	(%esi), %eax
	movl	4(%esi), %ecx
	movzbl	%cl, %ebp

.L_loop:
	movl	0x0C00(%ebx, %ebp, 4), %edx
	movzbl	%ch, %ebp
	xorl	0x0800(%ebx, %ebp, 4), %edx
	shrl	$16, %ecx
	xorl	8(%esi), %edx
	movzbl	%cl, %ebp
	xorl	0x0400(%ebx, %ebp, 4), %edx
	movzbl	%ch, %ebp
	xorl	(%ebx, %ebp, 4), %edx
	movzbl	%al, %ebp

	/*
	 * Read the next four bytes, for which the CRC is calculated
	 * on the next interation of the loop.
	 */
	movl	12(%esi), %ecx

	xorl	0x1C00(%ebx, %ebp, 4), %edx
	movzbl	%ah, %ebp
	shrl	$16, %eax
	xorl	0x1800(%ebx, %ebp, 4), %edx
	movzbl	%ah, %ebp
	movzbl	%al, %eax
	movl	0x1400(%ebx, %eax, 4), %eax
	addl	$8, %esi
	xorl	%edx, %eax
	xorl	0x1000(%ebx, %ebp, 4), %eax

	/* Check for end of aligned input. */
	cmpl	%edi, %esi
	movzbl	%cl, %ebp
	jne	.L_loop

	/*
	 * Process the remaining eight bytes, which we have already
	 * copied to %ecx and %edx.
	 */
	movl	0x0C00(%ebx, %ebp, 4), %edx
	movzbl	%ch, %ebp
	xorl	0x0800(%ebx, %ebp, 4), %edx
	shrl	$16, %ecx
	movzbl	%cl, %ebp
	xorl	0x0400(%ebx, %ebp, 4), %edx
	movzbl	%ch, %ebp
	xorl	(%ebx, %ebp, 4), %edx
	movzbl	%al, %ebp

	xorl	0x1C00(%ebx, %ebp, 4), %edx
	movzbl	%ah, %ebp
	shrl	$16, %eax
	xorl	0x1800(%ebx, %ebp, 4), %edx
	movzbl	%ah, %ebp
	movzbl	%al, %eax
	movl	0x1400(%ebx, %eax, 4), %eax
	addl	$8, %esi
	xorl	%edx, %eax
	xorl	0x1000(%ebx, %ebp, 4), %eax

	/* Copy the number of remaining bytes to %edi. */
	movl	0x18(%esp), %edi

.L_rest:
	/* Check for end of input. */
	testl	%edi, %edi
	jz	.L_return

	/* Calculate CRC of the next input byte. */
	movzbl	(%esi), %ebp
	incl	%esi
	movzbl	%al, %ecx
	xorl	%ecx, %ebp
	shrl	$8, %eax
	xorl	(%ebx, %ebp, 4), %eax
	decl	%edi
	jmp	.L_rest

.L_return:
	/* Complement the final value. */
	notl	%eax

	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

	.size	lzma_crc32, .-lzma_crc32

/*
 * This is needed to support non-executable stack. It's ugly to
 * use __linux__ here, but I don't know a way to detect when
 * we are using GNU assembler.
 */
#if defined(__ELF__) && defined(__linux__)
	.section	.note.GNU-stack,"",@progbits
#endif
