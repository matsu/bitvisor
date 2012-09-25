/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (C) 2007, 2008 
 *      National Institute of Information and Communications Technology
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

// Secure VM Project
// VPN Client Module (IPsec Driver) Source Code
// 
// Developed by Daiyuu Nobori (dnobori@cs.tsukuba.ac.jp)

// SeTypes.h
// 概要: 型定義ヘッダ

#ifndef	SETYPES_H
#define	SETYPES_H


#if !defined(SECRYPTO_C)
// OpenSSL が使用する構造体
typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct bio_st BIO;
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct X509_req_st X509_REQ;
//typedef struct PKCS12 PKCS12;
typedef struct bignum_st BIGNUM;
typedef struct DES_ks DES_key_schedule;
typedef struct dh_st DH;
#endif	// ENCRYPT_C

// コンパイラ依存コード
#ifdef	_MSC_VER
// MS-VC 用
#define SE_STRUCT_PACKED
#define SE_WIN32
#else	// WIN32
// gcc 用
#define SE_STRUCT_PACKED	__attribute__ ((__packed__))
#define SE_UNIX
#endif	// WIN32

//
// マクロ
//

// 行番号表示
#define SE_WHERE			{SeDebug("%s: %u", __FILE__, __LINE__);}

// 時間生成
#define SE_TIMESPAN(min, sec, millisec)	(((UINT64)(min) * 60ULL + (UINT64)(sec)) * 1000ULL + (UINT64)(millisec))

//
// 定数および型定義
//

#ifndef	_WINDOWS_
#define INFINITE			(0xFFFFFFFF)		// 無限
#define	MAX_PATH			260					// 最大パス長
typedef unsigned int		BOOL;
#define TRUE				1
#define FALSE				0
typedef	unsigned int		UINT;
typedef	unsigned int		UINT32;
typedef	unsigned int		DWORD;
typedef	signed int			INT;
typedef	signed int			INT32;
typedef	int					UINT_PTR;
typedef	long				LONG_PTR;
#endif	// _WINDOWS_

#ifndef	NULL
#define NULL				((void *)0)
#endif

#ifndef	__CORE_TYPES_H
typedef	unsigned int		bool;
#endif	// __CORE_TYPES_H

#define	true				1
#define	false				0

typedef	unsigned short		WORD;
typedef	unsigned short		USHORT;
typedef	signed short		SHORT;
typedef	unsigned char		BYTE;
typedef	unsigned char		UCHAR;
typedef signed char			CHAR;
typedef	unsigned long long	UINT64;
typedef signed long long	INT64;

typedef void *				SE_HANDLE;

// 標準バッファサイズ
#define	STD_SIZE			512
#define	MAX_SIZE			512
#define	BUF_SIZE			512

// 比較関数
typedef int (SE_CALLBACK_COMPARE)(void *p1, void *p2);

//
// マクロ
//

#ifdef	MAX
#undef	MAX
#endif	// MAX
#ifdef	MIN
#undef	MIN
#endif	// MIN
// a と b の最小値を求める
#define	MIN(a, b)			((a) >= (b) ? (b) : (a))
// a と b の最大値を求める
#define	MAX(a, b)			((a) >= (b) ? (a) : (b))
// max_value よりも小さい a を返す
#define	LESS(a, max_value)	((a) <= (max_value) ? (a) : (max_value))
// min_value よりも大きい a を返す
#define	MORE(a, min_value)	((a) >= (min_value) ? (a) : (min_value))
// a が b と c の内部にある値かどうか調べる
#define	INNER(a, b, c)		(((b) <= (c) && (a) >= (b) && (a) <= (c)) || ((b) >= (c) && (a) >= (c) && (a) <= (b)))
// a が b と c の外部にある値かどうか調べる
#define	OUTER(a, b, c)		(!INNER((a), (b), (c)))
// a を b と c の間にある値となるように調整する
#define	MAKESURE(a, b, c)	(((b) <= (c)) ? (MORE(LESS((a), (c)), (b))) : (MORE(LESS((a), (b)), (c))))
// デフォルト値
#define SE_DEFAULT_VALUE(config_value, default_value)	(((config_value) == 0) ? (default_value) : (config_value))
// a と b を比較
#define SE_COMPARE(a, b)	(((a) == (b)) ? 0 : (((a) > (b)) ? 1 : -1))

