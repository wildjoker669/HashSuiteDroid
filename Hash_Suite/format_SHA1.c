// This file is part of Hash Suite password cracker,
// Copyright (c) 2014-2015 by Alain Espinosa. See LICENSE.

#include "common.h"
#include "attack.h"

//Initial values
#define INIT_A 0x67452301
#define INIT_B 0xefcdab89
#define INIT_C 0x98badcfe
#define INIT_D 0x10325476
#define INIT_E 0xC3D2E1F0

#define SQRT_2 0x5a827999
#define SQRT_3 0x6ed9eba1

#define BINARY_SIZE			20
#define NTLM_MAX_KEY_LENGHT	27

// This is MIME Base64 (as opposed to crypt(3) encoding found in common.[ch])
PRIVATE void base64_unmap(char *in_block)
{
	int i;
	char *c;

	for (i = 0; i < 4; i++)
	{
		c = in_block + i;

		if (*c >= 'A' && *c <= 'Z')
		{
			*c -= 'A';
			continue;
		}

		if (*c >= 'a' && *c <= 'z')
		{
			*c -= 'a';
			*c += 26;
			continue;
		}

		if (*c == '+')
		{
			*c = 62;
			continue;
		}

		if (*c == '/')
		{
			*c = 63;
			continue;
		}

		if (*c >= '0' && *c <= '9')
		{
			*c -= '0';
			*c += 52;
			continue;
		}
		/* ignore trailing trash (if there were no '=' values */
		*c = 0;
	}
}
PUBLIC int base64_decode(const char *in, int inlen, char *out)
{
	int i;
	const char *in_block;
	char *out_block;
	char temp[4];

	out_block = out;
	in_block = in;

	for (i = 0; i < inlen; i += 4)
	{
		if (*in_block == '=')
			return 0;

		memcpy(temp, in_block, 4);
		memset(out_block, 0, 3);
		base64_unmap(temp);

		out_block[0] = ((temp[0] << 2) & 0xfc) | ((temp[1] >> 4) & 3);
		out_block[1] = ((temp[1] << 4) & 0xf0) | ((temp[2] >> 2) & 0xf);
		out_block[2] = ((temp[2] << 6) & 0xc0) | ((temp[3]) & 0x3f);

		out_block += 3;
		if (in_block[3] == '=')
			return 0;
		in_block += 4;
	}

	return 0;
}
PRIVATE int valid_mime_base64_string(char *in, uint32_t len)
{
	for (uint32_t i = 0; i < (len-1); i++)
	{
		int char_is_valid = FALSE;

		if (in[i] >= 'A' && in[i] <= 'Z') char_is_valid = TRUE;
		if (in[i] >= 'a' && in[i] <= 'z') char_is_valid = TRUE;
		if (in[i] == '+') char_is_valid = TRUE;
		if (in[i] == '/') char_is_valid = TRUE;
		if (in[i] >= '0' && in[i] <= '9') char_is_valid = TRUE;

		if (!char_is_valid) return FALSE;
	}

	return (in[len - 1] == '=');
}

PRIVATE int is_valid(char* user_name, char* sha1, char* unused, char* unused1)
{
	if (user_name)
	{
		if (sha1 && valid_hex_string(sha1, 40))
			return TRUE;

		if (!sha1 && !memcmp(user_name, "$dynamic_26$", 12) && valid_hex_string(user_name + 12, 40))
			return TRUE;

		// Netscape LDAP {SHA} also know as nsldap
		if (sha1 && !_strnicmp(sha1, "{SHA}", 5) && valid_mime_base64_string(sha1 + 5, 28))
			return TRUE;
	}

	return FALSE;
}

PRIVATE void add_hash_from_line(ImportParam* param, char* user_name, char* sha1, char* unused, char* unused1, sqlite3_int64 tag_id)
{
	if (user_name)
	{
		if (sha1 && valid_hex_string(sha1, 40))
			insert_hash_account(param, user_name, _strupr(sha1), SHA1_INDEX, tag_id);

		if (!sha1 && !memcmp(user_name, "$dynamic_26$", 12) && valid_hex_string(user_name + 12, 40))
			insert_hash_account(param, "user", _strupr(user_name + 12), SHA1_INDEX, tag_id);

		// Netscape LDAP {SHA} also know as nsldap
		if (sha1 && !_strnicmp(sha1, "{SHA}", 5) && valid_mime_base64_string(sha1 + 5, 28))
		{
			uint32_t sha1_bin[5];
			char hex_sha1[41];

			memset(sha1_bin, 0, sizeof(sha1_bin));
			base64_decode(sha1 + 5, 28, (char*)sha1_bin);
			swap_endianness_array(sha1_bin, 5);
			for (uint32_t i = 0; i < 5; i++)
				sprintf(hex_sha1 + 8 * i, "%08X", sha1_bin[i]);

			insert_hash_account(param, user_name, hex_sha1, SHA1_INDEX, tag_id);
		}
	}
}
PRIVATE unsigned int get_binary(const unsigned char* ciphertext, void* binary, void* salt)
{
	unsigned int* out = (unsigned int*)binary;

	for (unsigned int i = 0; i < 5; i++)
	{
		unsigned int temp = (hex_to_num[ciphertext[i * 8 + 0]]) << 28;
		temp |= (hex_to_num[ciphertext[i * 8 + 1]]) << 24;
		
		temp |= (hex_to_num[ciphertext[i * 8 + 2]]) << 20;
		temp |= (hex_to_num[ciphertext[i * 8 + 3]]) << 16;
		
		temp |= (hex_to_num[ciphertext[i * 8 + 4]]) << 12;
		temp |= (hex_to_num[ciphertext[i * 8 + 5]]) << 8;
		
		temp |= (hex_to_num[ciphertext[i * 8 + 6]]) << 4;
		temp |= (hex_to_num[ciphertext[i * 8 + 7]]) << 0;

		out[i] = temp;
	}

	out[0] -= INIT_A;
	out[1] -= INIT_B;
	out[2] -= INIT_C;
	out[3] -= INIT_D;
	out[4] -= INIT_E;

	// C
	out[2] = rotate(out[2], 32-30);
	// A
	out[0] -= rotate(out[1], 5) + (out[2] ^ out[3] ^ out[4]) + 0xCA62C1D6;
	// D
	out[3] = rotate(out[3], 32-30);
	// B
	out[1] -= rotate(out[2], 5) + 0xCA62C1D6;
	//E
	out[4] = rotate(out[4], 32-30);

	return out[0];
}

#ifdef HS_ARM
	#define NT_NUM_KEYS		    128
#endif

#ifdef HS_X86
	#define NT_NUM_KEYS		    256
#endif

