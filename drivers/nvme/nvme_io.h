/*
 * Copyright (c) 2017 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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

/**
 * @file	drivers/nvme/nvme_io.h
 * @brief	Facilities for interacting with NVMe driver
 * @author	Ake Koomsin
 */

#ifndef _NVME_IO_H
#define _NVME_IO_H

/*
 * === nvme_io overview ===
 *
 * Functions provided here can be categorized into:
 * 1) Buffer related functions
 * 2) Host Controller driver related functions
 * 3) Guest requests related functions
 * 4) I/O commands related functions
 * 5) Extension related functions
 *
 * To create an extension, create a file, make sure that it is compiled,
 * and initialize it by following this pattern:
 *
 * static void
 * init_ext (void)
 * {
 *         nvme_io_register_ext ("ext_name", init_func);
 * }
 *
 * INITFUNC ("driver1", init_ext);
 *
 * Inside the init_func(), we can create an interceptor by initializing
 * nvme_io_interceptor object, and call nvme_io_install_interceptor().
 * io_interceptor->on_init() will be called when the guest submits its
 * first I/O request. If the interceptor needs to access the drive for
 * initialization, it is recommended to create a thread for initialization
 * in on_init(), and return NVME_IO_PAUSE_FETCHING_GUEST_CMDS to delay
 * fetching guest requests. Don't forget to call
 * nvme_io_start_fetching_g_reqs() after initialization is complete.
 * Note that nvme_io_install_interceptor() fails if an interceptor is
 * already installed.
 *
 * When intercepting a guest request, an interceptor can pause the request
 * by using nvme_io_pause_guest_request(), and do something on behalf of it.
 * it can access the guest request buffer by creating a nvme_io_g_buf object
 * from nvme_io_alloc_g_buf(). After the operation finishes, it can turn
 * the guest request into a dummy request  by calling
 * nvme_io_change_g_req_to_dummy_read(). Finally, it need to resume
 * the guest request by calling nvme_io_resume_guest_request().
 * It is also possible to get a guest request completion event by
 * setting up a callback with nvme_io_set_req_callback().
 *
 * The NVMe driver implementation, by default, does not shadow guest
 * requests' buffers. If an interceptor wants to create a shadow buffer,
 * it must allocate a nvme_io_dmabuf, and call nvme_io_set_shadow_buffer().
 * Copying data between the guest buffer and the allocated nvme_io_dmabuf
 * can be done by nvme_io_memcpy_g_buf().
 *
 * Sending I/O requests to the NVMe driver can be done through
 * nvme_io_descriptor objects. Sender will create an nvme_io_descriptor
 * object through nvme_io_init_descriptor(), set up buffer by
 * nvme_io_set_phys_buffers(), and send a request with either
 * nvme_io_read_request() or nvme_io_write_request(). It is possible
 * to create an nvme_io_descriptor object using a guest request's buffer
 * through nvme_io_g_buf_io_desc().
 *
 */

struct pci_device;

struct nvme_host;
struct nvme_io_descriptor;
struct nvme_request;

#define PAGE_NBYTES (4096)
#define PAGE_NBYTES_DIV_EXPO (12)

/* Return value for on_init() */
#define NVME_IO_RESUME_FETCHING_GUEST_CMDS (0)
#define NVME_IO_PAUSE_FETCHING_GUEST_CMDS  (1)

struct nvme_io_interceptor {
	void *interceptor;
	int  (*on_init) (void *interceptor);
	void (*on_read) (void *interceptor,
			 struct nvme_request *g_req,
			 u32 nsid,
			 u64 start_lba,
			 u16 n_lbas);
	void (*on_write) (void *interceptor,
			  struct nvme_request *g_req,
			  u32 nsid,
			  u64 start_lba,
			  u16 n_lbas);
	void (*on_compare) (void *interceptor,
			    struct nvme_request *g_req,
			    u32 nsid,
			    u64 start_lba,
			    u16 n_lbas);
	u32 (*on_data_management) (void *interceptor,
				   struct nvme_request *g_req,
				   u32 nsid,
				   void *range_buf,
				   u32 buf_nbytes,
				   u32 n_ranges);
	int (*can_stop) (void *interceptor);

	/*
	 * This function tells the driver if it should poll for command
	 * completeness when the guest writes to a submission doorbell.
	 * This is useful when you have to deal with accesses from
	 * firmwares. Their timeout is short.
	 */
	int (*poll_completeness) (void *interceptor);
	/*
	 * When polling happens, the interceptor may want to check for
	 * something.
	 */
	void (*polling_callback) (void *interceptor);

	void (*filter_identify_data) (void *interceptor,
				      u32 nsid,
				      u16 controller_id,
				      u8 cns,
				      u8 *data);
	/* Fetching limit per queue */
	uint (*get_fetching_limit) (void *interceptor,
				  uint n_waiting_g_reqs);
	u16 (*get_io_entries) (void *interceptor,
			       u16 g_n_entries,
			       u16 max_n_entries);

	/*
	 * If the interceptor wants to limit numbers of requests
	 * to process globally, not just a queue, the interceptor
	 * should set serialize_queue_fetch flag. This will tell
	 * NVMe driver to fetch from one queue at a time.
	 */
	u8 serialize_queue_fetch;
};
#define NVME_IO_INTERCEPTOR_NBYTES (sizeof (struct nvme_io_interceptor))

/* ----- Start buffer related functions ----- */