// 可変個引数
#ifndef	va_list
#define va_list			__builtin_va_list
#endif	// va_list

#ifndef	va_start
#define va_start(PTR, LASTARG)	__builtin_va_start (PTR, LASTARG)
#endif	// va_start

#ifndef	va_end
#define va_end(PTR)		__builtin_va_end (PTR)
#endif	// va_end

#ifndef	va_arg
#define va_arg(PTR, TYPE)	__builtin_va_arg (PTR, TYPE)
#endif	// va_arg


//
// 構造体の typedef 定義
//

// SeInterface.h
typedef struct SE_SYSCALL_TABLE SE_SYSCALL_TABLE;
typedef struct SE_NICINFO SE_NICINFO;
typedef struct SE_ROOT SE_ROOT;
typedef void (SE_SYS_CALLBACK_TIMER)(SE_HANDLE timer_handle, void *param);
typedef void (SE_SYS_CALLBACK_RECV_NIC)(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes, void *param);

// SeKernel.h
typedef struct SE_LOCK SE_LOCK;
typedef struct SE_ETH SE_ETH;
typedef struct SE_TIMER SE_TIMER;
typedef struct SE_TIMER_ENTRY SE_TIMER_ENTRY;
typedef struct SE_ETH_SENDER_MAC SE_ETH_SENDER_MAC;
typedef void (SE_ETH_RECV_CALLBACK)(SE_ETH *eth, void *param);
typedef void (SE_TIMER_CALLBACK)(SE_TIMER *t, UINT64 current_tick, void *param);

// SeMemory.h
typedef struct SE_BUF SE_BUF;
typedef struct SE_FIFO SE_FIFO;
typedef struct SE_LIST SE_LIST;
typedef struct SE_QUEUE SE_QUEUE;
typedef struct SE_STACK SE_STACK;

// SeStr.h
typedef struct SE_TOKEN_LIST SE_TOKEN_LIST;

// SeConfig.h
typedef struct SE_CONFIG_ENTRY SE_CONFIG_ENTRY;

// SeCrypto.h
typedef struct SE_DES_KEY SE_DES_KEY;
typedef struct SE_DES_KEY_VALUE SE_DES_KEY_VALUE;
typedef struct SE_CERT SE_CERT;
typedef struct SE_KEY SE_KEY;
typedef struct SE_DH SE_DH;

