/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

/**
 *  This module enables fake pci device support for native QEMU.
 *  This allows quick creation of snapshots in native mode, ready to be run in S2E.
 */


#include <stdio.h>
#include "qemu-common.h"
#include "hw/hw.h"
#include "hw/pci.h"
#include "fakepci.h"
#undef CONFIG_S2E

#ifndef CONFIG_S2E


static fake_pci_t *s_fake_pci=NULL;
PCIDevice *fake_pci_dev;


/////////////////////////////////////////////////////////////////////
/* Dummy I/O functions for symbolic devices. Unused for now. */
static uint64_t fake_read(void *opaque, target_phys_addr_t addr,
                            unsigned size)
{
    return 0;
}

static void fake_write(void *opaque, target_phys_addr_t addr,
                         uint64_t data, unsigned size)
{

}

static const MemoryRegionOps fake_ops = {
    .read = fake_read,
    .write = fake_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

typedef struct _PCIFakeState {
    PCIDevice dev;
    MemoryRegion io[PCI_NUM_REGIONS];
}PCIFakeState;

static int pci_fake_init(PCIDevice *pci_dev)
{
    PCIFakeState *d = DO_UPCAST(PCIFakeState, dev, pci_dev);
    uint8_t *pci_conf;
    int i;

    pci_conf = d->dev.config;
    pci_config_set_vendor_id(pci_conf, s_fake_pci->fake_pci_vendor_id);
    pci_config_set_device_id(pci_conf, s_fake_pci->fake_pci_device_id);
    pci_config_set_class(pci_conf, s_fake_pci->fake_pci_class_code);
    pci_conf[PCI_HEADER_TYPE] = PCI_HEADER_TYPE_NORMAL; // header_type
    pci_conf[0x3d] = 1; // interrupt pin 0

    for(i=0; i<s_fake_pci->fake_pci_num_resources; ++i) {
        int type = s_fake_pci->fake_pci_resources[i].type;

        memory_region_init_io(&d->io[i], &fake_ops, d, "fake", s_fake_pci->fake_pci_resources[i].size);
        pci_register_bar(&d->dev, i, type, &d->io[i]); //, pci_fake_map
    }

    return 0;
}


static int pci_fake_exit(PCIDevice *pci_dev)
{
    return 0;
}

static  VMStateDescription vmstate_pci_fake = {
    .name = "fake",
    .version_id = 3,
    .minimum_version_id = 3,
    .minimum_version_id_old = 3,
    .fields      = (VMStateField []) {
        VMSTATE_PCI_DEVICE(dev, PCIFakeState),
        VMSTATE_END_OF_LIST()
    }
};


static Property fakepci_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void fakepci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->init = pci_fake_init;
    k->exit = pci_fake_exit;

#if 0
    k->vendor_id = s_fake_pci->fake_pci_vendor_id;
    k->device_id = s_fake_pci->fake_pci_device_id;
    k->revision = s_fake_pci->fake_pci_revision_id;
    k->class_id = s_fake_pci->fake_pci_class_code;
    k->subsystem_vendor_id = s_fake_pci->fake_pci_ss_vendor_id;
    k->subsystem_id = s_fake_pci->fake_pci_ss_id;
#endif

    dc->vmsd = &vmstate_pci_fake;
    dc->props = fakepci_properties;
}

static TypeInfo fakepci_info = {
    .name          = "fakepci",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIFakeState),
    .class_init    = fakepci_class_init,
};

static void pci_fakepci_register_types(void)
{
    type_register_static(&fakepci_info);
}

type_init(pci_fakepci_register_types)


#endif
