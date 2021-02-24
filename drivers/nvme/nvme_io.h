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
 * setting up a callback with nvme_io_set_g_req_callback().
 *
 * The NVMe driver implementation, by default, does not shadow guest
 * requests' buffers. If an interceptor wants to create a shadow buffer,
 * it must allocate a nvme_io_dmabuf, and call nvme_io_set_shadow_buffer().
 * Copying data between the guest buffer and the allocated nvme_io_dmabuf
 * can be done by nvme_io_memcpy_g_buf().
 *
 * To send I/O requests, senders need to create nvme_io_descriptor and
 * nvme_io_req_handle objects. Senders create an nvme_io_descriptor object
 * by calling nvme_io_init_descriptor(), and set up its buffer by calling
 * nvme_io_set_phys_buffers(). To submit requests, senders create an
 * nvme_io_req_handle object by calling nvme_io_prepare_requests(). Senders
 * then pass nvme_io_descriptor objects to nvme_io_add_read_request()/
 * nvme_io_add_write_request() to prepare requests to be submitted.
 * nvme_io_descriptor objects are freed automatically after they are added to
 * an nvme_io_req_handle object. Senders call nvme_io_submit_requests() to
 * submit requests to the controller and wait for completion by calling
 * nvme_io_wait_for_completion() for a specified time in seconds
 * (0 means indefinitely). nvme_io_wait_for_completion() frees the
 * nvme_io_req_handle object automatically.
 *
 * We provide nvme_io_read_request() and nvme_io_write_request() in case
 * Senders need only a request. We also provide nvme_io_flush_request() for
 * submitting a flush command. Senders can destroy an nvme_io_req_handle object
 * by calling nvme_io_destroy_handle() if the nvme_io_req_handle object is not
 * yet submitted. For admin commands, we provides only nvme_io_identify() and
 * nvme_io_get_n_queues() for now.
 *
 */

struct pci_device;

struct nvme_host;
struct nvme_io_descriptor;
struct nvme_request;

#define PAGE_NBYTES (4096)
#define PAGE_NBYTES_DIV_EXPO (12)

typedef enum _nvme_io_error_t {
	NVME_IO_ERROR_OK,
	NVME_IO_ERROR_NOT_READY,
	NVME_IO_ERROR_INVALID_PARAM,
	NVME_IO_ERROR_IO_ERROR,
	NVME_IO_ERROR_TIMEOUT,
	NVME_IO_ERROR_NO_OPERATION,
	NVME_IO_ERROR_INTERNAL_ERROR,
} nvme_io_error_t;

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

/* ----- Start buffer related functions ----- */

struct nvme_io_dmabuf {
	u8 *buf;
	phys_t buf_phys;
	phys_t *dma_list;
	phys_t dma_list_phys;
	u64 nbytes;
};

/* Return NULL if nbytes is 0 */
struct nvme_io_dmabuf * nvme_io_alloc_dmabuf (u64 nbytes);

void nvme_io_free_dmabuf (struct nvme_io_dmabuf *dmabuf);

struct nvme_io_g_buf;

/* Return NULL if a parameter is invalid */
struct nvme_io_g_buf * nvme_io_alloc_g_buf (struct nvme_host *host,
					    struct nvme_request *g_req);

void nvme_io_free_g_buf (struct nvme_io_g_buf *g_buf);

nvme_io_error_t nvme_io_memcpy_g_buf (struct nvme_io_g_buf *g_buf,
				      u8 *buf, u64 buf_nbytes,
				      u64 g_buf_offset, int g_buf_to_buf);

nvme_io_error_t nvme_io_memset_g_buf (struct nvme_io_g_buf *g_buf,
				      u8 value, u64 buf_nbytes,
				      u64 g_buf_offset);

/* ----- End buffer related functions ----- */

/* ----- Start NVMe host controller driver related functions ----- */

nvme_io_error_t nvme_io_host_ready (struct nvme_host *host);

nvme_io_error_t nvme_io_get_pci_device (struct nvme_host *host,
					struct pci_device **pci);

nvme_io_error_t
nvme_io_install_interceptor (struct nvme_host *host,
			     struct nvme_io_interceptor *io_interceptor);

nvme_io_error_t nvme_io_start_fetching_g_reqs (struct nvme_host *host);

nvme_io_error_t nvme_io_get_n_ns (struct nvme_host *host, u32 *n_ns);

nvme_io_error_t nvme_io_get_total_lbas (struct nvme_host *host, u32 nsid,
					u64 *total_lbas);

nvme_io_error_t nvme_io_get_lba_nbytes (struct nvme_host *host, u32 nsid,
					u32 *lba_nbytes);

nvme_io_error_t nvme_io_get_max_n_lbas (struct nvme_host *host, u32 nsid,
					u16 *max_n_lbas);

/* ----- End NVMe host controller driver related functions ----- */