// SePacket.h
typedef struct SE_MAC_HEADER SE_MAC_HEADER;
typedef struct SE_ARPV4_HEADER SE_ARPV4_HEADER;
typedef struct SE_IPV4_HEADER SE_IPV4_HEADER;
typedef struct SE_UDP_HEADER SE_UDP_HEADER;
typedef struct SE_UDPV4_PSEUDO_HEADER SE_UDPV4_PSEUDO_HEADER;
typedef struct SE_TCP_HEADER SE_TCP_HEADER;
typedef struct SE_TCPV4_PSEUDO_HEADER SE_TCPV4_PSEUDO_HEADER;
typedef struct SE_ICMP_HEADER SE_ICMP_HEADER;
typedef struct SE_ICMP_ECHO SE_ICMP_ECHO;
typedef struct SE_DHCPV4_HEADER SE_DHCPV4_HEADER;
typedef struct SE_PACKET SE_PACKET;
typedef struct SE_IPV6_HEADER SE_IPV6_HEADER;
typedef struct SE_IPV6_FRAGMENT_HEADER SE_IPV6_FRAGMENT_HEADER;
typedef struct SE_IPV4_ADDR SE_IPV4_ADDR;
typedef struct SE_IPV6_ADDR SE_IPV6_ADDR;
typedef struct SE_IPV6_PSEUDO_HEADER SE_IPV6_PSEUDO_HEADER;
typedef struct SE_IPV6_OPTION_HEADER SE_IPV6_OPTION_HEADER;
typedef struct SE_ICMPV6_OPTION SE_ICMPV6_OPTION;
typedef struct SE_ICMPV6_OPTION_LINK_LAYER SE_ICMPV6_OPTION_LINK_LAYER;
typedef struct SE_ICMPV6_OPTION_PREFIX SE_ICMPV6_OPTION_PREFIX;
typedef struct SE_ICMPV6_OPTION_MTU SE_ICMPV6_OPTION_MTU;
typedef struct SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_HEADER SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_HEADER;
typedef struct SE_ICMPV6_NEIGHBOR_SOLICIATION_HEADER SE_ICMPV6_NEIGHBOR_SOLICIATION_HEADER;
typedef struct SE_ICMPV6_ROUTER_SOLICIATION_HEADER SE_ICMPV6_ROUTER_SOLICIATION_HEADER;
typedef struct SE_ICMPV6_ROUTER_ADVERTISEMENT_HEADER SE_ICMPV6_ROUTER_ADVERTISEMENT_HEADER;
typedef struct SE_ICMPV6_OPTION_LIST SE_ICMPV6_OPTION_LIST;

// SeIp4.h
typedef struct SE_ARPV4_ENTRY SE_ARPV4_ENTRY;
typedef struct SE_ARPV4_WAIT SE_ARPV4_WAIT;
typedef struct SE_IPV4_WAIT SE_IPV4_WAIT;
typedef struct SE_IPV4_FRAGMENT SE_IPV4_FRAGMENT;
typedef struct SE_IPV4_COMBINE SE_IPV4_COMBINE;
typedef struct SE_DHCPV4_OPTION SE_DHCPV4_OPTION;
typedef struct SE_DHCPV4_OPTION_LIST SE_DHCPV4_OPTION_LIST;
typedef struct SE_IPV4 SE_IPV4;
typedef struct SE_IPV4_HEADER_INFO SE_IPV4_HEADER_INFO;
typedef struct SE_UDPV4_HEADER_INFO SE_UDPV4_HEADER_INFO;
typedef struct SE_ICMPV4_HEADER_INFO SE_ICMPV4_HEADER_INFO;
typedef void (SE_IPV4_RECV_CALLBACK)(SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size, void *param);

// SeIp6.h
typedef struct SE_IPV6 SE_IPV6;
typedef struct SE_IPV6_HEADER_PACKET_INFO SE_IPV6_HEADER_PACKET_INFO;
typedef struct SE_IPV6_HEADER_INFO SE_IPV6_HEADER_INFO;
typedef struct SE_ICMPV6_HEADER_INFO SE_ICMPV6_HEADER_INFO;
typedef struct SE_UDPV6_HEADER_INFO SE_UDPV6_HEADER_INFO;
typedef struct SE_IPV6_COMBINE SE_IPV6_COMBINE;
typedef struct SE_IPV6_FRAGMENT SE_IPV6_FRAGMENT;
typedef struct SE_IPV6_WAIT SE_IPV6_WAIT;
typedef struct SE_NDPV6_WAIT SE_NDPV6_WAIT;
typedef struct SE_IPV6_NEIGHBOR_ENTRY SE_IPV6_NEIGHBOR_ENTRY;
typedef void (SE_IPV6_RECV_CALLBACK)(SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size, void *param);


