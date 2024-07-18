/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define u8  unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long long

#define uint unsigned int

void
mpumul_64_64 (u64 m1, u64 m2, u64 ans[2])
{
	u64 low, high;

	low = m1 * m2;
	asm ("umulh %0, %1, %2" : "=r"(high) : "r"(m1), "r"(m2) :);
	ans[0] = low;
	ans[1] = high;
}

u32
mpudiv_128_32 (u64 d1[2], u32 d2, u64 quotient[2])
{
	/*
	 * Let x = 2^32, we can write v_128 in the mx^3 + nx^2 + ox + p.
	 * We want to divide v_128 with y. We rearrange the formula in terms
	 * of y.
	 *
	 * v_128 = mx^3 + nx^2 + ox + p
	 *
	 * Let m = ay + r_a
	 *
	 * v_128 = ayx^3 + (r_a)x^3 + nx^2 + ox + p
	 * v_128 = ayx^3 + ((r_a)x + n)x^2 + ox + p
	 *
	 * Let (r_a)x + n = by + r_b
	 *
	 * v_128 = ayx^3 + byx^2 + (r_b)x^2 + ox + p
	 * v_128 = ayx^3 + byx^2 + ((r_b)x + o)x + p
	 *
	 * Let (r_b)x + o = cy + r_c
	 *
	 * v_128 = ayx^3 + byx^2 + cyx + (r_c)x + p
	 *
	 * Let (r_c)x + p = dy + r_d
	 *
	 * v_128 = ayx^3 + byx^2 + cyx + dy + r_d
	 *
	 * Divide v_128 by y gives us
	 *
	 * v_128 / y = ax^3 + bx^2 + cx + d + (r_d / y)
	 *
	 * Where
	 *
	 * a = m / y
	 * r_a = m % y
	 *
	 * b = ((r_a)x + n) / y
	 * r_b = ((r_a)x + n) % y
	 *
	 * c = ((r_b)x + o) / y
	 * r_c = ((r_b)x + o) % y
	 *
	 * d = ((r_c)x + p) / y
	 * r_d = ((r_c)x + p) % y
	 *
	 * Since x = 2^32, we can use << 32 instead of multiplication.
	 *
	 * As a result, we do calculation according to the above formula, and
	 * return quotient and reminder accordingly.
	 *
	 * quotient[1] = a << 32 | b
	 * quotient[0] = c << 32 | d
	 * reminder = r_d
	 *
	 */

	u64 m = d1[1] >> 32;
	u64 n = d1[1] & 0xFFFFFFFF;
	u64 o = d1[0] >> 32;
	u64 p = d1[0] & 0xFFFFFFFF;

	u64 a = m / d2;
	u64 r_a_x = (m % d2) << 32;

	u64 b = (r_a_x + n) / d2;
	u64 r_b_x = ((r_a_x + n) % d2) << 32;

	u64 c = (r_b_x + o) / d2;
	u64 r_c_x = ((r_b_x + o) % d2) << 32;

	u64 d = (r_c_x + p) / d2;
	u64 r_d = (r_c_x + p) % d2;

	quotient[1] = a << 32 | b;
	quotient[0] = c << 32 | d;

	return r_d;
}

static inline u64
add_fold_carry_u64 (u64 a, u64 b)
{
	u64 sum;

	sum = a + b;
	sum += (sum < a);

	return sum;
}

static inline u32
add_fold_carry_u32 (u32 a, u32 b)
{
	u32 sum;

	sum = a + b;
	sum += (sum < a);

	return sum;
}

static inline u16
add_fold_carry_u16 (u16 a, u16 b)
{
	u16 sum;

	sum = a + b;
	sum += (sum < a);

	return sum;
}

/*
 * The implementation is based on RFC 1071. However, we operate on 16 bytes
 * at a time instead of 2 bytes at a time. Note that modern compilers can
 * optimize add_fold_carry*() to make use of carry flag resulting in efficient
 * code for us.
 */
u16
ipchecksum (void *buf, uint len)
{
	const void *b;
	u64 sum64;
	u32 sum32;
	u16 sum16;

	sum64 = 0;
	b = buf;

	/*
	 * Compiler can optimize the code to use load 16 bytes at a time if
	 * the target architecture, like AArch64, supports. This is efficient
	 * theoretically.
	 */
	while (len >= sizeof (u64) * 2) {
		const u64 *d = b;
		sum64 = add_fold_carry_u64 (sum64, d[0]);
		sum64 = add_fold_carry_u64 (sum64, d[1]);
		len -= (sizeof *d * 2);
		b += (sizeof *d * 2);
	}

	if (len >= sizeof (u64)) {
		const u64 *d = b;
		sum64 = add_fold_carry_u64 (sum64, *d);
		len -= sizeof *d;
		b += sizeof *d;
	}

	if (len >= sizeof (u32)) {
		const u32 *d = b;
		sum64 = add_fold_carry_u64 (sum64, *d);
		len -= sizeof *d;
		b += sizeof *d;
	}

	if (len >= sizeof (u16)) {
		const u16 *d = b;
		sum64 = add_fold_carry_u64 (sum64, *d);
		len -= sizeof *d;
		b += sizeof *d;
	}

	if (len >= sizeof (u8)) {
		const u8 *d = b;
		sum64 = add_fold_carry_u64 (sum64, *d);
		/* No need to subtract len and increase b anymore */
	}

	/* Fold the sum result to 2 bytes */
	sum32 = add_fold_carry_u32 (sum64 >> 32, sum64 & 0xFFFFFFFF);
	sum16 = add_fold_carry_u16 (sum32 >> 16, sum32 & 0xFFFF);

	/* Apply one's complement at the end */
	return ~sum16;
}
