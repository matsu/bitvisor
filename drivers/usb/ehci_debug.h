#ifndef _EHCI_DEBUG_H
#define _EHCI_DEBUG_H
#include <core/mm.h>
#include <core/types.h>
#include "ehci.h"

phys32_t ehci_dump_qtd (int loglvl, int indent, const struct mm_as *as,
			phys32_t adr, phys32_t *altnext);
void ehci_dump_alist (int loglvl, const struct mm_as *as, phys_t headqh_phys,
		      int shadow);
void
ehci_dump_urblist(int loglvl, struct usb_request_block *urb);
void ehci_dump_urb (int loglvl, const struct mm_as *as,
		    struct usb_request_block *urb);
void ehci_dump_qtdm (int loglvl, const struct mm_as *as,
		     struct ehci_qtd_meta *qtdm);
void
ehci_dump_async(int loglvl, struct ehci_host *host, int which);
void
ehci_dump_all(int loglvl, struct ehci_host *host);

#endif /* _EHCI_DEBUG_H */