struct nvme_io_dmabuf {
	u8 *buf;
	phys_t buf_phys;
	phys_t *dma_list;
	phys_t dma_list_phys;
	u64 nbytes;
};

struct nvme_io_dmabuf * nvme_io_alloc_dmabuf (u64 nbytes);

void nvme_io_free_dmabuf (struct nvme_io_dmabuf *dmabuf);

struct nvme_io_g_buf;

struct nvme_io_g_buf * nvme_io_alloc_g_buf (struct nvme_host *host,
					    struct nvme_request *g_req);

void nvme_io_free_g_buf (struct nvme_io_g_buf *g_buf);

void nvme_io_memcpy_g_buf (struct nvme_io_g_buf *g_buf,
			   u8 *buf,
			   u64 buf_nbytes,
			   u64 g_buf_offset,
			   int g_buf_to_buf);

void nvme_io_memset_g_buf (struct nvme_io_g_buf *g_buf,
			   u8  value,
			   u64 buf_nbytes,
			   u64 g_buf_offset);

/* ----- End buffer related functions ----- */

/* ----- Start NVMe host controller driver related functions ----- */

int nvme_io_host_ready (struct nvme_host *host);

struct pci_device * nvme_io_get_pci_device (struct nvme_host *host);

/* Return 1 on success */
int nvme_io_install_interceptor (struct nvme_host *host,
				 struct nvme_io_interceptor *io_interceptor);

void nvme_io_start_fetching_g_reqs (struct nvme_host *host);

u32 nvme_io_get_n_ns (struct nvme_host *host);

u64 nvme_io_get_total_lbas (struct nvme_host *host, u32 nsid);

u64 nvme_io_get_lba_nbytes (struct nvme_host *host, u32 nsid);

u16 nvme_io_get_max_n_lbas (struct nvme_host *host, u32 nsid);

/* ----- End NVMe host controller driver related functions ----- */

/* ----- Start NVMe guest request related functions ----- */

void nvme_io_pause_guest_request (struct nvme_request *g_req);

/* Change a guest request's access area */
int nvme_io_patch_start_lba (struct nvme_host *host,
			     struct nvme_request *g_req,
			     u64 new_start_lba);

#define NVME_IO_NO_TRIGGER_SUBMIT (0)
#define NVME_IO_TRIGGER_SUBMIT    (1)
void nvme_io_resume_guest_request (struct nvme_host *host,
				   struct nvme_request *g_req,
			      	   int trigger_submit);

void nvme_io_change_g_req_to_dummy_read (struct nvme_request *g_req,
					 phys_t dummy_buf,
				    	 u64 dummy_lba);

u16 nvme_io_req_queue_id (struct nvme_request *g_req);

void nvme_io_set_req_callback (struct nvme_request *req,
			       void (*callback) (struct nvme_host *host,
						 u8 status_type,
					    	 u8 status,
					    	 void *arg),
			       void *arg);

/* Overide buffer provided by the guest */
int nvme_io_set_shadow_buffer (struct nvme_request *g_req,
			       struct nvme_io_dmabuf *dmabuf);

/* ----- End NVMe guest request related functions ----- */

/* ----- Start I/O related functions ----- */

/* Return NULL if a parameter is invalid */
struct nvme_io_descriptor * nvme_io_init_descriptor (struct nvme_host *host,
						     u32 nsid,
						     u16 queue_id,
			 			     u64 lba_start,
			 			     u16 n_lbas);

/* Return 0 if a parameter is invalid */
int nvme_io_set_phys_buffers (struct nvme_host *host,
			      struct nvme_io_descriptor *io_desc,
			      phys_t *pagebuf_arr,
			      phys_t pagebuf_arr_phys,
			      u64 n_pages_accessed,
			      u64 first_page_offset);

struct nvme_io_descriptor * nvme_io_g_buf_io_desc (struct nvme_host *host,
						   struct nvme_request *g_req,
		       				   struct nvme_io_g_buf *g_buf,
		       				   u64 g_buf_offset,
		       				   u64 lba_start,
		       				   u16 n_lbas);

/* Return 1 if it succeeds */
int nvme_io_read_request (struct nvme_host *host,
			  struct nvme_io_descriptor *io_desc,
		      	  void (*callback) (struct nvme_host *host,
					    u8 status_type,
					    u8 status,
					    void *arg),
		      	  void *arg);

/* Return 1 if it succeeds */
int nvme_io_write_request (struct nvme_host *host,
			   struct nvme_io_descriptor *io_desc,
			   void (*callback) (struct nvme_host *host,
					     u8 status_type,
					     u8 status,
					     void *arg),
			   void *arg);

/* Return 1 if it succeeds */
int nvme_io_flush_request (struct nvme_host *host,
			   u32 nsid,
		       	   void (*callback) (struct nvme_host *host,
					     u8 status_type,
					     u8 status,
					     void *arg),
		       	   void *arg);

/* Return 1 if it succeeds */
int nvme_io_identify (struct nvme_host *host,
		      u32 nsid,
		      phys_t pagebuf,
		      u8 cns, u16 controller_id,
		      void (*callback) (struct nvme_host *host,
					u8 status_type,
					u8 status,
					void *arg),
		      void *arg);

/* ----- End I/O related functions ----- */

/* ----- Start extension related functions ----- */

void nvme_io_register_ext (char *name,
			   int (*init) (struct nvme_host *host));

/* ----- End extension related functions ----- */

#endif /* _NVME_IO_H */
