/*	$OpenBSD: if_ne_pci.c,v 1.8 2001/03/12 05:37:00 aaron Exp $	*/
/*	$NetBSD: if_ne_pci.c,v 1.8 1998/07/05 00:51:24 jonathan Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#ifdef __NetBSD__
#include <net/if_ether.h>
#endif
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#ifdef __NetBSD__
#include <netinet/if_inarp.h>
#else
#include <netinet/if_ether.h>
#endif
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/miivar.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/ic/ne2000reg.h>
#include <dev/ic/ne2000var.h>

#include <dev/ic/rtl80x9reg.h>
#include <dev/ic/rtl80x9var.h>

struct ne_pci_softc {
	struct ne2000_softc sc_ne2000;		/* real "ne2000" softc */

	/* PCI-specific goo */
	void *sc_ih;				/* interrupt handle */
};

int ne_pci_match __P((struct device *, void *, void *));
void ne_pci_attach __P((struct device *, struct device *, void *));

struct cfattach ne_pci_ca = {
	sizeof(struct ne_pci_softc), ne_pci_match, ne_pci_attach
};

const struct ne_pci_product {
	pci_vendor_id_t npp_vendor;
	pci_product_id_t npp_product;
	int (*npp_mediachange) __P((struct dp8390_softc *));
	void (*npp_mediastatus) __P((struct dp8390_softc *,
	    struct ifmediareq *));
	void (*npp_init_card) __P((struct dp8390_softc *));
	void (*npp_media_init) __P((struct dp8390_softc *));
} ne_pci_prod[] = {
	{ PCI_VENDOR_REALTEK,		PCI_PRODUCT_REALTEK_RT8029,
	  rtl80x9_mediachange,		rtl80x9_mediastatus,
	  rtl80x9_init_card,		rtl80x9_media_init,
	  /* RealTek 8029 */ },

	{ PCI_VENDOR_WINBOND,		PCI_PRODUCT_WINBOND_W89C940F,
	  NULL,				NULL,
	  NULL,				NULL,
	  /* Winbond 89C940F */ },

	{ PCI_VENDOR_VIATECH,		PCI_PRODUCT_VIATECH_VT86C926,
	  NULL,				NULL,
	  NULL,				NULL,
	  /* VIA Technologies VT86C926 */ },

	{ PCI_VENDOR_SURECOM,		PCI_PRODUCT_SURECOM_NE34,
	  NULL,				NULL,
	  NULL,				NULL,
	  /* Surecom NE-34 */ },

	{ PCI_VENDOR_NETVIN,		PCI_PRODUCT_NETVIN_NV5000,
	  NULL,				NULL,
	  NULL,				NULL,
	  /* NetVin 5000 */ },

	/* XXX The following entries need sanity checking in pcidevs */
	{ PCI_VENDOR_COMPEX,		PCI_PRODUCT_COMPEX_COMPEXE,
	  NULL,				NULL,
	  NULL,				NULL,
	  /* Compex */ },

	{ PCI_VENDOR_WINBOND2,		PCI_PRODUCT_WINBOND2_W89C940,
	  NULL,				NULL,
	  NULL,				NULL,
	  /* ProLAN */ },

	{ PCI_VENDOR_KTI,		PCI_PRODUCT_KTI_KTIE,
	  NULL,				NULL,
	  NULL,				NULL,
	  /* KTI */ },
};

const struct ne_pci_product *ne_pci_lookup __P((struct pci_attach_args *));

const struct ne_pci_product *
ne_pci_lookup(pa)
	struct pci_attach_args *pa;
{
	const struct ne_pci_product *npp;

	for (npp = ne_pci_prod;
	    npp < &ne_pci_prod[sizeof(ne_pci_prod)/sizeof(ne_pci_prod[0])];
	    npp++) {
		if (PCI_VENDOR(pa->pa_id) == npp->npp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == npp->npp_product)
			return (npp);
	}
	return (NULL);
}

/*
 * PCI constants.
 * XXX These should be in a common file!
 */
#define PCI_CBIO	0x10		/* Configuration Base IO Address */

int
ne_pci_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = aux;

	if (ne_pci_lookup(pa) != NULL)
 		return (1);

	return (0);
}

void
ne_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ne_pci_softc *psc = (struct ne_pci_softc *)self;
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
#ifndef __NetBSD__
	bus_addr_t iobase;
	bus_size_t iosize;
#endif
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	const char *intrstr;
	const struct ne_pci_product *npp;
	pci_intr_handle_t ih;
	pcireg_t csr;

	npp = ne_pci_lookup(pa);
	if (npp == NULL) {
		printf("\n");
		panic("ne_pci_attach: impossible");
	}

#ifdef __NetBSD__
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	    &nict, &nich, NULL, NULL)) {
		printf(": can't map i/o space\n");
		return;
	}
#else
	if (pci_io_find(pc, pa->pa_tag, PCI_CBIO, &iobase, &iosize)) {
		printf(": can't find I/O base\n");
		return;
	}

	nict = pa->pa_iot;

	if (bus_space_map(nict, iobase, iosize, 0, &nich)) {
		printf(": can't map I/O space\n");
		return;
	}
#endif

	asict = nict;
	if (bus_space_subregion(nict, nich, NE2000_ASIC_OFFSET,
	    NE2000_ASIC_NPORTS, &asich)) {
		printf(": can't subregion i/o space\n");
		return;
	}

	dsc->sc_regt = nict;
	dsc->sc_regh = nich;

	nsc->sc_asict = asict;
	nsc->sc_asich = asich;

	/* Enable the card. */
	csr = pci_conf_read(pc, pa->pa_tag,
	    PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	/* This interface is always enabled. */
	dsc->sc_enabled = 1;

	dsc->sc_mediachange = npp->npp_mediachange;
	dsc->sc_mediastatus = npp->npp_mediastatus;
	dsc->sc_media_init = npp->npp_media_init;
	dsc->init_card = npp->npp_init_card;

	/* Map and establish the interrupt. */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, dp8390_intr, dsc,
		dsc->sc_dev.dv_xname);
	if (psc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s\n", intrstr);

	/*
	 * Do generic NE2000 attach.  This will read the station address
	 * from the EEPROM.
	 */
	ne2000_attach(nsc, NULL);
}
