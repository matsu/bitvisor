/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (C) 2007, 2008
 *      National Institute of Information and Communications Technology
 * Copyright (c) 2010 Igel Co., Ltd
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


#ifndef _CORE_VPN_RTL8169_H
#define _CORE_VPN_RTL8169_H

#include "io.h"
#include "pci.h"

//
#define RTL8169_REGISTER_SIZE			256	// レジスタサイズ
#define RTL8169_ARRY_MAX_NUM			256
#define RTL8169_MAC_LEN			       6
#define RTL8169_TXDESCMASK				0x70000000


#define RTL8169_RXDESC_MAX_NUM			512
#define RTL8169_TXDESC_MAX_NUM			768
#define RTL8169_VPN_INIT_ACCESS_L		0xE60E
#define RTL8169_VPN_INIT_ACCESS_W		0xE70E


#define ETHER_TYPE_FIRST				12
#define ETHER_TYPE_LAST				13
#define ETHER_SIZE					14
#define IP_TOTALSIZE_FIRST				16
#define IP_TOTALSIZE_LAST				17
#define ARP_SIZE					42


//
// RTL8169 register offset constants
//
#define RTL8169_REG_IDR0				0x0000		// ID Register 0
#define RTL8169_REG_IDR1				0x0001		// ID Register 1
#define RTL8169_REG_IDR2				0x0002		// ID Register 2
#define RTL8169_REG_IDR3				0x0003		// ID Register 3
#define RTL8169_REG_IDR4				0x0004		// ID Register 4
#define RTL8169_REG_IDR5				0x0005		// ID Register 5
#define RTL8169_REG_MAR0				0x0008		// Multicast Register 0
#define RTL8169_REG_MAR1				0x0009		// Multicast Register 1
#define RTL8169_REG_MAR2				0x000A		// Multicast Register 2
#define RTL8169_REG_MAR3				0x000B		// Multicast Register 3
#define RTL8169_REG_MAR4				0x000C		// Multicast Register 4
#define RTL8169_REG_MAR5				0x000D		// Multicast Register 5
#define RTL8169_REG_MAR6				0x000E		// Multicast Register 6
#define RTL8169_REG_MAR7				0x000F		// Multicast Register 7
#define RTL8169_REG_DTCCR				0x0010		// Dump Tally Counter Command Register
#define RTL8169_REG_TNPDS				0x0020		// Transmit Normal Priority Descriptors: Start address(64-bit)
#define RTL8169_REG_THPDS				0x0028		// Transmit High Priority Descriptors: Start address(64-bit)
#define RTL8169_REG_FLASH				0x0030
#define RTL8169_REG_ERBCR				0x0034
#define RTL8169_REG_ERSR				0x0036
#define RTL8169_REG_CR				0x0037
#define RTL8169_REG_TPPOLL				0x0038		// Transmit Priority Polling register
#define RTL8169_REG_IMR				0x003C		// Interrupt Mask Register
#define RTL8169_REG_ISR				0x003E		// Interrupt Status Register
#define RTL8169_REG_TCR				0x0040
#define RTL8169_REG_RCR				0x0044
#define RTL8169_REG_TCTR				0x0048
#define RTL8169_REG_MPC				0x004C
#define RTL8169_REG_9346CR				0x0050
#define RTL8169_REG_CONFIG0				0x0051
#define RTL8169_REG_CONFIG1				0x0052
#define RTL8169_REG_CONFIG2				0x0053
#define RTL8169_REG_CONFIG3				0x0054
#define RTL8169_REG_CONFIG4				0x0055
#define RTL8169_REG_CONFIG5				0x0056
#define RTL8169_REG_TIMERINT			0x0058
#define RTL8169_REG_MULINT				0x005C
#define RTL8169_REG_PHYAR				0x0060
#define RTL8169_REG_TBICSR0				0x0064
#define RTL8169_REG_TBI_ANAR			0x0068
#define RTL8169_REG_TBI_LPAR			0x006A
#define RTL8169_REG_PHYSTATUS			0x006C
#define RTL8169_REG_WAKEUP0				0x0084
#define RTL8169_REG_WAKEUP1				0x008C
#define RTL8169_REG_WAKEUP2LD			0x0094
#define RTL8169_REG_WAKEUP2HD			0x009C
#define RTL8169_REG_WAKEUP3LD			0x00A4
#define RTL8169_REG_WAKEUP3HD			0x00AC
#define RTL8169_REG_WAKEUP4LD			0x00B4
#define RTL8169_REG_WAKEUP4HD			0x00BC
#define RTL8169_REG_CRC0				0x00C4
#define RTL8169_REG_CRC1				0x00C6
#define RTL8169_REG_CRC2				0x00C8
#define RTL8169_REG_CRC3				0x00CA
#define RTL8169_REG_CRC4				0x00CC
#define RTL8169_REG_RMS				0x00DA
#define RTL8169_REG_CCR				0x00E0
#define RTL8169_REG_RDSAR				0x00E4		// Receive Descriptor Start Address Register(256-byte alignment)
#define RTL8169_REG_ETTHR				0x00EC
#define RTL8169_REG_FER				0x00F0
#define RTL8169_REG_FEMR				0x00F4
#define RTL8169_REG_FPSR				0x00F8
#define RTL8169_REG_FFER				0x00FC