// SeIke.h
typedef struct SE_IKE_HEADER SE_IKE_HEADER;
typedef struct SE_IKE_PAYLOAD_HEADER SE_IKE_PAYLOAD_HEADER;
typedef struct SE_IKE_COMMON_HEADER SE_IKE_COMMON_HEADER;
typedef struct SE_IKE_PROPOSAL_HEADER SE_IKE_PROPOSAL_HEADER;
typedef struct SE_IKE_TRANSFORM_HEADER SE_IKE_TRANSFORM_HEADER;
typedef struct SE_IKE_TRANSFORM_VALUE SE_IKE_TRANSFORM_VALUE;
typedef struct SE_IKE_ID_HEADER SE_IKE_ID_HEADER;
typedef struct SE_IKE_CERT_HEADER SE_IKE_CERT_HEADER;
typedef struct SE_IKE_CERT_REQUEST_HEADER SE_IKE_CERT_REQUEST_HEADER;
typedef struct SE_IKE_NOTICE_HEADER SE_IKE_NOTICE_HEADER;
typedef struct SE_IKE_SA_HEADER SE_IKE_SA_HEADER;
typedef struct SE_IKE_DELETE_HEADER SE_IKE_DELETE_HEADER;
typedef struct SE_IKE_PACKET SE_IKE_PACKET;
typedef struct SE_IKE_PACKET_PAYLOAD SE_IKE_PACKET_PAYLOAD;
typedef struct SE_IKE_PACKET_SA_PAYLOAD SE_IKE_PACKET_SA_PAYLOAD;
typedef struct SE_IKE_PACKET_PROPOSAL_PAYLOAD SE_IKE_PACKET_PROPOSAL_PAYLOAD;
typedef struct SE_IKE_PACKET_TRANSFORM_PAYLOAD SE_IKE_PACKET_TRANSFORM_PAYLOAD;
typedef struct SE_IKE_PACKET_TRANSFORM_VALUE SE_IKE_PACKET_TRANSFORM_VALUE;
typedef struct SE_IKE_PACKET_ID_PAYLOAD SE_IKE_PACKET_ID_PAYLOAD;
typedef struct SE_IKE_PACKET_CERT_PAYLOAD SE_IKE_PACKET_CERT_PAYLOAD;
typedef struct SE_IKE_PACKET_CERT_REQUEST_PAYLOAD SE_IKE_PACKET_CERT_REQUEST_PAYLOAD;
typedef struct SE_IKE_PACKET_DATA_PAYLOAD SE_IKE_PACKET_DATA_PAYLOAD;
typedef struct SE_IKE_PACKET_NOTICE_PAYLOAD SE_IKE_PACKET_NOTICE_PAYLOAD;
typedef struct SE_IKE_PACKET_DELETE_PAYLOAD SE_IKE_PACKET_DELETE_PAYLOAD;
typedef struct SE_IKE_CRYPTO_PARAM SE_IKE_CRYPTO_PARAM;
typedef struct SE_IKE_IP_ADDR SE_IKE_IP_ADDR;
typedef struct SE_IKE_P1_KEYSET SE_IKE_P1_KEYSET;


// SeSec.h
typedef struct SE_SEC SE_SEC;
typedef struct SE_SEC_CLIENT_FUNCTION_TABLE SE_SEC_CLIENT_FUNCTION_TABLE;
typedef struct SE_SEC_CONFIG SE_SEC_CONFIG;
typedef struct SE_IKE_SA SE_IKE_SA;
typedef struct SE_IPSEC_SA SE_IPSEC_SA;
typedef void (SE_SEC_TIMER_CALLBACK)(UINT64 tick, void *param);
typedef void (SE_SEC_UDP_RECV_CALLBACK)(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size, void *param);
typedef void (SE_SEC_ESP_RECV_CALLBACK)(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size, void *param);
typedef void (SE_SEC_VIRTUAL_IP_RECV_CALLBACK)(void *data, UINT size, void *param);

// SeVpn.h
typedef struct SE_VPN_CONFIG SE_VPN_CONFIG;
typedef struct SE_VPN SE_VPN;

// SeVpn4.h
typedef struct SE_VPN4 SE_VPN4;

// SeVpn6.h
typedef struct SE_VPN6 SE_VPN6;


#endif	// SETYPES_H

