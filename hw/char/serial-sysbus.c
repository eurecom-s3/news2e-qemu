#include "hw/char/serial.h"
#include "hw/sysbus.h"
#include "sysemu/char.h"

#define XMIT_FIFO           0       
#define RECV_FIFO           1

typedef struct SysbusSerialState {      
    SysBusDevice dev;       
    uint32_t index;     
    uint32_t iobase;        
    uint32_t irq;       
    SerialState state;      
} SysbusSerialState;

static const VMStateDescription vmstate_sysbus_serial = {       
    .name = "serial",       
    .version_id = 3,        
    .minimum_version_id = 2,        
    .fields      = (VMStateField []) {      
        VMSTATE_STRUCT(state, SysbusSerialState, 0, vmstate_serial, SerialState),       
        VMSTATE_END_OF_LIST()       
    }       
};

static int serial_sysbus_initfn(SysBusDevice *dev)      
{       
    static int index;       
    SysbusSerialState *sysbus = DO_UPCAST(SysbusSerialState, dev, dev);     
    SerialState *s = &sysbus->state;        
    if (sysbus->index == -1)        
        sysbus->index = index;      
    if (sysbus->index >= MAX_SERIAL_PORTS)      
        return -1;      
//    if (sysbus->iobase == -1)     
//        return -1;        
//        sysbus->iobase = isa_serial_io[sysbus->index];        
//    if (sysbus->isairq == -1)     
//        sysbus->isairq = isa_serial_irq[isa->index];      
    index++;        
    s->baudbase = 115200;       
    s->chr = qemu_char_get_next_serial();       
    s->it_shift = 2;        
    sysbus_init_irq(dev, &s->irq);      
//    isa_init_irq(dev, &s->irq, isa->isairq);      
    serial_realize_core(s, &error_fatal);
//    qdev_set_legacy_instance_id(&dev->qdev, isa->iobase, 3);      
    memory_region_init_io(&s->io, NULL, &serial_mm_ops[0], s, "serial", 8 << s->it_shift);        
    sysbus_init_mmio(dev, &s->io);      
//    isa_register_ioport(dev, &s->io, isa->iobase);        
    return 0;       
}

static Property serial_sysbus_properties[] = {      
    DEFINE_PROP_UINT32("index", SysbusSerialState, index,   -1),        
    DEFINE_PROP_UINT32("iobase", SysbusSerialState, iobase,  -1),        
    DEFINE_PROP_UINT32("irq",   SysbusSerialState, irq,  -1),       
    DEFINE_PROP_CHR("chardev",  SysbusSerialState, state.chr),      
    DEFINE_PROP_UINT32("wakeup", SysbusSerialState, state.wakeup, 0),       
    DEFINE_PROP_END_OF_LIST(),      
};

static void serial_sysbus_class_initfn(ObjectClass *klass, void *data)      
{       
    DeviceClass *dc = DEVICE_CLASS(klass);      
    SysBusDeviceClass *ic = SYS_BUS_DEVICE_CLASS(klass);        
    ic->init = serial_sysbus_initfn;        
    dc->vmsd = &vmstate_sysbus_serial;      
    dc->props = serial_sysbus_properties;       
}

static TypeInfo serial_sysbus_info = {      
    .name          = "sysbus-serial",       
    .parent        = TYPE_SYS_BUS_DEVICE,       
    .instance_size = sizeof(SysbusSerialState),     
    .class_init    = serial_sysbus_class_initfn,        
};

static void serial_register_types(void)     
{       
    type_register_static(&serial_sysbus_info);      
}
    
type_init(serial_register_types)