//
// RTL8169 register bit constants
//

// Bit for RTL8169_REG_ISR
#define RTL8169_REG_ISR_SERR			0x8000
#define RTL8169_REG_ISR_TIMEOUT			0x4000
#define RTL8169_REG_ISR_SWINT			0x0100
#define RTL8169_REG_ISR_TDU				0x0080
#define RTL8169_REG_ISR_FOVW			0x0040
#define RTL8169_REG_ISR_PUN				0x0020
#define RTL8169_REG_ISR_RDU				0x0010
#define RTL8169_REG_ISR_TER				0x0008
#define RTL8169_REG_ISR_TOK				0x0004
#define RTL8169_REG_ISR_RER				0x0002
#define RTL8169_REG_ISR_ROK				0x0001
#define RTL8169_REG_IMR_ROK				0x0001

// Bit for RTL8169_REG_TPPOLL
#define RTL8169_REG_TPPOLL_HPQ			0x80		// High Priority Queue polling
#define RTL8169_REG_TPPOLL_NPQ			0x40		// Normal Priority Queue polling
#define RTL8169_REG_TPPOLL_FSWINT			0x01		// Forced Software Interrupt

//
// Internal constants
//

// Flags for rtl8169_get_desc_addr()
#define RTL8169_TX_NORMAL_PRIORITY_DESC		1
#define RTL8169_TX_HIGH_PRIORITY_DESC		2
#define RTL8169_RX_DESC				3

#define OPT_OWN		0x80000000U
#define OPT_EOR		0x40000000U
#define OPT_FS		0x20000000U
#define OPT_LS		0x10000000U
#define OPT_LGSND	0x08000000U
#define OPT_IPCS	0x00040000U
#define OPT_UDPCS	0x00020000U
#define OPT_TCPCS	0x00010000U

struct desc {
	u64 opts;
	u64 addr;
};

typedef struct RTL8169_CTX{
	struct				pci_device *dev;
	spinlock_t			lock;
	bool				vpn_inited;
	struct netdata			*net_handle;
	u8				macaddr[6];
	net_recv_callback_t		*CallbackRecvPhyNic;
	void				*CallbackRecvPhyNicParam;
	net_recv_callback_t		*CallbackRecvVirtNic;
	void                 	*CallbackRecvVirtNicParam;
	void				*TxBufAddr[RTL8169_ARRY_MAX_NUM];	// 送信バッファ（ディスクリプタで指定されているもの）
	void				*RxBufAddr[RTL8169_ARRY_MAX_NUM];	// 受信バッファ（ディスクリプタで指定されているもの）
	unsigned int			TxBufSize[RTL8169_ARRY_MAX_NUM];
	struct desc                    *txtmpdesc[RTL8169_ARRY_MAX_NUM];
	unsigned int			RxBufSize[RTL8169_ARRY_MAX_NUM];
	int				RxTmpSize[RTL8169_ARRY_MAX_NUM];
	struct desc                    *rxtmpdesc[RTL8169_ARRY_MAX_NUM];
	unsigned int			RxDescNum;
	struct RTL8169_SUB_CTX      *sctx;
	struct RTL8169_SUB_CTX      *sctx_mmio;
	void   *TNPDSvirt,      *THPDSvirt,      *RDSARvirt;
	phys_t  TNPDSphys,       THPDSphys,       RDSARphys;
	u64     TNPDSreg,        THPDSreg,        RDSARreg;
	void   *tnbufvirt[256], *thbufvirt[256], *rdbufvirt[256];
	phys_t  tnbufphys[256],  thbufphys[256],  rdbufphys[256];
	int sendindex;
	int enableflag;
	bool conceal;
}RTL8169_CTX;

