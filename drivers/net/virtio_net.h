/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2015 Igel Co., Ltd
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

#include <core/types.h>

struct mm_as;
struct nicfunc;
struct pci_bar_info;
struct pci_device;

#ifdef VIRTIO_NET
void virtio_net_handle_config_read (void *handle, u8 iosize, u16 offset,
				    union mem *data);
void virtio_net_handle_config_write (void *handle, u8 iosize, u16 offset,
				     union mem *data);
void virtio_net_set_multifunction (void *handle, int enable);
struct msix_table *
virtio_net_set_msix (void *handle,
		     void (*msix_disable) (void *msix_param),
		     void (*msix_enable) (void *msix_param),
		     void (*msix_vector_change) (void *msix_param,
						 unsigned int queue,
						 int vector),
		     void (*msix_generate) (void *msix_param,
					    unsigned int queue),
		     void (*msix_mmio_update) (void *msix_param),
		     void *msix_param);
void virtio_net_set_pci_device (void *handle, struct pci_device *dev,
				struct pci_bar_info *initial_bar_info,
				void (*mmio_change) (void *mmio_param,
						     struct pci_bar_info
						     *bar_info),
				void *mmio_param);
void *virtio_net_init (struct nicfunc **func, u8 *macaddr,
		       const struct mm_as *as_dma,
		       void (*intr_clear) (void *intr_param),
		       void (*intr_set) (void *intr_param),
		       void (*intr_disable) (void *intr_param),
		       void (*intr_enable) (void *intr_param),
		       void *intr_param);
bool virtio_net_add_cap (void *handle, u8 cap_start, u8 size);
void virtio_net_unregister_handler (void *handle);
#else
static inline void
virtio_net_handle_config_read (void *handle, u8 iosize, u16 offset,
			       union mem *data)
{
}

static inline void
virtio_net_handle_config_write (void *handle, u8 iosize, u16 offset,
				union mem *data)
{
}

static inline void
virtio_net_set_multifunction (void *handle, int enable)
{
}

static inline struct msix_table *
virtio_net_set_msix (void *handle,
		     void (*msix_disable) (void *msix_param),
		     void (*msix_enable) (void *msix_param),
		     void (*msix_vector_change) (void *msix_param,
						 unsigned int queue,
						 int vector),
		     void (*msix_generate) (void *msix_param,
					    unsigned int queue),
		     void (*msix_mmio_update) (void *msix_param),
		     void *msix_param)
{
	return NULL;
}

static inline void
virtio_net_set_pci_device (void *handle, struct pci_device *dev,
			   struct pci_bar_info *initial_bar_info,
			   void (*mmio_change) (void *mmio_param,
						struct pci_bar_info *bar_info),
			   void *mmio_param)
{
}

static inline void *
virtio_net_init (struct nicfunc **func, u8 *macaddr,
		 const struct mm_as *as_dma,
		 void (*intr_clear) (void *intr_param),
		 void (*intr_set) (void *intr_param),
		 void (*intr_disable) (void *intr_param),
		 void (*intr_enable) (void *intr_param), void *intr_param)
{
	return NULL;
}

static inline bool
virtio_net_add_cap (void *handle, u8 cap_start, u8 size)
{
	return false;
}

static inline void
virtio_net_unregister_handler (void *handle)
{
}
#endif