/* ----- Start NVMe guest request related functions ----- */

nvme_io_error_t nvme_io_pause_guest_request (struct nvme_request *g_req);

/* Change a guest request's access area */
nvme_io_error_t nvme_io_patch_start_lba (struct nvme_host *host,
					 struct nvme_request *g_req,
					 u64 new_start_lba);

nvme_io_error_t nvme_io_resume_guest_request (struct nvme_host *host,
					      struct nvme_request *g_req,
					      bool trigger_submit);

nvme_io_error_t nvme_io_change_g_req_to_dummy_read (struct nvme_request *g_req,
						    phys_t dummy_buf,
						    u64 dummy_lba);

nvme_io_error_t nvme_io_req_queue_id (struct nvme_request *g_req,
				      u16 *queue_id);

typedef void (*nvme_io_req_callback_t) (struct nvme_host *host,
					u8 status_type,
					u8 status,
					u32 cmd_specific,
					void *arg);

nvme_io_error_t nvme_io_set_g_req_callback (struct nvme_request *g_req,
					    nvme_io_req_callback_t callback,
					    void *arg);

/* Override buffer provided by the guest */
nvme_io_error_t nvme_io_set_shadow_buffer (struct nvme_request *g_req,
					   struct nvme_io_dmabuf *dmabuf);

/* ----- End NVMe guest request related functions ----- */

/* ----- Start I/O related functions ----- */

struct nvme_io_req_handle;

/* Return NULL if a parameter is invalid */
struct nvme_io_descriptor * nvme_io_init_descriptor (struct nvme_host *host,
						     u32 nsid,
			 			     u64 lba_start,
			 			     u16 n_lbas);

nvme_io_error_t nvme_io_set_phys_buffers (struct nvme_host *host,
					  struct nvme_io_descriptor *io_desc,
					  phys_t *pagebuf_arr,
					  phys_t pagebuf_arr_phys,
					  u64 n_pages_accessed,
					  u64 first_page_offset);

/* Return NULL if a parameter is invalid */
struct nvme_io_descriptor * nvme_io_g_buf_io_desc (struct nvme_host *host,
						   struct nvme_request *g_req,
		       				   struct nvme_io_g_buf *g_buf,
		       				   u64 g_buf_offset,
		       				   u64 lba_start,
		       				   u16 n_lbas);

nvme_io_error_t
nvme_io_prepare_requests (struct nvme_host *host, u16 queue_id,
			  struct nvme_io_req_handle **req_handle);

nvme_io_error_t
nvme_io_destroy_req_handle (struct nvme_io_req_handle *req_handle);

nvme_io_error_t
nvme_io_add_read_request (struct nvme_host *host,
			  struct nvme_io_req_handle *req_handle,
			  struct nvme_io_descriptor *io_desc,
			  nvme_io_req_callback_t callback, void *arg);

nvme_io_error_t
nvme_io_add_write_request (struct nvme_host *host,
			   struct nvme_io_req_handle *req_handle,
			   struct nvme_io_descriptor *io_desc,
			   nvme_io_req_callback_t callback, void *arg);

nvme_io_error_t
nvme_io_submit_requests (struct nvme_host *host,
			 struct nvme_io_req_handle *req_handle);

nvme_io_error_t
nvme_io_read_request (struct nvme_host *host,
		      struct nvme_io_descriptor *io_desc, u16 queue_id,
		      nvme_io_req_callback_t callback, void *arg,
		      struct nvme_io_req_handle **req_handle);

nvme_io_error_t
nvme_io_write_request (struct nvme_host *host,
		       struct nvme_io_descriptor *io_desc, u16 queue_id,
		       nvme_io_req_callback_t callback, void *arg,
		       struct nvme_io_req_handle **req_handle);

nvme_io_error_t
nvme_io_flush_request (struct nvme_host *host, u32 nsid, u16 queue_id,
		       nvme_io_req_callback_t callback, void *arg,
		       struct nvme_io_req_handle **req_handle);

nvme_io_error_t nvme_io_destroy_handle (struct nvme_io_req_handle *req_handle);

/* If timeout_sec is 0, it means no timeout */
nvme_io_error_t
nvme_io_wait_for_completion (struct nvme_io_req_handle *req_handle,
			     uint timeout_sec);

/* Polling */
nvme_io_error_t nvme_io_identify (struct nvme_host *host, u32 nsid,
				  phys_t pagebuf, u8 cns, u16 controller_id);

/* Polling */
nvme_io_error_t nvme_io_get_n_queues (struct nvme_host *host,
				      u16 *n_subm_queues,
				      u16 *n_comp_queues);

/* ----- End I/O related functions ----- */

/* ----- Start extension related functions ----- */

void nvme_io_register_ext (char *name,
			   nvme_io_error_t (*init) (struct nvme_host *host));

/* ----- End extension related functions ----- */

#endif /* _NVME_IO_H */