typedef struct RTL8169_SUB_CTX{
	int	i;
	int	e;
	int	io;
	int	hd;
	void	*h;
	void	*map;
	uint	maplen;
	phys_t	mapaddr;		// メモリマップドレジスタの物理アドレス
	phys_t ioaddr;			// I/Oマップドレジスタの物理アドレス
	struct	RTL8169_CTX *ctx;
	bool   hwReset;
}RTL8169_SUB_CTX;

#ifdef _DEBUG
u64 time = 0;
#endif

// 関数プロトタイプ
static void rtl8169_init();
static void rtl8169_new(struct pci_device *pci_device);
static int  rtl8169_config_read (struct pci_device *pci_device, u8 iosize,
				 u16 offset, union mem *data);
static int  rtl8169_config_write (struct pci_device *pci_device, u8 iosize,
				  u16 offset, union mem *data);
static int  rtl8169_mm_handler(void *data, phys_t gphys, bool wr, void *buf, uint len, u32 flags);
static int  rtl8169_io_handler(core_io_t io, union mem *data, void *arg);
static bool rtl8169_init_vpn_client(RTL8169_CTX *ctx, RTL8169_SUB_CTX *sctx);
static bool rtl8169_get_macaddr (struct RTL8169_SUB_CTX *sctx, void *buf);
static void rtl8169_write(RTL8169_SUB_CTX *sctx, phys_t offset, UINT data, UINT size);
static unsigned int rtl8169_read(RTL8169_SUB_CTX *sctx, phys_t offset, UINT size);
static bool rtl8169_hook_write(RTL8169_SUB_CTX *sctx, phys_t offset, UINT data, UINT len);
static bool rtl8169_get_txdata_to_vpn(struct RTL8169_CTX *ctx, struct RTL8169_SUB_CTX *sctx, int Desckind);
static bool rtl8169_hook_read(RTL8169_SUB_CTX *sctx, phys_t offset, UINT *data, UINT len);
static void rtl8169_get_rxdata_to_vpn(struct RTL8169_CTX *ctx, struct RTL8169_SUB_CTX *sctx);
static void rtl8169_get_rxdesc_data(struct RTL8169_CTX *ctx, int *ArrayNum, struct desc *TargetDesc, UINT BufSize, void *);
static void GetPhysicalNicInfo (void *handle, struct nicinfo *info);
static void SendPhysicalNic(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes, bool print_ok);
static void SetPhysicalNicRecvCallback (void *handle,
					net_recv_callback_t *callback,
					void *param);
static void GetVirtualNicInfo (void *handle, struct nicinfo *info);
static void SendVirtualNic (SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes, bool print_ok);
static void SetVirtualNicRecvCallback (void *handle,
				       net_recv_callback_t *callback,
				       void *param);
static void rtl8169_send_virt_nic(SE_HANDLE nic_handle, phys_t rxdescphys, void *data, UINT size);

static int  rtl8169_offset_check (struct pci_device *dev, u8 iosize,
				  u16 offset, union mem *data);
static void reghook(struct RTL8169_SUB_CTX *sctx, int i,
		    struct pci_bar_info *bar);
static void unreghook(struct RTL8169_SUB_CTX *sctx);

#endif	// _CORE_VPN_RTL8169_H