PRIVATE void crypt_utf8_coalesc_protocol_body(CryptParam* param, crypt_kernel_asm_func* crypt_kernel_asm)
{
	unsigned int* nt_buffer = (unsigned int*)_aligned_malloc((8+16+3) * sizeof(unsigned int) * NT_NUM_KEYS, 32);

	unsigned int* unpacked_W  = nt_buffer  + 8 * NT_NUM_KEYS;
	unsigned int* unpacked_as = unpacked_W + 2 * NT_NUM_KEYS;
	unsigned int* unpacked_bs = unpacked_W + 4 * NT_NUM_KEYS;
	unsigned int* unpacked_cs = unpacked_W + 9 * NT_NUM_KEYS;
	unsigned int* unpacked_ds = unpacked_W + 16 * NT_NUM_KEYS;
	unsigned int* unpacked_es = unpacked_W + 17 * NT_NUM_KEYS;
	unsigned int* indexs	  = unpacked_W + 18 * NT_NUM_KEYS;

	unsigned char key[MAX_KEY_LENGHT_SMALL];

	memset(nt_buffer, 0, 8 * sizeof(unsigned int)* NT_NUM_KEYS);
	memset(key, 0, sizeof(key));

	while (continue_attack && param->gen(nt_buffer, NT_NUM_KEYS, param->thread_id))
	{
		crypt_kernel_asm(nt_buffer, bit_table, size_bit_table);

		for (unsigned int i = 0; i < NT_NUM_KEYS; i++)
			if (indexs[i])
			{
				unsigned int indx = table[unpacked_as[i] & size_table];
				// Partial match
				while (indx != NO_ELEM)
				{
					unsigned int aa = unpacked_as[i], bb, cc, dd, ee, W11, W15;
					unsigned int* bin = ((unsigned int*)binary_values) + indx * 5;

					if (aa != bin[0]) goto next_iteration;
					// W: 0,1,3,5,6,7,8,10,11,12,13,14,15
					aa = rotate(aa - unpacked_W[15*NT_NUM_KEYS+i], 32 - 30);
					cc = rotate(unpacked_cs[i], 30);
					W11 = rotate(unpacked_W[11*NT_NUM_KEYS+i] ^ unpacked_W[8*NT_NUM_KEYS+i] ^ unpacked_W[3*NT_NUM_KEYS+i] ^ unpacked_W[13*NT_NUM_KEYS+i], 1);
					ee = unpacked_es[i] + rotate(aa, 5) + (unpacked_bs[i] ^ cc ^ unpacked_ds[i]) + 0xCA62C1D6 + W11; bb = rotate(unpacked_bs[i], 30);
					if (ee != bin[4]) goto next_iteration;

					dd = unpacked_ds[i] + rotate(ee, 5) + (aa ^ bb ^ cc) + 0xCA62C1D6 + unpacked_W[12*NT_NUM_KEYS+i]; aa = rotate(aa, 30);
					if (dd != bin[3]) goto next_iteration;

					W15 = rotate(unpacked_W[15*NT_NUM_KEYS+i], 32 - 1); W15 ^= unpacked_W[12*NT_NUM_KEYS+i] ^ unpacked_W[7*NT_NUM_KEYS+i] ^ unpacked_W[1*NT_NUM_KEYS+i];
					cc += rotate(dd, 5) + (ee ^ aa ^ bb) + 0xCA62C1D6 +  rotate(unpacked_W[13*NT_NUM_KEYS+i] ^ unpacked_W[10*NT_NUM_KEYS+i] ^ unpacked_W[5*NT_NUM_KEYS+i] ^ W15, 1); ee = rotate(ee, 30);
					if (cc != bin[2]) goto next_iteration;

					bb += (dd ^ ee ^ aa) + rotate(unpacked_W[14*NT_NUM_KEYS+i] ^ W11 ^ unpacked_W[6*NT_NUM_KEYS+i] ^ unpacked_W[0*NT_NUM_KEYS+i], 1);
					if (bb != bin[1]) goto next_iteration;

					// Total match
					password_was_found(indx, utf8_be_coalesc2utf8_key(nt_buffer, key, NT_NUM_KEYS, i));

				next_iteration:
					indx = same_hash_next[indx];
				}
			}

		report_keys_processed(NT_NUM_KEYS);
	}

	_aligned_free(nt_buffer);

	finish_thread();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// C code
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _M_X64
#define DCC2_R(w0, w1, w2, w3)	(W[w0] = rotate((W[w0] ^ W[w1] ^ W[w2] ^ W[w3]), 1))
PRIVATE void crypt_utf8_coalesc_protocol_c_code(CryptParam* param)
{
	unsigned int nt_buffer[8 * NT_NUM_KEYS];
	unsigned int W[16];
	unsigned int A, B, C, D, E, index;

	unsigned char key[MAX_KEY_LENGHT_SMALL];

	memset(nt_buffer, 0, sizeof(nt_buffer));
	memset(key, 0, sizeof(key));

	while (continue_attack && param->gen(nt_buffer, NT_NUM_KEYS, param->thread_id))
	{
		for (int i = 0; i < NT_NUM_KEYS; i++)
		{
			SWAP_ENDIANNESS(W[0], nt_buffer[0*NT_NUM_KEYS+i]);
			SWAP_ENDIANNESS(W[1], nt_buffer[1*NT_NUM_KEYS+i]);
			SWAP_ENDIANNESS(W[2], nt_buffer[2*NT_NUM_KEYS+i]);
			SWAP_ENDIANNESS(W[3], nt_buffer[3*NT_NUM_KEYS+i]);
			SWAP_ENDIANNESS(W[4], nt_buffer[4*NT_NUM_KEYS+i]);
			SWAP_ENDIANNESS(W[5], nt_buffer[5*NT_NUM_KEYS+i]);
			SWAP_ENDIANNESS(W[6], nt_buffer[6*NT_NUM_KEYS+i]);
			W[15] = nt_buffer[7*NT_NUM_KEYS+i];

			/* Round 1 */
			E = 0x9fb498b3 + W[0];
			D = rotate(E, 5) + 0x66b0cd0d + W[1];
			C = rotate(D, 5) + (0x7bf36ae2 ^ (E & 0x22222222)) + 0xf33d5697 + W[2]; E = rotate(E, 30);
			B = rotate(C, 5) + (0x59d148c0 ^ (D & (E ^ 0x59d148c0))) + 0xd675e47b + W[3]; D = rotate(D, 30);
			A = rotate(B, 5) + (E ^ (C & (D ^ E))) + 0xb453c259 + W[4 ]; C = rotate(C, 30);

			E += rotate(A, 5) + (D ^ (B & (C ^ D))) + SQRT_2 + W[5 ]; B = rotate(B, 30);
			D += rotate(E, 5) + (C ^ (A & (B ^ C))) + SQRT_2 + W[6 ]; A = rotate(A, 30);
			C += rotate(D, 5) + (B ^ (E & (A ^ B))) + SQRT_2		; E = rotate(E, 30);
			B += rotate(C, 5) + (A ^ (D & (E ^ A))) + SQRT_2		; D = rotate(D, 30);
			A += rotate(B, 5) + (E ^ (C & (D ^ E))) + SQRT_2		; C = rotate(C, 30);
			E += rotate(A, 5) + (D ^ (B & (C ^ D))) + SQRT_2		; B = rotate(B, 30);
			D += rotate(E, 5) + (C ^ (A & (B ^ C))) + SQRT_2		; A = rotate(A, 30);
			C += rotate(D, 5) + (B ^ (E & (A ^ B))) + SQRT_2		; E = rotate(E, 30);
			B += rotate(C, 5) + (A ^ (D & (E ^ A))) + SQRT_2		; D = rotate(D, 30);
			A += rotate(B, 5) + (E ^ (C & (D ^ E))) + SQRT_2		; C = rotate(C, 30);
			E += rotate(A, 5) + (D ^ (B & (C ^ D))) + SQRT_2 + W[15]; B = rotate(B, 30);
			D += rotate(E, 5) + (C ^ (A & (B ^ C))) + SQRT_2 + (W[0] = rotate(W[0] ^ W[2 ]		 , 1)); A = rotate(A, 30);
			C += rotate(D, 5) + (B ^ (E & (A ^ B))) + SQRT_2 + (W[1] = rotate(W[1] ^ W[3 ]		 , 1)); E = rotate(E, 30);
			B += rotate(C, 5) + (A ^ (D & (E ^ A))) + SQRT_2 + (W[2] = rotate(W[2] ^ W[15] ^ W[4], 1)); D = rotate(D, 30);
			A += rotate(B, 5) + (E ^ (C & (D ^ E))) + SQRT_2 + (W[3] = rotate(W[3] ^ W[ 0] ^ W[5], 1)); C = rotate(C, 30);

			/* Round 2 */
			E += rotate(A, 5) + (B ^ C ^ D) + SQRT_3 + (W[4 ] = rotate(W[4 ] ^ W[1 ] ^ W[6 ], 1)); B = rotate(B, 30);
			D += rotate(E, 5) + (A ^ B ^ C) + SQRT_3 + (W[5 ] = rotate(W[5 ] ^ W[2 ]		, 1)); A = rotate(A, 30);
			C += rotate(D, 5) + (E ^ A ^ B) + SQRT_3 + (W[6 ] = rotate(W[6 ] ^ W[3 ]		, 1)); E = rotate(E, 30);
			B += rotate(C, 5) + (D ^ E ^ A) + SQRT_3 + (W[7 ] = rotate(W[4 ] ^ W[15]		, 1)); D = rotate(D, 30);
			A += rotate(B, 5) + (C ^ D ^ E) + SQRT_3 + (W[8 ] = rotate(W[5 ] ^ W[ 0]		, 1)); C = rotate(C, 30);
			E += rotate(A, 5) + (B ^ C ^ D) + SQRT_3 + (W[9 ] = rotate(W[6 ] ^ W[ 1]		, 1)); B = rotate(B, 30);
			D += rotate(E, 5) + (A ^ B ^ C) + SQRT_3 + (W[10] = rotate(W[7 ] ^ W[ 2]		, 1)); A = rotate(A, 30);
			C += rotate(D, 5) + (E ^ A ^ B) + SQRT_3 + (W[11] = rotate(W[8 ] ^ W[ 3]		, 1)); E = rotate(E, 30);
			B += rotate(C, 5) + (D ^ E ^ A) + SQRT_3 + (W[12] = rotate(W[9 ] ^ W[ 4]		, 1)); D = rotate(D, 30);
			A += rotate(B, 5) + (C ^ D ^ E) + SQRT_3 + (W[13] = rotate(W[10] ^ W[ 5] ^ W[15], 1)); C = rotate(C, 30);
			E += rotate(A, 5) + (B ^ C ^ D) + SQRT_3 + (W[14] = rotate(W[11] ^ W[ 6] ^ W[ 0], 1)); B = rotate(B, 30);
			D += rotate(E, 5) + (A ^ B ^ C) + SQRT_3 + DCC2_R(15, 12, 7,  1); A = rotate(A, 30);
			C += rotate(D, 5) + (E ^ A ^ B) + SQRT_3 + DCC2_R(0 , 13, 8,  2); E = rotate(E, 30);
			B += rotate(C, 5) + (D ^ E ^ A) + SQRT_3 + DCC2_R(1 , 14, 9,  3); D = rotate(D, 30);
			A += rotate(B, 5) + (C ^ D ^ E) + SQRT_3 + DCC2_R(2 , 15, 10, 4); C = rotate(C, 30);
			E += rotate(A, 5) + (B ^ C ^ D) + SQRT_3 + DCC2_R(3 ,  0, 11, 5); B = rotate(B, 30);
			D += rotate(E, 5) + (A ^ B ^ C) + SQRT_3 + DCC2_R(4 ,  1, 12, 6); A = rotate(A, 30);
			C += rotate(D, 5) + (E ^ A ^ B) + SQRT_3 + DCC2_R(5 ,  2, 13, 7); E = rotate(E, 30);
			B += rotate(C, 5) + (D ^ E ^ A) + SQRT_3 + DCC2_R(6 ,  3, 14, 8); D = rotate(D, 30);
			A += rotate(B, 5) + (C ^ D ^ E) + SQRT_3 + DCC2_R(7 ,  4, 15, 9); C = rotate(C, 30);

			/* Round 3 */
			E += rotate(A, 5) + ((B & C) | (D & (B | C))) + 0x8F1BBCDC + DCC2_R( 8, 5,  0, 10); B = rotate(B, 30);
			D += rotate(E, 5) + ((A & B) | (C & (A | B))) + 0x8F1BBCDC + DCC2_R( 9, 6,  1, 11); A = rotate(A, 30);
			C += rotate(D, 5) + ((E & A) | (B & (E | A))) + 0x8F1BBCDC + DCC2_R(10, 7,  2, 12); E = rotate(E, 30);
			B += rotate(C, 5) + ((D & E) | (A & (D | E))) + 0x8F1BBCDC + DCC2_R(11, 8,  3, 13); D = rotate(D, 30);
			A += rotate(B, 5) + ((C & D) | (E & (C | D))) + 0x8F1BBCDC + DCC2_R(12, 9,  4, 14); C = rotate(C, 30);
			E += rotate(A, 5) + ((B & C) | (D & (B | C))) + 0x8F1BBCDC + DCC2_R(13, 10, 5, 15); B = rotate(B, 30);
			D += rotate(E, 5) + ((A & B) | (C & (A | B))) + 0x8F1BBCDC + DCC2_R(14, 11, 6,  0); A = rotate(A, 30);
			C += rotate(D, 5) + ((E & A) | (B & (E | A))) + 0x8F1BBCDC + DCC2_R(15, 12, 7,  1); E = rotate(E, 30);
			B += rotate(C, 5) + ((D & E) | (A & (D | E))) + 0x8F1BBCDC + DCC2_R( 0, 13, 8,  2); D = rotate(D, 30);
			A += rotate(B, 5) + ((C & D) | (E & (C | D))) + 0x8F1BBCDC + DCC2_R( 1, 14, 9,  3); C = rotate(C, 30);
			E += rotate(A, 5) + ((B & C) | (D & (B | C))) + 0x8F1BBCDC + DCC2_R( 2, 15, 10, 4); B = rotate(B, 30);
			D += rotate(E, 5) + ((A & B) | (C & (A | B))) + 0x8F1BBCDC + DCC2_R( 3, 0, 11,  5); A = rotate(A, 30);
			C += rotate(D, 5) + ((E & A) | (B & (E | A))) + 0x8F1BBCDC + DCC2_R( 4, 1, 12,  6); E = rotate(E, 30);
			B += rotate(C, 5) + ((D & E) | (A & (D | E))) + 0x8F1BBCDC + DCC2_R( 5, 2, 13,  7); D = rotate(D, 30);
			A += rotate(B, 5) + ((C & D) | (E & (C | D))) + 0x8F1BBCDC + DCC2_R( 6, 3, 14,  8); C = rotate(C, 30);
			E += rotate(A, 5) + ((B & C) | (D & (B | C))) + 0x8F1BBCDC + DCC2_R( 7, 4, 15,  9); B = rotate(B, 30);
			D += rotate(E, 5) + ((A & B) | (C & (A | B))) + 0x8F1BBCDC + DCC2_R( 8, 5,  0, 10); A = rotate(A, 30);
			C += rotate(D, 5) + ((E & A) | (B & (E | A))) + 0x8F1BBCDC + DCC2_R( 9, 6,  1, 11); E = rotate(E, 30);
			B += rotate(C, 5) + ((D & E) | (A & (D | E))) + 0x8F1BBCDC + DCC2_R(10, 7,  2, 12); D = rotate(D, 30);
			A += rotate(B, 5) + ((C & D) | (E & (C | D))) + 0x8F1BBCDC + DCC2_R(11, 8,  3, 13); C = rotate(C, 30);

			/* Round 4 */
			E += rotate(A, 5) + (B ^ C ^ D) + 0xCA62C1D6 + DCC2_R(12, 9, 4, 14); B = rotate(B, 30);
			D += rotate(E, 5) + (A ^ B ^ C) + 0xCA62C1D6 + DCC2_R(13,10, 5, 15); A = rotate(A, 30);
			C += rotate(D, 5) + (E ^ A ^ B) + 0xCA62C1D6 + DCC2_R(14, 11, 6, 0); E = rotate(E, 30);
			B += rotate(C, 5) + (D ^ E ^ A) + 0xCA62C1D6 + DCC2_R(15, 12, 7, 1); D = rotate(D, 30);
			A += rotate(B, 5) + (C ^ D ^ E) + 0xCA62C1D6 + DCC2_R( 0, 13, 8, 2); C = rotate(C, 30);
			E += rotate(A, 5) + (B ^ C ^ D) + 0xCA62C1D6 + DCC2_R( 1, 14, 9, 3); B = rotate(B, 30);
			D += rotate(E, 5) + (A ^ B ^ C) + 0xCA62C1D6 + DCC2_R(2, 15, 10, 4); A = rotate(A, 30);
			C += rotate(D, 5) + (E ^ A ^ B) + 0xCA62C1D6 + DCC2_R( 3, 0, 11, 5); E = rotate(E, 30);
			B += rotate(C, 5) + (D ^ E ^ A) + 0xCA62C1D6 + DCC2_R( 4, 1, 12, 6); D = rotate(D, 30);
			A += rotate(B, 5) + (C ^ D ^ E) + 0xCA62C1D6 + DCC2_R( 5, 2, 13, 7); C = rotate(C, 30);
			E += rotate(A, 5) + (B ^ C ^ D) + 0xCA62C1D6 + DCC2_R( 6, 3, 14, 8); B = rotate(B, 30);
			D += rotate(E, 5) + (A ^ B ^ C) + 0xCA62C1D6 + DCC2_R( 7, 4, 15, 9); A = rotate(A, 30);
			C += rotate(D, 5) + (E ^ A ^ B) + 0xCA62C1D6 + DCC2_R( 8, 5, 0, 10); E = rotate(E, 30);
			B += rotate(C, 5) + (D ^ E ^ A) + 0xCA62C1D6 + DCC2_R( 9, 6, 1, 11); D = rotate(D, 30);
			A += rotate(B, 5) + (C ^ D ^ E) + 0xCA62C1D6 + DCC2_R(10, 7, 2, 12); 
			
			DCC2_R(12, 9, 4, 14); A = rotate(A, 30) + DCC2_R(15, 12, 7, 1);

			// Search for a match
			index = table[A & size_table];

			// Partial match
			while (index != NO_ELEM)
			{
				unsigned int aa, bb, cc, dd, ee, W11, W15;
				unsigned int* bin = ((unsigned int*)binary_values) + index * 5;

				if (A != bin[0]) goto next_iteration;
				// W: 0,1,3,5,6,7,8,10,11,12,13,14,15
				aa = rotate(A - W[15], 32 - 30);
				cc = rotate(C, 30);
				W11 = rotate(W[11] ^ W[8] ^ W[3] ^ W[13], 1);
				ee = E + rotate(aa, 5) + (B ^ cc ^ D) + 0xCA62C1D6 + W11; bb = rotate(B, 30);
				if (ee != bin[4]) goto next_iteration;

				dd = D + rotate(ee, 5) + (aa ^ bb ^ cc) + 0xCA62C1D6 + W[12]; aa = rotate(aa, 30);
				if (dd != bin[3]) goto next_iteration;

				W15 = rotate(W[15], 32 - 1); W15 ^= W[12] ^ W[7] ^ W[1];
				cc += rotate(dd, 5) + (ee ^ aa ^ bb) + 0xCA62C1D6 +  rotate(W[13] ^ W[10] ^ W[5] ^ W15, 1); ee = rotate(ee, 30);
				if (cc != bin[2]) goto next_iteration;

				bb += (dd ^ ee ^ aa) + rotate((W[14] ^ W11 ^ W[6] ^ W[0]), 1);
				if (bb != bin[1]) goto next_iteration;

				// Total match
				password_was_found(index, utf8_coalesc2utf8_key(nt_buffer, key, NT_NUM_KEYS, i));

			next_iteration:
				index = same_hash_next[index];
			}
		}

		report_keys_processed(NT_NUM_KEYS);
	}

	finish_thread();
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Neon code
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef HS_ARM

void crypt_sha1_neon_kernel_asm(unsigned int* nt_buffer, unsigned int* bit_table, unsigned int size_bit_table);
PRIVATE void crypt_utf8_coalesc_protocol_neon(CryptParam* param)
{
	crypt_utf8_coalesc_protocol_body(param, crypt_sha1_neon_kernel_asm);
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SSE2 code
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef HS_X86
#include "arch_simd.h"

#define SHA1_NUM		(NT_NUM_KEYS/4)
#define LOAD_BIG_ENDIAN_SSE2(x,data) x = SSE2_ROTATE(data, 16); x = SSE2_ADD(_mm_slli_epi32(SSE2_AND(x, mask), 8), SSE2_AND(_mm_srli_epi32(x, 8), mask));
#undef DCC2_R
#define DCC2_R(w0, w1, w2, w3)	W[w0*SHA1_NUM] = SSE2_ROTATE(SSE2_4XOR(W[w0*SHA1_NUM], W[w1*SHA1_NUM], W[w2*SHA1_NUM], W[w3*SHA1_NUM]), 1)
PRIVATE void crypt_kernel_sse2(SSE2_WORD* nt_buffer, unsigned int* bit_table, unsigned int size_bit_table)
{
	SSE2_WORD* W = nt_buffer + 8 *SHA1_NUM;
	SSE2_WORD mask = _mm_set1_epi32(0x00FF00FF);
	SSE2_WORD step_const;
	for (int i = 0; i < SHA1_NUM; i++, nt_buffer++, W++)
	{
		LOAD_BIG_ENDIAN_SSE2(W[0*SHA1_NUM], nt_buffer[0*SHA1_NUM]);
		LOAD_BIG_ENDIAN_SSE2(W[1*SHA1_NUM], nt_buffer[1*SHA1_NUM]);
		LOAD_BIG_ENDIAN_SSE2(W[2*SHA1_NUM], nt_buffer[2*SHA1_NUM]);
		LOAD_BIG_ENDIAN_SSE2(W[3*SHA1_NUM], nt_buffer[3*SHA1_NUM]);
		LOAD_BIG_ENDIAN_SSE2(W[4*SHA1_NUM], nt_buffer[4*SHA1_NUM]);
		LOAD_BIG_ENDIAN_SSE2(W[5*SHA1_NUM], nt_buffer[5*SHA1_NUM]);
		LOAD_BIG_ENDIAN_SSE2(W[6*SHA1_NUM], nt_buffer[6*SHA1_NUM]);
		W[15*SHA1_NUM] = nt_buffer[7*SHA1_NUM];
		nt_buffer[0*SHA1_NUM] = W[0*SHA1_NUM];
		nt_buffer[1*SHA1_NUM] = W[1*SHA1_NUM];
		nt_buffer[2*SHA1_NUM] = W[2*SHA1_NUM];
		nt_buffer[3*SHA1_NUM] = W[3*SHA1_NUM];
		nt_buffer[4*SHA1_NUM] = W[4*SHA1_NUM];
		nt_buffer[5*SHA1_NUM] = W[5*SHA1_NUM];
		nt_buffer[6*SHA1_NUM] = W[6*SHA1_NUM];

		/* Round 1 */
		SSE2_WORD E = SSE2_ADD(SSE2_CONST(0x9fb498b3), W[0*SHA1_NUM]);
		SSE2_WORD D = SSE2_3ADD(SSE2_ROTATE(E, 5), SSE2_CONST(0x66b0cd0d), W[1*SHA1_NUM]);
		SSE2_WORD C = SSE2_4ADD(SSE2_ROTATE(D, 5), SSE2_XOR(SSE2_CONST(0x7bf36ae2), SSE2_AND(E, SSE2_CONST(0x22222222))), SSE2_CONST(0xf33d5697), W[2*SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		SSE2_WORD B = SSE2_4ADD(SSE2_ROTATE(C, 5), SSE2_XOR(SSE2_CONST(0x59d148c0), SSE2_AND(D, SSE2_XOR(E, SSE2_CONST(0x59d148c0)))), SSE2_CONST(0xd675e47b), W[3*SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		SSE2_WORD A = SSE2_4ADD(SSE2_ROTATE(B, 5), SSE2_XOR(E, SSE2_AND(C, SSE2_XOR(D, E))), SSE2_CONST(0xb453c259), W[4*SHA1_NUM]); C = SSE2_ROTATE(C, 30);

		step_const = _mm_set1_epi32(SQRT_2);
																								 E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_XOR(D, SSE2_AND(B, SSE2_XOR(C, D))), step_const, W[5 *SHA1_NUM]); B = SSE2_ROTATE(B, 30);
																								 D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_XOR(C, SSE2_AND(A, SSE2_XOR(B, C))), step_const, W[6 *SHA1_NUM]); A = SSE2_ROTATE(A, 30);
																								 C = SSE2_4ADD(C, SSE2_ROTATE(D, 5), SSE2_XOR(B, SSE2_AND(E, SSE2_XOR(A, B))), step_const				 ); E = SSE2_ROTATE(E, 30);
																								 B = SSE2_4ADD(B, SSE2_ROTATE(C, 5), SSE2_XOR(A, SSE2_AND(D, SSE2_XOR(E, A))), step_const				 ); D = SSE2_ROTATE(D, 30);
																								 A = SSE2_4ADD(A, SSE2_ROTATE(B, 5), SSE2_XOR(E, SSE2_AND(C, SSE2_XOR(D, E))), step_const				 ); C = SSE2_ROTATE(C, 30);
																								 E = SSE2_4ADD(E, SSE2_ROTATE(A, 5), SSE2_XOR(D, SSE2_AND(B, SSE2_XOR(C, D))), step_const				 ); B = SSE2_ROTATE(B, 30);
																								 D = SSE2_4ADD(D, SSE2_ROTATE(E, 5), SSE2_XOR(C, SSE2_AND(A, SSE2_XOR(B, C))), step_const				 ); A = SSE2_ROTATE(A, 30);
																								 C = SSE2_4ADD(C, SSE2_ROTATE(D, 5), SSE2_XOR(B, SSE2_AND(E, SSE2_XOR(A, B))), step_const				 ); E = SSE2_ROTATE(E, 30);
																								 B = SSE2_4ADD(B, SSE2_ROTATE(C, 5), SSE2_XOR(A, SSE2_AND(D, SSE2_XOR(E, A))), step_const				 ); D = SSE2_ROTATE(D, 30);
																								 A = SSE2_4ADD(A, SSE2_ROTATE(B, 5), SSE2_XOR(E, SSE2_AND(C, SSE2_XOR(D, E))), step_const				 ); C = SSE2_ROTATE(C, 30);
																								 E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_XOR(D, SSE2_AND(B, SSE2_XOR(C, D))), step_const, W[15*SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		W[0*SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[0*SHA1_NUM], W[2 *SHA1_NUM]				  ), 1); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_XOR(C, SSE2_AND(A, SSE2_XOR(B, C))), step_const, W[0 *SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		W[1*SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[1*SHA1_NUM], W[3 *SHA1_NUM]				  ), 1); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_XOR(B, SSE2_AND(E, SSE2_XOR(A, B))), step_const, W[1 *SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		W[2*SHA1_NUM] = SSE2_ROTATE(SSE2_3XOR(W[2*SHA1_NUM], W[15*SHA1_NUM], W[4*SHA1_NUM]), 1); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_XOR(A, SSE2_AND(D, SSE2_XOR(E, A))), step_const, W[2 *SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		W[3*SHA1_NUM] = SSE2_ROTATE(SSE2_3XOR(W[3*SHA1_NUM], W[ 0*SHA1_NUM], W[5*SHA1_NUM]), 1); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_XOR(E, SSE2_AND(C, SSE2_XOR(D, E))), step_const, W[3 *SHA1_NUM]); C = SSE2_ROTATE(C, 30);

		/* Round 2 */
		step_const = _mm_set1_epi32(SQRT_3);
		W[4 *SHA1_NUM] = SSE2_ROTATE(SSE2_3XOR(W[4 *SHA1_NUM], W[1 *SHA1_NUM], W[6 *SHA1_NUM]), 1); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_3XOR(B, C, D), step_const, W[4 *SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		W[5 *SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[5 *SHA1_NUM], W[2 *SHA1_NUM])				  , 1); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_3XOR(A, B, C), step_const, W[5 *SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		W[6 *SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[6 *SHA1_NUM], W[3 *SHA1_NUM])				  , 1); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_3XOR(E, A, B), step_const, W[6 *SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		W[7 *SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[4 *SHA1_NUM], W[15*SHA1_NUM])				  , 1); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_3XOR(D, E, A), step_const, W[7 *SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		W[8 *SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[5 *SHA1_NUM], W[ 0*SHA1_NUM])				  , 1); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_3XOR(C, D, E), step_const, W[8 *SHA1_NUM]); C = SSE2_ROTATE(C, 30);
		W[9 *SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[6 *SHA1_NUM], W[ 1*SHA1_NUM])				  , 1); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_3XOR(B, C, D), step_const, W[9 *SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		W[10*SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[7 *SHA1_NUM], W[ 2*SHA1_NUM])				  , 1); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_3XOR(A, B, C), step_const, W[10*SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		W[11*SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[8 *SHA1_NUM], W[ 3*SHA1_NUM])				  , 1); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_3XOR(E, A, B), step_const, W[11*SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		W[12*SHA1_NUM] = SSE2_ROTATE(SSE2_XOR (W[9 *SHA1_NUM], W[ 4*SHA1_NUM])				  , 1); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_3XOR(D, E, A), step_const, W[12*SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		W[13*SHA1_NUM] = SSE2_ROTATE(SSE2_3XOR(W[10*SHA1_NUM], W[ 5*SHA1_NUM], W[15*SHA1_NUM]), 1); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_3XOR(C, D, E), step_const, W[13*SHA1_NUM]); C = SSE2_ROTATE(C, 30);
		W[14*SHA1_NUM] = SSE2_ROTATE(SSE2_3XOR(W[11*SHA1_NUM], W[ 6*SHA1_NUM], W[ 0*SHA1_NUM]), 1); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_3XOR(B, C, D), step_const, W[14*SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		DCC2_R(15, 12, 7,  1)																	  ; D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_3XOR(A, B, C), step_const, W[15*SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		DCC2_R(0 , 13, 8,  2)																	  ; C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_3XOR(E, A, B), step_const, W[0 *SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		DCC2_R(1 , 14, 9,  3)																	  ; B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_3XOR(D, E, A), step_const, W[1 *SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		DCC2_R(2 , 15, 10, 4)																	  ; A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_3XOR(C, D, E), step_const, W[2 *SHA1_NUM]); C = SSE2_ROTATE(C, 30);
		DCC2_R(3 ,  0, 11, 5)																	  ; E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_3XOR(B, C, D), step_const, W[3 *SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		DCC2_R(4 ,  1, 12, 6)																	  ; D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_3XOR(A, B, C), step_const, W[4 *SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		DCC2_R(5 ,  2, 13, 7)																	  ; C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_3XOR(E, A, B), step_const, W[5 *SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		DCC2_R(6 ,  3, 14, 8)																	  ; B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_3XOR(D, E, A), step_const, W[6 *SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		DCC2_R(7 ,  4, 15, 9)																	  ; A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_3XOR(C, D, E), step_const, W[7 *SHA1_NUM]); C = SSE2_ROTATE(C, 30);
										  
		/* Round 3 */
		step_const = _mm_set1_epi32(0x8F1BBCDC);
		DCC2_R( 8, 5,  0, 10); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_OR(SSE2_AND(B, C), SSE2_AND(D, SSE2_OR(B, C))), step_const, W[ 8*SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		DCC2_R( 9, 6,  1, 11); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_OR(SSE2_AND(A, B), SSE2_AND(C, SSE2_OR(A, B))), step_const, W[ 9*SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		DCC2_R(10, 7,  2, 12); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_OR(SSE2_AND(E, A), SSE2_AND(B, SSE2_OR(E, A))), step_const, W[10*SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		DCC2_R(11, 8,  3, 13); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_OR(SSE2_AND(D, E), SSE2_AND(A, SSE2_OR(D, E))), step_const, W[11*SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		DCC2_R(12, 9,  4, 14); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_OR(SSE2_AND(C, D), SSE2_AND(E, SSE2_OR(C, D))), step_const, W[12*SHA1_NUM]); C = SSE2_ROTATE(C, 30);
		DCC2_R(13, 10, 5, 15); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_OR(SSE2_AND(B, C), SSE2_AND(D, SSE2_OR(B, C))), step_const, W[13*SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		DCC2_R(14, 11, 6,  0); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_OR(SSE2_AND(A, B), SSE2_AND(C, SSE2_OR(A, B))), step_const, W[14*SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		DCC2_R(15, 12, 7,  1); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_OR(SSE2_AND(E, A), SSE2_AND(B, SSE2_OR(E, A))), step_const, W[15*SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		DCC2_R( 0, 13, 8,  2); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_OR(SSE2_AND(D, E), SSE2_AND(A, SSE2_OR(D, E))), step_const, W[ 0*SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		DCC2_R( 1, 14, 9,  3); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_OR(SSE2_AND(C, D), SSE2_AND(E, SSE2_OR(C, D))), step_const, W[ 1*SHA1_NUM]); C = SSE2_ROTATE(C, 30);
		DCC2_R( 2, 15, 10, 4); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_OR(SSE2_AND(B, C), SSE2_AND(D, SSE2_OR(B, C))), step_const, W[ 2*SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		DCC2_R( 3, 0, 11,  5); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_OR(SSE2_AND(A, B), SSE2_AND(C, SSE2_OR(A, B))), step_const, W[ 3*SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		DCC2_R( 4, 1, 12,  6); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_OR(SSE2_AND(E, A), SSE2_AND(B, SSE2_OR(E, A))), step_const, W[ 4*SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		DCC2_R( 5, 2, 13,  7); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_OR(SSE2_AND(D, E), SSE2_AND(A, SSE2_OR(D, E))), step_const, W[ 5*SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		DCC2_R( 6, 3, 14,  8); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_OR(SSE2_AND(C, D), SSE2_AND(E, SSE2_OR(C, D))), step_const, W[ 6*SHA1_NUM]); C = SSE2_ROTATE(C, 30);
		DCC2_R( 7, 4, 15,  9); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_OR(SSE2_AND(B, C), SSE2_AND(D, SSE2_OR(B, C))), step_const, W[ 7*SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		DCC2_R( 8, 5,  0, 10); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_OR(SSE2_AND(A, B), SSE2_AND(C, SSE2_OR(A, B))), step_const, W[ 8*SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		DCC2_R( 9, 6,  1, 11); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_OR(SSE2_AND(E, A), SSE2_AND(B, SSE2_OR(E, A))), step_const, W[ 9*SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		DCC2_R(10, 7,  2, 12); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_OR(SSE2_AND(D, E), SSE2_AND(A, SSE2_OR(D, E))), step_const, W[10*SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		DCC2_R(11, 8,  3, 13); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_OR(SSE2_AND(C, D), SSE2_AND(E, SSE2_OR(C, D))), step_const, W[11*SHA1_NUM]); C = SSE2_ROTATE(C, 30);
										  
		/* Round 4 */
		step_const = _mm_set1_epi32(0xCA62C1D6);
		DCC2_R(12, 9, 4, 14); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_3XOR(B, C, D), step_const, W[12*SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		DCC2_R(13,10, 5, 15); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_3XOR(A, B, C), step_const, W[13*SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		DCC2_R(14, 11, 6, 0); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_3XOR(E, A, B), step_const, W[14*SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		DCC2_R(15, 12, 7, 1); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_3XOR(D, E, A), step_const, W[15*SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		DCC2_R( 0, 13, 8, 2); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_3XOR(C, D, E), step_const, W[ 0*SHA1_NUM]); C = SSE2_ROTATE(C, 30);
		DCC2_R( 1, 14, 9, 3); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_3XOR(B, C, D), step_const, W[ 1*SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		DCC2_R(2, 15, 10, 4); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_3XOR(A, B, C), step_const, W[ 2*SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		DCC2_R( 3, 0, 11, 5); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_3XOR(E, A, B), step_const, W[ 3*SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		DCC2_R( 4, 1, 12, 6); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_3XOR(D, E, A), step_const, W[ 4*SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		DCC2_R( 5, 2, 13, 7); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_3XOR(C, D, E), step_const, W[ 5*SHA1_NUM]); C = SSE2_ROTATE(C, 30);
		DCC2_R( 6, 3, 14, 8); E = SSE2_5ADD(E, SSE2_ROTATE(A, 5), SSE2_3XOR(B, C, D), step_const, W[ 6*SHA1_NUM]); B = SSE2_ROTATE(B, 30);
		DCC2_R( 7, 4, 15, 9); D = SSE2_5ADD(D, SSE2_ROTATE(E, 5), SSE2_3XOR(A, B, C), step_const, W[ 7*SHA1_NUM]); A = SSE2_ROTATE(A, 30);
		DCC2_R( 8, 5, 0, 10); C = SSE2_5ADD(C, SSE2_ROTATE(D, 5), SSE2_3XOR(E, A, B), step_const, W[ 8*SHA1_NUM]); E = SSE2_ROTATE(E, 30);
		DCC2_R( 9, 6, 1, 11); B = SSE2_5ADD(B, SSE2_ROTATE(C, 5), SSE2_3XOR(D, E, A), step_const, W[ 9*SHA1_NUM]); D = SSE2_ROTATE(D, 30);
		DCC2_R(10, 7, 2, 12); A = SSE2_5ADD(A, SSE2_ROTATE(B, 5), SSE2_3XOR(C, D, E), step_const, W[10*SHA1_NUM]); 
			
		DCC2_R(12, 9, 4, 14); DCC2_R(15, 12, 7, 1); A = SSE2_ADD(SSE2_ROTATE(A, 30), W[15*SHA1_NUM]);

		// Save
		W[2  * SHA1_NUM] = A;
		W[4  * SHA1_NUM] = B;
		W[9  * SHA1_NUM] = C;
		W[16 * SHA1_NUM] = D;
		W[17 * SHA1_NUM] = E;

		A = SSE2_AND(A, SSE2_CONST(size_bit_table));
		for (int j = 0; j < 4; j++)
		{
			unsigned int val = ((unsigned int*)(&A))[j];

			((unsigned int*)W)[18 * NT_NUM_KEYS + j] = (bit_table[val >> 5] >> (val & 31)) & 1;
		}
	}
}
PRIVATE void crypt_utf8_coalesc_protocol_sse2(CryptParam* param)
{
	crypt_utf8_coalesc_protocol_body(param, (crypt_kernel_asm_func*)crypt_kernel_sse2);
}
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AVX code
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef HS_X86

void crypt_sha1_avx_kernel_asm(unsigned int* nt_buffer, unsigned int* bit_table, unsigned int size_bit_table);
PRIVATE void crypt_utf8_coalesc_protocol_avx(CryptParam* param)
{
	crypt_utf8_coalesc_protocol_body(param, crypt_sha1_avx_kernel_asm);
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AVX2 code
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef HS_X86

void crypt_sha1_avx2_kernel_asm(unsigned int* nt_buffer, unsigned int* bit_table, unsigned int size_bit_table);
PRIVATE void crypt_utf8_coalesc_protocol_avx2(CryptParam* param)
{
	crypt_utf8_coalesc_protocol_body(param, crypt_sha1_avx2_kernel_asm);
}
#endif


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// OpenCL Implementations
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef HS_OPENCL_SUPPORT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Charset
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
PRIVATE void ocl_write_sha1_header(char* source, GPUDevice* gpu, cl_uint ntlm_size_bit_table)
{
	source[0] = 0;
	// Header definitions
	if (num_passwords_loaded > 1)
		strcat(source, "#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable\n");

	sprintf(source + strlen(source), "#define bs(c,b,a) (%s)\n", (gpu->flags & GPU_FLAG_NATIVE_BITSELECT) ? "bitselect((c),(b),(a))" : "((c)^((a)&((b)^(c))))");
#ifdef USE_MAJ_SELECTOR
	switch (MAJ_SELECTOR)
	{
	case 0:
		sprintf(source + strlen(source), "#define MAJ(b,c,d) (%s)\n", (gpu->flags & GPU_FLAG_NATIVE_BITSELECT) ? "bs(c,b,d^c)" : "(b&(c|d))|(c&d)");
		break;
	case 1:
		sprintf(source + strlen(source), "#define MAJ(b,d,c) (%s)\n", (gpu->flags & GPU_FLAG_NATIVE_BITSELECT) ? "bs(c,b,d^c)" : "(b&(c|d))|(c&d)");
		break;
	case 2:
		sprintf(source + strlen(source), "#define MAJ(c,b,d) (%s)\n", (gpu->flags & GPU_FLAG_NATIVE_BITSELECT) ? "bs(c,b,d^c)" : "(b&(c|d))|(c&d)");
		break;
	case 3:
		sprintf(source + strlen(source), "#define MAJ(c,d,b) (%s)\n", (gpu->flags & GPU_FLAG_NATIVE_BITSELECT) ? "bs(c,b,d^c)" : "(b&(c|d))|(c&d)");
		break;
	case 4:
		sprintf(source + strlen(source), "#define MAJ(d,b,c) (%s)\n", (gpu->flags & GPU_FLAG_NATIVE_BITSELECT) ? "bs(c,b,d^c)" : "(b&(c|d))|(c&d)");
		break;
	case 5:
		sprintf(source + strlen(source), "#define MAJ(d,c,b) (%s)\n", (gpu->flags & GPU_FLAG_NATIVE_BITSELECT) ? "bs(c,b,d^c)" : "(b&(c|d))|(c&d)");
		break;
	}
#else
	sprintf(source + strlen(source), "#define MAJ(b,c,d) (%s)\n", (gpu->flags & GPU_FLAG_NATIVE_BITSELECT) ? "bs(c,b,d^c)" : "(b&(c|d))|(c&d)");
#endif
	
	//Initial values
	sprintf(source + strlen(source),
		"#define SQRT_2 0x5a827999\n"
		"#define SQRT_3 0x6ed9eba1\n"
		"#define CONST3 0x8F1BBCDC\n"
		"#define CONST4 0xCA62C1D6\n"
		"#define DCC2_R(w0,w1,w2,w3)	(W ## w0)=rotate((W ## w0)^(W ## w1)^(W ## w2)^(W ## w3),1U)\n");

	if (num_passwords_loaded > 1)
		sprintf(source + strlen(source),
		"#define SIZE_TABLE %uu\n"
		"#define SIZE_BIT_TABLE %uu\n", size_table, ntlm_size_bit_table);
}
PRIVATE void ocl_write_sha1_header_charset(char* source, GPUDevice* gpu, cl_uint ntlm_size_bit_table)
{
	ocl_write_sha1_header(source, gpu, ntlm_size_bit_table);
	
	if (!is_charset_consecutive(charset))
	{
		sprintf(source + strlen(source), "\n__constant uint charset_xor[]={");

		for (cl_uint i = 0; i < num_char_in_charset; i++)
			sprintf(source + strlen(source), "%s%uU", i ? "," : "", i ? (cl_uint)(charset[i] ^ charset[i - 1]) : (cl_uint)(charset[0]));

		strcat(source, "};\n");
	}
}
PRIVATE void ocl_gen_kernel(char* source, cl_uint key_lenght, char* nt_buffer[], cl_uint output_size, char* tmp_W[64])
{
	if (is_charset_consecutive(charset))
		sprintf(source + strlen(source), "nt_buffer0+=%uu;", (is_charset_consecutive(charset)-1) << 24);

	// Begin cycle changing first character
	sprintf(source + strlen(source), "for(uint i=0;i<%uU;i++){", num_char_in_charset);

	if (is_charset_consecutive(charset))
		sprintf(source + strlen(source), "nt_buffer0+=%uu;", 1<<24);
	else
		sprintf(source + strlen(source), "nt_buffer0^=((uint)charset_xor[i])<<24u;");

	/* Round 1 */
	sprintf(source + strlen(source),
		"E=0x9fb498b3+nt_buffer0;"
		"D=rotate(E,5u)+0x66b0cd0d%s;"
		"C=rotate(D,5u)+(0x7bf36ae2^(E&0x22222222))+0xf33d5697%s;E=rotate(E,30u);"
		"B=rotate(C,5u)+(0x59d148c0^(D&(E^0x59d148c0)))+0xd675e47b%s;D=rotate(D,30u);"
		"A=rotate(B,5u)+bs(E,D,C)+0xb453c259%s;C=rotate(C,30u);"

		"E+=rotate(A,5u)+bs(D,C,B)+SQRT_2%s;B=rotate(B,30u);"
		"D+=rotate(E,5u)+bs(C,B,A)+SQRT_2%s;A=rotate(A,30u);"
		"C+=rotate(D,5u)+bs(B,A,E)+SQRT_2;E=rotate(E,30u);"
		"B+=rotate(C,5u)+bs(A,E,D)+SQRT_2;D=rotate(D,30u);"
		"A+=rotate(B,5u)+bs(E,D,C)+SQRT_2;C=rotate(C,30u);"
		"E+=rotate(A,5u)+bs(D,C,B)+SQRT_2;B=rotate(B,30u);"
		"D+=rotate(E,5u)+bs(C,B,A)+SQRT_2;A=rotate(A,30u);"
		"C+=rotate(D,5u)+bs(B,A,E)+SQRT_2;E=rotate(E,30u);"
		"B+=rotate(C,5u)+bs(A,E,D)+SQRT_2;D=rotate(D,30u);"
		"A+=rotate(B,5u)+bs(E,D,C)+SQRT_2;C=rotate(C,30u);"
		"E+=rotate(A,5u)+bs(D,C,B)+%uu;B=rotate(B,30u);"
		, nt_buffer[1], nt_buffer[2], nt_buffer[3], nt_buffer[4], nt_buffer[5], nt_buffer[6], (key_lenght << 3) + SQRT_2);


		// Last Round 1
	sprintf(source + strlen(source),
		"uint word0_r2=rotate(nt_buffer0,2u);"
		"D+=rotate(E,5u)+bs(C,B,A)+SQRT_2+(%s^rotate(nt_buffer0,1u));A=rotate(A,30u);"
		"C+=rotate(D,5u)+bs(B,A,E)+%s;E=rotate(E,30u);"
		"B+=rotate(C,5u)+bs(A,E,D)+%s;D=rotate(D,30u);"
		"A+=rotate(B,5u)+bs(E,D,C)+SQRT_2+(%s^word0_r2);C=rotate(C,30u);"
		
		/* Round 2 */
		"uint word0_r3=rotate(nt_buffer0,3u);"
		"uint word0_r4=rotate(nt_buffer0,4u);"
		"uint word0_r5=rotate(nt_buffer0,5u);"
		"uint word0_r6=rotate(nt_buffer0,6u);"
		"E+=rotate(A,5u)+(B^C^D)+%s;B=rotate(B,30u);"
		"D+=rotate(E,5u)+(A^B^C)+%s;A=rotate(A,30u);"
		"C+=rotate(D,5u)+(E^A^B)+SQRT_3+(%s^word0_r3);E=rotate(E,30u);"
		"B+=rotate(C,5u)+(D^E^A)+%s;D=rotate(D,30u);"
		"A+=rotate(B,5u)+(C^D^E)+SQRT_3+(%s^word0_r2);C=rotate(C,30u);"//word0_r2
		"E+=rotate(A,5u)+(B^C^D)+SQRT_3+(%s^word0_r4);B=rotate(B,30u);"
		"D+=rotate(E,5u)+(A^B^C)+%s;A=rotate(A,30u);"
		"C+=rotate(D,5u)+(E^A^B)+%s;E=rotate(E,30u);"
		"B+=rotate(C,5u)+(D^E^A)+SQRT_3+(%s^word0_r5);D=rotate(D,30u);"
		"A+=rotate(B,5u)+(C^D^E)+%s;C=rotate(C,30u);"
		"E+=rotate(A,5u)+(B^C^D)+SQRT_3+(%s^word0_r2^word0_r4);B=rotate(B,30u);"//word0_r2
		"D+=rotate(E,5u)+(A^B^C)+SQRT_3+(%s^word0_r6);A=rotate(A,30u);"
		, tmp_W[0], tmp_W[1], tmp_W[2], tmp_W[3], tmp_W[4], tmp_W[5], tmp_W[6], tmp_W[7], tmp_W[8], tmp_W[9], tmp_W[10], tmp_W[11], tmp_W[12], tmp_W[13], tmp_W[14], tmp_W[15]);

	/* Round 2 */
	sprintf(source + strlen(source),
		"uint word0_r46=word0_r4^word0_r6;"
		"uint word0_r7=rotate(nt_buffer0,7u);"
		"uint word0_r8 =rotate(nt_buffer0,8u);"
		"C+=rotate(D,5u)+(E^A^B)+SQRT_3+(%s^word0_r2^word0_r3);E=rotate(E,30u);"//word0_r2----------------
		"B+=rotate(C,5u)+(D^E^A)+%s;D=rotate(D,30u);"
		"A+=rotate(B,5u)+(C^D^E)+SQRT_3+(%s^word0_r7);C=rotate(C,30u);"
		"E+=rotate(A,5u)+(B^C^D)+SQRT_3+(%s^word0_r4);B=rotate(B,30u);"
		"D+=rotate(E,5u)+(A^B^C)+SQRT_3+(%s^word0_r46);A=rotate(A,30u);"
		"C+=rotate(D,5u)+(E^A^B)+SQRT_3+(%s^word0_r8);E=rotate(E,30u);"
		"B+=rotate(C,5u)+(D^E^A)+SQRT_3+(%s^word0_r4);D=rotate(D,30u);"
		"A+=rotate(B,5u)+(C^D^E)+%s;C=rotate(C,30u);"

		/*Round 3 */
		"uint word0_r48=word0_r4^word0_r8;"
		"uint word0_r10=rotate(nt_buffer0,10u);"
		"uint word0_r11=rotate(nt_buffer0,11u);"
		"E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+(%s^word0_r4^rotate(nt_buffer0,9u));B=rotate(B,30u);"
		"D+=rotate(E,5u)+MAJ(A,B,C)+%s;A=rotate(A,30u);"
		"C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+(%s^word0_r6^word0_r8);E=rotate(E,30u);"
		"B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+(%s^word0_r10);D=rotate(D,30u);"
		"A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+(%s^word0_r3^word0_r6^word0_r7);C=rotate(C,30u);"
		"E+=rotate(A,5u)+MAJ(B,C,D)+%s;B=rotate(B,30u);"
		"D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+(%s^word0_r4^word0_r11);A=rotate(A,30u);"//word0_r4----------------
		"C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+(%s^word0_r48);E=rotate(E,30u);"
		, tmp_W[16], tmp_W[17], tmp_W[18], tmp_W[19], tmp_W[20], tmp_W[21], tmp_W[22], tmp_W[23], tmp_W[24], tmp_W[25], tmp_W[26], tmp_W[27], tmp_W[28], tmp_W[29], tmp_W[30], tmp_W[31]);

	/* Round 3 */
	sprintf(source + strlen(source),
		"uint word0_r12=rotate(nt_buffer0,12u);"
		"uint word0_r712=word0_r7^word0_r12;"
		"uint word0_r13=rotate(nt_buffer0,13u);"
		"uint word0_r14=rotate(nt_buffer0,14u);"
		"uint word0_r15=rotate(nt_buffer0,15u);"
		"B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+(%s^word0_r3^word0_r48^word0_r5^word0_r10);D=rotate(D,30u);"//word0_r3----------------
		"A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+(%s^word0_r12);C=rotate(C,30u);"
		"E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+(%s^word0_r8);B=rotate(B,30u);"
		"D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+(%s^word0_r46);A=rotate(A,30u);"
		"C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+(%s^word0_r48^word0_r13);E=rotate(E,30u);"
		"B+=rotate(C,5u)+MAJ(D,E,A)+%s;D=rotate(D,30u);"
		"A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+(%s^word0_r712^word0_r10);C=rotate(C,30u);"
		"E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+(%s^word0_r14);B=rotate(B,30u);"
		"D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+(%s^word0_r46^word0_r7^word0_r10^word0_r11);A=rotate(A,30u);"//word0_r10----------------
		"C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+(%s^word0_r8);E=rotate(E,30u);"
		"B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+(%s^word0_r48^word0_r15);D=rotate(D,30u);"
		"uint word0_r812=word0_r8^word0_r12;"
		"A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+(%s^word0_r812);C=rotate(C,30u);"

		/*Round 4 */
		"word0_r46^=word0_r812;"//word0_r812----------------
		"uint word0_r16=rotate(nt_buffer0,16u);"
		"E+=rotate(A,5u)+(B^C^D)+CONST4+(%s^word0_r48^word0_r712^word0_r14);B=rotate(B,30u);"//word0_r48----------------word0_r712
		"D+=rotate(E,5u)+(A^B^C)+CONST4+(%s^word0_r16);A=rotate(A,30u);"
		"C+=rotate(D,5u)+(E^A^B)+CONST4+(%s^word0_r46);E=rotate(E,30u);"
		"B+=rotate(C,5u)+(D^E^A)+CONST4+(%s^word0_r8);D=rotate(D,30u);"
		, tmp_W[32], tmp_W[33], tmp_W[34], tmp_W[35], tmp_W[36], tmp_W[37], tmp_W[38], tmp_W[39], tmp_W[40], tmp_W[41], tmp_W[42], tmp_W[43], tmp_W[44], tmp_W[45], tmp_W[46], tmp_W[47]);

	/* Round 4 */
	sprintf(source + strlen(source),
		"uint word0_r1216=word0_r16^word0_r12;"
		"uint word0_r18=rotate(nt_buffer0,18u);"
		"A+=rotate(B,5u)+(C^D^E)+CONST4+(%s^word0_r46^word0_r7^rotate(nt_buffer0,17u));C=rotate(C,30u);"//word0_r46----------------
		"E+=rotate(A,5u)+(B^C^D)+%s;B=rotate(B,30u);"
		"D+=rotate(E,5u)+(A^B^C)+CONST4+(%s^word0_r14^word0_r16);A=rotate(A,30u);"
		"C+=rotate(D,5u)+(E^A^B)+CONST4+(%s^word0_r8^word0_r18);E=rotate(E,30u);"
		"B+=rotate(C,5u)+(D^E^A)+CONST4+(%s^word0_r11^word0_r14^word0_r15);D=rotate(D,30u);"
		"A+=rotate(B,5u)+(C^D^E)+%s;C=rotate(C,30u);"
		"E+=rotate(A,5u)+(B^C^D)+CONST4+(%s^word0_r12^rotate(nt_buffer0,19u));B=rotate(B,30u);"
		"D+=rotate(E,5u)+(A^B^C)+CONST4+(%s^word0_r1216);A=rotate(A,30u);"
		"C+=rotate(D,5u)+(E^A^B)+CONST4+(%s^word0_r5^word0_r11^word0_r18^word0_r1216^word0_r13);E=rotate(E,30u);"
		"B+=rotate(C,5u)+(D^E^A)+CONST4+(%s^rotate(nt_buffer0,20u));D=rotate(D,30u);"
		"A+=rotate(B,5u)+(C^D^E)+CONST4+(%s^word0_r8^word0_r16);"
		
		"A=rotate(A,30u)+(%s^word0_r8^rotate(nt_buffer0,22u));"
		, tmp_W[48], tmp_W[49], tmp_W[50], tmp_W[51], tmp_W[52], tmp_W[53], tmp_W[54], tmp_W[55], tmp_W[56], tmp_W[57], tmp_W[58], tmp_W[63]);

	// Find match
	if (num_passwords_loaded == 1)
	{
		unsigned int* bin = (unsigned int*)binary_values;
		sprintf(source + strlen(source),
				"if(A==%uu)"
				"{"
					"A=rotate(A-(%s^word0_r8^rotate(nt_buffer0,22u)),2u);"
					"C=rotate(C,30u);"
					"uint ww11=rotate(%s^%s^word0_r13^word0_r11^word0_r5^%s^%s,1u);"
					"E+=rotate(A,5u)+(B^C^D)+0xCA62C1D6+ww11;B=rotate(B,30u);"
							
					"D+=rotate(E,5u)+(A^B^C)+0xCA62C1D6+rotate(%s^word0_r7^%s^rotate(nt_buffer0,20u)^%s^word0_r11^word0_r15^%s^word0_r6,1u);A=rotate(A,30u);"
							
					"C+=rotate(D,5u)+(E^A^B)+0xCA62C1D6+rotate(%s^%s^(%s-CONST4)^%s,1u);"
							
					"B+=(D^rotate(E,30u)^A)+rotate(%s^word0_r12^ww11^%s^rotate(nt_buffer0,19u)^%s^rotate(nt_buffer0,17u)^word0_r7,1u);"

					"if(B==%uu&&C==%uu&&D==%uu&&E==%uu)"
					"{"
						"output[0]=1u;"
						"output[1]=get_global_id(0)*NUM_CHAR_IN_CHARSET+i;"
						"output[2]=0;"
					"}"
				"}"
				, bin[0]
				, tmp_W[63], tmp_W[43], tmp_W[56], tmp_W[51], tmp_W[45]
				, tmp_W[44], tmp_W[57], tmp_W[52], tmp_W[46]
				, tmp_W[45], tmp_W[58], tmp_W[53], tmp_W[47]
				, tmp_W[46], tmp_W[54], tmp_W[48]
				, bin[1], bin[2], bin[3], bin[4]);
	}
	else
	{
		sprintf(source + strlen(source),
			"indx=A&SIZE_BIT_TABLE;"
			"if((bit_table[indx>>5u]>>(indx&31u))&1u)"
			"{"
				"indx=table[A & SIZE_TABLE];"

				"while(indx!=0xffffffff)"
				//"if(indx!=0xffffffff)"
				"{"
					"if(A==binary_values[indx*5u+0u])"
					"{"
						"uint aa=rotate(A-(%s^word0_r8^rotate(nt_buffer0,22u)),2u);"
						"uint cc=rotate(C,30u);"
						"uint ww11=rotate(%s^%s^word0_r13^word0_r11^word0_r5^%s^%s,1u);"
						"uint ee=E+rotate(aa,5u)+(B^cc^D)+0xCA62C1D6+ww11;uint bb=rotate(B,30u);"
							
						"uint dd=D+rotate(ee,5u)+(aa^bb^cc)+0xCA62C1D6+rotate(%s^word0_r7^%s^rotate(nt_buffer0,20u)^%s^word0_r11^word0_r15^%s^word0_r6,1u);aa=rotate(aa,30u);"
							
						"cc+=rotate(dd,5u)+(ee^aa^bb)+0xCA62C1D6+rotate(%s^%s^(%s-CONST4)^%s,1u);"
							
						"bb+=(dd^rotate(ee,30u)^aa)+rotate(%s^word0_r12^ww11^%s^rotate(nt_buffer0,19u)^%s^rotate(nt_buffer0,17u)^word0_r7,1u);"

						"if(bb==binary_values[indx*5u+1u]&&cc==binary_values[indx*5u+2u]&&dd==binary_values[indx*5u+3u]&&ee==binary_values[indx*5u+4u])"
						"{"
							"uint found=atomic_inc(output);"
							"if(found<%uu){"
							"output[2*found+1]=get_global_id(0)*NUM_CHAR_IN_CHARSET+i;"
							"output[2*found+2]=indx;}"
						"}"
						, tmp_W[63], tmp_W[43], tmp_W[56], tmp_W[51], tmp_W[45]
						, tmp_W[44], tmp_W[57], tmp_W[52], tmp_W[46]
						, tmp_W[45], tmp_W[58], tmp_W[53], tmp_W[47]
						, tmp_W[46], tmp_W[54], tmp_W[48]
						, output_size);

	strcat(source, "}"
					"indx=same_hash_next[indx];"
				"}"
			"}");
	}

	strcat(source, "}}");
}
PRIVATE void ocl_gen_kernel_with_lenght_local(char* source, cl_uint key_lenght, cl_uint vector_size, cl_uint ntlm_size_bit_table, cl_uint output_size, DivisionParams div_param, char** str_comp, cl_bool value_map_collission, cl_uint workgroup)
{
	char* nt_buffer[] = {"+nt_buffer0", "+nt_buffer1", "+nt_buffer2", "+nt_buffer3", "+nt_buffer4", "+nt_buffer5", "+nt_buffer6"};

	ocl_charset_load_buffer_be(source, key_lenght, &vector_size, div_param, nt_buffer);

#ifdef HS_OCL_CURRENT_KEY_AS_REGISTERS
	cl_uint bits_by_char;
	_BitScanReverse(&bits_by_char, ceil_power_2(num_char_in_charset));
	cl_uint key_mask = ceil_power_2(num_char_in_charset) - 1;
#endif

	// Pre-Calculate W
	sprintf(source + strlen(source),
		"uint A,B,C,D,E;"
		"local uint tmp_W[128];"
		"uint lid=get_local_id(0);"
		"uint lsize=get_local_size(0);");

	if (key_lenght == 1)
		sprintf(source + strlen(source), "uint lidx_base=get_group_id(0)*lsize;");
	else
#ifdef HS_OCL_CURRENT_KEY_AS_REGISTERS
		sprintf(source + strlen(source), "uint lidx_base=get_group_id(0)*lsize+(current_key0&%uu)+((current_key0>>%uu)&%uu)*NUM_CHAR_IN_CHARSET+((current_key0>>%uu)&%uu)*NUM_CHAR_IN_CHARSET*NUM_CHAR_IN_CHARSET;"
					, key_mask, bits_by_char, key_mask, 2 * bits_by_char, key_mask);
#else
		sprintf(source + strlen(source), "uint lidx_base=get_group_id(0)*lsize+current_key[1]+current_key[2]*NUM_CHAR_IN_CHARSET+current_key[3]*NUM_CHAR_IN_CHARSET*NUM_CHAR_IN_CHARSET;");
#endif

	sprintf(source + strlen(source),
		"lsize--;"
		"uint lidx_end=(lidx_base+lsize)/%uu;"
		"uint lidx=(lidx_base+lid)/%uu;"
		"lidx_base=lidx_base/%uu;"

		"lidx=(lidx==lidx_base)?0:64u;"

		"if(lid==0||(lidx_end!=lidx_base&&lid==lsize))"
		"{"
			"tmp_W[lidx+0 ]=rotate((0u%s),1u);"
			"tmp_W[lidx+1 ]=rotate((0u%s)^(0u%s),1u);"
			"tmp_W[lidx+2 ]=rotate((0u%s)^%uu^(0u%s),1u);"
			"tmp_W[lidx+3 ]=rotate((0u%s)^tmp_W[lidx+0]^(0u%s),1u);"
			"tmp_W[lidx+4 ]=rotate((0u%s)^tmp_W[lidx+1]^(0u%s),1u);"
			"tmp_W[lidx+5 ]=rotate((0u%s)^tmp_W[lidx+2],1u);"
			"tmp_W[lidx+6 ]=rotate((0u%s)^tmp_W[lidx+3],1u);"
			"tmp_W[lidx+7 ]=rotate(tmp_W[lidx+4]^%uu,1u);"
			"tmp_W[lidx+8 ]=rotate(tmp_W[lidx+5]^tmp_W[lidx+0],1u);"
			"tmp_W[lidx+9 ]=rotate(tmp_W[lidx+6]^tmp_W[lidx+1],1u);"
			"tmp_W[lidx+10]=rotate(tmp_W[lidx+7]^tmp_W[lidx+2],1u);"
			"tmp_W[lidx+11]=rotate(tmp_W[lidx+8]^tmp_W[lidx+3],1u);"
			"tmp_W[lidx+12]=rotate(tmp_W[lidx+9]^tmp_W[lidx+4],1u);"
			"tmp_W[lidx+13]=rotate(tmp_W[lidx+10]^tmp_W[lidx+5]^%uu,1u);"
			"tmp_W[lidx+14]=rotate(tmp_W[lidx+11]^tmp_W[lidx+6]^tmp_W[lidx+0],1u);"
			"tmp_W[lidx+15]=rotate(%uu^tmp_W[lidx+12]^tmp_W[lidx+7]^tmp_W[lidx+1],1u);"

			"tmp_W[lidx+16]=rotate(tmp_W[lidx+0]^tmp_W[lidx+13]^tmp_W[lidx+8 ]^tmp_W[lidx+2],1u);"
			"tmp_W[lidx+17]=rotate(tmp_W[lidx+1]^tmp_W[lidx+14]^tmp_W[lidx+9 ]^tmp_W[lidx+3],1u);"
			"tmp_W[lidx+18]=rotate(tmp_W[lidx+2]^tmp_W[lidx+15]^tmp_W[lidx+10]^tmp_W[lidx+4],1u);"
			"tmp_W[lidx+19]=rotate(tmp_W[lidx+3]^tmp_W[lidx+16]^tmp_W[lidx+11]^tmp_W[lidx+5],1u);"
			"tmp_W[lidx+20]=rotate(tmp_W[lidx+4]^tmp_W[lidx+17]^tmp_W[lidx+12]^tmp_W[lidx+6],1u);"
			"tmp_W[lidx+21]=rotate(tmp_W[lidx+5]^tmp_W[lidx+18]^tmp_W[lidx+13]^tmp_W[lidx+7],1u);"
			"tmp_W[lidx+22]=rotate(tmp_W[lidx+6]^tmp_W[lidx+19]^tmp_W[lidx+14]^tmp_W[lidx+8],1u);"
			"tmp_W[lidx+23]=rotate(tmp_W[lidx+7]^tmp_W[lidx+20]^tmp_W[lidx+15]^tmp_W[lidx+9],1u);"
			"tmp_W[lidx+24]=rotate(tmp_W[lidx+8]^tmp_W[lidx+21]^tmp_W[lidx+16]^tmp_W[lidx+10],1u);"
			"tmp_W[lidx+25]=rotate(tmp_W[lidx+9]^tmp_W[lidx+22]^tmp_W[lidx+17]^tmp_W[lidx+11],1u);"
			"tmp_W[lidx+26]=rotate(tmp_W[lidx+10]^tmp_W[lidx+23]^tmp_W[lidx+18]^tmp_W[lidx+12],1u);"
			"tmp_W[lidx+27]=rotate(tmp_W[lidx+11]^tmp_W[lidx+24]^tmp_W[lidx+19]^tmp_W[lidx+13],1u);"
			"tmp_W[lidx+28]=rotate(tmp_W[lidx+12]^tmp_W[lidx+25]^tmp_W[lidx+20]^tmp_W[lidx+14],1u);"//3
			"tmp_W[lidx+29]=rotate(tmp_W[lidx+13]^tmp_W[lidx+26]^tmp_W[lidx+21]^tmp_W[lidx+15],1u);"
			"tmp_W[lidx+30]=rotate(tmp_W[lidx+14]^tmp_W[lidx+27]^tmp_W[lidx+22]^tmp_W[lidx+16],1u);"
			"tmp_W[lidx+31]=rotate(tmp_W[lidx+15]^tmp_W[lidx+28]^tmp_W[lidx+23]^tmp_W[lidx+17],1u);"

			"tmp_W[lidx+32]=rotate(tmp_W[lidx+16]^tmp_W[lidx+29]^tmp_W[lidx+24]^tmp_W[lidx+18],1u);"//5
			"tmp_W[lidx+33]=rotate(tmp_W[lidx+17]^tmp_W[lidx+30]^tmp_W[lidx+25]^tmp_W[lidx+19],1u);"
			"tmp_W[lidx+34]=rotate(tmp_W[lidx+18]^tmp_W[lidx+31]^tmp_W[lidx+26]^tmp_W[lidx+20],1u);"
			"tmp_W[lidx+35]=rotate(tmp_W[lidx+19]^tmp_W[lidx+32]^tmp_W[lidx+27]^tmp_W[lidx+21],1u);"
			"tmp_W[lidx+36]=rotate(tmp_W[lidx+20]^tmp_W[lidx+33]^tmp_W[lidx+28]^tmp_W[lidx+22],1u);"//3
			"tmp_W[lidx+37]=rotate(tmp_W[lidx+21]^tmp_W[lidx+34]^tmp_W[lidx+29]^tmp_W[lidx+23],1u);"
			"tmp_W[lidx+38]=rotate(tmp_W[lidx+22]^tmp_W[lidx+35]^tmp_W[lidx+30]^tmp_W[lidx+24],1u);"//3
			"tmp_W[lidx+39]=rotate(tmp_W[lidx+23]^tmp_W[lidx+36]^tmp_W[lidx+31]^tmp_W[lidx+25],1u);"
			"tmp_W[lidx+40]=rotate(tmp_W[lidx+24]^tmp_W[lidx+37]^tmp_W[lidx+32]^tmp_W[lidx+26],1u);"//5
			"tmp_W[lidx+41]=rotate(tmp_W[lidx+25]^tmp_W[lidx+38]^tmp_W[lidx+33]^tmp_W[lidx+27],1u);"
			"tmp_W[lidx+42]=rotate(tmp_W[lidx+26]^tmp_W[lidx+39]^tmp_W[lidx+34]^tmp_W[lidx+28],1u);"//3
			"tmp_W[lidx+43]=rotate(tmp_W[lidx+27]^tmp_W[lidx+40]^tmp_W[lidx+35]^tmp_W[lidx+29],1u);"
			"tmp_W[lidx+44]=rotate(tmp_W[lidx+28]^tmp_W[lidx+41]^tmp_W[lidx+36]^tmp_W[lidx+30],1u);"//5
			"tmp_W[lidx+45]=rotate(tmp_W[lidx+29]^tmp_W[lidx+42]^tmp_W[lidx+37]^tmp_W[lidx+31],1u);"
			"tmp_W[lidx+46]=rotate(tmp_W[lidx+30]^tmp_W[lidx+43]^tmp_W[lidx+38]^tmp_W[lidx+32],1u);"//4
			"tmp_W[lidx+47]=rotate(tmp_W[lidx+31]^tmp_W[lidx+44]^tmp_W[lidx+39]^tmp_W[lidx+33],1u);"

			"tmp_W[lidx+48]=rotate(tmp_W[lidx+32]^tmp_W[lidx+45]^tmp_W[lidx+40]^tmp_W[lidx+34],1u);"//6
			"tmp_W[lidx+49]=rotate(tmp_W[lidx+33]^tmp_W[lidx+46]^tmp_W[lidx+41]^tmp_W[lidx+35],1u);"
			"tmp_W[lidx+50]=rotate(tmp_W[lidx+34]^tmp_W[lidx+47]^tmp_W[lidx+42]^tmp_W[lidx+36],1u);"
			"tmp_W[lidx+51]=rotate(tmp_W[lidx+35]^tmp_W[lidx+48]^tmp_W[lidx+43]^tmp_W[lidx+37],1u);"
			"tmp_W[lidx+52]=rotate(tmp_W[lidx+36]^tmp_W[lidx+49]^tmp_W[lidx+44]^tmp_W[lidx+38],1u);"//3
			"tmp_W[lidx+53]=rotate(tmp_W[lidx+37]^tmp_W[lidx+50]^tmp_W[lidx+45]^tmp_W[lidx+39],1u);"
			"tmp_W[lidx+54]=rotate(tmp_W[lidx+38]^tmp_W[lidx+51]^tmp_W[lidx+46]^tmp_W[lidx+40],1u);"
			"tmp_W[lidx+55]=rotate(tmp_W[lidx+39]^tmp_W[lidx+52]^tmp_W[lidx+47]^tmp_W[lidx+41],1u);"
			"tmp_W[lidx+56]=rotate(tmp_W[lidx+40]^tmp_W[lidx+53]^tmp_W[lidx+48]^tmp_W[lidx+42],1u);"
			"tmp_W[lidx+57]=rotate(tmp_W[lidx+41]^tmp_W[lidx+54]^tmp_W[lidx+49]^tmp_W[lidx+43],1u);"
			"tmp_W[lidx+58]=rotate(tmp_W[lidx+42]^tmp_W[lidx+55]^tmp_W[lidx+50]^tmp_W[lidx+44],1u);"
			//"tmp_W[lidx+59]=rotate(tmp_W[lidx+43]^tmp_W[lidx+56]^tmp_W[lidx+51]^tmp_W[lidx+45],1u);"//--
			//"tmp_W[lidx+60]=rotate(tmp_W[lidx+44]^tmp_W[lidx+57]^tmp_W[lidx+52]^tmp_W[lidx+46],1u);"//-----
			//"tmp_W[lidx+61]=rotate(tmp_W[lidx+45]^tmp_W[lidx+58]^tmp_W[lidx+53]^tmp_W[lidx+47],1u);"//--
			//"tmp_W[lidx+62]=rotate(tmp_W[lidx+46]^tmp_W[lidx+59]^tmp_W[lidx+54]^tmp_W[lidx+48],1u);"//--
			"tmp_W[lidx+63]=rotate(tmp_W[lidx+47]^rotate(tmp_W[lidx+44]^tmp_W[lidx+57]^tmp_W[lidx+52]^tmp_W[lidx+46],1u)^tmp_W[lidx+55]^tmp_W[lidx+49],1u);"

			"tmp_W[lidx+2]+=SQRT_2;"
			"tmp_W[lidx+5]+=SQRT_3;"
			"tmp_W[lidx+1]+=SQRT_2;"
			"tmp_W[lidx+4]+=SQRT_3;"
			"tmp_W[lidx+7]+=SQRT_3;"
			"tmp_W[lidx+10]+=SQRT_3;"
			"tmp_W[lidx+11]+=SQRT_3;"
			"tmp_W[lidx+13]+=SQRT_3;"
			
			"tmp_W[lidx+25]+=CONST3;"
			"tmp_W[lidx+29]+=CONST3;"
			"tmp_W[lidx+37]+=CONST3;"
			
			"tmp_W[lidx+49]+=CONST4;"
			"tmp_W[lidx+17]+=SQRT_3;"
			"tmp_W[lidx+23]+=SQRT_3;"
			
			"tmp_W[lidx+53]+=CONST4;"
		"}"
		"barrier(CLK_LOCAL_MEM_FENCE);"
		, POW3(num_char_in_charset), POW3(num_char_in_charset), POW3(num_char_in_charset)

		, nt_buffer[2]
		, nt_buffer[1], nt_buffer[3]
		, nt_buffer[2], key_lenght << 3, nt_buffer[4]
		, nt_buffer[3], nt_buffer[5]
		, nt_buffer[4], nt_buffer[6]
		, nt_buffer[5]
		, nt_buffer[6]
		, key_lenght << 3





		, key_lenght << 3

		, key_lenght << 3);

	char* tmp_W[] = {
			"tmp_W[lidx+0 ]", "tmp_W[lidx+1 ]", "tmp_W[lidx+2 ]", "tmp_W[lidx+3 ]", "tmp_W[lidx+4 ]", "tmp_W[lidx+5 ]", "tmp_W[lidx+6 ]", "tmp_W[lidx+7 ]", "tmp_W[lidx+8 ]", "tmp_W[lidx+9 ]", "tmp_W[lidx+10]", "tmp_W[lidx+11]", "tmp_W[lidx+12]", "tmp_W[lidx+13]", "tmp_W[lidx+14]", "tmp_W[lidx+15]",
			"tmp_W[lidx+16]", "tmp_W[lidx+17]", "tmp_W[lidx+18]", "tmp_W[lidx+19]", "tmp_W[lidx+20]", "tmp_W[lidx+21]", "tmp_W[lidx+22]", "tmp_W[lidx+23]", "tmp_W[lidx+24]", "tmp_W[lidx+25]", "tmp_W[lidx+26]", "tmp_W[lidx+27]", "tmp_W[lidx+28]", "tmp_W[lidx+29]", "tmp_W[lidx+30]", "tmp_W[lidx+31]",
			"tmp_W[lidx+32]", "tmp_W[lidx+33]", "tmp_W[lidx+34]", "tmp_W[lidx+35]", "tmp_W[lidx+36]", "tmp_W[lidx+37]", "tmp_W[lidx+38]", "tmp_W[lidx+39]", "tmp_W[lidx+40]", "tmp_W[lidx+41]", "tmp_W[lidx+42]", "tmp_W[lidx+43]", "tmp_W[lidx+44]", "tmp_W[lidx+45]", "tmp_W[lidx+46]", "tmp_W[lidx+47]",
			"tmp_W[lidx+48]", "tmp_W[lidx+49]", "tmp_W[lidx+50]", "tmp_W[lidx+51]", "tmp_W[lidx+52]", "tmp_W[lidx+53]", "tmp_W[lidx+54]", "tmp_W[lidx+55]", "tmp_W[lidx+56]", "tmp_W[lidx+57]", "tmp_W[lidx+58]", "tmp_W[lidx+59]", "tmp_W[lidx+60]", "tmp_W[lidx+61]", "tmp_W[lidx+62]", "tmp_W[lidx+63]"
		};

	ocl_gen_kernel(source, key_lenght, nt_buffer, output_size, tmp_W);
}
PRIVATE void ocl_gen_kernel_with_lenght_mixed(char* source, cl_uint key_lenght, cl_uint vector_size, cl_uint ntlm_size_bit_table, cl_uint output_size, DivisionParams div_param, char** str_comp, cl_bool value_map_collission, cl_uint workgroup)
{
	char* nt_buffer[] = { "+nt_buffer0", "+nt_buffer1", "+nt_buffer2", "+nt_buffer3", "+nt_buffer4", "+nt_buffer5", "+nt_buffer6" };

	ocl_charset_load_buffer_be(source, key_lenght, &vector_size, div_param, nt_buffer);

	// Pre-Calculate W
	sprintf(source + strlen(source),
		"uint A,B,C,D,E;"
		"uint tmp_W0=rotate((0u%s),1u);"
		"uint tmp_W1=rotate((0u%s)^(0u%s),1u);"
		"uint tmp_W2=rotate((0u%s)^%uu^(0u%s),1u);"
		"uint tmp_W3=rotate((0u%s)^tmp_W0^(0u%s),1u);"
		"uint tmp_W4=rotate((0u%s)^tmp_W1^(0u%s),1u);"
		"uint tmp_W5=rotate((0u%s)^tmp_W2,1u);"
		"uint tmp_W6=rotate((0u%s)^tmp_W3,1u);"
		"uint tmp_W7=rotate(tmp_W4^%uu,1u);"
		"uint tmp_W8=rotate(tmp_W5^tmp_W0,1u);"
		"uint tmp_W9=rotate(tmp_W6^tmp_W1,1u);"
		"uint tmp_W10=rotate(tmp_W7^tmp_W2,1u);"
		"uint tmp_W11=rotate(tmp_W8^tmp_W3,1u);"
		"uint tmp_W12=rotate(tmp_W9^tmp_W4,1u);"
		"uint tmp_W13=rotate(tmp_W10^tmp_W5^%uu,1u);"
		"uint tmp_W14=rotate(tmp_W11^tmp_W6^tmp_W0,1u);"
		"uint tmp_W15=rotate(%uu^tmp_W12^tmp_W7^tmp_W1,1u);"
		, nt_buffer[2]
		, nt_buffer[1], nt_buffer[3]
		, nt_buffer[2], key_lenght << 3, nt_buffer[4]
		, nt_buffer[3], nt_buffer[5]
		, nt_buffer[4], nt_buffer[6]
		, nt_buffer[5]
		, nt_buffer[6]
		, key_lenght << 3





		, key_lenght << 3

		, key_lenght << 3);

	sprintf(source + strlen(source),
		"uint tmp_W16=rotate(tmp_W0 ^tmp_W13^tmp_W8 ^tmp_W2 ,1u);"
		"uint tmp_W17=rotate(tmp_W1 ^tmp_W14^tmp_W9 ^tmp_W3 ,1u);"
		"uint tmp_W18=rotate(tmp_W2 ^tmp_W15^tmp_W10^tmp_W4 ,1u);"
		"uint tmp_W19=rotate(tmp_W3 ^tmp_W16^tmp_W11^tmp_W5 ,1u);"
		"uint tmp_W20=rotate(tmp_W4 ^tmp_W17^tmp_W12^tmp_W6 ,1u);"
		"uint tmp_W21=rotate(tmp_W5 ^tmp_W18^tmp_W13^tmp_W7 ,1u);"
		"uint tmp_W22=rotate(tmp_W6 ^tmp_W19^tmp_W14^tmp_W8 ,1u);"
		"uint tmp_W23=rotate(tmp_W7 ^tmp_W20^tmp_W15^tmp_W9 ,1u);"
		"uint tmp_W24=rotate(tmp_W8 ^tmp_W21^tmp_W16^tmp_W10,1u);"
		"uint tmp_W25=rotate(tmp_W9 ^tmp_W22^tmp_W17^tmp_W11,1u);"
		"uint tmp_W26=rotate(tmp_W10^tmp_W23^tmp_W18^tmp_W12,1u);"
		"uint tmp_W27=rotate(tmp_W11^tmp_W24^tmp_W19^tmp_W13,1u);"
		"uint tmp_W28=rotate(tmp_W12^tmp_W25^tmp_W20^tmp_W14,1u);"//3
		"uint tmp_W29=rotate(tmp_W13^tmp_W26^tmp_W21^tmp_W15,1u);"
		"uint tmp_W30=rotate(tmp_W14^tmp_W27^tmp_W22^tmp_W16,1u);"
		"uint tmp_W31=rotate(tmp_W15^tmp_W28^tmp_W23^tmp_W17,1u);");

#ifdef HS_OCL_CURRENT_KEY_AS_REGISTERS
	cl_uint bits_by_char;
	_BitScanReverse(&bits_by_char, ceil_power_2(num_char_in_charset));
	cl_uint key_mask = ceil_power_2(num_char_in_charset) - 1;
#endif

	// Pre-Calculate W
	sprintf(source + strlen(source),
		"local uint tmp_W[128];"
		"uint lid=get_local_id(0);"
		"uint lsize=get_local_size(0);");

		if (key_lenght == 1)
			sprintf(source + strlen(source), "uint lidx_base=get_group_id(0)*lsize;");
		else
#ifdef HS_OCL_CURRENT_KEY_AS_REGISTERS
			sprintf(source + strlen(source), "uint lidx_base=get_group_id(0)*lsize+(current_key0&%uu)+((current_key0>>%uu)&%uu)*NUM_CHAR_IN_CHARSET+((current_key0>>%uu)&%uu)*NUM_CHAR_IN_CHARSET*NUM_CHAR_IN_CHARSET;"
							, key_mask, bits_by_char, key_mask, 2 * bits_by_char, key_mask);
#else
			sprintf(source + strlen(source), "uint lidx_base=get_group_id(0)*lsize+current_key[1]+current_key[2]*NUM_CHAR_IN_CHARSET+current_key[3]*NUM_CHAR_IN_CHARSET*NUM_CHAR_IN_CHARSET;");
#endif

	sprintf(source + strlen(source),
		"lsize--;"
		"uint lidx_end=(lidx_base+lsize)/%uu;"
		"uint lidx=(lidx_base+lid)/%uu;"
		"lidx_base=lidx_base/%uu;"

		"lidx=(lidx==lidx_base)?0:64u;"

		"if(lid==0||(lidx_end!=lidx_base&&lid==lsize))"
		"{"
			"tmp_W[lidx+32]=rotate(tmp_W16^tmp_W29^tmp_W24^tmp_W18,1u);"//5
			"tmp_W[lidx+33]=rotate(tmp_W17^tmp_W30^tmp_W25^tmp_W19,1u);"
			"tmp_W[lidx+34]=rotate(tmp_W18^tmp_W31^tmp_W26^tmp_W20,1u);"
			"tmp_W[lidx+35]=rotate(tmp_W19^tmp_W[lidx+32]^tmp_W27^tmp_W21,1u);"
			"tmp_W[lidx+36]=rotate(tmp_W20^tmp_W[lidx+33]^tmp_W28^tmp_W22,1u);"//3
			"tmp_W[lidx+37]=rotate(tmp_W21^tmp_W[lidx+34]^tmp_W29^tmp_W23,1u);"
			"tmp_W[lidx+38]=rotate(tmp_W22^tmp_W[lidx+35]^tmp_W30^tmp_W24,1u);"//3
			"tmp_W[lidx+39]=rotate(tmp_W23^tmp_W[lidx+36]^tmp_W31^tmp_W25,1u);"
			"tmp_W[lidx+40]=rotate(tmp_W24^tmp_W[lidx+37]^tmp_W[lidx+32]^tmp_W26,1u);"//5
			"tmp_W[lidx+41]=rotate(tmp_W25^tmp_W[lidx+38]^tmp_W[lidx+33]^tmp_W27,1u);"
			"tmp_W[lidx+42]=rotate(tmp_W26^tmp_W[lidx+39]^tmp_W[lidx+34]^tmp_W28,1u);"//3
			"tmp_W[lidx+43]=rotate(tmp_W27^tmp_W[lidx+40]^tmp_W[lidx+35]^tmp_W29,1u);"
			"tmp_W[lidx+44]=rotate(tmp_W28^tmp_W[lidx+41]^tmp_W[lidx+36]^tmp_W30,1u);"//5
			"tmp_W[lidx+45]=rotate(tmp_W29^tmp_W[lidx+42]^tmp_W[lidx+37]^tmp_W31,1u);"
			"tmp_W[lidx+46]=rotate(tmp_W30^tmp_W[lidx+43]^tmp_W[lidx+38]^tmp_W[lidx+32],1u);"//4
			"tmp_W[lidx+47]=rotate(tmp_W31^tmp_W[lidx+44]^tmp_W[lidx+39]^tmp_W[lidx+33],1u);"

			"tmp_W[lidx+48]=rotate(tmp_W[lidx+32]^tmp_W[lidx+45]^tmp_W[lidx+40]^tmp_W[lidx+34],1u);"//6
			"tmp_W[lidx+49]=rotate(tmp_W[lidx+33]^tmp_W[lidx+46]^tmp_W[lidx+41]^tmp_W[lidx+35],1u);"
			"tmp_W[lidx+50]=rotate(tmp_W[lidx+34]^tmp_W[lidx+47]^tmp_W[lidx+42]^tmp_W[lidx+36],1u);"
			"tmp_W[lidx+51]=rotate(tmp_W[lidx+35]^tmp_W[lidx+48]^tmp_W[lidx+43]^tmp_W[lidx+37],1u);"
			"tmp_W[lidx+52]=rotate(tmp_W[lidx+36]^tmp_W[lidx+49]^tmp_W[lidx+44]^tmp_W[lidx+38],1u);"//3
			"tmp_W[lidx+53]=rotate(tmp_W[lidx+37]^tmp_W[lidx+50]^tmp_W[lidx+45]^tmp_W[lidx+39],1u);"
			"tmp_W[lidx+54]=rotate(tmp_W[lidx+38]^tmp_W[lidx+51]^tmp_W[lidx+46]^tmp_W[lidx+40],1u);"
			"tmp_W[lidx+55]=rotate(tmp_W[lidx+39]^tmp_W[lidx+52]^tmp_W[lidx+47]^tmp_W[lidx+41],1u);"
			"tmp_W[lidx+56]=rotate(tmp_W[lidx+40]^tmp_W[lidx+53]^tmp_W[lidx+48]^tmp_W[lidx+42],1u);"
			"tmp_W[lidx+57]=rotate(tmp_W[lidx+41]^tmp_W[lidx+54]^tmp_W[lidx+49]^tmp_W[lidx+43],1u);"
			"tmp_W[lidx+58]=rotate(tmp_W[lidx+42]^tmp_W[lidx+55]^tmp_W[lidx+50]^tmp_W[lidx+44],1u);"
			"tmp_W[lidx+63]=rotate(tmp_W[lidx+47]^rotate(tmp_W[lidx+44]^tmp_W[lidx+57]^tmp_W[lidx+52]^tmp_W[lidx+46],1u)^tmp_W[lidx+55]^tmp_W[lidx+49],1u);"

			"tmp_W[lidx+37]+=CONST3;"
			"tmp_W[lidx+49]+=CONST4;"
			"tmp_W[lidx+53]+=CONST4;"
		"}"
		"barrier(CLK_LOCAL_MEM_FENCE);"

		"tmp_W1+=SQRT_2;"
		"tmp_W2+=SQRT_2;"
		"tmp_W4+=SQRT_3;"
		"tmp_W5+=SQRT_3;"
		"tmp_W7+=SQRT_3;"
		"tmp_W10+=SQRT_3;"
		"tmp_W11+=SQRT_3;"
		"tmp_W13+=SQRT_3;"
		"tmp_W17+=SQRT_3;"
		"tmp_W23+=SQRT_3;"
		"tmp_W25+=CONST3;"
		"tmp_W29+=CONST3;"
		, POW3(num_char_in_charset), POW3(num_char_in_charset), POW3(num_char_in_charset));

	char* tmp_W[] = {
		"tmp_W0", "tmp_W1", "tmp_W2", "tmp_W3", "tmp_W4", "tmp_W5", "tmp_W6", "tmp_W7", "tmp_W8", "tmp_W9", "tmp_W10", "tmp_W11", "tmp_W12", "tmp_W13", "tmp_W14", "tmp_W15",
		"tmp_W16", "tmp_W17", "tmp_W18", "tmp_W19", "tmp_W20", "tmp_W21", "tmp_W22", "tmp_W23", "tmp_W24", "tmp_W25", "tmp_W26", "tmp_W27", "tmp_W28", "tmp_W29", "tmp_W30", "tmp_W31",
		"tmp_W[lidx+32]", "tmp_W[lidx+33]", "tmp_W[lidx+34]", "tmp_W[lidx+35]", "tmp_W[lidx+36]", "tmp_W[lidx+37]", "tmp_W[lidx+38]", "tmp_W[lidx+39]", "tmp_W[lidx+40]", "tmp_W[lidx+41]", "tmp_W[lidx+42]", "tmp_W[lidx+43]", "tmp_W[lidx+44]", "tmp_W[lidx+45]", "tmp_W[lidx+46]", "tmp_W[lidx+47]",
		"tmp_W[lidx+48]", "tmp_W[lidx+49]", "tmp_W[lidx+50]", "tmp_W[lidx+51]", "tmp_W[lidx+52]", "tmp_W[lidx+53]", "tmp_W[lidx+54]", "tmp_W[lidx+55]", "tmp_W[lidx+56]", "tmp_W[lidx+57]", "tmp_W[lidx+58]", "tmp_W[lidx+59]", "tmp_W[lidx+60]", "tmp_W[lidx+61]", "tmp_W[lidx+62]", "tmp_W[lidx+63]"
	};

	ocl_gen_kernel(source, key_lenght, nt_buffer, output_size, tmp_W);
}
PRIVATE void ocl_gen_kernel_with_lenght_all_reg(char* source, cl_uint key_lenght, cl_uint vector_size, cl_uint ntlm_size_bit_table, cl_uint output_size, DivisionParams div_param, char** str_comp, cl_bool value_map_collission, cl_uint workgroup)
{
	char* nt_buffer[] = { "+nt_buffer0", "+nt_buffer1", "+nt_buffer2", "+nt_buffer3", "+nt_buffer4", "+nt_buffer5", "+nt_buffer6" };

	ocl_charset_load_buffer_be(source, key_lenght, &vector_size, div_param, nt_buffer);

	// Pre-Calculate W
	sprintf(source + strlen(source),
		"uint A,B,C,D,E;"
		"uint tmp_W0=rotate((0u%s),1u);"
		"uint tmp_W1=rotate((0u%s)^(0u%s),1u);"
		"uint tmp_W2=rotate((0u%s)^%uu^(0u%s),1u);"
		"uint tmp_W3=rotate((0u%s)^tmp_W0^(0u%s),1u);"
		"uint tmp_W4=rotate((0u%s)^tmp_W1^(0u%s),1u);"
		"uint tmp_W5=rotate((0u%s)^tmp_W2,1u);"
		"uint tmp_W6=rotate((0u%s)^tmp_W3,1u);"
		"uint tmp_W7=rotate(tmp_W4^%uu,1u);"
		"uint tmp_W8=rotate(tmp_W5^tmp_W0,1u);"
		"uint tmp_W9=rotate(tmp_W6^tmp_W1,1u);"
		"uint tmp_W10=rotate(tmp_W7^tmp_W2,1u);"
		"uint tmp_W11=rotate(tmp_W8^tmp_W3,1u);"
		"uint tmp_W12=rotate(tmp_W9^tmp_W4,1u);"
		"uint tmp_W13=rotate(tmp_W10^tmp_W5^%uu,1u);"
		"uint tmp_W14=rotate(tmp_W11^tmp_W6^tmp_W0,1u);"
		"uint tmp_W15=rotate(%uu^tmp_W12^tmp_W7^tmp_W1,1u);"
		, nt_buffer[2]
		, nt_buffer[1], nt_buffer[3]
		, nt_buffer[2], key_lenght << 3, nt_buffer[4]
		, nt_buffer[3], nt_buffer[5]
		, nt_buffer[4], nt_buffer[6]
		, nt_buffer[5]
		, nt_buffer[6]
		, key_lenght << 3





		, key_lenght << 3

		, key_lenght << 3);

	sprintf(source + strlen(source),
		"uint tmp_W16=rotate(tmp_W0 ^tmp_W13^tmp_W8 ^tmp_W2 ,1u);"
		"uint tmp_W17=rotate(tmp_W1 ^tmp_W14^tmp_W9 ^tmp_W3 ,1u);"
		"uint tmp_W18=rotate(tmp_W2 ^tmp_W15^tmp_W10^tmp_W4 ,1u);"
		"uint tmp_W19=rotate(tmp_W3 ^tmp_W16^tmp_W11^tmp_W5 ,1u);"
		"uint tmp_W20=rotate(tmp_W4 ^tmp_W17^tmp_W12^tmp_W6 ,1u);"
		"uint tmp_W21=rotate(tmp_W5 ^tmp_W18^tmp_W13^tmp_W7 ,1u);"
		"uint tmp_W22=rotate(tmp_W6 ^tmp_W19^tmp_W14^tmp_W8 ,1u);"
		"uint tmp_W23=rotate(tmp_W7 ^tmp_W20^tmp_W15^tmp_W9 ,1u);"
		"uint tmp_W24=rotate(tmp_W8 ^tmp_W21^tmp_W16^tmp_W10,1u);"
		"uint tmp_W25=rotate(tmp_W9 ^tmp_W22^tmp_W17^tmp_W11,1u);"
		"uint tmp_W26=rotate(tmp_W10^tmp_W23^tmp_W18^tmp_W12,1u);"
		"uint tmp_W27=rotate(tmp_W11^tmp_W24^tmp_W19^tmp_W13,1u);"
		"uint tmp_W28=rotate(tmp_W12^tmp_W25^tmp_W20^tmp_W14,1u);"//3
		"uint tmp_W29=rotate(tmp_W13^tmp_W26^tmp_W21^tmp_W15,1u);"
		"uint tmp_W30=rotate(tmp_W14^tmp_W27^tmp_W22^tmp_W16,1u);"
		"uint tmp_W31=rotate(tmp_W15^tmp_W28^tmp_W23^tmp_W17,1u);"

		"uint tmp_W32=rotate(tmp_W16^tmp_W29^tmp_W24^tmp_W18,1u);"//5
		"uint tmp_W33=rotate(tmp_W17^tmp_W30^tmp_W25^tmp_W19,1u);"
		"uint tmp_W34=rotate(tmp_W18^tmp_W31^tmp_W26^tmp_W20,1u);"
		"uint tmp_W35=rotate(tmp_W19^tmp_W32^tmp_W27^tmp_W21,1u);"
		"uint tmp_W36=rotate(tmp_W20^tmp_W33^tmp_W28^tmp_W22,1u);"//3
		"uint tmp_W37=rotate(tmp_W21^tmp_W34^tmp_W29^tmp_W23,1u);"
		"uint tmp_W38=rotate(tmp_W22^tmp_W35^tmp_W30^tmp_W24,1u);"//3
		"uint tmp_W39=rotate(tmp_W23^tmp_W36^tmp_W31^tmp_W25,1u);"
		"uint tmp_W40=rotate(tmp_W24^tmp_W37^tmp_W32^tmp_W26,1u);"//5
		"uint tmp_W41=rotate(tmp_W25^tmp_W38^tmp_W33^tmp_W27,1u);"
		"uint tmp_W42=rotate(tmp_W26^tmp_W39^tmp_W34^tmp_W28,1u);"//3
		"uint tmp_W43=rotate(tmp_W27^tmp_W40^tmp_W35^tmp_W29,1u);"
		"uint tmp_W44=rotate(tmp_W28^tmp_W41^tmp_W36^tmp_W30,1u);"//5
		"uint tmp_W45=rotate(tmp_W29^tmp_W42^tmp_W37^tmp_W31,1u);"
		"uint tmp_W46=rotate(tmp_W30^tmp_W43^tmp_W38^tmp_W32,1u);"//4
		"uint tmp_W47=rotate(tmp_W31^tmp_W44^tmp_W39^tmp_W33,1u);"

		"uint tmp_W48=rotate(tmp_W32^tmp_W45^tmp_W40^tmp_W34,1u);"//6
		"uint tmp_W49=rotate(tmp_W33^tmp_W46^tmp_W41^tmp_W35,1u);"
		"uint tmp_W50=rotate(tmp_W34^tmp_W47^tmp_W42^tmp_W36,1u);"
		"uint tmp_W51=rotate(tmp_W35^tmp_W48^tmp_W43^tmp_W37,1u);"
		"uint tmp_W52=rotate(tmp_W36^tmp_W49^tmp_W44^tmp_W38,1u);"//3
		"uint tmp_W53=rotate(tmp_W37^tmp_W50^tmp_W45^tmp_W39,1u);"
		"uint tmp_W54=rotate(tmp_W38^tmp_W51^tmp_W46^tmp_W40,1u);"
		"uint tmp_W55=rotate(tmp_W39^tmp_W52^tmp_W47^tmp_W41,1u);"
		"uint tmp_W56=rotate(tmp_W40^tmp_W53^tmp_W48^tmp_W42,1u);"
		"uint tmp_W57=rotate(tmp_W41^tmp_W54^tmp_W49^tmp_W43,1u);"
		"uint tmp_W58=rotate(tmp_W42^tmp_W55^tmp_W50^tmp_W44,1u);"
		"uint tmp_W63=rotate(tmp_W47^rotate(tmp_W44^tmp_W57^tmp_W52^tmp_W46,1u)^tmp_W55^tmp_W49,1u);"

		"tmp_W2 +=SQRT_2;"
		"tmp_W5 +=SQRT_3;"
		"tmp_W1 +=SQRT_2;"
		"tmp_W4 +=SQRT_3;"
		"tmp_W7 +=SQRT_3;"
		"tmp_W10+=SQRT_3;"
		"tmp_W11+=SQRT_3;"
		"tmp_W13+=SQRT_3;"

		"tmp_W25+=CONST3;"
		"tmp_W29+=CONST3;"
		"tmp_W37+=CONST3;"

		"tmp_W49+=CONST4;"
		"tmp_W17+=SQRT_3;"
		"tmp_W23+=SQRT_3;"

		"tmp_W53+=CONST4;");
	
	char* tmp_W[] = {
		"tmp_W0", "tmp_W1", "tmp_W2", "tmp_W3", "tmp_W4", "tmp_W5", "tmp_W6", "tmp_W7", "tmp_W8", "tmp_W9", "tmp_W10", "tmp_W11", "tmp_W12", "tmp_W13", "tmp_W14", "tmp_W15",
		"tmp_W16", "tmp_W17", "tmp_W18", "tmp_W19", "tmp_W20", "tmp_W21", "tmp_W22", "tmp_W23", "tmp_W24", "tmp_W25", "tmp_W26", "tmp_W27", "tmp_W28", "tmp_W29", "tmp_W30", "tmp_W31",
		"tmp_W32", "tmp_W33", "tmp_W34", "tmp_W35", "tmp_W36", "tmp_W37", "tmp_W38", "tmp_W39", "tmp_W40", "tmp_W41", "tmp_W42", "tmp_W43", "tmp_W44", "tmp_W45", "tmp_W46", "tmp_W47",
		"tmp_W48", "tmp_W49", "tmp_W50", "tmp_W51", "tmp_W52", "tmp_W53", "tmp_W54", "tmp_W55", "tmp_W56", "tmp_W57", "tmp_W58", "tmp_W59", "tmp_W60", "tmp_W61", "tmp_W62", "tmp_W63"
	};

	ocl_gen_kernel(source, key_lenght, nt_buffer, output_size, tmp_W);
}
PRIVATE void ocl_gen_kernel_with_lenght_less_reg(char* source, cl_uint key_lenght, cl_uint vector_size, cl_uint ntlm_size_bit_table, cl_uint output_size, DivisionParams div_param, char** str_comp, cl_bool value_map_collission, cl_uint workgroup)
{
	char* nt_buffer[] = { "+nt_buffer0", "+nt_buffer1", "+nt_buffer2", "+nt_buffer3", "+nt_buffer4", "+nt_buffer5", "+nt_buffer6" };

	ocl_charset_load_buffer_be(source, key_lenght, &vector_size, div_param, nt_buffer);

	sprintf(source + strlen(source), "uint A,B,C,D,E,W0,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14,W15;");

	if (is_charset_consecutive(charset))
		sprintf(source + strlen(source), "nt_buffer0+=%uu;", is_charset_consecutive(charset) << 24u);

	// Begin cycle changing first character
	sprintf(source + strlen(source), "for(uint i=0;i<%uU;i+=%uU){", num_char_in_charset, vector_size);

	if (is_charset_consecutive(charset))
		sprintf(source + strlen(source), "W0=nt_buffer0+(i<<24u);");
	else
		sprintf(source + strlen(source), "W0=nt_buffer0+(((uint)charset[i])<<24u);");

	/* Round 1 */
	sprintf(source + strlen(source),
		"E=0x9fb498b3+W0;"
		"D=rotate(E,5u)+0x66b0cd0d%s;"
		"C=rotate(D,5u)+(0x7bf36ae2^(E&0x22222222))+0xf33d5697%s;E=rotate(E,30u);"
		"B=rotate(C,5u)+(0x59d148c0^(D&(E^0x59d148c0)))+0xd675e47b%s;D=rotate(D,30u);"
		"A=rotate(B,5u)+bs(E,D,C)+0xb453c259%s;C=rotate(C,30u);"

		"E+=rotate(A,5u)+bs(D,C,B)+SQRT_2%s;B=rotate(B,30u);"
		"D+=rotate(E,5u)+bs(C,B,A)+SQRT_2%s;A=rotate(A,30u);"
		"C+=rotate(D,5u)+bs(B,A,E)+SQRT_2;E=rotate(E,30u);"
		"B+=rotate(C,5u)+bs(A,E,D)+SQRT_2;D=rotate(D,30u);"
		"A+=rotate(B,5u)+bs(E,D,C)+SQRT_2;C=rotate(C,30u);"
		"E+=rotate(A,5u)+bs(D,C,B)+SQRT_2;B=rotate(B,30u);"
		"D+=rotate(E,5u)+bs(C,B,A)+SQRT_2;A=rotate(A,30u);"
		"C+=rotate(D,5u)+bs(B,A,E)+SQRT_2;E=rotate(E,30u);"
		"B+=rotate(C,5u)+bs(A,E,D)+SQRT_2;D=rotate(D,30u);"
		"A+=rotate(B,5u)+bs(E,D,C)+SQRT_2;C=rotate(C,30u);"
		"E+=rotate(A,5u)+bs(D,C,B)+%uu;B=rotate(B,30u);"
		, nt_buffer[1], nt_buffer[2], nt_buffer[3], nt_buffer[4], nt_buffer[5], nt_buffer[6], (key_lenght << 3) + SQRT_2);

	sprintf(source + strlen(source),
		"W0=rotate(W0^(0u%s),1u);"
		"W1=rotate((0u%s)^(0u%s),1u);"
		"W2=rotate((0u%s)^%uu^(0u%s),1u);"
		"W3=rotate((0u%s)^W0^(0u%s),1u);"
		"W4=rotate((0u%s)^W1^(0u%s),1u);"
		"W5=rotate((0u%s)^W2,1u);"
		"W6=rotate((0u%s)^W3,1u);"
		"W7=rotate(W4^%uu,1u);"
		"W8=rotate(W5^W0,1u);"
		"W9=rotate(W6^W1,1u);"
		"W10=rotate(W7^W2,1u);"
		"W11=rotate(W8^W3,1u);"
		"W12=rotate(W9^W4,1u);"
		"W13=rotate(W10^W5^%uu,1u);"
		"W14=rotate(W11^W6^W0,1u);"
		"W15=rotate(%uu^W12^W7^W1,1u);"
		, nt_buffer[2]
		, nt_buffer[1], nt_buffer[3]
		, nt_buffer[2], key_lenght << 3, nt_buffer[4]
		, nt_buffer[3], nt_buffer[5]
		, nt_buffer[4], nt_buffer[6]
		, nt_buffer[5]
		, nt_buffer[6]
		, key_lenght << 3





		, key_lenght << 3
		
		, key_lenght << 3);

	/* Round 2 */
	sprintf(source + strlen(source),
		"D+=rotate(E,5u)+bs(C,B,A)+SQRT_2+W0;A=rotate(A,30u);"
		"C+=rotate(D,5u)+bs(B,A,E)+SQRT_2+W1;E=rotate(E,30u);"
		"B+=rotate(C,5u)+bs(A,E,D)+SQRT_2+W2;D=rotate(D,30u);"
		"A+=rotate(B,5u)+bs(E,D,C)+SQRT_2+W3;C=rotate(C,30u);"

		"E+=rotate(A,5u)+(B^C^D)+SQRT_3+W4;B=rotate(B,30u);"
		"D+=rotate(E,5u)+(A^B^C)+SQRT_3+W5;A=rotate(A,30u);"
		"C+=rotate(D,5u)+(E^A^B)+SQRT_3+W6;E=rotate(E,30u);"
		"B+=rotate(C,5u)+(D^E^A)+SQRT_3+W7;D=rotate(D,30u);"
		"A+=rotate(B,5u)+(C^D^E)+SQRT_3+W8;C=rotate(C,30u);"
		"E+=rotate(A,5u)+(B^C^D)+SQRT_3+W9;B=rotate(B,30u);"
		"D+=rotate(E,5u)+(A^B^C)+SQRT_3+W10;A=rotate(A,30u);"
		"C+=rotate(D,5u)+(E^A^B)+SQRT_3+W11;E=rotate(E,30u);"
		"B+=rotate(C,5u)+(D^E^A)+SQRT_3+W12;D=rotate(D,30u);"
		"A+=rotate(B,5u)+(C^D^E)+SQRT_3+W13;C=rotate(C,30u);"
		"E+=rotate(A,5u)+(B^C^D)+SQRT_3+W14;B=rotate(B,30u);"
		"D+=rotate(E,5u)+(A^B^C)+SQRT_3+W15;A=rotate(A,30u);"
		"DCC2_R(0,13,8,2);C+=rotate(D,5u)+(E^A^B)+SQRT_3+W0;E=rotate(E,30u);"
		"DCC2_R(1,14,9,3);B+=rotate(C,5u)+(D^E^A)+SQRT_3+W1;D=rotate(D,30u);"
		"DCC2_R(2,15,10,4);A+=rotate(B,5u)+(C^D^E)+SQRT_3+W2;C=rotate(C,30u);"
		"DCC2_R(3,0,11,5);E+=rotate(A,5u)+(B^C^D)+SQRT_3+W3;B=rotate(B,30u);"
		"DCC2_R(4,1,12,6);D+=rotate(E,5u)+(A^B^C)+SQRT_3+W4;A=rotate(A,30u);"
		"DCC2_R(5,2,13,7);C+=rotate(D,5u)+(E^A^B)+SQRT_3+W5;E=rotate(E,30u);"
		"DCC2_R(6,3,14,8);B+=rotate(C,5u)+(D^E^A)+SQRT_3+W6;D=rotate(D,30u);"
		"DCC2_R(7,4,15,9);A+=rotate(B,5u)+(C^D^E)+SQRT_3+W7;C=rotate(C,30u);");

	/* Round 3 */
	sprintf(source + strlen(source),
		"DCC2_R(8,5,0,10);E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+W8;B=rotate(B,30u);"
		"DCC2_R(9,6,1,11);D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+W9;A=rotate(A,30u);"
		"DCC2_R(10,7,2,12);C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+W10;E=rotate(E,30u);"
		"DCC2_R(11,8,3,13);B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+W11;D=rotate(D,30u);"
		"DCC2_R(12,9,4,14);A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+W12;C=rotate(C,30u);"
		"DCC2_R(13,10,5,15);E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+W13;B=rotate(B,30u);"
		"DCC2_R(14,11,6,0);D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+W14;A=rotate(A,30u);"
		"DCC2_R(15,12,7,1);C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+W15;E=rotate(E,30u);"
		"DCC2_R(0,13,8,2);B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+W0;D=rotate(D,30u);"
		"DCC2_R(1,14,9,3);A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+W1;C=rotate(C,30u);"
		"DCC2_R(2,15,10,4);E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+W2;B=rotate(B,30u);"
		"DCC2_R(3,0,11,5);D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+W3;A=rotate(A,30u);"
		"DCC2_R(4,1,12,6);C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+W4;E=rotate(E,30u);"
		"DCC2_R(5,2,13,7);B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+W5;D=rotate(D,30u);"
		"DCC2_R(6,3,14,8);A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+W6;C=rotate(C,30u);"
		"DCC2_R(7,4,15,9);E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+W7;B=rotate(B,30u);"
		"DCC2_R(8,5,0,10);D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+W8;A=rotate(A,30u);"
		"DCC2_R(9,6,1,11);C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+W9;E=rotate(E,30u);"
		"DCC2_R(10,7,2,12);B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+W10;D=rotate(D,30u);"
		"DCC2_R(11,8,3,13);A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+W11;C=rotate(C,30u);");

	/* Round 4 */
	sprintf(source + strlen(source),
		"DCC2_R(12,9,4,14);E+=rotate(A,5u)+(B^C^D)+CONST4+W12;B=rotate(B,30u);"
		"DCC2_R(13,10,5,15);D+=rotate(E,5u)+(A^B^C)+CONST4+W13;A=rotate(A,30u);"
		"DCC2_R(14,11,6,0);C+=rotate(D,5u)+(E^A^B)+CONST4+W14;E=rotate(E,30u);"
		"DCC2_R(15,12,7,1);B+=rotate(C,5u)+(D^E^A)+CONST4+W15;D=rotate(D,30u);"
		"DCC2_R(0,13,8,2);A+=rotate(B,5u)+(C^D^E)+CONST4+W0;C=rotate(C,30u);"
		"DCC2_R(1,14,9,3);E+=rotate(A,5u)+(B^C^D)+CONST4+W1;B=rotate(B,30u);"
		"DCC2_R(2,15,10,4);D+=rotate(E,5u)+(A^B^C)+CONST4+W2;A=rotate(A,30u);"
		"DCC2_R(3,0,11,5);C+=rotate(D,5u)+(E^A^B)+CONST4+W3;E=rotate(E,30u);"
		"DCC2_R(4,1,12,6);B+=rotate(C,5u)+(D^E^A)+CONST4+W4;D=rotate(D,30u);"
		"DCC2_R(5,2,13,7);A+=rotate(B,5u)+(C^D^E)+CONST4+W5;C=rotate(C,30u);"
		"DCC2_R(6,3,14,8);E+=rotate(A,5u)+(B^C^D)+CONST4+W6;B=rotate(B,30u);"
		"DCC2_R(7,4,15,9);D+=rotate(E,5u)+(A^B^C)+CONST4+W7;A=rotate(A,30u);"
		"DCC2_R(8,5,0,10);C+=rotate(D,5u)+(E^A^B)+CONST4+W8;E=rotate(E,30u);"
		"DCC2_R(9,6,1,11);B+=rotate(C,5u)+(D^E^A)+CONST4+W9;D=rotate(D,30u);"
		"DCC2_R(10,7,2,12);A+=rotate(B,5u)+(C^D^E)+CONST4+W10;"

		"DCC2_R(12,9,4,14);DCC2_R(15,12,7,1);A=rotate(A,30u)+W15;");

	// Find match
	if (num_passwords_loaded == 1)
	{
		unsigned int* bin = (unsigned int*)binary_values;
		sprintf(source + strlen(source),
				"if(A==%uu)"
				"{"
					"A=rotate(A-W15,2u);"
					"C=rotate(C,30u);"
					"W11=rotate(W11^W8^W3^W13,1u);"
					"E+=rotate(A,5u)+(B^C^D)+CONST4+W11;B=rotate(B,30u);"

					"D+=rotate(E,5u)+(A^B^C)+CONST4+W12;A=rotate(A,30u);"

					"W15=rotate(W15,31u);W15^=W12^W7^W1;"
					"C+=rotate(D,5u)+(E^A^B)+CONST4+rotate(W13^W10^W5^W15,1u);"

					"B+=(D^rotate(E,30u)^A)+rotate(W14^W11^W6^W0,1u);"

					"if(B==%uu&&C==%uu&&D==%uu&&E==%uu)"
					"{"
						"output[0]=1u;"
						"output[1]=get_global_id(0)*NUM_CHAR_IN_CHARSET+i;"
						"output[2]=0;"
					"}"
				"}"
				, bin[0], bin[1], bin[2], bin[3], bin[4]);
	}
	else
	{
		sprintf(source + strlen(source),
			"indx=A&SIZE_BIT_TABLE;"
			"if((bit_table[indx>>5u]>>(indx&31u))&1u)"
			"{"
				"indx=table[A & SIZE_TABLE];"

				"while(indx!=0xffffffff)"
				//"if(indx!=0xffffffff)"
				"{"
					"if(A==binary_values[indx*5u+0u])"
					"{"
						"uint aa=rotate(A-W15,2u);"
						"uint cc=rotate(C,30u);"
						"uint ww11=rotate(W11^W8^W3^W13,1u);"
						"uint ee=E+rotate(aa,5u)+(B^cc^D)+CONST4+ww11;uint bb=rotate(B,30u);"

						"uint dd=D+rotate(ee,5u)+(aa^bb^cc)+CONST4+W12;aa=rotate(aa,30u);"

						"uint ww15=rotate(W15,31u);ww15^=W12^W7^W1;"
						"cc+=rotate(dd,5u)+(ee^aa^bb)+CONST4+rotate(W13^W10^W5^ww15,1u);"

						"bb+=(dd^rotate(ee,30u)^aa)+rotate(W14^ww11^W6^W0,1u);"

						"if(bb==binary_values[indx*5u+1u]&&cc==binary_values[indx*5u+2u]&&dd==binary_values[indx*5u+3u]&&ee==binary_values[indx*5u+4u])"
						"{"
							"uint found=atomic_inc(output);"
							"if(found<%uu){"
							"output[2*found+1]=get_global_id(0)*NUM_CHAR_IN_CHARSET+i;"
							"output[2*found+2]=indx;}"
						"}", output_size);

	strcat(source, "}"
					"indx=same_hash_next[indx];"
				"}"
			"}");
	}

	strcat(source, "}}");
}

PRIVATE void ocl_protocol_charset_init(OpenCL_Param* param, cl_uint gpu_index, generate_key_funtion* gen, gpu_crypt_funtion** gpu_crypt)
{
	cl_uint keys_opencl_divider = 4 * (num_passwords_loaded == 1 ? 2 : 1);
	cl_uint sha1_empy_hash[] = {0xc59a63c5, 0xd6c964e2,	0x666b8bc6, 0x14b7106a, 0xb0149467};

#ifdef HS_OCL_REDUCE_REGISTER_USE
	ocl_charset_init(param, gpu_index, gen, gpu_crypt, BINARY_SIZE, 0, ocl_write_sha1_header_charset, ocl_gen_kernel_with_lenght_less_reg, sha1_empy_hash, CL_FALSE, keys_opencl_divider);
#else
	ocl_gen_kernel_with_lenght_func* gen_kernels[5];

	if (POW3(num_char_in_charset) < gpu_devices[gpu_index].max_work_group_size)
	{
		gen_kernels[0] = ocl_gen_kernel_with_lenght_all_reg;
		gen_kernels[1] = ocl_gen_kernel_with_lenght_less_reg;
		gen_kernels[2] = NULL;
	}
	else
	{
		if (gpu_devices[gpu_index].vendor == OCL_VENDOR_NVIDIA)
		{
			gen_kernels[0] = ocl_gen_kernel_with_lenght_local;
			gen_kernels[1] = ocl_gen_kernel_with_lenght_mixed;
			gen_kernels[2] = ocl_gen_kernel_with_lenght_less_reg;
			gen_kernels[3] = ocl_gen_kernel_with_lenght_all_reg;
		}
		else
		{
			gen_kernels[0] = ocl_gen_kernel_with_lenght_all_reg;
			gen_kernels[1] = ocl_gen_kernel_with_lenght_less_reg;
			gen_kernels[2] = ocl_gen_kernel_with_lenght_mixed;
			gen_kernels[3] = ocl_gen_kernel_with_lenght_local;
		}
		gen_kernels[4] = NULL;
	}

	ocl_charset_kernels_init(param, gpu_index, gen, gpu_crypt, BINARY_SIZE, 0, ocl_write_sha1_header_charset, gen_kernels, sha1_empy_hash, keys_opencl_divider);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Common
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
PRIVATE void ocl_gen_kernel_sha1(char* source, char* kernel_name, ocl_begin_rule_funtion* ocl_load, ocl_write_code* ocl_end, char* found_param_3, int* aditional_param, cl_uint lenght, cl_uint NUM_KEYS_OPENCL, cl_uint value_map_collission, void* salt_param, cl_uint prefered_vector_size)
{
	char nt_buffer[16][16];
	char buffer_vector_size[16];
	// Needed when use a rule with more than one param
	int found_multiplier = found_param_3 ? 3 : 2;
	char output_3[64];
	output_3[0] = 0;

	// Function definition
	sprintf(source + strlen(source), "\n__kernel void %s(const __global uint* keys,__global uint* restrict output", kernel_name);

	if (num_passwords_loaded > 1)
		strcat(source, ",const __global uint* restrict table,const __global uint* restrict binary_values,const __global uint* restrict same_hash_next,const __global uint* restrict bit_table");

	if (aditional_param)
	{
		sprintf(source + strlen(source), ",uint param");
		*aditional_param = num_passwords_loaded > 1 ? 6 : 2;
	}

	// Begin function code
	sprintf(source + strlen(source), "){uint indx;");

	// Convert the key into a nt_buffer
	memset(buffer_vector_size, 1, sizeof(buffer_vector_size));
	ocl_load(source, nt_buffer, buffer_vector_size, lenght, NUM_KEYS_OPENCL, 1);

	sprintf(source + strlen(source), "uint A,B,C,D,E,W0,W1,W2,W3,W4,W5,W6,W7,W8,W9,W10,W11,W12,W13,W14,W15;");

	ocl_convert_2_big_endian(source, nt_buffer[0], "W0");
	ocl_convert_2_big_endian(source, nt_buffer[1], "W1");
	ocl_convert_2_big_endian(source, nt_buffer[2], "W2");
	ocl_convert_2_big_endian(source, nt_buffer[3], "W3");
	ocl_convert_2_big_endian(source, nt_buffer[4], "W4");
	ocl_convert_2_big_endian(source, nt_buffer[5], "W5");
	ocl_convert_2_big_endian(source, nt_buffer[6], "W6");
	sprintf(source + strlen(source), "W15=0%s;", nt_buffer[7]);
	/* Round 1 */
	sprintf(source + strlen(source),
			"E=0x9fb498b3+W0;"
			"D=rotate(E,5u)+0x66b0cd0d+W1;"
			"C=rotate(D,5u)+(0x7bf36ae2^(E&0x22222222))+0xf33d5697+W2;E=rotate(E,30u);"
			"B=rotate(C,5u)+(0x59d148c0^(D&(E^0x59d148c0)))+0xd675e47b+W3;D=rotate(D,30u);"
			"A=rotate(B,5u)+bs(E,D,C)+0xb453c259+W4;C=rotate(C,30u);"
	
			"E+=rotate(A,5u)+bs(D,C,B)+SQRT_2+W5;B=rotate(B,30u);"
			"D+=rotate(E,5u)+bs(C,B,A)+SQRT_2+W6;A=rotate(A,30u);"
			"C+=rotate(D,5u)+bs(B,A,E)+SQRT_2;E=rotate(E,30u);"
			"B+=rotate(C,5u)+bs(A,E,D)+SQRT_2;D=rotate(D,30u);"
			"A+=rotate(B,5u)+bs(E,D,C)+SQRT_2;C=rotate(C,30u);"
			"E+=rotate(A,5u)+bs(D,C,B)+SQRT_2;B=rotate(B,30u);"
			"D+=rotate(E,5u)+bs(C,B,A)+SQRT_2;A=rotate(A,30u);"
			"C+=rotate(D,5u)+bs(B,A,E)+SQRT_2;E=rotate(E,30u);"
			"B+=rotate(C,5u)+bs(A,E,D)+SQRT_2;D=rotate(D,30u);"
			"A+=rotate(B,5u)+bs(E,D,C)+SQRT_2;C=rotate(C,30u);"
			"E+=rotate(A,5u)+bs(D,C,B)+SQRT_2+W15;B=rotate(B,30u);"
			"W0=rotate(W0^W2,1u);D+=rotate(E,5u)+bs(C,B,A)+SQRT_2+W0;A=rotate(A,30u);"
			"W1=rotate(W1^W3,1u);C+=rotate(D,5u)+bs(B,A,E)+SQRT_2+W1;E=rotate(E,30u);"
			"W2=rotate(W2^W15^W4,1u);B+=rotate(C,5u)+bs(A,E,D)+SQRT_2+W2;D=rotate(D,30u);"
			"W3=rotate(W3^W0^W5,1u);A+=rotate(B,5u)+bs(E,D,C)+SQRT_2+W3;C=rotate(C,30u);");

			/* Round 2 */
			sprintf(source + strlen(source),
			"W4=rotate(W4^W1^W6,1u);E+=rotate(A,5u)+(B^C^D)+SQRT_3+W4;B=rotate(B,30u);"
			"W5=rotate(W5^W2,1u);D+=rotate(E,5u)+(A^B^C)+SQRT_3+W5;A=rotate(A,30u);"
			"W6=rotate(W6^W3,1u);C+=rotate(D,5u)+(E^A^B)+SQRT_3+W6;E=rotate(E,30u);"
			"W7=rotate(W4^W15,1u);B+=rotate(C,5u)+(D^E^A)+SQRT_3+W7;D=rotate(D,30u);"
			"W8=rotate(W5^W0,1u);A+=rotate(B,5u)+(C^D^E)+SQRT_3+W8;C=rotate(C,30u);"
			"W9=rotate(W6^W1,1u);E+=rotate(A,5u)+(B^C^D)+SQRT_3+W9;B=rotate(B,30u);"
			"W10=rotate(W7^W2,1u);D+=rotate(E,5u)+(A^B^C)+SQRT_3+W10;A=rotate(A,30u);"
			"W11=rotate(W8^W3,1u);C+=rotate(D,5u)+(E^A^B)+SQRT_3+W11;E=rotate(E,30u);"
			"W12=rotate(W9^W4,1u);B+=rotate(C,5u)+(D^E^A)+SQRT_3+W12;D=rotate(D,30u);"
			"W13=rotate(W10^W5^W15,1u);A+=rotate(B,5u)+(C^D^E)+SQRT_3+W13;C=rotate(C,30u);"
			"W14=rotate(W11^W6^W0,1u);E+=rotate(A,5u)+(B^C^D)+SQRT_3+W14;B=rotate(B,30u);"
			"DCC2_R(15,12,7,1);D+=rotate(E,5u)+(A^B^C)+SQRT_3+W15;A=rotate(A,30u);"
			"DCC2_R(0,13,8,2);C+=rotate(D,5u)+(E^A^B)+SQRT_3+W0;E=rotate(E,30u);"
			"DCC2_R(1,14,9,3);B+=rotate(C,5u)+(D^E^A)+SQRT_3+W1;D=rotate(D,30u);"
			"DCC2_R(2,15,10,4);A+=rotate(B,5u)+(C^D^E)+SQRT_3+W2;C=rotate(C,30u);"
			"DCC2_R(3,0,11,5);E+=rotate(A,5u)+(B^C^D)+SQRT_3+W3;B=rotate(B,30u);"
			"DCC2_R(4,1,12,6);D+=rotate(E,5u)+(A^B^C)+SQRT_3+W4;A=rotate(A,30u);"
			"DCC2_R(5,2,13,7);C+=rotate(D,5u)+(E^A^B)+SQRT_3+W5;E=rotate(E,30u);"
			"DCC2_R(6,3,14,8);B+=rotate(C,5u)+(D^E^A)+SQRT_3+W6;D=rotate(D,30u);"
			"DCC2_R(7,4,15,9);A+=rotate(B,5u)+(C^D^E)+SQRT_3+W7;C=rotate(C,30u);");

			/* Round 3 */
			sprintf(source + strlen(source),
			"DCC2_R(8,5,0,10);E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+W8;B=rotate(B,30u);"
			"DCC2_R(9,6,1,11);D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+W9;A=rotate(A,30u);"
			"DCC2_R(10,7,2,12);C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+W10;E=rotate(E,30u);"
			"DCC2_R(11,8,3,13);B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+W11;D=rotate(D,30u);"
			"DCC2_R(12,9,4,14);A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+W12;C=rotate(C,30u);"
			"DCC2_R(13,10,5,15);E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+W13;B=rotate(B,30u);"
			"DCC2_R(14,11,6,0);D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+W14;A=rotate(A,30u);"
			"DCC2_R(15,12,7,1);C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+W15;E=rotate(E,30u);"
			"DCC2_R(0,13,8,2);B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+W0;D=rotate(D,30u);"
			"DCC2_R(1,14,9,3);A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+W1;C=rotate(C,30u);"
			"DCC2_R(2,15,10,4);E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+W2;B=rotate(B,30u);"
			"DCC2_R(3,0,11,5);D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+W3;A=rotate(A,30u);"
			"DCC2_R(4,1,12,6);C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+W4;E=rotate(E,30u);"
			"DCC2_R(5,2,13,7);B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+W5;D=rotate(D,30u);"
			"DCC2_R(6,3,14,8);A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+W6;C=rotate(C,30u);"
			"DCC2_R(7,4,15,9);E+=rotate(A,5u)+MAJ(B,C,D)+CONST3+W7;B=rotate(B,30u);"
			"DCC2_R(8,5,0,10);D+=rotate(E,5u)+MAJ(A,B,C)+CONST3+W8;A=rotate(A,30u);"
			"DCC2_R(9,6,1,11);C+=rotate(D,5u)+MAJ(E,A,B)+CONST3+W9;E=rotate(E,30u);"
			"DCC2_R(10,7,2,12);B+=rotate(C,5u)+MAJ(D,E,A)+CONST3+W10;D=rotate(D,30u);"
			"DCC2_R(11,8,3,13);A+=rotate(B,5u)+MAJ(C,D,E)+CONST3+W11;C=rotate(C,30u);");

			/* Round 4 */
			sprintf(source + strlen(source),
			"DCC2_R(12,9,4,14);E+=rotate(A,5u)+(B^C^D)+CONST4+W12;B=rotate(B,30u);"
			"DCC2_R(13,10,5,15);D+=rotate(E,5u)+(A^B^C)+CONST4+W13;A=rotate(A,30u);"
			"DCC2_R(14,11,6,0);C+=rotate(D,5u)+(E^A^B)+CONST4+W14;E=rotate(E,30u);"
			"DCC2_R(15,12,7,1);B+=rotate(C,5u)+(D^E^A)+CONST4+W15;D=rotate(D,30u);"
			"DCC2_R(0,13,8,2);A+=rotate(B,5u)+(C^D^E)+CONST4+W0;C=rotate(C,30u);"
			"DCC2_R(1,14,9,3);E+=rotate(A,5u)+(B^C^D)+CONST4+W1;B=rotate(B,30u);"
			"DCC2_R(2,15,10,4);D+=rotate(E,5u)+(A^B^C)+CONST4+W2;A=rotate(A,30u);"
			"DCC2_R(3,0,11,5);C+=rotate(D,5u)+(E^A^B)+CONST4+W3;E=rotate(E,30u);"
			"DCC2_R(4,1,12,6);B+=rotate(C,5u)+(D^E^A)+CONST4+W4;D=rotate(D,30u);"
			"DCC2_R(5,2,13,7);A+=rotate(B,5u)+(C^D^E)+CONST4+W5;C=rotate(C,30u);"
			"DCC2_R(6,3,14,8);E+=rotate(A,5u)+(B^C^D)+CONST4+W6;B=rotate(B,30u);"
			"DCC2_R(7,4,15,9);D+=rotate(E,5u)+(A^B^C)+CONST4+W7;A=rotate(A,30u);"
			"DCC2_R(8,5,0,10);C+=rotate(D,5u)+(E^A^B)+CONST4+W8;E=rotate(E,30u);"
			"DCC2_R(9,6,1,11);B+=rotate(C,5u)+(D^E^A)+CONST4+W9;D=rotate(D,30u);"
			"DCC2_R(10,7,2,12);A+=rotate(B,5u)+(C^D^E)+CONST4+W10;"
			
			"DCC2_R(12,9,4,14);DCC2_R(15,12,7,1);A=rotate(A,30u)+W15;");

	// Match
	if (num_passwords_loaded == 1)
	{
		unsigned int* bin = (unsigned int*)binary_values;

			if (found_param_3)
				sprintf(output_3, "output[3u]=%s;", found_param_3);

			sprintf(source + strlen(source),
			"if(A==%uu)"
			"{"
				"A=rotate(A-W15,2u);"
				"C=rotate(C,30u);"
				"W11=rotate(W11^W8^W3^W13,1u);"
				"E+=rotate(A,5u)+(B^C^D)+CONST4+W11;B=rotate(B,30u);"
				
				"D+=rotate(E,5u)+(A^B^C)+CONST4+W12;A=rotate(A,30u);"
				
				"W15=rotate(W15,31u);W15^=W12^W7^W1;"
				"C+=rotate(D,5u)+(E^A^B)+CONST4+rotate(W13^W10^W5^W15,1u);"
				
				"B+=(D^rotate(E,30u)^A)+rotate(W14^W11^W6^W0,1u);"

				"if(B==%uu&&C==%uu&&D==%uu&&E==%uu)"
				"{"
					"output[0]=1u;"
					"output[1]=get_global_id(0);"
					"output[2]=0;"
					"%s"
				"}"
			"}"
			, bin[0], bin[1], bin[2], bin[3], bin[4], output_3);
	}
	else
	{
		if (found_param_3)
			sprintf(output_3, "output[3u*found+3u]=%s;", found_param_3);

		sprintf(source + strlen(source),
			"indx=A&SIZE_BIT_TABLE;"
			"if((bit_table[indx>>5u]>>(indx&31u))&1u)"
			"{"
				"indx=table[A & SIZE_TABLE];"

				"while(indx!=0xffffffff)"
				//"if(indx!=0xffffffff)"
				"{"
					"if(A==binary_values[indx*5u+0u])"
					"{"
						"uint aa=rotate(A-W15,2u);"
						"uint cc=rotate(C,30u);"
						"uint ww11=rotate(W11^W8^W3^W13,1u);"
						"uint ee=E+rotate(aa,5u)+(B^cc^D)+CONST4+ww11;uint bb=rotate(B,30u);"
							
						"uint dd=D+rotate(ee,5u)+(aa^bb^cc)+CONST4+W12;aa=rotate(aa,30u);"
							
						"uint ww15=rotate(W15,31u);ww15^=W12^W7^W1;"
						"cc+=rotate(dd,5u)+(ee^aa^bb)+CONST4+rotate(W13^W10^W5^ww15,1u);"
							
						"bb+=(dd^rotate(ee,30u)^aa)+rotate(W14^ww11^W6^W0,1u);"

						"if(bb==binary_values[indx*5u+1u]&&cc==binary_values[indx*5u+2u]&&dd==binary_values[indx*5u+3u]&&ee==binary_values[indx*5u+4u])"
						"{"
							"uint found=atomic_inc(output);"
							"output[%iu*found+1]=get_global_id(0);"
							"output[%iu*found+2]=indx;"
							"%s"
						"}", found_multiplier, found_multiplier, output_3);

	strcat(source, "}"
					"indx=same_hash_next[indx];"
				"}"
			"}");
	}

	if (ocl_end)	ocl_end(source);
	// End of kernel
	strcat(source, "}");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UTF8
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
PRIVATE void ocl_protocol_utf8_init(OpenCL_Param* param, cl_uint gpu_index, generate_key_funtion* gen, gpu_crypt_funtion** gpu_crypt)
{
#ifdef ANDROID
	ocl_common_init(param, gpu_index, gen, gpu_crypt, BINARY_SIZE, 0, ocl_write_sha1_header, ocl_gen_kernel_sha1, kernels2common + UTF8_INDEX_IN_KERNELS, 32, ocl_rule_simple_copy_utf8_le);
#else
	ocl_common_init(param, gpu_index, gen, gpu_crypt, BINARY_SIZE, 0, ocl_write_sha1_header, ocl_gen_kernel_sha1, kernels2common + UTF8_INDEX_IN_KERNELS, 4/*consider 2 for Nvidia*/, ocl_rule_simple_copy_utf8_le);
#endif
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Phrases
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
PRIVATE void ocl_protocol_phrases_init(OpenCL_Param* param, cl_uint gpu_index, generate_key_funtion* gen, gpu_crypt_funtion** gpu_crypt)
{
	ocl_common_init(param, gpu_index, gen, gpu_crypt, BINARY_SIZE, 0, ocl_write_sha1_header, ocl_gen_kernel_sha1, kernels2common + PHRASES_INDEX_IN_KERNELS, 64/*consider 32 for Nvidia*/, ocl_rule_simple_copy_utf8_le);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rules
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
PRIVATE void ocl_protocol_rules_init(OpenCL_Param* param, cl_uint gpu_index, generate_key_funtion* gen, gpu_crypt_funtion** gpu_crypt)
{
	ocl_rules_init(param, gpu_index, gen, gpu_crypt, BINARY_SIZE, 0, ocl_write_sha1_header, ocl_gen_kernel_sha1, RULE_UTF8_LE_INDEX, 1);
}
#endif

PRIVATE int bench_values[] = { 1, 10, 100, 1000, 10000, 65536, 100000, 1000000 };
Format raw_sha1_format = {
	"Raw-SHA1",
	"Raw SHA1 format.",
	NTLM_MAX_KEY_LENGHT,
	BINARY_SIZE,
	0,
	6,
	bench_values,
	LENGHT(bench_values),
	get_binary,
	is_valid,
	add_hash_from_line,
#ifdef _M_X64
	{{CPU_CAP_AVX2, PROTOCOL_UTF8_COALESC_LE, crypt_utf8_coalesc_protocol_avx2}, {CPU_CAP_AVX, PROTOCOL_UTF8_COALESC_LE, crypt_utf8_coalesc_protocol_avx}, {CPU_CAP_SSE2, PROTOCOL_UTF8_COALESC_LE, crypt_utf8_coalesc_protocol_sse2}},
#else
#ifdef HS_ARM
	{{CPU_CAP_NEON, PROTOCOL_UTF8_COALESC_LE, crypt_utf8_coalesc_protocol_neon}, {CPU_CAP_C_CODE, PROTOCOL_UTF8_COALESC_LE, crypt_utf8_coalesc_protocol_c_code}, {CPU_CAP_C_CODE, PROTOCOL_UTF8_COALESC_LE, crypt_utf8_coalesc_protocol_c_code}},
#else
	{{CPU_CAP_SSE2, PROTOCOL_UTF8_COALESC_LE, crypt_utf8_coalesc_protocol_sse2}, {CPU_CAP_SSE2, PROTOCOL_UTF8_COALESC_LE, crypt_utf8_coalesc_protocol_sse2}, {CPU_CAP_C_CODE, PROTOCOL_UTF8_COALESC_LE, crypt_utf8_coalesc_protocol_c_code } },
#endif
#endif
#ifdef HS_OPENCL_SUPPORT
	{{PROTOCOL_CHARSET_OCL, ocl_protocol_charset_init}, {PROTOCOL_PHRASES_OPENCL, ocl_protocol_phrases_init}, {PROTOCOL_RULES_OPENCL, ocl_protocol_rules_init}, {PROTOCOL_UTF8, ocl_protocol_utf8_init}}
#endif
};